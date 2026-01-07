/**
 * @file BoundType.hpp
 * @brief Lightweight bound type for fast property/method access
 * 
 * BoundType combines a type handle with an object pointer, providing
 * fast property and method access without shared_ptr overhead.
 * 
 * This is the "hot path" API for RTTM - use it when you need maximum
 * performance for property reads/writes and method calls.
 */

#ifndef RTTM_DETAIL_BOUND_TYPE_HPP
#define RTTM_DETAIL_BOUND_TYPE_HPP

#include "TypeInfo.hpp"
#include "TypeTraits.hpp"
#include "Exceptions.hpp"

#include <string_view>
#include <any>
#include <array>
#include <span>
#include <cstddef>

namespace rttm {

namespace detail {
    // Use the hash from TypeInfo.hpp
    using detail::type_info_hash;
}

/**
 * @brief Lightweight bound type for fast property/method access
 * 
 * BoundType is a stack-allocated value type that holds:
 * - A pointer to TypeInfo (type metadata)
 * - A raw pointer to the bound object
 * 
 * No heap allocation, no refcount, no virtual calls on the hot path.
 * 
 * Usage:
 * @code
 * MyClass obj;
 * auto bound = RTypeHandle::get<MyClass>().bind(obj);
 * 
 * // Fast property access
 * int& val = bound.get<int>("value");
 * bound.set("value", 42);
 * 
 * // Fast method call
 * int result = bound.call<int>("getValue");
 * @endcode
 */
class BoundType {
public:
    /**
     * @brief Construct BoundType from TypeInfo and object pointer
     */
    constexpr BoundType(const detail::TypeInfo* info, void* obj) noexcept
        : info_(info), obj_(obj) {}
    
    // Default constructor creates invalid bound type
    constexpr BoundType() noexcept : info_(nullptr), obj_(nullptr) {}
    
    // Copy/move are trivial
    BoundType(const BoundType&) noexcept = default;
    BoundType& operator=(const BoundType&) noexcept = default;
    BoundType(BoundType&&) noexcept = default;
    BoundType& operator=(BoundType&&) noexcept = default;
    
    /**
     * @brief Check if bound type is valid
     */
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return info_ != nullptr && obj_ != nullptr;
    }
    
