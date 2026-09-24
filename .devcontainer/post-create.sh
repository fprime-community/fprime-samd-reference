#!/bin/bash
#
# Runs once, after the workspace has been mounted into the container.
#
# The Python install lives here rather than in the Dockerfile because
# requirements.txt references ./lib/fprime/requirements.txt and
# ./lib/fprime-samd/requirements.txt, which only exist once the git submodules are
# checked out -- and that happens with the workspace mount, after the image build.
set -e

if [ ! -f lib/fprime/requirements.txt ] || [ ! -f lib/fprime-samd/requirements.txt ]; then
    echo "Submodules are not checked out; fetching them now."
    git submodule update --init --recursive
fi

echo "Installing Python dependencies..."
pip3 install --break-system-packages --ignore-installed -r requirements.txt

echo ""
echo "Development environment ready!"
echo ""
echo "Installed tools:"
echo "  - Python:         $(python3 --version)"
echo "  - CMake:          $(cmake --version | head -1)"
echo "  - ARM GCC:        $("${HOME}"/.arduino15/packages/adafruit/tools/arm-none-eabi-gcc/9-2019q4/bin/arm-none-eabi-gcc --version | head -1)"
echo ""
echo "F Prime packages:"
pip3 list | grep fprime | sed 's/^/  - /'
echo ""
echo "To build the project:"
echo "  fprime-util generate microchip_curiosity"
echo "  fprime-util build -p CuriosityReference microchip_curiosity"
