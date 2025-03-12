#ifndef RTTM_H
#define RTTM_H
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <typeindex>
#include <set>
#include <unordered_set>

#include "Function.hpp"

namespace RTTM
{
    enum class RTTMTypeBits
    {
        Class,
        Enum,
        Variable,
    };

    template <typename... Types>
    static std::type_index GetTypeIndex()
    {
        return std::type_index(typeid(std::tuple<Types...>));
    }

    // 检测是否是 STL 容器
    template <typename T>
    struct is_stl_container : std::false_type
    {
    };

    template <typename T, typename A>
    struct is_stl_container<std::vector<T, A>> : std::true_type
    {
    };

    template <typename T, typename A>
    struct is_stl_container<std::list<T, A>> : std::true_type
    {
    };

    template <typename K, typename V, typename C, typename A>
    struct is_stl_container<std::map<K, V, C, A>> : std::true_type
    {
    };

    template <typename K, typename C, typename A>
    struct is_stl_container<std::unordered_set<K, C, A>> : std::true_type
    {
    };

    template <typename K, typename C, typename A>
    struct is_stl_container<std::set<K, C, A>> : std::true_type
    {
    };

    template <typename K, typename V, typename C, typename A>
    struct is_stl_container<std::unordered_map<K, V, C, A>> : std::true_type
    {
    };

    template <typename T>
    struct is_stl_container<std::queue<T>> : std::true_type
    {
    };

    template <typename T>
    struct is_stl_container<std::priority_queue<T>> : std::true_type
    {
    };

    template <typename T>
    constexpr bool is_stl_container_v = is_stl_container<T>::value;

    // 检测是否是 std::string
    template <typename T>
    constexpr bool is_string_v = std::is_same_v<T, std::string>;

    class IFactory
    {
    protected:
        Ref<IFunctionWrapper> createFunc;

    public:
        //返回void*智能指针,待使用时转换
        template <typename... Args>
        Ref<char> Create(Args... args)
        {
            return (*std::dynamic_pointer_cast<FunctionWrapper<Ref<char>, Args...>>(createFunc))(
                std::forward<Args>(args)...);
        }
    };

    template <typename T, typename... Args>
    class Factory : public IFactory
    {
        using Allocator = std::allocator<T>;
        using Traits = std::allocator_traits<Allocator>;
        Ref<Allocator> allocator;

    public:
        Factory()
        {
            if constexpr (!std::is_class_v<T>)
            {
                throw std::runtime_error("RTTM: T must be a structure or class");
            }
            allocator = CreateRef<Allocator>();
            createFunc = std::make_shared<FunctionWrapper<Ref<char>, Args...>>([this](Args... args)
            {
                return Create(std::forward<Args>(args)...);
            });
        }

        static Ref<IFactory> CreateFactory()
        {
            return CreateRef<Factory<T, Args...>>();
        }

        Ref<char> Create(Args... args)
        {
            T* rawPtr = Traits::allocate(*allocator, 1);
            if (rawPtr == nullptr)
            {
                throw std::runtime_error("RTTM: Cannot allocate memory for object");
            }
            try
            {
                Traits::construct(*allocator, rawPtr, std::forward<Args>(args)...);
            }
            catch (std::exception& e)
            {
                Traits::deallocate(*allocator, rawPtr, 1);
                std::cerr << "RTTM: " << e.what() << std::endl;
                throw;
            }

            // 确保使用正确的删除器
            return Ref<char>(reinterpret_cast<char*>(rawPtr),
                             [allocCopy = this->allocator](char* ptr)
                             {
                                 T* typedPtr = reinterpret_cast<T*>(ptr);
                                 Traits::destroy(*allocCopy, typedPtr);
                                 Traits::deallocate(*allocCopy, typedPtr, 1);
                             }
            );
        }
    };

    namespace detail
    {
        using Member = std::pair<size_t, RTTMTypeBits>;
        using ValueMap = std::unordered_map<std::string, Ref<void>>; //变量
        using FunctionMap = std::unordered_map<std::string, Ref<IFunctionWrapper>>; //函数
        using MemberMap = std::unordered_map<std::string, Member>; //类的成员的偏移量
        using EnumMap = std::unordered_map<std::string, int>; //枚举的值
        struct TypeBaseInfo
        {
            RTTMTypeBits type;
            size_t size;
            FunctionMap functions;
            MemberMap members;
            std::unordered_map<std::string, Ref<IFactory>> factories; // 构造函数,如果是枚举则为空
            std::unordered_map<std::string, std::string> membersType; //成员的类型
            Ref<FunctionWrapper<void, void*>> destructor;
            Ref<FunctionWrapper<void, void*, void*>> copier;

