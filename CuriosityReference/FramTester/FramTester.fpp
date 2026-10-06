module CuriosityReference {
    @ Exercises Fram.FramDriver -- and the Samd21.SpiDriver and Samd21.DmaDriver
    @ behind it -- end-to-end from the ground. WRITE_PATTERN fills a region of the
    @ F-RAM with a deterministic byte pattern, READ_VERIFY reads it back and
    @ compares, READ dumps raw bytes into an event, and SET_POLLING turns on a
    @ write-then-verify soak loop driven from the rate group. Offsets and lengths
    @ are passed to the driver unchecked on purpose: the ground can provoke each
    @ Fram.Status rejection (OUT_OF_RANGE, TOO_LARGE, BUSY) and see it counted.
    @
    @ This is also the worked example of the Fram.FramDriver client contract: a
    @ request is issued on an output port and the outcome arrives later on the
    @ matching callback port, from the main context -- except when the driver
    @ rejects the request, in which case the callback runs synchronously inside
    @ the request call. Component state must therefore be committed before a
    @ request is issued. See docs/sdd.md.
    passive component FramTester {
        # ------------------------------------------------------------------
        # Job Ports
        # ------------------------------------------------------------------
        @ Rate-group tick. When polling is enabled (see SET_POLLING) each tick
        @ writes a fresh pattern to the poll region and verifies it on the next
        @ tick. Polling is off at boot so a freshly reset board leaves the F-RAM
        @ contents alone.
        sync input port schedIn: Svc.Sched

        # ------------------------------------------------------------------
        # F-RAM Interface Ports
        # ------------------------------------------------------------------
        @ Read request: fill the supplied buffer from the F-RAM starting at offset
        output port framReadOut: Fram.Request

        @ Read completion. The buffer is the one passed to framReadOut.
        sync input port readCompleteIn: Fram.Reply

        @ Write request: store the supplied buffer into the F-RAM starting at offset
        output port framWriteOut: Fram.Request

        @ Write completion. The buffer is the one passed to framWriteOut.
        sync input port writeCompleteIn: Fram.Reply

        # ------------------------------------------------------------------
        # Commands
        # ------------------------------------------------------------------
        @ Write length bytes of pattern (seed, seed+1, ...) starting at offset
        sync command WRITE_PATTERN(
            offset: U32 @< F-RAM byte offset of the first byte
            length: U8  @< Bytes to write; 0..Fram.Config.MaxTransfer (0 is forwarded so the driver's TOO_LARGE reject can be exercised)
            seed: U8    @< Value of the first byte; each following byte increments by one
        ) \
            opcode 0

        @ Read length bytes starting at offset and compare against the pattern
        @ (seed, seed+1, ...). The first mismatch is reported in PatternMismatch.
        sync command READ_VERIFY(
            offset: U32 @< F-RAM byte offset of the first byte
            length: U8  @< Bytes to read; 0..Fram.Config.MaxTransfer (0 is forwarded so the driver's TOO_LARGE reject can be exercised)
            seed: U8    @< Expected value of the first byte
        ) \
            opcode 1

        @ Read length bytes starting at offset and report the first eight in ReadComplete
        sync command READ(
            offset: U32 @< F-RAM byte offset of the first byte
            length: U8  @< Bytes to read; 0..Fram.Config.MaxTransfer (0 is forwarded so the driver's TOO_LARGE reject can be exercised)
        ) \
            opcode 2

        @ Enable or disable the schedIn-driven write/verify soak loop
        sync command SET_POLLING(
            enable: bool @< true to alternate WRITE_PATTERN / READ_VERIFY on the poll region each tick
        ) \
            opcode 3

        @ Set the region the soak loop writes and verifies
        sync command SET_POLL_REGION(
            offset: U32 @< F-RAM byte offset of the poll region
            length: U8  @< Bytes per poll transaction; must be in [1, Fram.Config.MaxTransfer]
        ) \
            opcode 4

        # ------------------------------------------------------------------
        # Types
        # ------------------------------------------------------------------
        @ Transaction the component is currently executing
        enum State: U8 {
            IDLE        @< No transaction in flight; new requests accepted
            WRITE_CMD   @< WRITE_PATTERN in flight; replies on cmdResponse
            VERIFY_CMD  @< READ_VERIFY in flight; replies on cmdResponse
            READ_CMD    @< READ in flight; replies on cmdResponse
            POLL_WRITE  @< Soak-loop pattern write from schedIn
            POLL_VERIFY @< Soak-loop verify read from schedIn
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
            format "FramTester busy: op={}, current={}"

        @ The driver reported a non-OK status for a transaction
        event FramError(
            op: State           @< Transaction that failed
            $status: Fram.Status @< Status reported by the driver
            offset: U32         @< Offset of the failed transaction
        ) \
            severity warning high \
            id 1 \
            format "FramTester {} failed: status={}, offset=0x{x}"

        @ A verify read returned data that does not match the expected pattern
        event PatternMismatch(
            offset: U32  @< Offset of the first mismatching byte
            expected: U8 @< Pattern byte expected there
            actual: U8   @< Byte read back
        ) \
            severity warning high \
            id 2 \
            format "FramTester pattern mismatch at 0x{x}: expected 0x{x}, read 0x{x}"

        @ A command argument was out of range; nothing was issued
        event InvalidArgument(
            value: U32 @< The rejected value
        ) \
            severity warning high \
            id 3 \
            format "FramTester rejected out-of-range argument {}"

        @ A READ completed. data0/data1 carry the first eight bytes, MSB first
        @ (byte 0 in the most significant octet of data0); bytes past the
        @ requested length are zero.
        event ReadComplete(
            offset: U32 @< Offset the read started at
            length: U8  @< Bytes read
            data0: U32  @< Bytes 0-3, MSB first
            data1: U32  @< Bytes 4-7, MSB first
        ) \
            severity activity low \
            id 4 \
            format "FramTester read 0x{x} ({} bytes): 0x{x} 0x{x}"

        @ A WRITE_PATTERN completed successfully
        event WriteComplete(
            offset: U32 @< Offset the write started at
            length: U8  @< Bytes written
            seed: U8    @< First pattern byte
        ) \
            severity activity low \
            id 5 \
            format "FramTester wrote 0x{x} ({} bytes) seed 0x{x}"

        @ A READ_VERIFY completed and every byte matched
        event VerifyComplete(
            offset: U32 @< Offset the verify started at
            length: U8  @< Bytes verified
            seed: U8    @< First pattern byte
        ) \
            severity activity low \
            id 6 \
            format "FramTester verified 0x{x} ({} bytes) seed 0x{x}"

        # ------------------------------------------------------------------
        # Telemetry
        # ------------------------------------------------------------------
        @ Writes completed with OK status (commands and soak loop)
        telemetry WriteCount: U32 id 0
        @ Reads completed with OK status (READ, READ_VERIFY and soak loop)
        telemetry ReadCount: U32 id 1
        @ Verify reads whose data did not match the pattern
        telemetry VerifyFailures: U32 id 2
        @ Transactions the driver completed with a non-OK status
        telemetry FramErrors: U32 id 3
        @ Requests this component rejected because one was already in flight
        telemetry BusyRejects: U32 id 4
        @ Status of the most recent completion
        telemetry LastStatus: Fram.Status id 5
        @ First four bytes of the most recent READ, MSB first
        telemetry ReadData0: U32 id 6
        @ Bytes four to seven of the most recent READ, MSB first
        telemetry ReadData1: U32 id 7
        @ Soak-loop write/verify pairs that completed and matched
        telemetry PollCycles: U32 id 8

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        time get port timeCaller
        import Fw.Command
        import Fw.Event
        import Fw.Channel
    }
}
