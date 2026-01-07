/**
 * @file Registry.hpp
 * @brief Type registration API for RTTM reflection library
 * 
 * This file implements the Registry<T> class which provides a fluent interface
 * for registering type information including:
 * - Properties (member variables)
 * - Methods (member functions)
 * - Constructors
 * - Base class relationships
 */

#ifndef RTTM_DETAIL_REGISTRY_HPP
#define RTTM_DETAIL_REGISTRY_HPP

#include "TypeInfo.hpp"
#include "TypeManager.hpp"
#include "TypeTraits.hpp"

#include <string_view>
#include <type_traits>
#include <functional>
#include <memory>
#include <any>
#include <span>
#include <cstddef>

namespace rttm {

/**
 * @brief Type registration class with fluent interface
 * 
 * Registry<T> provides a chainable API for registering type metadata.
 * Upon construction, it automatically:
 * - Creates TypeInfo for the type
 * - Registers default constructor if available
 * - Sets up destructor and copier functions
 * 
 * @tparam T The type to register (must satisfy Reflectable concept)
 * 
 * Usage:
 * @code
 * Registry<MyClass>()
 *     .property("name", &MyClass::name)
 *     .property("age", &MyClass::age)
 *     .method("getName", &MyClass::getName)
 *     .constructor<int, std::string>();
 * @endcode
 */
template<Reflectable T>
class Registry {
public:
    /**
     * @brief Construct a Registry and initialize type information
     * 
     * Automatically registers:
     * - Type name, size, and type_index
     * - Default constructor (if T is default constructible)
     * - Destructor function
     * - Copy function (if T is copy constructible)
     */
    Registry() {
        // Get type name at compile time
        type_name_ = std::string{detail::type_name<T>()};
        
        // Check if already registered
        auto& mgr = detail::TypeManager::instance();
        if (mgr.is_registered(type_name_)) {
            // Already registered - get existing info for potential updates
            info_ = mgr.get_type_mutable(type_name_);
            return;
        }
        
        // Create new TypeInfo
        detail::TypeInfo new_info{
            type_name_,
            sizeof(T),
            std::type_index(typeid(T))
        };
        
        // Set up destructor
        new_info.destructor = [](void* ptr) {
            static_cast<T*>(ptr)->~T();
        };
        
        // Set up copier if copy constructible
        if constexpr (CopyConstructible<T>) {
            new_info.copier = [](void* dest, const void* src) {
                new (dest) T(*static_cast<const T*>(src));
            };
        }
        
        // Auto-register default constructor if available
        if constexpr (DefaultConstructible<T>) {
            new_info.factories["default"] = []() -> std::shared_ptr<void> {
                return std::make_shared<T>();
            };
            // Set raw factory pointer for fast path
            new_info.default_factory_raw = &default_factory_impl;
        }
        
        // Register the type with TypeId for fast lookup
        mgr.register_type(type_name_, std::move(new_info), detail::type_id<T>);
        info_ = mgr.get_type_mutable(type_name_);
    }

    /**
     * @brief Register a property (member variable)
     * 
     * Calculates the member offset and detects the member type category.
     * Supports chainable calls.
     * 
     * @tparam U The member type
     * @param name The property name
     * @param member Pointer to member
     * @return Reference to this Registry for chaining
     */
    template<typename U>
    Registry& property(std::string_view name, U T::* member) {
        if (!info_) return *this;
        
        // Calculate offset using offsetof-like technique
        // We use a null pointer cast to calculate the offset
        std::size_t offset = reinterpret_cast<std::size_t>(
            &(static_cast<T*>(nullptr)->*member)
        );
        
        // Determine member category
        detail::MemberCategory category = detect_category<U>();
        
        // Get type name
        std::string member_type_name{detail::type_name<U>()};
        
        // Create and store member info
        detail::MemberInfo member_info{
            name,
            offset,
            std::type_index(typeid(U)),
            member_type_name,
            category
        };
        
        info_->members[std::string{name}] = std::move(member_info);
        info_->invalidate_name_cache();  // Invalidate cached name list
        
        return *this;
    }
    
