# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this
repository.

## Project Overview

This is an F´ (F Prime) reference deployment targeting the **Microchip SAMD21 Curiosity
Nano** (ATSAMD21G17D, ARM Cortex-M0+). It is **baremetal** — no RTOS — and uses the
SAMD21 drivers and services from `lib/fprime-samd` plus the no-OS OSAL from
`lib/fprime-baremetal`.

It is a *reference*: the point is that it is small, correct, and readable. Prefer the
change that is easier to copy from over the change that is cleverer.

**Key characteristics:**

- Baremetal, no operating system, no threads
- Passive components only — no active or queued components anywhere
- Rate-group scheduling driven by the hardware RTC, CPU asleep (`wfi`) between cycles
- UART uplink/downlink with DMA in both directions
- Command dispatch and telemetry packetization are **compile-time** tables
- No dynamic memory at all; `operator delete` is trapped in `Main.cpp`
- 128 KiB flash, **16 KiB RAM** — every change has a size budget

Read **`docs/architecture.md`** before touching the topology or the scheduling model. It
is accurate and it explains the parts that are surprising.

## Build System

### F Prime utility tool

`fprime-util` lives in the virtual environment at `venv/bin/fprime-util`.

```bash
source venv/bin/activate

# Generate a build cache. `microchip_curiosity` is the only cross toolchain; it is also
# `default_toolchain` in settings.ini, so the name is optional here.
fprime-util generate microchip_curiosity

# Build the deployment. NOTE: the flag is -p (path), not -d.
fprime-util build -j"$(nproc)" -p CuriosityReference microchip_curiosity

# Build one component
fprime-util build -p CuriosityReference/GpioIn microchip_curiosity

# Unit tests (native, no cross toolchain needed).
# NOTE: `generate --ut`, NOT `generate native-ut` -- `native-ut` is the name of the
# build cache and of the CMake preset, not of a toolchain. There is no
# cmake/toolchain/native-ut.cmake anywhere, so `generate native-ut` fails.
fprime-util generate --ut
fprime-util check -j"$(nproc)" -p CuriosityReference/I2cTester
fprime-util check                # from a component directory
fprime-util check --coverage     # with gcovr output under coverage/

# Generate unit-test scaffolding
fprime-util impl --ut

# Format changed code
git diff --name-only origin/main...HEAD | fprime-util format --stdin

# Check formatting the way CI does
fprime-util format --check --dirs CuriosityReference config-samd-reference
```

### CMake presets

**Generate the cache with `fprime-util`, not with `cmake --preset`.** `fprime-util` is
what passes `settings.ini`'s `library_locations` (without which the `Samd21` platform and
the `fprime-samd` modules are not found) and `default_cmake_options` to CMake; a bare
`cmake --preset ...` does not read `settings.ini`. Each preset's `binaryDir` matches the
cache `fprime-util` produces, so build and test steps work against it either way:

```bash
fprime-util generate microchip_curiosity
cmake --build --preset microchip_curiosity

fprime-util generate --ut
ctest --preset native-ut
```

All three preset kinds exist in `CMakePresets.json` (`configurePresets`, `buildPresets`,
`testPresets`), so `ctest --preset native-ut` really works — unlike in the upstream
project this was ported from, which documented that command with no `testPresets` block
behind it. If you add a preset, add all three kinds and keep `binaryDir` aligned with
`fprime-util`'s cache naming.

### The cross toolchain is found by absolute path

`lib/fprime-samd/cmake/toolchain/samd21-bare-toolchain.cmake` searches for
`arm-none-eabi-gcc`, CMSIS and CMSIS-Atmel under
`$HOME/.arduino15/packages/adafruit/tools`, with the versions `9-2019q4`, `5.4.0` and
`1.2.2` written in as literal path components. `/usr/bin` is **not** on the search list and
the CMSIS block has no non-Arduino fallback — it raises `FATAL_ERROR` at configure time.

So: an apt-installed `gcc-arm-none-eabi` will not be used, and would not supply `sam.h`
anyway. Install with
`arduino-cli core install adafruit:samd@1.7.17` (see the README), or work in the
devcontainer. Do not "fix" a configure failure by editing the toolchain file in the
submodule.

### Flashing

```bash
python3 scripts/jlink_flash.py CuriosityReference
```

That is the only flash script; there is no `scripts/flash.py`. It requires SEGGER's
`JLinkExe` and a J-Link probe on the SWD pads. `scripts/flash.jlink` is the command
template it renders.

## Architecture

### Directory layout

