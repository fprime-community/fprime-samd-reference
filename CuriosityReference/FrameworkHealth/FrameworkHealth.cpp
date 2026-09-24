// ======================================================================
// \title  FrameworkHealth.cpp
// \author tumbar
// \brief  cpp file for FrameworkHealth component implementation class
// ======================================================================

#include "CuriosityReference/FrameworkHealth/FrameworkHealth.hpp"

namespace CuriosityReference {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FrameworkHealth ::FrameworkHealth(const char* const compName)
    : FrameworkHealthComponentBase(compName), m_running(true), m_counter(0) {}

FrameworkHealth ::~FrameworkHealth() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void FrameworkHealth ::schedIn_handler(FwIndexType portNum, U32 context) {
    (void)portNum;  // One rate group drives this component
    (void)context;  // Call order within the rate group is not used

    if (this->m_running) {
        // Publish first, then advance, so the first sample the ground ever sees
        // is 0. tlmWrite_* timestamps the sample through timeCaller.
        this->tlmWrite_Counter(this->m_counter);
        this->m_counter++;
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void FrameworkHealth ::SET_STATE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool state) {
    this->m_running = state;
    // Acknowledge the state change out-of-band as well: with emission disabled
    // the Counter channel stops updating, and the event is what distinguishes
    // "commanded off" from "the board stopped ticking".
    this->log_ACTIVITY_HI_EmissionStateSet(state);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace CuriosityReference
