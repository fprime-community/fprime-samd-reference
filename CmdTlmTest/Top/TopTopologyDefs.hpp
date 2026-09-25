// ======================================================================
// \title  TopTopologyDefs.hpp
// \brief required header file containing the required definitions for the topology autocoder
//
// Test-only topology. See CuriosityReference/Top/TopTopologyDefs.hpp for the
// production original this mirrors.
// ======================================================================
#ifndef CMD_TLM_TEST_TOP_TOPOLOGY_DEFS_HPP
#define CMD_TLM_TEST_TOP_TOPOLOGY_DEFS_HPP

#include <Fw/FPrimeBasicTypes.hpp>
#include <Svc/FrameAccumulator/FrameDetector/FprimeFrameDetector.hpp>

#include "fprime-samd/Svc/StaticMallocator/StaticMallocator.hpp"

// Definitions are placed within a namespace named after the deployment
namespace CmdTlmTest {

/**
 * \brief required type definition to carry state
 *
 * The topology autocoder requires an object named `CmdTlmTest::TopologyState`. This test
 * topology has nothing to carry -- gtest drives it directly, there is no command line --
 * so, as in CuriosityReference, the struct is empty but still required to exist.
 */
struct TopologyState {};

namespace Allocation {
/**
 * \brief frame accumulator allocation
 *
 * Same reasoning as CuriosityReference::Allocation::frameAccumulatorAllocator: no heap on
 * this target's real deployments, and this test topology exercises the same
 * Samd21::StaticMallocator-backed configuration rather than a host allocator, so behavior
 * here matches the flight topology. The size must agree with the frame buffer size passed
 * to frameAccumulator.configure() in instances.fpp.
 */
extern Samd21::StaticMallocator<128, 1> frameAccumulatorAllocator;
}  // namespace Allocation

}  // namespace CmdTlmTest

#endif
