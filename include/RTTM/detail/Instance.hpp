/**
 * @file Instance.hpp
 * @brief Dynamic object instance for pure runtime reflection
 * 
 * Instance wraps an object with its type information, allowing
 * pure dynamic property/method access without compile-time type knowledge.
 * 
 * Key features:
 * - No template parameters needed for access
 * - DLL-compatible (works with dynamically loaded types)
 * - Owns or references the object
 * - Full reflection access via string names
 */

#ifndef RTTM_DETAIL_INSTANCE_HPP
#define RTTM_DETAIL_INSTANCE_HPP

#include "TypeInfo.hpp"
#include "TypeManager.hpp"
#include "Variant.hpp"
#include "Exceptions.hpp"

#include <memory>
#include <string_view>
#include <any>
#include <span>

namespace rttm {

// Forward declaration
class RTypeHandle;
class DynamicProperty;
class DynamicMethod;

/**
 * @brief Cached property handle for pure dynamic access
 * 
 * DynamicProperty caches the property lookup result, allowing
 * fast repeated access without string lookup overhead.
 */
class DynamicProperty {
public:
    DynamicProperty() noexcept = default;
    
    DynamicProperty(const detail::MemberInfo* member, const detail::TypeInfo* type_info)
        : member_(member), type_info_(type_info) {}
    
    [[nodiscard]] bool is_valid() const noexcept { return member_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return is_valid(); }
    
    [[nodiscard]] std::string_view name() const noexcept {
        return member_ ? std::string_view{member_->name} : std::string_view{};
    }
    
    [[nodiscard]] std::type_index type() const noexcept {
        return member_ ? member_->type_index : std::type_index(typeid(void));
    }
    
    /**
     * @brief Get property value from object pointer
     */
    [[nodiscard]] Variant get_value(void* obj) const;
    
    /**
     * @brief Set property value on object pointer
     */
    void set_value(void* obj, const Variant& value) const;
    
    /**
     * @brief Set property value directly (fast path for primitives)
     */
    template<typename T>
    void set_value_direct(void* obj, T value) const {
        if (!member_) [[unlikely]] return;
        void* prop_ptr = static_cast<char*>(obj) + member_->offset;
        
        if (member_->type_index == typeid(T)) [[likely]] {
            *static_cast<T*>(prop_ptr) = value;
            return;
        }
        
        // Handle numeric conversions
        if constexpr (std::is_arithmetic_v<T>) {
            if (member_->type_index == typeid(int)) {
                *static_cast<int*>(prop_ptr) = static_cast<int>(value);
            } else if (member_->type_index == typeid(float)) {
                *static_cast<float*>(prop_ptr) = static_cast<float>(value);
            } else if (member_->type_index == typeid(double)) {
                *static_cast<double*>(prop_ptr) = static_cast<double>(value);
            } else if (member_->type_index == typeid(long)) {
                *static_cast<long*>(prop_ptr) = static_cast<long>(value);
            }
        }
    }
    
    /**
     * @brief Get property value directly (fast path for primitives)
     */
    template<typename T>
    [[nodiscard]] T get_value_direct(void* obj) const {
        if (!member_) [[unlikely]] return T{};
        void* prop_ptr = static_cast<char*>(obj) + member_->offset;
        
        if (member_->type_index == typeid(T)) [[likely]] {
            return *static_cast<T*>(prop_ptr);
        }
        
        // Handle numeric conversions
        if constexpr (std::is_arithmetic_v<T>) {
            if (member_->type_index == typeid(int)) {
                return static_cast<T>(*static_cast<int*>(prop_ptr));
            } else if (member_->type_index == typeid(float)) {
                return static_cast<T>(*static_cast<float*>(prop_ptr));
            } else if (member_->type_index == typeid(double)) {
                return static_cast<T>(*static_cast<double*>(prop_ptr));
            }
        }
        return T{};
    }

private:
    const detail::MemberInfo* member_ = nullptr;
    const detail::TypeInfo* type_info_ = nullptr;
};

/**
 * @brief Cached method handle for pure dynamic access
 */
class DynamicMethod {
public:
    DynamicMethod() noexcept = default;
    
    DynamicMethod(const detail::MethodInfo* method, const detail::TypeInfo* type_info)
        : method_(method), type_info_(type_info) {}
    
    [[nodiscard]] bool is_valid() const noexcept { return method_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return is_valid(); }
    
    [[nodiscard]] std::string_view name() const noexcept {
        return method_ ? std::string_view{method_->name} : std::string_view{};
    }
    
