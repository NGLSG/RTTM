/**
 * @file PropertyHandle.hpp
 * @brief Pre-cached property handle for maximum performance
 * 
 * PropertyHandle caches the property offset and type info, allowing
 * repeated property access with minimal overhead (just pointer arithmetic).
 * 
 * This is the fastest possible property access in RTTM - use it when
 * you access the same property many times in a hot loop.
 */

#ifndef RTTM_DETAIL_PROPERTY_HANDLE_HPP
#define RTTM_DETAIL_PROPERTY_HANDLE_HPP

#include "TypeInfo.hpp"
#include "Exceptions.hpp"

#include <string_view>
#include <typeindex>
#include <cstddef>

namespace rttm {

/**
 * @brief Pre-cached property handle for fastest access
 * 
 * PropertyHandle stores the offset and type info for a property,
 * eliminating the need for hash lookup on every access.
 * 
 * Usage:
 * @code
 * // Cache the property handle once
 * auto handle = RTypeHandle::get<MyClass>();
 * auto prop = handle.get_property<int>("value");
 * 
 * // Use in hot loop - just pointer arithmetic
 * MyClass obj;
 * for (int i = 0; i < 1000000; ++i) {
 *     prop.get(obj) = i;  // ~1ns per access
 * }
 * @endcode
 */
template<typename T>
class PropertyHandle {
public:
    /**
     * @brief Construct from MemberInfo
     */
    explicit PropertyHandle(const detail::MemberInfo* member) noexcept
        : offset_(member ? member->offset : 0)
        , valid_(member != nullptr)
    {}
    
    /**
     * @brief Default constructor creates invalid handle
     */
    constexpr PropertyHandle() noexcept : offset_(0), valid_(false) {}
    
    /**
     * @brief Check if handle is valid
     */
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return valid_;
    }
    
    /**
     * @brief Explicit bool conversion
     */
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return valid_;
    }
    
    /**
     * @brief Get property value from object (fastest path)
     * 
     * This is pure pointer arithmetic - no lookup, no type check.
     * 
     * @tparam U Object type
     * @param obj Reference to object
     * @return Reference to property value
     */
    template<typename U>
    [[nodiscard]] T& get(U& obj) const noexcept {
        void* ptr = static_cast<char*>(static_cast<void*>(&obj)) + offset_;
        return *static_cast<T*>(ptr);
    }
    
    /**
     * @brief Get property value from raw pointer (fastest path)
     */
    [[nodiscard]] T& get(void* obj) const noexcept {
        void* ptr = static_cast<char*>(obj) + offset_;
        return *static_cast<T*>(ptr);
    }
    
    /**
     * @brief Set property value
     */
    template<typename U, typename V>
    void set(U& obj, V&& value) const noexcept {
        get(obj) = std::forward<V>(value);
    }
    
    /**
     * @brief Get cached offset
     */
    [[nodiscard]] constexpr std::size_t offset() const noexcept {
        return offset_;
    }

private:
    std::size_t offset_;
    bool valid_;
};

/**
 * @brief Pre-cached method handle for fast invocation
 * 
 * MethodHandle stores the method info pointer, eliminating
 * the need for hash lookup on every call.
 */
class MethodHandle {
public:
    /**
     * @brief Construct from MethodInfo
     */
    explicit MethodHandle(const detail::MethodInfo* method) noexcept
        : method_(method) {}
    
    /**
     * @brief Default constructor creates invalid handle
     */
    constexpr MethodHandle() noexcept : method_(nullptr) {}
    
    /**
     * @brief Check if handle is valid
     */
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return method_ != nullptr;
    }
    
    /**
     * @brief Explicit bool conversion
     */
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return is_valid();
    }
    
    /**
     * @brief Call method with no arguments
     */
    template<typename R>
    [[nodiscard]] R call(void* obj) const {
        std::span<std::any> empty_args;
        std::any result = method_->call(obj, empty_args);
        
        if constexpr (std::is_void_v<R>) {
            return;
        } else {
            return std::any_cast<R>(result);
        }
    }
    
    /**
     * @brief Call method with arguments
     */
    template<typename R, typename... Args>
    [[nodiscard]] R call(void* obj, Args&&... args) const {
        if constexpr (sizeof...(Args) == 0) {
            return call<R>(obj);
        } else {
            std::array<std::any, sizeof...(Args)> arg_array = {std::any{std::forward<Args>(args)}...};
            std::span<std::any> arg_span{arg_array};
            std::any result = method_->call(obj, arg_span);
            
            if constexpr (std::is_void_v<R>) {
                return;
            } else {
                return std::any_cast<R>(result);
            }
        }
    }
    
    /**
     * @brief Get method info
     */
    [[nodiscard]] const detail::MethodInfo* info() const noexcept {
        return method_;
    }

private:
    const detail::MethodInfo* method_;
};

} // namespace rttm

#endif // RTTM_DETAIL_PROPERTY_HANDLE_HPP