    /**
     * @brief Register a non-const member method
     * 
     * Creates a type-erased invoker that handles argument conversion
     * and return value wrapping.
     * 
     * @tparam R Return type
     * @tparam Args Parameter types
     * @param name The method name
     * @param func Pointer to member function
     * @return Reference to this Registry for chaining
     */
    template<typename R, typename... Args>
    Registry& method(std::string_view name, R(T::*func)(Args...)) {
        if (!info_) return *this;
        
        // Create type-erased invoker
        auto invoker = [func](void* obj, std::span<std::any> args) -> std::any {
            return invoke_method<R, Args...>(static_cast<T*>(obj), func, args, 
                                             std::index_sequence_for<Args...>{});
        };
        
        // Build parameter type list
        std::vector<std::type_index> param_types;
        (param_types.push_back(std::type_index(typeid(Args))), ...);
        
        // Get return type name
        std::string return_type_name{detail::type_name<R>()};
        
        // Create method info
        detail::MethodInfo method_info{
            name,
            std::move(invoker),
            std::move(param_types),
            std::type_index(typeid(R)),
            return_type_name,
            false  // non-const
        };
        
        // Add to methods map (supports overloading)
        info_->methods[std::string{name}].push_back(std::move(method_info));
        info_->invalidate_name_cache();  // Invalidate cached name list
        
        return *this;
    }
    
    /**
     * @brief Register a const member method
     * 
     * @tparam R Return type
     * @tparam Args Parameter types
     * @param name The method name
     * @param func Pointer to const member function
     * @return Reference to this Registry for chaining
     */
    template<typename R, typename... Args>
    Registry& method(std::string_view name, R(T::*func)(Args...) const) {
        if (!info_) return *this;
        
        // Create type-erased invoker for const method
        auto invoker = [func](void* obj, std::span<std::any> args) -> std::any {
            return invoke_const_method<R, Args...>(static_cast<T*>(obj), func, args,
                                                   std::index_sequence_for<Args...>{});
        };
        
        // Build parameter type list
        std::vector<std::type_index> param_types;
        (param_types.push_back(std::type_index(typeid(Args))), ...);
        
        // Get return type name
        std::string return_type_name{detail::type_name<R>()};
        
        // Create method info
        detail::MethodInfo method_info{
            name,
            std::move(invoker),
            std::move(param_types),
            std::type_index(typeid(R)),
            return_type_name,
            true  // const method
        };
        
        // Add to methods map
        info_->methods[std::string{name}].push_back(std::move(method_info));
        info_->invalidate_name_cache();  // Invalidate cached name list
        
        return *this;
    }

    /**
     * @brief Register a constructor with specific parameters
     * 
     * Creates a factory function that can construct objects with
     * the specified argument types.
     * 
     * @tparam Args Constructor parameter types
     * @return Reference to this Registry for chaining
     */
    template<typename... Args>
    Registry& constructor() {
        if (!info_) return *this;
        
        // Generate a signature key for this constructor
        std::string signature = generate_constructor_signature<Args...>();
        
        // Create factory function
        if constexpr (sizeof...(Args) == 0) {
            // Default constructor - may already be registered
            if (info_->factories.find("default") == info_->factories.end()) {
                info_->factories["default"] = []() -> std::shared_ptr<void> {
                    return std::make_shared<T>();
                };
            }
        } else {
            // Store factory with args - we need a way to pass args
            // For now, store a marker that this constructor exists
            // The actual construction will be handled by RType::create
            info_->factories[signature] = []() -> std::shared_ptr<void> {
                // This is a placeholder - actual construction with args
                // is handled differently through RType
                return nullptr;
            };
        }
        
        return *this;
    }
    
    /**
     * @brief Declare a base class relationship
     * 
     * Merges base class properties and methods into this type's info.
     * Supports multiple levels of inheritance.
     * 
     * @tparam Base The base class type
     * @return Reference to this Registry for chaining
     */
    template<typename Base>
    requires std::is_base_of_v<Base, T>
    Registry& base() {
        if (!info_) return *this;
        
        // Record base type
        info_->base_types.push_back(std::type_index(typeid(Base)));
        
        // Get base type info
        auto& mgr = detail::TypeManager::instance();
        std::string base_name{detail::type_name<Base>()};
        const detail::TypeInfo* base_info = mgr.get_type(base_name);
        
        if (base_info) {
            // Merge base class members (with offset adjustment)
            for (const auto& [name, member] : base_info->members) {
                if (info_->members.find(name) == info_->members.end()) {
                    // Calculate adjusted offset for base member in derived class
                    // The offset is relative to the base subobject
                    detail::MemberInfo adjusted_member = member;
                    // Base offset in derived is 0 for single inheritance
                    // For multiple inheritance, we'd need to calculate the base offset
                    info_->members[name] = std::move(adjusted_member);
                }
            }
            
            // Merge base class methods
            for (const auto& [name, method_list] : base_info->methods) {
                auto& derived_methods = info_->methods[name];
                for (const auto& method : method_list) {
                    // Check if method is already overridden
                    bool overridden = false;
                    for (const auto& existing : derived_methods) {
                        if (existing.param_types == method.param_types) {
                            overridden = true;
                            break;
                        }
                    }
                    if (!overridden) {
                        derived_methods.push_back(method);
                    }
                }
            }
            
            // Recursively merge base's base types
            for (const auto& base_base : base_info->base_types) {
                if (std::find(info_->base_types.begin(), info_->base_types.end(), base_base) 
                    == info_->base_types.end()) {
                    info_->base_types.push_back(base_base);
                }
            }
        }
        
        return *this;
    }

private:
    std::string type_name_;
    detail::TypeInfo* info_ = nullptr;
    
