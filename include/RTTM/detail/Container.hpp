/**
 * @file Container.hpp
 * @brief Container abstraction interfaces for RTTM reflection library
 * 
 * This file defines abstract interfaces for container reflection:
 * - ISequentialContainer: Interface for sequential containers (vector, list, deque)
 * - IAssociativeContainer: Interface for associative containers (map, set, etc.)
 * 
 * These interfaces allow uniform access to container members through reflection
 * without knowing the specific container type at compile time.
 */

#ifndef RTTM_DETAIL_CONTAINER_HPP
#define RTTM_DETAIL_CONTAINER_HPP

#include "TypeTraits.hpp"

#include <memory>
#include <cstddef>
#include <any>
#include <functional>
#include <vector>
#include <list>
#include <deque>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>

namespace rttm {

// Forward declaration
class RType;

/**
 * @brief Abstract interface for sequential containers
 * 
 * Provides a uniform interface for accessing sequential containers
 * (std::vector, std::list, std::deque) through reflection.
 * 
 * Usage:
 * @code
 * auto container = rtype->property("items")->as_sequential();
 * for (auto it = container->begin(); it->has_current(); it->next()) {
 *     auto item = it->current();
 *     // Process item...
 * }
 * @endcode
 */
class ISequentialContainer {
public:
    virtual ~ISequentialContainer() = default;
    
    // ==================== Size and State ====================
    
    /**
     * @brief Get the number of elements in the container
     * @return The container size
     */
    [[nodiscard]] virtual std::size_t size() const = 0;
    
    /**
     * @brief Check if the container is empty
     * @return true if the container has no elements
     */
    [[nodiscard]] virtual bool empty() const = 0;
    
    /**
     * @brief Remove all elements from the container
     */
    virtual void clear() = 0;
    
    // ==================== Element Access ====================
    
    /**
     * @brief Access element at specified index
     * @param index The element index
     * @return Shared pointer to RType wrapping the element
     * @throws std::out_of_range if index is out of bounds
     */
    [[nodiscard]] virtual std::shared_ptr<RType> at(std::size_t index) = 0;
    
    /**
     * @brief Add an element to the end of the container
     * @param value The value to add (as std::any)
     */
    virtual void push_back(const std::any& value) = 0;
    
    /**
     * @brief Remove the last element from the container
     * @throws std::out_of_range if container is empty
     */
    virtual void pop_back() = 0;
    
    // ==================== Iterator ====================
    
    /**
     * @brief Iterator for sequential container traversal
     */
    class Iterator {
    public:
        virtual ~Iterator() = default;
        
        /**
         * @brief Check if iterator has a current element
         * @return true if current() can be called
         */
        [[nodiscard]] virtual bool has_current() const = 0;
        
        /**
         * @brief Get the current element
         * @return Shared pointer to RType wrapping the current element
         */
        [[nodiscard]] virtual std::shared_ptr<RType> current() = 0;
        
        /**
         * @brief Move to the next element
         * @return true if moved to a valid element, false if at end
         */
        virtual bool next() = 0;
        
        /**
         * @brief Reset iterator to the beginning
         */
        virtual void reset() = 0;
    };
    
    /**
     * @brief Get an iterator to the beginning of the container
     * @return Unique pointer to Iterator
     */
    [[nodiscard]] virtual std::unique_ptr<Iterator> begin() = 0;
};

/**
 * @brief Abstract interface for associative containers
 * 
 * Provides a uniform interface for accessing associative containers
 * (std::map, std::unordered_map, std::set, std::unordered_set) through reflection.
 * 
 * Usage:
 * @code
 * auto container = rtype->property("lookup")->as_associative();
 * if (container->contains(key)) {
 *     auto value = container->find(key);
 *     // Process value...
 * }
 * @endcode
 */
class IAssociativeContainer {
public:
    virtual ~IAssociativeContainer() = default;
    
    // ==================== Size and State ====================
    
    /**
     * @brief Get the number of elements in the container
     * @return The container size
     */
    [[nodiscard]] virtual std::size_t size() const = 0;
    
    /**
     * @brief Check if the container is empty
     * @return true if the container has no elements
     */
    [[nodiscard]] virtual bool empty() const = 0;
    
    /**
     * @brief Remove all elements from the container
     */
    virtual void clear() = 0;
    
    // ==================== Lookup Operations ====================
    
