# CuriosityReference::I2cTester

Bus-exercising instrument for `Samd21::I2cDriver`, and the worked example of the
asynchronous I2C client contract.

## 1. Introduction

`CuriosityReference::I2cTester` models no real part. It writes a register-offset
byte to a ground-settable 7-bit address, reads a ground-settable number of bytes
back across a repeated START, and reports both the bytes and the outcome. Its
purpose is to prove that `Samd21::I2cDriver` and the `Samd21::DmaDriver` behind
it work end-to-end on a new board, and to make every failure mode the driver can
report legible from telemetry alone.

Two audiences:

- **Board bring-up.** Point it at whatever is on the bus — or at nothing, which
  is itself a useful test: an address with no target NACKs, and the NACK shows up
  as a `WriteErrors` count and an `I2cError` event rather than as silence.
- **Component authors.** This is the smallest complete correct client of the
  driver's async request/callback interface. Three properties of that interface
  are easy to get wrong, all three are load-bearing here, and all three are
  spelled out in §3.5.

It is a passive component. Every request it makes is issued from a handler
(a rate-group tick or a command) and completes later on a callback input port.

## 2. Requirements

| Name         | Description                                                                                                                       | Validation |
| ------------ | --------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| CREF-I2CT-001 | The I2cTester shall perform a write-read against a configurable 7-bit address and register offset when enabled and ticked.        | Hardware Test |
| CREF-I2CT-002 | The I2cTester shall report the bytes read back as telemetry and as an event.                                                      | Hardware Test |
| CREF-I2CT-003 | The I2cTester shall count every transaction outcome the driver can report in a separate per-failure-mode counter.                 | Unit Test  |
| CREF-I2CT-004 | The I2cTester shall emit an event on every failed transaction, in addition to counting it.                                        | Unit Test  |
| CREF-I2CT-005 | The I2cTester shall perform one I2C transaction at a time, rejecting and counting any request that arrives while one is active.    | Unit Test  |
| CREF-I2CT-006 | The I2cTester shall reject out-of-range ground-supplied addresses and read lengths without asserting.                              | Unit Test  |
| CREF-I2CT-007 | The I2cTester shall not drive the bus until commanded to, so that a reset board is quiet by default.                               | Unit Test  |
| CREF-I2CT-008 | The I2cTester shall reject every request issued before `configure()` has succeeded.                                                | Unit Test  |

Unit tests are **not yet written** — see §7.

## 3. Design

### 3.1 Overview

The component holds a single `State` (`IDLE` when free) and services one
transaction at a time. A transaction is started from one of three places:

- `schedIn`, when polling is enabled (`READ_SCHED`) — no reply path;
- the `READ_ONCE` command (`READ_CMD`) — replies on `cmdResponse`;
- the `WRITE_ONCE` command (`WRITE_CMD`) — replies on `cmdResponse`.

Anything that arrives while non-`IDLE` is rejected: a `Busy` event is emitted,
`BusyRejects` is bumped, and command initiators get `EXECUTION_ERROR`.

On completion the callback handler decodes the result, drives telemetry and
events, and then `finishOperation()` returns the component to `IDLE` and replies
to whoever started the transaction.

### 3.2 Ports

| Kind         | Name                  | Port Type                  | Usage                                              |
| ------------ | --------------------- | -------------------------- | -------------------------------------------------- |
| `sync input` | `schedIn`             | `Svc.Sched`                | Rate-group tick; drives the periodic read          |
| `output`     | `i2cWriteReadOut`     | `Drv.I2cWriteReadRequest`  | Write the register offset, then read the data      |
| `sync input` | `writeReadCompleteIn` | `Drv.I2cWriteReadCallback` | Write-read completion + status                     |
| `output`     | `i2cWriteOut`         | `Drv.I2cRequest`           | Write-only transaction (`WRITE_ONCE`)              |
| `sync input` | `writeCompleteIn`     | `Drv.I2cCallback`          | Write completion + status                          |
| `time get`   | `timeCaller`          | —                          | Timestamp source for telemetry                     |

`Drv.I2cWriteReadRequest` and `Drv.I2cRequest` are **void-returning**. The
outcome of a request never comes back as a return value; it always arrives later
on the matching callback port. Both callback ports must be connected — see
§3.5.3.

### 3.3 Commands

