/**
 * @file Instance.cpp
 * @brief Implementation of Instance class for pure dynamic reflection
 */

#include "RTTM/detail/Instance.hpp"
#include "RTTM/detail/TypeManager.hpp"

namespace rttm {

// ============================================================================
// DynamicProperty implementation
// ============================================================================

Variant DynamicProperty::get_value(void* obj) const {
    if (!member_) [[unlikely]] return Variant{};
    
    void* prop_ptr = static_cast<char*>(obj) + member_->offset;
    
    // Fast path for common types
    if (member_->type_index == typeid(int)) {
        return Variant::create(*static_cast<int*>(prop_ptr));
    }
    if (member_->type_index == typeid(float)) {
        return Variant::create(*static_cast<float*>(prop_ptr));
    }
    if (member_->type_index == typeid(double)) {
        return Variant::create(*static_cast<double*>(prop_ptr));
    }
    if (member_->type_index == typeid(bool)) {
        return Variant::create(*static_cast<bool*>(prop_ptr));
    }
    if (member_->type_index == typeid(long)) {
        return Variant::create(*static_cast<long*>(prop_ptr));
    }
    if (member_->type_index == typeid(std::string)) {
        return Variant::create(*static_cast<std::string*>(prop_ptr));
    }
    
    throw ReflectionError("Unsupported property type: " + member_->type_name);
}

void DynamicProperty::set_value(void* obj, const Variant& value) const {
    if (!member_) [[unlikely]] return;
    
    void* prop_ptr = static_cast<char*>(obj) + member_->offset;
    auto ti = member_->type_index;
    
    // Fast path for common types
    if (ti == typeid(int)) {
        if (value.is_type<int>()) {
            *static_cast<int*>(prop_ptr) = value.get_unchecked<int>();
        } else if (value.can_convert<int>()) {
            *static_cast<int*>(prop_ptr) = value.convert<int>();
        }
        return;
    }
    if (ti == typeid(float)) {
        if (value.is_type<float>()) {
            *static_cast<float*>(prop_ptr) = value.get_unchecked<float>();
        } else if (value.can_convert<float>()) {
            *static_cast<float*>(prop_ptr) = value.convert<float>();
        }
        return;
    }
    if (ti == typeid(double)) {
        if (value.is_type<double>()) {
            *static_cast<double*>(prop_ptr) = value.get_unchecked<double>();
        } else if (value.can_convert<double>()) {
            *static_cast<double*>(prop_ptr) = value.convert<double>();
        }
        return;
    }
    if (ti == typeid(bool)) {
        if (value.is_type<bool>()) {
            *static_cast<bool*>(prop_ptr) = value.get_unchecked<bool>();
        }
        return;
    }
    if (ti == typeid(std::string)) {
        *static_cast<std::string*>(prop_ptr) = value.get<std::string>();
        return;
    }
    
    throw ReflectionError("Unsupported property type: " + member_->type_name);
}

// ============================================================================
// DynamicMethod implementation
// ============================================================================

Variant DynamicMethod::invoke(void* obj, std::span<const Variant> args) const {
    if (!method_) [[unlikely]] return Variant{};
    
    // Fast path: use variant_invoker if available
    if (method_->variant_invoker) {
        Variant result;
        
        if (args.empty()) {
            method_->variant_invoker(obj, &result, nullptr, 0, method_->method_ptr);
        } else if (args.size() <= 8) {
            std::array<const void*, 8> arg_ptrs;
            for (std::size_t i = 0; i < args.size(); ++i) {
                arg_ptrs[i] = &args[i];
            }
            method_->variant_invoker(obj, &result, arg_ptrs.data(), args.size(), method_->method_ptr);
        } else {
            std::vector<const void*> arg_ptrs;
            arg_ptrs.reserve(args.size());
            for (const auto& v : args) {
                arg_ptrs.push_back(&v);
            }
            method_->variant_invoker(obj, &result, arg_ptrs.data(), args.size(), method_->method_ptr);
        }
        
        return result;
    }
    
    // Fallback to std::any path
    if (args.empty()) {
        std::span<std::any> empty_args;
        std::any result = method_->call(obj, empty_args);
        return Instance::convert_result(result, method_->return_type);
    }
    
    // Convert Variant args to std::any
    if (args.size() <= 8) {
        std::array<std::any, 8> any_args;
        for (std::size_t i = 0; i < args.size(); ++i) {
            any_args[i] = Instance::variant_to_any(args[i]);
        }
        std::span<std::any> arg_span{any_args.data(), args.size()};
        std::any result = method_->call(obj, arg_span);
        return Instance::convert_result(result, method_->return_type);
    }
    
    std::vector<std::any> any_args;
    any_args.reserve(args.size());
    for (const auto& v : args) {
        any_args.push_back(Instance::variant_to_any(v));
    }
    std::span<std::any> arg_span{any_args};
    std::any result = method_->call(obj, arg_span);
    return Instance::convert_result(result, method_->return_type);
}

Instance Instance::create(std::string_view type_name) {
    auto& mgr = detail::TypeManager::instance();
    const detail::TypeInfo* info = mgr.get_type(type_name);
    
    if (!info) [[unlikely]] {
        throw TypeNotRegisteredError(type_name);
    }
    
    // Try raw factory first (fastest)
    std::shared_ptr<void> obj;
    if (info->default_factory_raw) [[likely]] {
        obj = info->default_factory_raw();
    } else {
        auto it = info->factories.find("default");
        if (it != info->factories.end() && it->second) {
            obj = it->second();
        }
    }
    
    if (!obj) [[unlikely]] {
        throw ReflectionError("Failed to create instance of type: " + std::string(type_name));
    }
    
    return Instance(std::move(obj), nullptr, info);
}

Instance Instance::from_owned(std::shared_ptr<void> obj, const detail::TypeInfo* type_info) {
    return Instance(std::move(obj), nullptr, type_info);
}

Instance Instance::from_ref(void* obj, const detail::TypeInfo* type_info) {
    return Instance(nullptr, obj, type_info);
}

Variant Instance::get_property(std::string_view name) const {
    if (!is_valid()) [[unlikely]] {
        throw ObjectNotCreatedError(type_name().empty() ? "unknown" : std::string(type_name()));
    }
    
    const detail::MemberInfo* member = type_info_->find_member(name);
    if (!member) [[unlikely]] {
        throw PropertyNotFoundError(std::string(type_name()), name, type_info_->member_names());
    }
    
    void* obj_ptr = const_cast<void*>(get_raw());
    void* prop_ptr = static_cast<char*>(obj_ptr) + member->offset;
    auto ti = member->type_index;
    
    // Direct type checks (faster than map lookup)
    if (ti == typeid(int)) return Variant::create(*static_cast<int*>(prop_ptr));
    if (ti == typeid(float)) return Variant::create(*static_cast<float*>(prop_ptr));
    if (ti == typeid(double)) return Variant::create(*static_cast<double*>(prop_ptr));
    if (ti == typeid(bool)) return Variant::create(*static_cast<bool*>(prop_ptr));
    if (ti == typeid(long)) return Variant::create(*static_cast<long*>(prop_ptr));
    if (ti == typeid(long long)) return Variant::create(*static_cast<long long*>(prop_ptr));
    if (ti == typeid(std::string)) return Variant::create(*static_cast<std::string*>(prop_ptr));
    
    throw ReflectionError("Unsupported property type: " + member->type_name);
}

void Instance::set_property(std::string_view name, const Variant& value) {
    if (!is_valid()) [[unlikely]] {
        throw ObjectNotCreatedError(type_name().empty() ? "unknown" : std::string(type_name()));
    }
    
    const detail::MemberInfo* member = type_info_->find_member(name);
    if (!member) [[unlikely]] {
        throw PropertyNotFoundError(std::string(type_name()), name, type_info_->member_names());
    }
    
    void* obj_ptr = const_cast<void*>(get_raw());
    void* prop_ptr = static_cast<char*>(obj_ptr) + member->offset;
    auto ti = member->type_index;
    
    // Direct type checks (faster than map lookup)
    if (ti == typeid(int)) {
        if (value.is_type<int>()) [[likely]] {
            *static_cast<int*>(prop_ptr) = value.get_unchecked<int>();
        } else {
            *static_cast<int*>(prop_ptr) = value.convert<int>();
        }
        return;
    }
    if (ti == typeid(float)) {
        if (value.is_type<float>()) [[likely]] {
            *static_cast<float*>(prop_ptr) = value.get_unchecked<float>();
        } else {
            *static_cast<float*>(prop_ptr) = value.convert<float>();
        }
        return;
    }
    if (ti == typeid(double)) {
        if (value.is_type<double>()) [[likely]] {
            *static_cast<double*>(prop_ptr) = value.get_unchecked<double>();
        } else {
            *static_cast<double*>(prop_ptr) = value.convert<double>();
        }
        return;
    }
    if (ti == typeid(bool)) {
        *static_cast<bool*>(prop_ptr) = value.is_type<bool>() ? value.get_unchecked<bool>() : value.convert<bool>();
        return;
    }
    if (ti == typeid(long)) {
        *static_cast<long*>(prop_ptr) = value.is_type<long>() ? value.get_unchecked<long>() : value.convert<long>();
        return;
    }
    if (ti == typeid(long long)) {
        *static_cast<long long*>(prop_ptr) = value.is_type<long long>() ? value.get_unchecked<long long>() : value.convert<long long>();
        return;
    }
    if (ti == typeid(std::string)) {
        *static_cast<std::string*>(prop_ptr) = value.get<std::string>();
        return;
    }
    
    throw ReflectionError("Unsupported property type: " + member->type_name);
}

// Optimized invoke: use variant_invoker when available
Variant Instance::invoke(std::string_view name, std::span<const Variant> args) const {
    if (!is_valid()) [[unlikely]] {
        throw ObjectNotCreatedError(type_name().empty() ? "unknown" : std::string(type_name()));
    }
    
    const auto* method_list = type_info_->find_methods(name);
    if (!method_list) [[unlikely]] {
        throw MethodNotFoundError(std::string(type_name()), name, type_info_->method_names());
    }
    
    // Find overload with matching parameter count
    const detail::MethodInfo* matched = nullptr;
    for (const auto& method : *method_list) {
        if (method.param_types.size() == args.size()) {
            matched = &method;
            break;
        }
    }
    
    if (!matched) [[unlikely]] {
        throw MethodSignatureMismatchError(name, "expected " + std::to_string(args.size()) + " args", 
                                           "no matching overload");
    }
    
    void* obj_ptr = const_cast<void*>(get_raw());
    
    // Fast path: use variant_invoker if available (avoids std::any conversion)
    if (matched->variant_invoker) [[likely]] {
        Variant result;
        
        if (args.empty()) {
            matched->variant_invoker(obj_ptr, &result, nullptr, 0, matched->method_ptr);
        } else if (args.size() <= 8) {
            // Use stack array for small arg counts
            std::array<const void*, 8> arg_ptrs;
            for (std::size_t i = 0; i < args.size(); ++i) {
                arg_ptrs[i] = &args[i];
            }
            matched->variant_invoker(obj_ptr, &result, arg_ptrs.data(), args.size(), matched->method_ptr);
        } else {
            // Fallback for many args
            std::vector<const void*> arg_ptrs;
            arg_ptrs.reserve(args.size());
            for (const auto& v : args) {
                arg_ptrs.push_back(&v);
            }
            matched->variant_invoker(obj_ptr, &result, arg_ptrs.data(), args.size(), matched->method_ptr);
        }
        
        return result;
    }
    
    // Fallback: use std::any path
    // Fast path for 0 args
    if (args.empty()) {
        std::span<std::any> empty_args;
        std::any result = matched->call(obj_ptr, empty_args);
        return convert_result(result, matched->return_type);
    }
    
    // Fast path for 1 arg (most common case)
    if (args.size() == 1) {
        std::any arg = variant_to_any(args[0]);
        std::array<std::any, 1> arg_array = {std::move(arg)};
        std::span<std::any> arg_span{arg_array};
        std::any result = matched->call(obj_ptr, arg_span);
        return convert_result(result, matched->return_type);
    }
    
    // General case: use stack array for small arg counts
    if (args.size() <= 8) {
        std::array<std::any, 8> arg_array;
        for (std::size_t i = 0; i < args.size(); ++i) {
            arg_array[i] = variant_to_any(args[i]);
        }
        std::span<std::any> arg_span{arg_array.data(), args.size()};
        std::any result = matched->call(obj_ptr, arg_span);
        return convert_result(result, matched->return_type);
    }
    
    // Fallback for many args
    std::vector<std::any> any_args;
    any_args.reserve(args.size());
    for (const auto& v : args) {
        any_args.push_back(variant_to_any(v));
    }
    std::span<std::any> arg_span{any_args};
    std::any result = matched->call(obj_ptr, arg_span);
    return convert_result(result, matched->return_type);
}

std::any Instance::variant_to_any(const Variant& v) {
    auto ti = v.type();
    
    // Use direct type checks for common types
    if (ti == typeid(int)) return std::any{v.get_unchecked<int>()};
    if (ti == typeid(float)) return std::any{v.get_unchecked<float>()};
    if (ti == typeid(double)) return std::any{v.get_unchecked<double>()};
    if (ti == typeid(bool)) return std::any{v.get_unchecked<bool>()};
    if (ti == typeid(long)) return std::any{v.get_unchecked<long>()};
    if (ti == typeid(long long)) return std::any{v.get_unchecked<long long>()};
    if (ti == typeid(std::string)) return std::any{v.get_unchecked<std::string>()};
    
    throw ReflectionError("Unsupported argument type for dynamic invoke");
}

Variant Instance::convert_result(const std::any& result, std::type_index return_type) {
    if (!result.has_value() || return_type == typeid(void)) {
        return Variant{};
    }
    
    // Use direct type checks
    if (return_type == typeid(int)) return Variant::create(std::any_cast<int>(result));
    if (return_type == typeid(float)) return Variant::create(std::any_cast<float>(result));
    if (return_type == typeid(double)) return Variant::create(std::any_cast<double>(result));
    if (return_type == typeid(bool)) return Variant::create(std::any_cast<bool>(result));
    if (return_type == typeid(long)) return Variant::create(std::any_cast<long>(result));
    if (return_type == typeid(long long)) return Variant::create(std::any_cast<long long>(result));
    if (return_type == typeid(std::string)) return Variant::create(std::any_cast<std::string>(result));
    
    throw ReflectionError("Unsupported return type for dynamic invoke");
}

// Explicit template instantiations
template Instance Instance::from_ref<int>(int&);
template Instance Instance::from_ref<float>(float&);
template Instance Instance::from_ref<double>(double&);
template Instance Instance::from_ref<bool>(bool&);
template Instance Instance::from_ref<std::string>(std::string&);

template<typename T>
Instance Instance::from_ref(T& obj) {
    auto& mgr = detail::TypeManager::instance();
    const detail::TypeInfo* info = mgr.get_type_by_id(detail::type_id<T>);
    if (!info) {
        info = mgr.get_type(detail::type_name<T>());
    }
    return Instance(nullptr, static_cast<void*>(&obj), info);
}

template<typename T>
Instance Instance::from_owned(std::shared_ptr<T> obj) {
    auto& mgr = detail::TypeManager::instance();
    const detail::TypeInfo* info = mgr.get_type_by_id(detail::type_id<T>);
    if (!info) {
        info = mgr.get_type(detail::type_name<T>());
    }
    return Instance(std::static_pointer_cast<void>(obj), nullptr, info);
}

} // namespace rttm
