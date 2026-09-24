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
# This directory therefore produces the target `CuriosityReference_GpioIn`, and
# the autocoded header lands at
# `<build>/F-Prime/CuriosityReference/GpioIn/GpioInComponentAc.hpp`. That is why
# the `#include` in GpioIn.hpp is prefixed with the on-disk path: moving this
# directory requires updating the include prefix to match.
#
# No DEPENDS entry is needed for Drv.GpioRead / Drv.GpioStatus -- framework FPP
# and headers resolve through the global include path.

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/GpioIn.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/GpioIn.cpp"
#   DEPENDS
#       MyPackage_MyOtherModule
)

### Unit Tests ###
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/GpioIn.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/GpioInTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/GpioInTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )
