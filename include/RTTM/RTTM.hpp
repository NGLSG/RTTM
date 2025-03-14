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

    template <typename T>
    inline static const std::type_index& CachedTypeIndex()
    {
        static const std::type_index index(typeid(T));
        return index;
    }

    template <typename T>
    inline static const std::string& CachedTypeName()
    {
        static const std::string name = Object::GetTypeName<T>();
        return name;
    }

    // 检测是否是 STL 容器 - using variable templates for better performance
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
        template <typename T>
        static void PreparePool(size_t count)
        {
            if constexpr (std::is_class_v<T>)
            {
                static thread_local std::vector<Ref<char>> objectPool;
                objectPool.reserve(count);
                //预先分配内存
            }
        }

        //返回void*智能指针,待使用时转换
        template <typename... Args>
        Ref<char> Create(Args... args)
        {
            return (*std::static_pointer_cast<FunctionWrapper<Ref<char>, Args...>>(createFunc))(
                std::forward<Args>(args)...);
        }
    };

    template <typename T, typename... Args>
    class Factory : public IFactory
    {
        using Allocator = std::allocator<T>;
        using Traits = std::allocator_traits<Allocator>;
        Ref<Allocator> allocator;

        static constexpr size_t BLOCK_SIZE = 64;
        static thread_local std::vector<void*> memoryPool;

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

            if constexpr (sizeof(T) <= BLOCK_SIZE && std::is_trivially_destructible_v<T>)
            {
                for (size_t i = 0; i < 8; i++)
                {
                    memoryPool.push_back(Traits::allocate(*allocator, 1));
                }
            }
        }

        static Ref<IFactory> CreateFactory()
        {
            static thread_local Ref<Factory<T, Args...>> instance = CreateRef<Factory<T, Args...>>();
            return instance;
        }

        Ref<char> Create(Args... args)
        {
            T* rawPtr = nullptr;

            if constexpr (sizeof(T) <= BLOCK_SIZE && std::is_trivially_destructible_v<T>)
            {
                if (!memoryPool.empty())
                {
                    rawPtr = static_cast<T*>(memoryPool.back());
                    memoryPool.pop_back();
                }
            }

            if (rawPtr == nullptr)
            {
                rawPtr = Traits::allocate(*allocator, 1);
            }

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

            return Ref<char>(reinterpret_cast<char*>(rawPtr),
                             [allocCopy = this->allocator](char* ptr)
                             {
                                 T* typedPtr = reinterpret_cast<T*>(ptr);
                                 Traits::destroy(*allocCopy, typedPtr);

                                 if constexpr (sizeof(T) <= BLOCK_SIZE && std::is_trivially_destructible_v<T>)
                                 {
                                     if (memoryPool.size() < 64)
                                     {
                                         memoryPool.push_back(typedPtr);
                                         return;
                                     }
                                 }

                                 Traits::deallocate(*allocCopy, typedPtr, 1);
                             }
            );
        }
    };

    template <typename T, typename... Args>
    thread_local std::vector<void*> Factory<T, Args...>::memoryPool;

    namespace detail
    {
        struct Member
        {
            size_t offset;
            RTTMTypeBits type;
            std::type_index typeIndex = std::type_index(typeid(void));
        };

        using ValueMap = std::unordered_map<std::string, Ref<void>, std::hash<std::string>, std::equal_to<>>; //变量
        using FunctionMap = std::unordered_map<std::string, Ref<IFunctionWrapper>, std::hash<std::string>, std::equal_to
                                               <>>; //函数
        using MemberMap = std::unordered_map<std::string, Member, std::hash<std::string>, std::equal_to<>>; //类的成员的偏移量
        using EnumMap = std::unordered_map<std::string, int, std::hash<std::string>, std::equal_to<>>; //枚举的值

        struct TypeBaseInfo
        {
            RTTMTypeBits type;
            std::type_index typeIndex = std::type_index(typeid(void));
            size_t size;
            FunctionMap functions;
            MemberMap members;
            std::unordered_map<std::string, Ref<IFactory>, std::hash<std::string>, std::equal_to<>> factories;
            // 构造函数,如果是枚举则为空
            std::unordered_map<std::string, std::string, std::hash<std::string>, std::equal_to<>> membersType; //成员的类型
            Ref<FunctionWrapper<void, void*>> destructor;
            Ref<FunctionWrapper<void, void*, void*>> copier;

            mutable std::unordered_map<std::string, Ref<IFunctionWrapper>> functionCache;

            bool hierarchyFlattened = false;

            void AppendFunction(const std::string& name, const Ref<IFunctionWrapper>& func)
            {
                functions[name] = func;
                functionCache.clear();
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
                functionCache.clear(); // Invalidate cache
            }

            void AppendMembers(const MemberMap& mems)
            {
                members.insert(mems.begin(), mems.end());
            }

            void AppendMembersType(const std::unordered_map<std::string, std::string>& mems)
            {
                membersType.insert(mems.begin(), mems.end());
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
                typeIndex = other.typeIndex;
                size = other.size;
                functions = other.functions;
                members = other.members;
                factories = other.factories;
                membersType = other.membersType;
                destructor = other.destructor;
                copier = other.copier;
                functionCache.clear();
                hierarchyFlattened = false;
                return *this;
            }

            Ref<IFunctionWrapper> FindFunction(const std::string& signature) const
            {
                auto it = functionCache.find(signature);
                if (it != functionCache.end())
                {
                    return it->second;
                }

                auto fit = functions.find(signature);
                if (fit != functions.end())
                {
                    functionCache[signature] = fit->second;
                    return fit->second;
                }

                return nullptr;
            }
        };

        using TypeInfoMap = std::unordered_map<std::string, TypeBaseInfo, std::hash<std::string>, std::equal_to<>>;
        //类的信息

        inline TypeInfoMap TypesInfo; //预注册的类的信息
        inline ValueMap Variables; //全局变量
        inline FunctionMap GFunctions; //全局函数
        inline std::unordered_map<std::string, EnumMap, std::hash<std::string>, std::equal_to<>> Enums;
        //枚举

        inline std::unordered_set<std::string> RegisteredTypes;

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

        template <typename T>
        struct ReplaceStringTypes
        {
            using type = T;
        };

        template <>
        struct ReplaceStringTypes<std::string>
        {
            using type = const char*;
        };

        template <>
        struct ReplaceStringTypes<std::wstring>
        {
            using type = const wchar_t*;
        };

        template <typename T, typename... Args>
        struct TransformedFactory
        {
            using FactoryType = Factory<T, typename ReplaceStringTypes<Args>::type...>;

            static auto Create()
            {
                return FactoryType::CreateFactory();
            }
        };

        struct StringPatternCache
        {
            static std::string ReplacePattern(const std::string& src,
                                              const std::string& from,
                                              const std::string& to)
            {
                std::string key = src + "|" + from + "|" + to;

                static thread_local std::unordered_map<std::string, std::string> cache;
                auto it = cache.find(key);
                if (it != cache.end())
                {
                    return it->second;
                }

                std::string result = src;
                size_t start_pos = 0;
                while ((start_pos = result.find(from, start_pos)) != std::string::npos)
                {
                    result.replace(start_pos, from.length(), to);
                    start_pos += to.length();
                }

                if (cache.size() > 1000)
                {
                    cache.clear();
                }
                cache[key] = result;
                return result;
            }
        };
    }

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
                if constexpr (std::is_destructible_v<T>)
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
            if (detail::RegisteredTypes.count(typeName) > 0)
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

            auto& typeInfo = detail::TypesInfo[typeName];
            typeInfo.type = type;
            typeInfo.size = sizeof(T);
            typeInfo.typeIndex = typeIndex;

            detail::RegisteredTypes.insert(typeName);

            destructor(); //注册析构函数
            constructor(); //注册默认构造函数
            copier(); //注册拷贝构造函数
        }

        template <typename U>
        Registry_& base()
        {
            auto base = Object::GetTypeName<U>();
            if (detail::RegisteredTypes.count(base) == 0)
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
            const static std::string cc_strType = Object::GetTypeName<const char*>();
            const static std::string strType = Object::GetTypeName<std::string>();
            const static std::string cwc_strType = Object::GetTypeName<const wchar_t*>();
            const static std::string wstrType = Object::GetTypeName<std::wstring>();

            std::string name = Object::GetTypeName<Args...>();
            detail::TypesInfo[typeName].factories[name] =
                Factory<T, Args...>::CreateFactory();

            if (name.find(strType) != std::string::npos)
            {
                std::string modifiedName = detail::StringPatternCache::ReplacePattern(name, strType, cc_strType);
                detail::TypesInfo[typeName].factories[modifiedName] = detail::TransformedFactory<T, Args...>::Create();
            }

            if (name.find(wstrType) != std::string::npos)
            {
                std::string modifiedName = detail::StringPatternCache::ReplacePattern(name, wstrType, cwc_strType);
                detail::TypesInfo[typeName].factories[modifiedName] = detail::TransformedFactory<T, Args...>::Create();
            }

            return *this;
        }


        template <typename U>
        Registry_& property(const char* name, U T::* value)
        {
            size_t offset = reinterpret_cast<size_t>(&(reinterpret_cast<T*>(0)->*value));

            if constexpr (std::is_class_v<U> && !is_stl_container_v<U> && !is_string_v<U>)
            {
                detail::TypesInfo[typeName].members[name] = {offset, RTTMTypeBits::Class, std::type_index(typeid(U))};
            }
            else if constexpr (std::is_enum_v<U>)
            {
                detail::TypesInfo[typeName].members[name] = {offset, RTTMTypeBits::Enum, std::type_index(typeid(U))};
            }
            else
            {
                detail::TypesInfo[typeName].members[name] = {
                    offset, RTTMTypeBits::Variable, std::type_index(typeid(U))
                };
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

            std::string signature = name + Object::GetTypeName<Args...>();

            auto func = CreateRef<FunctionWrapper<R, void*, Args...>>([memFunc](void* obj, Args... args)
            {
                T* _obj = static_cast<T*>(obj);
                return (_obj->*memFunc)(std::forward<Args>(args)...);
            });

            detail::TypesInfo[typeName].functions[signature] = func;
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

            std::string signature = name + Object::GetTypeName<Args...>();

            auto func = CreateRef<FunctionWrapper<R, void*, Args...>>([memFunc](void* obj, Args... args)
            {
                T* _obj = static_cast<T*>(obj);
                return (_obj->*memFunc)(std::forward<Args>(args)...);
            });

            detail::TypesInfo[typeName].functions[signature] = func;
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

        struct CallCache
        {
            std::vector<uint8_t> argsHash;
            T result;
            bool valid = false;
        };

        mutable CallCache lastCall;

        template <typename... Args>
        std::vector<uint8_t> hashArgs(Args&&... args) const
        {
            std::vector<uint8_t> hash;
            if constexpr (std::conjunction_v<std::is_trivial<Args>...> && sizeof...(Args) > 0)
            {
                (appendBytes(hash, args), ...);
            }
            return hash;
        }

        template <typename Arg>
        void appendBytes(std::vector<uint8_t>& hash, const Arg& arg) const
        {
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&arg);
            hash.insert(hash.end(), bytes, bytes + sizeof(Arg));
        }

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
            if (!IsValid())
            {
                throw std::runtime_error("Cannot invoke invalid method: " + name);
            }

            if constexpr (std::conjunction_v<std::is_trivial<Args>...> &&
                std::is_trivial_v<T> &&
                sizeof...(Args) > 0)
            {
                auto hash = hashArgs(args...);
                if (lastCall.valid && lastCall.argsHash == hash)
                {
                    return lastCall.result;
                }

                if (isMemberFunction)
                {
                    if (!instance)
                        throw std::runtime_error("Instance is null for member function: " + name);
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, void*, Args...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "Function signature mismatch for: " + name + " Arguments are: " + Object::GetTypeName<Args
                                ...>());

                    T result = (*wrapper)(instance, std::forward<Args>(args)...);

                    lastCall.argsHash = std::move(hash);
                    lastCall.result = result;
                    lastCall.valid = true;

                    return result;
                }
                else
                {
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, Args...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "Function signature mismatch for: " + name + " Arguments are: " + Object::GetTypeName<Args
                                ...>());

                    T result = (*wrapper)(std::forward<Args>(args)...);

                    lastCall.argsHash = std::move(hash);
                    lastCall.result = result;
                    lastCall.valid = true;

                    return result;
                }
            }
            else
            {
                if (isMemberFunction)
                {
                    if (!instance)
                        throw std::runtime_error("Instance is null for member function: " + name);
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, void*, Args...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "Function signature mismatch for: " + name + " Arguments are: " + Object::GetTypeName<Args
                                ...>());

                    return (*wrapper)(instance, std::forward<Args>(args)...);
                }
                else
                {
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, Args...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "Function signature mismatch for: " + name + " Arguments are: " + Object::GetTypeName<Args
                                ...>());

                    return (*wrapper)(std::forward<Args>(args)...);
                }
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
            name = other.name;
            instance = other.instance;
            isMemberFunction = other.isMemberFunction;
            lastCall.valid = false;
            return *this;
        }
    };

    class RType
    {
    private:
        mutable bool lastValidityCheck = false;
        mutable bool validityCacheValid = false;

        template <typename R = bool>
        R checkValid() const
        {
            if (validityCacheValid)
            {
                if (!lastValidityCheck)
                {
                    throw std::runtime_error("RTTM: Invalid object state");
                }
                return R(lastValidityCheck);
            }

            bool isValid = valid && created;
            if (!isValid)
            {
                if (!valid)
                    throw std::runtime_error("RTTM: The structure has not been registered");
                if (!created)
                    throw std::runtime_error("RTTM: The object has not been created");
            }

            // Cache the result
            lastValidityCheck = isValid;
            validityCacheValid = true;

            return R(isValid);
        }

        void _copyFrom(const Ref<char>& newInst) const
        {
            if (!checkValid()) return;

            if (detail::TypesInfo[type].copier)
            {
                detail::TypesInfo[type].copier->operator()(newInst.get(), instance.get());
            }
            else
            {
                throw std::runtime_error("RTTM: Copy constructor not registered for: " + type);
            }
        }

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

        template <typename T, typename... Args>
        Method<T> GetMethodOrig(const std::string& name) const
        {
            static thread_local std::unordered_map<std::string, std::string> signatureCache;

            std::string tn;
            std::string cacheKey = name + "|" + type;
            auto sigIt = signatureCache.find(cacheKey);
            if (sigIt != signatureCache.end())
            {
                tn = sigIt->second;
            }
            else
            {
                tn = name + Object::GetTypeName<Args...>();
                signatureCache[cacheKey] = tn;
            }

            if (!checkValid())
            {
                return Method<T>(nullptr, name, nullptr, false);
            }

            auto& typeInfo = detail::TypesInfo[type];
            Ref<IFunctionWrapper> funcWrapper = typeInfo.FindFunction(tn);

            if (!funcWrapper)
            {
                throw std::runtime_error("Function not found: " + name);
            }

            auto _f = std::static_pointer_cast<FunctionWrapper<T, void*, Args...>>(funcWrapper);
            if (!_f)
            {
                throw std::runtime_error(
                    "Function signature mismatch for: " + name + " Arguments are: " + Object::GetTypeName<Args...>());
            }

            return Method<T>(_f, name, static_cast<void*>(instance.get()), true);
        }

        void preloadAllProperties() const
        {
            if (!checkValid() || propertiesPreloaded) return;

            auto& typeInfo = detail::TypesInfo[type];
            for (const auto& [name, memberInfo] : typeInfo.members)
            {
                // If already in cache, skip
                if (membersCache.find(name) != membersCache.end()) continue;

                auto typeIt = typeInfo.membersType.find(name);
                if (typeIt == typeInfo.membersType.end()) continue;

                auto newStruct = Create(typeIt->second, memberInfo.type, memberInfo.typeIndex);
                newStruct->instance = AliasCreate(instance, instance.get() + memberInfo.offset);
                newStruct->valid = true;
                newStruct->created = true;
                membersCache[name] = newStruct;
            }

            propertiesPreloaded = true;
        }

    protected:
        std::string type;
        RTTMTypeBits typeEnum;
        std::type_index typeIndex = std::type_index(typeid(void));
        mutable Ref<char> instance;
        mutable bool valid = false;
        mutable bool created = false;

        mutable std::unordered_set<std::string> membersName;
        mutable std::unordered_set<std::string> funcsName;
        mutable std::unordered_map<std::string, Ref<RType>> membersCache;

        mutable bool propertiesPreloaded = false;

        struct PropertyLookupCache
        {
            std::string lastPropertyName;
            size_t offset = 0;
            bool valid = false;
        };

        mutable PropertyLookupCache propCache;

    public:
        RType() = default;

        RType(std::string _type) : type(std::move(_type)), valid(!_type.empty())
        {
            if (valid)
            {
                if (detail::TypesInfo.find(type) == detail::TypesInfo.end())
                {
                    valid = false;
                    throw std::runtime_error("RTTM: Type not registered: " + type);
                    return;
                }

                auto& typeInfo = detail::TypesInfo[type];
                typeEnum = typeInfo.type;
                typeIndex = typeInfo.typeIndex;

                for (const auto& [name, _] : typeInfo.members)
                {
                    membersName.insert(name);
                }

                for (const auto& [name, _] : typeInfo.functions)
                {
                    size_t paramStart = name.find('(');
                    if (paramStart != std::string::npos)
                    {
                        funcsName.insert(name.substr(0, paramStart));
                    }
                    else
                    {
                        funcsName.insert(name);
                    }
                }
            }
        }

        RType(const std::string& _type, RTTMTypeBits _typeEnum, const std::type_index& _typeIndex,
              const std::unordered_set<std::string>& _membersName = {},
              const std::unordered_set<std::string>& _funcsName = {},
              const Ref<char>& _instance = nullptr):
            type(_type),
            typeEnum(_typeEnum),
            typeIndex(_typeIndex),
            instance(_instance),
            valid(!_type.empty()),
            created(_typeEnum == RTTMTypeBits::Variable || _instance != nullptr),
            membersName(_membersName),
            funcsName(_funcsName)
        {
            if (!valid && !type.empty())
            {
                if (detail::RegisteredTypes.count(type) == 0)
                {
                    throw std::runtime_error("RTTM: Type not registered: " + type);
                    valid = false;
                    return;
                }
            }

            if (valid)
            {
                if (membersName.empty())
                {
                    for (const auto& [name, _] : detail::TypesInfo[type].members)
                    {
                        membersName.insert(name);
                    }
                }

                if (funcsName.empty())
                {
                    for (const auto& [name, _] : detail::TypesInfo[type].functions)
                    {
                        size_t paramStart = name.find('(');
                        if (paramStart != std::string::npos)
                        {
                            funcsName.insert(name.substr(0, paramStart));
                        }
                        else
                        {
                            funcsName.insert(name);
                        }
                    }
                }
            }
        }

        static Ref<RType> Create(const std::string& type, RTTMTypeBits typeEnum, const std::type_index& typeIndex,
                                 const std::unordered_set<std::string>& membersName = {},
                                 const std::unordered_set<std::string>& funcsName = {},
                                 const Ref<char>& instance = nullptr)
        {
            static thread_local std::vector<Ref<RType>> typePool;

            if (!typePool.empty())
            {
                auto rtype = typePool.back();
                typePool.pop_back();

                rtype->type = type;
                rtype->typeEnum = typeEnum;
                rtype->typeIndex = typeIndex;
                rtype->instance = instance;
                rtype->valid = !type.empty();
                rtype->created = (typeEnum == RTTMTypeBits::Variable || instance != nullptr);
                rtype->membersName = membersName;
                rtype->funcsName = funcsName;
                rtype->membersCache.clear();
                rtype->propertiesPreloaded = false;
                rtype->validityCacheValid = false;
                rtype->propCache.valid = false;

                return rtype;
            }

            return CreateRef<RType>(type, typeEnum, typeIndex, membersName, funcsName, instance);
        }

        template <typename... Args>
        bool Create(Args... args) const
        {
            if (!valid)
            {
                throw std::runtime_error("RTTM: Invalid type: " + type);
            }

            static thread_local std::unordered_map<std::string, std::pair<std::string, Ref<IFactory>>> factoryCache;

            std::string factoryKey = type + Object::GetTypeName<Args...>();
            Ref<IFactory> factory;

            auto factoryIt = factoryCache.find(factoryKey);
            if (factoryIt != factoryCache.end())
            {
                factory = factoryIt->second.second;
            }
            else
            {
                auto _t = Object::GetTypeName<Args...>();
                auto& factories = detail::TypesInfo[type].factories;
                auto factoryMapIt = factories.find(_t);

                if (factoryMapIt == factories.end())
                {
                    throw std::runtime_error("RTTM: Factory not registered for: " + type + " with args " + _t);
                }

                factory = factoryMapIt->second;
                if (factoryCache.size() > 100)
                {
                    factoryCache.clear();
                }
                factoryCache[factoryKey] = {_t, factory};
            }

            if (!factory)
            {
                throw std::runtime_error("RTTM: Invalid factory for: " + type);
            }

            auto newInst = factory->Create(std::forward<Args>(args)...);
            if (!newInst)
            {
                throw std::runtime_error("RTTM: Failed to create instance of: " + type);
            }

            if (instance)
            {
                _copyFrom(newInst);
            }
            else
            {
                instance = newInst;
            }

            created = true;
            validityCacheValid = true;
            lastValidityCheck = true;
            propertiesPreloaded = false;
            propCache.valid = false;
            membersCache.clear();

            return true;
        }

        template <typename T>
        void Attach(T& inst) const
        {
#ifndef NDEBUG
            if (typeIndex != std::type_index(typeid(T)))
            {
                throw std::runtime_error("RTTM: Type mismatch in Attach");
            }
#endif

            instance = AliasCreate(instance, static_cast<char*>(reinterpret_cast<void*>(&inst)));
            membersCache.clear();
            created = true;
            valid = true;
            validityCacheValid = true;
            lastValidityCheck = true;
            propertiesPreloaded = false;
            propCache.valid = false;
        }

        template <typename R, typename... Args>
        R Invoke(const std::string& name, Args... args) const
        {
            static thread_local std::unordered_map<std::string, std::string> signatureCache;

            std::string cacheKey = name + "|" + type;
            std::string tn;
            auto sigIt = signatureCache.find(cacheKey);
            if (sigIt != signatureCache.end())
            {
                tn = sigIt->second;
            }
            else
            {
                tn = name + Object::GetTypeName<Args...>();
                signatureCache[cacheKey] = tn;
            }

            if (!checkValid<R>())
            {
                return R();
            }

            auto& typeInfo = detail::TypesInfo[type];
            Ref<IFunctionWrapper> funcWrapper = typeInfo.FindFunction(tn);

            if (!funcWrapper)
            {
                throw std::runtime_error("RTTM: Function not found: " + tn);
            }

            auto funcPtr = std::static_pointer_cast<FunctionWrapper<R, void*, Args...>>(funcWrapper);
            if (!funcPtr)
            {
                throw std::runtime_error("RTTM: Invalid function signature for: " + tn);
            }

            return funcPtr->operator()(static_cast<void*>(instance.get()), std::forward<Args>(args)...);
        }

        template <typename FuncType>
        auto GetMethod(const std::string& name) const
        {
            return GetMethodImpl<FuncType>(name);
        }

        template <typename T>
        T& GetProperty(const std::string& name) const
        {
            if (!checkValid())
            {
                static T dummy{};
                return dummy;
            }

            size_t offset;

            if (propCache.valid && propCache.lastPropertyName == name)
            {
                offset = propCache.offset;
            }
            else
            {
                auto& members = detail::TypesInfo[type].members;
                auto it = members.find(name);
                if (it == members.end())
                {
                    static T dummy{};
                    throw std::runtime_error("RTTM: Member not found: " + name);
                    return dummy;
                }

                offset = it->second.offset;

                propCache.lastPropertyName = name;
                propCache.offset = offset;
                propCache.valid = true;
            }

            return *reinterpret_cast<T*>(instance.get() + offset);
        }

        Ref<RType> GetProperty(const std::string& name) const
        {
            if (!checkValid())
            {
                throw std::runtime_error("RTTM: Invalid object access: " + type);
            }

            auto cacheIt = membersCache.find(name);
            if (cacheIt != membersCache.end())
            {
                return cacheIt->second;
            }

            auto& members = detail::TypesInfo[type].members;
            auto it = members.find(name);
            if (it == members.end())
            {
                throw std::runtime_error("RTTM: Member not found: " + name);
            }

            auto& typeInfo = detail::TypesInfo[type];
            auto typeIt = typeInfo.membersType.find(name);
            if (typeIt == typeInfo.membersType.end())
            {
                throw std::runtime_error("RTTM: Member type not found: " + name);
            }

            auto newStruct = Create(typeIt->second, it->second.type, it->second.typeIndex);
            newStruct->instance = AliasCreate(instance, instance.get() + it->second.offset);
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

            if (!propertiesPreloaded)
            {
                preloadAllProperties();
            }

            return membersCache;
        }

        template <typename T>
        bool SetValue(const T& value) const
        {
            if (!checkValid())
            {
                return false;
            }

#ifndef NDEBUG
            if (typeIndex != std::type_index(typeid(T)) && !IsPrimitiveType())
            {
                throw std::runtime_error("RTTM: Type mismatch in SetValue");
            }
#endif

            *reinterpret_cast<T*>(instance.get()) = value;
            return true;
        }

        void Destructor() const
        {
            if (valid && created && instance && detail::TypesInfo[type].destructor)
            {
                detail::TypesInfo[type].destructor->operator()(instance.get());

                created = false;
                validityCacheValid = false;
            }
        }

        bool IsClass() const { return typeEnum == RTTMTypeBits::Class; }
        bool IsEnum() const { return typeEnum == RTTMTypeBits::Enum; }
        bool IsVariable() const { return typeEnum == RTTMTypeBits::Variable; }

        bool IsValid() const
        {
            if (validityCacheValid)
            {
                return lastValidityCheck;
            }

            bool isValid = valid && created;
            lastValidityCheck = isValid;
            validityCacheValid = true;
            return isValid;
        }

        template <typename T>
        bool Is() const
        {
            static const std::type_index cachedTypeIndex = std::type_index(typeid(T));
            return typeIndex == cachedTypeIndex;
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

        void* GetRaw() const
        {
            return valid && created ? instance.get() : nullptr;
        }

        template <typename T>
        T& As() const
        {
            if (!checkValid())
            {
                throw std::runtime_error("RTTM: Invalid object access: " + type);
            }

#ifndef NDEBUG
            if (typeIndex != std::type_index(typeid(T)))
            {
                throw std::runtime_error("RTTM: Type mismatch in As<T>");
            }
#endif

            return *reinterpret_cast<T*>(instance.get());
        }

        static Ref<RType> Get(const std::string& typeName)
        {
            if (detail::RegisteredTypes.count(typeName) == 0)
            {
                Ref<RType> newStruct = CreateRef<RType>(typeName);
                throw std::runtime_error("RTTM: Type not registered: " + typeName);
                return newStruct;
            }

            std::unordered_set<std::string> membersName;
            std::unordered_set<std::string> funcsName;

            auto& typeInfo = detail::TypesInfo[typeName];
            for (const auto& [name, _] : typeInfo.members)
            {
                membersName.insert(name);
            }

            for (const auto& [name, _] : typeInfo.functions)
            {
                size_t paramStart = name.find('(');
                if (paramStart != std::string::npos)
                {
                    funcsName.insert(name.substr(0, paramStart));
                }
                else
                {
                    funcsName.insert(name);
                }
            }

            return CreateRef<RType>(typeName, typeInfo.type, typeInfo.typeIndex,
                                    membersName, funcsName, nullptr);
        }

        template <typename T>
        static Ref<RType> Get()
        {
            return Get(CachedTypeName<T>());
        }

        ~RType()
        {
            if (valid && created && !type.empty())
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
                throw std::runtime_error("RTTM: Function not registered: " + name);
            }

            auto _f = std::static_pointer_cast<FunctionWrapper<T, Args...>>(detail::GFunctions[name]);
            if (!_f)
            {
                throw std::runtime_error(
                    "RTTM: Function signature mismatch for: " + name +
                    " Arguments are: " + Object::GetTypeName<Args...>() +
                    " Required types are: " + detail::GFunctions[name]->argumentTypes);
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

        static bool FunctionExists(const std::string& name)
        {
            static thread_local std::unordered_map<std::string, bool> existenceCache;

            auto it = existenceCache.find(name);
            if (it != existenceCache.end())
            {
                return it->second;
            }

            bool exists = detail::GFunctions.find(name) != detail::GFunctions.end();
            existenceCache[name] = exists;
            return exists;
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
            auto it = detail::Variables.find(name);
            if (it == detail::Variables.end())
            {
                throw std::runtime_error("RTTM: Variable not found: " + name);
            }

            auto ptr = std::static_pointer_cast<T>(it->second);
            if (!ptr)
            {
                throw std::runtime_error("RTTM: Type mismatch for variable: " + name);
            }

            return *ptr;
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
            if (!FunctionExists(name))
            {
                throw std::runtime_error("RTTM: Function not registered: " + name);
            }

            return GetMethodOrig<T, Args...>(name).Invoke(std::forward<Args>(args)...);
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
