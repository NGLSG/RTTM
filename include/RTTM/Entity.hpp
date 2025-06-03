#ifndef RTTMENTITY_H
#define RTTMENTITY_H

#include <memory>
#include <stdexcept>
#include <string>
#include <optional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <array>
#include <set>
#include <functional>
#include <vector>
#include <iostream>

#include "Object.hpp"

namespace RTTM
{
    struct UUID
    {
    public:
        UUID() : data({0})
        {
        }

        static UUID New();

        std::string toString() const;

        std::array<uint32_t, 4> GetData() const
        {
            return data;
        }

        void SetData(const std::array<uint32_t, 4> data)
        {
            this->data = data;
        }

        bool operator==(const UUID& other) const;

        bool operator<(const UUID& other) const;

        std::ostream& operator<<(std::ostream& os) const;

    private:
        inline static std::set<UUID> uuids;
        std::array<uint32_t, 4> data;
    };

    // 组件基类，所有组件都应该从这里继承
    class Component
    {
    public:
        virtual ~Component() = default;

        // 获取组件类型名称，用于调试
        virtual std::string GetTypeName() const = 0;

        // 获取组件类型ID
        virtual std::type_index GetTypeIndex() const = 0;

        // 检查是否为指定类型（包括继承关系）
        template <typename T>
        bool IsOfType() const
        {
            return dynamic_cast<const T*>(this) != nullptr;
        }

        // 转换为指定类型（包括继承关系）
        template <typename T>
        T* As()
        {
            return dynamic_cast<T*>(this);
        }

        template <typename T>
        const T* As() const
        {
            return dynamic_cast<const T*>(this);
        }
    };

    class Registry
    {
    public:
        using EntityMap = std::unordered_map<std::type_index, std::shared_ptr<Component>>;
        using ComponentList = std::vector<std::shared_ptr<Component>>;

    private:
        EntityMap entities;
        ComponentList allComponents; // 保存所有组件的列表，用于遍历查找

    public:
        Registry() = default;

        template <typename T>
        T& Emplace()
        {
            static_assert(std::is_base_of_v<Component, T>, "组件必须继承自 Component 基类");
            if (Contains<T>())
            {
                return Get<T>().value();
            }
            auto component = std::make_shared<T>();
            entities[std::type_index(typeid(T))] = component;
            allComponents.push_back(component);
            return *component;
        }

        template <typename T, typename... Args>
        T& Emplace(Args&&... args)
        {
            static_assert(std::is_base_of_v<Component, T>, "组件必须继承自 Component 基类");
            if (Contains<T>())
            {
                std::cerr << "组件已存在: " << Object::GetTypeName<T>() << std::endl;
                return Get<T>().value();
            }
            auto component = std::make_shared<T>(std::forward<Args>(args)...);
            entities[std::type_index(typeid(T))] = component;
            allComponents.push_back(component);
            return *component;
        }

        template <typename T>
        T& GetOrEmplace()
        {
            if (Contains<T>())
            {
                return Get<T>().value();
            }
            return Emplace<T>();
        }

        template <typename T, typename... Args>
        T& GetOrEmplace(Args&&... args)
        {
            if (Contains<T>())
            {
                return Get<T>().value();
            }
            return Emplace<T>(std::forward<Args>(args)...);
        }

        // 精确类型获取
        template <typename T>
        std::optional<std::reference_wrapper<T>> Get()
        {
            const auto& typeIndex = std::type_index(typeid(T));
            auto it = entities.find(typeIndex);
            if (it == entities.end())
            {
                return std::nullopt;
            }
            auto ptr = std::static_pointer_cast<T>(it->second);
            if (!ptr)
            {
                return std::nullopt;
            }
            return *ptr;
        }

        // 动态类型获取（支持继承关系）
        template <typename T>
        std::optional<std::reference_wrapper<T>> GetDynamic()
        {
            static_assert(std::is_base_of_v<Component, T>, "类型必须继承自 Component 基类");

            // 首先尝试精确匹配
            auto exact = Get<T>();
            if (exact.has_value())
            {
                return exact;
            }

            // 遍历所有组件查找继承关系
            for (auto& component : allComponents)
            {
                if (auto derived = dynamic_cast<T*>(component.get()))
                {
                    return *derived;
                }
            }

            return std::nullopt;
        }

        // 获取所有指定类型的组件（包括继承的子类）
        template <typename T>
        std::vector<std::reference_wrapper<T>> GetAll()
        {
            static_assert(std::is_base_of_v<Component, T>, "类型必须继承自 Component 基类");

            std::vector<std::reference_wrapper<T>> results;

            for (auto& component : allComponents)
            {
                if (auto derived = dynamic_cast<T*>(component.get()))
                {
                    results.emplace_back(*derived);
                }
            }

            return results;
        }

        template <typename T>
        bool Contains()
        {
            return entities.find(std::type_index(typeid(T))) != entities.end();
        }