    /**
     * @brief Find an element by key
     * @param key The key to search for (as std::any)
     * @return Shared pointer to RType wrapping the found value, or nullptr if not found
     */
    [[nodiscard]] virtual std::shared_ptr<RType> find(const std::any& key) = 0;
    
    /**
     * @brief Check if a key exists in the container
     * @param key The key to check (as std::any)
     * @return true if the key exists
     */
    [[nodiscard]] virtual bool contains(const std::any& key) = 0;
    
    // ==================== Modification Operations ====================
    
    /**
     * @brief Insert a key-value pair into the container
     * 
     * For set-like containers, only the key is used.
     * 
     * @param key The key to insert (as std::any)
     * @param value The value to insert (as std::any), ignored for sets
     */
    virtual void insert(const std::any& key, const std::any& value) = 0;
    
    /**
     * @brief Remove an element by key
     * @param key The key to remove (as std::any)
     * @return true if an element was removed
     */
    virtual bool erase(const std::any& key) = 0;
    
    // ==================== Iterator ====================
    
    /**
     * @brief Key-value iterator for associative container traversal
     */
    class KeyValueIterator {
    public:
        virtual ~KeyValueIterator() = default;
        
        /**
         * @brief Check if iterator has a current element
         * @return true if key()/value() can be called
         */
        [[nodiscard]] virtual bool has_current() const = 0;
        
        /**
         * @brief Get the current key
         * @return Shared pointer to RType wrapping the current key
         */
        [[nodiscard]] virtual std::shared_ptr<RType> key() = 0;
        
        /**
         * @brief Get the current value
         * 
         * For set-like containers, returns the same as key().
         * 
         * @return Shared pointer to RType wrapping the current value
         */
        [[nodiscard]] virtual std::shared_ptr<RType> value() = 0;
        
        /**
         * @brief Move to the next element
         * @return true if moved to a valid element, false if at end
         */
        virtual bool next() = 0;
        
        /**
         * @brief Reset iterator to the beginning
         */
        virtual void reset() = 0;
    };
    
    /**
     * @brief Get an iterator to the beginning of the container
     * @return Unique pointer to KeyValueIterator
     */
    [[nodiscard]] virtual std::unique_ptr<KeyValueIterator> begin() = 0;
};

} // namespace rttm


namespace rttm {
namespace detail {

// Forward declaration for RType creation helper
std::shared_ptr<RType> create_rtype_for_value(void* ptr, const std::type_info& type_info);

/**
 * @brief Concrete implementation of ISequentialContainer for std::vector, std::list, std::deque
 * 
 * @tparam Container The container type (e.g., std::vector<int>)
 */
template<SequentialContainer Container>
class SequentialContainerImpl : public ISequentialContainer {
public:
    using value_type = typename Container::value_type;
    using iterator = typename Container::iterator;
    
    /**
     * @brief Construct wrapper around existing container
     * @param container Pointer to the container to wrap
     */
    explicit SequentialContainerImpl(Container* container)
        : container_(container) {}
    
    // ==================== Size and State ====================
    
    [[nodiscard]] std::size_t size() const override {
        return container_->size();
    }
    
    [[nodiscard]] bool empty() const override {
        return container_->empty();
    }
    
    void clear() override {
        container_->clear();
    }
    
    // ==================== Element Access ====================
    
    [[nodiscard]] std::shared_ptr<RType> at(std::size_t index) override {
        if (index >= container_->size()) {
            throw std::out_of_range("Index out of range: " + std::to_string(index));
        }
        
        // Get iterator to element
        auto it = container_->begin();
        std::advance(it, index);
        
        return create_element_rtype(&(*it));
    }
    
    void push_back(const std::any& value) override {
        container_->push_back(std::any_cast<value_type>(value));
    }
    
    void pop_back() override {
        if (container_->empty()) {
            throw std::out_of_range("Cannot pop_back from empty container");
        }
        container_->pop_back();
    }
    
    // ==================== Iterator ====================
    
    class IteratorImpl : public Iterator {
    public:
        IteratorImpl(Container* container)
            : container_(container)
            , current_(container->begin())
            , end_(container->end()) {}
        
        [[nodiscard]] bool has_current() const override {
            return current_ != end_;
        }
        
        [[nodiscard]] std::shared_ptr<RType> current() override {
            if (current_ == end_) {
                return nullptr;
            }
            return SequentialContainerImpl::create_element_rtype(&(*current_));
        }
        
