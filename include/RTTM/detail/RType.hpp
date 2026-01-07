/**
 * @file RType.hpp
 * @brief Runtime type handle for RTTM reflection library
 * 
 * This file implements the RType class which provides:
 * - Runtime type information access
 * - Dynamic object creation
 * - Property access (type-safe and dynamic)
 * - Method invocation with automatic argument conversion
 * - Property and method enumeration
 */

#ifndef RTTM_DETAIL_RTYPE_HPP
#define RTTM_DETAIL_RTYPE_HPP

#include "TypeInfo.hpp"
#include "TypeManager.hpp"
#include "TypeTraits.hpp"
#include "Exceptions.hpp"

#include <string>
#include <string_view>
#include <memory>
#include <any>
#include <array>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <typeindex>

namespace rttm {

// Forward declaration
class RType;

namespace detail {
    // Compile-time string hash for fast lookup (FNV-1a)
    constexpr std::size_t hash_string(std::string_view str) noexcept {
        std::size_t hash = 14695981039346656037ULL;
        for (char c : str) {
            hash ^= static_cast<std::size_t>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }
    
    // Small inline cache for hot path optimization
    template<typename Key, typename Value, std::size_t N = 4>
    class InlineCache {
    public:
        struct Entry {
            Key key{};
            Value value{};
            bool valid = false;
        };
        
        Value* find(const Key& key) noexcept {
            for (auto& entry : entries_) {
                if (entry.valid && entry.key == key) {
                    return &entry.value;
                }
            }
            return nullptr;
        }
        
        void insert(const Key& key, const Value& value) noexcept {
            // Find empty slot or replace oldest
            for (auto& entry : entries_) {
                if (!entry.valid) {
                    entry.key = key;
                    entry.value = value;
                    entry.valid = true;
                    return;
                }
            }
            // Replace first entry (simple eviction)
            entries_[0].key = key;
            entries_[0].value = value;
        }
        
    private:
        std::array<Entry, N> entries_{};
    };
}

/**
 * @brief Runtime type handle for dynamic reflection operations
 * 
 * RType provides a unified interface for:
 * - Creating objects of registered types
 * - Accessing properties by name
 * - Invoking methods by name
 * - Querying type information
 * 
 * Usage:
 * @code
 * // Get type handle
 * auto rtype = RType::get<MyClass>();
 * 
 * // Create instance
 * rtype->create();
 * 
 * // Access property
 * int& value = rtype->property<int>("value");
 * 
 * // Invoke method
 * auto result = rtype->invoke<int>("getValue");
 * @endcode
 */
class RType : public std::enable_shared_from_this<RType> {
public:
    /**
     * @brief Get RType by type name (string)
     * 
     * @param type_name The registered type name
     * @return Shared pointer to RType
     * @throws TypeNotRegisteredError if type is not registered
     */
    static std::shared_ptr<RType> get(std::string_view type_name) {
        auto& mgr = detail::TypeManager::instance();
        const detail::TypeInfo* info = mgr.get_type(type_name);
        
        if (!info) [[unlikely]] {
            throw TypeNotRegisteredError(type_name);
        }
        
        return std::make_shared<RType>(info);
    }
    
    /**
     * @brief Get RType by template type (optimized with static cache)
     * 
     * Uses a per-type static cache to avoid repeated TypeInfo lookups.
     * Each call still creates a new RType instance for thread safety.
     * 
     * @tparam T The type to get handle for
     * @return Shared pointer to RType
     * @throws TypeNotRegisteredError if type is not registered
     */
    template<typename T>
    static std::shared_ptr<RType> get() {
        // Static cache per type - initialized once, thread-safe
        static const detail::TypeInfo* cached_info = []() {
            auto& mgr = detail::TypeManager::instance();
            const detail::TypeInfo* info = mgr.get_type_by_id(detail::type_id<T>);
            if (!info) {
                info = mgr.get_type(detail::type_name<T>());
            }
            return info;
        }();
        
        if (!cached_info) [[unlikely]] {
            throw TypeNotRegisteredError(detail::type_name<T>());
        }
        
        return std::make_shared<RType>(cached_info);
    }
    
    // Allow construction with TypeInfo for optimization
    explicit RType(const detail::TypeInfo* info = nullptr) noexcept : info_(info) {}
    
