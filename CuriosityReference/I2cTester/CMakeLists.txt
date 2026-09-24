####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
#
####

# Module names are derived from the path from the nearest project/library/framework
# root when not specifically overridden by the developer, i.e. the module defined by
# `MyProj/Some/Path/CMakeLists.txt` will be named `MyProj_Some_Path`.
#
# This directory therefore produces the target `CuriosityReference_I2cTester`, and
# the autocoded header lands at
# `<build>/F-Prime/CuriosityReference/I2cTester/I2cTesterComponentAc.hpp`. That is
# why the `#include` in I2cTester.hpp is prefixed with the on-disk path: moving
# this directory requires updating the include prefix to match.
#
# No DEPENDS entry is needed. The I2C port types (Drv.I2cRequest,
# Drv.I2cWriteReadRequest, Drv.I2cCallback, Drv.I2cWriteReadCallback) and
# Drv.I2cStatus are framework definitions in lib/fprime/Drv/Ports and resolve
# through the global FPP and include paths; nothing here depends on the SAMD21
# driver implementation.

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/I2cTester.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/I2cTester.cpp"
#   DEPENDS
#       MyPackage_MyOtherModule
)

### Unit Tests ###
# Uncomment and run `fprime-util impl --ut` in this directory to scaffold the
# harness before enabling this block; the sources below do not exist yet.
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/I2cTester.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/I2cTesterTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/I2cTesterTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )
