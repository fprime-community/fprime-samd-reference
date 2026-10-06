// ======================================================================
// \title  FramTesterTester.cpp
// \brief  Unit tests for the CuriosityReference FramTester
// ======================================================================

#include "FramTesterTester.hpp"

#include <cstring>

namespace CuriosityReference {

// Odr-used by the gtest comparison macros (C++14 needs the definition)
constexpr U32 FramTesterTester::MODEL_SIZE;

namespace {
constexpr U32 POLL_OFFSET = 0x1000;
constexpr U8 POLL_LENGTH = 16;
constexpr U32 CMD_OFFSET = 0x100;
constexpr U8 CMD_LENGTH = 8;
constexpr U8 SEED = 0xA5;
constexpr U8 OVERSIZE = static_cast<U8>(FramTester::MAX_LENGTH + 1);
}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

FramTesterTester ::FramTesterTester()
    : FramTesterGTestBase("FramTesterTester", FramTesterTester::MAX_HISTORY_SIZE),
      component("FramTester"),
      m_requestPending(false),
      m_requestIsRead(false),
      m_requestOffset(0),
      m_requestBuffer() {
    (void)std::memset(this->m_fram, 0xEE, sizeof(this->m_fram));
    this->initComponents();
    this->connectPorts();
}

FramTesterTester ::~FramTesterTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void FramTesterTester ::testPollAlternates() {
    this->sendCmd_SET_POLL_REGION(0, 1, POLL_OFFSET, POLL_LENGTH);
    ASSERT_CMD_RESPONSE(0, FramTester::OPCODE_SET_POLL_REGION, 1, Fw::CmdResponse::OK);
    this->sendCmd_SET_POLLING(0, 2, true);
    ASSERT_CMD_RESPONSE(1, FramTester::OPCODE_SET_POLLING, 2, Fw::CmdResponse::OK);
    this->clearHistory();

    // Each cycle must write seed s and then verify s; the next cycle must WRITE s+1,
    // not verify again (regression: the hardware soak re-verified with the next seed)
    for (U8 seed = 0; seed < 6; seed++) {
        this->pollCycleOk(POLL_OFFSET, POLL_LENGTH, seed, static_cast<U32>(seed) + 1);
    }
    ASSERT_EVENTS_PatternMismatch_SIZE(0);
    ASSERT_EVENTS_FramError_SIZE(0);
    ASSERT_EVENTS_Busy_SIZE(0);
    ASSERT_TLM_VerifyFailures_SIZE(0);

    // Disabling polling stops the traffic
    this->sendCmd_SET_POLLING(0, 3, false);
    this->clearHistory();
    this->tick();
    this->tick();
    ASSERT_from_framWriteOut_SIZE(0);
    ASSERT_from_framReadOut_SIZE(0);
}

void FramTesterTester ::testPollFailureRewritesSameSeed() {
    this->sendCmd_SET_POLL_REGION(0, 1, POLL_OFFSET, POLL_LENGTH);
    this->sendCmd_SET_POLLING(0, 2, true);
    this->clearHistory();
    this->pollCycleOk(POLL_OFFSET, POLL_LENGTH, 0, 1);

    // Cycle for seed 1 whose data gets corrupted behind the tester's back
    this->tick();
    this->expectWriteRequest(POLL_OFFSET, POLL_LENGTH, 1);
    this->completeWrite();
    this->m_fram[POLL_OFFSET + 3] = static_cast<U8>(~this->m_fram[POLL_OFFSET + 3]);
    this->tick();
    this->expectReadRequest(POLL_OFFSET, POLL_LENGTH);
    this->clearHistory();
    this->completeRead();
    ASSERT_EVENTS_PatternMismatch_SIZE(1);
    ASSERT_EVENTS_PatternMismatch(0, POLL_OFFSET + 3, 4, static_cast<U8>(~4));
    ASSERT_TLM_VerifyFailures(0, 1);
    ASSERT_TLM_PollCycles_SIZE(0);

    // The failed seed is rewritten, not skipped, and the cycle count does not advance until it verifies
    this->pollCycleOk(POLL_OFFSET, POLL_LENGTH, 1, 2);
    this->pollCycleOk(POLL_OFFSET, POLL_LENGTH, 2, 3);
}

void FramTesterTester ::testCommandRoundTrip() {
    // WRITE_PATTERN completes asynchronously: no response until the driver replies
    this->sendCmd_WRITE_PATTERN(0, 1, CMD_OFFSET, CMD_LENGTH, SEED);
    this->expectWriteRequest(CMD_OFFSET, CMD_LENGTH, SEED);
    ASSERT_CMD_RESPONSE_SIZE(0);
    this->completeWrite();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FramTester::OPCODE_WRITE_PATTERN, 1, Fw::CmdResponse::OK);
    ASSERT_EVENTS_WriteComplete_SIZE(1);
    ASSERT_EVENTS_WriteComplete(0, CMD_OFFSET, CMD_LENGTH, SEED);
    ASSERT_TLM_WriteCount(0, 1);
    ASSERT_TLM_LastStatus(0, Fram::Status::OK);
    this->clearHistory();

