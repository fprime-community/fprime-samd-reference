# CuriosityReference::FramTester

Ground-driven exerciser for `Fram::FramDriver`, and the worked example of the
asynchronous F-RAM client contract.

## 1. Introduction

`CuriosityReference::FramTester` proves that `Fram::FramDriver`, and the
`Samd21::SpiDriver` and `Samd21::DmaDriver` behind it, work end-to-end on a board
with a CY15B102QN attached, and makes every outcome the driver can report legible
from events and telemetry alone. It writes deterministic byte patterns, reads them
back and compares, dumps raw bytes, and can run a write/verify soak loop from the
rate group. It is part of the CuriosityReference **test build** only; it is not
shipped with fprime-devices.

Offsets and lengths are forwarded to the driver on purpose with only the check
needed to keep this component's own buffer safe, so the ground can provoke each
`Fram.Status` rejection and see it counted.

## 2. Requirements

| Name | Description | Validation |
| ---- | ----------- | ---------- |
| REQ-FRAMT-001 | The FramTester shall, on `WRITE_PATTERN(offset, length, seed)`, write the pattern `seed + i` (i = 0..length-1) at `offset` and report completion with `WriteComplete`. | Hardware Test |
| REQ-FRAMT-002 | The FramTester shall, on `READ_VERIFY(offset, length, seed)`, read `length` bytes at `offset`, compare them against `seed + i`, and report `VerifyComplete` or the first mismatch with `PatternMismatch`. | Hardware Test |
| REQ-FRAMT-003 | The FramTester shall, on `READ(offset, length)`, read `length` bytes and report the first eight in `ReadComplete` and in `ReadData0/1`, zero past `length`; and shall, when polling is enabled, alternate pattern write and verify on the poll region each `schedIn` tick. | Hardware Test |
| REQ-FRAMT-004 | The FramTester shall reject a `length` above `Fram.Config.MaxTransfer` with `InvalidArgument` and `VALIDATION_ERROR` without issuing a request, and shall report every non-OK driver status with `FramError` and count it in `FramErrors`. | Hardware Test |
| REQ-FRAMT-005 | The FramTester shall count completed writes, reads, verify failures, driver errors, busy rejects and soak cycles in telemetry and report the most recent status in `LastStatus`; command responses shall be issued only when the driver's completion arrives. | Hardware Test |

## 3. Design

### 3.1 Overview

The component holds a single `State` (`IDLE` when free) and services one
transaction at a time. A transaction is started from one of four places:

- `WRITE_PATTERN` (`WRITE_CMD`), `READ_VERIFY` (`VERIFY_CMD`), `READ` (`READ_CMD`) — reply on `cmdResponse` when the driver completes;
- `schedIn` with polling enabled — alternates `POLL_WRITE` (pattern `pollSeed`) and `POLL_VERIFY`; after a verify, pass or fail, the next tick rewrites (pass advances `pollSeed`, fail rewrites the same seed). No reply path.

Anything arriving while non-`IDLE` is rejected: `Busy` event, `BusyRejects` bumped,
command initiators get `EXECUTION_ERROR`.

### 3.2 Ports

| Kind | Name | Type | Notes |
| ---- | ---- | ---- | ----- |
| sync input | `schedIn` | `Svc.Sched` | 1 Hz rate group; drives the soak loop |
| output | `framReadOut` | `Fram.Request` | |
| sync input | `readCompleteIn` | `Fram.Reply` | main context (or synchronous on rejection) |
| output | `framWriteOut` | `Fram.Request` | |
| sync input | `writeCompleteIn` | `Fram.Reply` | main context (or synchronous on rejection) |

### 3.3 Commands

| Command | Args | Behaviour |
| ------- | ---- | --------- |
| `WRITE_PATTERN` | `offset: U32, length: U8, seed: U8` | write `seed+i`; `WriteComplete` |
| `READ_VERIFY` | `offset, length, seed` | read, compare; `VerifyComplete` or `PatternMismatch` + `EXECUTION_ERROR` |
| `READ` | `offset, length` | read; `ReadComplete(offset, length, data0, data1)` |
| `SET_POLLING` | `enable: bool` | soak loop on/off (off at boot) |
| `SET_POLL_REGION` | `offset, length (1..MaxTransfer)` | region the soak loop uses (default 0x0, 32 bytes) |

`length` is checked against `Fram.Config.MaxTransfer` (the size of this component's only
buffer); `length = 0` and any `offset` are forwarded so the driver's `TOO_LARGE` and
`OUT_OF_RANGE` rejections can be exercised from the ground.

### 3.4 Events and telemetry

Events: `Busy`, `FramError(op, status, offset)`, `PatternMismatch(offset, expected, actual)`,
`InvalidArgument(value)`, `ReadComplete`, `WriteComplete`, `VerifyComplete`.

Telemetry (packet `Fram`, 1 Hz): `WriteCount`, `ReadCount`, `VerifyFailures`,
`FramErrors`, `BusyRejects`, `LastStatus`, `ReadData0`, `ReadData1`, `PollCycles`.

### 3.5 The async F-RAM client contract

1. **Normal completions arrive in main context; rejections do not.** A valid request
   completes later from `Fram.framDriver.activeIn`. A rejected request (`BUSY`,
   `OUT_OF_RANGE`, `TOO_LARGE`) re-enters the completion handler **synchronously,
   inside the request call**. The component therefore stores the command's
   opcode/sequence and sets its state *before* calling `framReadOut`/`framWriteOut`,
   and `finishOperation()` clears the state *before* replying.
2. **The buffer belongs to the client.** `m_data` is handed to the driver through
   `m_buffer` and must not be touched until the completion arrives: for reads the
   driver fills it from main context on the completing tick; its identity is asserted
   on completion.
3. **Both completion ports must be connected.** The driver drops a completion whose
   port is unconnected and the client wedges mid-transaction.

## 4. Integration

Instance `framTester` (base id 0xDD40) in `CuriosityReference/Top`: `rg1Hz` member 2 →
`schedIn`; request/completion pairs to `Fram.framDriver`; channels in the `Fram`
telemetry packet, which `rg1Hz` member 3 drives into `tlm.pktSendIn[2]`. The F-RAM
chip select `framCs` (PA18) is configured in `TopTopology.cpp` before `configComponents()`.

## 5. Tests

- Integration: `test/int/test_fram_tester.py` (pytest, `fprime_test_api`) — round trip,
  short read padding, mismatch, end-of-device, out-of-range, zero length, oversize,
  counters/last status, polling soak. `BUSY` cannot be provoked deterministically from
  the ground and is covered by the FramDriver unit test.
- Unit test: `test/ut` (GTest, FramDriver played by the harness with a byte model of the device) — poll alternation and seed advance, failed verify rewrites the same seed, command round trips incl. short-read padding, busy rejection, argument validation, driver error status. Regression for the `m_pollVerifyNext` fault found on hardware.
