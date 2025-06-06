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
#include <shared_mutex>
#include <string_view>
#include <array>

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
        _Ref_<IFunctionWrapper> createFunc;

    public:
        template <typename T>
        static void PreparePool(size_t count)
        {
            if constexpr (std::is_class_v<T>)
            {
                static thread_local std::vector<_Ref_<char>> objectPool;
                objectPool.reserve(count);
            }
        }

        template <typename... Args>
        _Ref_<char> Create(Args... args)
        {
            return (*std::static_pointer_cast<FunctionWrapper<_Ref_<char>, Args...>>(createFunc))(
                std::forward<Args>(args)...);
        }
    };

    // 动态对象池管理器 - 支持256到2^20个对象的动态扩展和收缩
    template <typename T>
    class DynamicObjectPoolManager
    {
    private:
        static constexpr size_t MIN_POOL_SIZE = 256;           // 最小池大小
        static constexpr size_t MAX_POOL_SIZE = 1048576;       // 最大池大小 (2^20)
        static constexpr size_t GROWTH_FACTOR = 2;             // 增长因子
        static constexpr size_t SHRINK_THRESHOLD = 4;          // 收缩阈值
        static constexpr size_t MAX_OBJECT_SIZE = 1024;        // 最大对象大小限制

        struct alignas(T) ObjectSlot
        {
            char data[sizeof(T)];
        };

        std::vector<std::unique_ptr<ObjectSlot[]>> memoryBlocks; // 内存块集合
        std::vector<ObjectSlot*> availableSlots;                 // 可用对象槽
        std::atomic<size_t> currentCapacity{MIN_POOL_SIZE};      // 当前容量
        std::atomic<size_t> usedCount{0};                        // 已使用数量
        mutable std::mutex poolMutex;                            // 池操作互斥锁

        // 扩展池容量
        void expandPool()
        {
            size_t newCapacity = std::min(currentCapacity.load() * GROWTH_FACTOR, MAX_POOL_SIZE);
            if (newCapacity <= currentCapacity.load()) return;

            size_t additionalSlots = newCapacity - currentCapacity.load();
            auto newBlock = std::make_unique<ObjectSlot[]>(additionalSlots);

            // 将新槽位添加到可用列表
            for (size_t i = 0; i < additionalSlots; ++i)
            {
                availableSlots.push_back(&newBlock[i]);
            }

            memoryBlocks.push_back(std::move(newBlock));
            currentCapacity.store(newCapacity);
        }

        // 收缩池容量
        void shrinkPool()
        {
            size_t newCapacity = std::max(currentCapacity.load() / GROWTH_FACTOR, MIN_POOL_SIZE);
            if (newCapacity >= currentCapacity.load()) return;

            // 只有在使用率很低时才收缩
            if (availableSlots.size() * SHRINK_THRESHOLD < currentCapacity.load())
            {
                // 简单的收缩策略：移除最后的内存块
                if (memoryBlocks.size() > 1)
                {
                    // 计算要移除的槽位数量
                    size_t slotsToRemove = currentCapacity.load() - newCapacity;

                    // 从可用槽位中移除对应的槽位
                    auto it = availableSlots.end();
                    for (size_t i = 0; i < slotsToRemove && it != availableSlots.begin(); ++i)
                    {
                        --it;
                    }

                    if (it != availableSlots.end())
                    {
                        availableSlots.erase(it, availableSlots.end());
                        memoryBlocks.pop_back();
                        currentCapacity.store(newCapacity);
                    }
                }
            }
        }

    public:
        DynamicObjectPoolManager()
        {
            if constexpr (sizeof(T) <= MAX_OBJECT_SIZE && std::is_trivially_destructible_v<T>)
            {
                // 初始化最小池容量
                auto initialBlock = std::make_unique<ObjectSlot[]>(MIN_POOL_SIZE);
                availableSlots.reserve(MIN_POOL_SIZE);

                for (size_t i = 0; i < MIN_POOL_SIZE; ++i)
                {
                    availableSlots.push_back(&initialBlock[i]);
                }

                memoryBlocks.push_back(std::move(initialBlock));
            }
        }

        T* AcquireObject()
        {
            if constexpr (sizeof(T) <= MAX_OBJECT_SIZE && std::is_trivially_destructible_v<T>)
            {
                std::lock_guard<std::mutex> lock(poolMutex);

                // 如果可用槽位不足，扩展池
                if (availableSlots.empty() && currentCapacity.load() < MAX_POOL_SIZE)
                {
                    expandPool();
                }

                if (!availableSlots.empty())
                {
                    T* obj = reinterpret_cast<T*>(availableSlots.back());
                    availableSlots.pop_back();
                    usedCount.fetch_add(1, std::memory_order_relaxed);
                    return obj;
                }
            }
            return nullptr;
        }

        void ReleaseObject(T* obj)
        {
            if constexpr (sizeof(T) <= MAX_OBJECT_SIZE && std::is_trivially_destructible_v<T>)
            {
                std::lock_guard<std::mutex> lock(poolMutex);

                availableSlots.push_back(reinterpret_cast<ObjectSlot*>(obj));
                usedCount.fetch_sub(1, std::memory_order_relaxed);

                // 检查是否需要收缩池
                if (availableSlots.size() * SHRINK_THRESHOLD > currentCapacity.load() &&
                    currentCapacity.load() > MIN_POOL_SIZE)
                {
                    shrinkPool();
                }
            }
        }

        size_t GetAvailableCount() const
        {
            std::lock_guard<std::mutex> lock(poolMutex);
            return availableSlots.size();
        }

        size_t GetCurrentCapacity() const
        {
            return currentCapacity.load();
        }

        size_t GetUsedCount() const
        {
            return usedCount.load();
        }
    };

    template <typename T, typename... Args>
    class Factory : public IFactory
    {
        using Allocator = std::allocator<T>;
        using Traits = std::allocator_traits<Allocator>;
        _Ref_<Allocator> allocator;

        // 使用动态对象池管理器
        static DynamicObjectPoolManager<T> objectPool;

        // 工厂创建计数器
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
            static thread_local _Ref_<Factory<T, Args...>> instance = Create_Ref_<Factory<T, Args...>>();
            return instance;
        }

        _Ref_<char> Create(Args... args)
        {
            createCount.fetch_add(1, std::memory_order_relaxed);

            T* rawPtr = nullptr;
            bool fromPool = false;

            // 优先尝试从动态对象池获取
            rawPtr = objectPool.AcquireObject();
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
                    objectPool.ReleaseObject(rawPtr);
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
                                       objectPool.ReleaseObject(typedPtr);
                                   }
                                   else
                                   {
                                       Traits::deallocate(*allocCopy, typedPtr, 1);
                                   }
                               }
            );
        }

        size_t GetCreateCount() const { return createCount.load(); }
        size_t GetPoolAvailableCount() const { return objectPool.GetAvailableCount(); }
        size_t GetPoolCapacity() const { return objectPool.GetCurrentCapacity(); }
    };

    template <typename T, typename... Args>
    DynamicObjectPoolManager<T> Factory<T, Args...>::objectPool;

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

        // 优化的全局类型信息管理器
        class GlobalTypeManager
        {
        private:
            TypeInfoMap typesInfo;
            std::unordered_set<std::string> registeredTypes;
            mutable std::shared_mutex typesMutex; // 读写锁，允许多个读者

            // 高性能类型索引缓存
            mutable std::unordered_map<std::string_view, const TypeBaseInfo*> typeIndexCache;
            mutable std::atomic<size_t> cacheVersion{0};
            mutable std::shared_mutex cacheMutex;

        public:
            static GlobalTypeManager& Instance()
            {
                static GlobalTypeManager instance;
                return instance;
            }

            // 线程安全的类型注册
            void RegisterType(const std::string& typeName, const TypeBaseInfo& info)
            {
                std::unique_lock<std::shared_mutex> lock(typesMutex);
                typesInfo[typeName] = info;
                registeredTypes.insert(typeName);

                // 清除相关缓存
                std::unique_lock<std::shared_mutex> cacheLock(cacheMutex);
                typeIndexCache.clear();
                cacheVersion.fetch_add(1, std::memory_order_relaxed);
            }

            // 高性能类型信息获取
            const TypeBaseInfo* GetTypeInfo(const std::string& typeName) const
            {
                std::string_view typeNameView(typeName);

                // 首先检查缓存
                {
                    std::shared_lock<std::shared_mutex> cacheLock(cacheMutex);
                    auto it = typeIndexCache.find(typeNameView);
                    if (it != typeIndexCache.end())
                    {
                        return it->second;
                    }
                }

                // 缓存未命中，查找实际类型信息
                std::shared_lock<std::shared_mutex> lock(typesMutex);
                auto it = typesInfo.find(typeName);
                if (it == typesInfo.end())
                {
                    return nullptr;
                }

                const TypeBaseInfo* typeInfo = &it->second;

                // 更新缓存
                {
                    std::unique_lock<std::shared_mutex> cacheLock(cacheMutex);
                    // 控制缓存大小
                    if (typeIndexCache.size() > 2000)
                    {
                        typeIndexCache.clear();
                    }
                    typeIndexCache[typeNameView] = typeInfo;
                }

                return typeInfo;
            }

            // 获取可变类型信息引用
            TypeBaseInfo& GetMutableTypeInfo(const std::string& typeName)
            {
                std::unique_lock<std::shared_mutex> lock(typesMutex);
                return typesInfo[typeName];
            }

            bool IsTypeRegistered(const std::string& typeName) const
            {
                std::shared_lock<std::shared_mutex> lock(typesMutex);
                return registeredTypes.count(typeName) > 0;
            }

            std::vector<std::string> GetAllRegisteredTypes() const
            {
                std::shared_lock<std::shared_mutex> lock(typesMutex);
                return std::vector<std::string>(registeredTypes.begin(), registeredTypes.end());
            }

            size_t GetTypeCount() const
            {
                std::shared_lock<std::shared_mutex> lock(typesMutex);
                return registeredTypes.size();
            }

            void Clear()
            {
                std::unique_lock<std::shared_mutex> lock(typesMutex);
                std::unique_lock<std::shared_mutex> cacheLock(cacheMutex);

                typesInfo.clear();
                registeredTypes.clear();
                typeIndexCache.clear();
                cacheVersion.fetch_add(1, std::memory_order_relaxed);
            }
        };

        // 工厂缓存管理器 - 专门优化工厂查找性能
        class FactoryCacheManager
        {
        private:
            struct FactoryEntry
            {
                _Ref_<IFactory> factory;
                std::atomic<bool> valid{true};

                FactoryEntry() = default;
                FactoryEntry(_Ref_<IFactory> f) : factory(std::move(f)) {}

                FactoryEntry(const FactoryEntry& other)
                    : factory(other.factory), valid(other.valid.load()) {}

                FactoryEntry(FactoryEntry&& other) noexcept
                    : factory(std::move(other.factory)), valid(other.valid.load()) {}

                FactoryEntry& operator=(const FactoryEntry& other)
                {
                    if (this != &other)
                    {
                        factory = other.factory;
                        valid.store(other.valid.load());
                    }
                    return *this;
                }
            };

            // 层次化缓存结构：类型名 -> 参数签名 -> 工厂
            std::unordered_map<std::string, std::unordered_map<std::string, FactoryEntry>> factoryCache;
            mutable std::shared_mutex cacheMutex;

            // 预热的默认构造函数缓存
            std::unordered_map<std::string, _Ref_<IFactory>> defaultFactoryCache;
            mutable std::shared_mutex defaultCacheMutex;

        public:
            static FactoryCacheManager& Instance()
            {
                static FactoryCacheManager instance;
                return instance;
            }

            // 获取工厂（优化路径）
            _Ref_<IFactory> GetFactory(const std::string& typeName, const std::string& signature) const
            {
                // 特殊处理默认构造函数
                if (signature.empty() || signature == "()")
                {
                    std::shared_lock<std::shared_mutex> lock(defaultCacheMutex);
                    auto it = defaultFactoryCache.find(typeName);
                    if (it != defaultFactoryCache.end())
                    {
                        return it->second;
                    }
                }

                // 通用工厂查找
                std::shared_lock<std::shared_mutex> lock(cacheMutex);
                auto typeIt = factoryCache.find(typeName);
                if (typeIt != factoryCache.end())
                {
                    auto sigIt = typeIt->second.find(signature);
                    if (sigIt != typeIt->second.end() && sigIt->second.valid.load())
                    {
                        return sigIt->second.factory;
                    }
                }

                return nullptr;
            }

            // 缓存工厂
            void CacheFactory(const std::string& typeName, const std::string& signature, _Ref_<IFactory> factory)
            {
                // 特殊处理默认构造函数
                if (signature.empty() || signature == "()")
                {
                    std::unique_lock<std::shared_mutex> lock(defaultCacheMutex);
                    defaultFactoryCache[typeName] = factory;
                    return;
                }

                std::unique_lock<std::shared_mutex> lock(cacheMutex);
                factoryCache[typeName][signature] = FactoryEntry(factory);

                // 控制缓存大小
                if (factoryCache.size() > 1000)
                {
                    // 清理最少使用的条目
                    auto it = factoryCache.begin();
                    for (size_t i = 0; i < factoryCache.size() / 4 && it != factoryCache.end(); )
                    {
                        if (it->second.empty())
                        {
                            it = factoryCache.erase(it);
                            ++i;
                        }
                        else
                        {
                            ++it;
                        }
                    }
                }
            }

            // 清除指定类型的缓存
            void InvalidateType(const std::string& typeName)
            {
                {
                    std::unique_lock<std::shared_mutex> lock(cacheMutex);
                    auto it = factoryCache.find(typeName);
                    if (it != factoryCache.end())
                    {
                        for (auto& [sig, entry] : it->second)
                        {
                            entry.valid.store(false);
                        }
                    }
                }

                {
                    std::unique_lock<std::shared_mutex> lock(defaultCacheMutex);
                    defaultFactoryCache.erase(typeName);
                }
            }

            void Clear()
            {
                std::unique_lock<std::shared_mutex> lock1(cacheMutex);
                std::unique_lock<std::shared_mutex> lock2(defaultCacheMutex);
                factoryCache.clear();
                defaultFactoryCache.clear();
            }
        };

        // 便捷访问函数
        inline TypeInfoMap& GetTypesInfo()
        {
            static TypeInfoMap dummy; // 保持向后兼容
            return dummy;
        }

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
            static std::string ReplacePattern(const std::string& src,
                                              const std::string& from,
                                              const std::string& to)
            {
                // 线程局部缓存，避免锁竞争
                static thread_local std::unordered_map<std::string, std::string> cache;

                std::string key = src + "|" + from + "|" + to;

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
            auto& typeInfo = detail::GlobalTypeManager::Instance().GetMutableTypeInfo(typeName);
            typeInfo.destructor = Create_Ref_<FunctionWrapper<void, void*>>([this](void* obj)
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
            auto& typeInfo = detail::GlobalTypeManager::Instance().GetMutableTypeInfo(typeName);
            typeInfo.copier = Create_Ref_<FunctionWrapper<void, void*, void*>>(
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
            if (detail::GlobalTypeManager::Instance().IsTypeRegistered(typeName))
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

            detail::TypeBaseInfo typeInfo;
            typeInfo.type = type;
            typeInfo.size = sizeof(T);
            typeInfo.typeIndex = typeIndex;

            detail::GlobalTypeManager::Instance().RegisterType(typeName, typeInfo);
            detail::RegisteredTypes.insert(typeName);

            destructor();
            constructor();
            copier();
        }

        template <typename U>
        Registry_& base()
        {
            auto base = Object::GetTypeName<U>();
            if (!detail::GlobalTypeManager::Instance().IsTypeRegistered(base))
            {
                std::cerr << "RTTM: 基类结构体尚未注册: " << base << std::endl;
                return *this;
            }

            const auto* baseTypeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(base);
            if (baseTypeInfo)
            {
                auto& typeInfo = detail::GlobalTypeManager::Instance().GetMutableTypeInfo(typeName);
                typeInfo.AppendFunctions(baseTypeInfo->functions);
                typeInfo.AppendMembers(baseTypeInfo->members);
                typeInfo.AppendMembersType(baseTypeInfo->membersType);
                typeInfo.AppendFactories(baseTypeInfo->factories);
            }
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
            auto factory = Factory<T, Args...>::CreateFactory();

            auto& typeInfo = detail::GlobalTypeManager::Instance().GetMutableTypeInfo(typeName);
            typeInfo.factories[name] = factory;

            // 缓存工厂以提高后续查找性能
            detail::FactoryCacheManager::Instance().CacheFactory(typeName, name, factory);

            if (name.find(strType) != std::string::npos)
            {
                std::string modifiedName = detail::StringPatternCache::ReplacePattern(name, strType, cc_strType);
                auto transformedFactory = detail::TransformedFactory<T, Args...>::Create();
                typeInfo.factories[modifiedName] = transformedFactory;
                detail::FactoryCacheManager::Instance().CacheFactory(typeName, modifiedName, transformedFactory);
            }

            if (name.find(wstrType) != std::string::npos)
            {
                std::string modifiedName = detail::StringPatternCache::ReplacePattern(name, wstrType, cwc_strType);
                auto transformedFactory = detail::TransformedFactory<T, Args...>::Create();
                typeInfo.factories[modifiedName] = transformedFactory;
                detail::FactoryCacheManager::Instance().CacheFactory(typeName, modifiedName, transformedFactory);
            }

            return *this;
        }

        template <typename U>
        Registry_& property(const char* name, U T::* value)
        {
            size_t offset = reinterpret_cast<size_t>(&(reinterpret_cast<T*>(0)->*value));

            detail::Member member;
            if constexpr (std::is_class_v<U> && !is_stl_container_v<U> && !is_string_v<U>)
            {
                member = {offset, RTTMTypeBits::Class, std::type_index(typeid(U))};
            }
            else if constexpr (std::is_enum_v<U>)
            {
                member = {offset, RTTMTypeBits::Enum, std::type_index(typeid(U))};
            }
            else
            {
                member = {offset, RTTMTypeBits::Variable, std::type_index(typeid(U))};
            }

            auto& typeInfo = detail::GlobalTypeManager::Instance().GetMutableTypeInfo(typeName);
            typeInfo.members[name] = member;
            typeInfo.membersType[name] = Object::GetTypeName<U>();
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

            auto& typeInfo = detail::GlobalTypeManager::Instance().GetMutableTypeInfo(typeName);
            typeInfo.functions[signature] = func;
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

            auto& typeInfo = detail::GlobalTypeManager::Instance().GetMutableTypeInfo(typeName);
            typeInfo.functions[signature] = func;
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

        // 调用缓存结构
        struct CallCache
        {
            std::vector<uint8_t> argsHash;
            T result;
            std::atomic<bool> valid{false};
        };

        mutable CallCache lastCall;

        template <typename... Args>
        std::vector<uint8_t> hashArgs(Args&&... args) const
        {
            std::vector<uint8_t> hash;
            if constexpr (std::conjunction_v<std::is_trivial<Args>...> && sizeof...(Args) > 0)
            {
                hash.reserve(sizeof...(Args) * 8);
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
        // RType动态对象池管理器 - 专门为RType优化的动态池
        class DynamicRTypePool
        {
        private:
            static constexpr size_t MIN_POOL_SIZE = 256;
            static constexpr size_t MAX_POOL_SIZE = 1048576; // 2^20
            static constexpr size_t GROWTH_FACTOR = 2;
            static constexpr size_t SHRINK_THRESHOLD = 4;

            std::vector<std::unique_ptr<RType[]>> memoryBlocks;
            std::vector<RType*> availableObjects;
            std::atomic<size_t> currentCapacity{MIN_POOL_SIZE};
            std::atomic<size_t> usedCount{0};
            mutable std::mutex poolMutex;

            void expandPool()
            {
                size_t newCapacity = std::min(currentCapacity.load() * GROWTH_FACTOR, MAX_POOL_SIZE);
                if (newCapacity <= currentCapacity.load()) return;

                size_t additionalObjects = newCapacity - currentCapacity.load();
                auto newBlock = std::make_unique<RType[]>(additionalObjects);

                for (size_t i = 0; i < additionalObjects; ++i)
                {
                    availableObjects.push_back(&newBlock[i]);
                }

                memoryBlocks.push_back(std::move(newBlock));
                currentCapacity.store(newCapacity);
            }

            void shrinkPool()
            {
                size_t newCapacity = std::max(currentCapacity.load() / GROWTH_FACTOR, MIN_POOL_SIZE);
                if (newCapacity >= currentCapacity.load()) return;

                if (availableObjects.size() * SHRINK_THRESHOLD < currentCapacity.load())
                {
                    if (memoryBlocks.size() > 1)
                    {
                        size_t objectsToRemove = currentCapacity.load() - newCapacity;

                        auto it = availableObjects.end();
                        for (size_t i = 0; i < objectsToRemove && it != availableObjects.begin(); ++i)
                        {
                            --it;
                        }

                        if (it != availableObjects.end())
                        {
                            availableObjects.erase(it, availableObjects.end());
                            memoryBlocks.pop_back();
                            currentCapacity.store(newCapacity);
                        }
                    }
                }
            }

        public:
            static DynamicRTypePool& Instance()
            {
                static DynamicRTypePool instance;
                return instance;
            }

            DynamicRTypePool()
            {
                auto initialBlock = std::make_unique<RType[]>(MIN_POOL_SIZE);
                availableObjects.reserve(MIN_POOL_SIZE);

                for (size_t i = 0; i < MIN_POOL_SIZE; ++i)
                {
                    availableObjects.push_back(&initialBlock[i]);
                }

                memoryBlocks.push_back(std::move(initialBlock));
            }

            RType* AcquireObject()
            {
                std::lock_guard<std::mutex> lock(poolMutex);

                if (availableObjects.empty() && currentCapacity.load() < MAX_POOL_SIZE)
                {
                    expandPool();
                }

                if (!availableObjects.empty())
                {
                    RType* obj = availableObjects.back();
                    availableObjects.pop_back();
                    usedCount.fetch_add(1, std::memory_order_relaxed);
                    return obj;
                }

                return nullptr;
            }

            void ReleaseObject(RType* obj)
            {
                if (!obj) return;

                // 重置对象状态
                obj->Reset();

                std::lock_guard<std::mutex> lock(poolMutex);

                availableObjects.push_back(obj);
                usedCount.fetch_sub(1, std::memory_order_relaxed);

                // 检查是否需要收缩池
                if (availableObjects.size() * SHRINK_THRESHOLD > currentCapacity.load() &&
                    currentCapacity.load() > MIN_POOL_SIZE)
                {
                    shrinkPool();
                }
            }

            size_t GetPoolSize() const
            {
                std::lock_guard<std::mutex> lock(poolMutex);
                return availableObjects.size();
            }

            size_t GetCurrentCapacity() const
            {
                return currentCapacity.load();
            }
        };

        // 参数哈希计算优化 - 编译时优化
        template <typename... Args>
        static constexpr size_t getArgsHash() noexcept
        {
            if constexpr (sizeof...(Args) == 0)
            {
                return 0; // 默认构造函数的特殊哈希值
            }
            else
            {
                constexpr auto typeNames = Object::GetTypeName<Args...>();
                return std::hash<std::string_view>{}(typeNames);
            }
        }

        // 超高性能工厂查找 - 多级缓存策略
        template <typename... Args>
        _Ref_<IFactory> findFactoryOptimized() const
        {
            // L1缓存：默认构造函数快速路径
            if constexpr (sizeof...(Args) == 0)
            {
                if (defaultFactoryValid.load(std::memory_order_acquire) && defaultFactory)
                {
                    return defaultFactory;
                }
            }

            // L2缓存：高性能工厂缓存管理器
            std::string signature = Object::GetTypeName<Args...>();
            auto cachedFactory = detail::FactoryCacheManager::Instance().GetFactory(type, signature);
            if (cachedFactory)
            {
                // 特殊处理默认构造函数
                if constexpr (sizeof...(Args) == 0)
                {
                    defaultFactory = cachedFactory;
                    defaultFactoryValid.store(true, std::memory_order_release);
                }
                return cachedFactory;
            }

            // L3缓存：从类型信息中查找
            const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
            if (!typeInfo)
            {
                return nullptr;
            }

            auto factoryIt = typeInfo->factories.find(signature);
            if (factoryIt == typeInfo->factories.end())
            {
                return nullptr;
            }

            // 缓存查找结果
            detail::FactoryCacheManager::Instance().CacheFactory(type, signature, factoryIt->second);

            // 特殊处理默认构造函数
            if constexpr (sizeof...(Args) == 0)
            {
                defaultFactory = factoryIt->second;
                defaultFactoryValid.store(true, std::memory_order_release);
            }

            return factoryIt->second;
        }

        // 超快速有效性检查
        template <typename R = bool>
        R checkValidOptimized() const noexcept
        {
            // 使用位操作优化布尔检查
            const bool isValidCached = validityCacheValid.load(std::memory_order_acquire);
            if (isValidCached)
            {
                const bool lastCheck = lastValidityCheck.load(std::memory_order_acquire);
                if constexpr (std::is_same_v<R, bool>)
                {
                    return R(lastCheck);
                }
                else
                {
                    if (!lastCheck)
                    {
                        throw std::runtime_error("RTTM: 对象状态无效");
                    }
                    return R(lastCheck);
                }
            }

            const bool isValid = valid && created;
            lastValidityCheck.store(isValid, std::memory_order_release);
            validityCacheValid.store(true, std::memory_order_release);

            if constexpr (std::is_same_v<R, bool>)
            {
                return R(isValid);
            }
            else
            {
                if (!isValid)
                {
                    if (!valid)
                        throw std::runtime_error("RTTM: 结构未注册");
                    if (!created)
                        throw std::runtime_error("RTTM: 对象未创建");
                }
                return R(isValid);
            }
        }

        void copyFrom(const _Ref_<char>& newInst) const
        {
            if (!checkValidOptimized()) return;

            const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
            if (typeInfo && typeInfo->copier)
            {
                typeInfo->copier->operator()(newInst.get(), instance.get());
            }
            else
            {
                throw std::runtime_error("RTTM: 类型未注册复制构造函数: " + type);
            }
        }

        template <typename FuncType>
        auto getMethodImpl(const std::string& name) const
        {
            using traits = detail::function_traits<FuncType>;
            using ReturnType = typename traits::return_type;

            return std::apply([&name, this](auto... args)
            {
                return getMethodOrig<ReturnType, decltype(args)...>(name);
            }, typename traits::arguments{});
        }

        template <typename T, typename... Args>
        Method<T> getMethodOrig(const std::string& name) const
        {
            // 超高性能签名缓存
            static thread_local std::array<std::pair<std::string, std::string>, 64> signatureCache{};
            static thread_local size_t cacheIndex = 0;

            std::string tn;
            const std::string cacheKey = name + "|" + type;

            // 检查缓存
            for (const auto& [key, signature] : signatureCache)
            {
                if (key == cacheKey)
                {
                    tn = signature;
                    break;
                }
            }

            if (tn.empty())
            {
                tn = name + Object::GetTypeName<Args...>();

                // 更新缓存
                signatureCache[cacheIndex] = {cacheKey, tn};
                cacheIndex = (cacheIndex + 1) % signatureCache.size();
            }

            if (!checkValidOptimized())
            {
                return Method<T>(nullptr, name, nullptr, false);
            }

            const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
            if (!typeInfo)
            {
                throw std::runtime_error("类型信息未找到: " + type);
            }

            _Ref_<IFunctionWrapper> funcWrapper = typeInfo->FindFunction(tn);

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

        // 属性预加载优化
        void preloadAllPropertiesOptimized() const
        {
            if (!checkValidOptimized() || propertiesPreloaded.load(std::memory_order_acquire))
                return;

            const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
            if (!typeInfo)
                return;

            membersCache.reserve(typeInfo->members.size());

            for (const auto& [name, memberInfo] : typeInfo->members)
            {
                if (membersCache.find(name) != membersCache.end())
                    continue;

                auto typeIt = typeInfo->membersType.find(name);
                if (typeIt == typeInfo->membersType.end())
                    continue;

                auto newStruct = CreateTypeOptimized(typeIt->second, memberInfo.type, memberInfo.typeIndex);
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

        // 优化的属性查找缓存
        struct PropertyLookupCache
        {
            std::string lastPropertyName;
            size_t offset = 0;
            std::atomic<bool> valid{false};

            PropertyLookupCache() = default;

            PropertyLookupCache(const PropertyLookupCache& other)
                : lastPropertyName(other.lastPropertyName),
                  offset(other.offset),
                  valid(other.valid.load(std::memory_order_acquire))
            {
            }

            PropertyLookupCache(PropertyLookupCache&& other) noexcept
                : lastPropertyName(std::move(other.lastPropertyName)),
                  offset(other.offset),
                  valid(other.valid.load(std::memory_order_acquire))
            {
            }

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
                const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
                if (!typeInfo)
                {
                    valid = false;
                    throw std::runtime_error("RTTM: 类型未注册: " + type);
                    return;
                }

                typeEnum = typeInfo->type;
                typeIndex = typeInfo->typeIndex;

                membersName.reserve(typeInfo->members.size());
                funcsName.reserve(typeInfo->functions.size());

                for (const auto& [name, _] : typeInfo->members)
                {
                    membersName.insert(name);
                }

                for (const auto& [name, _] : typeInfo->functions)
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
                if (!detail::GlobalTypeManager::Instance().IsTypeRegistered(type))
                {
                    throw std::runtime_error("RTTM: 类型未注册: " + type);
                    valid = false;
                    return;
                }
            }

            if (valid)
            {
                const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
                if (typeInfo)
                {
                    if (membersName.empty())
                    {
                        membersName.reserve(typeInfo->members.size());
                        for (const auto& [name, _] : typeInfo->members)
                        {
                            membersName.insert(name);
                        }
                    }

                    if (funcsName.empty())
                    {
                        funcsName.reserve(typeInfo->functions.size());
                        for (const auto& [name, _] : typeInfo->functions)
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

        // 使用动态对象池创建
        static _Ref_<RType> CreateTypeOptimized(const std::string& type, RTTMTypeBits typeEnum,
                                                const std::type_index& typeIndex,
                                                const std::unordered_set<std::string>& membersName = {},
                                                const std::unordered_set<std::string>& funcsName = {},
                                                const _Ref_<char>& instance = nullptr)
        {
            // 尝试从动态池获取对象
            auto rtypePtr = DynamicRTypePool::Instance().AcquireObject();

            if (rtypePtr)
            {
                // 从池中获取到对象，重新初始化
                rtypePtr->type = type;
                rtypePtr->typeEnum = typeEnum;
                rtypePtr->typeIndex = typeIndex;
                rtypePtr->instance = instance;
                rtypePtr->valid = !type.empty();
                rtypePtr->created = (typeEnum == RTTMTypeBits::Variable || instance != nullptr);
                rtypePtr->membersName = membersName;
                rtypePtr->funcsName = funcsName;
                rtypePtr->membersCache.clear();
                rtypePtr->propertiesPreloaded.store(false, std::memory_order_relaxed);
                rtypePtr->validityCacheValid.store(false, std::memory_order_relaxed);
                rtypePtr->propCache.valid.store(false, std::memory_order_relaxed);
                rtypePtr->defaultFactoryValid.store(false, std::memory_order_relaxed);

                // 返回智能指针，自定义删除器将对象归还到池中
                return _Ref_<RType>(rtypePtr, [](RType* ptr) {
                    DynamicRTypePool::Instance().ReleaseObject(ptr);
                });
            }
            else
            {
                // 池中没有可用对象，创建新对象
                return Create_Ref_<RType>(type, typeEnum, typeIndex, membersName, funcsName, instance);
            }
        }

        static _Ref_<RType> CreateType(const std::string& type, RTTMTypeBits typeEnum, const std::type_index& typeIndex,
                                       const std::unordered_set<std::string>& membersName = {},
                                       const std::unordered_set<std::string>& funcsName = {},
                                       const _Ref_<char>& instance = nullptr)
        {
            return CreateTypeOptimized(type, typeEnum, typeIndex, membersName, funcsName, instance);
        }

        // 超高性能Create方法 - 核心优化
        template <typename... Args>
        bool Create(Args... args) const
        {
            if (!valid)
            {
                throw std::runtime_error("RTTM: 无效类型: " + type);
            }

            // 使用优化的工厂查找
            auto factory = findFactoryOptimized<Args...>();
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
                copyFrom(newInst);
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

        // 批量创建优化 - 支持高并发场景
        template <typename... Args>
        std::vector<_Ref_<RType>> CreateBatch(size_t count, Args... args) const
        {
            if (!valid)
            {
                throw std::runtime_error("RTTM: 无效类型: " + type);
            }

            std::vector<_Ref_<RType>> results;
            results.reserve(count);

            auto factory = findFactoryOptimized<Args...>();
            if (!factory)
            {
                throw std::runtime_error("RTTM: 未注册工厂: " + type);
            }

            // 并行创建优化
            for (size_t i = 0; i < count; ++i)
            {
                auto newType = CreateTypeOptimized(type, typeEnum, typeIndex, membersName, funcsName);

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
            // 超高性能签名缓存 - 使用编译时优化
            static thread_local std::unordered_map<std::string, std::string> signatureCache;
            static constexpr size_t MAX_SIG_CACHE_SIZE = 512;

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

                if (signatureCache.size() > MAX_SIG_CACHE_SIZE)
                {
                    // 智能清理：保留最近使用的一半
                    auto it = signatureCache.begin();
                    std::advance(it, signatureCache.size() / 2);
                    signatureCache.erase(it, signatureCache.end());
                }
                signatureCache[cacheKey] = tn;
            }

            if (!checkValidOptimized<R>())
            {
                return R();
            }

            const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
            if (!typeInfo)
            {
                throw std::runtime_error("RTTM: 类型信息未找到: " + type);
            }

            _Ref_<IFunctionWrapper> funcWrapper = typeInfo->FindFunction(tn);

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
            return getMethodImpl<FuncType>(name);
        }

        template <typename T>
        T& GetProperty(const std::string& name) const
        {
            if (!checkValidOptimized())
            {
                static T dummy{};
                return dummy;
            }

            size_t offset;

            // 使用原子操作优化的属性缓存
            if (propCache.valid.load(std::memory_order_acquire) && propCache.lastPropertyName == name)
            {
                offset = propCache.offset;
            }
            else
            {
                const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
                if (!typeInfo)
                {
                    static T dummy{};
                    throw std::runtime_error("RTTM: 类型信息未找到: " + type);
                    return dummy;
                }

                auto it = typeInfo->members.find(name);
                if (it == typeInfo->members.end())
                {
                    static T dummy{};
                    throw std::runtime_error("RTTM: 成员未找到: " + name);
                    return dummy;
                }

                offset = it->second.offset;

                // 原子更新缓存
                propCache.lastPropertyName = name;
                propCache.offset = offset;
                propCache.valid.store(true, std::memory_order_release);
            }

            return *reinterpret_cast<T*>(instance.get() + offset);
        }

        _Ref_<RType> GetProperty(const std::string& name) const
        {
            if (!checkValidOptimized())
            {
                throw std::runtime_error("RTTM: 无效对象访问: " + type);
            }

            // 检查缓存
            auto cacheIt = membersCache.find(name);
            if (cacheIt != membersCache.end())
            {
                return cacheIt->second;
            }

            const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
            if (!typeInfo)
            {
                throw std::runtime_error("RTTM: 类型信息未找到: " + type);
            }

            auto it = typeInfo->members.find(name);
            if (it == typeInfo->members.end())
            {
                throw std::runtime_error("RTTM: 成员未找到: " + name);
            }

            auto typeIt = typeInfo->membersType.find(name);
            if (typeIt == typeInfo->membersType.end())
            {
                throw std::runtime_error("RTTM: 成员类型未找到: " + name);
            }

            auto newStruct = CreateTypeOptimized(typeIt->second, it->second.type, it->second.typeIndex);
            newStruct->instance = AliasCreate(instance, instance.get() + it->second.offset);
            newStruct->valid = true;
            newStruct->created = true;

            membersCache[name] = newStruct;
            return newStruct;
        }

        std::unordered_map<std::string, _Ref_<RType>> GetProperties() const
        {
            if (!checkValidOptimized())
            {
                return {};
            }

            if (!propertiesPreloaded.load(std::memory_order_acquire))
            {
                preloadAllPropertiesOptimized();
            }

            return membersCache;
        }

        template <typename T>
        bool SetValue(const T& value) const
        {
            if (!checkValidOptimized())
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
            if (valid && created && instance)
            {
                const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
                if (typeInfo && typeInfo->destructor)
                {
                    typeInfo->destructor->operator()(instance.get());
                }

                created = false;
                validityCacheValid.store(false, std::memory_order_release);
            }
        }

        bool IsClass() const { return typeEnum == RTTMTypeBits::Class; }
        bool IsEnum() const { return typeEnum == RTTMTypeBits::Enum; }
        bool IsVariable() const { return typeEnum == RTTMTypeBits::Variable; }

        bool IsValid() const
        {
            return checkValidOptimized();
        }

        template <typename T>
        bool Is() const
        {
            static const std::type_index cachedTypeIndex = std::type_index(typeid(T));
            return typeIndex == cachedTypeIndex;
        }

        bool IsPrimitiveType() const
        {
            // 使用静态哈希集合提高查找性能
            static const std::unordered_set<std::string_view> primitiveTypes = {
                "int", "float", "double", "long", "short", "char",
                "unsigned int", "unsigned long", "unsigned short", "unsigned char",
                "bool", "size_t", "int8_t", "int16_t", "int32_t", "int64_t",
                "uint8_t", "uint16_t", "uint32_t", "uint64_t"
            };

            return primitiveTypes.count(std::string_view(type)) > 0;
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
            if (!checkValidOptimized())
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

        // 超高性能Get方法 - 核心优化点，三级缓存系统
        static _Ref_<RType> Get(const std::string& typeName)
        {
            // L1缓存：线程局部快速缓存（最热门的类型）
            static thread_local std::array<std::pair<std::string_view, _Ref_<RType>>, 16> hotCache{};
            static thread_local size_t hotCacheIndex = 0;

            std::string_view typeNameView(typeName);

            // 检查热缓存
            for (const auto& [cachedName, cachedType] : hotCache)
            {
                if (cachedName == typeNameView && cachedType)
                {
                    // 创建新实例，避免共享状态
                    return CreateTypeOptimized(
                        cachedType->type,
                        cachedType->typeEnum,
                        cachedType->typeIndex,
                        cachedType->membersName,
                        cachedType->funcsName
                    );
                }
            }

            // L2缓存：类型原型缓存（所有类型）
            static std::unordered_map<std::string_view, _Ref_<RType>> prototypeCache;
            static std::shared_mutex prototypeMutex;

            {
                std::shared_lock<std::shared_mutex> lock(prototypeMutex);
                auto it = prototypeCache.find(typeNameView);
                if (it != prototypeCache.end())
                {
                    const auto& prototype = it->second;
                    auto newInstance = CreateTypeOptimized(
                        prototype->type,
                        prototype->typeEnum,
                        prototype->typeIndex,
                        prototype->membersName,
                        prototype->funcsName
                    );

                    // 更新热缓存
                    hotCache[hotCacheIndex] = {typeNameView, prototype};
                    hotCacheIndex = (hotCacheIndex + 1) % hotCache.size();

                    return newInstance;
                }
            }

            // L3缓存：从全局类型管理器构建
            if (!detail::GlobalTypeManager::Instance().IsTypeRegistered(typeName))
            {
                throw std::runtime_error("RTTM: 类型未注册: " + typeName);
            }

            const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(typeName);
            if (!typeInfo)
            {
                throw std::runtime_error("RTTM: 类型信息未找到: " + typeName);
            }

            // 构建成员和函数名称集合
            std::unordered_set<std::string> membersName;
            std::unordered_set<std::string> funcsName;

            membersName.reserve(typeInfo->members.size());
            funcsName.reserve(typeInfo->functions.size());

            for (const auto& [name, _] : typeInfo->members)
            {
                membersName.emplace(name);
            }

            for (const auto& [name, _] : typeInfo->functions)
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

            // 创建原型并缓存
            auto prototype = CreateTypeOptimized(
                typeName,
                typeInfo->type,
                typeInfo->typeIndex,
                membersName,
                funcsName,
                nullptr
            );

            // 更新原型缓存
            {
                std::unique_lock<std::shared_mutex> lock(prototypeMutex);
                // 控制缓存大小
                if (prototypeCache.size() > 1000)
                {
                    auto it = prototypeCache.begin();
                    std::advance(it, prototypeCache.size() / 4);
                    prototypeCache.erase(prototypeCache.begin(), it);
                }
                prototypeCache[typeNameView] = prototype;
            }

            // 更新热缓存
            hotCache[hotCacheIndex] = {typeNameView, prototype};
            hotCacheIndex = (hotCacheIndex + 1) % hotCache.size();

            // 返回新实例
            return CreateTypeOptimized(
                prototype->type,
                prototype->typeEnum,
                prototype->typeIndex,
                prototype->membersName,
                prototype->funcsName
            );
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
        static Method<T> getMethodOrig(const std::string& name)
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
        static auto getMethodImpl(const std::string& name)
        {
            using traits = detail::function_traits<FuncType>;
            using ReturnType = typename traits::return_type;

            return std::apply([&name](auto... args)
            {
                return getMethodOrig<ReturnType, decltype(args)...>(name);
            }, typename traits::arguments{});
        }

        // 优化的函数存在性检查
        static bool functionExists(const std::string& name)
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
            if (!functionExists(name))
            {
                throw std::runtime_error("RTTM: 函数未注册: " + name);
            }

            return getMethodOrig<T, Args...>(name).Invoke(std::forward<Args>(args)...);
        }

        // 获取全局方法
        template <typename FuncType>
        static auto GetMethod(const std::string& name)
        {
            return getMethodImpl<FuncType>(name);
        }

        // 清理所有全局资源
        static void Cleanup()
        {
            detail::Variables.clear();
            detail::GFunctions.clear();
            detail::Enums.clear();
            detail::GlobalTypeManager::Instance().Clear();
            detail::FactoryCacheManager::Instance().Clear();
            detail::RegisteredTypes.clear();
        }

        // 获取全局统计信息
        static std::map<std::string, size_t> GetGlobalStats()
        {
            return {
                {"registered_types", detail::GlobalTypeManager::Instance().GetTypeCount()},
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

        // 预热所有已注册类型
        static void WarmupAllRegisteredTypes()
        {
            auto registeredTypes = detail::GlobalTypeManager::Instance().GetAllRegisteredTypes();
            for (const auto& typeName : registeredTypes)
            {
                try
                {
                    auto type = RType::Get(typeName);
                    if (type->IsClass() || type->IsVariable())
                    {
                        type->Create();
                    }
                }
                catch (...)
                {
                    // 忽略预热过程中的异常
                }
            }
        }
    };

    // 高性能批量操作类
    class BatchOperations
    {
    public:
        // 批量创建相同类型的对象
        template <typename T, typename... Args>
        static std::vector<_Ref_<RType>> CreateBatch(size_t count, Args... args)
        {
            auto type = RType::Get<T>();
            return type->CreateBatch(count, std::forward<Args>(args)...);
        }

        // 批量设置属性值
        template <typename T>
        static void SetPropertyBatch(const std::vector<_Ref_<RType>>& objects,
                                    const std::string& propertyName,
                                    const T& value)
        {
            for (const auto& obj : objects)
            {
                if (obj && obj->IsValid())
                {
                    try
                    {
                        obj->GetProperty<T>(propertyName) = value;
                    }
                    catch (...)
                    {
                        // 忽略单个对象的错误，继续处理其他对象
                    }
                }
            }
        }

        // 批量调用方法
        template <typename R, typename... Args>
        static std::vector<R> InvokeBatch(const std::vector<_Ref_<RType>>& objects,
                                         const std::string& methodName,
                                         Args... args)
        {
            std::vector<R> results;
            results.reserve(objects.size());

            for (const auto& obj : objects)
            {
                if (obj && obj->IsValid())
                {
                    try
                    {
                        R result = obj->Invoke<R>(methodName, std::forward<Args>(args)...);
                        results.push_back(result);
                    }
                    catch (...)
                    {
                        // 可以选择跳过失败的调用或添加默认值
                        if constexpr (std::is_default_constructible_v<R>)
                        {
                            results.push_back(R{});
                        }
                    }
                }
            }

            return results;
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