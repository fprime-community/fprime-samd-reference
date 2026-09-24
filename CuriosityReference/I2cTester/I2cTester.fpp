module CuriosityReference {
    @ Exercises the Samd21.I2cDriver -- and the Samd21.DmaDriver behind it --
    @ end-to-end without modeling any real part. The 7-bit target address, the
    @ register offset written before each read, and the number of bytes read
    @ back are all settable from the ground, so the component can poke at
    @ whatever device happens to be on the bus (or at nothing at all, to
    @ exercise the address-NACK path). Every failure mode the driver can report
    @ is broken out into its own counter, so a bring-up problem is diagnosable
    @ from telemetry alone.
    @
    @ This is also the worked example of the asynchronous I2C client contract:
    @ a request is issued on an output port and the outcome arrives later on a
    @ callback input port. See docs/sdd.md before writing a device component
    @ against Samd21.I2cDriver -- two of its rules (the synchronous busy-reject
    @ callback, and the client-owned DMA buffers) are easy to get wrong.
    passive component I2cTester {
        # ------------------------------------------------------------------
        # Job Ports
        # ------------------------------------------------------------------

        @ Rate-group tick. When polling is enabled (see SET_POLLING) each tick
        @ kicks off one write-read against the configured address and offset.
        @ Polling is off at boot so a freshly reset board does not drive the bus.
        sync input port schedIn: Svc.Sched

        # ------------------------------------------------------------------
        # I2C Interface Ports
        # ------------------------------------------------------------------

        @ Output port for write-read I2C transactions: write the register
        @ offset, then read [ReadLength] bytes back across a repeated START.
        output port i2cWriteReadOut: Drv.I2cWriteReadRequest

        @ Callback port invoked when an I2C write-read completes.
        @ The I2cDriver delivers this from the main context (its own activeIn),
        @ so the handler may drive telemetry, events, and command replies
        @ directly.
        @ NOTE: when the driver is already busy it invokes this callback
        @ synchronously from inside i2cWriteReadOut_out, so the component state
        @ must always be committed before a request is issued.
        sync input port writeReadCompleteIn: Drv.I2cWriteReadCallback

        @ Output port for write-only I2C transactions.
        @ Used by WRITE_ONCE to write [regOffset, value] to the target.
        output port i2cWriteOut: Drv.I2cRequest

        @ Callback port invoked when a write-only I2C transaction completes
        sync input port writeCompleteIn: Drv.I2cCallback

        # ------------------------------------------------------------------
        # Commands
        # ------------------------------------------------------------------

        @ Set the 7-bit I2C address the tester addresses
        sync command SET_ADDRESS(
            addr: U8 @< 7-bit address; must be in [0x08, 0x77]
        ) \
            opcode 0

        @ Set the register offset written before each read, and how many bytes
        @ to read back afterwards
        sync command SET_REGISTER(
            regOffset: U8 @< Register/offset byte written before the read
            readLen: U8   @< Bytes to read back; must be in [1, 8]
        ) \
            opcode 1

        @ Issue a single write-read using the current address, offset, and length
        sync command READ_ONCE() opcode 2

        @ Issue a single write of [regOffset, value] to the current address
        sync command WRITE_ONCE(
            value: U8 @< Data byte written after the register offset
        ) \
            opcode 3

        @ Enable or disable the schedIn-driven periodic read
        sync command SET_POLLING(
            enable: bool @< true to issue one write-read per schedIn tick
        ) \
            opcode 4

        # ------------------------------------------------------------------
        # Types
        # ------------------------------------------------------------------

        @ Transaction the component is currently executing
        enum State: U8 {
            IDLE       @< No transaction in flight; new requests accepted
            READ_SCHED @< Periodic write-read from schedIn; no reply expected
            READ_CMD   @< One-shot write-read from READ_ONCE; replies on cmdResponse
            WRITE_CMD  @< One-shot write from WRITE_ONCE; replies on cmdResponse
        } default IDLE

        # ------------------------------------------------------------------
        # Events
        # ------------------------------------------------------------------

        @ A request arrived while another transaction was still in flight
        event Busy(
            op: State      @< Transaction that was rejected
            current: State @< Transaction already in flight
        ) \
            severity warning low \
            id 0 \
            format "I2cTester busy: op={}, current={}"

        @ An I2C transaction failed. The status is the finest granularity the
        @ driver exposes to a client; the driver's own I2cBusError event carries
        @ the underlying SERCOM error flags.
        event I2cError(
            status: Drv.I2cStatus @< Status reported by the driver
            addr: U8              @< 7-bit address that was addressed
            regOffset: U8         @< Register offset of the failed transaction
        ) \
            severity warning high \
            id 1 \
            format "I2cTester transaction failed: status={}, addr=0x{x}, reg=0x{x}"

        @ A write-read completed with OK status but the driver returned a buffer
        @ smaller than the requested length
        event ShortRead(
            expected: U8 @< Bytes requested
            actual: U8   @< Bytes returned
        ) \
            severity warning high \
            id 2 \
            format "I2cTester short read: expected {} bytes, got {}"

        @ A configure() argument or command argument was out of range; the
        @ setting was left unchanged
        event InvalidConfig(
            value: U8 @< The rejected value
        ) \
            severity warning high \
            id 3 \
            format "I2cTester rejected out-of-range value {}"

        @ A write-read completed successfully. data0/data1 carry the bytes read,
        @ packed MSB-first (byte 0 in the most significant octet of data0) and
        @ zero-padded past [length].
        event ReadComplete(
            regOffset: U8 @< Register offset the read started at
            length: U8    @< Bytes read
            data0: U32    @< Read bytes 0-3, MSB first
            data1: U32    @< Read bytes 4-7, MSB first
        ) \
            severity activity low \
            id 4 \
            format "I2cTester read reg 0x{x} ({} bytes): 0x{x} 0x{x}"

        @ A write-only transaction completed successfully
        event WriteComplete(
            regOffset: U8 @< Register offset written
            value: U8     @< Data byte written after the offset
        ) \
            severity activity low \
            id 5 \
            format "I2cTester wrote reg 0x{x} = 0x{x}"

        # ------------------------------------------------------------------
        # Telemetry
        # ------------------------------------------------------------------

        @ Bytes 0-3 of the last successful write-read, MSB first
        telemetry ReadData0: U32 id 0

        @ Bytes 4-7 of the last successful write-read, MSB first
        telemetry ReadData1: U32 id 1

        @ Bytes read per transaction. This is both the configured read length
        @ and, because a completion carrying fewer bytes is rejected as a short
        @ read, the number of bytes behind [ReadData0] / [ReadData1].
        telemetry ReadLength: U8 id 2

        @ Count of transactions that completed successfully (reads and writes)
        telemetry TxnCount: U32 id 3

        @ Count of requests this component rejected itself because one of its
        @ own transactions was still in flight
        telemetry BusyRejects: U32 id 4

        @ Count of completions with I2C_ADDRESS_ERR. The SAMD21 driver does not
        @ currently produce this status -- see docs/sdd.md -- so a non-zero
        @ value here means the driver grew a new failure path.
        telemetry AddressErrors: U32 id 5

        @ Count of completions with I2C_WRITE_ERR -- the target NACKed the write
        @ (or the register-offset write of a write-read). Nothing on the bus at
        @ the configured address shows up here.
        telemetry WriteErrors: U32 id 6

        @ Count of completions with I2C_READ_ERR -- the read phase failed
        telemetry ReadErrors: U32 id 7

        @ Count of requests dropped because the I2C output port was not
        @ connected, plus any completion reporting I2C_OPEN_ERR
        telemetry OpenErrors: U32 id 8

        @ Count of completions with I2C_OTHER_ERR -- the driver was busy when
        @ the request arrived, or its stall watchdog force-recovered the
        @ transaction
        telemetry OtherErrors: U32 id 9

        @ Count of successful reads that returned fewer bytes than requested
        telemetry ShortReads: U32 id 10

        @ Current 7-bit target address, for ground readback
        telemetry TargetAddress: U8 id 11

        @ Current register offset, for ground readback
        telemetry RegisterOffset: U8 id 12

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################

        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel
    }
}
