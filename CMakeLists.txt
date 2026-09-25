####
# This sets up the build system for the 'FprimeSamdReference' project, including
# components and deployments. In addition, it imports the core F Prime components.
####

cmake_minimum_required(VERSION 3.24.2)
project(FprimeSamdReference C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

###
# F' Core Setup
# This includes all of the F prime core components, and imports the make-system.
###
include("${CMAKE_CURRENT_LIST_DIR}/lib/fprime/cmake/FPrime.cmake")
register_fprime_build_autocoder("autocoder/static_tlm_packet" OFF)
register_fprime_build_autocoder("autocoder/static_cmd_dispatch" OFF)
fprime_setup_included_code()

set(SAMD_REFERENCE_EXCLUDED_MODULES
    # Reference CCSDS fields/constants that config-samd-reference/ComCfg.fpp does not
    # declare: ComCfg::FrameContext::{get,set}_firstHeaderPointer / _sequenceFlags,
    # ComCfg::Pvn::{SPACE_PACKET,ENCAPSULATION_PACKET}_PROTOCOL, and the
    # ComCfg::Apid::SPP_IDLE_PACKET == 0x07FF static_assert. Downlink in this project is
    # Samd21::Framer + Svc::FprimeProtocol, not CCSDS space packets.
    Svc_Ccsds_ApidManager
    Svc_Ccsds_AosDeframer
    Svc_Ccsds_SpacePacketDeframer
    Svc_Ccsds_SpacePacketFramer
    Svc_Ccsds_TmFramer
    Svc_ComAggregator

    # static_assert that the component "requires serialization", which
    # config-samd-reference/FpConfig.h compiles out (FW_PORT_SERIALIZATION = 0) to fit in
    # 16 KiB of RAM. Svc_WasmSequencer additionally cargo-builds a host-architecture Rust
    # staticlib, which is pure waste in a cross build.
    Svc_FpySequencer
    Svc_WasmSequencer

    # Os::Baremetal::MicroFs does not compile at the pinned fprime-baremetal commit: its
    # implementation predates the Os::FileSystem/Os::Directory interface split in
    # lib/fprime and no longer satisfies it. Nothing here uses a filesystem.
    Os_Baremetal_MicroFs
    Os_File_Baremetal_MicroFs_Implementation

    # Duplicates Samd21::StaticCmdDispatcher, which this deployment uses instead (dispatch
    # is a compile-time table here -- see docs/architecture.md). Excluded to keep `all`
    # from building a second, unused dispatcher.
    fprime-baremetal_Svc_PassiveCmdDispatcher
)

if (BUILD_TESTING)
    list(APPEND SAMD_REFERENCE_EXCLUDED_MODULES
        # Unit tests of the MicroFs modules excluded above; they cannot link without them.
        MicroFsFullTest
        MicroFsFileTest
    )
endif()

foreach (SAMD_REFERENCE_EXCLUDED_MODULE IN LISTS SAMD_REFERENCE_EXCLUDED_MODULES)
    if (TARGET "${SAMD_REFERENCE_EXCLUDED_MODULE}")
        set_target_properties("${SAMD_REFERENCE_EXCLUDED_MODULE}" PROPERTIES EXCLUDE_FROM_ALL TRUE)
    else()
        message(STATUS "[SamdReference] EXCLUDE_FROM_ALL skipped, no such target: ${SAMD_REFERENCE_EXCLUDED_MODULE}")
    endif()
endforeach()

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/config-samd-reference")

# === Deployments === #
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/CuriosityReference/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/CmdTlmTest/")
# === /Deployments === #