    /**
     * @brief Invoke method on object pointer
     */
    [[nodiscard]] Variant invoke(void* obj, std::span<const Variant> args = {}) const;

private:
    const detail::MethodInfo* method_ = nullptr;
    const detail::TypeInfo* type_info_ = nullptr;
};

/**
 * @brief Dynamic object instance for pure runtime reflection
 * 
 * Instance provides RTTR-like dynamic object access:
 * - Create objects by type name (string)
 * - Access properties by name without knowing type
 * - Call methods by name without knowing signature
 * - Works with DLL-loaded types
 * 
 * Usage:
 * @code
 * // Create by type name (no template)
 * auto inst = Instance::create("MyClass");
 * 
 * // Property access (returns Variant)
 * Variant val = inst.get_property("intValue");
 * inst.set_property("intValue", Variant::create(42));
 * 
 * // Method call (returns Variant)
 * Variant result = inst.invoke("getInt");
 * inst.invoke("setInt", {Variant::create(100)});
 * 
 * // Get raw pointer for interop
 * void* ptr = inst.get_raw();
 * @endcode
 */
class Instance {
public:
    /**
     * @brief Create instance by type name
     * 
     * This is the key DLL-compatible API - creates an object
     * using only the registered type name.
     */
    [[nodiscard]] static Instance create(std::string_view type_name);
    
    /**
     * @brief Create instance from existing object (takes ownership)
     */
    [[nodiscard]] static Instance from_owned(std::shared_ptr<void> obj, const detail::TypeInfo* type_info);
    
    /**
     * @brief Create instance from existing object (non-owning reference)
     */
    [[nodiscard]] static Instance from_ref(void* obj, const detail::TypeInfo* type_info);
    
    /**
     * @brief Create instance from existing object with type lookup
     */
    template<typename T>
    [[nodiscard]] static Instance from_ref(T& obj);
    
    /**
     * @brief Create instance from existing object (takes ownership)
     */
    template<typename T>
    [[nodiscard]] static Instance from_owned(std::shared_ptr<T> obj);
    
    // Default constructor creates invalid instance
    Instance() noexcept = default;
    
    // Move only (no copy to avoid ownership issues)
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) noexcept = default;
    Instance& operator=(Instance&&) noexcept = default;
    
    /**
     * @brief Check if instance is valid
     */
    [[nodiscard]] bool is_valid() const noexcept {
        return type_info_ != nullptr && (owned_obj_ || ref_obj_);
    }
    
    [[nodiscard]] explicit operator bool() const noexcept {
        return is_valid();
    }
    
    /**
     * @brief Get type name
     */
    [[nodiscard]] std::string_view type_name() const noexcept {
        return type_info_ ? std::string_view{type_info_->name} : std::string_view{};
    }
    
    /**
     * @brief Get type_index
     */
    [[nodiscard]] std::type_index type_index() const noexcept {
        return type_info_ ? type_info_->type_index : std::type_index(typeid(void));
    }
    
    /**
     * @brief Get type info
     */
    [[nodiscard]] const detail::TypeInfo* type_info() const noexcept {
        return type_info_;
    }
    
    /**
     * @brief Get raw object pointer
     */
    [[nodiscard]] void* get_raw() noexcept {
        return owned_obj_ ? owned_obj_.get() : ref_obj_;
    }
    
    [[nodiscard]] const void* get_raw() const noexcept {
        return owned_obj_ ? owned_obj_.get() : ref_obj_;
    }
    
    /**
     * @brief Check if instance owns the object
     */
    [[nodiscard]] bool is_owned() const noexcept {
        return owned_obj_ != nullptr;
    }
    
    // ========================================================================
    // Pure Dynamic Property Access (no templates needed)
    // ========================================================================
    
    /**
     * @brief Get property value as Variant
     * 
     * This is the pure dynamic API - no template parameters needed.
     * The returned Variant contains the property value.
     */
    [[nodiscard]] Variant get_property(std::string_view name) const;
    
    /**
     * @brief Set property value from Variant
     */
    void set_property(std::string_view name, const Variant& value);
    
    /**
     * @brief Set property value from any type (like RTTR's set_value)
     * 
     * Template overload that accepts any value type directly.
     * Internally converts to the property's type.
     */
    template<typename T>
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((always_inline))
#endif
    inline void set_property(std::string_view name, T value) {
        const detail::MemberInfo* member = type_info_->find_member(name);
        if (!member) [[unlikely]] {
            if (!is_valid()) {
                throw ObjectNotCreatedError(type_name().empty() ? "unknown" : std::string(type_name()));
            }
            throw PropertyNotFoundError(std::string(type_name()), name, type_info_->member_names());
        }
        
        void* prop_ptr = static_cast<char*>(get_raw()) + member->offset;
        auto ti = member->type_index;
        
        // Direct write for matching types
        if (ti == typeid(T)) [[likely]] {
            *static_cast<T*>(prop_ptr) = value;
            return;
        }
        
        // Handle numeric conversions
        if constexpr (std::is_arithmetic_v<T>) {
            if (ti == typeid(int)) { *static_cast<int*>(prop_ptr) = static_cast<int>(value); return; }
            if (ti == typeid(float)) { *static_cast<float*>(prop_ptr) = static_cast<float>(value); return; }
            if (ti == typeid(double)) { *static_cast<double*>(prop_ptr) = static_cast<double>(value); return; }
            if (ti == typeid(long)) { *static_cast<long*>(prop_ptr) = static_cast<long>(value); return; }
            if (ti == typeid(bool)) { *static_cast<bool*>(prop_ptr) = static_cast<bool>(value); return; }
        }
        
        throw ReflectionError("Type mismatch in set_property");
    }
    
