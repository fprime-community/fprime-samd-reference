// ======================================================================
// \title  Main.cpp
// \brief main program for the F' application. Uses fprime-samd MCU library
//
// ======================================================================
// Used to access topology functions
#include <CuriosityReference/Top/TopTopology.hpp>
#include <CuriosityReference/Top/TopTopologyAc.hpp>

#include <Fw/Types/Assert.hpp>

// Override operator delete to trap
void operator delete(void* ptr) noexcept {
    FW_ASSERT(false, static_cast<FwAssertArgType>(reinterpret_cast<PlatformPointerCastType>(ptr)));
}

void operator delete(void* ptr, unsigned int size) noexcept {
    (void)size;
    FW_ASSERT(false, static_cast<FwAssertArgType>(reinterpret_cast<PlatformPointerCastType>(ptr)));
}

int main() {
    // Initialize topology
    CuriosityReference::TopologyState inputs;
    CuriosityReference::setupTopology(inputs);

    // Main loop - run the cycler on every interrupt and go to sleep.
    //
    // There is no scheduler and there are no threads. Svc::PassiveCycler is the
    // deployment's entire "run queue": it calls each component wired to its cycleOut
    // port until none of them reports having more work, which is how driver ISRs hand
    // deferred work (DMA completions, I2C transaction completions, rate-group ticks)
    // back to the main context.
    for (;;) {
        // Service every component that is "active"
        CuriosityReference::cycler.cycle();

        // Put the MCU to sleep and wait for another HW interrupt. Any interrupt wakes
        // us and the loop re-runs the cycler, so a wake-up never needs to be signalled.
        asm("wfi");
    }

    return 0;
}
