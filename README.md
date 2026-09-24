# F Prime SAMD21 reference deployment

A small, complete, buildable [F Prime](https://fprime.jpl.nasa.gov) flight-software
deployment for the **[Microchip SAMD21 Curiosity Nano](https://www.microchip.com/en-us/development-tool/dm320119)**
(ATSAMD21G17D, ARM Cortex-M0+, 128 KiB flash, **16 KiB RAM**), running **baremetal**.

This repository builds off of other repositories:

- SAMD21 Drivers [fprime-samd](https://github.com/fprime-community/fprime-samd)
- [fprime-baremetal](https://github.com/fprime-community/fprime-baremetal) OSAL

## Hardware

The SAMD21 Curiosity Nano (Microchip part number `DM320119`) and nothing else. Optional:
an I2C device for `I2cTester` to talk to, and a jumper between the two GPIO pins so
`GpioIn` can see what `GpioOut` drives.

| Function               | Pin(s)                 | Peripheral                                   |
| ---------------------- | ---------------------- | -------------------------------------------- |
| Uplink / downlink UART | PA08 (TX), PA09 (RX)   | SERCOM0, 115200 8N1, LSB first               |
| I2C                    | PA12 (SDA), PA13 (SCL) | SERCOM4, 400 kHz fast mode                   |
| GPIO input             | PA23                   | pull-up, EIC interrupt on both edges         |
| GPIO output            | PA25                   | push-pull                                    |
| Rate-group tick        | —                      | RTC, ultra-low-power 32 kHz oscillator, 8 Hz |

The Curiosity Nano's on-board nEDBG debugger exposes a USB CDC serial port, but that's
separate from the UART pins above — bring your own 3.3 V USB-serial adapter on PA08 /
PA09, or re-pin `comDriver` in `CuriosityReference/Top/instances.fpp`.

## Quickstart

```bash
git clone --recursive https://github.com/fprime-community/fprime-samd-reference.git
cd fprime-samd-reference

python3 -m venv venv
. venv/bin/activate
pip install -r requirements.txt

fprime-util generate microchip_curiosity
fprime-util build -j"$(nproc)" -p CuriosityReference microchip_curiosity
```

The build prints an `arm-none-eabi-size -A` table and leaves artifacts in
`build-artifacts/microchip_curiosity/CuriosityReference/`:

```
bin/CuriosityReference.elf        linked image
bin/CuriosityReference.elf.bin    raw binary, loads at 0x0000
bin/CuriosityReference.elf.hex    Intel HEX
bin/CuriosityReference.map        linker map
bin/CuriosityReference.nm         symbol sizes
bin/CuriosityReference.objdump    disassembly
dict/TopTopologyDictionary.json   the dictionary the ground system reads
```

Always **generate** with `fprime-util` — it's what passes `settings.ini`'s
`library_locations` and `default_cmake_options` to CMake; a bare `cmake --preset ...`
never reads `settings.ini`. Once the cache exists, the presets in `CMakePresets.json`
share `fprime-util`'s `binaryDir`, so you can also drive the build directly:

```bash
cmake --build --preset microchip_curiosity
```

### A faster clone

`--recursive` pulls the complete F Prime history, which is large. Neither the build nor
the tools need it:

```bash
git clone --recursive --shallow-submodules \
    https://github.com/fprime-community/fprime-samd-reference.git
```

or, to keep full history but skip file contents you never check out:

```bash
git clone --recursive --filter=blob:none \
    https://github.com/fprime-community/fprime-samd-reference.git
```

### The cross toolchain

`lib/fprime-samd/cmake/toolchain/samd21-bare-toolchain.cmake` finds `arm-none-eabi-gcc`,
CMSIS and CMSIS-Atmel **by absolute path** under
`~/.arduino15/packages/adafruit/tools`, at pinned versions, with no `/usr/bin` fallback
and no non-Arduino branch for CMSIS. An apt-installed `gcc-arm-none-eabi` won't be found,
and wouldn't supply the SAMD21 device headers (`sam.h`) anyway. Install the toolchain the
way the toolchain file expects:

```bash
arduino-cli core update-index \
    --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
arduino-cli core install adafruit:samd@1.7.17 \
    --additional-urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
```

Don't float that version: `9-2019q4`, `5.4.0` and `1.2.2` are literal path components in
the toolchain file, and a core bump that moves any tool version breaks path resolution
with a `FATAL_ERROR` at configure time.

Or use the [devcontainer](.devcontainer/README.md), which does all of this for you.

## Flashing

With a J-Link probe on the SWD pads:

```bash
python3 scripts/jlink_flash.py CuriosityReference
```

The script discovers deployments from the root `CMakeLists.txt`, reads
`default_toolchain` from `settings.ini`, selects `ATSAMD21G17A` (JLinkExe has no `D`
variant; the geometry is identical), and loads the `.bin` at `0x0000` — there's no
bootloader on this board. `scripts/flash.jlink` is the command template it renders.

The on-board nEDBG debugger speaks CMSIS-DAP rather than J-Link's protocol. To flash
through it instead, point `pymcuprog` or OpenOCD at the same `.bin`.

## Running the ground system

```bash
fprime-gds -n \
    --dictionary build-artifacts/microchip_curiosity/CuriosityReference/dict/TopTopologyDictionary.json \
    --packet-set-name Main \
    --communication-selection uart \
    --uart-device /dev/ttyUSB0 \
    --uart-baud 115200
```

`-n` because the binary runs on the board, not the host. `--packet-set-name Main`
selects the packet set the topology declares (`telemetry packets Main`); telemetry here
is **packetized**, not per-channel, so without it the GDS can't decode the stream.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Briefly: build before you push, run
`fprime-util format` over your changes, and watch the flash/RAM delta CI posts on your
pull request — this repository's whole claim is that F Prime fits in 128 KiB.

## License

Apache-2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE). The submodules under `lib/` keep
their own licenses and are not redistributed here.
