/**
 * @file RTypeHandle.hpp
 * @brief Lightweight type handle for zero-cost type lookup
 * 
 * RTypeHandle is a lightweight value type (just a pointer) that provides
 * near-zero-cost type lookup, similar to RTTR's type handle design.
 * 
 * Key differences from RType:
 * - RTypeHandle: lightweight value (pointer), no refcount, for type queries
 * - RType: full-featured, manages instance lifetime, has caches
 * 
 * Use RTypeHandle when you need fast type lookup without instance management.
 * Use RType when you need to create/attach objects and access properties.
 */

#ifndef RTTM_DETAIL_RTYPE_HANDLE_HPP
#define RTTM_DETAIL_RTYPE_HANDLE_HPP

#include "TypeInfo.hpp"
#include "TypeManager.hpp"
#include "TypeTraits.hpp"
#include "Exceptions.hpp"
#include "PropertyHandle.hpp"

#include <string_view>
#include <typeindex>

namespace rttm {

// Forward declarations
class RType;
class BoundType;

/**
 * @brief Lightweight type handle - just a pointer, zero overhead
 * 
 * RTypeHandle provides fast type lookup without any allocation or refcount.
 * It's designed for hot paths where you need type information but don't
 * need instance management.
 * 
 * Usage:
 * @code
 * // Fast type lookup (no allocation)
 * auto handle = RTypeHandle::get<MyClass>();  // ~0.6ns like RTTR
 * 
 * // Query type info
 * std::cout << handle.name() << std::endl;
 * 
 * // Bind to object for property access
 * MyClass obj;
 * auto bound = handle.bind(obj);  // Returns BoundType
 * int& val = bound.property<int>("value");
 * @endcode
 */
class RTypeHandle {
public:
    /**
     * @brief Get type handle by template type (fastest path)
     * 
     * Uses static initialization - effectively zero cost after first call.
     * 
     * @tparam T The type to get handle for
     * @return RTypeHandle (lightweight value)
     */
    template<typename T>
    [[nodiscard]] static RTypeHandle get() noexcept {
        // Static cache per type - initialized once, thread-safe
        static const detail::TypeInfo* cached_info = []() {
            auto& mgr = detail::TypeManager::instance();
            const detail::TypeInfo* info = mgr.get_type_by_id(detail::type_id<T>);
            if (!info) {
                info = mgr.get_type(detail::type_name<T>());
            }
            return info;
        }();
        return RTypeHandle{cached_info};
    }
    
    /**
     * @brief Get type handle by name (string lookup)
     * 
     * @param type_name The registered type name
     * @return RTypeHandle (may be invalid if type not found)
     */
    [[nodiscard]] static RTypeHandle get(std::string_view type_name) noexcept {
        auto& mgr = detail::TypeManager::instance();
        return RTypeHandle{mgr.get_type(type_name)};
    }
    
    /**
     * @brief Get type handle by name, throwing if not found
     * 
     * @param type_name The registered type name
     * @return RTypeHandle
     * @throws TypeNotRegisteredError if type not found
     */
    [[nodiscard]] static RTypeHandle get_or_throw(std::string_view type_name) {
        auto& mgr = detail::TypeManager::instance();
        const detail::TypeInfo* info = mgr.get_type(type_name);
        if (!info) [[unlikely]] {
            throw TypeNotRegisteredError(type_name);
        }
        return RTypeHandle{info};
    }
    
    // Default constructor creates invalid handle
    constexpr RTypeHandle() noexcept : info_(nullptr) {}
    
    // Explicit construction from TypeInfo pointer
    explicit constexpr RTypeHandle(const detail::TypeInfo* info) noexcept : info_(info) {}
    
    // Copy/move are trivial (just pointer copy)
    RTypeHandle(const RTypeHandle&) noexcept = default;
    RTypeHandle& operator=(const RTypeHandle&) noexcept = default;
    RTypeHandle(RTypeHandle&&) noexcept = default;
    RTypeHandle& operator=(RTypeHandle&&) noexcept = default;
    
    /**
     * @brief Check if handle is valid
     */
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return info_ != nullptr;
    }
    
