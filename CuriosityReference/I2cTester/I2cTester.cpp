// ======================================================================
// \title  I2cTester.cpp
// \author tumbar
// \brief  cpp file for I2cTester component implementation class
// ======================================================================

#include "CuriosityReference/I2cTester/I2cTester.hpp"
#include <cstring>
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"

namespace CuriosityReference {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

I2cTester ::I2cTester(const char* const compName)
    : I2cTesterComponentBase(compName),
      m_state(I2cTester_State::IDLE),
      m_pendingOpCode(0),
      m_pendingCmdSeq(0),
      m_configLoaded(false),
      m_polling(false),  // the bus stays quiet until SET_POLLING enables it
      m_i2cAddress(MIN_I2C_ADDRESS),
      m_regOffset(0),
      m_readLength(1),
      m_writeValue(0),
      m_writeBuffer{},
      m_readBuffer{},
      m_txnCount(0),
      m_busyRejects(0),
      m_addressErrors(0),
      m_writeErrors(0),
      m_readErrors(0),
      m_openErrors(0),
      m_otherErrors(0),
      m_shortReads(0) {}

I2cTester ::~I2cTester() {}

// ----------------------------------------------------------------------
// Public interface
// ----------------------------------------------------------------------

void I2cTester ::configure(U8 i2cAddr, U8 regOffset, U8 readLen) {
    // Validate and return rather than assert: a bad argument here is a topology
    // defect, and faulting the board on it hides the defect behind a reset
    // loop. Leaving m_configLoaded false makes every later request reject
    // itself, so the misconfiguration is visible from the ground.
    if (i2cAddr < MIN_I2C_ADDRESS || i2cAddr > MAX_I2C_ADDRESS) {
        this->log_WARNING_HI_InvalidConfig(i2cAddr);
        return;
    }
    if (readLen == 0 || readLen > MAX_READ_BYTES) {
        this->log_WARNING_HI_InvalidConfig(readLen);
        return;
    }

    this->m_i2cAddress = i2cAddr;
    this->m_regOffset = regOffset;
    this->m_readLength = readLen;
    this->m_configLoaded = true;

    this->reportSettings();
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void I2cTester ::schedIn_handler(FwIndexType portNum, U32 context) {
    (void)portNum;  // One rate group drives this component
    (void)context;  // The rate group's call-order token is unused here

    if (!this->m_configLoaded || !this->m_polling) {
        // Neither is an error: polling is off at boot by design.
        return;
    }
    if (!this->beginOperation(I2cTester_State::READ_SCHED)) {
        // beginOperation logged Busy and bumped BusyRejects; schedIn has no
        // reply path, so there is nothing further to do.
        return;
    }

    this->startRead();
}

void I2cTester ::writeReadCompleteIn_handler(FwIndexType portNum,
                                             Fw::Buffer& writeBuffer,
                                             Fw::Buffer& readBuffer,
                                             const Drv::I2cStatus& status) {
    (void)portNum;  // One driver instance drives this component

    const U8* data = static_cast<const U8*>(readBuffer.getData());
    const FwSizeType len = readBuffer.getSize();

    // The driver hands back the exact descriptor it was given, so this must be
    // our own receive buffer. Route the pointers through a pointer-width
    // integer before narrowing to the (diagnostic-only) assert-arg type: a
    // direct pointer -> FwAssertArgType cast loses precision on 64-bit hosts
    // such as the native unit-test build.
    FW_ASSERT(data == this->m_readBuffer, static_cast<FwAssertArgType>(reinterpret_cast<PlatformPointerCastType>(data)),
              static_cast<FwAssertArgType>(reinterpret_cast<PlatformPointerCastType>(this->m_readBuffer)));

    // Recover the offset that was actually on the wire before doing anything
    // else: it is the only faithful description of the transaction that just
    // finished.
    const U8 regOffset = this->recoverRegOffset(writeBuffer);

    if (status.e != Drv::I2cStatus::I2C_OK) {
        this->countError(status, regOffset);
        this->finishOperation(false);
        return;
    }

    this->finishOperation(this->handleReadComplete(data, len, regOffset));
}

void I2cTester ::writeCompleteIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::I2cStatus& status) {
    (void)portNum;  // One driver instance drives this component

    const U8 regOffset = this->recoverRegOffset(buffer);

    if (status.e != Drv::I2cStatus::I2C_OK) {
        this->countError(status, regOffset);
        this->finishOperation(false);
        return;
    }

    // A write carries nothing back, so success is just a count and an event.
    this->m_txnCount++;
    this->tlmWrite_TxnCount(this->m_txnCount);
    this->log_ACTIVITY_LO_WriteComplete(regOffset, this->m_writeValue);
    this->finishOperation(true);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void I2cTester ::SET_ADDRESS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 addr) {
    // Ground input: reject out of range, never assert.
    if (addr < MIN_I2C_ADDRESS || addr > MAX_I2C_ADDRESS) {
        this->log_WARNING_HI_InvalidConfig(addr);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    // Takes effect on the next transaction, so no idle check is needed: a
    // transaction already in flight keeps the address it started with.
    this->m_i2cAddress = addr;
    this->reportSettings();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void I2cTester ::SET_REGISTER_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 regOffset, U8 readLen) {
    // This bound is what keeps the driver's 255-byte payload FW_ASSERT
    // unreachable from the ground.
    if (readLen == 0 || readLen > MAX_READ_BYTES) {
        this->log_WARNING_HI_InvalidConfig(readLen);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    this->m_regOffset = regOffset;
    this->m_readLength = readLen;
    this->reportSettings();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void I2cTester ::READ_ONCE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (!this->m_configLoaded) {
        this->log_WARNING_HI_InvalidConfig(this->m_i2cAddress);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    if (!this->beginOperation(I2cTester_State::READ_CMD)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Stash the reply target BEFORE issuing the request: if the driver is busy
    // it rejects the request and re-enters finishOperation() synchronously from
    // inside startRead(), which needs these already set.
    this->m_pendingOpCode = opCode;
    this->m_pendingCmdSeq = cmdSeq;
    this->startRead();
}

void I2cTester ::WRITE_ONCE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 value) {
    if (!this->m_configLoaded) {
        this->log_WARNING_HI_InvalidConfig(this->m_i2cAddress);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    if (!this->beginOperation(I2cTester_State::WRITE_CMD)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    // Same ordering requirement as READ_ONCE: commit everything the completion
    // path reads before issuing the request.
    this->m_pendingOpCode = opCode;
    this->m_pendingCmdSeq = cmdSeq;
    this->m_writeValue = value;
    this->startWrite(value);
}

void I2cTester ::SET_POLLING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool enable) {
    this->m_polling = enable;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Private helper methods
// ----------------------------------------------------------------------

bool I2cTester ::beginOperation(I2cTester_State op) {
    if (this->m_state != I2cTester_State::IDLE) {
        this->log_WARNING_LO_Busy(op, this->m_state);
        this->m_busyRejects++;
        this->tlmWrite_BusyRejects(this->m_busyRejects);
        return false;
    }

    this->m_state = op;
    return true;
}

void I2cTester ::startRead() {
    // A code-contract assert, not an input check: both configure() and
    // SET_REGISTER bound m_readLength before it can reach this line, so a
    // violation here is a defect in this component.
    FW_ASSERT(this->m_readLength >= 1 && this->m_readLength <= MAX_READ_BYTES,
              static_cast<FwAssertArgType>(this->m_readLength));

    // Clear the receive buffer so bytes left over from a previous read cannot
    // be mistaken for data if the transaction fails part-way.
    std::memset(this->m_readBuffer, 0, this->m_readLength);

    this->m_writeBuffer[0] = this->m_regOffset;

    // Both buffers are descriptors over this component's own storage. The
    // driver DMAs out of txBuf and into rxBuf, so neither may be touched until
    // the completion callback arrives. rxBuf's size *is* the byte count to read.
    Fw::Buffer txBuf(this->m_writeBuffer, 1);
    Fw::Buffer rxBuf(this->m_readBuffer, static_cast<FwSizeType>(this->m_readLength));

    if (!this->isConnected_i2cWriteReadOut_OutputPort(0)) {
        // The driver never produces I2C_OPEN_ERR; it is synthesized here so a
        // topology wiring mistake reads the same way as a bus failure.
        this->m_openErrors++;
        this->tlmWrite_OpenErrors(this->m_openErrors);
        this->log_WARNING_HI_I2cError(Drv::I2cStatus::I2C_OPEN_ERR, this->m_i2cAddress, this->m_regOffset);
        this->finishOperation(false);
        return;
    }

    // Completion arrives via writeReadCompleteIn -- normally from the driver's
    // activeIn tick, but SYNCHRONOUSLY from inside this call if the driver is
    // already busy.
    this->i2cWriteReadOut_out(0, static_cast<U32>(this->m_i2cAddress), txBuf, rxBuf);
}

void I2cTester ::startWrite(U8 value) {
    this->m_writeBuffer[0] = this->m_regOffset;
    this->m_writeBuffer[1] = value;
    Fw::Buffer wrBuf(this->m_writeBuffer, WRITE_BUFFER_SIZE);

    if (!this->isConnected_i2cWriteOut_OutputPort(0)) {
        this->m_openErrors++;
        this->tlmWrite_OpenErrors(this->m_openErrors);
        this->log_WARNING_HI_I2cError(Drv::I2cStatus::I2C_OPEN_ERR, this->m_i2cAddress, this->m_regOffset);
        this->finishOperation(false);
        return;
    }

    // Completion arrives via writeCompleteIn; see the caveat in startRead().
    this->i2cWriteOut_out(0, static_cast<U32>(this->m_i2cAddress), wrBuf);
}

U8 I2cTester ::recoverRegOffset(const Fw::Buffer& writeBuffer) const {
    if (writeBuffer.getSize() < 1) {
        return this->m_regOffset;
    }
    return static_cast<const U8*>(writeBuffer.getData())[0];
}

bool I2cTester ::handleReadComplete(const U8* data, FwSizeType len, U8 regOffset) {
    if (len < this->m_readLength) {
        // The SAMD21 driver returns the requested size on a successful read, so
        // this should be unreachable. Counting it rather than dropping it is the
        // point: a silent short read would look exactly like healthy telemetry.
        this->m_shortReads++;
        this->tlmWrite_ShortReads(this->m_shortReads);
        this->log_WARNING_HI_ShortRead(this->m_readLength, static_cast<U8>(len));
        return false;
    }

    // Pack MSB-first and zero-padded past m_readLength, so a 2-byte read of
    // 0xAB 0xCD reads as 0xABCD0000 -- left-justified, matching how a
    // multi-byte device register is normally presented.
    U32 words[2] = {0, 0};
    for (U8 i = 0; i < this->m_readLength; i++) {
        const U8 shift = static_cast<U8>(24 - 8 * (i % 4));
        words[i / 4] |= static_cast<U32>(data[i]) << shift;
    }

    // One timestamp for the whole set: these channels describe a single
    // transaction and should not appear to have been sampled at different times.
    const Fw::Time now = this->getTime();
    this->tlmWrite_ReadData0(words[0], now);
    this->tlmWrite_ReadData1(words[1], now);
    this->tlmWrite_ReadLength(this->m_readLength, now);
    this->m_txnCount++;
    this->tlmWrite_TxnCount(this->m_txnCount, now);

    this->log_ACTIVITY_LO_ReadComplete(regOffset, this->m_readLength, words[0], words[1]);
    return true;
}

void I2cTester ::countError(const Drv::I2cStatus& status, U8 regOffset) {
    const Fw::Time now = this->getTime();

    switch (status.e) {
        case Drv::I2cStatus::I2C_ADDRESS_ERR:
            this->m_addressErrors++;
            this->tlmWrite_AddressErrors(this->m_addressErrors, now);
            break;
        case Drv::I2cStatus::I2C_WRITE_ERR:
            this->m_writeErrors++;
            this->tlmWrite_WriteErrors(this->m_writeErrors, now);
            break;
        case Drv::I2cStatus::I2C_READ_ERR:
            this->m_readErrors++;
            this->tlmWrite_ReadErrors(this->m_readErrors, now);
            break;
        case Drv::I2cStatus::I2C_OPEN_ERR:
            this->m_openErrors++;
            this->tlmWrite_OpenErrors(this->m_openErrors, now);
            break;
        case Drv::I2cStatus::I2C_OTHER_ERR:
        default:
            // I2C_OTHER_ERR means the driver was busy when the request arrived,
            // or its stall watchdog force-recovered the transaction. An
            // unrecognized status lands here too: the status crosses a driver
            // boundary, so it is bucketed rather than asserted on.
            this->m_otherErrors++;
            this->tlmWrite_OtherErrors(this->m_otherErrors, now);
            break;
    }

    // The counter says how often; the event says which transaction. regOffset
    // comes from the completed transaction's write buffer, not from
    // m_regOffset, which SET_REGISTER may have changed since the request went
    // out.
    this->log_WARNING_HI_I2cError(status, this->m_i2cAddress, regOffset);
}

void I2cTester ::finishOperation(bool success) {
    const Fw::CmdResponse cmdStatus = success ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR;

    // Snapshot and clear the state BEFORE replying: the reply is dispatched
    // synchronously into the command dispatcher, and anything it reaches may
    // issue a new request re-entrantly. It must see an idle component.
    const I2cTester_State state = this->m_state;
    this->m_state = I2cTester_State::IDLE;

    switch (state) {
        case I2cTester_State::READ_CMD:
        case I2cTester_State::WRITE_CMD:
            this->cmdResponse_out(this->m_pendingOpCode, this->m_pendingCmdSeq, cmdStatus);
            break;
        case I2cTester_State::READ_SCHED:
            // Periodic read: no initiator to reply to.
            break;
        case I2cTester_State::IDLE:
        default:
            // A completion with no active transaction. Reachable if a completion
            // port is wired to a component that never requested anything; a
            // safe no-op rather than a fault.
            break;
    }
}

void I2cTester ::reportSettings() {
    const Fw::Time now = this->getTime();
    this->tlmWrite_TargetAddress(this->m_i2cAddress, now);
    this->tlmWrite_RegisterOffset(this->m_regOffset, now);
    this->tlmWrite_ReadLength(this->m_readLength, now);
}

}  // namespace CuriosityReference