    /**
     * @brief Set property value directly (fast path, avoids Variant creation)
     * 
     * This is the optimized API for setting primitive types directly.
     */
    template<typename T>
    void set_property_direct(std::string_view name, T value);
    
    /**
     * @brief Get property as specific type (convenience)
     */
    template<typename T>
    [[nodiscard]] T get_property_as(std::string_view name) const {
        Variant v = get_property(name);
        return v.get<T>();
    }
    
    /**
     * @brief Set property from value (convenience) - uses fast path for primitives
     */
    template<typename T>
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((always_inline))
#endif
    inline void set_property_value(std::string_view name, T value) {
        set_property_direct(name, value);
    }
    
    /**
     * @brief Get property value directly as type (fast path)
     */
    template<typename T>
    [[nodiscard]] T get_property_value(std::string_view name) const;
    
    /**
     * @brief Check if property exists
     */
    [[nodiscard]] bool has_property(std::string_view name) const noexcept {
        return type_info_ && type_info_->has_member(name);
    }
    
    /**
     * @brief Get all property names
     */
    [[nodiscard]] const std::vector<std::string_view>& property_names() const noexcept {
        static const std::vector<std::string_view> empty;
        return type_info_ ? type_info_->member_names() : empty;
    }
    
    /**
     * @brief Get cached property handle for fast repeated access
     */
    [[nodiscard]] DynamicProperty get_property_handle(std::string_view name) const {
        if (!type_info_) return DynamicProperty{};
        const detail::MemberInfo* member = type_info_->find_member(name);
        return DynamicProperty{member, type_info_};
    }
    
    // ========================================================================
    // Pure Dynamic Method Invocation (no templates needed)
    // ========================================================================
    
    /**
     * @brief Invoke method with Variant arguments, returns Variant
     * 
     * This is the pure dynamic API - no template parameters needed.
     */
    [[nodiscard]] Variant invoke(std::string_view name, std::span<const Variant> args = {}) const;
    
    /**
     * @brief Invoke method with raw arguments (like RTTR's invoke)
     * 
     * Template overload that accepts any argument types directly.
     */
    template<typename... Args>
    [[nodiscard]] Variant invoke(std::string_view name, Args&&... args) const {
        if constexpr (sizeof...(Args) == 0) {
            return invoke(name, std::span<const Variant>{});
        } else {
            std::array<Variant, sizeof...(Args)> var_args = {Variant::create(std::forward<Args>(args))...};
            return invoke(name, std::span<const Variant>{var_args});
        }
    }
    
    /**
     * @brief Invoke method with typed arguments (convenience)
     */
    template<typename R, typename... Args>
    [[nodiscard]] R invoke_as(std::string_view name, Args&&... args) const {
        std::array<Variant, sizeof...(Args)> var_args = {Variant::create(std::forward<Args>(args))...};
        Variant result = invoke(name, var_args);
        if constexpr (std::is_void_v<R>) {
            return;
        } else {
            return result.get<R>();
        }
    }
    
    /**
     * @brief Check if method exists
     */
    [[nodiscard]] bool has_method(std::string_view name) const noexcept {
        return type_info_ && type_info_->has_method(name);
    }
    
    /**
     * @brief Get all method names
     */
    [[nodiscard]] const std::vector<std::string_view>& method_names() const noexcept {
        static const std::vector<std::string_view> empty;
        return type_info_ ? type_info_->method_names() : empty;
    }
    
    /**
     * @brief Get cached method handle for fast repeated access
     */
    [[nodiscard]] DynamicMethod get_method_handle(std::string_view name, std::size_t arg_count = 0) const {
        if (!type_info_) return DynamicMethod{};
        const auto* method_list = type_info_->find_methods(name);
        if (!method_list || method_list->empty()) return DynamicMethod{};
        
        // Find overload with matching parameter count
        for (const auto& method : *method_list) {
            if (method.param_types.size() == arg_count) {
                return DynamicMethod{&method, type_info_};
            }
        }
        return DynamicMethod{&(*method_list)[0], type_info_};
    }
    