| Opcode | Name           | Description                                                                 |
| ------ | -------------- | --------------------------------------------------------------------------- |
| 0      | `SET_ADDRESS`  | Retarget to a new 7-bit address in `[0x08, 0x77]`                           |
| 1      | `SET_REGISTER` | Set the register offset and the read length (`[1, 8]`)                      |
| 2      | `READ_ONCE`    | Issue one write-read now; the reply is deferred until it completes           |
| 3      | `WRITE_ONCE`   | Write `[regOffset, value]` now; the reply is deferred until it completes     |
| 4      | `SET_POLLING`  | Enable or disable the `schedIn`-driven periodic read                        |

`SET_ADDRESS` and `SET_REGISTER` do not require an idle component: they only
affect the *next* transaction. A transaction already in flight keeps the address,
offset and length it started with, which is why the completion handlers recover
the offset from the wire rather than reading `m_regOffset` (§3.6).

`READ_ONCE` and `WRITE_ONCE` are *deferred-reply* commands: the handler claims
the component, stashes the opcode and command sequence, and issues the request.
The `cmdResponse` is sent from `finishOperation()` when the driver reports the
outcome, which is typically one `cycler` tick later.

### 3.4 Transaction Behavior

A **read** is a single I2C write-read: one register-offset byte is written, the
clock is held for a repeated START, and `ReadLength` bytes are read back. The
bytes are packed MSB-first and zero-padded into `ReadData0` / `ReadData1`, so a
2-byte read of `0xAB 0xCD` reads as `ReadData0 = 0xABCD0000`. The same two words
are repeated in the `ReadComplete` event, so a read is visible even to a ground
system that is not subscribed to the telemetry packet.

A **write** is `[regOffset, value]` in one transaction. Nothing comes back, so
success is a `TxnCount` bump plus a `WriteComplete` event.

`MAX_READ_BYTES` is 8. That is not arbitrary: it fills the two `U32` telemetry
channels exactly, and it keeps `readLen` — which is settable from the ground —
far below the driver's 255-byte DMA payload limit, which the driver enforces with
an `FW_ASSERT`. A ground-settable value must never be able to reach a framework
assert, so the bound is enforced in `SET_REGISTER` and in `configure()`.

### 3.5 The async I2C client contract

The three properties below are the reason this component exists as a reference.
Each of them is a real bug source in a device component written against
`Samd21::I2cDriver`.

#### 3.5.1 Normal completions arrive in main context; the busy reject does not

The driver's DMA/SERCOM ISR only records the outcome. The completion callbacks
are emitted from the driver's `activeIn` handler, which the topology wires to
`cycler.cycleOut` — i.e. from the main loop, outside interrupt context. That is
why the handlers here may freely emit telemetry, emit events, and call
`cmdResponse_out`, and it is why `cycler.cycleOut -> i2cDriver.activeIn` is a
**mandatory** topology connection: without it, no completion is ever delivered
and the component wedges non-`IDLE` forever.

There is one exception. If the driver is already mid-transaction when a request
arrives, it rejects the request by invoking the completion callback with
`I2C_OTHER_ERR` **synchronously, from inside the client's own
`i2cWriteReadOut_out(...)` call**. The client's completion handler therefore
re-enters before the request call returns.

Two consequences, both visible in the code:

- `beginOperation()` commits `m_state` and the command handlers stash
  `m_pendingOpCode` / `m_pendingCmdSeq` **before** `startRead()` / `startWrite()`
  is called. Anything the completion path reads must already be set.
- `finishOperation()` snapshots `m_state` into a local and resets it to `IDLE`
  **before** dispatching the reply, so a reply handler that issues a new request
  re-entrantly sees an idle component.

#### 3.5.2 The buffers belong to the client

`Fw::Buffer` here is a non-owning descriptor. There is no allocator, no
`bufferAllocate`/`bufferDeallocate` port, and nothing is returned to a manager.
The driver copies the *descriptor* and DMAs out of / into the client's own
storage, and hands the identical descriptor back on completion — which is what
the pointer `FW_ASSERT` in `writeReadCompleteIn_handler` checks.

Therefore: the transmit and receive buffers must not be touched between issuing a
request and its completion. Because only one transaction is in flight per driver,
one transmit buffer and one receive buffer per client is sufficient.

`readBuffer.getSize()` on the *request* is the byte count to read. The driver
does not shrink it to an actual received count on completion (the DMA is
fixed-length), so a short read is something the client detects by its own
bookkeeping — see `ShortReads`.

The receive buffer here is **per-instance**. A device component instantiated many
times on one bus may instead share a single file-static receive buffer, since only
one transaction is in flight per driver and the completion runs in the main
context; `MoonfallPm::Ltc2945`, from which this component is derived, does exactly
that across nine instances. At 8 bytes the sharing is not worth the cross-instance
invariant it would impose.

