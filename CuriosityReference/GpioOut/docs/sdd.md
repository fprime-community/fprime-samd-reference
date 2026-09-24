# CuriosityReference::GpioOut

Drives one GPIO output pin to a commanded logic level.

## Introduction

`GpioOut` is the write half of the `Samd21::GpioDriver` demonstration and the
shortest path in the whole deployment from a ground command to a hardware
register:

```
uplink frame -> Samd21.UsartDriver -> Svc.ComStub -> Svc.FrameAccumulator
  -> Svc.FprimeDeframer -> Samd21.FprimeRouter -> Samd21.StaticCmdDispatcher
  -> GpioOut::SET_cmdHandler -> Drv.GpioWrite -> Samd21::GpioDriver
  -> PORT peripheral OUT register
```

Everything from the dispatcher rightward is synchronous on one stack, with no
queue and no thread anywhere in it. `SET` returns to the dispatcher only after the
pin has physically changed. That is the whole point of the passive/baremetal
model, and this component is the smallest place to see it.

The component exists mostly to make one further point: **`Drv.GpioWrite` returns a
`Drv.GpioStatus`, and a command handler that discards it reports `OK` for writes
that never reached a pin.** The plausible failure is a topology mistake — a driver
instance whose `configureOutput` call was never added to the `startTasks` phase
(`NOT_OPENED`), or one configured with `configureInput` (`INVALID_MODE`). Both are
provoked by a single ground command, so both are reported (`WriteFailed`) and fail
the command (`EXECUTION_ERROR`) rather than asserted on. Checking that status is
the only reason this component imports `Fw.Event` and takes a time port at all;
without them it would have no way to tell the ground the write was refused.

## Requirements

| Name | Description | Rationale | Validation |
|---|---|---|---|
| GPIOOUT-001 | `SET($state)` shall drive the pin to `$state` through `write` and reply `OK`. | Nominal output path. | Code inspection; hardware test (Curiosity Nano, PA25) |
| GPIOOUT-002 | `SET` shall, when `write` returns a status other than `OP_OK`, emit `WriteFailed` with that status and the requested level, and reply `EXECUTION_ERROR`. | The status is reachable from the ground via a mis-wired or unconfigured driver instance; reporting `OK` for a write that never reached a pin is worse than failing. | Code inspection |
| GPIOOUT-003 | `SET` shall not assert on any value of `Drv.GpioStatus`. | A single ground command must not be able to reset the board. | Code inspection |
| GPIOOUT-004 | The driver invocation shall use output port index 0. | One instance drives exactly one pin. | Code inspection |
| GPIOOUT-005 | The component shall hold no state and require no configuration. | Project constraint: passive, no dynamic allocation; nothing to initialize. | Code inspection (no members) |

## Design

### Ports

| Port | Kind | Type | Purpose |
|---|---|---|---|
| `write` | output | `Drv.GpioWrite($state: Fw.Logic)` → `Drv.GpioStatus` | Drive the pin. Wired to `Samd21.GpioDriver.gpioWrite`. |
| `timeCaller` | time get | — | Timestamps the `WriteFailed` event. |

`Fw.Command` and `Fw.Event` are imported. No `Fw.Channel` import and no telemetry:
the commanded level is already in the command dictionary and the command
response, so a channel echoing it would add a packet slot for nothing.

### Commands

| Opcode | Command | Arguments | Behavior |
|---|---|---|---|
| 0 | `SET` | `$state: Fw.Logic` | Drives the pin; `OK`, or `WriteFailed` + `EXECUTION_ERROR`. |

### Events

| Id | Event | Severity | Meaning |
|---|---|---|---|
| 0 | `WriteFailed(status: Drv.GpioStatus, $state: Fw.Logic)` | warning high | The driver refused the write and the pin was not changed. `NOT_OPENED` = the paired driver instance was never configured; `INVALID_MODE` = it was configured as an input. |

The event carries the requested level as well as the status so a failed `SET`
is self-describing in the event log without cross-referencing the command
history.

### Telemetry

None.

### State

None. The component has no member variables; `SET_cmdHandler` is a single
delegate-and-check. The pin's level lives in the PORT peripheral, and reading it
back would require a `Drv.GpioRead` port on a driver instance configured as an
output — which returns `INVALID_MODE`. There is deliberately no shadow copy of
the pin state to drift out of sync with the hardware.

## Topology Integration

