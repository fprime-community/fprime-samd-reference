module CmdTlmTest {
    @ Stands in for "the proper component" a command is supposed to reach.
    @
    @ Test-only: this component exists so the CmdTlmTest topology has a
    @ concrete opcode -> component target to assert against, and a telemetry
    @ channel to exercise the packetizer/framer downlink path with a value the
    @ test controls.
    passive component Target {

        @ Record a value. The test builds a raw uplink frame carrying this
        @ command and confirms it lands here (and nowhere else) after passing
        @ through frameAccumulator -> deframer -> fprimeRouter -> cmdDisp.
        sync command SET_VALUE(
            value: U32 @< Value to record
        ) opcode 0

        @ Last value recorded by SET_VALUE
        telemetry Value: U32 id 0

        @ SET_VALUE was dispatched to this instance
        event ValueSet(
            value: U32 @< The new value
        ) \
            severity activity low \
            id 0 \
            format "Target value set to {}"

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channel handling
        import Fw.Channel

    }
}