            void AppendFunction(const std::string& name, const Ref<IFunctionWrapper>& func)
            {
                functions[name] = func;
            }

            void AppendMember(const std::string& name, const Member& member)
            {
                members[name] = member;
            }

            void AppendMemberType(const std::string& name, const std::string& type)
            {
                membersType[name] = type;
            }

            void AppendFactory(const std::string& name, const Ref<IFactory>& factory)
            {
                factories[name] = factory;
            }

            void AppendFunctions(const FunctionMap& funcs)
            {
                functions.insert(funcs.begin(), funcs.end());
            }

            void AppendMembers(const MemberMap& mems)
            {
                for (const auto& [name, member] : mems)
                {
                    members[name] = member;
                }
            }

            void AppendMembersType(const std::unordered_map<std::string, std::string>& mems)
            {
                for (const auto& [name, type] : mems)
                {
                    membersType[name] = type;
                }
            }

            void AppendFactories(const std::unordered_map<std::string, Ref<IFactory>>& facs)
            {
                for (const auto& [name, factory] : facs)
                {
                    if (name == "default")
                        continue; //默认构造函数不需要添加
                    factories[name] = factory;
                }
            }

            //所有工厂只在注册类时创建,不能在运行时创建
            TypeBaseInfo& operator=(const TypeBaseInfo& other)
            {
                type = other.type;
                functions = other.functions;
                members = other.members;
                factories = other.factories;
                return *this;
            }
        };

        using TypeInfoMap = std::unordered_map<std::string, TypeBaseInfo>; //类的信息

        inline static TypeInfoMap TypesInfo; //预注册的类的信息
        inline static ValueMap Variables; //全局变量
        inline static FunctionMap GFunctions; //全局函数
        inline static std::unordered_map<std::string, EnumMap> Enums; //枚举

        template <typename T>
        struct function_traits;

        // 普通函数
        template <typename Ret, typename... Args>
        struct function_traits<Ret(Args...)>
        {
            using return_type = Ret;
            using arguments = std::tuple<Args...>;
            static constexpr std::size_t arity = sizeof...(Args);

            template <std::size_t N>
            using arg_type = std::tuple_element_t<N, arguments>;
        };

        // 成员函数
        template <typename Ret, typename Class, typename... Args>
        struct function_traits<Ret(Class::*)(Args...)>
        {
            using return_type = Ret;
            using arguments = std::tuple<Args...>;
            static constexpr std::size_t arity = sizeof...(Args);

            template <std::size_t N>
            using arg_type = std::tuple_element_t<N, arguments>;
        };

        // const成员函数
        template <typename Ret, typename Class, typename... Args>
        struct function_traits<Ret(Class::*)(Args...) const>
        {
            using return_type = Ret;
            using arguments = std::tuple<Args...>;
            static constexpr std::size_t arity = sizeof...(Args);

            template <std::size_t N>
            using arg_type = std::tuple_element_t<N, arguments>;
        };
    }

    //TODO 重构RTTM使其不再依附于Serializable,提高性能
    //注册类或者结构体或者枚举
    template <typename T>
    class Registry_
    {
        RTTMTypeBits type;
        const std::string typeName;
        const std::type_index typeIndex;

        void destructor() const
        {
            detail::TypesInfo[typeName].destructor = CreateRef<FunctionWrapper<void, void*>>([this](void* obj)
            {
                if constexpr (!std::is_class_v<T>)
                {
                    std::cerr << "RTTM: T must be a structure or class" << std::endl;
                    return;
                }
                T* _obj = static_cast<T*>(obj);
                _obj->~T();
            });
        }

        void copier() const
        {
            detail::TypesInfo[typeName].copier = CreateRef<FunctionWrapper<void, void*, void*>>(
                [this](void* src, void* dest)
                {
                    if constexpr (!std::is_class_v<T>)
                    {
                        std::cerr << "RTTM: T must be a structure or class" << std::endl;
                        return;
                    }
                    std::swap(*static_cast<T*>(src), *static_cast<T*>(dest));
                });
        }