    /**
     * @brief Create an instance using default constructor
     * 
     * @return true if creation succeeded, false otherwise
     */
    bool create() {
        if (!info_) [[unlikely]] return false;
        
        // Fast path: use raw factory pointer if available
        if (info_->default_factory_raw) [[likely]] {
            try {
                instance_ = info_->default_factory_raw();
                if (instance_) [[likely]] {
                    created_ = true;
                    return true;
                }
            } catch (...) {
                instance_.reset();
                created_ = false;
                throw;
            }
        }
        
        // Fallback: use std::function factory
        if (!default_factory_) {
            auto factory_it = info_->factories.find("default");
            if (factory_it == info_->factories.end()) {
                return false;
            }
            default_factory_ = &factory_it->second;
        }
        
        try {
            instance_ = (*default_factory_)();
            if (instance_) [[likely]] {
                created_ = true;
                return true;
            }
        } catch (...) {
            // Creation failed - ensure clean state
            instance_.reset();
            created_ = false;
            throw;
        }
        
        return false;
    }
    
    /**
     * @brief Create an instance with constructor arguments
     * 
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return true if creation succeeded, false otherwise
     */
    template<typename... Args>
    bool create(Args&&... args) {
        if (!info_) return false;
        
        // For default constructor
        if constexpr (sizeof...(Args) == 0) {
            return create();
        } else {
            // Generate signature for lookup
            std::string signature = generate_signature<Args...>();
            
            // Try to find matching factory
            auto factory_it = info_->factories.find(signature);
            if (factory_it != info_->factories.end() && factory_it->second) {
                try {
                    instance_ = factory_it->second();
                    if (instance_) {
                        created_ = true;
                        return true;
                    }
                } catch (...) {
                    instance_.reset();
                    created_ = false;
                    throw;
                }
            }
            
            // Factory not found or doesn't support args directly
            // This is a limitation - parameterized construction needs special handling
            return false;
        }
    }
    
    /**
     * @brief Attach an existing instance to this RType
     * 
     * @tparam T The type of the instance
     * @param instance Reference to the instance to attach
     */
    template<typename T>
    void attach(T& instance) {
        // Verify type matches
        if (info_ && info_->type_index != std::type_index(typeid(T))) {
            throw ReflectionError("Type mismatch in attach()");
        }
        
        // Create a non-owning shared_ptr
        instance_ = std::shared_ptr<void>(&instance, [](void*) {});
        created_ = true;
    }
    
    /**
     * @brief Attach an existing instance via shared_ptr
     * 
     * @tparam T The type of the instance
     * @param instance Shared pointer to the instance
     */
    template<typename T>
    void attach(std::shared_ptr<T> instance) {
        if (info_ && info_->type_index != std::type_index(typeid(T))) {
            throw ReflectionError("Type mismatch in attach()");
        }
        
        instance_ = std::static_pointer_cast<void>(instance);
        created_ = true;
    }


    /**
     * @brief Type-safe property access
     * 
     * @tparam T The expected property type
     * @param name The property name
     * @return Reference to the property value
     * @throws ObjectNotCreatedError if no instance is attached
     * @throws PropertyNotFoundError if property doesn't exist
     * @throws PropertyTypeMismatchError if type doesn't match
     */
    template<typename T>
    T& property(std::string_view name) {
        if (!created_ || !instance_) [[unlikely]] {
            throw ObjectNotCreatedError(info_ ? info_->name : "unknown");
        }
        
        // Fast path: check inline cache first (no hash computation for common cases)
        const std::size_t name_hash = detail::hash_string(name);
        
        if (auto* cached = prop_cache_.find(name_hash)) [[likely]] {
            void* ptr = static_cast<char*>(instance_.get()) + *cached;
            return *static_cast<T*>(ptr);
        }
        
        // Cache miss - look up member info (no allocation with transparent hash)
        const detail::MemberInfo* member = info_->find_member(name);
        if (!member) [[unlikely]] {
            throw PropertyNotFoundError(info_->name, name, info_->member_names());
        }
        
        // Verify type matches (only on first access)
        if (member->type_index != std::type_index(typeid(T))) [[unlikely]] {
            throw PropertyTypeMismatchError(name, member->type_name, 
                                            std::string{detail::type_name<T>()});
        }
        
        // Cache the offset for future access
        prop_cache_.insert(name_hash, member->offset);
        
        // Calculate address and return reference
        void* ptr = static_cast<char*>(instance_.get()) + member->offset;
        return *static_cast<T*>(ptr);
    }
    
