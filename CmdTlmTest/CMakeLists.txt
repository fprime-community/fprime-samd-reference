#####
# 'CmdTlmTest' Deployment:
#
# gtest-driven test topology exercising the commanding and telemetry
# infrastructure this project's CuriosityReference deployment ships.
#####

restrict_platforms(native)

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Target/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/ComSink/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/EventSink/")

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top/")
