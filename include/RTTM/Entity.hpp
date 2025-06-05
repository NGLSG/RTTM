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
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "Object.hpp"

namespace RTTM
{
    // 前向声明
    class Entity;
    class ComponentBase;

    // 组件工厂类型定义
    using ComponentFactory = std::function<std::shared_ptr<ComponentBase>(Entity*)>;

    // 类型检查器函数类型定义
    using TypeChecker = std::function<bool(const std::shared_ptr<ComponentBase>&)>;

    // 检测 IsSingleton 方法
    template <typename T>
    class has_is_singleton_method
    {
    private:
        template <typename U>
        static auto test(int) -> decltype(std::declval<U>().IsSingleton(), std::true_type{});
        template <typename>
        static std::false_type test(...);

    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    // 基础组件类
    class ComponentBase
    {
    private:
        Entity* ownerEntity; // 拥有此组件的实体指针
    protected:
        template <typename... Args>
        static std::vector<std::type_index> __getDependencies()
        {
            return {std::type_index(typeid(std::remove_reference_t<Args>))...};
        }

    public:
        ComponentBase() : ownerEntity(nullptr)
        {
        }

        virtual ~ComponentBase() = default;

        // 获取组件类型名称，用于调试
        virtual std::string GetTypeName() const = 0;

        // 获取组件类型ID
        virtual std::type_index GetTypeIndex() const = 0;

        // 检查是否为单例组件（一个实体只能有一个此类型的组件）
        virtual bool IsSingleton() const { return false; }

        // 获取单例基类类型（如果是单例组件）
        virtual std::optional<std::type_index> GetSingletonBaseType() const { return std::nullopt; }

        // 获取组件依赖的其他组件类型
        virtual std::vector<std::type_index> GetDependencies() const { return {}; }

        // 设置拥有者实体
        void SetOwner(Entity* entity) { ownerEntity = entity; }

        // 获取拥有者实体
        Entity* GetOwner() const { return ownerEntity; }

        // 从拥有者实体获取其他组件
        template <typename T>
        T& GetComponent();

        template <typename T>
        std::optional<std::reference_wrapper<T>> TryGetComponent();

        template <typename T>
        bool HasComponent();

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

    // 组件类型注册器
    class ComponentRegistry
    {
    private:
        inline static std::unordered_map<std::type_index, ComponentFactory> factories;
        inline static std::unordered_map<std::type_index, std::string> typeNames;
        // 存储每个类型的继承关系检查器
        inline static std::unordered_map<std::type_index, TypeChecker> typeCheckers;
        // 存储类型的基类关系
        inline static std::unordered_map<std::type_index, std::vector<std::type_index>> inheritanceMap;
        // 存储单例组件的单例基类映射 (派生类型 -> 单例基类型)
        inline static std::unordered_map<std::type_index, std::type_index> singletonBaseMap;

    public:
        // 注册组件类型
        template <typename T>
        static void RegisterComponent()
        {
            static_assert(std::is_base_of_v<ComponentBase, T>, "注册的类型必须继承自ComponentBase");

            auto typeIndex = std::type_index(typeid(T));

            // 避免重复注册
            if (factories.find(typeIndex) != factories.end())
            {
                return;
            }

            // 注册工厂函数
            if constexpr (!std::is_abstract_v<T>)
            {
                factories[typeIndex] = [](Entity* owner) -> std::shared_ptr<ComponentBase>
                {
                    auto component = std::make_shared<T>();
                    component->SetOwner(owner);
                    return component;
                };
            }

            // 注册类型名称
            typeNames[typeIndex] = Object::GetTypeName<T>();

            // 注册类型检查器
            typeCheckers[typeIndex] = [](const std::shared_ptr<ComponentBase>& component) -> bool
            {
                return dynamic_cast<T*>(component.get()) != nullptr;
            };

            // 注册继承关系
            RegisterInheritanceRelation<T>();

            // 注册单例基类关系
            RegisterSingletonBaseRelation<T>();
        }

        // 根据类型索引创建组件
        static std::shared_ptr<ComponentBase> CreateComponent(const std::type_index& typeIndex, Entity* owner)
        {
            auto it = factories.find(typeIndex);
            if (it != factories.end())
            {
                return it->second(owner);
            }
            return nullptr;
        }

        // 获取类型名称
        static std::string GetTypeName(const std::type_index& typeIndex)
        {
            auto it = typeNames.find(typeIndex);
            if (it != typeNames.end())
            {
                return it->second;
            }
            return "未知类型";
        }

        // 检查类型是否已注册
        static bool IsRegistered(const std::type_index& typeIndex)
        {
            return factories.find(typeIndex) != factories.end();
        }

        // 检查组件是否是指定类型的实例（包括继承关系）
        static bool IsInstanceOf(const std::shared_ptr<ComponentBase>& component, const std::type_index& targetType)
        {
            auto it = typeCheckers.find(targetType);
            if (it != typeCheckers.end())
            {
                return it->second(component);
            }
            return false;
        }

        // 获取类型的所有基类
        static std::vector<std::type_index> GetBaseTypes(const std::type_index& typeIndex)
        {
            auto it = inheritanceMap.find(typeIndex);
            if (it != inheritanceMap.end())
            {
                return it->second;
            }
            return {};
        }

