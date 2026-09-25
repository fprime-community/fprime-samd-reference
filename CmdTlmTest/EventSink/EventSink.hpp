// ======================================================================
// \title  EventSink.hpp
// \brief  hpp file for EventSink component implementation class
//
// Test-only. See EventSink.fpp.
// ======================================================================

#ifndef CmdTlmTest_EventSink_HPP
#define CmdTlmTest_EventSink_HPP

#include "CmdTlmTest/EventSink/EventSinkComponentAc.hpp"

namespace CmdTlmTest {

class EventSink final : public EventSinkComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct EventSink object
    EventSink(const char* const compName  //!< The component name
    );

    //! Destroy EventSink object
    ~EventSink();

  public:
    // ----------------------------------------------------------------------
    // Test accessors
    // ----------------------------------------------------------------------

    //! ID of the most recently received event, or 0 if none has been
    //! received
    FwEventIdType getLastEventId() const;

    //! Number of events received since the last reset()
    U32 getEventCount() const;

    //! Clear the recorded event id/count between test cases
    void reset();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for LogRecv
    void LogRecv_handler(FwIndexType portNum,              //!< The port number
                         FwEventIdType id,                 //!< Log ID
                         Fw::Time& timeTag,                //!< Time Tag
                         const Fw::LogSeverity& severity,  //!< The severity argument
                         Fw::LogBuffer& args               //!< Buffer containing serialized log entry
                         ) override;

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    FwEventIdType m_lastEventId;
    U32 m_eventCount;
};

}  // namespace CmdTlmTest

#endif