#### 3.5.3 Unconnected callback ports fail silently

The driver guards every callback with `isConnected_*_OutputPort()`. If a client's
completion input port is not wired, the completion is **dropped without a
diagnostic** and the client stays non-`IDLE` forever, rejecting everything
afterwards as busy. Both `writeReadCompleteIn` and `writeCompleteIn` must be
connected.

The mirror case — the client's *request* output port not being connected — is
handled locally: `startRead()` / `startWrite()` check
`isConnected_i2cWriteReadOut_OutputPort(0)` and synthesize an `I2C_OPEN_ERR`
outcome (`OpenErrors` + `I2cError`), because the driver never produces that
status itself.

### 3.6 Error Handling

Every failure does three things: bumps a counter, writes that counter's channel
immediately, and emits an event. The immediate write matters on a board where
telemetry packets are polled slowly — a failure is visible on the tick it
happens, not at the next poll.

`Drv.I2cStatus` is the *finest granularity a client can see*. The driver's
internal `I2cError` enum (`ARBITRATION_LOST`, `BUS_ERROR`, the SCL timeouts, …)
appears only in the driver's own `I2cBusError` event and `BusErrorCount` channel.
Breaking the client-side counters out per status is therefore the most
discrimination a client can offer, and it is exactly the discrimination bring-up
needs.

| Failure mode                    | Counter         | Event                     |
| ------------------------------- | --------------- | ------------------------- |
| Request while busy (local)      | `BusyRejects`   | `Busy`                    |
| `I2C_ADDRESS_ERR`               | `AddressErrors` | `I2cError`                |
| `I2C_WRITE_ERR`                 | `WriteErrors`   | `I2cError`                |
| `I2C_READ_ERR`                  | `ReadErrors`    | `I2cError`                |
| `I2C_OPEN_ERR` (port unwired)   | `OpenErrors`    | `I2cError`                |
| `I2C_OTHER_ERR`, unknown status | `OtherErrors`   | `I2cError`                |
| Short read on an OK completion  | `ShortReads`    | `ShortRead`               |
| Bad ground/topology argument    | —               | `InvalidConfig`           |

**Diagnostic guide:**

- `WriteErrors` climbing while `AddressErrors` stays flat ⇒ nothing is responding
  at the configured address. The SAMD21 driver reports an address NACK as
  `I2C_WRITE_ERR`, never as `I2C_ADDRESS_ERR`; a non-zero `AddressErrors` means
  the driver grew a new failure path.
- `OtherErrors` climbing ⇒ either something else is contending for the driver, or
  the driver's stall watchdog is force-recovering transactions. Cross-check
  `i2cDriver.StallRecoveryCount`.
- `OpenErrors` non-zero ⇒ topology wiring, not hardware.
- Nothing moving at all, `BusyRejects` climbing ⇒ a completion is being dropped.
  Check that both callback ports and `cycler.cycleOut -> i2cDriver.activeIn` are
  connected.
- `i2cDriver.BusErrorCount` climbing with a `I2cBusError` event ⇒ electrical or
  protocol problem (pull-ups, bus capacitance, SMBus timeouts); see the timeout
  discussion in `Top/instances.fpp`.

An `I2C_OPEN_ERR` arriving *as a completion status* is bucketed into
`OpenErrors` alongside the locally synthesized case, so the channel means "the
request never reached the bus" regardless of which side noticed.

### 3.7 Telemetry and Events

| Kind      | Name                                    | Description                                              |
| --------- | --------------------------------------- | -------------------------------------------------------- |
| Telemetry | `ReadData0`, `ReadData1`                | Last successful read, MSB-first, zero-padded             |
| Telemetry | `ReadLength`                            | Bytes read per transaction                               |
| Telemetry | `TxnCount`                              | Successful transactions (reads and writes)               |
| Telemetry | `BusyRejects`                           | Requests rejected because one was already in flight      |
| Telemetry | `AddressErrors` … `OtherErrors`         | Per-`I2cStatus` failure counters (see §3.6)              |
| Telemetry | `ShortReads`                            | Successful reads that returned too few bytes             |
| Telemetry | `TargetAddress`, `RegisterOffset`       | Current settings, for ground readback                    |
| Event     | `ReadComplete`                          | Successful read, with the bytes (activity/low)           |
| Event     | `WriteComplete`                         | Successful write (activity/low)                          |
| Event     | `Busy`                                  | Request rejected, with both states (warning/low)         |
| Event     | `I2cError`                              | Failed transaction: status, address, offset (warning/high) |
| Event     | `ShortRead`                             | Expected vs. actual byte count (warning/high)            |
| Event     | `InvalidConfig`                         | Rejected out-of-range value (warning/high)               |

