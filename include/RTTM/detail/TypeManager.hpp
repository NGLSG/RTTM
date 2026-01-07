/**
 * @file TypeManager.hpp
 * @brief Type information storage and query management
 * 
 * This file implements the TypeManager singleton which provides:
 * - Thread-safe storage for registered type information
 * - Type registration with duplicate detection
 * - Type lookup by name or type_index
 * - Hash-based fast lookup for string names
 */

#ifndef RTTM_DETAIL_TYPE_MANAGER_HPP
#define RTTM_DETAIL_TYPE_MANAGER_HPP

#include "TypeInfo.hpp"
#include "TypeTraits.hpp"

#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <stdexcept>
#include <array>

namespace rttm {
// Forward declaration for exception
class TypeNotRegisteredError;
}

namespace rttm::detail {

// Import TypeId from TypeTraits
using TypeId = const void*;

// Helper function to throw TypeNotRegisteredError (defined after exception class)
[[noreturn]] inline void throw_type_not_registered(std::string_view type_name);

// FNV-1a hash for string_view (compile-time capable)
constexpr std::size_t fnv1a_hash(std::string_view str) noexcept {
    std::size_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<std::size_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Use TransparentStringHash and TransparentStringEqual from TypeInfo.hpp

/**
 * @brief Fast inline cache for hot path lookups
 * 
 * Uses hash-based lookup with linear probing.
 * Much faster than unordered_map for small number of entries.
 */
template<std::size_t N = 16>
class FastTypeCache {
public:
    struct Entry {
        std::size_t hash = 0;
        const TypeInfo* info = nullptr;
    };
    
    const TypeInfo* find(std::size_t hash) const noexcept {
        const std::size_t idx = hash & (N - 1);
        // Check primary slot
        if (entries_[idx].hash == hash) [[likely]] {
            return entries_[idx].info;
        }
        // Linear probe (one step)
        const std::size_t idx2 = (idx + 1) & (N - 1);
        if (entries_[idx2].hash == hash) {
            return entries_[idx2].info;
        }
        return nullptr;
    }
    
    void insert(std::size_t hash, const TypeInfo* info) noexcept {
        const std::size_t idx = hash & (N - 1);
        if (entries_[idx].info == nullptr || entries_[idx].hash == hash) {
            entries_[idx] = {hash, info};
            return;
        }
        // Linear probe
        const std::size_t idx2 = (idx + 1) & (N - 1);
        entries_[idx2] = {hash, info};
    }
    
    void clear() noexcept {
        for (auto& e : entries_) {
            e = {};
        }
    }

private:
    std::array<Entry, N> entries_{};
};

/**
 * @brief Singleton manager for all registered type information
 * 
 * TypeManager provides thread-safe storage and retrieval of TypeInfo
 * for all registered types. It uses a read-write lock to allow
 * concurrent reads while serializing writes.
 * 
 * Optimizations:
 * - Hash-based lookup using string_view (no allocation)
 * - Thread-local fast cache for repeated lookups
 * - Separate hash index for O(1) lookup by pre-computed hash
 */
class TypeManager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the global TypeManager instance
     */
    static TypeManager& instance() {
        static TypeManager mgr;
        return mgr;
    }
    
    // Delete copy and move operations
    TypeManager(const TypeManager&) = delete;
    TypeManager& operator=(const TypeManager&) = delete;
    TypeManager(TypeManager&&) = delete;
    TypeManager& operator=(TypeManager&&) = delete;
    
    /**
     * @brief Register a type with its information
     * 
     * If the type is already registered, this operation is a no-op
     * (idempotent registration).
     * 
     * @param name The type name
     * @param info The type information to register
     * @param type_id The compile-time type ID (optional, for fast lookup)
     * @return true if newly registered, false if already existed
     */
    bool register_type(std::string_view name, TypeInfo info, TypeId type_id = nullptr) {
        std::unique_lock lock(mutex_);
        
        const std::size_t hash = fnv1a_hash(name);
        
        // Check if already registered
        auto it = types_by_name_.find(name);
        if (it != types_by_name_.end()) {
            return false;
        }
        
        // Insert the type info
        auto [inserted_it, success] = types_by_name_.emplace(std::string{name}, std::move(info));
        if (success) {
            // Index by hash for fast lookup
            types_by_hash_[hash] = &inserted_it->second;
            // Also index by type_index
            types_by_index_[inserted_it->second.type_index] = &inserted_it->second;
            // Index by TypeId for fastest lookup
            if (type_id) {
                types_by_id_[type_id] = &inserted_it->second;
            }
        }
        
        return success;
    }
    
    /**
     * @brief Get type information by compile-time TypeId (fastest)
     * 
     * @param id The compile-time type ID
     * @return Pointer to TypeInfo if found, nullptr otherwise
     */
    const TypeInfo* get_type_by_id(TypeId id) const {
        std::shared_lock lock(mutex_);
        auto it = types_by_id_.find(id);
        return it != types_by_id_.end() ? it->second : nullptr;
    }

    /**
     * @brief Get type information by name (optimized for dynamic lookup)
     * 
     * Uses hash-based lookup with thread-local cache.
     * No string allocation on the hot path.
     * 
     * @param name The type name to look up
     * @return Pointer to TypeInfo if found, nullptr otherwise
     */
    const TypeInfo* get_type(std::string_view name) const {
        const std::size_t hash = fnv1a_hash(name);
        return get_type_by_hash_internal(hash, name);
    }
    
    /**
     * @brief Get type information by pre-computed hash (fastest dynamic lookup)
     * 
     * Use this when you can pre-compute the hash outside the hot loop.
     * 
     * @param hash Pre-computed FNV-1a hash of the type name
     * @param name The type name (for collision verification)
     * @return Pointer to TypeInfo if found, nullptr otherwise
     */
    const TypeInfo* get_type_by_hash(std::size_t hash, std::string_view name) const {
        return get_type_by_hash_internal(hash, name);
    }

private:
    /**
     * @brief Internal hash-based lookup with TLS cache
     */
    const TypeInfo* get_type_by_hash_internal(std::size_t hash, std::string_view name) const {
        // Fast path: check thread-local cache (no lock, no allocation)
        auto& cache = get_tls_cache();
        if (auto* info = cache.find(hash)) [[likely]] {
            return info;
        }
        
        // Cache miss - look up in hash index (need lock)
        std::shared_lock lock(mutex_);
        auto it = types_by_hash_.find(hash);
        if (it != types_by_hash_.end()) [[likely]] {
            // Verify name matches (hash collision check)
            if (it->second->name == name) [[likely]] {
                cache.insert(hash, it->second);
                return it->second;
            }
        }
        
        // Fallback: full string lookup (rare, handles collisions)
        auto name_it = types_by_name_.find(name);
        if (name_it != types_by_name_.end()) {
            cache.insert(hash, &name_it->second);
            return &name_it->second;
        }
        
        return nullptr;
    }

public:
    
    /**
     * @brief Get type information by name, throwing if not found
     * 
     * Uses thread-local cache for repeated lookups.
     * 
     * @param name The type name to look up
     * @return Reference to TypeInfo
     * @throws TypeNotRegisteredError if type is not registered
     */
    const TypeInfo& get_type_or_throw(std::string_view name) const {
        const TypeInfo* info = get_type(name);
        if (!info) {
            throw_type_not_registered(name);
        }
        return *info;
    }
    
    /**
     * @brief Get type information by type_index
     * 
     * @param index The type_index to look up
     * @return Pointer to TypeInfo if found, nullptr otherwise
     */
    const TypeInfo* get_type(std::type_index index) const {
        std::shared_lock lock(mutex_);
        
        auto it = types_by_index_.find(index);
        if (it != types_by_index_.end()) {
            return it->second;
        }
        
        return nullptr;
    }
    
    /**
     * @brief Get type information by type_index, throwing if not found
     * 
     * @param index The type_index to look up
     * @param type_name_hint Optional type name for error message
     * @return Reference to TypeInfo
     * @throws TypeNotRegisteredError if type is not registered
     */
    const TypeInfo& get_type_or_throw(std::type_index index, 
                                       std::string_view type_name_hint = "unknown") const {
        const TypeInfo* info = get_type(index);
        if (!info) {
            throw_type_not_registered(type_name_hint);
        }
        return *info;
    }
    
    /**
     * @brief Get mutable type information by name
     * 
     * Used during registration to add members/methods to existing types.
     * 
     * @param name The type name to look up
     * @return Pointer to TypeInfo if found, nullptr otherwise
     */
    TypeInfo* get_type_mutable(std::string_view name) {
        std::shared_lock lock(mutex_);
        
        auto it = types_by_name_.find(name);
        if (it != types_by_name_.end()) {
            return &it->second;
        }
        
        return nullptr;
    }
    
    /**
     * @brief Check if a type is registered
     * 
     * @param name The type name to check
     * @return true if registered, false otherwise
     */
    bool is_registered(std::string_view name) const {
        std::shared_lock lock(mutex_);
        return types_by_name_.find(name) != types_by_name_.end();
    }
    
    /**
     * @brief Check if a type is registered by type_index
     * 
     * @param index The type_index to check
     * @return true if registered, false otherwise
     */
    bool is_registered(std::type_index index) const {
        std::shared_lock lock(mutex_);
        return types_by_index_.find(index) != types_by_index_.end();
    }
    
    /**
     * @brief Get all registered type names
     * 
     * @return Vector of all registered type names
     */
    std::vector<std::string_view> get_all_type_names() const {
        std::shared_lock lock(mutex_);
        
        std::vector<std::string_view> names;
        names.reserve(types_by_name_.size());
        
        for (const auto& [name, _] : types_by_name_) {
            names.push_back(name);
        }
        
        return names;
    }
    
    /**
     * @brief Clear the thread-local cache
     * 
     * Useful for testing or when type registrations change.
     */
    void clear_cache() const {
        get_tls_cache().clear();
    }
    
    /**
     * @brief Get the number of registered types
     * 
     * @return Number of registered types
     */
    std::size_t size() const {
        std::shared_lock lock(mutex_);
        return types_by_name_.size();
    }

private:
    TypeManager() = default;
    ~TypeManager() = default;
    
    /**
     * @brief Get thread-local fast cache
     */
    static FastTypeCache<16>& get_tls_cache() {
        thread_local FastTypeCache<16> cache;
        return cache;
    }
    
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, TypeInfo, TransparentStringHash, TransparentStringEqual> types_by_name_;
    std::unordered_map<std::size_t, const TypeInfo*> types_by_hash_;  // Hash -> TypeInfo
    std::unordered_map<std::type_index, const TypeInfo*> types_by_index_;
    std::unordered_map<TypeId, const TypeInfo*> types_by_id_;
};

} // namespace rttm::detail

// Include exceptions after TypeManager is defined
#include "Exceptions.hpp"

namespace rttm::detail {

/**
 * @brief Helper function to throw TypeNotRegisteredError
 * 
 * This is defined after including Exceptions.hpp to avoid circular dependencies.
 */
[[noreturn]] inline void throw_type_not_registered(std::string_view type_name) {
    throw rttm::TypeNotRegisteredError(type_name);
}

} // namespace rttm::detail

#endif // RTTM_DETAIL_TYPE_MANAGER_HPP