    public:
        Registry_(): typeIndex(std::type_index(typeid(T))), type(RTTMTypeBits::Variable),
                     typeName(Object::GetTypeName<T>())
        {
            if (detail::TypesInfo.find(typeName) != detail::TypesInfo.end())
            {
                std::cerr << "RTTM: The structure has been registered: " << typeName << std::endl;
                return;
            }
            if constexpr (std::is_class_v<T>)
            {
                type = RTTMTypeBits::Class; //在C++中一般情况下是struct和class是一致的
            }
            else if constexpr (std::is_enum_v<T>)
            {
                type = RTTMTypeBits::Enum;
            }
            else
            {
                throw std::runtime_error("RTTM: T must be a structure or class or enum");
            }
            detail::TypesInfo[typeName].type = type;
            detail::TypesInfo[typeName].size = sizeof(T);
            destructor(); //注册析构函数
            constructor(); //注册默认构造函数
            copier(); //注册拷贝构造函数
        }

        template <typename U>
        Registry_& base()
        {
            auto base = Object::GetTypeName<U>();
            if (detail::TypesInfo.find(base) == detail::TypesInfo.end())
            {
                std::cerr << "RTTM: The base structure has not been registered: " << base
                    << std::endl;
                return *this;
            }
            auto baseType = detail::TypesInfo[base];
            detail::TypesInfo[typeName].AppendFunctions(baseType.functions);
            detail::TypesInfo[typeName].AppendMembers(baseType.members);
            detail::TypesInfo[typeName].AppendMembersType(baseType.membersType);
            detail::TypesInfo[typeName].AppendFactories(baseType.factories);
            return *this;
        }

        template <typename... Args>
        Registry_& constructor()
        {
            detail::TypesInfo[typeName].factories[Object::GetTypeName<Args...>()] =
                Factory<T, Args...>::CreateFactory();
            return *this;
        }


        template <typename U>
        Registry_& property(const char* name, U T::* value)
        {
            size_t offset = reinterpret_cast<size_t>(&(reinterpret_cast<T*>(0)->*value));

            if constexpr (std::is_class_v<U> && !is_stl_container_v<U> && !is_string_v<U>)
            {
                detail::TypesInfo[typeName].members[name] = {offset, RTTMTypeBits::Class};
            }
            else if constexpr (std::is_enum_v<U>)
            {
                detail::TypesInfo[typeName].members[name] = {offset, RTTMTypeBits::Enum};
            }
            else
            {
                detail::TypesInfo[typeName].members[name] = {offset, RTTMTypeBits::Variable};
            }
            detail::TypesInfo[typeName].membersType[name] = Object::GetTypeName<U>();
            return *this;
        }

        template <typename R, typename... Args>
        Registry_& method(const std::string& name, R (T::*memFunc)(Args...))
        {
            if constexpr (!std::is_class_v<T>)
            {
                std::cerr << "RTTM: The structure has not been registered: " << typeName << std::endl;
                return *this;
            }
            auto func = CreateRef<FunctionWrapper<R, void*, Args...>>([this, memFunc](void* obj, Args... args)
            {
                T* _obj = static_cast<T*>(obj);
                return (_obj->*memFunc)(std::forward<Args>(args)...);
            });
            detail::TypesInfo[typeName].functions[name + Object::GetTypeName<Args...>()] = func;
            return *this;
        }

        template <typename R, typename... Args>
        Registry_& method(const std::string& name, R (T::*memFunc)(Args...) const)
        {
            if constexpr (!std::is_class_v<T>)
            {
                std::cerr << "RTTM: The structure has not been registered: " << typeName << std::endl;
                return *this;
            }
            auto func = CreateRef<FunctionWrapper<R, void*, Args...>>([this, memFunc](void* obj, Args... args)
            {
                T* _obj = static_cast<T*>(obj);
                return (_obj->*memFunc)(std::forward<Args>(args)...);
            });
            detail::TypesInfo[typeName].functions[name + Object::GetTypeName<Args...>()] = func;
            return *this;
        }
    };

    template <typename T>
    class Enum_
    {
    public:
        Enum_()
        {
            if (detail::Enums.find(Object::GetTypeName<T>()) != detail::Enums.end())
            {
                detail::Enums.insert({Object::GetTypeName<T>(), {}});
            }
        }

        Enum_& value(const std::string& name, T value)
        {
            if constexpr (!std::is_enum_v<T>)
            {
                std::cerr << "RTTM: T must be an enum" << std::endl;
                return *this;
            }
            detail::Enums[Object::GetTypeName<T>()][name] = static_cast<int>(value);
            return *this;
        }