    /**
     * @brief Explicit bool conversion
     */
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return is_valid();
    }
    
    /**
     * @brief Get type name
     */
    [[nodiscard]] std::string_view name() const noexcept {
        return info_ ? std::string_view{info_->name} : std::string_view{};
    }
    
    /**
     * @brief Get type size
     */
    [[nodiscard]] std::size_t size() const noexcept {
        return info_ ? info_->size : 0;
    }
    
    /**
     * @brief Get type_index
     */
    [[nodiscard]] std::type_index type_index() const noexcept {
        return info_ ? info_->type_index : std::type_index{typeid(void)};
    }
    
    /**
     * @brief Check if property exists
     */
    [[nodiscard]] bool has_property(std::string_view name) const noexcept {
        return info_ && info_->has_member(name);
    }
    
    /**
     * @brief Check if method exists
     */
    [[nodiscard]] bool has_method(std::string_view name) const noexcept {
        return info_ && info_->has_method(name);
    }
    
    /**
     * @brief Get property names
     */
    [[nodiscard]] const std::vector<std::string_view>& property_names() const noexcept {
        static const std::vector<std::string_view> empty;
        return info_ ? info_->member_names() : empty;
    }
    
    /**
     * @brief Get method names
     */
    [[nodiscard]] const std::vector<std::string_view>& method_names() const noexcept {
        static const std::vector<std::string_view> empty;
        return info_ ? info_->method_names() : empty;
    }
    
    /**
     * @brief Bind to an object for property/method access
     * 
     * Returns a BoundType which provides fast property access
     * without shared_ptr overhead.
     * 
     * @tparam T The object type
     * @param obj Reference to the object
     * @return BoundType for property/method access
     */
    template<typename T>
    [[nodiscard]] inline BoundType bind(T& obj) const;
    
    /**
     * @brief Create a new instance
     * 
     * @return shared_ptr to new instance, or nullptr if creation fails
     */
    [[nodiscard]] std::shared_ptr<void> create() const {
        if (!info_) [[unlikely]] return nullptr;
        
        // Fast path: use raw factory pointer
        if (info_->default_factory_raw) [[likely]] {
            return info_->default_factory_raw();
        }
        
        // Fallback: use std::function factory
        auto it = info_->factories.find("default");
        if (it != info_->factories.end() && it->second) {
            return it->second();
        }
        
        return nullptr;
    }
    
    /**
     * @brief Get raw TypeInfo pointer (for advanced use)
     */
    [[nodiscard]] const detail::TypeInfo* type_info() const noexcept {
        return info_;
    }
    
    /**
     * @brief Get a pre-cached property handle for fastest access
     * 
     * Use this when you need to access the same property many times.
     * The returned handle caches the offset, so subsequent accesses
     * are just pointer arithmetic.
     * 
     * @tparam T Property type
     * @param name Property name
     * @return PropertyHandle (may be invalid if property not found)
     */
    template<typename T>
    [[nodiscard]] PropertyHandle<T> get_property(std::string_view name) const noexcept {
        if (!info_) return PropertyHandle<T>{};
        auto it = info_->members.find(std::string{name});
        if (it == info_->members.end()) return PropertyHandle<T>{};
        return PropertyHandle<T>{&it->second};
    }
    
    /**
     * @brief Get a pre-cached method handle for fastest invocation
     * 
     * @param name Method name
     * @param param_count Number of parameters (for overload resolution)
     * @return MethodHandle (may be invalid if method not found)
     */
    [[nodiscard]] MethodHandle get_method(std::string_view name, std::size_t param_count = 0) const noexcept {
        if (!info_) return MethodHandle{};
        auto it = info_->methods.find(std::string{name});
        if (it == info_->methods.end()) return MethodHandle{};
        
        // Find overload with matching parameter count
        for (const auto& method : it->second) {
            if (method.param_types.size() == param_count) {
                return MethodHandle{&method};
            }
        }
        return MethodHandle{};
    }
    
    // Comparison operators
    [[nodiscard]] constexpr bool operator==(const RTypeHandle& other) const noexcept {
        return info_ == other.info_;
    }
    
    [[nodiscard]] constexpr bool operator!=(const RTypeHandle& other) const noexcept {
        return info_ != other.info_;
    }

private:
    const detail::TypeInfo* info_;
};

} // namespace rttm

// Include BoundType after RTypeHandle is defined
#include "BoundType.hpp"

namespace rttm {

// Implementation of RTypeHandle::bind (needs BoundType definition)
template<typename T>
BoundType RTypeHandle::bind(T& obj) const {
    // Type check in debug builds
#ifndef NDEBUG
    if (info_ && info_->type_index != std::type_index(typeid(T))) {
        throw ReflectionError("Type mismatch in bind()");
    }
#endif
    return BoundType{info_, static_cast<void*>(&obj)};
}

} // namespace rttm

#endif // RTTM_DETAIL_RTYPE_HANDLE_HPP
