// ======================================================================
// \title  I2cTester.hpp
// \author tumbar
// \brief  hpp file for I2cTester component implementation class
//
// Validation instrument for Samd21::I2cDriver and, through it, the DMA
// engine. It models no real part: it writes a register offset and reads bytes
// back from a ground-settable 7-bit address, and reports every outcome the
// driver can hand it as a separate counter.
//
// It is also the reference implementation of the asynchronous I2C client
// contract. Three rules of that contract shape the code below and are called
// out where they apply:
//
//   1. The busy-reject callback is SYNCHRONOUS. If the driver is mid-
//      transaction it invokes the completion callback from inside the request
//      call, so component state must be committed before any request is
//      issued and the completion path must be re-entrancy safe.
//   2. The buffers are client-owned. The driver keeps only a descriptor and
//      DMAs out of / into this component's storage, so the buffers must not be
//      touched between issuing a request and its completion.
//   3. Both completion input ports must be connected. The driver guards each
//      callback with isConnected_* and otherwise drops it silently, which
//      would wedge this component non-IDLE forever.
//
// See docs/sdd.md.
// ======================================================================

#ifndef CuriosityReference_I2cTester_HPP
#define CuriosityReference_I2cTester_HPP

#include "CuriosityReference/I2cTester/I2cTesterComponentAc.hpp"
#include "Fw/Buffer/Buffer.hpp"

namespace CuriosityReference {

class I2cTester final : public I2cTesterComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct I2cTester object
    //! Non-explicit single-arg constructor is the standard F´ component idiom
    //! (matches I2cTesterComponentBase and every other component in the tree).
    // cppcheck-suppress noExplicitConstructor
    I2cTester(const char* const compName  //!< The component name
    );

    //! Destroy I2cTester object
    ~I2cTester();

    // ----------------------------------------------------------------------
    // Public interface
    // ----------------------------------------------------------------------

    //! Configure the tester's initial transaction parameters.
    //!
    //! Must be called during initialization -- from the topology startTasks
    //! phase, which is the first phase in which the event and time ports are
    //! wired -- before any request is serviced. An out-of-range argument logs
    //! InvalidConfig and leaves the component unconfigured, so a bad topology
    //! shows up on the ground instead of silently misbehaving.
    //!
    //! \param i2cAddr   7-bit target address, in [MIN_I2C_ADDRESS, MAX_I2C_ADDRESS]
    //! \param regOffset register/offset byte written before each read
    //! \param readLen   bytes to read back, in [1, MAX_READ_BYTES]
    void configure(U8 i2cAddr, U8 regOffset, U8 readLen);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for schedIn
    //!
    //! Issues one periodic write-read when polling is enabled. A tick that
    //! lands while a transaction is in flight is counted as a busy reject and
    //! otherwise ignored -- schedIn has no reply path.
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

    //! Handler implementation for writeReadCompleteIn
    //!
    //! Invoked by the I2cDriver from the main context on a normal completion,
    //! or synchronously from inside i2cWriteReadOut_out when the driver was
    //! already busy. Decodes the receive buffer, drives telemetry/events, and
    //! replies to the initiator.
    void writeReadCompleteIn_handler(FwIndexType portNum,          //!< The port number
                                     Fw::Buffer& writeBuffer,      //!< The transmitted buffer
                                     Fw::Buffer& readBuffer,       //!< The received buffer
                                     const Drv::I2cStatus& status  //!< Transaction outcome
                                     ) override;

    //! Handler implementation for writeCompleteIn
    //!
    //! Invoked by the I2cDriver for the write-only WRITE_ONCE transaction.
    //! A write carries no data back, so this only counts the outcome and
    //! replies to the initiator.
    void writeCompleteIn_handler(FwIndexType portNum,          //!< The port number
                                 Fw::Buffer& buffer,           //!< The transmitted buffer
                                 const Drv::I2cStatus& status  //!< Transaction outcome
                                 ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SET_ADDRESS
    //!
    //! Retarget the tester. Takes effect on the next transaction; an
    //! out-of-range address fails the command rather than asserting.
    void SET_ADDRESS_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq,           //!< The command sequence number
                                U8 addr               //!< The new 7-bit address
                                ) override;