    /**
     * @brief Explicit bool conversion
     */
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return is_valid();
    }
    
    /**
     * @brief Get property by name (type-safe, returns reference)
     * 
     * This is the fastest property access path:
     * - Hash lookup in member map
     * - Direct pointer arithmetic
     * - No allocation, no refcount
     * 
     * @tparam T Expected property type
     * @param name Property name
     * @return Reference to property value
     * @throws PropertyNotFoundError if property doesn't exist
     * @throws PropertyTypeMismatchError if type doesn't match
     */
    template<typename T>
    [[nodiscard]] T& get(std::string_view name) const {
        // Look up member info (no allocation with transparent hash)
        const detail::MemberInfo* member = info_->find_member(name);
        if (!member) [[unlikely]] {
            if (!is_valid()) {
                throw ObjectNotCreatedError(info_ ? info_->name : "unknown");
            }
            throw PropertyNotFoundError(info_->name, name, info_->member_names());
        }
        
#ifndef NDEBUG
        // Type check only in debug builds
        if (member->type_index != std::type_index(typeid(T))) [[unlikely]] {
            throw PropertyTypeMismatchError(name, member->type_name,
                                            std::string{detail::type_name<T>()});
        }
#endif
        
        // Direct pointer arithmetic - this is the hot path
        void* ptr = static_cast<char*>(obj_) + member->offset;
        return *static_cast<T*>(ptr);
    }
    
    /**
     * @brief Get property by name (const version)
     */
    template<typename T>
    [[nodiscard]] const T& cget(std::string_view name) const {
        if (!is_valid()) [[unlikely]] {
            throw ObjectNotCreatedError(info_ ? info_->name : "unknown");
        }
        
        const detail::MemberInfo* member = info_->find_member(name);
        if (!member) [[unlikely]] {
            throw PropertyNotFoundError(info_->name, name, info_->member_names());
        }
        
#ifndef NDEBUG
        if (member->type_index != std::type_index(typeid(T))) [[unlikely]] {
            throw PropertyTypeMismatchError(name, member->type_name,
                                            std::string{detail::type_name<T>()});
        }
#endif
        
        const void* ptr = static_cast<const char*>(obj_) + member->offset;
        return *static_cast<const T*>(ptr);
    }
    
    /**
     * @brief Set property by name
     * 
     * @tparam T Property type
     * @param name Property name
     * @param value Value to set
     */
    template<typename T>
    void set(std::string_view name, T&& value) const {
        get<std::remove_cvref_t<T>>(name) = std::forward<T>(value);
    }
    
    /**
     * @brief Get property with cached offset (fastest path)
     * 
     * Use this when you access the same property repeatedly.
     * Cache the offset once, then use it for all accesses.
     * 
     * @tparam T Property type
     * @param offset Cached offset from get_property_offset()
     * @return Reference to property value
     */
    template<typename T>
    [[nodiscard]] T& get_by_offset(std::size_t offset) const noexcept {
        void* ptr = static_cast<char*>(obj_) + offset;
        return *static_cast<T*>(ptr);
    }
    
    /**
     * @brief Get property offset for caching
     * 
     * @param name Property name
     * @return Offset in bytes, or SIZE_MAX if not found
     */
    [[nodiscard]] std::size_t get_property_offset(std::string_view name) const noexcept {
        if (!info_) return SIZE_MAX;
        const detail::MemberInfo* member = info_->find_member(name);
        return member ? member->offset : SIZE_MAX;
    }
    
    /**
     * @brief Call method by name (no arguments)
     * 
     * @tparam R Return type
     * @param name Method name
     * @return Method return value
     */
    template<typename R>
    [[nodiscard]] R call(std::string_view name) const {
        if (!is_valid()) [[unlikely]] {
            throw ObjectNotCreatedError(info_ ? info_->name : "unknown");
        }
        
        const auto* method_list = info_->find_methods(name);
        if (!method_list) [[unlikely]] {
            throw MethodNotFoundError(info_->name, name, info_->method_names());
        }
        
        // Find overload with no parameters
        const detail::MethodInfo* matched = nullptr;
        for (const auto& method : *method_list) {
            if (method.param_types.empty()) {
                matched = &method;
                break;
            }
        }
        
        if (!matched) [[unlikely]] {
            throw MethodSignatureMismatchError(name, "()", "no matching overload");
        }
        
        std::span<std::any> empty_args;
        std::any result = matched->call(obj_, empty_args);
        
        if constexpr (std::is_void_v<R>) {
            return;
        } else if constexpr (std::is_reference_v<R>) {
            // For reference returns, we need to return by value to avoid dangling reference
            return std::any_cast<std::remove_cvref_t<R>>(std::move(result));
        } else {
            return std::any_cast<R>(std::move(result));
        }
    }
    
    /**
     * @brief Call method by name with arguments
     * 
     * @tparam R Return type
     * @tparam Args Argument types
     * @param name Method name
     * @param args Method arguments
     * @return Method return value
     */
    template<typename R, typename... Args>
    [[nodiscard]] R call(std::string_view name, Args&&... args) const {
        if (!is_valid()) [[unlikely]] {
            throw ObjectNotCreatedError(info_ ? info_->name : "unknown");
        }
        
        const auto* method_list = info_->find_methods(name);
        if (!method_list) [[unlikely]] {
            throw MethodNotFoundError(info_->name, name, info_->method_names());
        }
        
        // Find overload with matching parameter count
        const detail::MethodInfo* matched = nullptr;
        for (const auto& method : *method_list) {
            if (method.param_types.size() == sizeof...(Args)) {
                matched = &method;
                break;
            }
        }
        
        if (!matched) [[unlikely]] {
            throw MethodSignatureMismatchError(name, "expected args", "no matching overload");
        }
        
        if constexpr (sizeof...(Args) == 0) {
            std::span<std::any> empty_args;
            std::any result = matched->call(obj_, empty_args);
            
            if constexpr (std::is_void_v<R>) {
                return;
            } else if constexpr (std::is_reference_v<R>) {
                return std::any_cast<std::remove_cvref_t<R>>(std::move(result));
            } else {
                return std::any_cast<R>(std::move(result));
            }
        } else {
            std::array<std::any, sizeof...(Args)> arg_array = {std::any{std::forward<Args>(args)}...};
            std::span<std::any> arg_span{arg_array};
            std::any result = matched->call(obj_, arg_span);
            
            if constexpr (std::is_void_v<R>) {
                return;
            } else if constexpr (std::is_reference_v<R>) {
                return std::any_cast<std::remove_cvref_t<R>>(std::move(result));
            } else {
                return std::any_cast<R>(std::move(result));
            }
        }
    }
    
    /**
     * @brief Call void method (convenience)
     */
    template<typename... Args>
    void call_void(std::string_view name, Args&&... args) const {
        call<void>(name, std::forward<Args>(args)...);
    }
    
    /**
     * @brief Get type name
     */
    [[nodiscard]] std::string_view type_name() const noexcept {
        return info_ ? std::string_view{info_->name} : std::string_view{};
    }
    
    /**
     * @brief Get raw object pointer
     */
    [[nodiscard]] void* raw() const noexcept {
        return obj_;
    }
    
    /**
     * @brief Cast to specific type
     */
    template<typename T>
    [[nodiscard]] T& as() const noexcept {
        return *static_cast<T*>(obj_);
    }
    
    /**
     * @brief Get TypeInfo pointer
     */
    [[nodiscard]] const detail::TypeInfo* type_info() const noexcept {
        return info_;
    }

private:
    const detail::TypeInfo* info_;
    void* obj_;
};

} // namespace rttm

#endif // RTTM_DETAIL_BOUND_TYPE_HPP
