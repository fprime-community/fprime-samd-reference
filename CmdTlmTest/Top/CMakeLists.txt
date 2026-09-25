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

# This directory builds two targets instead of one: the topology itself
# (register_fprime_module) and the gtest binary that drives it
# (register_fprime_ut). There is no register_fprime_deployment/Main.cpp here
# -- unlike CuriosityReference, this "deployment" only exists to be driven by
# gtest. The SOURCE_FILES/MOD_DEPS + UT_SOURCE_FILES/UT_MOD_DEPS variable
# style (rather than this repo's usual named SOURCES/DEPENDS arguments) is
# copied from lib/fprime/FppTestProject/FppTest/topology/main/CMakeLists.txt,
# the only known-working example of register_fprime_ut() on a *topology*
# rather than a single component.

set(SOURCE_FILES
    "${CMAKE_CURRENT_LIST_DIR}/instances.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/topology.fpp"
    "${CMAKE_CURRENT_LIST_DIR}/TopTopology.cpp"
)

set(MOD_DEPS
    CmdTlmTest_Target
    CmdTlmTest_ComSink
    CmdTlmTest_EventSink
)

register_fprime_module()
unset(SOURCE_FILES)
unset(MOD_DEPS)

### Unit Tests ###

set(UT_MOD_DEPS STest)

set(UT_SOURCE_FILES
    "${CMAKE_CURRENT_LIST_DIR}/test/ut/FrameBuilder.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/test/ut/CommandRoutingTests.cpp"
)

register_fprime_ut()

# Same reasoning as CuriosityReference/CMakeLists.txt's WHOLE_ARCHIVE override on
# libCuriosityReference_Top.a: the static_tlm_packet/static_cmd_dispatch build autocoders
# emit Samd21::StaticCmdDispatcher::lookupDispatchPort / Samd21::StaticTlmPacketizer::
# {writePoint,loadPacket} into libCmdTlmTest_Top.a, which fprime-samd's own
# StaticCmdDispatcher.cpp/StaticTlmPacketizer.cpp objects (linked after it) depend on. A
# normal archive is only searched for symbols already undefined when the linker reaches
# it, so without this override those members are never extracted and the gtest binary
# fails to link.
#
# CmdTlmTest_Top_ut_exe's LINK_LIBRARIES ends up with libCmdTlmTest_Top.a listed twice --
# ut.cmake's ut_executable_build() links the "current module" in explicitly on top of
# whatever register_fprime_ut() already resolved -- and WHOLE_ARCHIVE on a duplicated
# entry means every symbol in it gets defined twice, i.e. real "multiple definition"
# link errors. Deduplicating is deferred to the end of the whole configure (rather than
# done right here) because that duplicate entry is added by custom-target setup that
# runs after this file finishes -- fixing it here would just be undone afterward.
function(_cmd_tlm_test_finalize_ut_link)
    if (TARGET CmdTlmTest_Top_ut_exe)
        get_target_property(_CMD_TLM_TEST_TOP_UT_LIBS CmdTlmTest_Top_ut_exe LINK_LIBRARIES)
        if (_CMD_TLM_TEST_TOP_UT_LIBS)
            list(REMOVE_DUPLICATES _CMD_TLM_TEST_TOP_UT_LIBS)
            set_target_properties(CmdTlmTest_Top_ut_exe PROPERTIES LINK_LIBRARIES "${_CMD_TLM_TEST_TOP_UT_LIBS}")
        endif()
        set_property(TARGET CmdTlmTest_Top_ut_exe
            PROPERTY "LINK_LIBRARY_OVERRIDE_CmdTlmTest_Top" WHOLE_ARCHIVE)
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL _cmd_tlm_test_finalize_ut_link)
