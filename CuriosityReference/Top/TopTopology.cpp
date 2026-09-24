// ======================================================================
// \title  TopTopology.cpp
// \brief cpp file containing the topology instantiation code
//
// ======================================================================
// Provides access to autocoded functions
#include <CuriosityReference/Top/TopTopologyAc.hpp>

// Necessary project-specified types
#include <config/FppConstantsAc.hpp>
#include "fprime-samd/Svc/StaticMallocator/StaticMallocator.hpp"

// Allows easy reference to objects in FPP/autocoder required namespaces
using namespace CuriosityReference;

namespace CuriosityReference {
namespace Allocation {
Samd21::StaticMallocator<128, 1> frameAccumulatorAllocator;
}  // namespace Allocation
}  // namespace CuriosityReference

// The reference topology divides the incoming 8 Hz clock signal into 8 Hz, 1 Hz and 10 s rate groups
Samd21::PassiveRateGroupDriver::DividerSet rateGroupDivisors{{{1, 0}, {8, 0}, {80, 0}}};

// Rate groups may supply a context token to each of the attached children whose purpose is set by the project. The
// reference topology sets each token to zero as these contexts are unused in this project.
const Svc::PassiveRateGroup::ContextArray rgContext = {};

namespace Samd21 {
// Called by the assert hook (fprime-samd Overrides/reset_assert_hook.cpp) to get a FATAL report off the board before
// the reset. It runs with the system already declared untrustworthy, so it goes straight out of the synchronous framer
// rather than through the queued downlink pipeline.
void sendFatalPacket(Fw::ComBuffer& data) {
    fatalFramer.sendFatalPacket(data);
}

// Optional out-of-band last gasp, used when even the FATAL packet path cannot be trusted. This board has no second
// channel, so it is a no-op: the FATAL packet above carries the report.
void sendBailFrame(FILE_NAME_ARG file, FwSizeType lineNo) {
    (void)file;
    (void)lineNo;
}
}  // namespace Samd21

/**
 * \brief configure/setup components in project-specific way
 *
 * This is a *helper* function which configures/sets up each component requiring project specific input. This includes
 * allocating resources, passing-in arguments, etc. This function may be inlined into the topology setup function if
 * desired, but is extracted here for clarity.
 *
 * Everything that can be expressed as a `phase Fpp.ToCpp.Phases.configComponents` block belongs in instances.fpp
 * instead; what lands here is configuration that needs a file-scope object (the divisor set and the context array
 * below).
 */
void configureTopology(const TopologyState& state) {
    (void)state;  // Nothing configured here depends on the topology state

    // Rate group driver needs a divisor list
    rateGroupDriver.configure(rateGroupDivisors);

    // Rate groups require context arrays.
    rg8Hz.configure(rgContext);
    rg1Hz.configure(rgContext);
    rg10s.configure(rgContext);
}

// Public functions for use in main program are namespaced with deployment name CuriosityReference
namespace CuriosityReference {
void setupTopology(const TopologyState& state) {
    // Autocoded initialization. Function provided by autocoder.
    initComponents(state);
    // Autocoded id setup. Function provided by autocoder.
    setBaseIds();

    // Dma driver must be configured before all the rest of the components that depend on it.
    dmaDriver.configure();

    // Autocoded connection wiring. Function provided by autocoder.
    connectComponents();
    // Autocoded configuration. Function provided by autocoder.
    configComponents(state);
    // Project-specific component configuration. Function provided above. May be inlined, if desired.
    configureTopology(state);

    // Autocoded command registration. Function provided by autocoder.
    // regCommands(); NOT NEEDED! Samd21::StaticCmdDispatcher resolves opcodes against a compile-time table generated
    // by the static_cmd_dispatch build autocoder, so there is nothing to register at runtime.

    // Autocoded parameter loading. Function provided by autocoder.
    // DISABLED FOR BARE-METAL BOARDS. Loading parameters is not supported because there is no file system to load them
    // from, and no component in this deployment declares a parameter.
    // loadParameters();

    // Autocoded task kick-off (active components). Function provided by autocoder. Starts no threads here -- every
    // instance is passive -- but it does run the instances.fpp `startTasks` phases, which arm the RTC and configure the
    // GPIO pins and the I2C tester.
    startTasks(state);
}

void teardownTopology(const TopologyState& state) {
    // Autocoded (active component) task clean-up. Functions provided by topology autocoder.
    stopTasks(state);
    freeThreads(state);
}
};  // namespace CuriosityReference