    /**
     * @brief Const type-safe property access
     * 
     * @tparam T The expected property type
     * @param name The property name
     * @return Const reference to the property value
     */
    template<typename T>
    const T& property(std::string_view name) const {
        return const_cast<RType*>(this)->property<T>(name);
    }
    
    /**
     * @brief Dynamic property access returning RType
     * 
     * Returns a new RType wrapping the property value.
     * Useful for accessing nested objects.
     * 
     * @param name The property name
     * @return Shared pointer to RType wrapping the property
     * @throws ObjectNotCreatedError if no instance is attached
     * @throws PropertyNotFoundError if property doesn't exist
     */
    std::shared_ptr<RType> property(std::string_view name) {
        if (!created_ || !instance_) {
            throw ObjectNotCreatedError(info_ ? info_->name : "unknown");
        }
        
        // Look up member info (no allocation)
        const detail::MemberInfo* member = info_->find_member(name);
        if (!member) {
            throw PropertyNotFoundError(info_->name, name, info_->member_names());
        }
        
        // Get the property address
        void* prop_ptr = static_cast<char*>(instance_.get()) + member->offset;
        
        // Try to get type info for the property type
        auto& mgr = detail::TypeManager::instance();
        const detail::TypeInfo* prop_info = mgr.get_type(member->type_index);
        
        // Create new RType for the property
        auto prop_rtype = std::make_shared<RType>();
        
        if (prop_info) {
            prop_rtype->info_ = prop_info;
        } else {
            // Property type not registered - create minimal info
            prop_rtype->info_ = nullptr;
        }
        
        // Create non-owning shared_ptr to the property
        // The lifetime is tied to the parent object
        prop_rtype->instance_ = std::shared_ptr<void>(instance_, prop_ptr);
        prop_rtype->created_ = true;
        
        return prop_rtype;
    }
    
    /**
     * @brief Check if a property is a sequential container
     * @param name The property name
     * @return true if the property is a sequential container
     */
    [[nodiscard]] bool is_sequential_container(std::string_view name) const {
        if (!info_) return false;
        auto member_it = info_->members.find(std::string{name});
        if (member_it == info_->members.end()) return false;
        return member_it->second.category == detail::MemberCategory::Sequential;
    }
    
    /**
     * @brief Check if a property is an associative container
     * @param name The property name
     * @return true if the property is an associative container
     */
    [[nodiscard]] bool is_associative_container(std::string_view name) const {
        if (!info_) return false;
        auto member_it = info_->members.find(std::string{name});
        if (member_it == info_->members.end()) return false;
        return member_it->second.category == detail::MemberCategory::Associative;
    }


    /**
     * @brief Invoke a method with automatic argument conversion
     * 
     * @tparam R The expected return type
     * @tparam Args The argument types
     * @param name The method name
     * @param args The method arguments
     * @return The method return value
     * @throws ObjectNotCreatedError if no instance is attached
     * @throws MethodNotFoundError if method doesn't exist
     * @throws MethodSignatureMismatchError if arguments don't match
     */
    template<typename R, typename... Args>
    R invoke(std::string_view name, Args&&... args) {
        if (!created_ || !instance_) [[unlikely]] {
            throw ObjectNotCreatedError(info_ ? info_->name : "unknown");
        }
        
        const detail::MethodInfo* matched = nullptr;
        
        // Fast path: check inline cache
        const std::size_t name_hash = detail::hash_string(name);
        
        if (auto* cached = method_cache_.find(name_hash)) [[likely]] {
            if ((*cached)->param_types.size() == sizeof...(Args)) {
                matched = *cached;
            }
        }
        
        if (!matched) [[unlikely]] {
            // Cache miss - look up method (no allocation with transparent hash)
            const auto* overloads = info_->find_methods(name);
            if (!overloads) [[unlikely]] {
                throw MethodNotFoundError(info_->name, name, info_->method_names());
            }
            
            // Find matching overload by parameter count
            for (const auto& method : *overloads) {
                if (method.param_types.size() == sizeof...(Args)) {
                    matched = &method;
                    break;
                }
            }
            
            if (!matched) [[unlikely]] {
                std::string expected = build_signature(*overloads);
                std::string actual = build_arg_signature<Args...>();
                throw MethodSignatureMismatchError(name, expected, actual);
            }
            
            // Cache for future calls
            method_cache_.insert(name_hash, matched);
        }
        
        // Build argument array
        if constexpr (sizeof...(Args) == 0) {
            std::span<std::any> empty_args;
            std::any result = matched->call(instance_.get(), empty_args);
            
            if constexpr (std::is_void_v<R>) {
                return;
            } else {
                return std::any_cast<R>(result);
            }
        } else {
            std::array<std::any, sizeof...(Args)> arg_array = {std::any{std::forward<Args>(args)}...};
            std::span<std::any> arg_span{arg_array};
            std::any result = matched->call(instance_.get(), arg_span);
            
            if constexpr (std::is_void_v<R>) {
                return;
            } else {
                return std::any_cast<R>(result);
            }
        }
    }
    