Instances (`CuriosityReference/Top/instances.fpp`) — the component and the driver
instance that backs it:

```fpp
instance pinOut: CuriosityReference.GpioOut base id 0xBB05

instance outPA25: Samd21.GpioDriver base id 0xBB09 {
    phase Fpp.ToCpp.Phases.startTasks """
    outPA25.configureOutput(
        Samd21::GpioDriver::Group::PA,
        Samd21::GpioDriver::Pin::PIN_25
    );
    """
}
```

Connections (`CuriosityReference/Top/topology.fpp`):

```fpp
connections Pins {
    ...
    # Output
    pinOut.write -> outPA25.gpioWrite
}
```

`write` is a returning output port, so the generated base class asserts that it is
connected (`FW_ASSERT(m_write_OutputPort[portNum].isConnected())`) rather than
returning an error. Leaving the edge out of the topology is a FATAL on the first
`SET`, not a `WriteFailed`. This is the one failure mode the status check cannot
cover, and it is caught at integration time on the first command.

Commands, events, and time are wired by the topology's pattern specifiers.

Base IDs are tightly packed. Opcodes, event ids, and channel ids are each offset
from the instance's base id in their own numbering space, so `pinOut` at `0xBB05`
has room for offsets 0-3 in each space before it overlaps `outPA25` at `0xBB09`.
`GpioOut` uses opcode 0 and event id 0, so three of each are left. (The driver
instances themselves have no dictionary items, so their base ids are free to move.)

## Configuration

`GpioOut` itself takes no configuration. All of it lives on the paired
`Samd21.GpioDriver` instance, called once from the topology's `startTasks` phase:

- `configureOutput(group, pin)` — which physical pin. PA25 in the reference. There
  is no mode argument; calling `configureOutput` *is* the choice of direction, and
  `Samd21::GpioDriver` asserts if either configure method is called twice.

If the `configureOutput` call is omitted, every `SET` returns `NOT_OPENED` and
fails cleanly with a `WriteFailed` event — which is exactly the case GPIOOUT-002
exists to cover, and the easiest way to see it on hardware.

## Tested Configurations

| Board | Chip | Instances | Pin | Ops tested | Result |
|---|---|---|---|---|---|
| Microchip Curiosity Nano | SAMD21G17A | `pinOut`, `outPA25` | PA25 (output) | `SET(HIGH)`, `SET(LOW)` | Pass |

**PA25 is the SAMD21's USB D+ pad.** Using it as a GPIO is fine on the Curiosity
Nano, whose programming/debug link is the on-board debugger rather than the
SAMD21's own USB, but it precludes ever bringing up device USB on this build. The
datasheet also notes PA24/PA25 have no drive-strength option and recommend a pull
when left unconnected. PA25 was chosen for the reference because it is a
convenient breadboard pin, not because it is special — pick a pin your application
is not using.

## Adapting this component

- **A different pin.** Change `Pin::PIN_25` in the driver instance's
  `configureOutput` call. Nothing in `GpioOut` names a pin. Prefer a pin that is
  not PA24/PA25 if USB matters to you.
- **A second pin.** Add a second `GpioOut` instance *and* a second
  `Samd21.GpioDriver` instance, and connect them pairwise. `write_out(0, ...)` is
  hardcoded to index 0; one instance is one pin by construction.
- **A toggle or pulse command.** `TOGGLE` needs a shadow copy of the level (see
  **State** for why there isn't one); a `PULSE(duration)` command needs a
  `Svc.Sched` input port to time the release, since a busy-wait inside
  `SET_cmdHandler` would stall the whole main loop.
- **Driving the pin from something other than the ground.** Replace the command
  handler with a `Drv.GpioWrite`-typed *input* port and let another component
  drive it; keep the status check.
- **Renaming or moving the directory** requires updating the `#include` prefix in
  `GpioOut.hpp` to match the new path relative to the project root.

## Limitations

- One pin per instance (port index 0 is hardcoded).
- Ground-commanded only. There is no autonomous or scheduled output, and no way
  for flight software to drive the pin without adding a port.
- No pin-state readback, by design (see **State**). The ground's only record of
  the commanded level is the command history and the `WriteFailed` event.
- No telemetry, so a `SET` that succeeded leaves no trace in a packet — only in
  the command response.
- An unconnected `write` port is a FATAL assert on first use rather than a
  reported error; this is generated-code behavior for returning ports and is not
  something the component can intercept.
