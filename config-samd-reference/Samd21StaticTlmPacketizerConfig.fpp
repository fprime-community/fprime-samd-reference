module Samd21 {
    @ Size of StaticTlmPacketizer.pktSendIn. The port index is the packet id, so this
    @ must be strictly greater than the largest packet id driven from a port.
    @ CuriosityReference drives the Fram packet (id 2) from rg1Hz; Error/Tester/Health
    @ (ids 0, 1, 3) remain SEND_PKT-only.
    constant NUM_TLM_PACKETS = 4
}
