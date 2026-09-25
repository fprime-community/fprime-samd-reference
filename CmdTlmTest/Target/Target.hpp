// ======================================================================
// \title  Target.hpp
// \brief  hpp file for Target component implementation class
//
// Test-only component. Stands in for "the proper component" in the
// CmdTlmTest topology's commanding test: the test asserts that a command
// built as raw uplink bytes and injected at frameAccumulator.dataIn lands
// here, and only here.
// ======================================================================

#ifndef CmdTlmTest_Target_HPP
#define CmdTlmTest_Target_HPP

#include "CmdTlmTest/Target/TargetComponentAc.hpp"

namespace CmdTlmTest {

class Target final : public TargetComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Target object
    Target(const char* const compName  //!< The component name
    );

    //! Destroy Target object
    ~Target();

  public:
    // ----------------------------------------------------------------------
    // Test accessors
    // ----------------------------------------------------------------------

    //! The value recorded by the most recent SET_VALUE, or 0 if never called
    U32 getValue() const;

    //! Number of times SET_VALUE has been dispatched to this instance.
    //! Used to assert that a command with an unrecognized opcode never
    //! reaches this component.
    U32 getSetCount() const;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SET_VALUE
    void SET_VALUE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                              U32 cmdSeq,           //!< The command sequence number
                              U32 value             //!< Value to record
                              ) override;

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    U32 m_value;     //!< Last value recorded by SET_VALUE
    U32 m_setCount;  //!< Number of times SET_VALUE has been dispatched here
};

}  // namespace CmdTlmTest

#endif