    // ========================================================================
    // Type Casting
    // ========================================================================
    
    /**
     * @brief Cast to specific type (unsafe)
     */
    template<typename T>
    [[nodiscard]] T& as() {
        return *static_cast<T*>(get_raw());
    }
    
    template<typename T>
    [[nodiscard]] const T& as() const {
        return *static_cast<const T*>(get_raw());
    }
    
    /**
     * @brief Try cast to specific type (safe)
     */
    template<typename T>
    [[nodiscard]] T* try_as() noexcept {
        if (type_info_ && type_info_->type_index == std::type_index(typeid(T))) {
            return static_cast<T*>(get_raw());
        }
        return nullptr;
    }
    
    template<typename T>
    [[nodiscard]] const T* try_as() const noexcept {
        if (type_info_ && type_info_->type_index == std::type_index(typeid(T))) {
            return static_cast<const T*>(get_raw());
        }
        return nullptr;
    }

private:
    friend class DynamicMethod;  // Allow DynamicMethod to access private helpers
    
    Instance(std::shared_ptr<void> owned, void* ref, const detail::TypeInfo* info)
        : owned_obj_(std::move(owned)), ref_obj_(ref), type_info_(info) {}
    
    // Helper functions for invoke optimization
    static std::any variant_to_any(const Variant& v);
    static Variant convert_result(const std::any& result, std::type_index return_type);
    
    // Direct property setter implementation
    template<typename T>
    void set_property_direct_impl(const detail::MemberInfo* member, T value);
    
    std::shared_ptr<void> owned_obj_;  // Owned object (if any)
    void* ref_obj_ = nullptr;           // Referenced object (if not owned)
    const detail::TypeInfo* type_info_ = nullptr;
};

// Template implementation for set_property_direct
template<typename T>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((always_inline))
#endif
inline void Instance::set_property_direct(std::string_view name, T value) {
    const detail::MemberInfo* member = type_info_->find_member(name);
    if (!member) [[unlikely]] {
        if (!is_valid()) {
            throw ObjectNotCreatedError(type_name().empty() ? "unknown" : std::string(type_name()));
        }
        throw PropertyNotFoundError(std::string(type_name()), name, type_info_->member_names());
    }
    
    void* prop_ptr = static_cast<char*>(get_raw()) + member->offset;
    
    // Type check for safety
    if (member->type_index == typeid(T)) [[likely]] {
        *static_cast<T*>(prop_ptr) = value;
        return;
    }
    
    // Handle numeric conversions
    if constexpr (std::is_arithmetic_v<T>) {
        auto ti = member->type_index;
        if (ti == typeid(int)) { *static_cast<int*>(prop_ptr) = static_cast<int>(value); return; }
        if (ti == typeid(float)) { *static_cast<float*>(prop_ptr) = static_cast<float>(value); return; }
        if (ti == typeid(double)) { *static_cast<double*>(prop_ptr) = static_cast<double>(value); return; }
        if (ti == typeid(long)) { *static_cast<long*>(prop_ptr) = static_cast<long>(value); return; }
    }
    
    throw ReflectionError("Type mismatch in set_property_direct");
}

// Template implementation for get_property_value
template<typename T>
#if defined(__GNUC__) || defined(__clang__)
__attribute__((always_inline))
#endif
inline T Instance::get_property_value(std::string_view name) const {
    const detail::MemberInfo* member = type_info_->find_member(name);
    if (!member) [[unlikely]] {
        if (!is_valid()) {
            throw ObjectNotCreatedError(type_name().empty() ? "unknown" : std::string(type_name()));
        }
        throw PropertyNotFoundError(std::string(type_name()), name, type_info_->member_names());
    }
    
    void* prop_ptr = static_cast<char*>(const_cast<void*>(get_raw())) + member->offset;
    
    // Type check for safety
    if (member->type_index == typeid(T)) [[likely]] {
        return *static_cast<T*>(prop_ptr);
    }
    
    // Handle numeric conversions
    if constexpr (std::is_arithmetic_v<T>) {
        auto ti = member->type_index;
        if (ti == typeid(int)) return static_cast<T>(*static_cast<int*>(prop_ptr));
        if (ti == typeid(float)) return static_cast<T>(*static_cast<float*>(prop_ptr));
        if (ti == typeid(double)) return static_cast<T>(*static_cast<double*>(prop_ptr));
        if (ti == typeid(long)) return static_cast<T>(*static_cast<long*>(prop_ptr));
    }
    
    throw ReflectionError("Type mismatch in get_property_value");
}

} // namespace rttm

#endif // RTTM_DETAIL_INSTANCE_HPP
