// ======================================================================
// \title  CommandRoutingTests.cpp
// \brief  gtest cases driving the CmdTlmTest topology end to end
//
// Each test builds a raw uplink frame with CmdTlmTest::TestUtil::FrameBuilder and
// injects it at frameAccumulator's real input port -- exactly where
// comStub.dataOut feeds it in CuriosityReference/Top/topology.fpp -- via
// get_dataIn_InputPort(0)->invoke(...), the same call a connected output
// port would make. Nothing between "raw bytes" and "dispatched command" is
// mocked: frameAccumulator, deframer, fprimeRouter, and cmdDisp are the real
// production components.
// ======================================================================

#include "gtest/gtest.h"

#include "CmdTlmTest/Top/TopTopologyAc.hpp"
#include "CmdTlmTest/Top/test/ut/FrameBuilder.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Os/Os.hpp"

namespace CmdTlmTest {

static TopologyState state;

// Local (per-component) opcodes/event IDs, mirrored from each component's own .fpp. The
// autocoded OPCODE_*/EVENTID_* enum constants on *ComponentBase are declared `protected` --
// meant for the component's own handler dispatch, not for external callers -- so test code
// combines the local id with getIdBase() itself instead of referencing them.
constexpr FwOpcodeType TARGET_LOCAL_OPCODE_SET_VALUE = 0;             // Target.fpp: opcode 0
constexpr FwOpcodeType TLM_LOCAL_OPCODE_SEND_PKT = 0;                 // StaticTlmPacketizer.fpp: opcode 0
constexpr FwEventIdType CMD_DISP_LOCAL_EVENTID_INVALIDCOMMAND = 0x4;  // StaticCmdDispatcher.fpp: id 0x4

class CmdTlmTestFixture : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        Os::init();
        setup(state);
        // Stand-in for the driver's own readiness signal (see
        // CuriosityReference/Top/topology.fpp's comDriver.ready ->
        // framer.drvConnected). Framer refuses to flush until this fires.
        comSink.signalReady();
    }

    static void TearDownTestSuite() { teardown(state); }

    void SetUp() override {
        comSink.reset();
        eventSink.reset();
    }

  protected:
    //! Serialize a single big-endian value into `out`, returning its size
    template <typename T>
    static FwSizeType serializeArg(U8* out, FwSizeType capacity, T value) {
        Fw::ExternalSerializeBuffer buffer(out, capacity);
        const Fw::SerializeStatus status = buffer.serializeFrom(value);
        EXPECT_EQ(status, Fw::FW_SERIALIZE_OK);
        return buffer.getSize();
    }

    //! Build a command frame and inject it exactly where comStub.dataOut
    //! feeds frameAccumulator in the real topology.
    static void injectCommandFrame(FwOpcodeType opcode, const U8* args, FwSizeType argsSize) {
        U8 frame[64];
        const FwSizeType frameSize =
            TestUtil::FrameBuilder::buildCommandFrame(frame, sizeof(frame), opcode, args, argsSize);
        Fw::Buffer buffer(frame, frameSize);
        const ComCfg::FrameContext context;
        frameAccumulator.get_dataIn_InputPort(0)->invoke(buffer, context);
    }

    //! True if `needle` appears as a contiguous run inside `haystack`
    static bool contains(const U8* haystack, FwSizeType haystackSize, const U8* needle, FwSizeType needleSize) {
        if (needleSize == 0 || needleSize > haystackSize) {
            return false;
        }
        for (FwSizeType i = 0; i + needleSize <= haystackSize; i++) {
            FwSizeType j = 0;
            for (; j < needleSize; j++) {
                if (haystack[i + j] != needle[j]) {
                    break;
                }
            }
            if (j == needleSize) {
                return true;
            }
        }
        return false;
    }
};

