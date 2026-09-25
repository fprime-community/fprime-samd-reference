module CmdTlmTest {
    @ Minimal event receiver, standing in for Samd21.PassiveDownlink's
    @ LogRecv port so the topology's `event connections` pattern has a
    @ target. Records the most recent event ID and a running count instead of
    @ packetizing, so tests can assert dispatch-level events (e.g.
    @ Samd21.StaticCmdDispatcher's InvalidCommand) without a full
    @ GTestBase-style event history.
    @
    @ Test-only.
    passive component EventSink {

        @ Port for receiving events
        sync input port LogRecv: Fw.Log

    }
}
