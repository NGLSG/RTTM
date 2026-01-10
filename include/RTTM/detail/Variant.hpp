/**
 * @file Variant.hpp
 * @brief Type-erased value container for pure dynamic reflection
 * 
 * Variant provides RTTR-like dynamic value storage without needing
 * compile-time type knowledge. It supports:
 * - Any type storage with Small Buffer Optimization
 * - Type-safe conversions
 * - DLL-compatible (no templates in virtual interface)
 */

#ifndef RTTM_DETAIL_VARIANT_HPP
#define RTTM_DETAIL_VARIANT_HPP

#include "TypeInfo.hpp"
#include "TypeManager.hpp"
#include "Exceptions.hpp"

#include <memory>
#include <typeindex>
#include <string_view>
#include <cstring>
#include <type_traits>

namespace rttm {

/**
 * @brief Type-erased value container with Small Buffer Optimization
 * 
 * Variant can hold any value and provides type-safe access without
 * needing compile-time type knowledge. Uses SBO to avoid heap allocation
 * for small types (up to 16 bytes).
 */
class Variant {
    // Small Buffer Optimization
    static constexpr std::size_t SBO_SIZE = 16;
    static constexpr std::size_t SBO_ALIGN = 8;
    
    // Type-erased operations
    using DestroyFn = void(*)(void*) noexcept;
    using CopyFn = void(*)(void* dst, const void* src);
    using MoveFn = void(*)(void* dst, void* src) noexcept;
    using TypeFn = std::type_index(*)() noexcept;
    
    struct Ops {
        DestroyFn destroy;
        CopyFn copy;
        MoveFn move;
        TypeFn type;
        std::size_t size;
        bool use_sbo;
    };
    
    template<typename T>
    static const Ops* get_ops() {
        static constexpr bool use_sbo = sizeof(T) <= SBO_SIZE && 
                                        alignof(T) <= SBO_ALIGN &&
                                        std::is_nothrow_move_constructible_v<T>;
        static const Ops ops = {
            [](void* ptr) noexcept { static_cast<T*>(ptr)->~T(); },
            [](void* dst, const void* src) { new (dst) T(*static_cast<const T*>(src)); },
            [](void* dst, void* src) noexcept { new (dst) T(std::move(*static_cast<T*>(src))); },
            []() noexcept { return std::type_index(typeid(T)); },
            sizeof(T),
            use_sbo
        };
        return &ops;
    }

public:
    Variant() noexcept : ops_(nullptr), heap_ptr_(nullptr) {}
    
    template<typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Variant>>>
    static Variant create(T&& value) {
        using DecayT = std::decay_t<T>;
        Variant v;
        v.ops_ = get_ops<DecayT>();
        
        if (v.ops_->use_sbo) {
            new (v.sbo_) DecayT(std::forward<T>(value));
        } else {
            v.heap_ptr_ = new DecayT(std::forward<T>(value));
        }
        return v;
    }
    
    ~Variant() { clear(); }
    
    Variant(const Variant& other) : ops_(nullptr), heap_ptr_(nullptr) {
        if (other.ops_) {
            ops_ = other.ops_;
            if (ops_->use_sbo) {
                ops_->copy(sbo_, other.sbo_);
            } else {
                // Allocate and copy
                heap_ptr_ = ::operator new(ops_->size);
                ops_->copy(heap_ptr_, other.heap_ptr_);
            }
        }
    }
    
    Variant& operator=(const Variant& other) {
        if (this != &other) {
            clear();
            if (other.ops_) {
                ops_ = other.ops_;
                if (ops_->use_sbo) {
                    ops_->copy(sbo_, other.sbo_);
                } else {
                    heap_ptr_ = ::operator new(ops_->size);
                    ops_->copy(heap_ptr_, other.heap_ptr_);
                }
            }
        }
        return *this;
    }
    
    Variant(Variant&& other) noexcept : ops_(nullptr), heap_ptr_(nullptr) {
        if (other.ops_) {
            ops_ = other.ops_;
            if (ops_->use_sbo) {
                ops_->move(sbo_, other.sbo_);
                other.ops_->destroy(other.sbo_);
            } else {
                heap_ptr_ = other.heap_ptr_;
                other.heap_ptr_ = nullptr;
            }
            other.ops_ = nullptr;
        }
    }
    
