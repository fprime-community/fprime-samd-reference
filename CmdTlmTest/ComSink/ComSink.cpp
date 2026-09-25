// ======================================================================
// \title  ComSink.cpp
// \brief  cpp file for ComSink component implementation class
// ======================================================================

#include "CmdTlmTest/ComSink/ComSink.hpp"
#include "Drv/ByteStreamDriverModel/ByteStreamStatusEnumAc.hpp"

#include <cstring>

namespace CmdTlmTest {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ComSink ::ComSink(const char* const compName) : ComSinkComponentBase(compName), m_capturedSize(0), m_sendCount(0) {}

ComSink ::~ComSink() {}

// ----------------------------------------------------------------------
// Test accessors
// ----------------------------------------------------------------------

const U8* ComSink ::getCapturedData() const {
    return this->m_capturedSize == 0 ? nullptr : this->m_captured;
}

FwSizeType ComSink ::getCapturedSize() const {
    return this->m_capturedSize;
}

U32 ComSink ::getSendCount() const {
    return this->m_sendCount;
}

void ComSink ::reset() {
    this->m_capturedSize = 0;
    this->m_sendCount = 0;
}

void ComSink ::signalReady() {
    this->ready_out(0);
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ComSink ::send_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    // Frames sent here are a handful of bytes; a size beyond the capture
    // buffer would mean Samd21.Framer produced more than its own
    // FramerConfig::FRAMER_TX_BUFFER_SIZE, which is a bug in the framer, not
    // something this test double should silently work around.
    FW_ASSERT(fwBuffer.getSize() <= sizeof(this->m_captured), static_cast<FwAssertArgType>(fwBuffer.getSize()));
    (void)memcpy(this->m_captured, fwBuffer.getData(), fwBuffer.getSize());
    this->m_capturedSize = fwBuffer.getSize();
    this->m_sendCount++;

    // Nothing to wait on -- hand the buffer straight back.
    this->sendReturnOut_out(0, fwBuffer, Drv::ByteStreamStatus::OP_OK);
}

void ComSink ::dataReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer, const ComCfg::FrameContext& context) {
    (void)portNum;
    (void)fwBuffer;
    (void)context;
}

}  // namespace CmdTlmTest