    /**
     * @brief Invoke a void method
     * 
     * Convenience overload for void methods.
     * 
     * @tparam Args The argument types
     * @param name The method name
     * @param args The method arguments
     */
    template<typename... Args>
    void invoke_void(std::string_view name, Args&&... args) {
        invoke<void>(name, std::forward<Args>(args)...);
    }


    // ==================== Type Information Methods ====================
    
    /**
     * @brief Get the type name
     * @return The registered type name
     */
    [[nodiscard]] std::string_view type_name() const {
        static const std::string empty_string;
        return info_ ? info_->name : std::string_view(empty_string);
    }
    
    /**
     * @brief Check if this RType is valid (has type info)
     * @return true if type info is available
     */
    [[nodiscard]] bool is_valid() const {
        return info_ != nullptr;
    }
    
    /**
     * @brief Check if an instance has been created/attached
     * @return true if an instance is available
     */
    [[nodiscard]] bool has_instance() const {
        return created_ && instance_ != nullptr;
    }
    
    /**
     * @brief Check if the type is a class type
     * @return true if this is a class type
     */
    [[nodiscard]] bool is_class() const {
        return info_ != nullptr;  // All registered types are classes
    }
    
    /**
     * @brief Check if a property exists
     * @param name The property name
     * @return true if the property exists
     */
    [[nodiscard]] bool has_property(std::string_view name) const {
        return info_ && info_->has_member(name);
    }
    
    /**
     * @brief Check if a method exists
     * @param name The method name
     * @return true if the method exists
     */
    [[nodiscard]] bool has_method(std::string_view name) const {
        return info_ && info_->has_method(name);
    }
    
    // ==================== Enumeration Methods ====================
    
    /**
     * @brief Get all property names (cached, no allocation on repeated calls)
     * @return Reference to vector of property names
     */
    [[nodiscard]] const std::vector<std::string_view>& property_names() const {
        static const std::vector<std::string_view> empty;
        if (!info_) return empty;
        return info_->member_names();
    }
    
    /**
     * @brief Get all method names (cached, no allocation on repeated calls)
     * @return Reference to vector of method names
     */
    [[nodiscard]] const std::vector<std::string_view>& method_names() const {
        static const std::vector<std::string_view> empty;
        if (!info_) return empty;
        return info_->method_names();
    }
    
    /**
     * @brief Get property category
     * @param name The property name
     * @return The property category
     * @throws PropertyNotFoundError if property doesn't exist
     */
    [[nodiscard]] detail::MemberCategory property_category(std::string_view name) const {
        if (!info_) {
            throw ReflectionError("No type info available");
        }
        
        auto member_it = info_->members.find(std::string{name});
        if (member_it == info_->members.end()) {
            throw PropertyNotFoundError(info_->name, name, info_->member_names());
        }
        
        return member_it->second.category;
    }
    
    /**
     * @brief Get property type name
     * @param name The property name
     * @return The property type name
     * @throws PropertyNotFoundError if property doesn't exist
     */
    [[nodiscard]] std::string_view property_type_name(std::string_view name) const {
        if (!info_) {
            throw ReflectionError("No type info available");
        }
        
        auto member_it = info_->members.find(std::string{name});
        if (member_it == info_->members.end()) {
            throw PropertyNotFoundError(info_->name, name, info_->member_names());
        }
        
        return member_it->second.type_name;
    }


