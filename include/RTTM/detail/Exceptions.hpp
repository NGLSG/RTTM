/**
 * @file Exceptions.hpp
 * @brief Exception hierarchy for RTTM reflection library
 * 
 * This file defines all exception types used by the RTTM library:
 * - ReflectionError: Base exception class
 * - TypeNotRegisteredError: Thrown when a type is not registered
 * - PropertyNotFoundError: Thrown when a property is not found
 * - MethodSignatureMismatchError: Thrown when method signature doesn't match
 * - ObjectNotCreatedError: Thrown when object has not been created
 * - PropertyTypeMismatchError: Thrown for type mismatch during property access
 * - MethodNotFoundError: Thrown when a method is not found
 */

#ifndef RTTM_DETAIL_EXCEPTIONS_HPP
#define RTTM_DETAIL_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace rttm {

/**
 * @brief Base exception class for all reflection errors
 * 
 * All RTTM-specific exceptions derive from this class,
 * allowing users to catch all reflection-related errors
 * with a single catch block.
 */
class ReflectionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
    
    explicit ReflectionError(const std::string& message)
        : std::runtime_error(message) {}
    
    explicit ReflectionError(const char* message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when a type is not registered
 * 
 * This exception is thrown when attempting to access type information
 * for a type that has not been registered with the reflection system.
 * 
 * @code
 * try {
 *     auto rtype = RType::get("UnregisteredType");
 * } catch (const TypeNotRegisteredError& e) {
 *     std::cout << "Type not found: " << e.type_name() << std::endl;
 * }
 * @endcode
 */
class TypeNotRegisteredError : public ReflectionError {
public:
    explicit TypeNotRegisteredError(std::string_view type_name)
        : ReflectionError(format_message(type_name))
        , type_name_(type_name) {}
    
    /**
     * @brief Get the name of the unregistered type
     * @return The type name that was not found
     */
    [[nodiscard]] std::string_view type_name() const noexcept { 
        return type_name_; 
    }
    
private:
    static std::string format_message(std::string_view type_name) {
        return std::string{"Type '"} + std::string{type_name} + "' is not registered";
    }
    
    std::string type_name_;
};

/**
 * @brief Exception thrown when a property is not found
 * 
 * This exception includes the list of available properties
 * to help users identify the correct property name.
 * 
 * @code
 * try {
 *     auto value = rtype->property<int>("nonexistent");
 * } catch (const PropertyNotFoundError& e) {
 *     std::cout << "Property not found: " << e.property_name() << std::endl;
 *     std::cout << "Available: ";
 *     for (const auto& name : e.available_properties()) {
 *         std::cout << name << " ";
 *     }
 * }
 * @endcode
 */
class PropertyNotFoundError : public ReflectionError {
public:
    PropertyNotFoundError(std::string_view type_name, 
                          std::string_view prop_name,
                          const std::vector<std::string_view>& available)
        : ReflectionError(format_message(type_name, prop_name, available))
        , type_name_(type_name)
        , property_name_(prop_name)
        , available_properties_(available.begin(), available.end()) {}
    
    /**
     * @brief Get the type name where the property was not found
     * @return The type name
     */
    [[nodiscard]] std::string_view type_name() const noexcept { 
        return type_name_; 
    }
    
    /**
     * @brief Get the name of the property that was not found
     * @return The property name
     */
    [[nodiscard]] std::string_view property_name() const noexcept { 
        return property_name_; 
    }
    
    /**
     * @brief Get the list of available property names
     * @return Vector of available property names
     */
    [[nodiscard]] const std::vector<std::string>& available_properties() const noexcept { 
        return available_properties_; 
    }
    
private:
    static std::string format_message(std::string_view type, 
                                      std::string_view prop,
                                      const std::vector<std::string_view>& available) {
        std::string msg = "Property '" + std::string{prop} + "' not found in type '" + 
                          std::string{type} + "'. Available properties: [";
        bool first = true;
        for (const auto& name : available) {
            if (!first) msg += ", ";
            msg += std::string{name};
            first = false;
        }
        msg += "]";
        return msg;
    }
    
    std::string type_name_;
    std::string property_name_;
    std::vector<std::string> available_properties_;
};

/**
 * @brief Exception thrown when method signature doesn't match
 * 
 * This exception provides detailed information about the expected
 * and actual method signatures to help diagnose the mismatch.
 * 
 * @code
 * try {
 *     rtype->invoke<int>("method", "wrong", "args");
 * } catch (const MethodSignatureMismatchError& e) {
 *     std::cout << "Method: " << e.method_name() << std::endl;
 *     std::cout << "Expected: " << e.expected_signature() << std::endl;
 *     std::cout << "Actual: " << e.actual_signature() << std::endl;
 * }
 * @endcode
 */
class MethodSignatureMismatchError : public ReflectionError {
public:
    MethodSignatureMismatchError(std::string_view method_name,
                                  std::string_view expected,
                                  std::string_view actual)
        : ReflectionError(format_message(method_name, expected, actual))
        , method_name_(method_name)
        , expected_signature_(expected)
        , actual_signature_(actual) {}
    
    /**
     * @brief Get the method name
     * @return The method name
     */
    [[nodiscard]] std::string_view method_name() const noexcept { 
        return method_name_; 
    }
    
    /**
     * @brief Get the expected signature
     * @return The expected method signature
     */
    [[nodiscard]] std::string_view expected_signature() const noexcept { 
        return expected_signature_; 
    }
    
    /**
     * @brief Get the actual signature provided
     * @return The actual argument signature
     */
    [[nodiscard]] std::string_view actual_signature() const noexcept { 
        return actual_signature_; 
    }
    
private:
    static std::string format_message(std::string_view method_name,
                                      std::string_view expected,
                                      std::string_view actual) {
        return std::string{"Method '"} + std::string{method_name} + 
               "' signature mismatch: expected '" + std::string{expected} + 
               "', got '" + std::string{actual} + "'";
    }
    
    std::string method_name_;
    std::string expected_signature_;
    std::string actual_signature_;
};

/**
 * @brief Exception thrown when object has not been created
 * 
 * This exception is thrown when attempting to access properties
 * or invoke methods on an RType that doesn't have an attached instance.
 * 
 * @code
 * try {
 *     auto rtype = RType::get<MyClass>();
 *     // Forgot to call create()
 *     rtype->property<int>("value");  // Throws!
 * } catch (const ObjectNotCreatedError& e) {
 *     std::cout << "Object not created for type: " << e.type_name() << std::endl;
 * }
 * @endcode
 */
class ObjectNotCreatedError : public ReflectionError {
public:
    explicit ObjectNotCreatedError(std::string_view type_name)
        : ReflectionError(format_message(type_name))
        , type_name_(type_name) {}
    
    /**
     * @brief Get the type name
     * @return The type name of the uncreated object
     */
    [[nodiscard]] std::string_view type_name() const noexcept { 
        return type_name_; 
    }
    
private:
    static std::string format_message(std::string_view type_name) {
        return std::string{"Object of type '"} + std::string{type_name} + 
               "' has not been created";
    }
    
    std::string type_name_;
};

/**
 * @brief Exception thrown for type mismatch during property access
 * 
 * This exception is thrown when the requested property type
 * doesn't match the actual property type.
 * 
 * @code
 * try {
 *     // Property 'name' is std::string, not int
 *     auto& value = rtype->property<int>("name");
 * } catch (const PropertyTypeMismatchError& e) {
 *     std::cout << "Type mismatch for property: " << e.property_name() << std::endl;
 * }
 * @endcode
 */
class PropertyTypeMismatchError : public ReflectionError {
public:
    PropertyTypeMismatchError(std::string_view prop_name,
                               std::string_view expected_type,
                               std::string_view actual_type)
        : ReflectionError(format_message(prop_name, expected_type, actual_type))
        , property_name_(prop_name)
        , expected_type_(expected_type)
        , actual_type_(actual_type) {}
    
    /**
     * @brief Get the property name
     * @return The property name
     */
    [[nodiscard]] std::string_view property_name() const noexcept { 
        return property_name_; 
    }
    
    /**
     * @brief Get the expected type name
     * @return The expected type name
     */
    [[nodiscard]] std::string_view expected_type() const noexcept { 
        return expected_type_; 
    }
    
    /**
     * @brief Get the actual type name
     * @return The actual type name
     */
    [[nodiscard]] std::string_view actual_type() const noexcept { 
        return actual_type_; 
    }
    
private:
    static std::string format_message(std::string_view prop_name,
                                      std::string_view expected_type,
                                      std::string_view actual_type) {
        return std::string{"Property '"} + std::string{prop_name} + 
               "' type mismatch: expected '" + std::string{expected_type} + 
               "', got '" + std::string{actual_type} + "'";
    }
    
    std::string property_name_;
    std::string expected_type_;
    std::string actual_type_;
};

/**
 * @brief Exception thrown when a method is not found
 * 
 * This exception includes the list of available methods
 * to help users identify the correct method name.
 * 
 * @code
 * try {
 *     rtype->invoke<void>("nonexistentMethod");
 * } catch (const MethodNotFoundError& e) {
 *     std::cout << "Method not found: " << e.method_name() << std::endl;
 *     std::cout << "Available methods: ";
 *     for (const auto& name : e.available_methods()) {
 *         std::cout << name << " ";
 *     }
 * }
 * @endcode
 */
class MethodNotFoundError : public ReflectionError {
public:
    MethodNotFoundError(std::string_view type_name, 
                        std::string_view method_name,
                        const std::vector<std::string_view>& available)
        : ReflectionError(format_message(type_name, method_name, available))
        , type_name_(type_name)
        , method_name_(method_name)
        , available_methods_(available.begin(), available.end()) {}
    
    /**
     * @brief Get the type name
     * @return The type name
     */
    [[nodiscard]] std::string_view type_name() const noexcept { 
        return type_name_; 
    }
    
    /**
     * @brief Get the method name that was not found
     * @return The method name
     */
    [[nodiscard]] std::string_view method_name() const noexcept { 
        return method_name_; 
    }
    
    /**
     * @brief Get the list of available method names
     * @return Vector of available method names
     */
    [[nodiscard]] const std::vector<std::string>& available_methods() const noexcept { 
        return available_methods_; 
    }
    
private:
    static std::string format_message(std::string_view type, 
                                      std::string_view method,
                                      const std::vector<std::string_view>& available) {
        std::string msg = "Method '" + std::string{method} + "' not found in type '" + 
                          std::string{type} + "'. Available methods: [";
        bool first = true;
        for (const auto& name : available) {
            if (!first) msg += ", ";
            msg += std::string{name};
            first = false;
        }
        msg += "]";
        return msg;
    }
    
    std::string type_name_;
    std::string method_name_;
    std::vector<std::string> available_methods_;
};

} // namespace rttm

#endif // RTTM_DETAIL_EXCEPTIONS_HPP
