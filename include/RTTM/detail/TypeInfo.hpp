/**
 * @file TypeInfo.hpp
 * @brief Type information data structures for RTTM reflection library
 * 
 * This file defines the core data structures used to store type metadata:
 * - MemberInfo: Information about class member variables
 * - MethodInfo: Information about class methods
 * - TypeInfo: Complete type metadata including members, methods, and factories
 */

#ifndef RTTM_DETAIL_TYPE_INFO_HPP
#define RTTM_DETAIL_TYPE_INFO_HPP

#include <string>
#include <string_view>
#include <typeindex>
#include <functional>
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include <any>
#include <span>

namespace rttm::detail {

// FNV-1a hash for string_view
constexpr std::size_t type_info_hash(std::string_view str) noexcept {
    std::size_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<std::size_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Transparent hash for string_view lookups
struct TransparentStringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view sv) const noexcept {
        return type_info_hash(sv);
    }
    std::size_t operator()(const std::string& s) const noexcept {
        return type_info_hash(std::string_view{s});
    }
    std::size_t operator()(const char* s) const noexcept {
        return type_info_hash(std::string_view{s});
    }
};

struct TransparentStringEqual {
    using is_transparent = void;
    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
        return lhs == rhs;
    }
    bool operator()(const char* lhs, std::string_view rhs) const noexcept {
        return std::string_view{lhs} == rhs;
    }
    bool operator()(std::string_view lhs, const char* rhs) const noexcept {
        return lhs == std::string_view{rhs};
    }
};

/**
 * @brief Category of a member property
 * 
 * Used to identify the kind of type a property holds,
 * enabling appropriate handling during serialization and access.
 */
enum class MemberCategory {
    Primitive,      ///< Arithmetic types (int, float, etc.) and enums
    Class,          ///< User-defined class types
    Enum,           ///< Enumeration types
    Sequential,     ///< Sequential containers (vector, list, deque)
    Associative     ///< Associative containers (map, set, unordered_map, etc.)
};

/**
 * @brief Information about a class member variable
 * 
 * Stores metadata needed to access and manipulate a member at runtime.
 */
struct MemberInfo {
    std::string name;                   ///< Name of the member
    std::size_t offset;                 ///< Byte offset from object start
    std::type_index type_index;         ///< Type information for the member
    std::string type_name;              ///< Human-readable type name
    MemberCategory category;            ///< Category of the member type
    
    /**
     * @brief Default constructor
     */
    MemberInfo() 
        : name{}
        , offset{0}
        , type_index{typeid(void)}
        , type_name{}
        , category{MemberCategory::Primitive}
    {}
    
    /**
     * @brief Construct MemberInfo with all fields
     */
    MemberInfo(std::string_view n, std::size_t off, std::type_index ti, 
               std::string_view tn, MemberCategory cat)
        : name{n}
        , offset{off}
        , type_index{ti}
        , type_name{tn}
        , category{cat}
    {}
};


/**
 * @brief Raw invoker function pointer type for maximum performance
 * 
 * Using a raw function pointer avoids std::function's virtual call overhead.
 */
using RawInvoker = std::any(*)(void*, std::span<std::any>);

/**
 * @brief Information about a class method
 * 
 * Stores metadata needed to invoke a method at runtime, including
 * a type-erased invoker function that handles the actual call.
 */
struct MethodInfo {
    std::string name;                                               ///< Name of the method
    std::function<std::any(void*, std::span<std::any>)> invoker;   ///< Type-erased method invoker (fallback)
    RawInvoker raw_invoker = nullptr;                               ///< Raw function pointer invoker (fast path)
    std::vector<std::type_index> param_types;                       ///< Parameter type information
    std::type_index return_type;                                    ///< Return type information
    std::string return_type_name;                                   ///< Human-readable return type name
    bool is_const;                                                  ///< Whether this is a const method
    
    /**
     * @brief Default constructor
     */
    MethodInfo()
        : name{}
        , invoker{}
        , raw_invoker{nullptr}
        , param_types{}
        , return_type{typeid(void)}
        , return_type_name{"void"}
        , is_const{false}
    {}
    
