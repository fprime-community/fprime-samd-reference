module CuriosityReference {
    @ Counts rate-group ticks and publishes the count as telemetry.
    @
    @ This is the smallest useful component in the reference deployment. It
    @ demonstrates the baremetal scheduling model end to end: a passive
    @ component with no thread of its own, woken only by a Svc.Sched tick that
    @ originates in the Samd21.RtcDriver hardware timer and is fanned out by
    @ Samd21.PassiveRateGroupDriver / Svc.PassiveRateGroup.
    @
    @ The channel is rewritten on every tick, which latches it into the Health
    @ packet held by Samd21.StaticTlmPacketizer. The reference topology leaves
    @ tlm.pktSendIn unconnected, so that packet reaches the ground only when the
    @ ground polls it with tlm.SEND_PKT -- and two successive Health packets
    @ whose Counter advanced are direct evidence the whole chain is alive:
    @ RTC interrupt -> Samd21.PassiveRateGroupDriver -> Svc.PassiveRateGroup ->
    @ this component -> Samd21.StaticTlmPacketizer -> Samd21.Framer ->
    @ Samd21.UsartDriver. A Counter that does not advance between two polls is
    @ the first thing to look at when the board goes quiet.
    passive component FrameworkHealth {

        @ Rate-group tick. Wired to the 10 s rate group (rg10s) in the reference
        @ topology; one tick publishes one Counter sample.
        sync input port schedIn: Svc.Sched

        @ Enable or disable telemetry emission on this component.
        @ Emission is enabled at construction.
        sync command SET_STATE(
            $state: bool @< true to publish Counter on every tick
        ) \
            opcode 0

        @ Number of rate-group ticks observed while emission was enabled
        telemetry Counter: U32 id 0

        @ Emission was enabled or disabled by SET_STATE
        event EmissionStateSet(
            $state: bool @< The new emission state
        ) \
            severity activity high \
            id 0 \
            format "FrameworkHealth telemetry emission set to {}"

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
