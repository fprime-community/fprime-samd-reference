@ Number of rate group member output ports for ActiveRateGroup
@ Note: left at the F Prime default. This deployment has no active components, so
@ Svc.ActiveRateGroup is never instantiated.
constant ActiveRateGroupOutputPorts = 10

@ Number of rate group member output ports for PassiveRateGroup
constant PassiveRateGroupOutputPorts = 8

@ Used to drive rate groups
@ Note: exactly the three rate groups this deployment defines (8 Hz, 1 Hz, 10 s).
constant RateGroupDriverRateGroupPorts = 3

@ Used for command and registration ports
@ Note: sizes compCmdReg/compCmdSend on Samd21.StaticCmdDispatcher, i.e. one slot per
@ component instance that declares at least one command. The reference topology fills 11.
constant CmdDispatcherComponentCommandPorts = 16

@ Used for uplink/sequencer buffer/response ports
@ Note: sizes seqCmdBuff/seqCmdIn/seqCmdStatus on the dispatcher, i.e. one slot per source
@ of command buffers. The reference topology has exactly one (fprimeRouter, from the
@ uplink chain); the second slot is headroom.
constant CmdDispatcherSequencePorts = 2

@ Used for dispatching sequences to command sequencers
constant SeqDispatcherSequencerPorts = 2

@ Used for sizing the command splitter input arrays
constant CmdSplitterPorts = CmdDispatcherSequencePorts

@ Number of static memory allocations
@ Note: sizes bufferAllocate/bufferDeallocate on Svc.StaticMemory. The single
@ commsBufferManager instance serves only frameAccumulator, on index 0. Each allocation
@ additionally reserves STATIC_MEMORY_ALLOCATION_SIZE bytes of RAM
@ (config-samd-reference/StaticMemoryConfig.hpp), so this one is a real 128 B/slot cost.
constant StaticMemoryAllocations = 1

@ Used to ping active components
constant HealthPingPorts = 25

@ Used for broadcasting completed file downlinks
constant FileDownCompletePorts = 1

@ Used for number of Fw::Com type ports supported by Svc::ComQueue
constant ComQueueComPorts = 2

@ Used for number of Fw::Buffer type ports supported by Svc::ComQueue
constant ComQueueBufferPorts = 1

@ Used for maximum number of connected buffer repeater consumers
constant BufferRepeaterOutputPorts = 10

@ Size of port array for DpManager
constant DpManagerNumPorts = 5

@ Size of data product routing port arrays for DpWriter
constant DpWriterNumPorts = 5

@ Size of processing port array for DpWriter
constant DpWriterNumProcPorts = 5

@ The size of a file name string
@ Note: trimmed from 240 because this deployment has no file system -- the only remaining
@ consumer is AssertFatalAdapterEventFileSize below, and assert sites report a file CRC
@ rather than a path (FW_ASSERT_LEVEL is FW_FILEID_ASSERT in FpConfig.h).
constant FileNameStringSize = 16

module Samd21 {
    struct FatalData {
        file: U32,
        lineNo: U32,
        numArgs: FwSizeStoreType,
        arg1: I32,
        arg2: I32,
        arg3: I32,
        arg4: I32,
        arg5: I32,
        arg6: I32,
    }

    struct FatalTime {
        timeBase: TimeBase  @< basis of time (defined by system)
        timeContext: FwTimeContextStoreType  @< user settable value. Could be reboot count, node, etc
        seconds: U32  @< seconds portion of Time
        useconds: U32  @< microseconds portion of Time
    }

    struct FatalPacket {
        $type: ComCfg.Apid
        $id: FwEventIdType,
        $time: FatalTime,
        data: FatalData,
    } default {
        $type = ComCfg.Apid.FW_PACKET_LOG,
        $id = 0x0,
    }
}

@ The size of an assert text string
constant FwAssertTextSize = sizeof(Samd21.FatalPacket)

@ The size of a file name in an AssertFatalAdapter event (leading-truncation)
@ Note: File names in assertion failures are also truncated by
@ the constants FwAssertTextSize (in this file) and FW_LOG_STRING_MAX_SIZE (set
@ in FpConstants.fpp)
constant AssertFatalAdapterEventFileSize = FileNameStringSize

@ The maximum size in bytes of the argument blob carried in a Svc::SeqArgs buffer
@ (CmdSeqIn / RUN / INVOKE).
constant SequenceArgumentsMaxSize = 12
