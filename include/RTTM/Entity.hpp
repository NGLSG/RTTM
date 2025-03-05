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
}

#endif // ENTITY_H
