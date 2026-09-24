# CuriosityReference::FrameworkHealth

Counts rate-group ticks and publishes the count as telemetry.

## Introduction

`FrameworkHealth` is the smallest useful component in the reference deployment
and exists to make the baremetal scheduling model observable. It is passive: it
owns no thread, no queue, and no hardware. Every line it runs executes inside a
`Svc.Sched` invocation delivered by the rate group that drives it.

The full chain it demonstrates, all of it in main context:

```
RTC interrupt (Samd21.RtcDriver)          wakes the CPU out of WFI
  -> Svc.PassiveCycler main loop          calls rateDriver.activeIn
  -> Samd21.RtcDriver.CycleOut            emitted from activeIn_handler
  -> Samd21.PassiveRateGroupDriver        divides the 8 Hz tick
  -> Svc.PassiveRateGroup (rg10s)         fans out to its members
  -> FrameworkHealth.schedIn              tlmWrite_Counter()
  -> Samd21.StaticTlmPacketizer           latches the value into packet Health
  -> Samd21.Framer -> Samd21.UsartDriver  on tlm.SEND_PKT(Health)
```

Note the last step: the reference topology leaves `tlm.pktSendIn` unconnected,
so packets are emitted only in response to the ground command
`tlm.SEND_PKT(<packet id>)`. `Counter` is *latched* on every tick but *downlinked*
on demand. Two successive `Health` packets whose `Counter` advanced prove the
whole chain above is alive; a `Counter` that does not advance between polls is
the first thing to look at when the board goes quiet.

## Requirements

| Name | Description | Rationale | Validation |
|---|---|---|---|
| FWHEALTH-001 | Each `schedIn` invocation shall write the current tick count to the `Counter` channel and then advance it, while emission is enabled. | The channel is the deployment's liveness indicator; it must advance monotonically with the rate group. | Code inspection; hardware test (Curiosity Nano, `rg10s`) |
| FWHEALTH-002 | The first `Counter` sample after reset shall be 0. | An unambiguous post-reset value lets the ground distinguish a reset from a stall. | Code inspection (publish-then-increment) |
| FWHEALTH-003 | `SET_STATE(false)` shall stop `Counter` updates; `SET_STATE(true)` shall resume them. | Allows a noisy channel to be silenced without a rebuild or a topology change. | Code inspection |
| FWHEALTH-004 | `SET_STATE` shall emit `EmissionStateSet` with the new state and reply `OK`. | With emission off the channel stops updating; the event is what distinguishes "commanded off" from "the board stopped ticking". | Code inspection |
| FWHEALTH-005 | Emission shall be enabled at construction. | A board that has never been commanded must still report liveness. | Code inspection (constructor) |
| FWHEALTH-006 | The component shall hold no dynamic memory, no thread, and no hardware handle. | Project constraint: passive components only, no dynamic allocation, 16 KiB RAM. | Code inspection (two scalar members) |

## Design

### Ports

| Port | Kind | Type | Purpose |
|---|---|---|---|
| `schedIn` | sync input | `Svc.Sched` | Rate-group tick. Publishes one `Counter` sample. |
| `timeCaller` | time get | — | Timestamps telemetry and events. Resolved to `timeHandler` by the topology's `time connections` pattern. |

`Fw.Command`, `Fw.Event`, and `Fw.Channel` are imported, which adds the standard
command/event/telemetry ports.

### Commands

| Opcode | Command | Arguments | Behavior |
|---|---|---|---|
| 0 | `SET_STATE` | `$state: bool` | Sets the emission flag, logs `EmissionStateSet`, replies `OK`. Cannot fail. |

### Events

| Id | Event | Severity | Meaning |
|---|---|---|---|
| 0 | `EmissionStateSet($state: bool)` | activity high | `SET_STATE` changed the emission flag. |

### Telemetry

| Id | Channel | Type | Meaning |
|---|---|---|---|
| 0 | `Counter` | `U32` | Rate-group ticks observed while emission was enabled. Wraps at `2^32`. |

### State

Two members, both scalars:

- `m_running` (`bool`, `true` at construction) — emission flag set by `SET_STATE`.
- `m_counter` (`U32`, `0` at construction) — tick count.

`schedIn_handler` publishes `m_counter` *before* incrementing it, so the first
sample after reset is 0 (FWHEALTH-002). Ticks that arrive while `m_running` is
false are dropped entirely: the counter does not advance, so it counts *published
samples*, not elapsed ticks. That is deliberate — the channel is a downlink
liveness indicator, not a clock. Use `rg8Hz.CycleCount` if you need elapsed ticks.

There is no configure method and no initialization beyond the constructor.

## Topology Integration

Instance (`CuriosityReference/Top/instances.fpp`):

```fpp
instance fwHealth: CuriosityReference.FrameworkHealth base id 0xAA50
```

No `phase` blocks — the component needs no configuration.

Connections (`CuriosityReference/Top/topology.fpp`):

```fpp
connections RateGroups {
    # Rate group 3 -- 10 s period
    rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup3] -> rg10s.CycleIn
    rg10s.RateGroupMemberOut                              -> fwHealth.schedIn
}
```

`fwHealth` is the only member of `rg10s`, so it takes index 0 implicitly.
Commands, events, telemetry, and time are wired by the topology's pattern
specifiers (`command connections instance cmdDisp`, `event connections instance
downlink`, `telemetry connections instance tlm`, `time connections instance
timeHandler`).

The channel must also be named in the topology's packet set. FPP requires the
packet set to be complete — every channel in the topology must appear in exactly
one packet or in the `omit` block, or the build fails — and
`Samd21.StaticTlmPacketizer` generates its packet storage from that set at compile
time, logging `NoChan` at runtime for any channel it was not told about:

```fpp
packet Health group 0 {
    rateDriver.CycleOverrun
    fwHealth.Counter
    ...
}
```

## Configuration

None. The component has no `configure()` method, no config header, and no
compile-time tunables.

## Adapting this component

- **Change the rate.** Move the `rg10s.RateGroupMemberOut -> fwHealth.schedIn`
  edge to `rg1Hz` or `rg8Hz`. Nothing in the component depends on the period.
- **Add health inputs.** This is the natural place to hang a real
  `Svc.Health`-style check: add output ports for whatever you want to poll, call
  them from `schedIn_handler`, and add channels/events for the results. The
  existing `m_running` gate then doubles as a master enable.
- **Count elapsed ticks instead of published samples.** Move `m_counter++` above
  the `if (this->m_running)`.
- **Add a reset.** A `RESET_COUNTER` command zeroing `m_counter` is a two-line
  addition; it was left out to keep the dictionary minimal.
- **Renaming or moving the directory** requires updating the `#include` prefix in
  `FrameworkHealth.hpp` to match the new path relative to the project root — the
  autocoded base class lands at
  `<build>/F-Prime/<path-from-project-root>/FrameworkHealthComponentAc.hpp`.

## Limitations

- `Counter` is a free-running `U32` with no reset command; it wraps after
  `2^32` published samples (~1360 years at the 10 s rate).
- `Counter` counts published samples, not elapsed ticks (see **State**).
- `m_counter` is not persisted; it restarts at 0 on every reset, which is the
  intended reset indicator.
- The component reports nothing about the rest of the system. It is evidence the
  scheduler and the downlink path work, not a health monitor.