    /**
     * @brief Construct MethodInfo with all fields
     */
    MethodInfo(std::string_view n, 
               std::function<std::any(void*, std::span<std::any>)> inv,
               std::vector<std::type_index> params,
               std::type_index ret_type,
               std::string_view ret_name,
               bool is_const_method)
        : name{n}
        , invoker{std::move(inv)}
        , raw_invoker{nullptr}
        , param_types{std::move(params)}
        , return_type{ret_type}
        , return_type_name{ret_name}
        , is_const{is_const_method}
    {}
    
    /**
     * @brief Invoke the method using the fastest available path
     */
    [[nodiscard]] std::any call(void* obj, std::span<std::any> args) const {
        if (raw_invoker) [[likely]] {
            return raw_invoker(obj, args);
        }
        return invoker(obj, args);
    }
};

/**
 * @brief Raw factory function pointer type for maximum performance
 */
using RawFactory = std::shared_ptr<void>(*)();

// Transparent map types for string_view lookups without allocation
template<typename V>
using TransparentStringMap = std::unordered_map<std::string, V, TransparentStringHash, TransparentStringEqual>;

/**
 * @brief Complete type information structure
 * 
 * Contains all metadata about a registered type, including:
 * - Basic type information (name, size, type_index)
 * - Member variables and their metadata
 * - Methods and their metadata (supports overloading)
 * - Factory functions for object creation
 * - Destructor and copy functions
 * - Inheritance information
 */
struct TypeInfo {
    std::string name;                                                           ///< Type name
    std::size_t size;                                                           ///< Size of the type in bytes
    std::type_index type_index;                                                 ///< Type index for type comparison
    
    TransparentStringMap<MemberInfo> members;                                   ///< Member variables by name
    TransparentStringMap<std::vector<MethodInfo>> methods;                      ///< Methods by name (vector for overloads)
    TransparentStringMap<std::function<std::shared_ptr<void>()>> factories;     ///< Factory functions by signature
    
    std::function<void(void*)> destructor;                                      ///< Type-erased destructor
    std::function<void(void*, const void*)> copier;                             ///< Type-erased copy function
    
    std::vector<std::type_index> base_types;                                    ///< Base class type indices
    
    // Fast path: cached raw function pointer for default factory
    RawFactory default_factory_raw = nullptr;
    
    // Cached name lists for fast enumeration
    mutable std::vector<std::string_view> cached_member_names_;
    mutable std::vector<std::string_view> cached_method_names_;
    mutable bool member_names_cached_ = false;
    mutable bool method_names_cached_ = false;
    
    // Hash-based member cache for O(1) lookup
    // Using flat array for small member counts (common case)
    static constexpr std::size_t INLINE_CACHE_SIZE = 16;
    struct CacheEntry {
        std::size_t hash = 0;
        const MemberInfo* info = nullptr;
    };
    mutable std::array<CacheEntry, INLINE_CACHE_SIZE> member_inline_cache_{};
    mutable std::array<CacheEntry, INLINE_CACHE_SIZE> method_inline_cache_{};
    mutable std::unordered_map<std::size_t, const MemberInfo*> member_hash_cache_;
    mutable std::unordered_map<std::size_t, const std::vector<MethodInfo>*> method_hash_cache_;
    mutable bool member_cache_built_ = false;
    mutable bool method_cache_built_ = false;
    mutable bool use_inline_member_cache_ = false;
    mutable bool use_inline_method_cache_ = false;
    
    /**
     * @brief Default constructor
     */
    TypeInfo()
        : name{}
        , size{0}
        , type_index{typeid(void)}
        , members{}
        , methods{}
        , factories{}
        , destructor{}
        , copier{}
        , base_types{}
    {}
    
    /**
     * @brief Construct TypeInfo with basic information
     */
    TypeInfo(std::string_view n, std::size_t s, std::type_index ti)
        : name{n}
        , size{s}
        , type_index{ti}
        , members{}
        , methods{}
        , factories{}
        , destructor{}
        , copier{}
        , base_types{}
    {}
    
