/**
 * @file TypeTraits.hpp
 * @brief C++20 concepts and type traits for RTTM reflection library
 * 
 * This file defines the core concepts used throughout RTTM to constrain
 * template parameters and provide compile-time type checking.
 */

#ifndef RTTM_DETAIL_TYPE_TRAITS_HPP
#define RTTM_DETAIL_TYPE_TRAITS_HPP

#include <concepts>
#include <type_traits>
#include <string>
#include <string_view>
#include <iterator>

namespace rttm {

/**
 * @brief Concept for types that can be reflected
 * 
 * A type is Reflectable if it is a class type and not abstract.
 * This ensures we can create instances and access members.
 * 
 * @tparam T The type to check
 */
template<typename T>
concept Reflectable = std::is_class_v<T> && !std::is_abstract_v<T>;

/**
 * @brief Concept for default constructible types
 * 
 * Used to determine if automatic default constructor registration is possible.
 * 
 * @tparam T The type to check
 */
template<typename T>
concept DefaultConstructible = std::is_default_constructible_v<T>;

/**
 * @brief Concept for copy constructible types
 * 
 * @tparam T The type to check
 */
template<typename T>
concept CopyConstructible = std::is_copy_constructible_v<T>;

/**
 * @brief Concept for move constructible types
 * 
 * @tparam T The type to check
 */
template<typename T>
concept MoveConstructible = std::is_move_constructible_v<T>;

/**
 * @brief Concept for sequential containers (vector, list, deque, etc.)
 * 
 * A sequential container must support:
 * - value_type and iterator type aliases
 * - size(), empty(), begin(), end() methods
 * - push_back() method for adding elements
 * - pop_back() method for removing elements
 * 
 * This matches std::vector, std::list, std::deque.
 * 
 * @tparam T The container type to check
 */
template<typename T>
concept SequentialContainer = requires(T t, typename T::value_type v) {
    // Required type aliases
    typename T::value_type;
    typename T::iterator;
    typename T::const_iterator;
    typename T::size_type;
    
    // Size and state queries
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.empty() } -> std::convertible_to<bool>;
    
    // Iterator access
    { t.begin() } -> std::same_as<typename T::iterator>;
    { t.end() } -> std::same_as<typename T::iterator>;
    
    // Modification operations
    { t.push_back(v) };
    { t.pop_back() };
    { t.clear() };
};

/**
 * @brief Concept for associative containers (map, set, unordered_map, etc.)
 * 
 * An associative container must support:
 * - key_type and iterator type aliases
 * - size(), empty(), find(), contains() methods
 * - erase() method for removing elements
 * 
 * This matches std::map, std::unordered_map, std::set, std::unordered_set.
 * 
 * @tparam T The container type to check
 */
template<typename T>
concept AssociativeContainer = requires(T t, typename T::key_type k) {
    // Required type aliases
    typename T::key_type;
    typename T::iterator;
    typename T::const_iterator;
    typename T::size_type;
    
    // Size and state queries
    { t.size() } -> std::convertible_to<std::size_t>;
    { t.empty() } -> std::convertible_to<bool>;
    
    // Lookup operations
    { t.find(k) } -> std::same_as<typename T::iterator>;
    { t.contains(k) } -> std::convertible_to<bool>;
    
    // Modification operations
    { t.erase(k) } -> std::convertible_to<typename T::size_type>;
    { t.clear() };
};

/**
 * @brief Concept for key-value associative containers (map, unordered_map)
 * 
 * Extends AssociativeContainer with mapped_type requirement.
 * 
 * @tparam T The container type to check
 */
template<typename T>
concept KeyValueContainer = AssociativeContainer<T> && requires {
    typename T::mapped_type;
};

/**
 * @brief Concept for member function pointers
 * 
 * Checks if F is a pointer to a member function.
 * 
 * @tparam F The function pointer type to check
 */
template<typename F>
concept MemberFunction = std::is_member_function_pointer_v<F>;

/**
 * @brief Concept for member data pointers
 * 
 * Checks if F is a pointer to a member data.
 * 
 * @tparam F The pointer type to check
 */
template<typename F>
concept MemberPointer = std::is_member_object_pointer_v<F>;

