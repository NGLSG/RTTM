/**
 * @file RTTM.hpp
 * @brief Main entry header for RTTM (Runtime Turbo Mirror) C++20 reflection library
 * 
 * This is the single header include for basic reflection functionality.
 * Include this file to access all public RTTM APIs.
 * 
 * RTTM provides three API layers:
 * 
 * 1. Semi-static (fastest): PropertyHandle<T>, TypedMethodHandle<Sig>
 *    - Compile-time type knowledge, zero overhead
 *    
 * 2. Cached dynamic: MethodHandle, BoundType
 *    - Runtime lookup once, cached for repeated access
 *    
 * 3. Pure dynamic: Instance, Variant
 *    - No compile-time type knowledge needed
 *    - DLL-compatible, works with dynamically loaded types
 */

#ifndef RTTM_RTTM_HPP
#define RTTM_RTTM_HPP

// C++20 standard requirement
#if __cplusplus < 202002L
    #error "RTTM requires C++20 or later"
#endif

// Core type traits and concepts
#include "detail/TypeTraits.hpp"

// Type information and management
#include "detail/TypeInfo.hpp"
#include "detail/TypeManager.hpp"

// Exception hierarchy
#include "detail/Exceptions.hpp"

// Type registration
#include "detail/Registry.hpp"

// Runtime type handle
#include "detail/RType.hpp"

// Lightweight type handle (zero-cost type lookup)
// Note: RTypeHandle.hpp includes BoundType.hpp internally
#include "detail/RTypeHandle.hpp"

// Container reflection support
#include "detail/Container.hpp"
#include "detail/ContainerImpl.hpp"

// Pure dynamic reflection (RTTR-like)
#include "detail/Variant.hpp"
#include "detail/Instance.hpp"

/**
 * @brief Macro for registering types with RTTM
 * 
 * Usage:
 * @code
 * RTTM_REGISTRATION {
 *     rttm::Registry<MyClass>()
 *         .property("name", &MyClass::name)
 *         .method("getValue", &MyClass::getValue);
 * }
 * @endcode
 */
#define RTTM_REGISTRATION \
    static void rttm_register_types_(); \
    namespace { \
        struct RTTMAutoRegister_ { \
            RTTMAutoRegister_() { rttm_register_types_(); } \
        } rttm_auto_register_; \
    } \
    static void rttm_register_types_()

#endif // RTTM_RTTM_HPP
