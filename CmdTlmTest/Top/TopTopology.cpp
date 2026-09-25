// ======================================================================
// \title  TopTopology.cpp
// \brief cpp file containing the topology instantiation code
// ======================================================================
// Provides access to autocoded functions
#include <CmdTlmTest/Top/TopTopologyAc.hpp>

namespace CmdTlmTest {
namespace Allocation {
Samd21::StaticMallocator<128, 1> frameAccumulatorAllocator;
}  // namespace Allocation
}  // namespace CmdTlmTest
