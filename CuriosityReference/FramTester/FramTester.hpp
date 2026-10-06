// ======================================================================
// \title  FramTester.hpp
// \author Devin (JPL)
// \brief  hpp file for FramTester component implementation class
// ======================================================================

#ifndef CuriosityReference_FramTester_HPP
#define CuriosityReference_FramTester_HPP

#include "CuriosityReference/FramTester/FramTesterComponentAc.hpp"
#include "Fw/Buffer/Buffer.hpp"
#include "fprime-devices/Fram/FramConfig/FppConstantsAc.hpp"

namespace CuriosityReference {

//! Ground-driven exerciser for Fram.FramDriver. One transaction at a time: a
//! command (or a soak-loop tick) fills m_data, hands it to the driver through
//! m_buffer, and the completion callback reports the outcome. See docs/sdd.md.
class FramTester final : public FramTesterComponentBase {
  public:
    //! Largest transfer this component issues. Bounded by the driver's MaxTransfer, which is
    //! also the size of the only data buffer this component owns.
    static constexpr FwSizeType MAX_LENGTH = static_cast<FwSizeType>(Fram::Config::MaxTransfer);

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct FramTester object
    FramTester(const char* const compName  //!< The component name
    );

    //! Destroy FramTester object
    ~FramTester();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for readCompleteIn
    //!
    //! Read completion. The buffer is the one passed to framReadOut.
    void readCompleteIn_handler(FwIndexType portNum,        //!< The port number
                                U32 offset,                 //!< F-RAM byte offset the request started at
                                Fw::Buffer& buffer,         //!< The buffer passed with the request
                                const Fram::Status& status  //!< Outcome of the request
                                ) override;

    //! Handler implementation for schedIn
    //!
    //! Rate-group tick. When polling is enabled (see SET_POLLING) each tick
    //! writes a fresh pattern to the poll region and verifies it on the next
    //! tick. Polling is off at boot so a freshly reset board leaves the F-RAM
    //! contents alone.
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

    //! Handler implementation for writeCompleteIn
    //!
    //! Write completion. The buffer is the one passed to framWriteOut.
    void writeCompleteIn_handler(FwIndexType portNum,        //!< The port number
                                 U32 offset,                 //!< F-RAM byte offset the request started at
                                 Fw::Buffer& buffer,         //!< The buffer passed with the request
                                 const Fram::Status& status  //!< Outcome of the request
                                 ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command WRITE_PATTERN
    //!
    //! Write length bytes of pattern (seed, seed+1, ...) starting at offset
    void WRITE_PATTERN_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                  U32 cmdSeq,           //!< The command sequence number
                                  U32 offset,           //!< F-RAM byte offset of the first byte
                                  U8 length,  //!< Bytes to write; 0..Fram.Config.MaxTransfer (0 is forwarded so the
                                              //!< driver's TOO_LARGE reject can be exercised)
                                  U8 seed     //!< Value of the first byte; each following byte increments by one
                                  ) override;

    //! Handler implementation for command READ_VERIFY
    //!
    //! Read length bytes starting at offset and compare against the pattern
    //! (seed, seed+1, ...). The first mismatch is reported in PatternMismatch.
    void READ_VERIFY_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq,           //!< The command sequence number
                                U32 offset,           //!< F-RAM byte offset of the first byte
                                U8 length,  //!< Bytes to read; 0..Fram.Config.MaxTransfer (0 is forwarded so the
                                            //!< driver's TOO_LARGE reject can be exercised)
                                U8 seed     //!< Expected value of the first byte
                                ) override;

    //! Handler implementation for command READ
    //!
    //! Read length bytes starting at offset and report the first eight in ReadComplete
    void READ_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                         U32 cmdSeq,           //!< The command sequence number
                         U32 offset,           //!< F-RAM byte offset of the first byte
                         U8 length  //!< Bytes to read; 0..Fram.Config.MaxTransfer (0 is forwarded so the driver's
                                    //!< TOO_LARGE reject can be exercised)
                         ) override;

    //! Handler implementation for command SET_POLLING
    //!
    //! Enable or disable the schedIn-driven write/verify soak loop
    void SET_POLLING_cmdHandler(
        FwOpcodeType opCode,  //!< The opcode
        U32 cmdSeq,           //!< The command sequence number
        bool enable           //!< true to alternate WRITE_PATTERN / READ_VERIFY on the poll region each tick
        ) override;

    //! Handler implementation for command SET_POLL_REGION
    //!
    //! Set the region the soak loop writes and verifies
    void SET_POLL_REGION_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                    U32 cmdSeq,           //!< The command sequence number
                                    U32 offset,           //!< F-RAM byte offset of the poll region
                                    U8 length  //!< Bytes per poll transaction; must be in [1, Fram.Config.MaxTransfer]
                                    ) override;

  private:
    // ----------------------------------------------------------------------
    // Private helper methods
    // ----------------------------------------------------------------------

    //! Claim the component for one transaction. Logs Busy and returns false if one is in flight.
    bool beginOperation(FramTester_State op);

    //! Release the component and reply to the pending command, if the transaction was one
    void finishOperation(bool success);

    //! Fill m_data[0..length) with the pattern seed, seed+1, ...
    void fillPattern(U8 seed, FwSizeType length);

    //! Issue a read of length bytes at offset into m_data. State must already be committed.
    void startRead(U32 offset, FwSizeType length);

    //! Issue a write of m_data[0..length) to offset. State must already be committed.
    void startWrite(U32 offset, FwSizeType length);

    //! Compare m_data[0..m_length) against the pattern starting at m_seed. Logs the first mismatch.
    bool verifyPattern(U32 offset);

    //! Pack m_data[first..first+4) MSB first, zero past m_length
    U32 packWord(FwSizeType first) const;

    //! Record a completion status: LastStatus telemetry, and on failure the error counter and event
    bool recordStatus(U32 offset, const Fram::Status& status);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    FramTester_State m_state;      //!< Active transaction (IDLE when free)
    FwOpcodeType m_pendingOpCode;  //!< Opcode of the pending command transaction
    U32 m_pendingCmdSeq;           //!< Command sequence of the pending command transaction

    //! The data of the transaction in flight, and the buffer that carries it to the driver.
    //! Written before the request is issued and not touched again until the completion
    //! arrives: the driver fills it from the DMA ISR for reads.
    U8 m_data[MAX_LENGTH];
    Fw::Buffer m_buffer;
    FwSizeType m_length;  //!< Bytes in the transaction in flight
    U8 m_seed;            //!< First pattern byte of the transaction in flight

    bool m_polling;           //!< schedIn drives the write/verify soak loop (off at boot)
    bool m_pollVerifyNext;    //!< The poll region holds a pattern written with m_pollSeed; verify it next
    U32 m_pollOffset;         //!< Soak-loop region offset
    FwSizeType m_pollLength;  //!< Soak-loop region length
    U8 m_pollSeed;            //!< Pattern seed of the current soak cycle

    U32 m_writeCount;      //!< Writes completed OK
    U32 m_readCount;       //!< Reads completed OK
    U32 m_verifyFailures;  //!< Verify reads that did not match
    U32 m_framErrors;      //!< Completions with a non-OK status
    U32 m_busyRejects;     //!< Requests this component rejected as busy
    U32 m_pollCycles;      //!< Soak write/verify pairs that matched
};

}  // namespace CuriosityReference

#endif
