// ======================================================================
// \title  EventSink.cpp
// \brief  cpp file for EventSink component implementation class
// ======================================================================

#include "CmdTlmTest/EventSink/EventSink.hpp"

namespace CmdTlmTest {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

EventSink ::EventSink(const char* const compName)
    : EventSinkComponentBase(compName), m_lastEventId(0), m_eventCount(0) {}

EventSink ::~EventSink() {}

// ----------------------------------------------------------------------
// Test accessors
// ----------------------------------------------------------------------

FwEventIdType EventSink ::getLastEventId() const {
    return this->m_lastEventId;
}

U32 EventSink ::getEventCount() const {
    return this->m_eventCount;
}

void EventSink ::reset() {
    this->m_lastEventId = 0;
    this->m_eventCount = 0;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void EventSink ::LogRecv_handler(FwIndexType portNum,
                                 FwEventIdType id,
                                 Fw::Time& timeTag,
                                 const Fw::LogSeverity& severity,
                                 Fw::LogBuffer& args) {
    (void)portNum;
    (void)timeTag;
    (void)severity;
    (void)args;
    this->m_lastEventId = id;
    this->m_eventCount++;
}

}  // namespace CmdTlmTest
