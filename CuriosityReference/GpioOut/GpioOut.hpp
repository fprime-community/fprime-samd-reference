// ======================================================================
// \title  GpioOut.hpp
// \author tumbar
// \brief  hpp file for GpioOut component implementation class
//
// Consumer side of the Samd21::GpioDriver output path, and the deployment's
// shortest command-to-hardware path: uplink -> Samd21::StaticCmdDispatcher ->
// SET_cmdHandler -> Drv::GpioWrite -> PORT register, entirely synchronous on
// one stack with no queue anywhere in it.
//
// The component exists mostly to make one point: Drv::GpioWrite returns a
// Drv::GpioStatus, and a command handler that discards it reports OK for writes
// that never reached a pin.
// ======================================================================

#ifndef CuriosityReference_GpioOut_HPP
#define CuriosityReference_GpioOut_HPP

#include "CuriosityReference/GpioOut/GpioOutComponentAc.hpp"

namespace CuriosityReference {

class GpioOut final : public GpioOutComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct GpioOut object
    GpioOut(const char* const compName  //!< The component name
    );

    //! Destroy GpioOut object
    ~GpioOut();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SET
    //!
    //! Set the logic level of the GPIO. The driver's status is checked and a
    //! rejection fails the command; it must not be able to fault the board.
    void SET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                        U32 cmdSeq,           //!< The command sequence number
                        const Fw::Logic& state) override;
};

}  // namespace CuriosityReference

#endif