    /**
     * @brief Detect the category of a member type
     */
    template<typename U>
    static constexpr detail::MemberCategory detect_category() {
        if constexpr (std::is_enum_v<U>) {
            return detail::MemberCategory::Enum;
        } else if constexpr (detail::is_sequential_container_v<U>) {
            return detail::MemberCategory::Sequential;
        } else if constexpr (detail::is_associative_container_v<U>) {
            return detail::MemberCategory::Associative;
        } else if constexpr (std::is_arithmetic_v<U>) {
            return detail::MemberCategory::Primitive;
        } else if constexpr (std::is_class_v<U>) {
            return detail::MemberCategory::Class;
        } else {
            return detail::MemberCategory::Primitive;
        }
    }

    /**
     * @brief Generate a signature string for a constructor
     */
    template<typename... Args>
    static std::string generate_constructor_signature() {
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
     * @brief Helper to convert std::any argument with automatic type conversion
     * @throws std::bad_any_cast if conversion fails
     */
    template<typename Target>
    static Target convert_arg(const std::any& arg, std::size_t arg_index = 0) {
        try {
            // Try direct conversion first
            if (arg.type() == typeid(Target)) {
                return std::any_cast<Target>(arg);
            }
            
            // Handle const char* to std::string conversion
            if constexpr (std::is_same_v<Target, std::string>) {
                if (arg.type() == typeid(const char*)) {
                    return std::string{std::any_cast<const char*>(arg)};
                }
                if (arg.type() == typeid(char*)) {
                    return std::string{std::any_cast<char*>(arg)};
                }
            }
            
            // Handle numeric conversions
            if constexpr (std::is_arithmetic_v<Target>) {
                if (arg.type() == typeid(int)) {
                    return static_cast<Target>(std::any_cast<int>(arg));
                }
                if (arg.type() == typeid(long)) {
                    return static_cast<Target>(std::any_cast<long>(arg));
                }
                if (arg.type() == typeid(double)) {
                    return static_cast<Target>(std::any_cast<double>(arg));
                }
                if (arg.type() == typeid(float)) {
                    return static_cast<Target>(std::any_cast<float>(arg));
                }
            }
            
            // Fall back to direct cast (may throw)
            return std::any_cast<Target>(arg);
        } catch (const std::bad_any_cast&) {
            // Re-throw with more context - the caller will handle this
            throw;
        }
    }
    
    /**
     * @brief Invoke a non-const method with arguments from span
     */
    template<typename R, typename... Args, std::size_t... Is>
    static std::any invoke_method(T* obj, R(T::*func)(Args...), 
                                  std::span<std::any> args,
                                  std::index_sequence<Is...>) {
        if constexpr (std::is_void_v<R>) {
            (obj->*func)(convert_arg<Args>(args[Is])...);
            return std::any{};
        } else {
            return std::any{(obj->*func)(convert_arg<Args>(args[Is])...)};
        }
    }
    
    /**
     * @brief Invoke a const method with arguments from span
     */
    template<typename R, typename... Args, std::size_t... Is>
    static std::any invoke_const_method(T* obj, R(T::*func)(Args...) const,
                                        std::span<std::any> args,
                                        std::index_sequence<Is...>) {
        if constexpr (std::is_void_v<R>) {
            (obj->*func)(convert_arg<Args>(args[Is])...);
            return std::any{};
        } else {
            return std::any{(obj->*func)(convert_arg<Args>(args[Is])...)};
        }
    }
    
    /**
     * @brief Static default factory implementation for raw function pointer
     */
    static std::shared_ptr<void> default_factory_impl() {
        return std::make_shared<T>();
    }
};

} // namespace rttm

#endif // RTTM_DETAIL_REGISTRY_HPP
