// ======================================================================
// \title  FrameBuilder.hpp
// \brief  Builds raw uplink frames for the CmdTlmTest topology's gtest cases
//
// Constructs frames using the real Svc::FprimeProtocol::FrameHeader/
// FrameTrailer serializable types and this project's own ComCfg (a 1-byte
// FwPacketDescriptorType -- see config-samd-reference/ComCfg.fpp), rather
// than a hand-rolled byte array copied from the generic (2-byte-descriptor)
// example in Svc/FprimeDeframer/test/ut/FprimeDeframerTester.cpp. That
// generic example would silently miscount the frame against this project's
// narrowed config.
// ======================================================================

#ifndef CmdTlmTest_Test_FrameBuilder_HPP
#define CmdTlmTest_Test_FrameBuilder_HPP

#include "Fw/FPrimeBasicTypes.hpp"

namespace CmdTlmTest {
namespace TestUtil {

class FrameBuilder {
  public:
    //! Build a complete uplink frame carrying a command packet:
    //! [ FrameHeader | descriptor=FW_PACKET_COMMAND | opcode | args | FrameTrailer ]
    //!
    //! \param out       Destination buffer
    //! \param capacity  Size of `out`, in bytes
    //! \param opcode    Command opcode to embed
    //! \param args      Already-serialized command argument bytes (may be nullptr if argsSize is 0)
    //! \param argsSize  Number of bytes in `args`
    //! \return Number of bytes written to `out`
    static FwSizeType buildCommandFrame(U8* out,
                                        FwSizeType capacity,
                                        FwOpcodeType opcode,
                                        const U8* args,
                                        FwSizeType argsSize);
};

}  // namespace TestUtil
}  // namespace CmdTlmTest

#endif