        T GetValue(const std::string& name)
        {
            if constexpr (!std::is_enum_v<T>)
            {
                std::cerr << "RTTM: T must be an enum" << std::endl;
                return T();
            }
            return static_cast<T>(detail::Enums[Object::GetTypeName<T>()][name]);
        }
    };

    class Enum
    {
    public:
        template <typename T>
        static Enum_<T> Get()
        {
            return Enum_<T>();
        }
    };

    template <typename T>
    class Method
    {
        Ref<IFunctionWrapper> func;
        std::string name;
        void* instance;
        bool isMemberFunction;

    public:
        Method(const Ref<IFunctionWrapper>& func, const std::string& name, void* instance = nullptr,
               bool isMemberFunction = false)
            : func(func), name(name), instance(instance), isMemberFunction(isMemberFunction)
        {
        }

        bool IsValid() const
        {
            return func != nullptr;
        }

        template <typename... Args>
        T Invoke(Args... args) const
        {
            if (isMemberFunction)
            {
                if (!instance)
                    throw std::runtime_error("Instance is null for member function: " + name);
                auto wrapper = std::dynamic_pointer_cast<FunctionWrapper<T, void*, Args...>>(func);
                if (!wrapper)
                    throw std::runtime_error(
                        "Function signature mismatch for: " + name + " Arguments are: " + Object::GetTypeName<Args
                            ...>());
                return (*wrapper)(instance, std::forward<Args>(args)...);
            }
            else
            {
                auto wrapper = std::dynamic_pointer_cast<FunctionWrapper<T, Args...>>(func);
                if (!wrapper)
                    throw std::runtime_error(
                        "Function signature mismatch for: " + name + "Arguments are: " + Object::GetTypeName<Args
                            ...>());
                return (*wrapper)(std::forward<Args>(args)...);
            }
        }

        template <typename... Args>
        T operator()(Args... args) const
        {
            return Invoke(std::forward<Args>(args)...);
        }

        const std::string& GetName() const
        {
            return name;
        }

        Method& operator=(const Method& other) const
        {
            func = other.func;
            return *this;
        }
    };

    class RType
    {
    private:
        // 检查对象有效性的辅助函数
        template <typename R = bool>
        R checkValid() const
        {
            if (!valid)
            {
                throw std::runtime_error("RTTM: The structure has not been registered");
            }
            if (!created)
            {
                throw std::runtime_error("RTTM: The object has not been created");
            }
            return R(true);
        }

        // 深拷贝实现，优化错误处理
        void _copyFrom(const Ref<char>& newInst) const
        {
            if (!checkValid()) return;

            // 调用复制构造函数
            if (detail::TypesInfo[type].copier)
            {
                detail::TypesInfo[type].copier->operator()(instance.get(), newInst.get());
            }
            else
            {
                throw std::runtime_error("RTTM: The copy constructor has not been registered");
            }
        }

        // 优化了模板参数的展开方式
        template <typename FuncType>
        auto GetMethodImpl(const std::string& name) const
        {
            using traits = detail::function_traits<FuncType>;
            using ReturnType = typename traits::return_type;

            return std::apply([&name, this](auto... args)
            {
                return GetMethodOrig<ReturnType, decltype(args)...>(name);
            }, typename traits::arguments{});
        }

        // 获取方法的原始实现，改进了错误处理和返回值
        template <typename T, typename... Args>
        Method<T> GetMethodOrig(const std::string& name) const
        {
            auto tn = name + Object::GetTypeName<Args...>();

            if (!checkValid())
            {
                return Method<T>(nullptr, name, nullptr, false);
            }

            auto& functions = detail::TypesInfo[type].functions;
            auto it = functions.find(tn);
            if (it == functions.end())
            {
                throw std::runtime_error("Function not found: " + name);
            }

            auto _f = std::dynamic_pointer_cast<FunctionWrapper<T, void*, Args...>>(it->second);
            if (!_f)
            {
                throw std::runtime_error(
                    "Function signature mismatch for: " + name + " Arguments are: " + Object::GetTypeName<Args...>());
            }

            return Method<T>(_f, name, static_cast<void*>(instance.get()), true);
        }

