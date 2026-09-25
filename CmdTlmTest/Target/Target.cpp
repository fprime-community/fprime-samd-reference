// ======================================================================
// \title  Target.cpp
// \brief  cpp file for Target component implementation class
// ======================================================================

#include "CmdTlmTest/Target/Target.hpp"

namespace CmdTlmTest {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Target ::Target(const char* const compName) : TargetComponentBase(compName), m_value(0), m_setCount(0) {}

Target ::~Target() {}

// ----------------------------------------------------------------------
// Test accessors
// ----------------------------------------------------------------------

U32 Target ::getValue() const {
    return this->m_value;
}

U32 Target ::getSetCount() const {
    return this->m_setCount;
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Target ::SET_VALUE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 value) {
    this->m_value = value;
    this->m_setCount++;
    this->tlmWrite_Value(value);
    this->log_ACTIVITY_LO_ValueSet(value);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace CmdTlmTest