        // 检查是否包含指定类型（支持继承关系）
        template <typename T>
        bool ContainsDynamic()
        {
            static_assert(std::is_base_of_v<Component, T>, "类型必须继承自 Component 基类");

            // 首先检查精确匹配
            if (Contains<T>())
            {
                return true;
            }

            // 检查继承关系
            for (auto& component : allComponents)
            {
                if (dynamic_cast<T*>(component.get()) != nullptr)
                {
                    return true;
                }
            }

            return false;
        }

        template <typename T>
        void Remove()
        {
            auto typeIndex = std::type_index(typeid(T));
            auto it = entities.find(typeIndex);
            if (it != entities.end())
            {
                // 从组件列表中移除
                auto componentPtr = it->second;
                allComponents.erase(
                    std::remove(allComponents.begin(), allComponents.end(), componentPtr),
                    allComponents.end()
                );

                // 从类型映射中移除
                entities.erase(it);
            }
        }

        // 移除指定类型的所有组件（包括继承的子类）
        template <typename T>
        void RemoveAll()
        {
            static_assert(std::is_base_of_v<Component, T>, "类型必须继承自 Component 基类");

            // 收集要移除的组件
            std::vector<std::shared_ptr<Component>> toRemove;
            std::vector<std::type_index> typesToRemove;

            for (auto& component : allComponents)
            {
                if (dynamic_cast<T*>(component.get()) != nullptr)
                {
                    toRemove.push_back(component);
                }
            }

            // 从组件列表中移除
            for (auto& component : toRemove)
            {
                allComponents.erase(
                    std::remove(allComponents.begin(), allComponents.end(), component),
                    allComponents.end()
                );
            }

            // 从类型映射中移除
            for (auto it = entities.begin(); it != entities.end();)
            {
                if (std::find(toRemove.begin(), toRemove.end(), it->second) != toRemove.end())
                {
                    it = entities.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        // 获取所有组件
        const ComponentList& GetAllComponents() const
        {
            return allComponents;
        }
    };

    class Entity
    {
    private:
        RTTM::Registry registry;

    protected:
        UUID entityID;

    public:
        UUID GetEntityID() const
        {
            return entityID;
        }

        Entity()
        {
        }

        virtual ~Entity() = default;

        // 精确类型检查
        template <typename T>
        bool HasComponent()
        {
            return registry.Contains<T>();
        }

        // 动态类型检查（支持继承关系）
        template <typename T>
        bool HasComponentDynamic()
        {
            return registry.ContainsDynamic<T>();
        }

        // 精确类型获取
        template <typename T>
        T& GetComponent()
        {
            return registry.Get<T>().value();
        }

        // 动态类型获取（支持继承关系）
        template <typename T>
        T& GetComponentDynamic()
        {
            auto result = registry.GetDynamic<T>();
            if (!result.has_value())
            {
                throw std::runtime_error("组件不存在: " + Object::GetTypeName<T>());
            }
            return result.value();
        }

        // 尝试获取组件（动态类型，返回可选值）
        template <typename T>
        std::optional<std::reference_wrapper<T>> TryGetComponent()
        {
            return registry.GetDynamic<T>();
        }

        // 获取所有指定类型的组件
        template <typename T>
        std::vector<std::reference_wrapper<T>> GetComponents()
        {
            return registry.GetAll<T>();
        }

        template <typename T, typename... Args>
        T& AddComponent(Args&&... args)
        {
            return registry.Emplace<T>(std::forward<Args>(args)...);
        }

        template <typename T, typename... Args>
        T& GetOrAddComponent(Args&&... args)
        {
            if (HasComponent<T>())
            {
                return GetComponent<T>();
            }
            return AddComponent<T>(std::forward<Args>(args)...);
        }

        template <typename T>
        void RemoveComponent()
        {
            registry.Remove<T>();
        }

        // 移除所有指定类型的组件（包括继承的子类）
        template <typename T>
        void RemoveComponents()
        {
            registry.RemoveAll<T>();
        }

        // 遍历所有组件
        template <typename Func>
        void ForEachComponent(Func&& func)
        {
            for (auto& component : registry.GetAllComponents())
            {
                func(*component);
            }
        }

        // 遍历指定类型的所有组件
        template <typename T, typename Func>
        void ForEachComponent(Func&& func)
        {
            auto components = GetComponents<T>();
            for (auto& component : components)
            {
                func(component.get());
            }
        }
    };


    //特化的 EntityWithComponents 类，用于自动添加组件
    template <typename... ComponentTypes>
    class EntityWithComponents : public Entity
    {
    public:
        EntityWithComponents()
        {
            // 在构造函数中添加所需组件
            (GetOrAddComponent<ComponentTypes>(), ...);
        }
    };
}

#define REQUIRE_COMPONENTS(...)  public RTTM::EntityWithComponents<__VA_ARGS__>

namespace std
{
    template <>
    struct hash<RTTM::UUID>
    {
        size_t operator()(const RTTM::UUID& uuid) const noexcept
        {
            const auto& data = uuid.GetData();
            // 组合四个 32-bit 整数到哈希值
            size_t hash = 0;
            for (uint32_t val : data)
            {
                // 使用 Boost 风格的哈希组合（避免冲突）
                hash ^= std::hash<uint32_t>{}(val) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };
} // namespace std
#endif // RTTMENTITY_H