        bool next() override {
            if (current_ != end_) {
                ++current_;
            }
            return current_ != end_;
        }
        
        void reset() override {
            current_ = container_->begin();
            end_ = container_->end();
        }
        
    private:
        Container* container_;
        iterator current_;
        iterator end_;
    };
    
    [[nodiscard]] std::unique_ptr<Iterator> begin() override {
        return std::make_unique<IteratorImpl>(container_);
    }
    
private:
    Container* container_;
    
    /**
     * @brief Create an RType wrapper for a container element
     */
    static std::shared_ptr<RType> create_element_rtype(value_type* element);
};

/**
 * @brief Concrete implementation of IAssociativeContainer for map-like containers
 * 
 * @tparam Container The container type (e.g., std::map<std::string, int>)
 */
template<KeyValueContainer Container>
class KeyValueContainerImpl : public IAssociativeContainer {
public:
    using key_type = typename Container::key_type;
    using mapped_type = typename Container::mapped_type;
    using iterator = typename Container::iterator;
    
    explicit KeyValueContainerImpl(Container* container)
        : container_(container) {}
    
    // ==================== Size and State ====================
    
    [[nodiscard]] std::size_t size() const override {
        return container_->size();
    }
    
    [[nodiscard]] bool empty() const override {
        return container_->empty();
    }
    
    void clear() override {
        container_->clear();
    }
    
    // ==================== Lookup Operations ====================
    
    [[nodiscard]] std::shared_ptr<RType> find(const std::any& key) override {
        auto it = container_->find(std::any_cast<key_type>(key));
        if (it == container_->end()) {
            return nullptr;
        }
        return create_value_rtype(&(it->second));
    }
    
    [[nodiscard]] bool contains(const std::any& key) override {
        return container_->contains(std::any_cast<key_type>(key));
    }
    
    // ==================== Modification Operations ====================
    
    void insert(const std::any& key, const std::any& value) override {
        (*container_)[std::any_cast<key_type>(key)] = std::any_cast<mapped_type>(value);
    }
    
    bool erase(const std::any& key) override {
        return container_->erase(std::any_cast<key_type>(key)) > 0;
    }
    
    // ==================== Iterator ====================
    
    class KeyValueIteratorImpl : public KeyValueIterator {
    public:
        KeyValueIteratorImpl(Container* container)
            : container_(container)
            , current_(container->begin())
            , end_(container->end()) {}
        
        [[nodiscard]] bool has_current() const override {
            return current_ != end_;
        }
        
        [[nodiscard]] std::shared_ptr<RType> key() override {
            if (current_ == end_) {
                return nullptr;
            }
            return KeyValueContainerImpl::create_key_rtype(
                const_cast<key_type*>(&(current_->first)));
        }
        
        [[nodiscard]] std::shared_ptr<RType> value() override {
            if (current_ == end_) {
                return nullptr;
            }
            return KeyValueContainerImpl::create_value_rtype(&(current_->second));
        }
        
        bool next() override {
            if (current_ != end_) {
                ++current_;
            }
            return current_ != end_;
        }
        
        void reset() override {
            current_ = container_->begin();
            end_ = container_->end();
        }
        
    private:
        Container* container_;
        iterator current_;
        iterator end_;
    };
    
    [[nodiscard]] std::unique_ptr<KeyValueIterator> begin() override {
        return std::make_unique<KeyValueIteratorImpl>(container_);
    }
    
private:
    Container* container_;
    
    static std::shared_ptr<RType> create_key_rtype(key_type* key);
    static std::shared_ptr<RType> create_value_rtype(mapped_type* value);
};

/**
 * @brief Concrete implementation of IAssociativeContainer for set-like containers
 * 
 * @tparam Container The container type (e.g., std::set<int>)
 */
template<AssociativeContainer Container>
requires (!KeyValueContainer<Container>)
class SetContainerImpl : public IAssociativeContainer {
public:
    using key_type = typename Container::key_type;
    using iterator = typename Container::iterator;
    
    explicit SetContainerImpl(Container* container)
        : container_(container) {}
    
    // ==================== Size and State ====================
    
    [[nodiscard]] std::size_t size() const override {
        return container_->size();
    }
    
    [[nodiscard]] bool empty() const override {
        return container_->empty();
    }
    
    void clear() override {
        container_->clear();
    }
    
    // ==================== Lookup Operations ====================
    