/**
 * @brief Concept for callable types
 * 
 * @tparam F The callable type
 * @tparam Args The argument types
 */
template<typename F, typename... Args>
concept Callable = std::invocable<F, Args...>;

/**
 * @brief Concept for types convertible to string
 */
template<typename T>
concept StringConvertible = std::convertible_to<T, std::string> || 
                            std::convertible_to<T, std::string_view> ||
                            std::convertible_to<T, const char*>;

namespace detail {

/**
 * @brief Type trait to detect if a type is a sequential container
 */
template<typename T>
struct is_sequential_container : std::bool_constant<SequentialContainer<T>> {};

template<typename T>
inline constexpr bool is_sequential_container_v = is_sequential_container<T>::value;

/**
 * @brief Type trait to detect if a type is an associative container
 */
template<typename T>
struct is_associative_container : std::bool_constant<AssociativeContainer<T>> {};

template<typename T>
inline constexpr bool is_associative_container_v = is_associative_container<T>::value;

/**
 * @brief Type trait to detect if a type is a key-value container
 */
template<typename T>
struct is_key_value_container : std::bool_constant<KeyValueContainer<T>> {};

template<typename T>
inline constexpr bool is_key_value_container_v = is_key_value_container<T>::value;

/**
 * @brief Type trait to detect if a type is any container
 */
template<typename T>
struct is_container : std::bool_constant<SequentialContainer<T> || AssociativeContainer<T>> {};

template<typename T>
inline constexpr bool is_container_v = is_container<T>::value;

/**
 * @brief Type trait to detect if a type is an enum
 */
template<typename T>
struct is_enum_type : std::is_enum<T> {};

template<typename T>
inline constexpr bool is_enum_type_v = is_enum_type<T>::value;

/**
 * @brief Type trait to detect if a type is a primitive (arithmetic or enum)
 */
template<typename T>
struct is_primitive : std::bool_constant<std::is_arithmetic_v<T> || std::is_enum_v<T>> {};

template<typename T>
inline constexpr bool is_primitive_v = is_primitive<T>::value;

/**
 * @brief Helper to extract class type from member pointer
 */
template<typename T>
struct member_class;

template<typename T, typename C>
struct member_class<T C::*> {
    using type = C;
};

template<typename T>
using member_class_t = typename member_class<T>::type;

/**
 * @brief Helper to extract value type from member pointer
 */
template<typename T>
struct member_type;

template<typename T, typename C>
struct member_type<T C::*> {
    using type = T;
};

template<typename T>
using member_type_t = typename member_type<T>::type;

/**
 * @brief Compile-time type name generation
 * 
 * Uses compiler-specific intrinsics to get type name at compile time.
 */
template<typename T>
consteval std::string_view type_name() {
#if defined(__clang__)
    constexpr std::string_view name = __PRETTY_FUNCTION__;
    constexpr std::string_view prefix = "std::string_view rttm::detail::type_name() [T = ";
    constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
    constexpr std::string_view name = __PRETTY_FUNCTION__;
    constexpr std::string_view prefix = "consteval std::string_view rttm::detail::type_name() [with T = ";
    constexpr std::string_view suffix = "]";
#elif defined(_MSC_VER)
    constexpr std::string_view name = __FUNCSIG__;
    constexpr std::string_view prefix = "std::string_view __cdecl rttm::detail::type_name<";
    constexpr std::string_view suffix = ">(void)";
#else
    #error "Unsupported compiler"
#endif
    
    constexpr auto start = name.find(prefix) + prefix.size();
    constexpr auto end = name.rfind(suffix);
    return name.substr(start, end - start);
}

/**
 * @brief Compile-time unique type ID
 * 
 * Uses the address of a static variable as a unique identifier for each type.
 * This is much faster than string comparison or std::type_index.
 */
using TypeId = const void*;

template<typename T>
struct type_id_holder {
    static constexpr char id = 0;
};

template<typename T>
inline constexpr TypeId type_id = &type_id_holder<std::remove_cvref_t<T>>::id;

} // namespace detail

} // namespace rttm

#endif // RTTM_DETAIL_TYPE_TRAITS_HPP
