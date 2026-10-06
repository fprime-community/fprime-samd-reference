// ======================================================================
// \title  FramTesterTester.hpp
// \brief  Unit test harness for the CuriosityReference FramTester
//
// Plays the FramDriver: records every request the FramTester issues on
// framReadOut/framWriteOut, keeps a byte-accurate model of the F-RAM, and
// delivers completions on demand so that each test controls the moment
// (and status) of the asynchronous reply.
// ======================================================================

#ifndef CuriosityReference_FramTesterTester_HPP
#define CuriosityReference_FramTesterTester_HPP

#include "CuriosityReference/FramTester/FramTester.hpp"
#include "CuriosityReference/FramTester/FramTesterGTestBase.hpp"

namespace CuriosityReference {

class FramTesterTester final : public FramTesterGTestBase {
  public:
    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 64;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Size of the F-RAM model (CY15B102QN: 256 KiB)
    static constexpr U32 MODEL_SIZE = 0x40000;

  public:
    FramTesterTester();
    ~FramTesterTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! REQ-FRAMT-004: polling alternates write and verify, advancing the seed each verified cycle
    void testPollAlternates();

    //! REQ-FRAMT-004/005: a failed verify rewrites the same seed before verifying again
    void testPollFailureRewritesSameSeed();

    //! REQ-FRAMT-001..003: WRITE_PATTERN / READ_VERIFY / READ command round trips
    void testCommandRoundTrip();

    //! REQ-FRAMT-005: busy rejection and argument validation
    void testBusyAndInvalidArgument();

    //! REQ-FRAMT-005: non-OK driver status is counted, reported and fails the command
    void testDriverErrorStatus();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    void from_framReadOut_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) override;
    void from_framWriteOut_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) override;

  private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    void connectPorts();
    void initComponents();

    //! One rate-group tick
    void tick();

    //! Assert exactly one request is pending and it is a write of `seed, seed+1, ...`
    void expectWriteRequest(U32 offset, FwSizeType length, U8 seed);

    //! Assert exactly one request is pending and it is a read
    void expectReadRequest(U32 offset, FwSizeType length);

    //! Complete the pending write: on OK, commit the data to the model
    void completeWrite(Fram::Status status = Fram::Status::OK);

    //! Complete the pending read: on OK, fill the buffer from the model
    void completeRead(Fram::Status status = Fram::Status::OK);

    //! Run one poll cycle (write tick + verify tick) and expect it to pass
    void pollCycleOk(U32 offset, FwSizeType length, U8 seed, U32 cyclesAfter);

  private:
    FramTester component;

    U8 m_fram[MODEL_SIZE];  //!< Byte model of the device

    bool m_requestPending;       //!< A request has been issued and not yet completed
    bool m_requestIsRead;        //!< Pending request is a read (else a write)
    U32 m_requestOffset;         //!< Offset of the pending request
    Fw::Buffer m_requestBuffer;  //!< Buffer handed over with the pending request
};

}  // namespace CuriosityReference

#endif