        void preloadAllProperties() const
        {
            if (!checkValid() || !membersCache.empty()) return; // 已经加载过则跳过

            auto& typeInfo = detail::TypesInfo[type];
            for (const auto& [name, memberInfo] : typeInfo.members)
            {
                auto typeIt = typeInfo.membersType.find(name);
                if (typeIt == typeInfo.membersType.end()) continue;

                auto newStruct = CreateRef<RType>(typeIt->second, memberInfo.second);
                newStruct->instance = AliasCreate(instance, instance.get() + memberInfo.first);
                newStruct->valid = true;
                newStruct->created = true;
                membersCache[name] = newStruct;
            }
        }

    protected:
        std::string type;
        RTTMTypeBits typeEnum;
        mutable Ref<char> instance; // 标记为mutable，允许在const方法中修改
        mutable bool valid = false;
        mutable bool created = false;

        // 使用unordered_map替代vector存储名称，提高查找效率
        // 标记为mutable，允许在const方法中修改缓存
        mutable std::unordered_set<std::string> membersName;
        mutable std::unordered_set<std::string> funcsName;
        mutable std::unordered_map<std::string, Ref<RType>> membersCache;

    public:
        // 默认构造函数
        RType() = default;
        RType(std::string _type);

        // 带参数的构造函数，优化了初始化流程
        RType(const std::string& _type, RTTMTypeBits _typeEnum,
              const std::unordered_set<std::string>& _membersName = {},
              const std::unordered_set<std::string>& _funcsName = {}, const Ref<char>& _instance = nullptr):
            type(_type),
            typeEnum(_typeEnum),
            valid(!(type.empty())),
            created(_typeEnum == RTTMTypeBits::Variable),
            membersName(_membersName),
            funcsName(_funcsName)
        {
            if (!valid && !type.empty())
            {
                if (detail::TypesInfo.find(type) == detail::TypesInfo.end())
                {
                    throw std::runtime_error("RTTM: The structure has not been registered: " + type);
                    valid = false;
                    return;
                }
            }

            if (valid)
            {
                if (membersName.empty())
                    for (const auto& [name, _] : detail::TypesInfo[type].members)
                    {
                        membersName.insert(name);
                    }

                if (funcsName.empty())
                    for (const auto& [name, _] : detail::TypesInfo[type].functions)
                    {
                        funcsName.insert(name);
                    }
            }
        }

        // 对象创建方法，修改为const
        template <typename... Args>
        bool Create(Args... args) const
        {
            if (!valid)
            {
                throw std::runtime_error("RTTM: The structure has not been registered");
            }

            auto _t = Object::GetTypeName<Args...>();
            auto& factories = detail::TypesInfo[type].factories;
            auto factoryIt = factories.find(_t);

            if (factoryIt == factories.end())
            {
                throw std::runtime_error("RTTM: The factory has not been registered: " + _t);
            }

            auto newInst = factoryIt->second->Create(std::forward<Args>(args)...);
            if (!newInst)
            {
                throw std::runtime_error("RTTM: The factory has not been registered: " + _t);
            }

            if (instance)
            {
                _copyFrom(newInst);
            }
            else
            {
                instance = newInst;
            }

            // 更新缓存和成员信息
            //refreshMemberInfo();
            created = true;
            return true;
        }

        // 刷新成员信息的辅助方法，修改为const
        void refreshMemberInfo() const
        {
            membersCache.clear();
            membersName.clear();
            funcsName.clear();

            if (valid && !type.empty())
            {
                auto& typeInfo = detail::TypesInfo[type];
                for (const auto& [name, _] : typeInfo.members)
                {
                    membersName.insert(name);
                }

                for (const auto& [name, _] : typeInfo.functions)
                {
                    funcsName.insert(name);
                }
            }
        }

        // 附加现有实例的方法，修改为const
        template <typename T>
        void Attach(T& inst) const
        {
            instance = AliasCreate(instance, static_cast<char*>(reinterpret_cast<void*>(&inst)));
            membersCache.clear();
            created = true;
            valid = true;
        }

        // 调用方法的实现，已是const
        template <typename R, typename... Args>
        R Invoke(const std::string& name, Args... args) const
        {
            auto tn = name + Object::GetTypeName<Args...>();

            if (!checkValid<R>())
            {
                return R();
            }

            auto& functions = detail::TypesInfo[type].functions;
            auto it = functions.find(tn);
            if (it == functions.end())
            {
                throw std::runtime_error("RTTM: Function not found: " + tn);
            }

            auto funcPtr = std::dynamic_pointer_cast<FunctionWrapper<R, void*, Args...>>(it->second);
            if (!funcPtr)
            {
                throw std::runtime_error("RTTM: Function not found or invalid cast: " + tn);
            }

            return funcPtr->operator()(static_cast<void*>(instance.get()), std::forward<Args>(args)...);
        }

