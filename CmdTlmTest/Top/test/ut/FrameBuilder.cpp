// ======================================================================
// \title  FrameBuilder.cpp
// \brief  cpp file for FrameBuilder -- see FrameBuilder.hpp
// ======================================================================

#include "CmdTlmTest/Top/test/ut/FrameBuilder.hpp"

#include "Fw/Types/Assert.hpp"
#include "Svc/FprimeDeframer/FprimeDeframerComponentAc.hpp"  // brings in ComCfg::Apid
#include "Svc/FprimeProtocol/FrameHeaderSerializableAc.hpp"
#include "Svc/FprimeProtocol/FrameTrailerSerializableAc.hpp"
#include "Utils/Hash/Hash.hpp"

namespace CmdTlmTest {
namespace TestUtil {

FwSizeType FrameBuilder ::buildCommandFrame(U8* out,
                                            FwSizeType capacity,
                                            FwOpcodeType opcode,
                                            const U8* args,
                                            FwSizeType argsSize) {
    // descriptor (this project's 1-byte FwPacketDescriptorType) + opcode + args
    const FwSizeType payloadSize = sizeof(FwPacketDescriptorType) + sizeof(FwOpcodeType) + argsSize;
    const FwSizeType frameSize = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE + payloadSize +
                                 Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    FW_ASSERT(frameSize <= capacity, static_cast<FwAssertArgType>(frameSize));

    Fw::ExternalSerializeBuffer buffer(out, capacity);

    // ---------------- Frame header ----------------
    // startWord takes FrameHeader's FPP `default` value (0xdeadbeef); only lengthField
    // needs to be set here.
    Svc::FprimeProtocol::FrameHeader header;
    header.set_lengthField(static_cast<U32>(payloadSize));
    Fw::SerializeStatus status = header.serializeTo(buffer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));

    // ---------------- Payload: descriptor + opcode + args ----------------
    status = buffer.serializeFrom(static_cast<FwPacketDescriptorType>(ComCfg::Apid::FW_PACKET_COMMAND));
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
    status = buffer.serializeFrom(opcode);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
    if (argsSize > 0) {
        status = buffer.serializeFrom(args, argsSize, Fw::Serialization::OMIT_LENGTH);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
    }

    // ---------------- Frame trailer: CRC over header + payload ----------------
    Utils::Hash hash;
    Utils::HashBuffer hashBuffer;
    hash.init();
    hash.update(buffer.getBuffAddr(), static_cast<U32>(buffer.getSize()));
    hash.finalize(hashBuffer);

    Svc::FprimeProtocol::FrameTrailer trailer;
    trailer.set_crcField(hashBuffer.asBigEndianU32());
    status = trailer.serializeTo(buffer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));

    return buffer.getSize();
}

}  // namespace TestUtil
}  // namespace CmdTlmTest
