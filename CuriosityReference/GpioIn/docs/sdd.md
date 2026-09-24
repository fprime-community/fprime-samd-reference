# CuriosityReference::GpioIn

Reads and reports the level of one GPIO input pin.

## Introduction

`GpioIn` is the consumer side of the `Samd21::GpioDriver` input path. It is
passive, holds no state, and owns exactly one pin. It exists to demonstrate the
two distinct ways a component gets at a GPIO input, and the very different rules
that apply to each:

| | Mechanism | Trigger | Context |
|---|---|---|---|
| **pull** | `Drv.GpioRead` output port | `READ` ground command | Main context, on the command dispatcher's stack |
| **push** | `Svc.Cycle` input port (`Drv.Gpio`'s `gpioInterrupt`) | SAMD21 EIC edge detect | **ISR context** |

`Drv.GpioRead` is a *returning* port — it hands back a `Drv.GpioStatus` — and
this component always checks it. A read against a driver instance that was never
configured returns `NOT_OPENED`; one configured as an output returns
`INVALID_MODE`. Both are reachable from the ground by commanding `READ` against a
mis-wired topology, so both are reported as a `ReadFailed` event and an
`EXECUTION_ERROR` command response. Asserting on a driver status that a single
ground command can provoke would let that command reset the board.

## :warning: The `transitionIn` handler runs in ISR context

`Samd21::GpioDriver::gpioInterruptIsr()` calls `gpioInterrupt_out()` directly from
the SAMD21 External Interrupt Controller handler. Everything reachable from
`transitionIn_handler` therefore executes with the interrupt in progress:

1. `readOut_out(0, state)` — a PORT register read. Safe; it touches only the pin's
   own `IN` register.
2. `log_ACTIVITY_LO_LevelTransitioned(state)` — this is the problem. It calls
   `timeCaller` (`Samd21::Samd21Time`, an RTC counter read — safe), then pushes an
   `Fw::LogBuffer` through `Samd21::PassiveDownlink`, which serializes an
   `Fw::LogPacket` into a stack `Fw::ComBuffer` and calls
   `Samd21::Framer::comPacketQueueIn_handler`.

`Framer::comPacketQueueIn_handler` mutates a shared two-buffer TX pool
(`m_buffers[m_activeBufferIdx]`, `activeBuf->size`, `activeBuf->state`) and
serializes into it **with no critical section**. If the EIC fires while main
context is partway through that same handler — appending a telemetry packet, or
inside `flushActiveBuffer()` — the two appends interleave and the outgoing frame
is corrupted or a packet is lost. The window is small but it is real, and it grows
with edge rate.

The reference deployment ships this way because it is the shape the original
`Breadboard_Curiosity` deployment had and because PA23 in the reference wiring is
a slow, human-driven input. **Do not copy this pattern onto a fast signal.** The
safe restructuring, which requires no driver change:

```cpp
// in transitionIn_handler (ISR): latch only
this->m_edgePending = true;          // volatile bool, or an atomic flag

// in a new schedIn_handler (main context): read and report
if (this->m_edgePending) {
    this->m_edgePending = false;
    Fw::Logic state;
    const Drv::GpioStatus status = this->readOut_out(0, state);
    ...
}
```

That trades edge latency and coalescing of bursts for a handler that touches
nothing shared. It also costs a rate-group slot, which is why the trade is left
to the integrator rather than baked in.

A second consequence of the port type: `Drv.Gpio`'s `gpioInterrupt` is a
`Svc.Cycle` port, which carries only an `Os::RawTime` cycle start and has no room
for the pin level. The level in `LevelTransitioned` is therefore *re-sampled* in
the handler, after the edge. On a pin that can change twice inside the ISR
latency the reported level is the level after the second edge, and a rising/falling
pair can report the same level twice.

## Requirements

| Name | Description | Rationale | Validation |
|---|---|---|---|
| GPIOIN-001 | `READ` shall read the pin through `readOut` and emit `ReadLevel` with the level, then reply `OK`. | Nominal on-demand read path. | Code inspection; hardware test (Curiosity Nano, PA23) |
| GPIOIN-002 | `READ` shall, when `readOut` returns a status other than `OP_OK`, emit `ReadFailed` with that status and reply `EXECUTION_ERROR`, emitting no `ReadLevel`. | The status is reachable from the ground (mis-wired or unconfigured driver instance); it must not fault the board. | Code inspection |
| GPIOIN-003 | `transitionIn` shall read the pin through `readOut` and emit `LevelTransitioned` with the level. | Nominal edge-notification path. | Code inspection; hardware test (Curiosity Nano, PA23, both edges) |
| GPIOIN-004 | `transitionIn` shall, when `readOut` returns a status other than `OP_OK`, emit `ReadFailed` and return without emitting `LevelTransitioned`. | An ISR must not assert; the edge is dropped. | Code inspection |
| GPIOIN-005 | All driver invocations shall use output port index 0. | One instance drives exactly one pin. | Code inspection |
| GPIOIN-006 | The component shall hold no mutable state. | Nothing to corrupt when `transitionIn` pre-empts main context. | Code inspection (no members) |

## Design

### Ports

| Port | Kind | Type | Purpose |
|---|---|---|---|
| `readOut` | output | `Drv.GpioRead` → `Drv.GpioStatus` | Read the pin level. Wired to `Samd21.GpioDriver.gpioRead`. |
| `transitionIn` | sync input | `Svc.Cycle` | Edge notification from `Samd21.GpioDriver.gpioInterrupt`. **ISR context.** |
| `timeCaller` | time get | — | Timestamps events. |

`Fw.Command` and `Fw.Event` are imported. There is no `Fw.Channel` import and no
telemetry: this component reports events only.

### Commands

| Opcode | Command | Arguments | Behavior |
|---|---|---|---|
| 0 | `READ` | — | Reads the pin; `ReadLevel` + `OK`, or `ReadFailed` + `EXECUTION_ERROR`. |

### Events

| Id | Event | Severity | Meaning |
|---|---|---|---|
| 0 | `ReadLevel($state: Fw.Logic)` | activity low | Level reported by a commanded `READ`. |
| 1 | `LevelTransitioned($state: Fw.Logic)` | activity low | The pin saw a configured edge; level re-sampled after the edge. |
| 2 | `ReadFailed(status: Drv.GpioStatus)` | warning high | The driver rejected a read. `NOT_OPENED` = the paired driver instance was never configured; `INVALID_MODE` = it was configured as an output. |

### Telemetry

None.

### State

None. The component has no member variables. That is not an accident: with
`transitionIn` able to pre-empt `READ_cmdHandler` at any instruction, any member
this component owned would need a critical section around every access. Keeping
it stateless removes that whole class of bug — the only shared mutable state left
on the ISR path is inside `Samd21::Framer` (see the warning above).

## Topology Integration

Instances (`CuriosityReference/Top/instances.fpp`) — the component and the driver
instance that backs it:

```fpp
instance pinIn: CuriosityReference.GpioIn base id 0xBB00

instance inPA23: Samd21.GpioDriver base id 0xBB04 {
    phase Fpp.ToCpp.Phases.startTasks """
    inPA23.configureInput(
        Samd21::GpioDriver::Group::PA,
        Samd21::GpioDriver::Pin::PIN_23,
        Samd21::GpioDriver::InputPullMode::PULL_UP,
        Samd21::GpioDriver::ExternalInterruptMode::BOTH
    );
    """
}
```

Connections (`CuriosityReference/Top/topology.fpp`):

```fpp
connections Pins {
    # Input
    pinIn.readOut        -> inPA23.gpioRead
    inPA23.gpioInterrupt -> pinIn.transitionIn
    ...
}
```

Both edges matter. Without `pinIn.readOut -> inPA23.gpioRead` the component has
no way to sample the pin — an unconnected returning output port is a runtime
assert in the generated base class, not a graceful failure. Without
`inPA23.gpioInterrupt -> pinIn.transitionIn` the driver's `gpioInterruptIsr()`
returns early (it checks `isConnected_gpioInterrupt_OutputPort`) and edges are
silently dropped.

Base IDs are tightly packed. Opcodes, event ids, and channel ids are each offset
from the instance's base id in their own numbering space, so `pinIn` at `0xBB00`
has room for offsets 0-3 in each space before it overlaps `inPA23` at `0xBB04`.
`GpioIn` already uses event ids 0, 1, and 2 — **one event id is left**. A fourth
event, or a fifth of anything, needs the GPIO block re-spaced. (The driver
instances themselves have no dictionary items, so their base ids are free to move.)

## Configuration

`GpioIn` itself takes no configuration. All of it lives on the paired
`Samd21.GpioDriver` instance, called once from the topology's `startTasks` phase:

- `Group` / `Pin` — which physical pin. PA23 in the reference.
- `InputPullMode` — `NO_PULL`, `PULL_DOWN`, or `PULL_UP`. The reference uses
  `PULL_UP`, which is what a button-to-ground wants.
- `ExternalInterruptMode` — `NONE`, `RISING`, `FALLING`, or `BOTH`. `NONE` leaves
  the EIC untouched and no `transitionIn` ever fires; the `READ` command still
  works. Use `NONE` if you only want the pull path, and leave
  `gpioInterrupt` unconnected.

`configureInput` is called from `startTasks`, not `configComponents`, only because
that is where the reference topology puts all driver configuration. `GpioIn` does
not log during initialization, so either phase would work for it.

## Tested Configurations

| Board | Chip | Instances | Pin | Ops tested | Result |
|---|---|---|---|---|---|
| Microchip Curiosity Nano | SAMD21G17A | `pinIn`, `inPA23` | PA23 (input, pull-up, both edges) | `READ`, edge notification | Pass |

PA23 maps to `EXTINT[7]` on the SAMD21G. It is a plain breadboard pin in the
reference wiring, not a board-provided button or LED.

## Adapting this component

- **A different pin.** Change `Pin::PIN_23` in the driver instance's
  `configureInput` call. Nothing in `GpioIn` names a pin.
- **A second pin.** Add a second `GpioIn` instance *and* a second
  `Samd21.GpioDriver` instance, and connect them pairwise. `readOut_out(0, ...)`
  is hardcoded to index 0; one instance is one pin by construction.
- **Level-triggered or polled input.** Set `ExternalInterruptMode::NONE`, drop the
  `gpioInterrupt` edge, and add a `Svc.Sched` input port that calls the same read.
  That is also the fix for the ISR-context problem above.
- **Report the level as telemetry.** Add `import Fw.Channel` and a
  `telemetry Level: Fw.Logic` channel, then `tlmWrite_Level(state)` alongside the
  events — but note that `tlmWrite_` from `transitionIn` puts the packetizer on
  the ISR path too.
- **Renaming or moving the directory** requires updating the `#include` prefix in
  `GpioIn.hpp` to match the new path relative to the project root.

## Limitations

- One pin per instance (port index 0 is hardcoded).
- `transitionIn` runs in ISR context and emits an event from there. See the
  warning above; this is the component's one significant caveat.
- The reported transition level is re-sampled after the edge, not captured at it,
  so fast or bouncing inputs can report a stale or duplicate level. There is no
  debounce.
- No edge counter and no telemetry — every observation is an event, so a burst of
  edges can be dropped by `Framer` backpressure (visible as
  `framer.DroppedPackets`) with no record of how many were lost.
- Only one pin per SAMD21 `EXTINT` line can generate interrupts. PA23 uses
  `EXTINT[7]`, which it shares with PA07; configuring both as interrupting inputs
  is a hardware conflict the driver does not detect.
- `cycleStart` from the notification is discarded, so events are timestamped when
  the handler runs, not when the edge occurred.
