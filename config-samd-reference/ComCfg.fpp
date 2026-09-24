@ The width of packet descriptors when they are serialized by the framework
dictionary type FwPacketDescriptorType = U8

module ComCfg {

    @ Spacecraft ID (10 bits) for CCSDS Data Link layer
    dictionary constant SpacecraftId = 0x0044

    @ Fixed size of CCSDS TM frames
    dictionary constant TmFrameFixedSize = 1024  # Needs to be at least COM_BUFFER_MAX_SIZE + (2 * SpacePacketHeaderSize) + 1

    @ Upper Bound on Fixed size of CCSDS AOS frames
    constant AosMaxFrameFixedSize = 1536

    @ Aggregation buffer for ComAggregator component
    @ Note: inert in this deployment -- Svc.ComAggregator is neither instantiated nor built
    @ (see the non-CCSDS note at the top of this file). Kept so that the file stays a
    @ drop-in replacement for the F Prime default.
    constant AggregationSize = TmFrameFixedSize - 6 - 6 - 1 - 2  # TM primary header (6) + Space Packet primary header (6) + 1 idle byte + TM trailer/CRC (2)

    @ Packet Version Number (3 bits) for CCSDS packets
    enum Pvn : U8 {
        PVN_VERSION_0 = 0  @< Version 0
        PVN_VERSION_1 = 1  @< Version 1
        PVN_VERSION_2 = 2  @< Version 2
        PVN_VERSION_3 = 3  @< Version 3
        PVN_VERSION_4 = 4  @< Version 4
        PVN_VERSION_5 = 5  @< Version 5
        PVN_VERSION_6 = 6  @< Version 6
        PVN_VERSION_7 = 7  @< Version 7
        INVALID_UNINITIALIZED = 0xFF  @< Invalid/uninitialized value
    } default INVALID_UNINITIALIZED

    @ Packet type identifiers. Narrowed to FwPacketDescriptorType (U8) here, so the CCSDS
    @ 11-bit APID space is not available -- see the note at the top of this file.
    dictionary enum Apid : FwPacketDescriptorType {
        # APIDs prefixed with FW are reserved for F Prime and need to be present
        # in the enumeration. Their values can be changed
        FW_PACKET_COMMAND        = 0x00  @< Command packet type - incoming
        FW_PACKET_TELEM          = 0x01  @< Telemetry packet type - outgoing
        FW_PACKET_LOG            = 0x02  @< Log type - outgoing
        FW_PACKET_FILE           = 0x03  @< File type - incoming and outgoing
        FW_PACKET_PACKETIZED_TLM = 0x04  @< Packetized telemetry packet type
        FW_PACKET_DP             = 0x05  @< Data Product packet type
        FW_PACKET_IDLE           = 0x06  @< F Prime idle
        FW_PACKET_PARAM          = 0x07  @< Parameter value type - outgoing (unused here: no parameters)
        SPP_IDLE_PACKET          = 0xFC  @< SPP Idle (cannot be the standard 0x07FF in a U8)
        FW_PACKET_UNKNOWN        = 0xFD  @< F Prime unknown packet
        FW_PACKET_HAND           = 0xFE  @< F Prime handshake
        INVALID_UNINITIALIZED    = 0xFF  @< Anything equal or higher value is invalid and should not be used
    } default INVALID_UNINITIALIZED

    @ Type used to pass context info between components during framing/deframing
    struct FrameContext {
        comQueueIndex: FwIndexType  @< Queue Index used by the ComQueue, other components shall not modify
        apid: Apid                  @< 11 bits APID in CCSDS
        sequenceCount: U16          @< 14 bit Sequence count - sequence count is incremented per APID
        vcId: U8                    @< 6 bit Virtual Channel ID - used for AOS, TC, and TM Protocols
        sendNow: bool               @< Flag to AOS Framer that the Frame this packet goes into should be sent ASAP
        hasSecHdr: bool             @< Flag indicating if packet has secondary header
        pvn: Pvn                    @< Packet Version Number (3 bits)

    } default {
        comQueueIndex = 0
        apid = Apid.FW_PACKET_UNKNOWN
        sequenceCount = 0
        vcId = 1
        sendNow = false
        hasSecHdr = false
        pvn = Pvn.PVN_VERSION_0
    }

}