`ReadData0`, `ReadData1`, `ReadLength` and `TxnCount` share one timestamp: they
describe a single transaction and must not look like they were sampled at
different times.

## 4. Configuration

```cpp
void configure(U8 i2cAddr,    // 7-bit address, in [0x08, 0x77]
               U8 regOffset,  // register offset written before each read
               U8 readLen);   // bytes to read back, in [1, 8]
```

Called once from the topology's `startTasks` phase — **not**
`configComponents`. `configure()` can log `InvalidConfig`, which requires the
event and time ports to already be connected, and `startTasks` is the first phase
where that is true.

An out-of-range argument logs `InvalidConfig` and returns **without** marking the
component configured. It does not assert: a topology defect should be reported on
the ground, not turned into a boot loop. Every request handler re-checks the
configured flag and rejects if it is unset.

Polling is off after `configure()`. Send `SET_POLLING(true)` to start the
periodic read.

## 5. Integration

### 5.1 Topology Connections

```fpp
connections I2cTester {
    i2cTester.i2cWriteReadOut   -> i2cDriver.writeRead
    i2cDriver.writeReadComplete -> i2cTester.writeReadCompleteIn
    i2cTester.i2cWriteOut       -> i2cDriver.write
    i2cDriver.writeComplete     -> i2cTester.writeCompleteIn
}

connections RateGroups {
    # ... one slot in a rate group
    rg1Hz.RateGroupMemberOut[1] -> i2cTester.schedIn
}
```

Leave the request/callback pairs unindexed on the `i2cDriver` side: `I2cDriver.fpp`
declares `match writeComplete with write` and `match writeReadComplete with
writeRead`, so FPP enforces that a request and its callback use the same client
port index.

Elsewhere in the topology, and not optional:

```fpp
connections ActiveRateGroups {
    cycler.cycleOut -> i2cDriver.activeIn   # REQUIRED: delivers completions
}
```

### 5.2 Instance Definition

```fpp
instance i2cTester: CuriosityReference.I2cTester base id 0xCC00 {
    phase Fpp.ToCpp.Phases.startTasks """
    i2cTester.configure(
    /* i2c address */ 0x50,
    /* reg offset  */ 0x00,
    /* read length */ 4
    );
    """
}
```

### 5.3 Telemetry Packets

`Samd21::StaticTlmPacketizer` requires that **every** channel of every instance
appear either in a packet or in the topology's `omit` block; an incomplete set is
a hard FPP error. All 13 channels are accounted for in `Top/topology.fpp`: the
seven failure counters in the `Error` packet, the six data/settings channels in
the `Tester` packet.

## 6. Tested Configurations

| Board Name               | Chip       | Bus     | Speed   | Target                 | Result       |
| ------------------------ | ---------- | ------- | ------- | ---------------------- | ------------ |
| Microchip Curiosity Nano | SAMD21G17D | SERCOM4 | 400 kHz | none (NACK path)       | Not yet run  |
| Microchip Curiosity Nano | SAMD21G17D | SERCOM4 | 400 kHz | 24C-series EEPROM 0x50 | Not yet run  |

The default address `0x50` is the 24Cxx serial-EEPROM address. Those parts are
byte-addressed, so the tester's write-offset-then-read sequence is exactly a
valid EEPROM read, and an EEPROM breakout is the cheapest thing to hang on the
bus. With nothing attached the component still exercises the full driver path and
reports `WriteErrors`, which is a valid bring-up result.

## 7. Limitations

- One transaction at a time; concurrent requests are rejected with `Busy`.
- Maximum read is 8 bytes, bounded by the two `U32` telemetry channels.
- A write carries exactly one data byte after the register offset.
- 7-bit addressing only; the driver does not support 10-bit addressing.
- No runtime re-`configure()`. The address, offset and length are changed by
  command after boot, not by calling `configure()` again.
- **No unit test yet.** The harness (`test/ut/I2cTesterTestMain.cpp`,
  `test/ut/I2cTesterTester.cpp`) has not been written and the `register_fprime_ut`
  block in `CMakeLists.txt` is commented out. Scaffold it with
  `fprime-util impl --ut` in this directory. The cases that matter most are the
  synchronous busy-reject re-entrancy (§3.5.1), the short-read path, and the
  ground-input rejection in `SET_REGISTER`.