    //! Handler implementation for command SET_REGISTER
    //!
    //! Set the register offset and read length. The length bound enforced here
    //! is what keeps the driver's 255-byte payload assert unreachable from the
    //! ground.
    void SET_REGISTER_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                 U32 cmdSeq,           //!< The command sequence number
                                 U8 regOffset,         //!< The new register offset
                                 U8 readLen            //!< The new read length
                                 ) override;

    //! Handler implementation for command READ_ONCE
    //!
    //! Issue one write-read. The command reply is deferred until the driver
    //! reports the outcome.
    void READ_ONCE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                              U32 cmdSeq            //!< The command sequence number
                              ) override;

    //! Handler implementation for command WRITE_ONCE
    //!
    //! Issue one write of [regOffset, value]. The command reply is deferred
    //! until the driver reports the outcome.
    void WRITE_ONCE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq,           //!< The command sequence number
                               U8 value              //!< The data byte to write
                               ) override;

    //! Handler implementation for command SET_POLLING
    //!
    //! Enable or disable the schedIn-driven periodic read.
    void SET_POLLING_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq,           //!< The command sequence number
                                bool enable           //!< Whether to poll
                                ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Attempt to claim the component for a new transaction.
    //! \param op the transaction being started
    //! \return true if the component was idle and \p op is now active; false
    //!         (with a Busy event logged and BusyRejects bumped) otherwise.
    bool beginOperation(I2cTester_State op);

    //! Issue the write-read backing the currently active read operation:
    //! writes the one-byte register offset, then reads m_readLength bytes.
    //! The component state must already be committed -- the driver may reject
    //! the request and call back synchronously from inside this call.
    void startRead();

    //! Issue the write-only transaction backing WRITE_CMD: [m_regOffset, value].
    //! Same re-entrancy caveat as startRead().
    //! \param value the data byte written after the register offset
    void startWrite(U8 value);

    //! Recover the register offset that started a completed transaction from
    //! the write buffer the driver handed back. The driver returns the exact
    //! descriptor it was given, so byte 0 is the offset that was on the wire --
    //! which is not necessarily m_regOffset, because SET_REGISTER does not
    //! require an idle component and may have landed while the transaction was
    //! in flight.
    //! \param writeBuffer the write buffer returned by the completion callback
    //! \return the offset from the wire, or m_regOffset if the buffer is empty
    U8 recoverRegOffset(const Fw::Buffer& writeBuffer) const;

    //! Decode a successful read: pack the received bytes MSB-first into the two
    //! U32 telemetry channels, emit ReadComplete, and bump TxnCount.
    //! \param data      pointer to the received bytes (m_readBuffer)
    //! \param len       number of bytes the driver returned
    //! \param regOffset register offset the read started at, from the wire
    //! \return true on success; false (after ShortRead) if len < m_readLength
    bool handleReadComplete(const U8* data, FwSizeType len, U8 regOffset);

    //! Route a non-OK completion status into its per-failure-mode counter,
    //! emit the matching channel, and log I2cError. Breaking the counters out
    //! by status is the point of this component: an address NACK (nothing on
    //! the bus) reads very differently from I2C_OTHER_ERR (driver busy or
    //! stall recovery), and one merged counter cannot tell them apart.
    //! \param status    status reported by the driver
    //! \param regOffset register offset of the transaction that failed
    void countError(const Drv::I2cStatus& status, U8 regOffset);

    //! Reply to the initiator of the active transaction and return to IDLE.
    //! Snapshots and clears m_state BEFORE dispatching the reply so a reply
    //! handler that issues a new request re-entrantly sees an idle component.
    //! \param success whether the transaction succeeded
    void finishOperation(bool success);

    //! Emit TargetAddress / RegisterOffset / ReadLength after a settings change
    void reportSettings();

    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    //! Lowest general-purpose 7-bit I2C address (0x00-0x07 are reserved:
    //! general call, START byte, CBUS, Hs master code)
    static constexpr U8 MIN_I2C_ADDRESS = 0x08;

    //! Highest general-purpose 7-bit I2C address (0x78-0x7F are reserved for
    //! 10-bit addressing and device ID)
    static constexpr U8 MAX_I2C_ADDRESS = 0x77;

    //! Largest read this component will request. Chosen to fill the two U32
    //! telemetry channels exactly (2 * 4 bytes). Well under the driver's
    //! 255-byte DMA limit, which readLen must never be allowed to reach --
    //! the driver asserts on it, and readLen is ground-settable.
    static constexpr U8 MAX_READ_BYTES = 8;

    //! Transmit buffer size: one register-offset byte plus one data byte, the
    //! largest thing this component ever writes (WRITE_ONCE).
    static constexpr FwSizeType WRITE_BUFFER_SIZE = 2;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    I2cTester_State m_state;       //!< Active transaction (IDLE when free)
    FwOpcodeType m_pendingOpCode;  //!< Opcode of pending command transaction
    U32 m_pendingCmdSeq;           //!< Command sequence of pending command transaction

    bool m_configLoaded;  //!< configure() succeeded; requests are serviced
    bool m_polling;       //!< schedIn drives a periodic read (off at boot)

    U8 m_i2cAddress;  //!< 7-bit target address
    U8 m_regOffset;   //!< Register offset written before each read
    U8 m_readLength;  //!< Bytes requested per read, in [1, MAX_READ_BYTES]

    //! Value passed to the in-flight WRITE_ONCE, held so its completion event
    //! can report it. Unlike m_regOffset this needs no wire recovery: it is
    //! written only while claiming an idle component and read only on that
    //! transaction's completion, so the busy gate already pins it.
    U8 m_writeValue;

    //! Transmit buffer: [regOffset] for a read, [regOffset, value] for a write.
    //! Owned by this component -- the I2cDriver only borrows the descriptor and
    //! DMAs out of this storage, so it must not be touched between issuing a
    //! request and its completion.
    U8 m_writeBuffer[WRITE_BUFFER_SIZE];

    //! Receive buffer, DMA'd into by the driver.
    //!
    //! This buffer is per-instance. A device component that instantiates many
    //! copies of itself on one bus may instead share a single file-static
    //! receive buffer (only one transaction is in flight per driver, and the
    //! completion runs in the main context), but at 8 bytes the sharing is not
    //! worth the cross-instance invariant it would impose.
    U8 m_readBuffer[MAX_READ_BYTES];

    // Counters. Every one is emitted on the tick it changes, so a ground
    // operator sees the failure mode immediately rather than at the next poll.
    U32 m_txnCount;       //!< Successful transactions
    U32 m_busyRejects;    //!< Requests this component rejected as busy
    U32 m_addressErrors;  //!< Completions with I2C_ADDRESS_ERR
    U32 m_writeErrors;    //!< Completions with I2C_WRITE_ERR
    U32 m_readErrors;     //!< Completions with I2C_READ_ERR
    U32 m_openErrors;     //!< Port-not-connected drops and I2C_OPEN_ERR
    U32 m_otherErrors;    //!< Completions with I2C_OTHER_ERR
    U32 m_shortReads;     //!< Successful reads returning too few bytes
};

}  // namespace CuriosityReference

#endif