TEST_F(CmdTlmTestFixture, CommandReachesTarget) {
    const U32 setCountBefore = target.getSetCount();
    const FwOpcodeType opcode = target.getIdBase() + TARGET_LOCAL_OPCODE_SET_VALUE;

    U8 argBytes[sizeof(U32)];
    const FwSizeType argSize = serializeArg<U32>(argBytes, sizeof(argBytes), 0xCAFEF00D);

    injectCommandFrame(opcode, argBytes, argSize);

    EXPECT_EQ(target.getSetCount(), setCountBefore + 1);
    EXPECT_EQ(target.getValue(), 0xCAFEF00DU);
    // cmdDisp.OpCodeDispatched + cmdDisp.OpCodeCompleted + target.ValueSet -- all expected,
    // none of them cmdDisp's InvalidCommand/MalformedCommand/OpCodeError.
    EXPECT_EQ(eventSink.getEventCount(), 3U);
    EXPECT_NE(eventSink.getLastEventId(), cmdDisp.getIdBase() + CMD_DISP_LOCAL_EVENTID_INVALIDCOMMAND);
}

TEST_F(CmdTlmTestFixture, UnknownOpcodeDoesNotReachTarget) {
    const U32 setCountBefore = target.getSetCount();
    // Not target's opcode, not cmdDisp's own NO_OP/CLEAR_TRACKING/SET_EVENT_EMISSION, and
    // not tlm's SEND_PKT -- guaranteed absent from the compile-time dispatch table.
    const FwOpcodeType bogusOpcode = 0xFFFFFFFE;

    injectCommandFrame(bogusOpcode, nullptr, 0);

    EXPECT_EQ(target.getSetCount(), setCountBefore) << "an unrecognized opcode must not dispatch to target";
    EXPECT_EQ(eventSink.getLastEventId(), cmdDisp.getIdBase() + CMD_DISP_LOCAL_EVENTID_INVALIDCOMMAND);
}

TEST_F(CmdTlmTestFixture, TelemetryRoundTrip) {
    // First, set a known, distinctive value on target's telemetry channel through the
    // same uplink path the commanding test uses.
    const FwOpcodeType setValueOpcode = target.getIdBase() + TARGET_LOCAL_OPCODE_SET_VALUE;
    U8 setValueArgs[sizeof(U32)];
    const FwSizeType setValueArgsSize = serializeArg<U32>(setValueArgs, sizeof(setValueArgs), 0x1122EE33);
    injectCommandFrame(setValueOpcode, setValueArgs, setValueArgsSize);
    ASSERT_EQ(target.getValue(), 0x1122EE33U);

    // Then, ask the packetizer to flush the packet containing that channel -- SEND_PKT is
    // itself dispatched through cmdDisp, exactly like a ground command would be.
    const FwOpcodeType sendPktOpcode = tlm.getIdBase() + TLM_LOCAL_OPCODE_SEND_PKT;
    U8 sendPktArgs[sizeof(FwTlmPacketizeIdType)];
    const FwSizeType sendPktArgsSize = serializeArg<FwTlmPacketizeIdType>(sendPktArgs, sizeof(sendPktArgs), 0);
    injectCommandFrame(sendPktOpcode, sendPktArgs, sendPktArgsSize);

    // SEND_PKT only queues the packet into framer's TX buffer (Samd21::Framer::
    // comPacketQueueIn_handler); flushing it to the driver is normally the 1 Hz rate
    // group's job (rg1Hz.RateGroupMemberOut[last] -> framer.schedIn in
    // CuriosityReference/Top/topology.fpp). This topology has no rate group, so the test
    // plays that role directly, the same way it plays comStub's role for frameAccumulator.
    framer.get_schedIn_InputPort(0)->invoke(0);

    ASSERT_EQ(comSink.getSendCount(), 1U) << "framer should have flushed exactly one packet to the driver";
    U8 expectedValueBytes[sizeof(U32)];
    const FwSizeType expectedValueSize = serializeArg<U32>(expectedValueBytes, sizeof(expectedValueBytes), 0x1122EE33);
    EXPECT_TRUE(contains(comSink.getCapturedData(), comSink.getCapturedSize(), expectedValueBytes, expectedValueSize))
        << "downlinked packet should carry target's channel value";
}

}  // namespace CmdTlmTest