- **`CuriosityReference/`** — the deployment
  - `Main.cpp` — `setupTopology()`, then `cycler.cycle()` + `wfi` forever
  - `Top/` — `instances.fpp`, `topology.fpp`, `TopTopology.cpp`, `TopTopologyDefs.hpp`
  - `FrameworkHealth/` — template component: periodic work on a rate group
  - `GpioOut/` — template component: a command that touches hardware
  - `GpioIn/` — template component: a hardware interrupt reaching a component
  - `I2cTester/` — template component: a bus transaction state machine
- **`config-samd-reference/`** — F Prime configuration overrides
- **`docs/`** — `architecture.md`, `Component-Checklist.md`
- **`scripts/`** — J-Link flashing
- **`lib/`** — git submodules: `fprime`, `fprime-baremetal`, `fprime-samd`
- **`.github/`** — workflows (`build.yml` cross build + ELF measurement, `size-report.yml`
  posts the PR comment, `unit-tests.yml` native tests, `format-check.yml`), plus
  `scripts/size_report.py`

**The configuration directory must never be renamed to `config/`.** The repository root is
on the include path ahead of the build cache's override copies, and framework code includes
configuration through a `config/` prefix — so a repo-root `config/` shadows the overrides
and silently bypasses `CONFIGURATION_OVERRIDES`, building against F Prime defaults while
appearing to work. The warning is repeated in
`config-samd-reference/CMakeLists.txt`.

### Execution model

1. `Samd21.RtcDriver` interrupts at 8 Hz off the internal ultra-low-power 32 kHz
   oscillator, waking the core from `wfi`.
2. `Svc.PassiveCycler.cycle()` gives a turn to each driver with deferred work
   (`comDriver`, `rateDriver`, `i2cDriver`) and repeats until nobody reports work.
3. `rateDriver`'s turn emits the tick. `Samd21.PassiveRateGroupDriver` divides it by
   `{1, 8, 80}` (set in `TopTopology.cpp`) into three `Svc.PassiveRateGroup` instances:

| Group | Rate | Members, in call order |
| --- | --- | --- |
| `rg8Hz` | 8 Hz | `comDriver.schedIn` (extracts partial DMA RX — the uplink latency floor) |
| `rg1Hz` | 1 Hz | `i2cDriver.reportTelemetryIn`, `i2cTester.schedIn`, `framer.schedIn` |
| `rg10s` | 0.1 Hz | `fwHealth.schedIn` |

Rate-group members run nested on one stack, in output-port index order, to completion.
`framer.schedIn` is deliberately on the last index
(`[PassiveRateGroupOutputPorts - 1]`) so the flush happens after every other member has
written its telemetry. Give new rate-group connections an explicit index.

Interrupt context is the only concurrency. In this deployment, code runs in ISR context in:
`GpioIn.transitionIn` (EIC), and the drivers' `dmaReplyIn` ports (DMAC). Use
`fprime-samd`'s `CriticalSection` when sharing state with an ISR.

### Communication

Downlink: `downlink` (events, FATAL) and `tlm` (telemetry packets) →
`framer.comPacketQueueIn` → accumulated in two 128 B buffers → flushed at 1 Hz →
`comDriver.$send` → DMA channel 0.

Uplink: DMA channel 1 fills 256 B buffers circularly → `comDriver.schedIn` publishes
partial data at 8 Hz → `comStub` → `frameAccumulator` → `deframer` → `fprimeRouter` →
`cmdDisp`.

FATAL bypasses all of it: `fatalFramer.drvSendOut -> comDriver.sendSync` blocks until the
bytes are out, then `fatalHandler` resets the MCU.

**Telemetry is ground-polled in this deployment.** `tlm.pktSendIn` is not connected to
anything, so packets only leave on the `SEND_PKT(id)` command. Channel writes accumulate in
the static packet buffers continuously.

### Autocoded tables

Two `fprime-samd` build autocoders are registered in the root `CMakeLists.txt` between the
`FPrime.cmake` include and `fprime_setup_included_code()`:

- `static_tlm_packet` — turns the topology's `telemetry packets` block into the packet
  storage `Samd21.StaticTlmPacketizer` uses
- `static_cmd_dispatch` — turns the topology's command connections into the opcode table
  `Samd21.StaticCmdDispatcher` uses

Because dispatch is a compile-time table, `TopTopology.cpp` does **not** call
`regCommands()`. Do not "fix" that.

### Configuration phases

Instances configure in phases declared in `instances.fpp`:

1. `configObjects` — construct configuration objects (e.g. the frame detector)
2. `initComponents` — component state init
3. `configComponents` — pin mux and driver configuration
4. `startTasks` — enable interrupts, start the RTC, configure GPIO pins
5. `tearDownComponents` — cleanup

## Hardware configuration

UART (`comDriver`, from `instances.fpp`):

- SERCOM0 on PA08 (TX) / PA09 (RX)
- 115200 baud, 8N1
- `DataOrder::LSB_FIRST` — standard UART bit order. (Documentation elsewhere in this
  ecosystem has claimed MSB-first for this configuration. The code says `LSB_FIRST`;
  believe the code.)
