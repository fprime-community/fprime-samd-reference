// ======================================================================
// \title  GpioOut.cpp
// \author tumbar
// \brief  cpp file for GpioOut component implementation class
// ======================================================================

#include "CuriosityReference/GpioOut/GpioOut.hpp"
#include "Drv/Ports/GpioStatusEnumAc.hpp"

namespace CuriosityReference {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

GpioOut ::GpioOut(const char* const compName) : GpioOutComponentBase(compName) {}

GpioOut ::~GpioOut() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void GpioOut ::SET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Fw::Logic& state) {
    // Drv.GpioWrite is a returning port. Discarding the status would report OK
    // for a write the driver refused -- e.g. an output pin whose driver instance
    // was never configured in the topology's startTasks phase.
    const Drv::GpioStatus status = this->write_out(0, state);
    if (status.e != Drv::GpioStatus::OP_OK) {
        this->log_WARNING_HI_WriteFailed(status, state);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace CuriosityReference
