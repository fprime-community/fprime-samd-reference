// ======================================================================
// \title  GpioIn.cpp
// \author tumbar
// \brief  cpp file for GpioIn component implementation class
// ======================================================================

#include "CuriosityReference/GpioIn/GpioIn.hpp"
#include "Drv/Ports/GpioStatusEnumAc.hpp"

namespace CuriosityReference {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

GpioIn ::GpioIn(const char* const compName) : GpioInComponentBase(compName) {}

GpioIn ::~GpioIn() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void GpioIn ::transitionIn_handler(FwIndexType portNum, Os::RawTime& cycleStart) {
    (void)portNum;     // One driver instance drives this component
    (void)cycleStart;  // The edge timestamp is unused; the event carries the level

    // ISR CONTEXT -- see the declaration in GpioIn.hpp.
    //
    // The level is re-sampled here rather than carried in the notification,
    // because Drv.Gpio's gpioInterrupt port (Svc.Cycle) has no room for it. On a
    // fast pin the sampled level may therefore already be the level after a
    // second edge.
    Fw::Logic state;
    const Drv::GpioStatus status = this->readOut_out(0, state);
    if (status.e != Drv::GpioStatus::OP_OK) {
        // A mis-wired or unconfigured pin must not fault the board out of an
        // ISR. Report and drop the edge.
        this->log_WARNING_HI_ReadFailed(status);
        return;
    }

    this->log_ACTIVITY_LO_LevelTransitioned(state);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void GpioIn ::READ_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    Fw::Logic state;
    const Drv::GpioStatus status = this->readOut_out(0, state);
    if (status.e != Drv::GpioStatus::OP_OK) {
        // The driver's status is a runtime outcome reachable from the ground, so
        // it is reported and the command is failed. Asserting here would let a
        // single command against an unconfigured pin reset the board.
        this->log_WARNING_HI_ReadFailed(status);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->log_ACTIVITY_LO_ReadLevel(state);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace CuriosityReference