    [[nodiscard]] std::shared_ptr<RType> find(const std::any& key) override {
        auto it = container_->find(std::any_cast<key_type>(key));
        if (it == container_->end()) {
            return nullptr;
        }
        // For sets, the key is the value
        return create_element_rtype(const_cast<key_type*>(&(*it)));
    }
    
    [[nodiscard]] bool contains(const std::any& key) override {
        return container_->contains(std::any_cast<key_type>(key));
    }
    
    // ==================== Modification Operations ====================
    
    void insert(const std::any& key, const std::any& /*value*/) override {
        // For sets, only the key matters
        container_->insert(std::any_cast<key_type>(key));
    }
    
    bool erase(const std::any& key) override {
        return container_->erase(std::any_cast<key_type>(key)) > 0;
    }
    
    // ==================== Iterator ====================
    
    class SetIteratorImpl : public KeyValueIterator {
    public:
        SetIteratorImpl(Container* container)
            : container_(container)
            , current_(container->begin())
            , end_(container->end()) {}
        
        [[nodiscard]] bool has_current() const override {
            return current_ != end_;
        }
        
        [[nodiscard]] std::shared_ptr<RType> key() override {
            if (current_ == end_) {
                return nullptr;
            }
            return SetContainerImpl::create_element_rtype(
                const_cast<key_type*>(&(*current_)));
        }
        
        [[nodiscard]] std::shared_ptr<RType> value() override {
            // For sets, value is the same as key
            return key();
        }
        
        bool next() override {
            if (current_ != end_) {
                ++current_;
            }
            return current_ != end_;
        }
        
        void reset() override {
            current_ = container_->begin();
            end_ = container_->end();
        }
        
    private:
        Container* container_;
        iterator current_;
        iterator end_;
    };
    
    [[nodiscard]] std::unique_ptr<KeyValueIterator> begin() override {
        return std::make_unique<SetIteratorImpl>(container_);
    }
    
private:
    Container* container_;
    
    static std::shared_ptr<RType> create_element_rtype(key_type* element);
};

} // namespace detail


// ==================== Container Factory Functions ====================

/**
 * @brief Create an ISequentialContainer wrapper for a container
 * 
 * @tparam Container The container type
 * @param container Pointer to the container
 * @return Unique pointer to ISequentialContainer
 */
template<SequentialContainer Container>
std::unique_ptr<ISequentialContainer> make_sequential_container(Container* container) {
    return std::make_unique<detail::SequentialContainerImpl<Container>>(container);
}

/**
 * @brief Create an IAssociativeContainer wrapper for a key-value container
 * 
 * @tparam Container The container type
 * @param container Pointer to the container
 * @return Unique pointer to IAssociativeContainer
 */
template<KeyValueContainer Container>
std::unique_ptr<IAssociativeContainer> make_associative_container(Container* container) {
    return std::make_unique<detail::KeyValueContainerImpl<Container>>(container);
}

/**
 * @brief Create an IAssociativeContainer wrapper for a set-like container
 * 
 * @tparam Container The container type
 * @param container Pointer to the container
 * @return Unique pointer to IAssociativeContainer
 */
template<AssociativeContainer Container>
requires (!KeyValueContainer<Container>)
std::unique_ptr<IAssociativeContainer> make_set_container(Container* container) {
    return std::make_unique<detail::SetContainerImpl<Container>>(container);
}

// ==================== Container Category Detection ====================

/**
 * @brief Enum for container categories
 */
enum class ContainerCategory {
    None,           ///< Not a container
    Sequential,     ///< Sequential container (vector, list, deque)
    Associative     ///< Associative container (map, set, etc.)
};

/**
 * @brief Detect the category of a container type at compile time
 */
template<typename T>
constexpr ContainerCategory detect_container_category() {
    if constexpr (SequentialContainer<T>) {
        return ContainerCategory::Sequential;
    } else if constexpr (AssociativeContainer<T>) {
        return ContainerCategory::Associative;
    } else {
        return ContainerCategory::None;
    }
}

/**
 * @brief Check if a type is a sequential container
 */
template<typename T>
constexpr bool is_sequential() {
    return detect_container_category<T>() == ContainerCategory::Sequential;
}

/**
 * @brief Check if a type is an associative container
 */
template<typename T>
constexpr bool is_associative() {
    return detect_container_category<T>() == ContainerCategory::Associative;
}

} // namespace rttm

#endif // RTTM_DETAIL_CONTAINER_HPP
