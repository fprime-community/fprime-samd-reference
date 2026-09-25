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

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ComSink.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ComSink.cpp"
    DEPENDS
        # Provides the "samd-config/" include path ComSink.hpp needs for
        # FramerConfig.hpp -- see lib/fprime-samd/fprime-samd/Svc/Framer/CMakeLists.txt,
        # which depends on the same target for the same reason.
        config-fprime-samd
)
