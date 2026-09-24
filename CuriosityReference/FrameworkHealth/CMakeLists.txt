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
# This directory therefore produces the target `CuriosityReference_FrameworkHealth`,
# and the autocoded header lands at
# `<build>/F-Prime/CuriosityReference/FrameworkHealth/FrameworkHealthComponentAc.hpp`.
# That is why the `#include` in FrameworkHealth.hpp is prefixed with the on-disk
# path: moving this directory requires updating the include prefix to match.

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/FrameworkHealth.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/FrameworkHealth.cpp"
#   DEPENDS
#       MyPackage_MyOtherModule
)

### Unit Tests ###
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/FrameworkHealth.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameworkHealthTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameworkHealthTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )
