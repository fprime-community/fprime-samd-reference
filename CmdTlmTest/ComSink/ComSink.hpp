// ======================================================================
// \title  ComSink.hpp
// \brief  hpp file for ComSink component implementation class
//
// Test-only. Stands in for the byte-stream driver on both the downlink
// (Samd21.Framer) and uplink buffer-return (Svc.ComStub) sides -- see
// ComSink.fpp for the port-by-port mapping.
// ======================================================================

#ifndef CmdTlmTest_ComSink_HPP
#define CmdTlmTest_ComSink_HPP

#include "CmdTlmTest/ComSink/ComSinkComponentAc.hpp"
#include "samd-config/FramerConfig.hpp"

namespace CmdTlmTest {

class ComSink final : public ComSinkComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ComSink object
    ComSink(const char* const compName  //!< The component name
    );

    //! Destroy ComSink object
    ~ComSink();

  public:
    // ----------------------------------------------------------------------
    // Test accessors
    // ----------------------------------------------------------------------

    //! Pointer to the most recently captured buffer, or nullptr if $send has
    //! never been called
    const U8* getCapturedData() const;

    //! Size of the most recently captured buffer, or 0 if $send has never
    //! been called
    FwSizeType getCapturedSize() const;

    //! Number of times $send has been called
    U32 getSendCount() const;

    //! Clear the captured buffer between test cases
    void reset();

    //! Fire the `ready` output port, signaling the (stand-in) driver is
    //! ready to send. Call once during topology setup.
    void signalReady();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for $send
    void send_handler(FwIndexType portNum,  //!< The port number
                      Fw::Buffer& fwBuffer  //!< The buffer to send
                      ) override;

    //! Handler implementation for dataReturnIn. Nothing to do -- see
    //! ComSink.fpp.
    void dataReturnIn_handler(FwIndexType portNum,                 //!< The port number
                              Fw::Buffer& fwBuffer,                //!< The returned buffer
                              const ComCfg::FrameContext& context  //!< The buffer's context
                              ) override;

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    // Sized off the same constant Samd21::Framer sizes its own TX buffers
    // with, so a capture can never be truncated relative to what the framer
    // could actually produce.
    U8 m_captured[Samd21::FramerConfig::FRAMER_TX_BUFFER_SIZE];
    FwSizeType m_capturedSize;
    U32 m_sendCount;
};

}  // namespace CmdTlmTest

#endif
