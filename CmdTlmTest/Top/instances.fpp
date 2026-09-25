module CmdTlmTest {

    enum FixedBaseIds {
        @ Common base id to use for components that have no dictionary items
        NO_DICTIONARY = 0xFFFF
    }

    # ----------------------------------------------------------------------
    # Uplink: bytes -> frameAccumulator -> deframer -> fprimeRouter -> cmdDisp -> target
    # ----------------------------------------------------------------------

    instance frameAccumulator: Svc.FrameAccumulator base id 0x0100 {
        phase Fpp.ToCpp.Phases.configObjects """
        Svc::FrameDetectors::FprimeFrameDetector frameDetector;
        """

        phase Fpp.ToCpp.Phases.configComponents """
        frameAccumulator.configure(
            ConfigObjects::CmdTlmTest_frameAccumulator::frameDetector,
            1,
            Allocation::frameAccumulatorAllocator,
            /* frame buffer size */ 128
        );
        """

        phase Fpp.ToCpp.Phases.tearDownComponents """
        frameAccumulator.cleanup();
        """
    }

    instance commsBufferManager: Svc.StaticMemory base id FixedBaseIds.NO_DICTIONARY

    instance deframer: Svc.FprimeDeframer base id 0x0110
    instance fprimeRouter: Samd21.FprimeRouter base id 0x0120
    instance cmdDisp: Samd21.StaticCmdDispatcher base id 0x0130
    instance target: CmdTlmTest.Target base id 0x0140

    # ----------------------------------------------------------------------
    # Downlink: target's telemetry channel -> tlm -> framer -> comSink
    # ----------------------------------------------------------------------

    instance tlm: Samd21.StaticTlmPacketizer base id 0x0150
    instance framer: Samd21.Framer base id 0x0160
    instance comSink: CmdTlmTest.ComSink base id FixedBaseIds.NO_DICTIONARY

    # ----------------------------------------------------------------------
    # Pattern-connection targets: every component above has a logOut and a
    # timeCaller that must land somewhere for the topology autocoder.
    # ----------------------------------------------------------------------

    instance eventSink: CmdTlmTest.EventSink base id FixedBaseIds.NO_DICTIONARY
    instance timeHandler: Svc.PosixTime base id FixedBaseIds.NO_DICTIONARY
}