    // READ_VERIFY with the matching seed
    this->sendCmd_READ_VERIFY(0, 2, CMD_OFFSET, CMD_LENGTH, SEED);
    this->expectReadRequest(CMD_OFFSET, CMD_LENGTH);
    this->completeRead();
    ASSERT_CMD_RESPONSE(0, FramTester::OPCODE_READ_VERIFY, 2, Fw::CmdResponse::OK);
    ASSERT_EVENTS_VerifyComplete(0, CMD_OFFSET, CMD_LENGTH, SEED);
    ASSERT_EVENTS_PatternMismatch_SIZE(0);
    ASSERT_TLM_ReadCount(0, 1);
    this->clearHistory();

    // READ_VERIFY with the wrong seed reports the first mismatching byte and fails the command
    this->sendCmd_READ_VERIFY(0, 3, CMD_OFFSET, CMD_LENGTH, static_cast<U8>(SEED + 1));
    this->expectReadRequest(CMD_OFFSET, CMD_LENGTH);
    this->completeRead();
    ASSERT_CMD_RESPONSE(0, FramTester::OPCODE_READ_VERIFY, 3, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_PatternMismatch_SIZE(1);
    ASSERT_EVENTS_PatternMismatch(0, CMD_OFFSET, static_cast<U8>(SEED + 1), SEED);
    ASSERT_EVENTS_VerifyComplete_SIZE(0);
    ASSERT_TLM_VerifyFailures(0, 1);
    this->clearHistory();

    // READ dumps the first eight bytes MSB first
    this->sendCmd_READ(0, 4, CMD_OFFSET, CMD_LENGTH);
    this->expectReadRequest(CMD_OFFSET, CMD_LENGTH);
    this->completeRead();
    ASSERT_CMD_RESPONSE(0, FramTester::OPCODE_READ, 4, Fw::CmdResponse::OK);
    ASSERT_EVENTS_ReadComplete_SIZE(1);
    ASSERT_EVENTS_ReadComplete(0, CMD_OFFSET, CMD_LENGTH, 0xA5A6A7A8U, 0xA9AAABACU);
    ASSERT_TLM_ReadData0(0, 0xA5A6A7A8U);
    ASSERT_TLM_ReadData1(0, 0xA9AAABACU);
    this->clearHistory();

    // A short READ is zero-padded past length
    this->sendCmd_READ(0, 5, CMD_OFFSET, 3);
    this->expectReadRequest(CMD_OFFSET, 3);
    this->completeRead();
    ASSERT_EVENTS_ReadComplete(0, CMD_OFFSET, 3, 0xA5A6A700U, 0U);
    ASSERT_TLM_ReadCount(0, 4);  // every OK read completion counts, mismatching verify included
}

void FramTesterTester ::testBusyAndInvalidArgument() {
    this->sendCmd_SET_POLL_REGION(0, 1, POLL_OFFSET, POLL_LENGTH);
    this->sendCmd_SET_POLLING(0, 2, true);
    this->clearHistory();

    // A command in flight makes both a second command and the poll tick back off
    this->sendCmd_WRITE_PATTERN(0, 3, CMD_OFFSET, CMD_LENGTH, SEED);
    this->expectWriteRequest(CMD_OFFSET, CMD_LENGTH, SEED);
    this->sendCmd_READ(0, 4, CMD_OFFSET, CMD_LENGTH);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, FramTester::OPCODE_READ, 4, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_Busy_SIZE(1);
    ASSERT_EVENTS_Busy(0, FramTester_State::READ_CMD, FramTester_State::WRITE_CMD);
    ASSERT_TLM_BusyRejects(0, 1);
    this->tick();
    ASSERT_EVENTS_Busy_SIZE(2);
    ASSERT_EVENTS_Busy(1, FramTester_State::POLL_WRITE, FramTester_State::WRITE_CMD);
    ASSERT_TLM_BusyRejects(1, 2);
    // Still exactly one request outstanding
    ASSERT_from_framWriteOut_SIZE(1);
    ASSERT_from_framReadOut_SIZE(0);
    this->completeWrite();
    ASSERT_CMD_RESPONSE(1, FramTester::OPCODE_WRITE_PATTERN, 3, Fw::CmdResponse::OK);
    this->clearHistory();

    // Oversized lengths are rejected before any request is issued
    this->sendCmd_WRITE_PATTERN(0, 5, CMD_OFFSET, OVERSIZE, SEED);
    ASSERT_CMD_RESPONSE(0, FramTester::OPCODE_WRITE_PATTERN, 5, Fw::CmdResponse::VALIDATION_ERROR);
    this->sendCmd_READ_VERIFY(0, 6, CMD_OFFSET, OVERSIZE, SEED);
    ASSERT_CMD_RESPONSE(1, FramTester::OPCODE_READ_VERIFY, 6, Fw::CmdResponse::VALIDATION_ERROR);
    this->sendCmd_READ(0, 7, CMD_OFFSET, OVERSIZE);
    ASSERT_CMD_RESPONSE(2, FramTester::OPCODE_READ, 7, Fw::CmdResponse::VALIDATION_ERROR);
    this->sendCmd_SET_POLL_REGION(0, 8, CMD_OFFSET, OVERSIZE);
    ASSERT_CMD_RESPONSE(3, FramTester::OPCODE_SET_POLL_REGION, 8, Fw::CmdResponse::VALIDATION_ERROR);
    this->sendCmd_SET_POLL_REGION(0, 9, CMD_OFFSET, 0);
    ASSERT_CMD_RESPONSE(4, FramTester::OPCODE_SET_POLL_REGION, 9, Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidArgument_SIZE(5);
    ASSERT_EVENTS_InvalidArgument(0, OVERSIZE);
    ASSERT_EVENTS_InvalidArgument(4, 0U);
    ASSERT_from_framWriteOut_SIZE(0);
    ASSERT_from_framReadOut_SIZE(0);
    ASSERT_FALSE(this->m_requestPending);
}

void FramTesterTester ::testDriverErrorStatus() {
    // A rejected command write is counted, reported and fails the command
    this->sendCmd_WRITE_PATTERN(0, 1, CMD_OFFSET, CMD_LENGTH, SEED);
    this->expectWriteRequest(CMD_OFFSET, CMD_LENGTH, SEED);
    this->completeWrite(Fram::Status::OUT_OF_RANGE);
    ASSERT_CMD_RESPONSE(0, FramTester::OPCODE_WRITE_PATTERN, 1, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_FramError_SIZE(1);
    ASSERT_EVENTS_FramError(0, FramTester_State::WRITE_CMD, Fram::Status::OUT_OF_RANGE, CMD_OFFSET);
    ASSERT_EVENTS_WriteComplete_SIZE(0);
    ASSERT_TLM_FramErrors(0, 1);
    ASSERT_TLM_LastStatus(0, Fram::Status::OUT_OF_RANGE);
    ASSERT_TLM_WriteCount_SIZE(0);
    this->clearHistory();

    // A failed read in a verify command does not count as a verify failure
    this->sendCmd_READ_VERIFY(0, 2, CMD_OFFSET, CMD_LENGTH, SEED);
    this->expectReadRequest(CMD_OFFSET, CMD_LENGTH);
    this->completeRead(Fram::Status::SPI_ERROR);
    ASSERT_CMD_RESPONSE(0, FramTester::OPCODE_READ_VERIFY, 2, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_FramError(0, FramTester_State::VERIFY_CMD, Fram::Status::SPI_ERROR, CMD_OFFSET);
    ASSERT_TLM_FramErrors(0, 2);
    ASSERT_TLM_VerifyFailures_SIZE(0);
    this->clearHistory();

    // A failed poll write is retried with the same seed on the next tick
    this->sendCmd_SET_POLL_REGION(0, 3, POLL_OFFSET, POLL_LENGTH);
    this->sendCmd_SET_POLLING(0, 4, true);
    this->clearHistory();
    this->tick();
    this->expectWriteRequest(POLL_OFFSET, POLL_LENGTH, 0);
    this->completeWrite(Fram::Status::SPI_ERROR);
    ASSERT_EVENTS_FramError(0, FramTester_State::POLL_WRITE, Fram::Status::SPI_ERROR, POLL_OFFSET);
    ASSERT_TLM_FramErrors(0, 3);
    this->clearHistory();
    this->pollCycleOk(POLL_OFFSET, POLL_LENGTH, 0, 1);
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void FramTesterTester ::from_framReadOut_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) {
    EXPECT_FALSE(this->m_requestPending) << "request issued while another is outstanding";
    EXPECT_EQ(0, portNum);
    this->m_requestPending = true;
    this->m_requestIsRead = true;
    this->m_requestOffset = offset;
    this->m_requestBuffer = buffer;
    this->pushFromPortEntry_framReadOut(offset, buffer);
}

void FramTesterTester ::from_framWriteOut_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) {
    EXPECT_FALSE(this->m_requestPending) << "request issued while another is outstanding";
    EXPECT_EQ(0, portNum);
    this->m_requestPending = true;
    this->m_requestIsRead = false;
    this->m_requestOffset = offset;
    this->m_requestBuffer = buffer;
    this->pushFromPortEntry_framWriteOut(offset, buffer);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void FramTesterTester ::tick() {
    this->invoke_to_schedIn(0, 0);
}

void FramTesterTester ::expectWriteRequest(U32 offset, FwSizeType length, U8 seed) {
    ASSERT_TRUE(this->m_requestPending);
    ASSERT_FALSE(this->m_requestIsRead);
    ASSERT_EQ(offset, this->m_requestOffset);
    ASSERT_EQ(length, this->m_requestBuffer.getSize());
    for (FwSizeType i = 0; i < length; i++) {
        ASSERT_EQ(static_cast<U8>(seed + i), this->m_requestBuffer.getData()[i]) << "byte " << i;
    }
}

void FramTesterTester ::expectReadRequest(U32 offset, FwSizeType length) {
    ASSERT_TRUE(this->m_requestPending);
    ASSERT_TRUE(this->m_requestIsRead);
    ASSERT_EQ(offset, this->m_requestOffset);
    ASSERT_EQ(length, this->m_requestBuffer.getSize());
}

void FramTesterTester ::completeWrite(Fram::Status status) {
    ASSERT_TRUE(this->m_requestPending);
    ASSERT_FALSE(this->m_requestIsRead);
    const FwSizeType size = this->m_requestBuffer.getSize();
    ASSERT_LE(this->m_requestOffset + size, MODEL_SIZE);
    if (status == Fram::Status::OK) {
        (void)std::memcpy(&this->m_fram[this->m_requestOffset], this->m_requestBuffer.getData(), size);
    }
    this->m_requestPending = false;
    this->invoke_to_writeCompleteIn(0, this->m_requestOffset, this->m_requestBuffer, status);
}

void FramTesterTester ::completeRead(Fram::Status status) {
    ASSERT_TRUE(this->m_requestPending);
    ASSERT_TRUE(this->m_requestIsRead);
    const FwSizeType size = this->m_requestBuffer.getSize();
    ASSERT_LE(this->m_requestOffset + size, MODEL_SIZE);
    if (status == Fram::Status::OK) {
        (void)std::memcpy(this->m_requestBuffer.getData(), &this->m_fram[this->m_requestOffset], size);
    }
    this->m_requestPending = false;
    this->invoke_to_readCompleteIn(0, this->m_requestOffset, this->m_requestBuffer, status);
}

void FramTesterTester ::pollCycleOk(U32 offset, FwSizeType length, U8 seed, U32 cyclesAfter) {
    this->clearHistory();
    this->tick();
    this->expectWriteRequest(offset, length, seed);
    ASSERT_from_framReadOut_SIZE(0);
    this->completeWrite();
    this->tick();
    this->expectReadRequest(offset, length);
    ASSERT_from_framWriteOut_SIZE(1);
    this->completeRead();
    ASSERT_EVENTS_PatternMismatch_SIZE(0);
    ASSERT_EVENTS_FramError_SIZE(0);
    ASSERT_TLM_PollCycles_SIZE(1);
    ASSERT_TLM_PollCycles(0, cyclesAfter);
    ASSERT_FALSE(this->m_requestPending);
}

}  // namespace CuriosityReference
