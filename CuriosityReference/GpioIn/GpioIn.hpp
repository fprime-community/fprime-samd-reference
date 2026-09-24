// ======================================================================
// \title  GpioIn.hpp
// \author tumbar
// \brief  hpp file for GpioIn component implementation class
//
// Consumer side of the Samd21::GpioDriver input path. Demonstrates two
// distinct ways a passive component gets at a GPIO pin:
//   * pull -- a synchronous Drv::GpioRead invocation from a command handler,
//     which runs the driver's register read inline on the caller's stack;
//   * push -- a Svc::Cycle notification the driver emits from the SAMD21
//     External Interrupt Controller handler when the pin changes level.
//
// The push path is the interesting one and the dangerous one: it executes in
// ISR context. See docs/sdd.md.
// ======================================================================

#ifndef CuriosityReference_GpioIn_HPP
#define CuriosityReference_GpioIn_HPP

#include "CuriosityReference/GpioIn/GpioInComponentAc.hpp"

namespace CuriosityReference {

class GpioIn final : public GpioInComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct GpioIn object
    GpioIn(const char* const compName  //!< The component name
    );

    //! Destroy GpioIn object
    ~GpioIn();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for transitionIn
    //!
    //! Incoming message signaling we saw a GPIO edge.
    //!
    //! ISR CONTEXT. Samd21::GpioDriver emits gpioInterrupt directly from the
    //! EIC handler, so this handler -- including the readOut_out() register
    //! read and the event emission, which synchronously serializes a packet and
    //! pushes it at the framer -- runs with the interrupt in progress. Keep it
    //! short, and see docs/sdd.md for the re-entrancy caveat before adding
    //! anything to it.
    void transitionIn_handler(FwIndexType portNum,     //!< The port number
                              Os::RawTime& cycleStart  //!< Cycle start timestamp
                              ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command READ
    //!
    //! Read the logic level of the GPIO pin and emit an event. A driver
    //! rejection fails the command; it must not be able to fault the board.
    void READ_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                         U32 cmdSeq            //!< The command sequence number
                         ) override;
};

}  // namespace CuriosityReference

#endif