        // 获取组件的单例基类类型
        static std::optional<std::type_index> GetSingletonBaseType(const std::type_index& typeIndex)
        {
            auto it = singletonBaseMap.find(typeIndex);
            if (it != singletonBaseMap.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        // 检查组件是否是单例组件
        static bool IsSingletonComponent(const std::type_index& typeIndex)
        {
            return singletonBaseMap.find(typeIndex) != singletonBaseMap.end();
        }

        // 检查两个类型是否兼容（可以替换）
        template <typename T1, typename T2>
        static bool AreCompatibleForSwap()
        {
            auto t1Index = std::type_index(typeid(T1));
            auto t2Index = std::type_index(typeid(T2));

            // 检查是否是相同类型
            if (t1Index == t2Index)
            {
                return true;
            }

            // 检查是否具有继承关系
            if (std::is_base_of_v<T1, T2> || std::is_base_of_v<T2, T1>)
            {
                return true;
            }

            // 对于单例组件，检查是否具有相同的单例基类
            auto t1Base = GetSingletonBaseType(t1Index);
            auto t2Base = GetSingletonBaseType(t2Index);

            // 如果 T1 是基类且 T2 有单例基类，检查 T1 是否就是 T2 的单例基类
            if (!t1Base.has_value() && t2Base.has_value())
            {
                return t1Index == t2Base.value();
            }

            // 如果 T2 是基类且 T1 有单例基类，检查 T2 是否就是 T1 的单例基类
            if (!t2Base.has_value() && t1Base.has_value())
            {
                return t2Index == t1Base.value();
            }

            // 正常情况：两者都有单例基类
            return t1Base.has_value() && t2Base.has_value() &&
                t1Base.value() == t2Base.value();
        }

        // 根据基类类型查找现有的具体组件类型
        static std::optional<std::type_index> FindExistingComponentByBase(
            const std::type_index& baseType,
            const std::unordered_map<std::type_index, std::vector<std::shared_ptr<ComponentBase>>>& entities,
            const std::unordered_map<std::type_index, std::shared_ptr<ComponentBase>>& singletonComponents)
        {
            // 首先检查单例组件映射
            auto singletonIt = singletonComponents.find(baseType);
            if (singletonIt != singletonComponents.end())
            {
                return singletonIt->second->GetTypeIndex();
            }

            // 然后检查所有实体组件，查找继承自该基类的组件
            for (const auto& [typeIndex, componentList] : entities)
            {
                if (!componentList.empty() && IsInstanceOf(componentList[0], baseType))
                {
                    return typeIndex;
                }
            }

            return std::nullopt;
        }

    private:
        // 注册继承关系
        template <typename T>
        static void RegisterInheritanceRelation()
        {
            auto derivedTypeIndex = std::type_index(typeid(T));
            std::vector<std::type_index> baseTypes;

            // 收集所有基类的type_index
            CollectBaseTypes<T>(baseTypes);

            inheritanceMap[derivedTypeIndex] = baseTypes;
        }

        // 注册单例基类关系
        template <typename T>
        static void RegisterSingletonBaseRelation()
        {
            auto typeIndex = std::type_index(typeid(T));

            // 查找单例基类
            auto singletonBase = FindSingletonBase<T>();
            if (singletonBase.has_value())
            {
                singletonBaseMap.emplace(typeIndex, singletonBase.value());
            }
        }

        // 查找单例基类 - 使用 SFINAE 替代 requires (C++17 兼容)
        template <typename T>
        static std::optional<std::type_index> FindSingletonBase()
        {
            // 使用 SFINAE 检查自身是否有 IsSingleton 方法并返回 true
            if constexpr (has_is_singleton_method<T>::value)
            {
                if constexpr (!std::is_abstract_v<T>)
                {
                    T temp;
                    if (temp.IsSingleton())
                    {
                        return std::type_index(typeid(T));
                    }
                }
            }

            // 递归检查基类
            return FindSingletonBaseRecursive<T>();
        }

        // 递归查找单例基类
        template <typename T>
        static std::optional<std::type_index> FindSingletonBaseRecursive()
        {
            // 检查是否继承自某个已知的单例基类
            // 这里使用一个简化的实现，实际项目中可能需要更复杂的机制
            return CheckKnownSingletonBases<T>();
        }

        // 检查已知的单例基类
        template <typename T>
        static std::optional<std::type_index> CheckKnownSingletonBases()
        {
            // 遍历已注册的单例基类，检查T是否继承自它们
            for (const auto& [derivedType, singletonBaseType] : singletonBaseMap)
            {
                // 检查T是否可以转换为已知的单例基类
                auto baseChecker = typeCheckers.find(singletonBaseType);
                if (baseChecker != typeCheckers.end())
                {
                    // 创建临时对象进行类型检查
                    if constexpr (!std::is_abstract_v<T>)
                    {
                        auto temp = std::make_shared<T>();
                        if (baseChecker->second(temp))
                        {
                            return singletonBaseType;
                        }
                    }
                }
            }
            return std::nullopt;
        }

        // 递归收集基类类型
        template <typename T>
        static void CollectBaseTypes(std::vector<std::type_index>& baseTypes)
        {
            // 检查T是否继承自ComponentBase的直接基类
            if constexpr (std::is_base_of_v<ComponentBase, T> && !std::is_same_v<T, ComponentBase>)
            {
                // 使用SFINAE技术检测基类
                CollectDirectBaseTypes<T>(baseTypes);
            }
        }

        // 使用模板特化收集直接基类
        template <typename T>
        static void CollectDirectBaseTypes(std::vector<std::type_index>& baseTypes)
        {
            // 检查是否继承自已知的组件基类
            if constexpr (std::is_convertible_v<T*, ComponentBase*>)
            {
                // 尝试检测可能的基类
                TryAddBaseType<T, ComponentBase>(baseTypes);
            }
        }

        // 尝试添加基类类型
        template <typename Derived, typename Base>
        static void TryAddBaseType(std::vector<std::type_index>& baseTypes)
        {
            if constexpr (std::is_base_of_v<Base, Derived> && !std::is_same_v<Derived, Base>)
            {
                auto baseTypeIndex = std::type_index(typeid(Base));
                if (std::find(baseTypes.begin(), baseTypes.end(), baseTypeIndex) == baseTypes.end())
                {
                    baseTypes.push_back(baseTypeIndex);
                }
            }
        }
    };

    // 静态注册助手类
    template <typename T>
    class StaticComponentRegistrar
    {
    public:
        StaticComponentRegistrar()
        {
            ComponentRegistry::RegisterComponent<T>();
        }
    };

    // 自动注册的组件基类模板
    template <typename Derived>
    class Component : public ComponentBase
    {
    private:
        // 静态注册实例 - 在类加载时自动注册
        inline static StaticComponentRegistrar<Derived> registrar_;

    public:
        Component() : ComponentBase()
        {
            // 构造函数中确保注册已完成
            (void)registrar_; // 强制引用静态成员
        }

        std::string GetTypeName() const override
        {
            return Object::GetTypeName<Derived>();
        }

        std::type_index GetTypeIndex() const override
        {
            return std::type_index(typeid(Derived));
        }
    };

    // 单例组件模板
    template <typename Derived>
    class SingletonComponent : public Component<Derived>
    {
    public:
        bool IsSingleton() const override { return true; }

        std::optional<std::type_index> GetSingletonBaseType() const override
        {
            return std::type_index(typeid(Derived));
        }
    };

    // 基于基类的单例组件模板
    template <typename SingletonBase, typename Derived>
    class SingletonComponentBase : public Component<Derived>
    {
        static_assert(std::is_base_of_v<ComponentBase, SingletonBase>, "SingletonBase必须继承自ComponentBase");

    public:
        bool IsSingleton() const override { return true; }

        std::optional<std::type_index> GetSingletonBaseType() const override
        {
            return std::type_index(typeid(SingletonBase));
        }
    };

    // 检查类型是否为单例组件的工具
    template <typename T>
    struct is_singleton_component
    {
        template <typename U>
        static auto test(int) -> decltype(std::declval<U>().IsSingleton(), std::true_type{});
        template <typename>
        static std::false_type test(...);

        static constexpr bool value = decltype(test<T>(0))::value;
    };

    // 组件注册表类
    class Registry
    {
    public:
        // 修改存储结构：支持每个类型存储多个组件实例
        using EntityMap = std::unordered_map<std::type_index, std::vector<std::shared_ptr<ComponentBase>>>;
        using ComponentList = std::vector<std::shared_ptr<ComponentBase>>;

    private:
        EntityMap entities; // 存储所有组件类型及其实例列表
        ComponentList allComponents; // 保存所有组件的列表，用于遍历查找
        std::set<std::type_index> requiredComponents; // 存储必需的组件类型
        // 单例组件存储: 单例基类类型 -> 组件实例
        std::unordered_map<std::type_index, std::shared_ptr<ComponentBase>> singletonComponents;
        Entity* ownerEntity; // 拥有此注册表的实体

    public:
        Registry(Entity* owner) : ownerEntity(owner)
        {
        }

        // 添加必需组件类型
        template <typename T>
        void AddRequiredComponent()
        {
            requiredComponents.insert(std::type_index(typeid(T)));
        }

        // 检查是否为必需组件
        template <typename T>
        bool IsRequiredComponent() const
        {
            return requiredComponents.find(std::type_index(typeid(T))) != requiredComponents.end();
        }

        // 添加组件的依赖
        template <typename T>
        void AddComponentDependencies()
        {
            if constexpr (!std::is_abstract_v<T>)
            {
                // 创建临时实例来获取依赖信息
                auto tempComponent = std::make_shared<T>();
                auto dependencies = tempComponent->GetDependencies();

                // 递归添加所有依赖组件
                for (const auto& depType : dependencies)
                {
                    AddDependencyByTypeIndex(depType);
                }
            }
        }

        template <typename T>
        T& Emplace()
        {
            static_assert(std::is_base_of_v<ComponentBase, T>, "组件必须继承自ComponentBase基类");

            // 检查是否为抽象类
            if constexpr (std::is_abstract_v<T>)
            {
                throw std::runtime_error("无法实例化抽象类组件: " + Object::GetTypeName<T>());
            }

            auto typeIndex = std::type_index(typeid(T));

            // 检查是否为单例组件
            bool isSingleton = is_singleton_component<T>::value;

            if (isSingleton)
            {
                // 创建临时实例来获取单例基类类型
                auto tempComponent = std::make_shared<T>();
                auto singletonBaseType = tempComponent->GetSingletonBaseType();

                if (singletonBaseType.has_value())
                {
                    auto baseTypeIndex = singletonBaseType.value();

                    // 检查是否已存在相同单例基类的组件
                    auto it = singletonComponents.find(baseTypeIndex);
                    if (it != singletonComponents.end())
                    {
                        // 获取现有组件的类型名称
                        std::string existingTypeName = it->second->GetTypeName();
                        std::string newTypeName = Object::GetTypeName<T>();
                        std::string baseTypeName = ComponentRegistry::GetTypeName(baseTypeIndex);

                        std::cerr << "错误: 无法添加组件 " << newTypeName
                            << "，因为已存在相同单例基类 " << baseTypeName
                            << " 的组件: " << existingTypeName << std::endl;

                        throw std::runtime_error("单例组件冲突: 已存在相同基类的组件");
                    }

                    // 先添加依赖组件
                    AddComponentDependencies<T>();

                    auto component = std::make_shared<T>();
                    component->SetOwner(ownerEntity);

                    // 单例组件存储
                    singletonComponents.emplace(baseTypeIndex, component);
                    entities[typeIndex].push_back(component);
                    allComponents.push_back(component);

                    return *component;
                }
            }

            // 非单例组件允许重复添加
            // 先添加依赖组件
            AddComponentDependencies<T>();

            auto component = std::make_shared<T>();
            component->SetOwner(ownerEntity);
            entities[typeIndex].push_back(component);
            allComponents.push_back(component);
            return *component;
        }

        template <typename T, typename... Args>
        T& Emplace(Args&&... args)
        {
            static_assert(std::is_base_of_v<ComponentBase, T>, "组件必须继承自ComponentBase基类");

            // 检查是否为抽象类
            if constexpr (std::is_abstract_v<T>)
            {
                throw std::runtime_error("无法实例化抽象类组件: " + Object::GetTypeName<T>());
            }

            auto typeIndex = std::type_index(typeid(T));

            // 检查是否为单例组件
            bool isSingleton = is_singleton_component<T>::value;

            if (isSingleton)
            {
                // 创建临时实例来获取单例基类类型
                auto tempComponent = std::make_shared<T>();
                auto singletonBaseType = tempComponent->GetSingletonBaseType();

                if (singletonBaseType.has_value())
                {
                    auto baseTypeIndex = singletonBaseType.value();

                    // 检查是否已存在相同单例基类的组件
                    auto it = singletonComponents.find(baseTypeIndex);
                    if (it != singletonComponents.end())
                    {
                        // 获取现有组件的类型名称
                        std::string existingTypeName = it->second->GetTypeName();
                        std::string newTypeName = Object::GetTypeName<T>();
                        std::string baseTypeName = ComponentRegistry::GetTypeName(baseTypeIndex);

                        std::cerr << "错误: 无法添加组件 " << newTypeName
                            << "，因为已存在相同单例基类 " << baseTypeName
                            << " 的组件: " << existingTypeName << std::endl;

                        throw std::runtime_error("单例组件冲突: 已存在相同基类的组件");
                    }

                    // 先添加依赖组件
                    AddComponentDependencies<T>();

                    auto component = std::make_shared<T>(std::forward<Args>(args)...);
                    component->SetOwner(ownerEntity);

                    // 单例组件存储
                    singletonComponents.emplace(baseTypeIndex, component);
                    entities[typeIndex].push_back(component);
                    allComponents.push_back(component);

                    return *component;
                }
            }

            // 非单例组件允许重复添加
            // 先添加依赖组件
            AddComponentDependencies<T>();

            auto component = std::make_shared<T>(std::forward<Args>(args)...);
            component->SetOwner(ownerEntity);
            entities[typeIndex].push_back(component);
            allComponents.push_back(component);
            return *component;
        }

        // 通用组件替换功能
        template <typename FromComponent, typename ToComponent, typename... Args>
        ToComponent& SwapComponent(Args&&... args)
        {
            static_assert(std::is_base_of_v<ComponentBase, FromComponent>, "FromComponent必须继承自ComponentBase");
            static_assert(std::is_base_of_v<ComponentBase, ToComponent>, "ToComponent必须继承自ComponentBase");
            static_assert(!std::is_abstract_v<ToComponent>, "ToComponent不能是抽象类");

            // 检查两个组件类型是否兼容
            if (!ComponentRegistry::AreCompatibleForSwap<FromComponent, ToComponent>())
            {
                std::string fromTypeName = Object::GetTypeName<FromComponent>();
                std::string toTypeName = Object::GetTypeName<ToComponent>();
                std::cerr << "错误: 无法替换组件，" << fromTypeName << " 和 " << toTypeName
                    << " 类型不兼容" << std::endl;
                throw std::runtime_error("组件替换失败: 类型不兼容");
            }

            auto fromTypeIndex = std::type_index(typeid(FromComponent));
            auto toTypeIndex = std::type_index(typeid(ToComponent));

            // 查找要替换的组件（第一个）
            std::shared_ptr<ComponentBase> oldComponent;
            std::type_index actualFromTypeIndex = fromTypeIndex;

            // 尝试通过基类查找现有组件
            auto foundTypeIndex = ComponentRegistry::FindExistingComponentByBase(
                fromTypeIndex, entities, singletonComponents);
            if (foundTypeIndex.has_value())
            {
                actualFromTypeIndex = foundTypeIndex.value();
                auto foundIt = entities.find(actualFromTypeIndex);
                if (foundIt != entities.end() && !foundIt->second.empty())
                {
                    oldComponent = foundIt->second[0]; // 获取第一个组件
                }
            }
            else
            {
                // 直接查找
                auto fromIt = entities.find(fromTypeIndex);
                if (fromIt != entities.end() && !fromIt->second.empty())
                {
                    oldComponent = fromIt->second[0]; // 获取第一个组件
                    actualFromTypeIndex = fromTypeIndex;
                }
            }

            // 检查源组件是否存在
            if (!oldComponent)
            {
                std::string fromTypeName = ComponentRegistry::GetTypeName(fromTypeIndex);
                std::cerr << "错误: 源组件 " << fromTypeName << " 不存在，无法进行替换" << std::endl;
                throw std::runtime_error("组件替换失败: 源组件不存在");
            }

            // 获取旧组件的信息
            bool oldIsSingleton = oldComponent->IsSingleton();
            auto oldSingletonBase = oldComponent->GetSingletonBaseType();

            // 检查新组件类型
            bool newIsSingleton = is_singleton_component<ToComponent>::value;
            std::optional<std::type_index> newSingletonBase;

            if (newIsSingleton)
            {
                auto tempNewComponent = std::make_shared<ToComponent>();
                newSingletonBase = tempNewComponent->GetSingletonBaseType();

                // 对于单例组件，验证单例基类是否匹配
                if (oldIsSingleton && oldSingletonBase.has_value() && newSingletonBase.has_value())
                {
                    if (oldSingletonBase.value() != newSingletonBase.value())
                    {
                        std::string fromTypeName = oldComponent->GetTypeName();
                        std::string toTypeName = Object::GetTypeName<ToComponent>();
                        std::cerr << "错误: 无法替换单例组件，" << fromTypeName << " 和 " << toTypeName
                            << " 不具有相同的单例基类" << std::endl;
                        throw std::runtime_error("组件替换失败: 单例基类不匹配");
                    }
                }
            }

            // 先添加新组件的依赖
            AddComponentDependencies<ToComponent>();

            // 创建新组件
            auto newComponent = std::make_shared<ToComponent>(std::forward<Args>(args)...);
            newComponent->SetOwner(ownerEntity);

            // 移除旧组件从各个容器
            RemoveComponentFromContainers(oldComponent, actualFromTypeIndex);

            // 添加新组件到容器
            entities[toTypeIndex].push_back(newComponent);
            allComponents.push_back(newComponent);

            // 更新单例组件映射（如果新组件是单例组件）
            if (newIsSingleton && newSingletonBase.has_value())
            {
                singletonComponents[newSingletonBase.value()] = newComponent;
            }

            std::string fromTypeName = oldComponent->GetTypeName();
            std::string toTypeName = Object::GetTypeName<ToComponent>();
            std::cout << "成功替换组件: " << fromTypeName << " -> " << toTypeName << std::endl;

            return *newComponent;
        }

        // 通用组件替换功能（使用已有实例）
        template <typename FromComponent, typename ToComponent>
        ToComponent& SwapComponent(std::shared_ptr<ToComponent> newComponentInstance)
        {
            static_assert(std::is_base_of_v<ComponentBase, FromComponent>, "FromComponent必须继承自ComponentBase");
            static_assert(std::is_base_of_v<ComponentBase, ToComponent>, "ToComponent必须继承自ComponentBase");

            if (!newComponentInstance)
            {
                throw std::runtime_error("组件替换失败: 新组件实例为空");
            }

            // 检查两个组件类型是否兼容
            if (!ComponentRegistry::AreCompatibleForSwap<FromComponent, ToComponent>())
            {
                std::string fromTypeName = Object::GetTypeName<FromComponent>();
                std::string toTypeName = Object::GetTypeName<ToComponent>();
                std::cerr << "错误: 无法替换组件，" << fromTypeName << " 和 " << toTypeName
                    << " 类型不兼容" << std::endl;
                throw std::runtime_error("组件替换失败: 类型不兼容");
            }

            auto fromTypeIndex = std::type_index(typeid(FromComponent));
            auto toTypeIndex = std::type_index(typeid(ToComponent));

            // 查找要替换的组件（第一个）
            std::shared_ptr<ComponentBase> oldComponent;
            std::type_index actualFromTypeIndex = fromTypeIndex;

            // 尝试通过基类查找现有组件
            auto foundTypeIndex = ComponentRegistry::FindExistingComponentByBase(
                fromTypeIndex, entities, singletonComponents);
            if (foundTypeIndex.has_value())
            {
                actualFromTypeIndex = foundTypeIndex.value();
                auto foundIt = entities.find(actualFromTypeIndex);
                if (foundIt != entities.end() && !foundIt->second.empty())
                {
                    oldComponent = foundIt->second[0]; // 获取第一个组件
                }
            }
            else
            {
                // 直接查找
                auto fromIt = entities.find(fromTypeIndex);
                if (fromIt != entities.end() && !fromIt->second.empty())
                {
                    oldComponent = fromIt->second[0]; // 获取第一个组件
                    actualFromTypeIndex = fromTypeIndex;
                }
            }

            // 检查源组件是否存在
            if (!oldComponent)
            {
                std::string fromTypeName = ComponentRegistry::GetTypeName(fromTypeIndex);
                std::cerr << "错误: 源组件 " << fromTypeName << " 不存在，无法进行替换" << std::endl;
                throw std::runtime_error("组件替换失败: 源组件不存在");
            }

            // 获取旧组件的信息
            bool oldIsSingleton = oldComponent->IsSingleton();
            auto oldSingletonBase = oldComponent->GetSingletonBaseType();

            // 检查新组件类型
            bool newIsSingleton = newComponentInstance->IsSingleton();
            auto newSingletonBase = newComponentInstance->GetSingletonBaseType();

            // 对于单例组件，验证单例基类是否匹配
            if (oldIsSingleton && newIsSingleton &&
                oldSingletonBase.has_value() && newSingletonBase.has_value())
            {
                if (oldSingletonBase.value() != newSingletonBase.value())
                {
                    std::string fromTypeName = oldComponent->GetTypeName();
                    std::string toTypeName = newComponentInstance->GetTypeName();
                    std::cerr << "错误: 无法替换单例组件，" << fromTypeName << " 和 " << toTypeName
                        << " 不具有相同的单例基类" << std::endl;
                    throw std::runtime_error("组件替换失败: 单例基类不匹配");
                }
            }

            // 设置新组件的拥有者
            newComponentInstance->SetOwner(ownerEntity);

            // 移除旧组件从各个容器
            RemoveComponentFromContainers(oldComponent, actualFromTypeIndex);

            // 添加新组件到容器
            entities[toTypeIndex].push_back(newComponentInstance);
            allComponents.push_back(newComponentInstance);

            // 更新单例组件映射
            if (newIsSingleton && newSingletonBase.has_value())
            {
                singletonComponents[newSingletonBase.value()] = newComponentInstance;
            }

            std::string fromTypeName = oldComponent->GetTypeName();
            std::string toTypeName = newComponentInstance->GetTypeName();
            std::cout << "成功替换组件: " << fromTypeName << " -> " << toTypeName << std::endl;

            return *newComponentInstance;
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

        // 精确类型获取 - 返回第一个满足条件的组件
        template <typename T>
        std::optional<std::reference_wrapper<T>> Get()
        {
            const auto& typeIndex = std::type_index(typeid(T));
            auto it = entities.find(typeIndex);
            if (it == entities.end() || it->second.empty())
            {
                return std::nullopt;
            }
            auto ptr = std::static_pointer_cast<T>(it->second[0]); // 返回第一个组件
            if (!ptr)
            {
                return std::nullopt;
            }
            return *ptr;
        }

        // 动态类型获取 - 返回第一个满足条件的组件
        template <typename T>
        std::optional<std::reference_wrapper<T>> GetDynamic()
        {
            static_assert(std::is_base_of_v<ComponentBase, T>, "类型必须继承自ComponentBase基类");

            // 首先尝试精确匹配
            auto exact = Get<T>();
            if (exact.has_value())
            {
                return exact;
            }

            // 遍历所有组件查找继承关系，返回第一个找到的
            for (auto& component : allComponents)
            {
                if (auto derived = dynamic_cast<T*>(component.get()))
                {
                    return *derived;
                }
            }

            return std::nullopt;
        }

        // 获取所有指定类型的组件
        template <typename T>
        std::vector<std::reference_wrapper<T>> GetAll()
        {
            static_assert(std::is_base_of_v<ComponentBase, T>, "类型必须继承自ComponentBase基类");

            std::vector<std::reference_wrapper<T>> results;

            // 首先从精确类型匹配的组件中收集
            const auto& typeIndex = std::type_index(typeid(T));
            auto it = entities.find(typeIndex);
            if (it != entities.end())
            {
                for (auto& component : it->second)
                {
                    if (auto derived = std::static_pointer_cast<T>(component))
                    {
                        results.emplace_back(*derived);
                    }
                }
            }

            // 然后从所有组件中通过动态类型转换收集继承关系的组件
            for (auto& component : allComponents)
            {
                if (auto derived = dynamic_cast<T*>(component.get()))
                {
                    // 避免重复添加（已经在精确匹配中添加过的）
                    if (component->GetTypeIndex() != typeIndex)
                    {
                        results.emplace_back(*derived);
                    }
                }
            }

            return results;
        }

        template <typename T>
        bool Contains()
        {
            auto it = entities.find(std::type_index(typeid(T)));
            return it != entities.end() && !it->second.empty();
        }

        // 检查是否包含指定类型
        template <typename T>
        bool ContainsDynamic()
        {
            static_assert(std::is_base_of_v<ComponentBase, T>, "类型必须继承自ComponentBase基类");

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

        // 移除第一个指定类型的组件（与Get方法保持相同逻辑）
        template <typename T>
        void Remove()
        {
            auto typeIndex = std::type_index(typeid(T));
            auto it = entities.find(typeIndex);
            if (it == entities.end() || it->second.empty())
            {
                return; // 组件不存在
            }

            auto componentToRemove = it->second[0]; // 移除第一个组件

            // 检查删除后是否会违反必需组件约束
            if (!CanRemoveComponent(componentToRemove))
            {
                std::cerr << "警告: 无法删除组件 " << componentToRemove->GetTypeName()
                    << "，因为它是必需组件的最后一个实现" << std::endl;
                return;
            }

            // 执行删除
            RemoveComponentFromContainers(componentToRemove, typeIndex);
        }

        // 移除指定的组件实例
        template <typename T>
        bool RemoveComponent(const T& componentToRemove)
        {
            static_assert(std::is_base_of_v<ComponentBase, T>, "类型必须继承自ComponentBase基类");

            auto typeIndex = std::type_index(typeid(T));
            auto it = entities.find(typeIndex);
            if (it == entities.end())
            {
                return false; // 类型不存在
            }

            // 查找具体的组件实例
            auto& componentList = it->second;
            for (auto iter = componentList.begin(); iter != componentList.end(); ++iter)
            {
                if (iter->get() == &componentToRemove)
                {
                    auto component = *iter;

                    // 检查删除后是否会违反必需组件约束
                    if (!CanRemoveComponent(component))
                    {
                        std::cerr << "警告: 无法删除组件 " << component->GetTypeName()
                            << "，因为它是必需组件的最后一个实现" << std::endl;
                        return false;
                    }

                    // 执行删除
                    RemoveComponentFromContainers(component, typeIndex);

                    std::cout << "成功删除指定组件实例: " << component->GetTypeName() << std::endl;
                    return true;
                }
            }

            return false; // 没有找到指定的组件实例
        }

        // 移除指定类型的所有组件
        template <typename T>
        void RemoveAll()
        {
            static_assert(std::is_base_of_v<ComponentBase, T>, "类型必须继承自ComponentBase基类");

            // 收集要移除的组件
            std::vector<std::shared_ptr<ComponentBase>> toRemove;

            for (auto& component : allComponents)
            {
                if (dynamic_cast<T*>(component.get()) != nullptr)
                {
                    toRemove.push_back(component);
                }
            }

            // 检查删除后是否会违反必需组件约束
            for (auto& component : toRemove)
            {
                if (!CanRemoveComponent(component))
                {
                    std::cerr << "警告: 无法删除所有 " << Object::GetTypeName<T>()
                        << " 组件，因为至少需要保留一个必需组件的实现" << std::endl;
                    return;
                }
            }

            // 执行删除
            for (auto& component : toRemove)
            {
                // 查找对应的类型索引
                std::type_index componentTypeIndex = component->GetTypeIndex();
                RemoveComponentFromContainers(component, componentTypeIndex);
            }

            std::cout << "成功删除所有 " << Object::GetTypeName<T>() << " 类型的组件" << std::endl;
        }

        // 验证所有必需组件是否存在
        void ValidateRequiredComponents()
        {
            for (const auto& requiredType : requiredComponents)
            {
                bool found = false;

                // 检查是否有该必需类型的实现（包括继承关系）
                for (const auto& component : allComponents)
                {
                    if (CheckIfImplementsRequiredType(component, requiredType))
                    {
                        found = true;
                        break;
                    }
                }

                /*if (!found)
                {
                    std::string typeName = ComponentRegistry::GetTypeName(requiredType);
                    std::cerr << "错误: 需要组件 " << typeName << "，但并未找到对应的实现" << std::endl;
                }*/
            }
        }

        // 获取所有组件
        const std::vector<std::shared_ptr<ComponentBase>>& GetAllComponents() const
        {
            return allComponents;
        }

    private:
        // 从各个容器中移除组件的辅助方法
        void RemoveComponentFromContainers(const std::shared_ptr<ComponentBase>& component,
                                           const std::type_index& typeIndex)
        {
            // 从单例组件映射中移除
            bool isSingleton = component->IsSingleton();
            if (isSingleton)
            {
                auto singletonBaseType = component->GetSingletonBaseType();
                if (singletonBaseType.has_value())
                {
                    singletonComponents.erase(singletonBaseType.value());
                }
            }

            // 从所有组件列表中移除
            allComponents.erase(
                std::remove(allComponents.begin(), allComponents.end(), component),
                allComponents.end()
            );

            // 从实体映射中移除
            auto it = entities.find(typeIndex);
            if (it != entities.end())
            {
                auto& componentList = it->second;
                componentList.erase(
                    std::remove(componentList.begin(), componentList.end(), component),
                    componentList.end()
                );

                // 如果列表为空，删除该类型的条目
                if (componentList.empty())
                {
                    entities.erase(it);
                }
            }
        }

        // 检查组件是否实现了必需的类型
        bool CheckIfImplementsRequiredType(const std::shared_ptr<ComponentBase>& component,
                                           const std::type_index& requiredType)
        {
            // 使用注册器的类型检查功能
            return ComponentRegistry::IsInstanceOf(component, requiredType);
        }

        // 检查是否可以删除组件
        bool CanRemoveComponent(const std::shared_ptr<ComponentBase>& componentToRemove)
        {
            // 检查每个必需组件类型
            for (const auto& requiredType : requiredComponents)
            {
                // 如果要删除的组件不是这个必需类型的实现，则跳过
                if (!ComponentRegistry::IsInstanceOf(componentToRemove, requiredType))
                {
                    continue;
                }

                // 统计删除后还剩多少个这个必需类型的实现
                int remainingImplementations = 0;
                for (const auto& component : allComponents)
                {
                    if (component != componentToRemove &&
                        ComponentRegistry::IsInstanceOf(component, requiredType))
                    {
                        remainingImplementations++;
                    }
                }

                // 如果删除后没有实现了，则不能删除
                if (remainingImplementations == 0)
                {
                    return false;
                }
            }

            return true;
        }

        // 通过类型索引添加依赖组件
        void AddDependencyByTypeIndex(const std::type_index& typeIndex)
        {
            // 检查组件是否已存在
            auto it = entities.find(typeIndex);
            if (it != entities.end() && !it->second.empty())
            {
                return; // 组件已存在，无需重复添加
            }

            // 检查组件类型是否已注册
            if (!ComponentRegistry::IsRegistered(typeIndex))
            {
                std::string typeName = ComponentRegistry::GetTypeName(typeIndex);
                std::cerr << "警告: 组件类型 " << typeName << " 未注册，无法自动添加依赖组件" << std::endl;
                return;
            }

            // 创建组件实例
            auto component = ComponentRegistry::CreateComponent(typeIndex, ownerEntity);
            if (!component)
            {
                std::string typeName = ComponentRegistry::GetTypeName(typeIndex);
                std::cerr << "错误: 无法创建组件实例: " << typeName << std::endl;
                return;
            }

            // 检查是否为单例组件
            if (component->IsSingleton())
            {
                auto singletonBaseType = component->GetSingletonBaseType();
                if (singletonBaseType.has_value())
                {
                    auto baseTypeIndex = singletonBaseType.value();

                    // 检查单例组件是否已存在
                    auto singletonIt = singletonComponents.find(baseTypeIndex);
                    if (singletonIt != singletonComponents.end())
                    {
                        return; // 单例组件已存在
                    }
                    singletonComponents.emplace(baseTypeIndex, component);
                }
            }

            // 添加到实体注册表
            entities[typeIndex].push_back(component);
            allComponents.push_back(component);

            // 递归添加此组件的依赖
            auto dependencies = component->GetDependencies();
            for (const auto& depType : dependencies)
            {
                AddDependencyByTypeIndex(depType);
            }

            std::string typeName = ComponentRegistry::GetTypeName(typeIndex);
            std::cout << "自动添加依赖组件: " << typeName << std::endl;
        }
    };

    // 实体类
    class Entity
    {
    private:
        RTTM::Registry registry;

    public:
        Entity() : registry(this)
        {
        }

        virtual ~Entity() = default;


        // 精确类型检查
        template <typename T>
        bool HasComponent() { return registry.Contains<T>(); }

        // 动态类型检查
        template <typename T>
        bool HasComponentDynamic() { return registry.ContainsDynamic<T>(); }

        // 精确类型获取 - 返回第一个满足条件的组件
        template <typename T>
        T& GetComponent()
        {
            auto result = registry.Get<T>();
            if (!result.has_value())
            {
                auto typeName = Object::GetTypeName<T>();
                std::cerr << "错误: 需要组件 " << typeName << "，但并未找到对应的实现" << std::endl;

                throw std::runtime_error("组件不存在: " + typeName);
            }
            return result.value();
        }

        // 动态类型获取 - 返回第一个满足条件的组件
        template <typename T>
        T& GetComponentDynamic()
        {
            auto result = registry.GetDynamic<T>();
            if (!result.has_value())
            {
                auto typeName = Object::GetTypeName<T>();
                std::cerr << "错误: 需要组件 " << typeName << "，但并未找到对应的实现" << std::endl;

                throw std::runtime_error("组件不存在: " + typeName);
            }
            return result.value();
        }

        // 尝试获取组件 - 返回第一个满足条件的组件
        template <typename T>
        std::optional<std::reference_wrapper<T>> TryGetComponent()
        {
            return registry.GetDynamic<T>();
        }

        // 获取所有指定类型的组件 - 返回std::vector<std::reference_wrapper<T>>
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

        // 组件替换功能
        template <typename FromComponent, typename ToComponent, typename... Args>
        ToComponent& SwapComponent(Args&&... args)
        {
            return registry.SwapComponent<FromComponent, ToComponent>(std::forward<Args>(args)...);
        }

        // 组件替换功能（使用已有实例）
        template <typename FromComponent, typename ToComponent>
        ToComponent& SwapComponent(std::shared_ptr<ToComponent> newComponentInstance)
        {
            return registry.SwapComponent<FromComponent, ToComponent>(newComponentInstance);
        }

        // 移除第一个指定类型的组件（与GetComponent保持相同逻辑）
        template <typename T>
        void RemoveComponent()
        {
            registry.Remove<T>();
        }

        // 移除指定的组件实例
        template <typename T>
        bool RemoveComponent(const T& component)
        {
            return registry.RemoveComponent<T>(component);
        }

        // 移除所有指定类型的组件（包括继承的子类）
        template <typename T>
        void RemoveComponents()
        {
            registry.RemoveAll<T>();
        }

        // 验证必需组件
        void ValidateRequiredComponents()
        {
            registry.ValidateRequiredComponents();
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

    protected:
        // 添加必需组件类型（供派生类使用）
        template <typename T>
        void AddRequiredComponentType()
        {
            registry.AddRequiredComponent<T>();
        }
    };

    // 实现ComponentBase中的模板方法
    template <typename T>
    T& ComponentBase::GetComponent()
    {
        if (!ownerEntity)
        {
            throw std::runtime_error("组件没有绑定到实体");
        }
        return ownerEntity->GetComponent<T>();
    }

    template <typename T>
    std::optional<std::reference_wrapper<T>> ComponentBase::TryGetComponent()
    {
        if (!ownerEntity)
        {
            return std::nullopt;
        }
        return ownerEntity->TryGetComponent<T>();
    }

    template <typename T>
    bool ComponentBase::HasComponent()
    {
        if (!ownerEntity)
        {
            return false;
        }
        return ownerEntity->HasComponent<T>();
    }

    // 特化的EntityWithComponents类，用于自动添加组件
    template <typename... ComponentTypes>
    class EntityWithComponents : public Entity
    {
    public:
        EntityWithComponents()
        {
            // 添加必需组件类型到注册表
            (AddRequiredComponentType<ComponentTypes>(), ...);

            // 对于非抽象类，自动添加组件
            // 对于抽象类，只添加到必需组件列表，不实例化
            AddComponentsIfNotAbstract<ComponentTypes...>();

            // 验证必需组件（在构造完成后检查）
            ValidateRequiredComponents();
        }

    private:
        template <typename T>
        void AddComponentIfNotAbstract()
        {
            if constexpr (!std::is_abstract_v<T>)
            {
                GetOrAddComponent<T>();
            }
            // 对于抽象类，不执行任何操作，只是添加到必需列表
        }

        template <typename... Types>
        void AddComponentsIfNotAbstract()
        {
            (AddComponentIfNotAbstract<Types>(), ...);
        }
    };
}

#define REQUIRE_COMPONENTS(...)  public RTTM::EntityWithComponents<__VA_ARGS__>

#define COMPONENT_DEPENDENCIES(...) \
std::vector<std::type_index> GetDependencies() const override { \
return __getDependencies<__VA_ARGS__>(); \
}

#endif // RTTMENTITY_H