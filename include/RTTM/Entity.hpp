#ifndef RTTMENTITY_H
#define RTTMENTITY_H

#include <memory>
#include <stdexcept>
#include <stdexcept>
#include <string>
#include <optional>
#include <typeindex>
#include <unordered_map>
#include <utility>

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

    class Registry
    {
    public:
        using EntityMap = std::unordered_map<std::type_index, std::shared_ptr<void>>;

    private:
        EntityMap entities;

    public:
        Registry() = default;

        template <typename T>
        void Emplace()
        {
            static_assert(std::is_class_v<T>, "Entity must be a class/struct type");
            if (Contains<T>())
            {
                std::cerr << "Entity already exists: " << Object::GetTypeName<T>() << std::endl;
                return;
            }
            entities[std::type_index(typeid(T))] = std::make_shared<T>();
        }

        template <typename T, typename... Args>
        void Emplace(Args&&... args)
        {
            static_assert(std::is_class_v<T>, "Entity must be a class/struct type");
            if (Contains<T>())
            {
                std::cerr << "Entity already exists: " << Object::GetTypeName<T>() << std::endl;
                return;
            }
            entities[std::type_index(typeid(T))] = std::make_shared<T>(std::forward<Args>(args)...);
        }

        template <typename T>
        T& GetOrEmplace()
        {
            if (Contains<T>())
            {
                return Get<T>().value();
            }
            Emplace<T>();
            return Get<T>().value();
        }

        template <typename T, typename... Args>
        T& GetOrEmplace(Args&&... args)
        {
            if (Contains<T>())
            {
                return Get<T>().value();
            }
            Emplace<T>(std::forward<Args>(args)...);
            return Get<T>().value();
        }

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

        template <typename T>
        bool Contains() const
        {
            return entities.find(std::type_index(typeid(T))) != entities.end();
        }

        template <typename T>
        void Remove()
        {
            entities.erase(std::type_index(typeid(T)));
        }
    };

    class Entity;

    namespace ComponentRequirement
    {
        inline std::vector<std::type_index> pendingRequirements;

        inline std::unordered_map<std::type_index, std::vector<std::type_index>> classRequirements;
        inline std::unordered_map<std::type_index, std::function<void(void*)>> componentCreators;

        template <typename ComponentType>
        void AddPendingRequirement()
        {
            pendingRequirements.push_back(std::type_index(typeid(ComponentType)));

            auto typeIdx = std::type_index(typeid(ComponentType));
            if (componentCreators.find(typeIdx) == componentCreators.end())
            {
                componentCreators[typeIdx] = [](void* entityPtr)
                {
                    auto entity = static_cast<RTTM::Entity*>(entityPtr);
                    entity->template GetOrAddComponent<ComponentType>();
                };
            }
        }

        template <typename ClassType>
        void CaptureRequirements()
        {
            if (!pendingRequirements.empty())
            {
                classRequirements[std::type_index(typeid(ClassType))] = std::move(pendingRequirements);
                pendingRequirements.clear();
            }
        }

        template <typename ClassType>
        bool HasRequirements()
        {
            return classRequirements.find(std::type_index(typeid(ClassType))) != classRequirements.end();
        }

        template <typename ClassType, typename Entity>
        void ApplyRequirements(Entity& entity)
        {
            auto it = classRequirements.find(std::type_index(typeid(ClassType)));
            if (it != classRequirements.end())
            {
                for (const auto& compType : it->second)
                {
                    auto creatorIt = componentCreators.find(compType);
                    if (creatorIt != componentCreators.end())
                    {
                        creatorIt->second(static_cast<void*>(&entity));
                    }
                }
            }
        }
    }

#define REQUIRE_COMPONENT(ComponentType) \
    namespace { struct UNIQUE_NAME_HELPER_##__LINE__ { \
        UNIQUE_NAME_HELPER_##__LINE__() { \
            RTTM::ComponentRequirement::AddPendingRequirement<ComponentType>(); \
        } \
    } UNIQUE_NAME_##__LINE__; }

#define REQUIRE_COMPONENTS(...) \
    namespace { struct UNIQUE_NAME_HELPER_##__LINE__ { \
        UNIQUE_NAME_HELPER_##__LINE__() { \
            int dummy[] = {(RTTM::ComponentRequirement::AddPendingRequirement<__VA_ARGS__>(), 0)...}; \
            (void)dummy; \
        } \
    } UNIQUE_NAME_##__LINE__; }

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
            ComponentRequirement::CaptureRequirements<std::remove_cv_t<std::remove_reference_t<decltype(*this)>>>();

            ComponentRequirement::ApplyRequirements<std::remove_cv_t<std::remove_reference_t<decltype(*this)>>>(*this);
        }

        virtual ~Entity() = default;

        template <typename T>
        bool HasComponent()
        {
            return registry.Contains<T>();
        }

        template <typename T>
        T& GetComponent()
        {
            return registry.Get<T>().value();
        }

        template <typename T, typename... Args>
        void AddComponent(Args&&... args)
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
            AddComponent<T>(std::forward<Args>(args)...);
            return GetComponent<T>();
        }

        template <typename T>
        void RemoveComponent()
        {
            registry.Remove<T>();
        }
    };
}

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
#endif // ENTITY_H
