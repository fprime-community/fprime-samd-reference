FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

USER root

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV PATH="/opt/arduino-cli/bin:/root/.local/bin:/usr/local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

# Persist bash history.
RUN SNIPPET="export PROMPT_COMMAND='history -a' && export HISTFILE=/commandhistory/.bash_history" \
  && mkdir /commandhistory \
  && touch /commandhistory/.bash_history \
  && chown -R root /commandhistory

# Set `DEVCONTAINER` environment variable to help with orientation
ENV DEVCONTAINER=true
ENV IS_SANDBOX=1

# Install base dependencies. cmake and ninja deliberately come from pip (via
# lib/fprime/requirements.txt, which pins them), not from apt.
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    curl \
    wget \
    python3 \
    python3-pip \
    python3-venv \
    clang \
    clang-tools \
    llvm \
    lld \
    gdb-multiarch \
    openocd \
    socat \
    libusb-1.0-0 \
    libusb-1.0-0-dev \
    ca-certificates \
    vim \
    && rm -rf /var/lib/apt/lists/*

# Set the default editor and visual
ENV EDITOR=vim
ENV VISUAL=vim

# Install arduino-cli.
#
# arduino-cli is used ONLY as a downloader for the cross toolchain. The SAMD21
# toolchain file (lib/fprime-samd/cmake/toolchain/samd21-bare-toolchain.cmake) locates
# arm-none-eabi-gcc, CMSIS and CMSIS-Atmel by absolute path underneath
# ~/.arduino15/packages/adafruit/tools, with hardcoded versions and no fallback to
# /usr/bin. `apt install gcc-arm-none-eabi` therefore does NOT work: apt installs the
# compiler somewhere the probe never looks, and it supplies no CMSIS-Atmel device
# headers at all (sam.h / samd.h come from there). The Arduino *hardware core* that
# comes along for the ride is not used -- the Curiosity Nano linker script and variant
# directory live in fprime-samd -- but arduino-cli has no tools-only mode.
RUN mkdir -p /opt/arduino-cli/bin \
    && curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/opt/arduino-cli/bin sh

# Install the pinned adafruit:samd core. Do not float this to latest: the toolchain file
# hardcodes arm-none-eabi-gcc 9-2019q4, CMSIS 5.4.0 and CMSIS-Atmel 1.2.2 as literal
# path components, which are the tool versions this core version depends on.
ENV ADAFRUIT_SAMD_VERSION=1.7.17
ENV ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS=https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
RUN arduino-cli core update-index \
    && arduino-cli core install "adafruit:samd@${ADAFRUIT_SAMD_VERSION}"

# Python dependencies are installed by post-create.sh rather than here.
#
# requirements.txt fans out to ./lib/fprime/requirements.txt and
# ./lib/fprime-samd/requirements.txt, so it cannot be resolved until the git submodules
# are checked out -- which happens when the workspace is mounted, i.e. after the image
# is built. Installing at post-create time also means a submodule bump does not require
# an image rebuild.

# Set the default shell
SHELL ["/bin/bash", "-c"]
