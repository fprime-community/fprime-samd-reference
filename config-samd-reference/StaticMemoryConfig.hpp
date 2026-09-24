/*
 * StaticMemoryCfg.hpp:
 *
 * Configuration settings for the static memory component.
 */

#ifndef SVC_STATIC_MEMORY_CFG_HPP_
#define SVC_STATIC_MEMORY_CFG_HPP_

// Svc::StaticMemory is how a no-dynamic-memory deployment satisfies the framework's
// Fw::BufferGet / Fw::BufferSend allocation ports: it hands out slices of a
// compile-time-sized member array instead of calling malloc. The array is
// STATIC_MEMORY_ALLOCATION_SIZE bytes per allocation, times StaticMemoryAllocations
// (config-samd-reference/AcConstants.fpp, = 1 here), so this constant is a direct,
// literal RAM cost: 128 bytes.
namespace Svc {
enum StaticMemoryConfig { STATIC_MEMORY_ALLOCATION_SIZE = 128 };
}

#endif