    /**
     * @brief Check if a member exists (no allocation)
     */
    [[nodiscard]] bool has_member(std::string_view member_name) const {
        return members.find(member_name) != members.end();
    }
    
    /**
     * @brief Check if a method exists (no allocation)
     */
    [[nodiscard]] bool has_method(std::string_view method_name) const {
        return methods.find(method_name) != methods.end();
    }
    
    /**
     * @brief Find member by name using hash cache (fastest)
     */
    [[nodiscard]] const MemberInfo* find_member(std::string_view member_name) const {
        // Build cache on first access
        if (!member_cache_built_) [[unlikely]] {
            build_member_cache();
        }
        
        const std::size_t hash = type_info_hash(member_name);
        
        // Fast path: inline cache for small member counts
        if (use_inline_member_cache_) [[likely]] {
            const std::size_t idx = hash & (INLINE_CACHE_SIZE - 1);
            if (member_inline_cache_[idx].hash == hash) [[likely]] {
                return member_inline_cache_[idx].info;
            }
            // Linear probe
            const std::size_t idx2 = (idx + 1) & (INLINE_CACHE_SIZE - 1);
            if (member_inline_cache_[idx2].hash == hash) {
                return member_inline_cache_[idx2].info;
            }
        } else {
            auto it = member_hash_cache_.find(hash);
            if (it != member_hash_cache_.end()) [[likely]] {
                return it->second;
            }
        }
        
        // Fallback for hash collision (rare)
        auto map_it = members.find(member_name);
        return map_it != members.end() ? &map_it->second : nullptr;
    }
    
    /**
     * @brief Find member by pre-computed hash (fastest path)
     */
    [[nodiscard]] const MemberInfo* find_member_by_hash(std::size_t hash) const {
        if (!member_cache_built_) [[unlikely]] {
            build_member_cache();
        }
        auto it = member_hash_cache_.find(hash);
        return it != member_hash_cache_.end() ? it->second : nullptr;
    }
    
    /**
     * @brief Find methods by name using hash cache (fastest)
     */
    [[nodiscard]] const std::vector<MethodInfo>* find_methods(std::string_view method_name) const {
        // Build cache on first access
        if (!method_cache_built_) [[unlikely]] {
            build_method_cache();
        }
        
        const std::size_t hash = type_info_hash(method_name);
        
        // Fast path: inline cache for small method counts
        if (use_inline_method_cache_) [[likely]] {
            const std::size_t idx = hash & (INLINE_CACHE_SIZE - 1);
            if (method_inline_cache_[idx].hash == hash) [[likely]] {
                return reinterpret_cast<const std::vector<MethodInfo>*>(method_inline_cache_[idx].info);
            }
            // Linear probe
            const std::size_t idx2 = (idx + 1) & (INLINE_CACHE_SIZE - 1);
            if (method_inline_cache_[idx2].hash == hash) {
                return reinterpret_cast<const std::vector<MethodInfo>*>(method_inline_cache_[idx2].info);
            }
        } else {
            auto it = method_hash_cache_.find(hash);
            if (it != method_hash_cache_.end()) [[likely]] {
                return it->second;
            }
        }
        
        // Fallback for hash collision (rare)
        auto map_it = methods.find(method_name);
        return map_it != methods.end() ? &map_it->second : nullptr;
    }
    
    /**
     * @brief Find methods by pre-computed hash (fastest path)
     */
    [[nodiscard]] const std::vector<MethodInfo>* find_methods_by_hash(std::size_t hash) const {
        if (!method_cache_built_) [[unlikely]] {
            build_method_cache();
        }
        auto it = method_hash_cache_.find(hash);
        return it != method_hash_cache_.end() ? it->second : nullptr;
    }
    
