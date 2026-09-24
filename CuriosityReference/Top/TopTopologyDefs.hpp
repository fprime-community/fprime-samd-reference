// ======================================================================
// \title  TopTopologyDefs.hpp
// \brief required header file containing the required definitions for the topology autocoder
//
// ======================================================================
#ifndef CURIOSITY_REFERENCE_TOP_TOPOLOGY_DEFS_HPP
#define CURIOSITY_REFERENCE_TOP_TOPOLOGY_DEFS_HPP

#include <Fw/FPrimeBasicTypes.hpp>
#include <Svc/FrameAccumulator/FrameDetector/FprimeFrameDetector.hpp>
#include <fprime-samd/Drv/Types/PinMux.hpp>

#include "fprime-samd/Svc/StaticMallocator/StaticMallocator.hpp"

/**
 * \brief required ping constants
 *
 * The topology autocoder requires a WARN and FATAL constant definition for each component that supports the health-ping
 * interface. These are expressed as enum constants placed in a namespace named for the component instance. These
 * are all placed in the PingEntries namespace.
 *
 * Each constant specifies how many missed pings are allowed before a WARNING_HI/FATAL event is triggered. In the
 * following example, the health component will emit a WARNING_HI event if the component instance cmdDisp does not
 * respond for 3 pings and will FATAL if responses are not received after a total of 5 pings.
 *
 * ```c++
 * namespace PingEntries {
 * namespace cmdDisp {
 *     enum { WARN = 3, FATAL = 5 };
 * }
 * }
 * ```
 *
 * This deployment declares none: health pings exist to detect a hung active component, and every instance here is
 * passive. There is no Svc::Health instance either.
 */

// Definitions are placed within a namespace named after the deployment
namespace CuriosityReference {

/**
 * \brief required type definition to carry state
 *
 * The topology autocoder requires an object that carries state with the name
 * `CuriosityReference::TopologyState`. Only the type definition is required by the autocoder and the contents of this
 * object are otherwise opaque to the autocoder. The contents are entirely up to the definition of the project.
 *
 * A hosted F´ deployment typically puts command line arguments here (hostname, port, dictionary path). This is a
 * bare-metal target started by the reset vector: there is no command line and nothing to shuttle, so the struct is
 * empty. It is still required to exist.
 */
struct TopologyState {};

namespace Allocation {
/**
 * \brief frame accumulator allocation
 *
 * Svc::FrameAccumulator takes an Fw::MemAllocator to obtain its ring buffer. There is no heap on this target, so the
 * allocator is a Samd21::StaticMallocator: a compile-time arena of 1 allocation of 128 bytes, which is the frame buffer
 * size passed to frameAccumulator.configure() in instances.fpp. The two numbers must agree.
 */
extern Samd21::StaticMallocator<128, 1> frameAccumulatorAllocator;
}  // namespace Allocation

}  // namespace CuriosityReference

#endif
