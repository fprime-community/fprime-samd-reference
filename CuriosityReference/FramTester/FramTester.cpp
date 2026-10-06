// ======================================================================
// \title  FramTester.cpp
// \author Devin (JPL)
// \brief  cpp file for FramTester component implementation class
// ======================================================================

#include "CuriosityReference/FramTester/FramTester.hpp"

#include "Fw/Types/Assert.hpp"

namespace CuriosityReference {

constexpr FwSizeType FramTester::MAX_LENGTH;

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FramTester ::FramTester(const char* const compName)
    : FramTesterComponentBase(compName),
      m_state(FramTester_State::IDLE),
      m_pendingOpCode(0),
      m_pendingCmdSeq(0),
      m_data(),
      m_buffer(),
      m_length(0),
      m_seed(0),
      m_polling(false),
      m_pollVerifyNext(false),
      m_pollOffset(0),
      m_pollLength(32),
      m_pollSeed(0),
      m_writeCount(0),
      m_readCount(0),
      m_verifyFailures(0),
      m_framErrors(0),
      m_busyRejects(0),
      m_pollCycles(0) {}

FramTester ::~FramTester() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void FramTester ::readCompleteIn_handler(FwIndexType portNum,
                                         U32 offset,
                                         Fw::Buffer& buffer,
                                         const Fram::Status& status) {
    // The driver hands back the buffer it was given; anything else is a wiring fault
    FW_ASSERT(buffer.getData() == this->m_data);
    FW_ASSERT(this->m_state == FramTester_State::READ_CMD || this->m_state == FramTester_State::VERIFY_CMD ||
                  this->m_state == FramTester_State::POLL_VERIFY,
              static_cast<FwAssertArgType>(this->m_state));

    if (!this->recordStatus(offset, status)) {
        this->finishOperation(false);
        return;
    }
    this->m_readCount++;
    this->tlmWrite_ReadCount(this->m_readCount);

    bool success = true;
    switch (this->m_state) {
        case FramTester_State::READ_CMD: {
            const U32 data0 = this->packWord(0);
            const U32 data1 = this->packWord(4);
            this->tlmWrite_ReadData0(data0);
            this->tlmWrite_ReadData1(data1);
            this->log_ACTIVITY_LO_ReadComplete(offset, static_cast<U8>(this->m_length), data0, data1);
            break;
        }
        case FramTester_State::VERIFY_CMD:
            success = this->verifyPattern(offset);
            if (success) {
                this->log_ACTIVITY_LO_VerifyComplete(offset, static_cast<U8>(this->m_length), this->m_seed);
            }
            break;
        case FramTester_State::POLL_VERIFY:
        default:
            success = this->verifyPattern(offset);
            if (success) {
                this->m_pollCycles++;
                this->tlmWrite_PollCycles(this->m_pollCycles);
                this->m_pollSeed++;
            }
            // Verified or not, the region gets (re)written on the next tick
            this->m_pollVerifyNext = false;
            break;
    }
    this->finishOperation(success);
}

void FramTester ::schedIn_handler(FwIndexType portNum, U32 context) {
    if (!this->m_polling) {
        return;
    }
    const FramTester_State op = this->m_pollVerifyNext ? FramTester_State::POLL_VERIFY : FramTester_State::POLL_WRITE;
    if (!this->beginOperation(op)) {
        // beginOperation logged Busy and bumped BusyRejects; a tick has no one to reply to
        return;
    }
    this->m_seed = this->m_pollSeed;
    if (op == FramTester_State::POLL_WRITE) {
        this->fillPattern(this->m_seed, this->m_pollLength);
        this->startWrite(this->m_pollOffset, this->m_pollLength);
    } else {
        this->startRead(this->m_pollOffset, this->m_pollLength);
    }
}

void FramTester ::writeCompleteIn_handler(FwIndexType portNum,
                                          U32 offset,
                                          Fw::Buffer& buffer,
                                          const Fram::Status& status) {
    FW_ASSERT(buffer.getData() == this->m_data);
    FW_ASSERT(this->m_state == FramTester_State::WRITE_CMD || this->m_state == FramTester_State::POLL_WRITE,
              static_cast<FwAssertArgType>(this->m_state));

    if (!this->recordStatus(offset, status)) {
        this->finishOperation(false);
        return;
    }
    this->m_writeCount++;
    this->tlmWrite_WriteCount(this->m_writeCount);

    if (this->m_state == FramTester_State::WRITE_CMD) {
        this->log_ACTIVITY_LO_WriteComplete(offset, static_cast<U8>(this->m_length), this->m_seed);
    } else {
        // The poll region now holds pattern m_pollSeed; the next tick reads it back
        this->m_pollVerifyNext = true;
    }
    this->finishOperation(true);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void FramTester ::WRITE_PATTERN_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 offset, U8 length, U8 seed) {
    if (length > MAX_LENGTH) {
        this->log_WARNING_HI_InvalidArgument(length);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (!this->beginOperation(FramTester_State::WRITE_CMD)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    // Stash the reply target BEFORE issuing the request: if the driver rejects
    // the request it re-enters writeCompleteIn synchronously from inside
    // startWrite(), and finishOperation() needs these already set.
    this->m_pendingOpCode = opCode;
    this->m_pendingCmdSeq = cmdSeq;
    this->m_seed = seed;
    this->fillPattern(seed, length);
    this->startWrite(offset, length);
}

void FramTester ::READ_VERIFY_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 offset, U8 length, U8 seed) {
    if (length > MAX_LENGTH) {
        this->log_WARNING_HI_InvalidArgument(length);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (!this->beginOperation(FramTester_State::VERIFY_CMD)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->m_pendingOpCode = opCode;
    this->m_pendingCmdSeq = cmdSeq;
    this->m_seed = seed;
    this->startRead(offset, length);
}

void FramTester ::READ_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 offset, U8 length) {
    if (length > MAX_LENGTH) {
        this->log_WARNING_HI_InvalidArgument(length);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (!this->beginOperation(FramTester_State::READ_CMD)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->m_pendingOpCode = opCode;
    this->m_pendingCmdSeq = cmdSeq;
    this->startRead(offset, length);
}

void FramTester ::SET_POLLING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool enable) {
    this->m_polling = enable;
    // Start every soak run with a write so the first verify checks data we put there
    this->m_pollVerifyNext = false;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void FramTester ::SET_POLL_REGION_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 offset, U8 length) {
    if ((length == 0) || (length > MAX_LENGTH)) {
        this->log_WARNING_HI_InvalidArgument(length);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    this->m_pollOffset = offset;
    this->m_pollLength = length;
    // The new region has not been written yet; do not verify it against a stale seed
    this->m_pollVerifyNext = false;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

bool FramTester ::beginOperation(FramTester_State op) {
    if (this->m_state != FramTester_State::IDLE) {
        this->log_WARNING_LO_Busy(op, this->m_state);
        this->m_busyRejects++;
        this->tlmWrite_BusyRejects(this->m_busyRejects);
        return false;
    }
    this->m_state = op;
    return true;
}

void FramTester ::finishOperation(bool success) {
    // Snapshot and clear the state BEFORE replying: the reply is dispatched
    // synchronously into the command dispatcher, and anything it reaches may
    // issue a new request re-entrantly. It must see an idle component.
    const FramTester_State state = this->m_state;
    this->m_state = FramTester_State::IDLE;

    switch (state) {
        case FramTester_State::WRITE_CMD:
        case FramTester_State::VERIFY_CMD:
        case FramTester_State::READ_CMD:
            this->cmdResponse_out(this->m_pendingOpCode, this->m_pendingCmdSeq,
                                  success ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
            break;
        case FramTester_State::POLL_WRITE:
        case FramTester_State::POLL_VERIFY:
            if (!success) {
                // Whatever is in the region is suspect; rewrite it before verifying again
                this->m_pollVerifyNext = false;
            }
            break;
        case FramTester_State::IDLE:
        default:
            break;
    }
}

void FramTester ::fillPattern(U8 seed, FwSizeType length) {
    FW_ASSERT(length <= MAX_LENGTH, static_cast<FwAssertArgType>(length));
    for (FwSizeType i = 0; i < length; i++) {
        this->m_data[i] = static_cast<U8>(seed + i);
    }
}

void FramTester ::startRead(U32 offset, FwSizeType length) {
    FW_ASSERT(length <= MAX_LENGTH, static_cast<FwAssertArgType>(length));
    this->m_length = length;
    this->m_buffer.set(this->m_data, length);
    this->framReadOut_out(0, offset, this->m_buffer);
}

void FramTester ::startWrite(U32 offset, FwSizeType length) {
    FW_ASSERT(length <= MAX_LENGTH, static_cast<FwAssertArgType>(length));
    this->m_length = length;
    this->m_buffer.set(this->m_data, length);
    this->framWriteOut_out(0, offset, this->m_buffer);
}

bool FramTester ::verifyPattern(U32 offset) {
    for (FwSizeType i = 0; i < this->m_length; i++) {
        const U8 expected = static_cast<U8>(this->m_seed + i);
        if (this->m_data[i] != expected) {
            this->m_verifyFailures++;
            this->tlmWrite_VerifyFailures(this->m_verifyFailures);
            this->log_WARNING_HI_PatternMismatch(offset + static_cast<U32>(i), expected, this->m_data[i]);
            return false;
        }
    }
    return true;
}

U32 FramTester ::packWord(FwSizeType first) const {
    U32 word = 0;
    for (FwSizeType i = first; i < first + 4; i++) {
        const U8 byte = (i < this->m_length) ? this->m_data[i] : 0;
        word = (word << 8) | byte;
    }
    return word;
}

bool FramTester ::recordStatus(U32 offset, const Fram::Status& status) {
    this->tlmWrite_LastStatus(status);
    if (status == Fram::Status::OK) {
        return true;
    }
    this->m_framErrors++;
    this->tlmWrite_FramErrors(this->m_framErrors);
    this->log_WARNING_HI_FramError(this->m_state, status, offset);
    return false;
}

}  // namespace CuriosityReference
