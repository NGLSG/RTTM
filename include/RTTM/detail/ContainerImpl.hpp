/**
 * @file ContainerImpl.hpp
 * @brief Implementation details for container reflection
 * 
 * This file provides the implementation of RType creation helpers
 * for container elements. It must be included after RType.hpp.
 */

#ifndef RTTM_DETAIL_CONTAINER_IMPL_HPP
#define RTTM_DETAIL_CONTAINER_IMPL_HPP

#include "Container.hpp"
#include "RType.hpp"
#include "TypeManager.hpp"

namespace rttm {
namespace detail {

/**
 * @brief Create an RType wrapper for a value at a given pointer
 * 
 * This creates a non-owning RType that wraps an existing value.
 * The lifetime of the RType is tied to the lifetime of the original value.
 * 
 * @tparam T The value type
 * @param ptr Pointer to the value
 * @return Shared pointer to RType wrapping the value
 */
template<typename T>
std::shared_ptr<RType> create_value_rtype(T* ptr) {
    auto rtype = std::make_shared<RType>();
    
    // Try to get type info for registered types
    auto& mgr = TypeManager::instance();
    std::string type_name_str{type_name<T>()};
    const TypeInfo* info = mgr.get_type(type_name_str);
    
    // Create non-owning shared_ptr to the value
    auto non_owning = std::shared_ptr<void>(
        std::shared_ptr<void>{}, // Empty shared_ptr as control block
        ptr                       // Pointer to manage
    );
    
    rtype->attach_raw(info, non_owning);
    return rtype;
}

// ==================== SequentialContainerImpl Implementation ====================

template<SequentialContainer Container>
std::shared_ptr<RType> SequentialContainerImpl<Container>::create_element_rtype(value_type* element) {
    return detail::create_value_rtype(element);
}

// ==================== KeyValueContainerImpl Implementation ====================

template<KeyValueContainer Container>
std::shared_ptr<RType> KeyValueContainerImpl<Container>::create_key_rtype(key_type* key) {
    return detail::create_value_rtype(key);
}

template<KeyValueContainer Container>
std::shared_ptr<RType> KeyValueContainerImpl<Container>::create_value_rtype(mapped_type* value) {
    return detail::create_value_rtype(value);
}

// ==================== SetContainerImpl Implementation ====================

template<AssociativeContainer Container>
requires (!KeyValueContainer<Container>)
std::shared_ptr<RType> SetContainerImpl<Container>::create_element_rtype(key_type* element) {
    return detail::create_value_rtype(element);
}

} // namespace detail
} // namespace rttm

#endif // RTTM_DETAIL_CONTAINER_IMPL_HPP