        // 使用函数类型获取方法，修改为const
        template <typename FuncType>
        auto GetMethod(const std::string& name) const
        {
            return GetMethodImpl<FuncType>(name);
        }

        // 获取属性的泛型方法，修改为const
        template <typename T>
        T& GetProperty(const std::string& name) const
        {
            if (!checkValid())
            {
                static T dummy{}; // 安全返回一个静态对象而非空指针
                return dummy;
            }

            auto& members = detail::TypesInfo[type].members;
            auto it = members.find(name);
            if (it == members.end())
            {
                static T dummy{};
                throw std::runtime_error("RTTM: Member not found: " + name);
                return dummy;
            }

            return *reinterpret_cast<T*>(instance.get() + it->second.first);
        }

        // 获取复杂属性的方法，修改为const
        Ref<RType> GetProperty(const std::string& name) const
        {
            if (!checkValid())
            {
                throw std::runtime_error("RTTM: Invalid object access: " + type);
            }

            auto& members = detail::TypesInfo[type].members;
            auto it = members.find(name);
            if (it == members.end())
            {
                throw std::runtime_error("RTTM: Member not found: " + name);
            }

            // 优先使用缓存
            auto cacheIt = membersCache.find(name);
            if (cacheIt != membersCache.end())
            {
                return membersCache[name];
            }

            auto& typeInfo = detail::TypesInfo[type];
            auto typeIt = typeInfo.membersType.find(name);
            if (typeIt == typeInfo.membersType.end())
            {
                throw std::runtime_error("RTTM: Member type not found: " + name);
            }

            auto newStruct = CreateRef<RType>(typeIt->second, it->second.second);
            newStruct->instance = AliasCreate(instance, instance.get() + it->second.first);
            newStruct->valid = true;
            newStruct->created = true;

            membersCache[name] = newStruct;
            return newStruct;
        }

        std::unordered_map<std::string, Ref<RType>> GetProperties() const
        {
            if (!checkValid())
            {
                return {};
            }
            if (membersCache.empty())
            {
                for (const auto& [name, _] : detail::TypesInfo[type].members)
                {
                    auto newStruct = CreateRef<RType>(detail::TypesInfo[type].membersType[name],
                                                      detail::TypesInfo[type].members[name].second);
                    newStruct->instance = AliasCreate(
                        instance, instance.get() + detail::TypesInfo[type].members[name].first);
                    newStruct->valid = true;
                    newStruct->created = true;
                    membersCache[name] = newStruct;
                }
            }
            return membersCache;
        }

        // 设置值的泛型方法，修改为const
        template <typename T>
        bool SetValue(const T& value) const
        {
            if (!checkValid())
            {
                return false;
            }

            *reinterpret_cast<T*>(instance.get()) = value;
            return true;
        }

        // 析构方法，已是const
        void Destructor() const
        {
            if (valid && created && instance && detail::TypesInfo[type].destructor
            )
            {
                detail::TypesInfo[type].destructor->operator()(instance.get());
            }
        }

        // 类型检查方法，已是const
        bool IsClass() const { return typeEnum == RTTMTypeBits::Class; }

        bool IsValid() const { return valid && created; }

        template <typename T>
        bool Is() const
        {
            return type == Object::GetTypeName<T>();
        }

        bool IsPrimitiveType() const
        {
            static const std::unordered_set<std::string> primitiveTypes = {
                "int", "float", "double", "long", "short", "char",
                "unsigned int", "unsigned long", "unsigned short", "unsigned char",
                "bool", "size_t", "int8_t", "int16_t", "int32_t", "int64_t",
                "uint8_t", "uint16_t", "uint32_t", "uint64_t"
            };

            return primitiveTypes.count(type) > 0;
        }

        const std::unordered_set<std::string>& GetPropertyNames() const
        {
            return membersName;
        }

        const std::unordered_set<std::string>& GetMethodNames() const
        {
            return funcsName;
        }

        // 获取原始指针
        void* GetRaw() const
        {
            return valid && created ? instance.get() : nullptr;
        }

