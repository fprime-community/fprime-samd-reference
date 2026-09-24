// ======================================================================
// \title  FrameworkHealth.hpp
// \author tumbar
// \brief  hpp file for FrameworkHealth component implementation class
//
// A minimal rate-group-driven heartbeat. Demonstrates the passive scheduling
// model used throughout this deployment: the component owns no thread and no
// queue, and every line of code it runs executes inside the caller's
// schedIn_handler invocation, which in turn runs on the main loop's rate-group
// dispatch out of Samd21::RtcDriver.
// ======================================================================

#ifndef CuriosityReference_FrameworkHealth_HPP
#define CuriosityReference_FrameworkHealth_HPP

#include "CuriosityReference/FrameworkHealth/FrameworkHealthComponentAc.hpp"

namespace CuriosityReference {

class FrameworkHealth final : public FrameworkHealthComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct FrameworkHealth object
    FrameworkHealth(const char* const compName  //!< The component name
    );

    //! Destroy FrameworkHealth object
    ~FrameworkHealth();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for schedIn
    //!
    //! Publishes the current tick count and advances it. Runs in main context
    //! on the rate group that drives it (rg10s in the reference topology).
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SET_STATE
    //!
    //! Enable or disable the emission state on this component.
    //! Emission is enabled at construction.
    void SET_STATE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                              U32 cmdSeq,           //!< The command sequence number
                              bool state) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    bool m_running;  //!< Emit telemetry on each tick; true at construction
    U32 m_counter;   //!< Ticks observed while m_running was true
};

}  // namespace CuriosityReference

#endif