- 256-byte DMA RX buffers, circular

I2C (`i2cDriver`):

- SERCOM4 on PA12 (SDA) / PA13 (SCL), 400 kHz fast mode
- All four SMBus timeouts `DISABLED`

GPIO: PA23 input (pull-up, EIC on both edges), PA25 output.

DMA channels (`enum DmaChannel` in `topology.fpp`):

- 0: USART TX
- 1: USART RX (circular)
- 2: SERCOM4 I2C write
- 3: SERCOM4 I2C read

### Datasheets

Datasheets are **not** checked into this repository — they are vendor copyright. Fetch what
you need from Microchip:

- SAM D21 / DA1 Family Data Sheet (DS40001882)
- SAM D21 Curiosity Nano hardware user guide

For register-level questions about DMA, SERCOM, RTC, EIC, PORT or the clock tree, get the
family data sheet rather than guessing; the peripheral behaviour on this part has enough
corners that guessing produces plausible, wrong code.

## Important constraints

These come from the target and are not negotiable:

1. **No dynamic memory.** `operator delete` asserts in `Main.cpp`; nothing backs `new`.
   Buffers come from `Svc.StaticMemory` or `Samd21::StaticMallocator`, both of which are
   fixed arrays behind an allocator interface.
2. **No active or queued components**, so no `async` input ports. Everything is `sync`.
3. **No RTOS, no threads.**
4. **16 KiB RAM total**, shared between statics and the stack. Stack depth is not measured
   anywhere and rate-group members nest on one stack.
5. **No C++ exceptions, no RTTI** — disabled by the toolchain.
6. **C++14.**
7. **`restrict_platforms(Samd21)`** in `CuriosityReference/CMakeLists.txt` means the
   deployment itself does not build under `native-ut`; only component unit tests do.

## Gotchas

- **`FW_ASSERT` with a pointer** must cast through `PlatformPointerCastType` /
  `FwAssertArgType`, or the native-ut (64-bit) build fails to compile even though the
  32-bit cross build is fine. See the casts in `Main.cpp`.
- **Packet-set completeness.** Every telemetry channel of every instance in the topology
  must be in a packet or in the `omit { ... }` block. Adding a channel and forgetting the
  topology is a build error — which is the good outcome, but the error message is not
  obvious.
- **Packet ids are positional.** Inserting a packet into `telemetry packets Main`
  renumbers the ones after it and invalidates any `SEND_PKT` procedures.
- **`Samd21.NUM_TLM_PACKETS` defaults to 1.** It must exceed the largest *port-driven*
  packet id. Nothing here is port-driven, so 1 works by accident. Wiring `tlm.pktSendIn`
  for any packet past id 0 requires overriding
  `Samd21StaticTlmPacketizerConfig.fpp` in `config-samd-reference/`.
- **`ninja all` is not the build.** The root `CMakeLists.txt` marks several framework and
  library modules `EXCLUDE_FROM_ALL` with a comment each explaining why (mostly: they do
  not compile against this project's trimmed config, or they are broken at the pinned
  library commit). Build with `fprime-util build -p CuriosityReference`. Do not add an
  exclusion without a comment naming the specific symbol or header that fails.
- **`lib/` is submodules.** Do not edit code under `lib/` to fix a build here; fix it
  upstream or work around it in this repository with a comment saying which upstream
  change removes the need.
- **Two submodule pins are temporary workarounds** (a fork of `nasa/fprime`, and an
  unmerged `fprime-samd` branch). The README explains both. Do not silently repin.
- **`CuriosityReference/Top/CMakeLists.txt` carries a `DEPENDS
  Svc_TlmPacketizer_config_TlmPacketizerConfig` workaround** for a fix not yet present in
  the pinned `fprime-samd`. Its comment says when it can be removed.

## Skills

`.claude/skills/` carries four skills, all of which are general F Prime practice rather
than project-specific:

- `fprime-cpp-design` — the C/C++ design rules; read it before writing component code
- `fprime-unit-testing` — the `Tester` / `TestMain` / `GTestBase` pattern and
  `register_fprime_ut`
- `fprime-design-review` — reviewing a change for design fit
- `component-checklist` — evaluate a component against `docs/Component-Checklist.md`

## Working in this repository

- Prefer clarity to cleverness; the code is the documentation for someone else's project.
- Comment *why*, with the specific symbol, register, or upstream issue. The existing code
  is dense with that kind of comment — match it.
- When you change behaviour described in `docs/architecture.md` or the README, change the
  prose in the same commit.
- Run `fprime-util format` on what you touched before committing.
- Watch the flash/RAM number. A size regression with no justification is a defect here.