        // 类型转换方法
        template <typename T>
        T& As() const
        {
            if (!checkValid())
            {
                throw std::runtime_error("RTTM: Invalid object access: " + type);
            }

            return *reinterpret_cast<T*>(instance.get());
        }

        // 静态获取类型方法
        static RType Get(const std::string& typeName)
        {
            if (detail::TypesInfo.find(typeName) == detail::TypesInfo.end())
            {
                RType newStruct(typeName);
                throw std::runtime_error("RTTM: The structure has not been registered: " + typeName);
                return newStruct;
            }
            std::unordered_set<std::string> membersName;
            std::unordered_set<std::string> funcsName;
            for (const auto& [name, _] : detail::TypesInfo[typeName].members)
            {
                membersName.insert(name);
            }
            for (const auto& [name, _] : detail::TypesInfo[typeName].functions)
            {
                funcsName.insert(name);
            }
            RType newStruct(typeName, detail::TypesInfo[typeName].type, membersName, funcsName, nullptr);
            return newStruct;
        }

        template <typename T>
        static RType Get()
        {
            return Get(Object::GetTypeName<T>());
        }

        // 析构函数
        ~RType()
        {
            if (valid && created && !type.empty()
            )
            {
                Destructor();
            }
        }
    };

    class Global
    {
        template <typename T, typename... Args>
        static Method<T> GetMethodOrig(const std::string& name)
        {
            if (detail::GFunctions.find(name) == detail::GFunctions.end())
            {
                throw std::runtime_error("RTTM: The function has not been registered: " + name);
            }

            auto _f = std::dynamic_pointer_cast<FunctionWrapper<T, Args...>>(detail::GFunctions[name]);
            if (!_f)
            {
                throw std::runtime_error("RTTM: Function signature mismatch for: " + name);
            }
            return Method<T>(_f, name, nullptr, false);
        }

        template <typename FuncType>
        static auto GetMethodImpl(const std::string& name)
        {
            using traits = detail::function_traits<FuncType>;
            using ReturnType = typename traits::return_type;

            return std::apply([&name](auto... args)
            {
                return GetMethodOrig<ReturnType, decltype(args)...>(name);
            }, typename traits::arguments{});
        }

    public:
        template <typename T>
        static void RegisterVariable(const std::string& name, T value)
        {
            detail::Variables[name] = static_cast<Ref<void>>(CreateRef<T>(value));
        }

        template <typename T>
        static T& GetVariable(const std::string& name)
        {
            return *std::static_pointer_cast<T>(detail::Variables[name]);
        }

        template <typename R, typename... Args>
        static void RegisterGlobalMethod(const std::string& name, R (*f)(Args...))
        {
            detail::GFunctions[name] = CreateRef<FunctionWrapper<R, Args...>>(f);
        }

        template <typename R, typename... Args>
        static void RegisterGlobalMethod(const std::string& name, const std::function<R(Args...)>& func)
        {
            auto wrapper = CreateRef<FunctionWrapper<R, Args...>>(func);
            detail::GFunctions[name] = wrapper;
        }

        template <typename F>
        static void RegisterGlobalMethod(const std::string& name, F f)
        {
            std::function func = f;
            RegisterGlobalMethod(name, func);
        }

        template <typename T, typename... Args>
        static T InvokeG(const std::string& name, Args... args)
        {
            if (detail::GFunctions.find(name) == detail::GFunctions.end())
            {
                std::cerr << "RTTM: The function has not been registered: " << name << std::endl;
                return T();
            }
            return (*std::dynamic_pointer_cast<FunctionWrapper<T, Args...>>(detail::GFunctions[name]))(
                std::forward<Args>(args)...);
        }

        template <typename FuncType>
        static auto GetMethod(const std::string& name)
        {
            return GetMethodImpl<FuncType>(name);
        }
    };
} // RTTM

#define CONCAT_IMPL(s1, s2) s1##s2
#define CONCAT(s1, s2) CONCAT_IMPL(s1, s2)
#define RTTM_REGISTRATION                           \
namespace {                                               \
struct CONCAT(RegistrationHelper_, __LINE__) {          \
CONCAT(RegistrationHelper_, __LINE__)();           \
};}                                                    \
static CONCAT(RegistrationHelper_, __LINE__)           \
CONCAT(registrationHelperInstance_, __LINE__);    \
CONCAT(RegistrationHelper_, __LINE__)::CONCAT(RegistrationHelper_, __LINE__)()
#endif //RTTM_H
