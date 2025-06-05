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
#include <mutex>
#include <atomic>

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

    // 检测是否是 STL 容器 - 使用变量模板以获得更好的性能
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
        _Ref_<IFunctionWrapper> createFunc;

    public:
        template <typename T>
        static void PreparePool(size_t count)
        {
            if constexpr (std::is_class_v<T>)
            {
                static thread_local std::vector<_Ref_<char>> objectPool;
                objectPool.reserve(count);
                // 预先分配内存
            }
        }

        // 返回void*智能指针，使用时转换
        template <typename... Args>
        _Ref_<char> Create(Args... args)
        {
            return (*std::static_pointer_cast<FunctionWrapper<_Ref_<char>, Args...>>(createFunc))(
                std::forward<Args>(args)...);
        }
    };

    // 优化的对象池管理器
    template <typename T>
    class ObjectPoolManager
    {
    private:
        static constexpr size_t POOL_SIZE = 128; // 对象池大小
        static constexpr size_t BLOCK_SIZE = 1024; // 最大对象大小限制

        struct alignas(T) ObjectSlot
        {
            char data[sizeof(T)];
        };

        std::vector<ObjectSlot*> availableSlots; // 可用对象槽
        std::unique_ptr<ObjectSlot[]> memoryBlock; // 预分配内存块
        std::atomic<size_t> poolIndex{0}; // 原子索引，避免锁竞争
        mutable std::mutex poolMutex; // 保护池操作的互斥锁

    public:
        ObjectPoolManager()
        {
            if constexpr (sizeof(T) <= BLOCK_SIZE && std::is_trivially_destructible_v<T>)
            {
                // 预分配对齐的内存块
                memoryBlock = std::make_unique<ObjectSlot[]>(POOL_SIZE);
                availableSlots.reserve(POOL_SIZE);

                // 初始化所有槽位为可用状态
                for (size_t i = 0; i < POOL_SIZE; ++i)
                {
                    availableSlots.push_back(&memoryBlock[i]);
                }
            }
        }

        T* acquireObject()
        {
            if constexpr (sizeof(T) <= BLOCK_SIZE && std::is_trivially_destructible_v<T>)
            {
                std::lock_guard<std::mutex> lock(poolMutex);
                if (!availableSlots.empty())
                {
                    T* obj = reinterpret_cast<T*>(availableSlots.back());
                    availableSlots.pop_back();
                    return obj;
                }
            }
            return nullptr;
        }

        void releaseObject(T* obj)
        {
            if constexpr (sizeof(T) <= BLOCK_SIZE && std::is_trivially_destructible_v<T>)
            {
                std::lock_guard<std::mutex> lock(poolMutex);
                if (availableSlots.size() < POOL_SIZE)
                {
                    availableSlots.push_back(reinterpret_cast<ObjectSlot*>(obj));
                }
            }
        }

        size_t getAvailableCount() const
        {
            std::lock_guard<std::mutex> lock(poolMutex);
            return availableSlots.size();
        }
    };

    template <typename T, typename... Args>
    class Factory : public IFactory
    {
        using Allocator = std::allocator<T>;
        using Traits = std::allocator_traits<Allocator>;
        _Ref_<Allocator> allocator;

        // 使用优化的对象池管理器
        static ObjectPoolManager<T> objectPool;

        // 工厂创建计数器，用于性能监控
        mutable std::atomic<size_t> createCount{0};

    public:
        Factory()
        {
            if constexpr (!std::is_class_v<T>)
            {
                throw std::runtime_error("RTTM: T 必须是结构体或类");
            }
            allocator = Create_Ref_<Allocator>();
            createFunc = std::make_shared<FunctionWrapper<_Ref_<char>, Args...>>([this](Args... args)
            {
                return Create(std::forward<Args>(args)...);
            });
        }

        static _Ref_<IFactory> CreateFactory()
        {
            // 使用线程局部静态变量避免重复创建
            static thread_local _Ref_<Factory<T, Args...>> instance = Create_Ref_<Factory<T, Args...>>();
            return instance;
        }

        _Ref_<char> Create(Args... args)
        {
            createCount.fetch_add(1, std::memory_order_relaxed);

            T* rawPtr = nullptr;
            bool fromPool = false;

            // 优先尝试从对象池获取
            rawPtr = objectPool.acquireObject();
            fromPool = (rawPtr != nullptr);

            // 对象池没有可用对象，分配新内存
            if (rawPtr == nullptr)
            {
                rawPtr = Traits::allocate(*allocator, 1);
            }

            if (rawPtr == nullptr)
            {
                throw std::runtime_error("RTTM: 无法为对象分配内存");
            }

            try
            {
                // 使用完美转发和placement new构造对象
                Traits::construct(*allocator, rawPtr, std::forward<Args>(args)...);
            }
            catch (std::exception& e)
            {
                // 构造失败，归还内存
                if (fromPool)
                {
                    objectPool.releaseObject(rawPtr);
                }
                else
                {
                    Traits::deallocate(*allocator, rawPtr, 1);
                }
                std::cerr << "RTTM: " << e.what() << std::endl;
                throw;
            }

            // 创建智能指针，带自定义删除器
            return _Ref_<char>(reinterpret_cast<char*>(rawPtr),
                               [allocCopy = this->allocator, fromPool](char* ptr)
                               {
                                   T* typedPtr = reinterpret_cast<T*>(ptr);
                                   Traits::destroy(*allocCopy, typedPtr);

                                   if (fromPool)
                                   {
                                       objectPool.releaseObject(typedPtr);
                                   }
                                   else
                                   {
                                       Traits::deallocate(*allocCopy, typedPtr, 1);
                                   }
                               }
            );
        }

        // 获取创建统计信息
        size_t getCreateCount() const { return createCount.load(); }
        size_t getPoolAvailableCount() const { return objectPool.getAvailableCount(); }
    };

    template <typename T, typename... Args>
    ObjectPoolManager<T> Factory<T, Args...>::objectPool;

    namespace detail
    {
        struct Member
        {
            size_t offset;
            RTTMTypeBits type;
            std::type_index typeIndex = std::type_index(typeid(void));
        };

        using ValueMap = std::unordered_map<std::string, _Ref_<void>, std::hash<std::string>, std::equal_to<>>;
        using FunctionMap = std::unordered_map<std::string, _Ref_<IFunctionWrapper>, std::hash<std::string>,
                                               std::equal_to<>>;
        using MemberMap = std::unordered_map<std::string, Member, std::hash<std::string>, std::equal_to<>>;
        using EnumMap = std::unordered_map<std::string, int, std::hash<std::string>, std::equal_to<>>;

        struct TypeBaseInfo
        {
            RTTMTypeBits type;
            std::type_index typeIndex = std::type_index(typeid(void));
            size_t size;
            FunctionMap functions;
            MemberMap members;
            std::unordered_map<std::string, _Ref_<IFactory>, std::hash<std::string>, std::equal_to<>> factories;
            std::unordered_map<std::string, std::string, std::hash<std::string>, std::equal_to<>> membersType;
            _Ref_<FunctionWrapper<void, void*>> destructor;
            _Ref_<FunctionWrapper<void, void*, void*>> copier;

            mutable std::unordered_map<std::string, _Ref_<IFunctionWrapper>> functionCache;

            bool hierarchyFlattened = false;

            void AppendFunction(const std::string& name, const _Ref_<IFunctionWrapper>& func)
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

            void AppendFactory(const std::string& name, const _Ref_<IFactory>& factory)
            {
                factories[name] = factory;
            }

            void AppendFunctions(const FunctionMap& funcs)
            {
                functions.insert(funcs.begin(), funcs.end());
                functionCache.clear();
            }

            void AppendMembers(const MemberMap& mems)
            {
                members.insert(mems.begin(), mems.end());
            }

            void AppendMembersType(const std::unordered_map<std::string, std::string>& mems)
            {
                membersType.insert(mems.begin(), mems.end());
            }

            void AppendFactories(const std::unordered_map<std::string, _Ref_<IFactory>>& facs)
            {
                for (const auto& [name, factory] : facs)
                {
                    if (name == "default")
                        continue; // 默认构造函数不需要添加
                    factories[name] = factory;
                }
            }

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

            _Ref_<IFunctionWrapper> FindFunction(const std::string& signature) const
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

        inline TypeInfoMap TypesInfo;
        inline ValueMap Variables;
        inline FunctionMap GFunctions;
        inline std::unordered_map<std::string, EnumMap, std::hash<std::string>, std::equal_to<>> Enums;

        inline std::unordered_set<std::string> RegisteredTypes;

        template <typename T>
        struct function_traits;

        template <typename Ret, typename... Args>
        struct function_traits<Ret(Args...)>
        {
            using return_type = Ret;
            using arguments = std::tuple<Args...>;
            static constexpr std::size_t arity = sizeof...(Args);

            template <std::size_t N>
            using arg_type = std::tuple_element_t<N, arguments>;
        };

        template <typename Ret, typename Class, typename... Args>
        struct function_traits<Ret(Class::*)(Args...)>
        {
            using return_type = Ret;
            using arguments = std::tuple<Args...>;
            static constexpr std::size_t arity = sizeof...(Args);

            template <std::size_t N>
            using arg_type = std::tuple_element_t<N, arguments>;
        };

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

        // 优化的字符串模式替换缓存
        struct StringPatternCache
        {
            // 使用更高效的替换算法和缓存策略
            static std::string ReplacePattern(const std::string& src,
                                              const std::string& from,
                                              const std::string& to)
            {
                // 线程局部缓存，避免锁竞争
                static thread_local std::unordered_map<std::string, std::string> cache;
                static thread_local size_t cacheHits = 0;
                static thread_local size_t cacheMisses = 0;

                std::string key = src + "|" + from + "|" + to;

                auto it = cache.find(key);
                if (it != cache.end())
                {
                    ++cacheHits;
                    return it->second;
                }

                ++cacheMisses;
                std::string result = src;
                size_t start_pos = 0;
                while ((start_pos = result.find(from, start_pos)) != std::string::npos)
                {
                    result.replace(start_pos, from.length(), to);
                    start_pos += to.length();
                }

                // 智能缓存管理：当缓存过大时，清理最少使用的条目
                if (cache.size() > 2000)
                {
                    // 保留一半缓存，清理另一半
                    auto halfSize = cache.size() / 2;
                    auto it = cache.begin();
                    std::advance(it, halfSize);
                    cache.erase(it, cache.end());
                }

                cache[key] = result;
                return result;
            }

            // 获取缓存统计信息
            static std::pair<size_t, size_t> getCacheStats()
            {
                static thread_local size_t cacheHits = 0;
                static thread_local size_t cacheMisses = 0;
                return {cacheHits, cacheMisses};
            }
        };
    }

    template <typename T>
    class Registry_
    {
        RTTMTypeBits type;
        const std::string typeName;
        const std::type_index typeIndex;

        void destructor() const
        {
            detail::TypesInfo[typeName].destructor = Create_Ref_<FunctionWrapper<void, void*>>([this](void* obj)
            {
                if constexpr (!std::is_class_v<T>)
                {
                    std::cerr << "RTTM: T 必须是结构体或类" << std::endl;
                    return;
                }
                T* _obj = static_cast<T*>(obj);
                if constexpr (std::is_destructible_v<T>)
                    _obj->~T();
            });
        }

        void copier() const
        {
            detail::TypesInfo[typeName].copier = Create_Ref_<FunctionWrapper<void, void*, void*>>(
                [this](void* src, void* dest)
                {
                    if constexpr (!std::is_class_v<T>)
                    {
                        std::cerr << "RTTM: T 必须是结构体或类" << std::endl;
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
                std::cerr << "RTTM: 结构体已经注册: " << typeName << std::endl;
                return;
            }

            if constexpr (std::is_class_v<T>)
            {
                type = RTTMTypeBits::Class;
            }
            else if constexpr (std::is_enum_v<T>)
            {
                type = RTTMTypeBits::Enum;
            }
            else
            {
                throw std::runtime_error("RTTM: T 必须是结构体、类或枚举");
            }

            auto& typeInfo = detail::TypesInfo[typeName];
            typeInfo.type = type;
            typeInfo.size = sizeof(T);
            typeInfo.typeIndex = typeIndex;

            detail::RegisteredTypes.insert(typeName);

            destructor();
            constructor();
            copier();
        }

        template <typename U>
        Registry_& base()
        {
            auto base = Object::GetTypeName<U>();
            if (detail::RegisteredTypes.count(base) == 0)
            {
                std::cerr << "RTTM: 基类结构体尚未注册: " << base << std::endl;
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
            detail::TypesInfo[typeName].factories[name] = Factory<T, Args...>::CreateFactory();

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
                std::cerr << "RTTM: 结构体尚未注册: " << typeName << std::endl;
                return *this;
            }

            std::string signature = name + Object::GetTypeName<Args...>();

            auto func = Create_Ref_<FunctionWrapper<R, void*, Args...>>([memFunc](void* obj, Args... args)
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
                std::cerr << "RTTM: 结构体尚未注册: " << typeName << std::endl;
                return *this;
            }

            std::string signature = name + Object::GetTypeName<Args...>();

            auto func = Create_Ref_<FunctionWrapper<R, void*, Args...>>([memFunc](void* obj, Args... args)
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
                std::cerr << "RTTM: T 必须是枚举类型" << std::endl;
                return *this;
            }
            detail::Enums[Object::GetTypeName<T>()][name] = static_cast<int>(value);
            return *this;
        }

        T GetValue(const std::string& name)
        {
            if constexpr (!std::is_enum_v<T>)
            {
                std::cerr << "RTTM: T 必须是枚举类型" << std::endl;
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
        _Ref_<IFunctionWrapper> func;
        std::string name;
        void* instance;
        bool isMemberFunction;

        // 优化的调用缓存结构
        struct CallCache
        {
            std::vector<uint8_t> argsHash;
            T result;
            std::atomic<bool> valid{false}; // 原子操作避免竞争
        };

        mutable CallCache lastCall;

        template <typename... Args>
        std::vector<uint8_t> hashArgs(Args&&... args) const
        {
            std::vector<uint8_t> hash;
            if constexpr (std::conjunction_v<std::is_trivial<Args>...> && sizeof...(Args) > 0)
            {
                hash.reserve(sizeof...(Args) * 8); // 预分配空间
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
        Method(const _Ref_<IFunctionWrapper>& func, const std::string& name, void* instance = nullptr,
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
                throw std::runtime_error("无法调用无效方法: " + name);
            }

            // 缓存优化：仅对简单类型启用
            if constexpr (std::conjunction_v<std::is_trivial<Args>...> &&
                std::is_trivial_v<T> &&
                sizeof...(Args) > 0)
            {
                auto hash = hashArgs(args...);

                if (isMemberFunction)
                {
                    if (!instance)
                        throw std::runtime_error("成员函数实例为空: " + name);
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, void*, Args...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "函数签名不匹配: " + name + " 参数类型: " + Object::GetTypeName<Args...>());

                    T result = (*wrapper)(instance, std::forward<Args>(args)...);

                    lastCall.argsHash = std::move(hash);
                    lastCall.result = result;
                    lastCall.valid.store(true, std::memory_order_release);

                    return result;
                }
                else
                {
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, Args...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "函数签名不匹配: " + name + " 参数类型: " + Object::GetTypeName<Args...>());

                    T result = (*wrapper)(std::forward<Args>(args)...);

                    lastCall.argsHash = std::move(hash);
                    lastCall.result = result;
                    lastCall.valid.store(true, std::memory_order_release);

                    return result;
                }
            }
            else
            {
                // 直接调用，无缓存
                if (isMemberFunction)
                {
                    if (!instance)
                        throw std::runtime_error("成员函数实例为空: " + name);
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, void*, Args...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "函数签名不匹配: " + name + " 参数类型: " + Object::GetTypeName<Args...>());

                    return (*wrapper)(instance, std::forward<Args>(args)...);
                }
                else
                {
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, Args...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "函数签名不匹配: " + name + " 参数类型: " + Object::GetTypeName<Args...>());

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
            lastCall.valid.store(false, std::memory_order_release);
            return *this;
        }
    };

    class RType
    {
    private:
        // 工厂查找优化结构
        struct FactoryLookup
        {
            _Ref_<IFactory> factory;
            std::string signature;
            std::atomic<bool> valid{false};

            // 添加构造函数以支持正确的初始化
            FactoryLookup() = default;

            FactoryLookup(_Ref_<IFactory> f, const std::string& sig, bool v)
                : factory(f), signature(sig), valid(v)
            {
            }

            // 拷贝构造函数
            FactoryLookup(const FactoryLookup& other)
                : factory(other.factory), signature(other.signature), valid(other.valid.load())
            {
            }

            // 移动构造函数
            FactoryLookup(FactoryLookup&& other) noexcept
                : factory(std::move(other.factory)), signature(std::move(other.signature)), valid(other.valid.load())
            {
            }

            // 拷贝赋值运算符
            FactoryLookup& operator=(const FactoryLookup& other)
            {
                if (this != &other)
                {
                    factory = other.factory;
                    signature = other.signature;
                    valid.store(other.valid.load());
                }
                return *this;
            }

            // 移动赋值运算符
            FactoryLookup& operator=(FactoryLookup&& other) noexcept
            {
                if (this != &other)
                {
                    factory = std::move(other.factory);
                    signature = std::move(other.signature);
                    valid.store(other.valid.load());
                }
                return *this;
            }
        };

        // 参数哈希计算优化
        template <typename... Args>
        static constexpr size_t getArgsHash()
        {
            if constexpr (sizeof...(Args) == 0)
            {
                return 0; // 默认构造函数的特殊哈希值
            }
            else
            {
                return std::hash<std::string>{}(Object::GetTypeName<Args...>());
            }
        }

        // 快速工厂查找 - 核心优化点
        template <typename... Args>
        _Ref_<IFactory> findFactory() const
        {
            // 线程局部工厂缓存，避免全局锁竞争
            static thread_local std::unordered_map<std::string, std::pair<size_t, FactoryLookup>> factoryCache;
            static thread_local size_t cacheVersion = 0;

            constexpr size_t argsHash = getArgsHash<Args...>();
            std::string cacheKey = type + std::to_string(argsHash);

            // 检查缓存
            auto cacheIt = factoryCache.find(cacheKey);
            if (cacheIt != factoryCache.end() && cacheIt->second.second.valid.load(std::memory_order_acquire))
            {
                return cacheIt->second.second.factory;
            }

            // 特殊处理默认构造函数 - 最常用的情况
            if constexpr (sizeof...(Args) == 0)
            {
                if (defaultFactoryValid.load(std::memory_order_acquire) && defaultFactory)
                {
                    return defaultFactory;
                }
            }

            // 执行工厂查找
            std::string signature = Object::GetTypeName<Args...>();
            auto& factories = detail::TypesInfo[type].factories;
            auto factoryIt = factories.find(signature);

            if (factoryIt == factories.end())
            {
                // 缓存失败结果，避免重复查找
                FactoryLookup failedLookup = {nullptr, signature, false};
                factoryCache[cacheKey] = {cacheVersion, failedLookup};
                return nullptr;
            }

            // 缓存成功结果
            FactoryLookup successLookup = {factoryIt->second, signature, true};
            factoryCache[cacheKey] = {cacheVersion, successLookup};

            // 特殊处理默认构造函数
            if constexpr (sizeof...(Args) == 0)
            {
                defaultFactory = factoryIt->second;
                defaultFactoryValid.store(true, std::memory_order_release);
            }

            // 智能缓存管理：防止缓存无限增长
            if (factoryCache.size() > 1000)
            {
                ++cacheVersion;
                // 清理旧版本缓存条目
                auto it = factoryCache.begin();
                while (it != factoryCache.end())
                {
                    if (it->second.first < cacheVersion - 1)
                    {
                        it = factoryCache.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            return factoryIt->second;
        }

        // 有效性检查缓存
        template <typename R = bool>
        R checkValid() const
        {
            if (validityCacheValid.load(std::memory_order_acquire))
            {
                if (!lastValidityCheck.load(std::memory_order_acquire))
                {
                    throw std::runtime_error("RTTM: 对象状态无效");
                }
                return R(lastValidityCheck.load(std::memory_order_acquire));
            }

            bool isValid = valid && created;
            if (!isValid)
            {
                if (!valid)
                    throw std::runtime_error("RTTM: 结构未注册");
                if (!created)
                    throw std::runtime_error("RTTM: 对象未创建");
            }

            lastValidityCheck.store(isValid, std::memory_order_release);
            validityCacheValid.store(true, std::memory_order_release);

            return R(isValid);
        }

        void _copyFrom(const _Ref_<char>& newInst) const
        {
            if (!checkValid()) return;

            if (detail::TypesInfo[type].copier)
            {
                detail::TypesInfo[type].copier->operator()(newInst.get(), instance.get());
            }
            else
            {
                throw std::runtime_error("RTTM: 类型未注册复制构造函数: " + type);
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
            // 使用线程局部存储优化签名缓存
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
                std::string temp;
                temp.reserve(name.size() + 32);
                temp = name + Object::GetTypeName<Args...>();
                tn = std::move(temp);

                if (signatureCache.size() > 1000)
                {
                    signatureCache.clear();
                    signatureCache.reserve(500);
                }
                signatureCache[cacheKey] = tn;
            }

            if (!checkValid())
            {
                return Method<T>(nullptr, name, nullptr, false);
            }

            auto& typeInfo = detail::TypesInfo[type];
            _Ref_<IFunctionWrapper> funcWrapper = typeInfo.FindFunction(tn);

            if (!funcWrapper)
            {
                throw std::runtime_error("未找到函数: " + name);
            }

            auto _f = std::static_pointer_cast<FunctionWrapper<T, void*, Args...>>(funcWrapper);
            if (!_f)
            {
                throw std::runtime_error(
                    "函数签名不匹配: " + name + " 参数类型: " + Object::GetTypeName<Args...>());
            }

            return Method<T>(_f, name, static_cast<void*>(instance.get()), true);
        }

        void preloadAllProperties() const
        {
            if (!checkValid() || propertiesPreloaded.load(std::memory_order_acquire)) return;

            auto& typeInfo = detail::TypesInfo[type];
            membersCache.reserve(typeInfo.members.size());

            for (const auto& [name, memberInfo] : typeInfo.members)
            {
                if (membersCache.find(name) != membersCache.end()) continue;

                auto typeIt = typeInfo.membersType.find(name);
                if (typeIt == typeInfo.membersType.end()) continue;

                auto newStruct = CreateType(typeIt->second, memberInfo.type, memberInfo.typeIndex);
                newStruct->instance = AliasCreate(instance, instance.get() + memberInfo.offset);
                newStruct->valid = true;
                newStruct->created = true;
                membersCache[name] = newStruct;
            }

            propertiesPreloaded.store(true, std::memory_order_release);
        }

    private:
        // 原子变量优化，减少锁竞争
        mutable std::atomic<bool> lastValidityCheck{false};
        mutable std::atomic<bool> validityCacheValid{false};
        mutable std::atomic<bool> propertiesPreloaded{false};

        // 默认工厂缓存
        mutable _Ref_<IFactory> defaultFactory;
        mutable std::atomic<bool> defaultFactoryValid{false};

    protected:
        std::string type;
        RTTMTypeBits typeEnum;
        std::type_index typeIndex = std::type_index(typeid(void));
        mutable _Ref_<char> instance;
        mutable bool valid = false;
        mutable bool created = false;

        mutable std::unordered_set<std::string> membersName;
        mutable std::unordered_set<std::string> funcsName;
        mutable std::unordered_map<std::string, _Ref_<RType>> membersCache;

        // 修复 PropertyLookupCache 结构
        struct PropertyLookupCache
        {
            std::string lastPropertyName;
            size_t offset = 0;
            std::atomic<bool> valid{false};

            // 默认构造函数
            PropertyLookupCache() = default;

            // 自定义拷贝构造函数 - 处理原子变量
            PropertyLookupCache(const PropertyLookupCache& other)
                : lastPropertyName(other.lastPropertyName),
                  offset(other.offset),
                  valid(other.valid.load(std::memory_order_acquire)) // 从原子变量加载值
            {
            }

            // 自定义移动构造函数
            PropertyLookupCache(PropertyLookupCache&& other) noexcept
                : lastPropertyName(std::move(other.lastPropertyName)),
                  offset(other.offset),
                  valid(other.valid.load(std::memory_order_acquire))
            {
            }

            // 自定义拷贝赋值运算符
            PropertyLookupCache& operator=(const PropertyLookupCache& other)
            {
                if (this != &other)
                {
                    lastPropertyName = other.lastPropertyName;
                    offset = other.offset;
                    valid.store(other.valid.load(std::memory_order_acquire), std::memory_order_release);
                }
                return *this;
            }

            // 自定义移动赋值运算符
            PropertyLookupCache& operator=(PropertyLookupCache&& other) noexcept
            {
                if (this != &other)
                {
                    lastPropertyName = std::move(other.lastPropertyName);
                    offset = other.offset;
                    valid.store(other.valid.load(std::memory_order_acquire), std::memory_order_release);
                }
                return *this;
            }
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
                    throw std::runtime_error("RTTM: 类型未注册: " + type);
                    return;
                }

                auto& typeInfo = detail::TypesInfo[type];
                typeEnum = typeInfo.type;
                typeIndex = typeInfo.typeIndex;

                membersName.reserve(typeInfo.members.size());
                funcsName.reserve(typeInfo.functions.size());

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
              const _Ref_<char>& _instance = nullptr):
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
                    throw std::runtime_error("RTTM: 类型未注册: " + type);
                    valid = false;
                    return;
                }
            }

            if (valid)
            {
                if (membersName.empty())
                {
                    auto& members = detail::TypesInfo[type].members;
                    membersName.reserve(members.size());

                    for (const auto& [name, _] : members)
                    {
                        membersName.insert(name);
                    }
                }

                if (funcsName.empty())
                {
                    auto& functions = detail::TypesInfo[type].functions;
                    funcsName.reserve(functions.size());

                    for (const auto& [name, _] : functions)
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

        RType(const RType& rtype) noexcept:
            type(rtype.type),
            typeEnum(rtype.typeEnum),
            typeIndex(rtype.typeIndex),
            valid(rtype.valid),
            membersName(rtype.membersName),
            funcsName(rtype.funcsName),
            membersCache(rtype.membersCache),
            propCache(rtype.propCache),
            instance(nullptr),
            created(false)
        {
            // 原子变量需要显式初始化
            lastValidityCheck.store(rtype.lastValidityCheck.load(), std::memory_order_relaxed);
            validityCacheValid.store(rtype.validityCacheValid.load(), std::memory_order_relaxed);
            propertiesPreloaded.store(rtype.propertiesPreloaded.load(), std::memory_order_relaxed);
            defaultFactoryValid.store(false, std::memory_order_relaxed);
        }

        static _Ref_<RType> CreateType(const std::string& type, RTTMTypeBits typeEnum, const std::type_index& typeIndex,
                                       const std::unordered_set<std::string>& membersName = {},
                                       const std::unordered_set<std::string>& funcsName = {},
                                       const _Ref_<char>& instance = nullptr)
        {
            // 使用线程局部对象池
            static thread_local std::vector<_Ref_<RType>> typePool;
            static constexpr size_t MAX_POOL_SIZE = 128;

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
                rtype->propertiesPreloaded.store(false, std::memory_order_relaxed);
                rtype->validityCacheValid.store(false, std::memory_order_relaxed);
                rtype->propCache.valid.store(false, std::memory_order_relaxed);
                rtype->defaultFactoryValid.store(false, std::memory_order_relaxed);

                return rtype;
            }

            return Create_Ref_<RType>(type, typeEnum, typeIndex, membersName, funcsName, instance);
        }

        // 优化后的Create方法 - 核心性能改进
        template <typename... Args>
        bool Create(Args... args) const
        {
            if (!valid)
            {
                throw std::runtime_error("RTTM: 无效类型: " + type);
            }

            // 快速工厂查找
            auto factory = findFactory<Args...>();
            if (!factory)
            {
                throw std::runtime_error("RTTM: 未注册工厂: " + type + " 参数: " + Object::GetTypeName<Args...>());
            }

            // 通过工厂创建实例
            auto newInst = factory->Create(std::forward<Args>(args)...);
            if (!newInst)
            {
                throw std::runtime_error("RTTM: 创建实例失败: " + type);
            }

            // 处理实例赋值
            if (instance)
            {
                _copyFrom(newInst);
            }
            else
            {
                instance = newInst;
            }

            // 批量更新状态，减少原子操作
            created = true;
            validityCacheValid.store(true, std::memory_order_release);
            lastValidityCheck.store(true, std::memory_order_release);

            // 延迟清理缓存
            if (propertiesPreloaded.load(std::memory_order_acquire))
            {
                propertiesPreloaded.store(false, std::memory_order_release);
            }

            return true;
        }

        // 批量创建优化
        template <typename... Args>
        std::vector<_Ref_<RType>> CreateBatch(size_t count, Args... args) const
        {
            if (!valid)
            {
                throw std::runtime_error("RTTM: 无效类型: " + type);
            }

            std::vector<_Ref_<RType>> results;
            results.reserve(count);

            auto factory = findFactory<Args...>();
            if (!factory)
            {
                throw std::runtime_error("RTTM: 未注册工厂: " + type);
            }

            for (size_t i = 0; i < count; ++i)
            {
                auto newType = CreateType(type, typeEnum, typeIndex, membersName, funcsName);

                try
                {
                    auto newInst = factory->Create(args...);
                    if (!newInst)
                    {
                        throw std::runtime_error("RTTM: 批量创建失败");
                    }

                    newType->instance = newInst;
                    newType->created = true;
                    newType->valid = true;
                    newType->validityCacheValid.store(true, std::memory_order_relaxed);
                    newType->lastValidityCheck.store(true, std::memory_order_relaxed);

                    results.emplace_back(std::move(newType));
                }
                catch (...)
                {
                    results.clear();
                    throw;
                }
            }

            return results;
        }

        template <typename T>
        void Attach(T& inst) const
        {
#ifndef NDEBUG
            if (typeIndex != std::type_index(typeid(T)))
            {
                throw std::runtime_error("RTTM: Attach中类型不匹配");
            }
#endif

            instance = AliasCreate(instance, static_cast<char*>(reinterpret_cast<void*>(&inst)));
            membersCache.clear();
            created = true;
            valid = true;
            validityCacheValid.store(true, std::memory_order_release);
            lastValidityCheck.store(true, std::memory_order_release);
            propertiesPreloaded.store(false, std::memory_order_release);
            propCache.valid.store(false, std::memory_order_release);
        }

        template <typename R, typename... Args>
        R Invoke(const std::string& name, Args... args) const
        {
            static thread_local std::unordered_map<std::string, std::string> signatureCache;
            static constexpr size_t MAX_SIG_CACHE_SIZE = 500;

            std::string cacheKey = name + "|" + type;
            std::string tn;
            auto sigIt = signatureCache.find(cacheKey);
            if (sigIt != signatureCache.end())
            {
                tn = sigIt->second;
            }
            else
            {
                std::string temp;
                temp.reserve(name.size() + 32);
                temp = name + Object::GetTypeName<Args...>();
                tn = std::move(temp);

                if (signatureCache.size() > MAX_SIG_CACHE_SIZE)
                {
                    std::vector<std::string> keysToRemove;
                    keysToRemove.reserve(signatureCache.size() / 2);

                    size_t count = 0;
                    for (const auto& entry : signatureCache)
                    {
                        if (count++ % 2 == 0)
                            keysToRemove.push_back(entry.first);
                    }

                    for (const auto& key : keysToRemove)
                        signatureCache.erase(key);
                }
                signatureCache[cacheKey] = tn;
            }

            if (!checkValid<R>())
            {
                return R();
            }

            auto& typeInfo = detail::TypesInfo[type];
            _Ref_<IFunctionWrapper> funcWrapper = typeInfo.FindFunction(tn);

            if (!funcWrapper)
            {
                throw std::runtime_error("RTTM: 函数未找到: " + tn);
            }

            auto funcPtr = std::static_pointer_cast<FunctionWrapper<R, void*, Args...>>(funcWrapper);
            if (!funcPtr)
            {
                throw std::runtime_error("RTTM: 函数签名无效: " + tn);
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

            if (propCache.valid.load(std::memory_order_acquire) && propCache.lastPropertyName == name)
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
                    throw std::runtime_error("RTTM: 成员未找到: " + name);
                    return dummy;
                }

                offset = it->second.offset;

                propCache.lastPropertyName = name;
                propCache.offset = offset;
                propCache.valid.store(true, std::memory_order_release);
            }

            return *reinterpret_cast<T*>(instance.get() + offset);
        }

        _Ref_<RType> GetProperty(const std::string& name) const
        {
            if (!checkValid())
            {
                throw std::runtime_error("RTTM: 无效对象访问: " + type);
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
                throw std::runtime_error("RTTM: 成员未找到: " + name);
            }

            auto& typeInfo = detail::TypesInfo[type];
            auto typeIt = typeInfo.membersType.find(name);
            if (typeIt == typeInfo.membersType.end())
            {
                throw std::runtime_error("RTTM: 成员类型未找到: " + name);
            }

            auto newStruct = CreateType(typeIt->second, it->second.type, it->second.typeIndex);
            newStruct->instance = AliasCreate(instance, instance.get() + it->second.offset);
            newStruct->valid = true;
            newStruct->created = true;

            membersCache[name] = newStruct;
            return newStruct;
        }

        std::unordered_map<std::string, _Ref_<RType>> GetProperties() const
        {
            if (!checkValid())
            {
                return {};
            }

            if (!propertiesPreloaded.load(std::memory_order_acquire))
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
                throw std::runtime_error("RTTM: SetValue中类型不匹配");
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
                validityCacheValid.store(false, std::memory_order_release);
            }
        }

        bool IsClass() const { return typeEnum == RTTMTypeBits::Class; }
        bool IsEnum() const { return typeEnum == RTTMTypeBits::Enum; }
        bool IsVariable() const { return typeEnum == RTTMTypeBits::Variable; }

        bool IsValid() const
        {
            if (validityCacheValid.load(std::memory_order_acquire))
            {
                return lastValidityCheck.load(std::memory_order_acquire);
            }

            bool isValid = valid && created;
            lastValidityCheck.store(isValid, std::memory_order_release);
            validityCacheValid.store(true, std::memory_order_release);
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
                throw std::runtime_error("RTTM: 无效对象访问: " + type);
            }

#ifndef NDEBUG
            if (typeIndex != std::type_index(typeid(T)))
            {
                throw std::runtime_error("RTTM: As<T>中类型不匹配");
            }
#endif

            return *reinterpret_cast<T*>(instance.get());
        }

        static _Ref_<RType> Get(const std::string& typeName)
        {
            // 线程安全的类型缓存
            static std::unordered_map<std::string_view, RType> s_TypeCache;
            static std::mutex cacheMutex;

            if (detail::RegisteredTypes.count(typeName) == 0)
            {
                throw std::runtime_error("RTTM: 类型未注册: " + typeName);
            }

            std::string_view typeNameView(typeName.data(), typeName.size());

            std::lock_guard<std::mutex> lock(cacheMutex);
            auto it = s_TypeCache.find(typeNameView);

            if (it == s_TypeCache.end())
            {
                const auto& typeInfo = detail::TypesInfo[typeName];
                const size_t membersSize = typeInfo.members.size();
                const size_t functionsSize = typeInfo.functions.size();

                std::unordered_set<std::string> membersName;
                std::unordered_set<std::string> funcsName;
                membersName.reserve(membersSize);
                funcsName.reserve(functionsSize);

                for (const auto& [name, _] : typeInfo.members)
                {
                    membersName.emplace(name);
                }

                for (const auto& [name, _] : typeInfo.functions)
                {
                    size_t paramStart = name.find('(');
                    if (paramStart != std::string::npos)
                    {
                        funcsName.emplace(name.substr(0, paramStart));
                    }
                    else
                    {
                        funcsName.emplace(name);
                    }
                }

                RType newStruct(
                    typeName,
                    typeInfo.type,
                    typeInfo.typeIndex,
                    membersName,
                    funcsName,
                    nullptr
                );

                s_TypeCache.emplace(typeNameView, std::move(newStruct));
            }

            const RType& cachedType = s_TypeCache[typeNameView];
            return std::make_shared<RType>(cachedType);
        }

        // 获取泛型类型信息
        template <typename T>
        static _Ref_<RType> Get()
        {
            return Get(CachedTypeName<T>());
        }

        // 重置方法 - 清理资源和缓存
        void Reset() const
        {
            if (created && instance)
            {
                Destructor();
            }

            instance.reset();
            created = false;
            validityCacheValid.store(false, std::memory_order_release);
            propertiesPreloaded.store(false, std::memory_order_release);
            propCache.valid.store(false, std::memory_order_release);
            defaultFactoryValid.store(false, std::memory_order_release);
            membersCache.clear();
        }

        // 性能统计方法
        struct PerformanceStats
        {
            size_t createCount = 0;
            size_t cacheHits = 0;
            size_t cacheMisses = 0;
            size_t poolHits = 0;
        };

        PerformanceStats GetPerformanceStats() const
        {
            PerformanceStats stats;
            // 这里可以收集各种性能统计信息
            return stats;
        }

        // 析构函数
        ~RType()
        {
            if (valid && created && !type.empty())
            {
                Destructor();
            }
        }
    };

    // 全局函数管理类
    class Global
    {
        template <typename T, typename... Args>
        static Method<T> GetMethodOrig(const std::string& name)
        {
            if (detail::GFunctions.find(name) == detail::GFunctions.end())
            {
                throw std::runtime_error("RTTM: 函数未注册: " + name);
            }

            auto _f = std::static_pointer_cast<FunctionWrapper<T, Args...>>(detail::GFunctions[name]);
            if (!_f)
            {
                throw std::runtime_error(
                    "RTTM: 函数签名不匹配: " + name +
                    " 参数类型: " + Object::GetTypeName<Args...>() +
                    " 所需类型: " + detail::GFunctions[name]->argumentTypes);
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

        // 优化的函数存在性检查
        static bool FunctionExists(const std::string& name)
        {
            static thread_local std::unordered_map<std::string, bool> existenceCache;

            auto it = existenceCache.find(name);
            if (it != existenceCache.end())
            {
                return it->second;
            }

            bool exists = detail::GFunctions.find(name) != detail::GFunctions.end();

            // 控制缓存大小
            if (existenceCache.size() > 1000)
            {
                existenceCache.clear();
            }

            existenceCache[name] = exists;
            return exists;
        }

    public:
        // 注册全局变量
        template <typename T>
        static void RegisterVariable(const std::string& name, T value)
        {
            detail::Variables[name] = static_cast<_Ref_<void>>(Create_Ref_<T>(value));
        }

        // 获取全局变量
        template <typename T>
        static T& GetVariable(const std::string& name)
        {
            auto it = detail::Variables.find(name);
            if (it == detail::Variables.end())
            {
                throw std::runtime_error("RTTM: 变量未找到: " + name);
            }

            auto ptr = std::static_pointer_cast<T>(it->second);
            if (!ptr)
            {
                throw std::runtime_error("RTTM: 变量类型不匹配: " + name);
            }

            return *ptr;
        }

        // 注册全局方法 - 函数指针版本
        template <typename R, typename... Args>
        static void RegisterGlobalMethod(const std::string& name, R (*f)(Args...))
        {
            detail::GFunctions[name] = Create_Ref_<FunctionWrapper<R, Args...>>(f);
        }

        // 注册全局方法 - std::function版本
        template <typename R, typename... Args>
        static void RegisterGlobalMethod(const std::string& name, const std::function<R(Args...)>& func)
        {
            auto wrapper = Create_Ref_<FunctionWrapper<R, Args...>>(func);
            detail::GFunctions[name] = wrapper;
        }

        // 注册全局方法 - 泛型版本
        template <typename F>
        static void RegisterGlobalMethod(const std::string& name, F f)
        {
            std::function func = f;
            RegisterGlobalMethod(name, func);
        }

        // 调用全局方法
        template <typename T, typename... Args>
        static T InvokeG(const std::string& name, Args... args)
        {
            if (!FunctionExists(name))
            {
                throw std::runtime_error("RTTM: 函数未注册: " + name);
            }

            return GetMethodOrig<T, Args...>(name).Invoke(std::forward<Args>(args)...);
        }

        // 获取全局方法
        template <typename FuncType>
        static auto GetMethod(const std::string& name)
        {
            return GetMethodImpl<FuncType>(name);
        }

        // 清理所有全局资源
        static void Cleanup()
        {
            detail::Variables.clear();
            detail::GFunctions.clear();
            detail::Enums.clear();
            detail::TypesInfo.clear();
            detail::RegisteredTypes.clear();
        }

        // 获取全局统计信息
        static std::map<std::string, size_t> GetGlobalStats()
        {
            return {
                {"registered_types", detail::RegisteredTypes.size()},
                {"global_variables", detail::Variables.size()},
                {"global_functions", detail::GFunctions.size()},
                {"enum_types", detail::Enums.size()}
            };
        }
    };

    // 性能优化的工厂预热器
    class FactoryWarmer
    {
    public:
        // 预热指定类型的默认构造函数
        template <typename T>
        static void WarmupDefaultConstructor()
        {
            auto type = RType::Get<T>();
            try
            {
                type->Create(); // 预热默认构造函数
            }
            catch (...)
            {
                // 忽略预热过程中的异常
            }
        }

        // 预热指定类型的参数化构造函数
        template <typename T, typename... Args>
        static void WarmupConstructor(Args... args)
        {
            auto type = RType::Get<T>();
            try
            {
                type->Create(std::forward<Args>(args)...);
            }
            catch (...)
            {
                // 忽略预热过程中的异常
            }
        }

        // 批量预热多个类型
        template <typename... Types>
        static void WarmupTypes()
        {
            (WarmupDefaultConstructor<Types>(), ...);
        }

        static void WarmupAllRegisteredTypes()
        {
            for (const auto& [typeName, typeInfo] : detail::TypesInfo)
            {
                if (typeInfo.type == RTTMTypeBits::Variable || typeInfo.type == RTTMTypeBits::Class)
                {
                    RType::Get(typeName)->Create();
                }
            }
        }
    };
} // namespace RTTM

// 便利宏定义
#define CONCAT_IMPL(s1, s2) s1##s2
#define CONCAT(s1, s2) CONCAT_IMPL(s1, s2)

// RTTM注册宏 - 简化注册过程
#define RTTM_REGISTRATION                           \
namespace {                                               \
struct CONCAT(RegistrationHelper_, __LINE__) {          \
CONCAT(RegistrationHelper_, __LINE__)();           \
};}                                                    \
static CONCAT(RegistrationHelper_, __LINE__)           \
CONCAT(registrationHelperInstance_, __LINE__);    \
CONCAT(RegistrationHelper_, __LINE__)::CONCAT(RegistrationHelper_, __LINE__)()

// 性能优化宏 - 自动预热常用类型
#define RTTM_WARMUP_TYPE(Type) \
    static struct CONCAT(TypeWarmer_, __LINE__) { \
        CONCAT(TypeWarmer_, __LINE__)() { \
            RTTM::FactoryWarmer::WarmupDefaultConstructor<Type>(); \
        } \
    } CONCAT(typeWarmerInstance_, __LINE__)

// 批量预热宏
#define RTTM_WARMUP_TYPES(...) \
    static struct CONCAT(TypesWarmer_, __LINE__) { \
        CONCAT(TypesWarmer_, __LINE__)() { \
            RTTM::FactoryWarmer::WarmupTypes<__VA_ARGS__>(); \
        } \
    } CONCAT(typesWarmerInstance_, __LINE__)

#endif //RTTM_H