    Variant& operator=(Variant&& other) noexcept {
        if (this != &other) {
            clear();
            if (other.ops_) {
                ops_ = other.ops_;
                if (ops_->use_sbo) {
                    ops_->move(sbo_, other.sbo_);
                    other.ops_->destroy(other.sbo_);
                } else {
                    heap_ptr_ = other.heap_ptr_;
                    other.heap_ptr_ = nullptr;
                }
                other.ops_ = nullptr;
            }
        }
        return *this;
    }
    
    [[nodiscard]] bool is_valid() const noexcept { return ops_ != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return is_valid(); }
    
    template<typename T>
    [[nodiscard]] bool is_type() const noexcept {
        return ops_ && ops_->type() == std::type_index(typeid(T));
    }
    
    [[nodiscard]] std::type_index type() const noexcept {
        return ops_ ? ops_->type() : std::type_index(typeid(void));
    }
    
    [[nodiscard]] void* get_raw() noexcept {
        if (!ops_) return nullptr;
        return ops_->use_sbo ? sbo_ : heap_ptr_;
    }
    
    [[nodiscard]] const void* get_raw() const noexcept {
        if (!ops_) return nullptr;
        return ops_->use_sbo ? sbo_ : heap_ptr_;
    }
    
    template<typename T>
    [[nodiscard]] T& get() {
        if (!is_type<T>()) throw ReflectionError("Variant type mismatch");
        return *static_cast<T*>(get_raw());
    }
    
    template<typename T>
    [[nodiscard]] const T& get() const {
        if (!is_type<T>()) throw ReflectionError("Variant type mismatch");
        return *static_cast<const T*>(get_raw());
    }
    
    template<typename T>
    [[nodiscard]] T& get_unchecked() noexcept {
        return *static_cast<T*>(get_raw());
    }
    
    template<typename T>
    [[nodiscard]] const T& get_unchecked() const noexcept {
        return *static_cast<const T*>(get_raw());
    }
    
    template<typename T>
    [[nodiscard]] T* try_get() noexcept {
        return is_type<T>() ? static_cast<T*>(get_raw()) : nullptr;
    }
    
    template<typename T>
    [[nodiscard]] bool can_convert() const noexcept;
    
    template<typename T>
    [[nodiscard]] T convert() const;
    
    void clear() noexcept {
        if (ops_) {
            if (ops_->use_sbo) {
                ops_->destroy(sbo_);
            } else if (heap_ptr_) {
                ops_->destroy(heap_ptr_);
                ::operator delete(heap_ptr_);
            }
            ops_ = nullptr;
            heap_ptr_ = nullptr;
        }
    }

private:
    const Ops* ops_;
    alignas(SBO_ALIGN) char sbo_[SBO_SIZE];
    void* heap_ptr_;
};

// Type conversions
template<typename T>
bool Variant::can_convert() const noexcept {
    if (!ops_) return false;
    if (is_type<T>()) return true;
    
    if constexpr (std::is_arithmetic_v<T>) {
        auto ti = type();
        return ti == typeid(int) || ti == typeid(float) || ti == typeid(double) ||
               ti == typeid(long) || ti == typeid(short) || ti == typeid(char) ||
               ti == typeid(unsigned int) || ti == typeid(unsigned long) ||
               ti == typeid(long long) || ti == typeid(unsigned long long);
    }
    return false;
}

template<typename T>
T Variant::convert() const {
    if (!ops_) throw ReflectionError("Cannot convert empty variant");
    if (is_type<T>()) return get<T>();
    
    if constexpr (std::is_arithmetic_v<T>) {
        auto ti = type();
        if (ti == typeid(int)) return static_cast<T>(get_unchecked<int>());
        if (ti == typeid(float)) return static_cast<T>(get_unchecked<float>());
        if (ti == typeid(double)) return static_cast<T>(get_unchecked<double>());
        if (ti == typeid(long)) return static_cast<T>(get_unchecked<long>());
        if (ti == typeid(long long)) return static_cast<T>(get_unchecked<long long>());
        if (ti == typeid(short)) return static_cast<T>(get_unchecked<short>());
        if (ti == typeid(unsigned int)) return static_cast<T>(get_unchecked<unsigned int>());
    }
    throw ReflectionError("Cannot convert variant to requested type");
}

} // namespace rttm

#endif // RTTM_DETAIL_VARIANT_HPP