    // ==================== Raw Access Methods ====================
    
    /**
     * @brief Get raw pointer to the instance
     * 
     * Use with caution - for performance-critical paths only.
     * 
     * @return Raw pointer to the instance, or nullptr if not created
     */
    [[nodiscard]] void* raw() const {
        return instance_.get();
    }
    
    /**
     * @brief Cast to specific type
     * 
     * @tparam T The target type
     * @return Reference to the instance as type T
     * @throws ObjectNotCreatedError if no instance is attached
     * @throws ReflectionError if type doesn't match
     */
    template<typename T>
    T& as() {
        if (!created_ || !instance_) {
            throw ObjectNotCreatedError(info_ ? info_->name : "unknown");
        }
        
        // Verify type matches
        if (info_ && info_->type_index != std::type_index(typeid(T))) {
            throw ReflectionError("Type mismatch in as<T>()");
        }
        
        return *static_cast<T*>(instance_.get());
    }
    
    /**
     * @brief Const cast to specific type
     * 
     * @tparam T The target type
     * @return Const reference to the instance as type T
     */
    template<typename T>
    const T& as() const {
        return const_cast<RType*>(this)->as<T>();
    }
    
    /**
     * @brief Get the type info
     * @return Pointer to TypeInfo, or nullptr if not available
     */
    [[nodiscard]] const detail::TypeInfo* type_info() const {
        return info_;
    }
    
    /**
     * @brief Get the member info for a property
     * @param name The property name
     * @return Pointer to MemberInfo, or nullptr if not found
     */
    [[nodiscard]] const detail::MemberInfo* get_member_info(std::string_view name) const {
        if (!info_) return nullptr;
        auto it = info_->members.find(std::string{name});
        if (it == info_->members.end()) return nullptr;
        return &(it->second);
    }
    
    /**
     * @brief Attach raw type info and instance (for internal use)
     * 
     * Used by container implementations to create RType wrappers
     * for container elements.
     * 
     * @param info Pointer to TypeInfo (may be nullptr for unregistered types)
     * @param instance Shared pointer to the instance
     */
    void attach_raw(const detail::TypeInfo* info, std::shared_ptr<void> instance) {
        info_ = info;
        instance_ = std::move(instance);
        created_ = (instance_ != nullptr);
    }

private:
    const detail::TypeInfo* info_ = nullptr;
    std::shared_ptr<void> instance_;
    bool created_ = false;
    
    // Cached factory for fast object creation
    mutable const std::function<std::shared_ptr<void>()>* default_factory_ = nullptr;
    
    // Inline caches for hot path optimization (stack allocated, no heap)
    mutable detail::InlineCache<std::size_t, std::size_t, 8> prop_cache_;
    mutable detail::InlineCache<std::size_t, const detail::MethodInfo*, 4> method_cache_;
    
    /**
     * @brief Generate constructor signature string
     */
    template<typename... Args>
    static std::string generate_signature() {
        if constexpr (sizeof...(Args) == 0) {
            return "default";
        } else {
            std::string sig = "ctor(";
            bool first = true;
            ((sig += (first ? (first = false, "") : ",") + std::string{detail::type_name<Args>()}), ...);
            sig += ")";
            return sig;
        }
    }
    
    /**
     * @brief Build signature string from method overloads
     */
    static std::string build_signature(const std::vector<detail::MethodInfo>& overloads) {
        std::string result;
        for (const auto& method : overloads) {
            if (!result.empty()) result += " | ";
            result += "(";
            bool first = true;
            for (const auto& param : method.param_types) {
                if (!first) result += ", ";
                result += param.name();
                first = false;
            }
            result += ") -> " + method.return_type_name;
        }
        return result;
    }
    
    /**
     * @brief Build argument signature string
     */
    template<typename... Args>
    static std::string build_arg_signature() {
        if constexpr (sizeof...(Args) == 0) {
            return "()";
        } else {
            std::string sig = "(";
            bool first = true;
            ((sig += (first ? (first = false, "") : ", ") + std::string{detail::type_name<Args>()}), ...);
            sig += ")";
            return sig;
        }
    }
};

} // namespace rttm

#endif // RTTM_DETAIL_RTYPE_HPP