    /**
     * @brief Get all member names (cached)
     */
    [[nodiscard]] const std::vector<std::string_view>& member_names() const {
        if (!member_names_cached_) [[unlikely]] {
            cached_member_names_.clear();
            cached_member_names_.reserve(members.size());
            for (const auto& [n, _] : members) {
                cached_member_names_.push_back(n);
            }
            member_names_cached_ = true;
        }
        return cached_member_names_;
    }
    
    /**
     * @brief Get all method names (cached)
     */
    [[nodiscard]] const std::vector<std::string_view>& method_names() const {
        if (!method_names_cached_) [[unlikely]] {
            cached_method_names_.clear();
            cached_method_names_.reserve(methods.size());
            for (const auto& [n, _] : methods) {
                cached_method_names_.push_back(n);
            }
            method_names_cached_ = true;
        }
        return cached_method_names_;
    }
    
    /**
     * @brief Invalidate cached name lists (call after adding members/methods)
     */
    void invalidate_name_cache() {
        member_names_cached_ = false;
        method_names_cached_ = false;
    }
    
    /**
     * @brief Invalidate hash caches (call after adding members/methods)
     */
    void invalidate_hash_cache() {
        member_cache_built_ = false;
        method_cache_built_ = false;
        use_inline_member_cache_ = false;
        use_inline_method_cache_ = false;
        member_hash_cache_.clear();
        method_hash_cache_.clear();
        for (auto& entry : member_inline_cache_) entry = {};
        for (auto& entry : method_inline_cache_) entry = {};
    }

private:
    /**
     * @brief Build member hash cache for O(1) lookup
     */
    void build_member_cache() const {
        // Use inline cache for small member counts
        if (members.size() <= INLINE_CACHE_SIZE / 2) {
            use_inline_member_cache_ = true;
            for (auto& entry : member_inline_cache_) {
                entry = {};
            }
            for (const auto& [name, info] : members) {
                const std::size_t hash = type_info_hash(name);
                const std::size_t idx = hash & (INLINE_CACHE_SIZE - 1);
                if (member_inline_cache_[idx].info == nullptr) {
                    member_inline_cache_[idx] = {hash, &info};
                } else {
                    // Linear probe
                    const std::size_t idx2 = (idx + 1) & (INLINE_CACHE_SIZE - 1);
                    member_inline_cache_[idx2] = {hash, &info};
                }
            }
        } else {
            use_inline_member_cache_ = false;
            member_hash_cache_.clear();
            member_hash_cache_.reserve(members.size());
            for (const auto& [name, info] : members) {
                const std::size_t hash = type_info_hash(name);
                member_hash_cache_[hash] = &info;
            }
        }
        member_cache_built_ = true;
    }
    
    /**
     * @brief Build method hash cache for O(1) lookup
     */
    void build_method_cache() const {
        // Use inline cache for small method counts
        if (methods.size() <= INLINE_CACHE_SIZE / 2) {
            use_inline_method_cache_ = true;
            for (auto& entry : method_inline_cache_) {
                entry = {};
            }
            for (const auto& [name, info_vec] : methods) {
                const std::size_t hash = type_info_hash(name);
                const std::size_t idx = hash & (INLINE_CACHE_SIZE - 1);
                // Store pointer to vector in the info field (reinterpret)
                if (method_inline_cache_[idx].info == nullptr) {
                    method_inline_cache_[idx] = {hash, reinterpret_cast<const MemberInfo*>(&info_vec)};
                } else {
                    const std::size_t idx2 = (idx + 1) & (INLINE_CACHE_SIZE - 1);
                    method_inline_cache_[idx2] = {hash, reinterpret_cast<const MemberInfo*>(&info_vec)};
                }
            }
        } else {
            use_inline_method_cache_ = false;
            method_hash_cache_.clear();
            method_hash_cache_.reserve(methods.size());
            for (const auto& [name, info_vec] : methods) {
                const std::size_t hash = type_info_hash(name);
                method_hash_cache_[hash] = &info_vec;
            }
        }
        method_cache_built_ = true;
    }
};

} // namespace rttm::detail

#endif // RTTM_DETAIL_TYPE_INFO_HPP
