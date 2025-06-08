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

template <typename T>
struct is_streamable
{
private:
    template <typename U>
    static auto test(int) -> decltype(std::declval<std::ostream&>() << std::declval<U>(), std::true_type{});

    template <typename U>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

#define RTTM_DEBUG(...) \
do { \
if constexpr (is_streamable<decltype(__VA_ARGS__)>::value) { \
std::cout << "[RTTM] " << __FILE__ << ":" << __LINE__ << " (" << __FUNCTION__ << "): " << __VA_ARGS__ << std::endl; \
} else { \
std::cout << "[RTTM] " << __FILE__ << ":" << __LINE__ << " (" << __FUNCTION__ << "): " << "Unsupported type for stream output" << std::endl; \
} \
} while (0)

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

    // 智能线程安全策略类
    class ThreadSafetyStrategy
    {
    private:
        inline static std::atomic<bool> multiThreadMode{false};
        inline static std::atomic<int> activeThreadCount{1};

    public:
        // 检测多线程环境
        static void RegisterThread()
        {
            int count = activeThreadCount.fetch_add(1);
            if (count > 1)
            {
                multiThreadMode.store(true, std::memory_order_release);
            }
        }

        static void UnregisterThread()
        {
            int count = activeThreadCount.fetch_sub(1);
            if (count <= 1)
            {
                multiThreadMode.store(false, std::memory_order_release);
            }
        }

        static bool IsMultiThreaded()
        {
            return multiThreadMode.load(std::memory_order_acquire);
        }
    };

    // 智能锁包装器
    template <typename Mutex>
    class SmartLock
    {
    private:
        Mutex& mutex_;
        bool locked_;

    public:
        explicit SmartLock(Mutex& mutex) : mutex_(mutex), locked_(false)
        {
            if (ThreadSafetyStrategy::IsMultiThreaded())
            {
                mutex_.lock();
                locked_ = true;
            }
        }

        ~SmartLock()
        {
            if (locked_)
            {
                mutex_.unlock();
            }
        }
    };

    template <typename SharedMutex>
    class SmartSharedLock
    {
    private:
        SharedMutex& mutex_;
        bool locked_;

    public:
        explicit SmartSharedLock(SharedMutex& mutex) : mutex_(mutex), locked_(false)
        {
            if (ThreadSafetyStrategy::IsMultiThreaded())
            {
                mutex_.lock_shared();
                locked_ = true;
            }
        }

        ~SmartSharedLock()
        {
            if (locked_)
            {
                mutex_.unlock_shared();
            }
        }
    };

    // 动态对象池管理器 - 支持256到2^20个对象的动态扩展和收缩，使用智能指针管理
    template <typename T>
    class DynamicObjectPoolManager
    {
    private:
        static constexpr size_t MIN_POOL_SIZE = 256; // 最小池大小
        static constexpr size_t MAX_POOL_SIZE = 1048576; // 最大池大小 (2^20)
        static constexpr size_t GROWTH_FACTOR = 2; // 增长因子
        static constexpr size_t SHRINK_THRESHOLD = 4; // 收缩阈值
        static constexpr size_t MAX_OBJECT_SIZE = 1024; // 最大对象大小限制

        // 对象槽结构，保证内存对齐
        struct alignas(T) ObjectSlot
        {
            char data[sizeof(T)];

            // 获取对象指针
            T* GetObjectPtr() { return reinterpret_cast<T*>(data); }
            const T* GetObjectPtr() const { return reinterpret_cast<const T*>(data); }
        };

        // 内存块，使用智能指针管理
        using MemoryBlock = std::unique_ptr<ObjectSlot[]>;

        // 可用对象的智能指针包装
        struct AvailableObject
        {
            std::shared_ptr<ObjectSlot> slot;

            AvailableObject(ObjectSlot* rawSlot, MemoryBlock* parentBlock)
                : slot(rawSlot, [parentBlock](ObjectSlot*)
                {
                    // 自定义删除器，不实际删除，因为内存由MemoryBlock管理
                    // 这里可以添加调试信息或统计
                })
            {
            }
        };

        std::vector<MemoryBlock> memoryBlocks; // 内存块集合
        std::vector<AvailableObject> availableObjects; // 可用对象集合
        std::atomic<size_t> currentCapacity{MIN_POOL_SIZE}; // 当前容量
        std::atomic<size_t> usedCount{0}; // 已使用数量
        mutable std::mutex poolMutex; // 池操作互斥锁

        // 扩展池容量
        void expandPool()
        {
            size_t newCapacity = std::min(currentCapacity.load() * GROWTH_FACTOR, MAX_POOL_SIZE);
            if (newCapacity <= currentCapacity.load()) return;

            size_t additionalSlots = newCapacity - currentCapacity.load();
            auto newBlock = std::make_unique<ObjectSlot[]>(additionalSlots);

            // 保存原始指针用于后续引用
            MemoryBlock* blockPtr = &memoryBlocks.emplace_back(std::move(newBlock));

            // 将新槽位添加到可用列表
            for (size_t i = 0; i < additionalSlots; ++i)
            {
                availableObjects.emplace_back(&(*blockPtr)[i], blockPtr);
            }

            currentCapacity.store(newCapacity);
        }

        // 收缩池容量
        void shrinkPool()
        {
            size_t newCapacity = std::max(currentCapacity.load() / GROWTH_FACTOR, MIN_POOL_SIZE);
            if (newCapacity >= currentCapacity.load()) return;

            // 只有在使用率很低时才收缩
            if (availableObjects.size() * SHRINK_THRESHOLD < currentCapacity.load())
            {
                // 简单的收缩策略：移除最后的内存块
                if (memoryBlocks.size() > 1)
                {
                    // 计算要移除的槽位数量
                    size_t slotsToRemove = currentCapacity.load() - newCapacity;

                    // 从可用对象中移除对应的槽位
                    if (availableObjects.size() >= slotsToRemove)
                    {
                        availableObjects.erase(availableObjects.end() - slotsToRemove, availableObjects.end());
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
                MemoryBlock* blockPtr = &memoryBlocks.emplace_back(std::move(initialBlock));

                availableObjects.reserve(MIN_POOL_SIZE);

                for (size_t i = 0; i < MIN_POOL_SIZE; ++i)
                {
                    availableObjects.emplace_back(&(*blockPtr)[i], blockPtr);
                }
            }
        }

        // 获取对象，返回智能指针
        std::shared_ptr<T> AcquireObject()
        {
            if constexpr (sizeof(T) <= MAX_OBJECT_SIZE && std::is_trivially_destructible_v<T>)
            {
                std::lock_guard<std::mutex> lock(poolMutex);

                // 如果可用对象不足，扩展池
                if (availableObjects.empty() && currentCapacity.load() < MAX_POOL_SIZE)
                {
                    expandPool();
                }

                if (!availableObjects.empty())
                {
                    auto availableObj = std::move(availableObjects.back());
                    availableObjects.pop_back();
                    usedCount.fetch_add(1, std::memory_order_relaxed);

                    // 创建共享指针，自定义删除器负责归还到池中
                    return std::shared_ptr<T>(
                        availableObj.slot->GetObjectPtr(),
                        [this, slot = availableObj.slot](T* obj)
                        {
                            // 归还对象到池中
                            this->releaseObjectInternal(slot.get());
                        }
                    );
                }
            }
            return nullptr;
        }

        // 内部释放方法
        void releaseObjectInternal(ObjectSlot* slot)
        {
            if constexpr (sizeof(T) <= MAX_OBJECT_SIZE && std::is_trivially_destructible_v<T>)
            {
                std::lock_guard<std::mutex> lock(poolMutex);

                // 找到对应的内存块
                MemoryBlock* parentBlock = nullptr;
                for (auto& block : memoryBlocks)
                {
                    if (slot >= block.get() && slot < block.get() + (currentCapacity.load() / memoryBlocks.size()))
                    {
                        parentBlock = &block;
                        break;
                    }
                }

                if (parentBlock)
                {
                    availableObjects.emplace_back(slot, parentBlock);
                    usedCount.fetch_sub(1, std::memory_order_relaxed);

                    // 检查是否需要收缩池
                    if (availableObjects.size() * SHRINK_THRESHOLD > currentCapacity.load() &&
                        currentCapacity.load() > MIN_POOL_SIZE)
                    {
                        shrinkPool();
                    }
                }
            }
        }

        // 废弃的原始指针接口，保留兼容性

        [[deprecated]]
        T* AcquireObjectRaw()
        {
            auto sharedObj = AcquireObject();
            return sharedObj ? sharedObj.get() : nullptr;
        }

        [[deprecated]]
        void ReleaseObject(T* obj)
        {
            // 这个方法现在不安全，因为无法确定对象的来源
            // 建议使用智能指针版本
            if (obj)
            {
                ObjectSlot* slot = reinterpret_cast<ObjectSlot*>(
                    reinterpret_cast<char*>(obj) - offsetof(ObjectSlot, data)
                );
                releaseObjectInternal(slot);
            }
        }

        size_t GetAvailableCount() const
        {
            std::lock_guard<std::mutex> lock(poolMutex);
            return availableObjects.size();
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

        // 内存来源标记
        enum class MemorySource : uint8_t
        {
            Pool,
            Allocator
        };

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

            std::shared_ptr<T> poolObject = nullptr;
            T* rawPtr = nullptr;
            MemorySource memorySource = MemorySource::Allocator;

            // 优先尝试从动态对象池获取
            poolObject = objectPool.AcquireObject();
            if (poolObject)
            {
                rawPtr = poolObject.get();
                memorySource = MemorySource::Pool;
            }
            else
            {
                // 对象池没有可用对象，分配新内存
                rawPtr = Traits::allocate(*allocator, 1);
                memorySource = MemorySource::Allocator;
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
            catch (const std::exception& e)
            {
                // 构造失败，归还内存
                if (memorySource == MemorySource::Pool)
                {
                    // poolObject会自动归还到池中
                    poolObject.reset();
                }
                else
                {
                    Traits::deallocate(*allocator, rawPtr, 1);
                }
                std::cerr << "RTTM: " << e.what() << std::endl;
                throw;
            }

            // 创建控制结构，管理内存来源和生命周期
            struct ControlBlock
            {
                MemorySource source;
                _Ref_<Allocator> allocatorRef;
                std::shared_ptr<T> poolObjectRef; // 保持池对象的引用
                std::atomic<bool> destroyed{false}; // 添加析构标志，防止重复析构

                ControlBlock(MemorySource src, _Ref_<Allocator> alloc, std::shared_ptr<T> poolObj = nullptr)
                    : source(src), allocatorRef(alloc), poolObjectRef(poolObj)
                {
                }
            };

            auto controlBlock = std::make_shared<ControlBlock>(
                memorySource,
                this->allocator,
                poolObject
            );

            // 创建智能指针，带自定义删除器
            return _Ref_<char>(reinterpret_cast<char*>(rawPtr),
                               [controlBlock](char* ptr)
                               {
                                   if (ptr == nullptr) return;

                                   // 使用原子操作检查是否已经析构，防止重复析构
                                   bool expected = false;
                                   if (!controlBlock->destroyed.compare_exchange_strong(expected, true))
                                   {
                                       // 已经被析构过了，直接返回
                                       return;
                                   }

                                   T* typedPtr = reinterpret_cast<T*>(ptr);

                                   try
                                   {
                                       // 根据内存来源进行不同的处理
                                       if (controlBlock->source == MemorySource::Pool)
                                       {
                                           // 池对象：只需要重置智能指针，池会自动处理析构
                                           controlBlock->poolObjectRef.reset();
                                       }
                                       else
                                       {
                                           // 分配器对象：手动析构然后释放内存
                                           if constexpr (!std::is_trivially_destructible_v<T>)
                                           {
                                               Traits::destroy(*controlBlock->allocatorRef, typedPtr);
                                           }
                                           Traits::deallocate(*controlBlock->allocatorRef, typedPtr, 1);
                                       }
                                   }
                                   catch (const std::exception& e)
                                   {
                                       std::cerr << "RTTM: 销毁对象时发生错误: " << e.what() << std::endl;
                                       // 即使析构失败也要回收内存，避免内存泄漏
                                       try
                                       {
                                           if (controlBlock->source == MemorySource::Pool)
                                           {
                                               controlBlock->poolObjectRef.reset();
                                           }
                                           else
                                           {
                                               // 强制释放内存，不再尝试析构
                                               Traits::deallocate(*controlBlock->allocatorRef, typedPtr, 1);
                                           }
                                       }
                                       catch (...)
                                       {
                                           // 最后的兜底，避免程序崩溃
                                           std::cerr << "RTTM: 内存回收失败，可能存在内存泄漏" << std::endl;
                                       }
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

                FactoryEntry(_Ref_<IFactory> f) : factory(std::move(f))
                {
                }

                FactoryEntry(const FactoryEntry& other)
                    : factory(other.factory), valid(other.valid.load())
                {
                }

                FactoryEntry(FactoryEntry&& other) noexcept
                    : factory(std::move(other.factory)), valid(other.valid.load())
                {
                }

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
                    for (size_t i = 0; i < factoryCache.size() / 4 && it != factoryCache.end();)
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

        // 调用缓存结构 - 仅对非void类型启用
        struct CallCache
        {
            std::vector<uint8_t> argsHash;
            typename std::conditional_t<std::is_void_v<T>, int, T> result; // void类型用int占位
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

    private:
        template <typename... ConvertedArgs>
        T invokeInternal(ConvertedArgs&&... args) const
        {
            // 这里是原来的 Invoke 实现逻辑
            if constexpr (std::conjunction_v<std::is_trivial<ConvertedArgs>...> &&
                std::is_trivial_v<T> &&
                !std::is_void_v<T> &&
                sizeof...(ConvertedArgs) > 0)
            {
                auto hash = hashArgs(args...);

                if (isMemberFunction)
                {
                    if (!instance)
                        throw std::runtime_error("成员函数实例为空: " + name);
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, void*, ConvertedArgs...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "函数签名不匹配: " + name + " 参数类型: " + Object::GetTypeName<ConvertedArgs...>());

                    T result = (*wrapper)(instance, std::forward<ConvertedArgs>(args)...);

                    lastCall.argsHash = std::move(hash);
                    lastCall.result = result;
                    lastCall.valid.store(true, std::memory_order_release);

                    return result;
                }
                else
                {
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, ConvertedArgs...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "函数签名不匹配: " + name + " 参数类型: " + Object::GetTypeName<ConvertedArgs...>());

                    T result = (*wrapper)(std::forward<ConvertedArgs>(args)...);

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
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, void*, ConvertedArgs...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "函数签名不匹配: " + name + " 参数类型: " + Object::GetTypeName<ConvertedArgs...>());

                    if constexpr (std::is_void_v<T>)
                    {
                        (*wrapper)(instance, std::forward<ConvertedArgs>(args)...);
                    }
                    else
                    {
                        return (*wrapper)(instance, std::forward<ConvertedArgs>(args)...);
                    }
                }
                else
                {
                    auto wrapper = std::static_pointer_cast<FunctionWrapper<T, ConvertedArgs...>>(func);
                    if (!wrapper)
                        throw std::runtime_error(
                            "函数签名不匹配: " + name + " 参数类型: " + Object::GetTypeName<ConvertedArgs...>());

                    if constexpr (std::is_void_v<T>)
                    {
                        (*wrapper)(std::forward<ConvertedArgs>(args)...);
                    }
                    else
                    {
                        return (*wrapper)(std::forward<ConvertedArgs>(args)...);
                    }
                }
            }
        }

        // 参数转换辅助函数
        template <typename Arg>
        auto convertArgumentIfNeeded(Arg&& arg) const
        {
            using DecayedArg = std::decay_t<Arg>;

            // 如果是字符串字面量或 const char*，转换为 std::string
            if constexpr (std::is_same_v<DecayedArg, const char*> ||
                std::is_same_v<DecayedArg, char*> ||
                (std::is_array_v<std::remove_reference_t<Arg>> &&
                    std::is_same_v<std::remove_extent_t<std::remove_reference_t<Arg>>, char>))
            {
                return std::string(arg);
            }
            else
            {
                return std::forward<Arg>(arg);
            }
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
        T Invoke(Args&&... args) const
        {
            if (!IsValid())
            {
                throw std::runtime_error("无法调用无效方法: " + name);
            }

            // 转换参数类型
            auto convertedArgs = std::make_tuple(convertArgumentIfNeeded(std::forward<Args>(args))...);

            return std::apply([this](auto&&... convertedArgs)
            {
                return this->invokeInternal(std::forward<decltype(convertedArgs)>(convertedArgs)...);
            }, convertedArgs);
        }


        template <typename... Args>
        T operator()(Args... args) const
        {
            if constexpr (std::is_void_v<T>)
            {
                Invoke(std::forward<Args>(args)...);
            }
            else
            {
                return Invoke(std::forward<Args>(args)...);
            }
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
            mutable std::mutex poolMutex;

            // RType对象的智能指针包装
            struct PooledRType
            {
                std::unique_ptr<RType> rtypePtr;
                std::atomic<bool> inUse{false};
                std::atomic<bool> valid{true}; // 添加有效性标志

                PooledRType() : rtypePtr(std::make_unique<RType>())
                {
                }

                // 安全重置方法
                void SafeReset()
                {
                    if (valid.load() && rtypePtr)
                    {
                        try
                        {
                            rtypePtr->Reset();
                        }
                        catch (...)
                        {
                            // 忽略重置时的异常
                        }
                    }
                    inUse.store(false);
                }

                // 标记为无效
                void Invalidate()
                {
                    valid.store(false);
                    inUse.store(false);
                }
            };

            std::vector<std::unique_ptr<PooledRType[]>> memoryBlocks;
            std::vector<std::shared_ptr<PooledRType>> availableObjects;
            std::atomic<size_t> currentCapacity{MIN_POOL_SIZE};
            std::atomic<size_t> usedCount{0};
            std::atomic<bool> shuttingDown{false}; // 添加关闭标志

            void expandPool()
            {
                if (shuttingDown.load()) return; // 关闭时不再扩展

                size_t newCapacity = std::min(currentCapacity.load() * GROWTH_FACTOR, MAX_POOL_SIZE);
                if (newCapacity <= currentCapacity.load()) return;

                size_t additionalObjects = newCapacity - currentCapacity.load();
                auto newBlock = std::make_unique<PooledRType[]>(additionalObjects);

                for (size_t i = 0; i < additionalObjects; ++i)
                {
                    auto pooledPtr = std::shared_ptr<PooledRType>(
                        &newBlock[i],
                        [](PooledRType*)
                        {
                            // 自定义删除器，不实际删除
                        }
                    );
                    availableObjects.push_back(pooledPtr);
                }

                memoryBlocks.push_back(std::move(newBlock));
                currentCapacity.store(newCapacity);
            }

            void shrinkPool()
            {
                if (shuttingDown.load()) return; // 关闭时不再收缩

                size_t newCapacity = std::max(currentCapacity.load() / GROWTH_FACTOR, MIN_POOL_SIZE);
                if (newCapacity >= currentCapacity.load()) return;

                if (availableObjects.size() * SHRINK_THRESHOLD < currentCapacity.load())
                {
                    if (memoryBlocks.size() > 1)
                    {
                        size_t objectsToRemove = currentCapacity.load() - newCapacity;

                        if (availableObjects.size() >= objectsToRemove)
                        {
                            availableObjects.erase(
                                availableObjects.end() - objectsToRemove,
                                availableObjects.end()
                            );
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
                auto initialBlock = std::make_unique<PooledRType[]>(MIN_POOL_SIZE);
                availableObjects.reserve(MIN_POOL_SIZE);

                for (size_t i = 0; i < MIN_POOL_SIZE; ++i)
                {
                    auto pooledPtr = std::shared_ptr<PooledRType>(
                        &initialBlock[i],
                        [](PooledRType*)
                        {
                            // 自定义删除器，不实际删除
                        }
                    );
                    availableObjects.push_back(pooledPtr);
                }

                memoryBlocks.push_back(std::move(initialBlock));
            }

            // 析构函数 - 安全关闭
            ~DynamicRTypePool()
            {
                shuttingDown.store(true);

                std::lock_guard<std::mutex> lock(poolMutex);

                // 清理容器
                availableObjects.clear();
                memoryBlocks.clear();
            }

            std::shared_ptr<RType> AcquireObject()
            {
                SmartLock<std::mutex> lock(poolMutex);

                if (availableObjects.empty() && currentCapacity.load() < MAX_POOL_SIZE)
                {
                    expandPool();
                }

                if (!availableObjects.empty())
                {
                    auto pooledObj = availableObjects.back();
                    availableObjects.pop_back();

                    if (pooledObj && pooledObj->valid.load())
                    {
                        pooledObj->inUse.store(true);
                        usedCount.fetch_add(1, std::memory_order_relaxed);

                        // 创建弱引用来避免循环引用
                        std::weak_ptr<PooledRType> weakPooledObj = pooledObj;

                        // 返回RType智能指针，使用弱引用避免访问已销毁的对象
                        return std::shared_ptr<RType>(
                            pooledObj->rtypePtr.get(),
                            [this, weakPooledObj](RType* rtype)
                            {
                                // 使用弱引用检查对象是否仍然有效
                                if (auto pooledObj = weakPooledObj.lock())
                                {
                                    this->releaseObjectInternal(pooledObj);
                                }
                            }
                        );
                    }
                }

                return nullptr;
            }

        private:
            void releaseObjectInternal(std::shared_ptr<PooledRType> pooledObj)
            {
                if (!pooledObj || shuttingDown.load()) return;

                // 安全重置对象
                pooledObj->SafeReset();

                SmartLock<std::mutex> lock(poolMutex);

                if (!shuttingDown.load()) // 只在未关闭时才归还
                {
                    availableObjects.push_back(pooledObj);
                    usedCount.fetch_sub(1, std::memory_order_relaxed);

                    // 检查是否需要收缩池
                    if (availableObjects.size() * SHRINK_THRESHOLD > currentCapacity.load() &&
                        currentCapacity.load() > MIN_POOL_SIZE)
                    {
                        shrinkPool();
                    }
                }
            }

        public:
            size_t GetPoolSize() const
            {
                if (shuttingDown.load()) return 0;
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
        template <typename Value>
        auto convertValueIfNeeded(const Value& value) const
        {
            using DecayedValue = std::decay_t<Value>;

            // 如果是字符串字面量或 const char*，转换为 std::string
            if constexpr (std::is_same_v<DecayedValue, const char*> ||
                          std::is_same_v<DecayedValue, char*> ||
                          (std::is_array_v<std::remove_reference_t<Value>> &&
                           std::is_same_v<std::remove_extent_t<std::remove_reference_t<Value>>, char>))
            {
                return std::string(value);
            }
            else
            {
                return value;
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

            return getMethodImplHelper<ReturnType, typename traits::arguments>(name);
        }

    private:
        template <typename ReturnType, typename ArgsTuple>
        struct getMethodImplHelperStruct;

        template <typename ReturnType, typename... Args>
        struct getMethodImplHelperStruct<ReturnType, std::tuple<Args...>>
        {
            template <typename Self>
            static auto call(Self* self, const std::string& name)
            {
                return self->template getMethodOrig<ReturnType, std::decay_t<Args>...>(name);
            }
        };

        template <typename ReturnType, typename ArgsTuple>
        auto getMethodImplHelper(const std::string& name) const
        {
            return getMethodImplHelperStruct<ReturnType, ArgsTuple>::call(this, name);
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

        static _Ref_<RType> CreateTypeOptimized(const std::string& type, RTTMTypeBits typeEnum,
                                                const std::type_index& typeIndex,
                                                const std::unordered_set<std::string>& membersName = {},
                                                const std::unordered_set<std::string>& funcsName = {},
                                                const _Ref_<char>& instance = nullptr)
        {
            // 线程安全的静态类型缓存，使用智能锁策略
            static std::unordered_map<std::string, std::pair<
                                          std::unordered_set<std::string>, std::unordered_set<std::string>>> typeCache;
            static std::shared_mutex typeCacheMutex;

            // 尝试从动态池获取可复用的RType对象
            auto rtypePtr = DynamicRTypePool::Instance().AcquireObject();

            if (rtypePtr)
            {
                // 从池中成功获取对象，重新初始化对象状态
                rtypePtr->type = type;
                rtypePtr->typeEnum = typeEnum;
                rtypePtr->typeIndex = typeIndex;
                rtypePtr->instance = instance;
                rtypePtr->valid = !type.empty();
                rtypePtr->created = (typeEnum == RTTMTypeBits::Variable || instance != nullptr);

                // 清理之前的缓存数据
                rtypePtr->membersName.clear();
                rtypePtr->funcsName.clear();

                // 如果没有提供成员名称，尝试从缓存或类型信息中获取
                if (membersName.empty())
                {
                    // 使用智能读锁检查缓存
                    {
                        SmartSharedLock<std::shared_mutex> readLock(typeCacheMutex);
                        auto cacheIt = typeCache.find(type);
                        if (cacheIt != typeCache.end())
                        {
                            // 从缓存中获取成员名称
                            rtypePtr->membersName = cacheIt->second.first;
                        }
                    }

                    // 如果缓存中没有，从类型信息中获取
                    if (rtypePtr->membersName.empty())
                    {
                        // 从全局类型管理器中获取类型信息
                        const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
                        if (typeInfo)
                        {
                            rtypePtr->membersName.reserve(typeInfo->members.size());
                            for (const auto& [name, _] : typeInfo->members)
                            {
                                rtypePtr->membersName.insert(name);
                            }
                        }
                    }
                }
                else
                {
                    // 使用提供的成员名称
                    rtypePtr->membersName = membersName;
                }

                // 如果没有提供函数名称，尝试从缓存或类型信息中获取
                if (funcsName.empty())
                {
                    // 使用智能读锁检查缓存
                    {
                        SmartSharedLock<std::shared_mutex> readLock(typeCacheMutex);
                        auto cacheIt = typeCache.find(type);
                        if (cacheIt != typeCache.end())
                        {
                            // 从缓存中获取函数名称
                            rtypePtr->funcsName = cacheIt->second.second;
                        }
                    }

                    // 如果缓存中没有，从类型信息中获取
                    if (rtypePtr->funcsName.empty())
                    {
                        // 从全局类型管理器中获取类型信息
                        const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
                        if (typeInfo)
                        {
                            rtypePtr->funcsName.reserve(typeInfo->functions.size());
                            for (const auto& [name, _] : typeInfo->functions)
                            {
                                // 提取函数名称（去除参数部分）
                                size_t paramStart = name.find('(');
                                if (paramStart != std::string::npos)
                                {
                                    rtypePtr->funcsName.insert(name.substr(0, paramStart));
                                }
                                else
                                {
                                    rtypePtr->funcsName.insert(name);
                                }
                            }
                        }
                    }
                }
                else
                {
                    // 使用提供的函数名称
                    rtypePtr->funcsName = funcsName;
                }

                // 如果成功获取了成员或函数信息，智能地更新缓存
                if (!rtypePtr->membersName.empty() || !rtypePtr->funcsName.empty())
                {
                    SmartLock<std::shared_mutex> writeLock(typeCacheMutex);

                    // 双重检查，避免重复写入
                    auto cacheIt = typeCache.find(type);
                    if (cacheIt == typeCache.end())
                    {
                        typeCache[type] = {rtypePtr->membersName, rtypePtr->funcsName};
                    }
                }

                // 清理成员缓存和重置各种状态标志
                rtypePtr->membersCache.clear();
                rtypePtr->propertiesPreloaded.store(false, std::memory_order_relaxed);
                rtypePtr->validityCacheValid.store(false, std::memory_order_relaxed);
                rtypePtr->propCache.valid.store(false, std::memory_order_relaxed);
                rtypePtr->defaultFactoryValid.store(false, std::memory_order_relaxed);

                // 返回从池中获取的对象，智能指针会自动管理生命周期
                return rtypePtr;
            }
            else
            {
                // 池中没有可用对象，创建新的RType实例
                // 这是回退机制，确保在池耗尽时仍能正常工作
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

            // 转换参数值
            auto convertedValue = convertValueIfNeeded(value);
            using ConvertedType = std::decay_t<decltype(convertedValue)>;

#ifndef NDEBUG
            if (typeIndex != std::type_index(typeid(ConvertedType)) && !IsPrimitiveType())
            {
                throw std::runtime_error("RTTM: SetValue中类型不匹配");
            }
#endif

            *reinterpret_cast<ConvertedType*>(instance.get()) = convertedValue;
            return true;
        }


        /*void Destructor() const
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
        }*/

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
                RTTM_DEBUG("RTTM: 无效对象访问: " + type);
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
            // 静态预缓存系统 - 程序启动时加载所有注册类型
            static std::once_flag initFlag;
            static std::unordered_map<std::string, _Ref_<RType>> staticTypeCache;
            static std::shared_mutex staticCacheMutex;

            // 一次性初始化所有注册类型的缓存
            std::call_once(initFlag, []()
            {
                std::unique_lock<std::shared_mutex> lock(staticCacheMutex);

                // 获取所有注册的类型
                const auto& registeredTypes = detail::GlobalTypeManager::Instance().GetAllRegisteredTypes();

                // 预先为所有类型创建原型并缓存
                staticTypeCache.reserve(registeredTypes.size());

                for (const auto& typeName : registeredTypes)
                {
                    const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(typeName);
                    if (!typeInfo) continue;

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

                    // 创建类型原型
                    auto prototype = std::make_shared<RType>(
                        typeName,
                        typeInfo->type,
                        typeInfo->typeIndex,
                        membersName,
                        funcsName,
                        nullptr
                    );

                    staticTypeCache[typeName] = prototype;
                }
            });

            // 从静态缓存中查找类型
            {
                std::shared_lock<std::shared_mutex> lock(staticCacheMutex);
                auto it = staticTypeCache.find(typeName);
                if (it != staticTypeCache.end())
                {
                    const auto& prototype = it->second;
                    // 使用优化的创建方法创建新实例
                    return CreateTypeOptimized(
                        prototype->type,
                        prototype->typeEnum,
                        prototype->typeIndex,
                        prototype->membersName,
                        prototype->funcsName
                    );
                }
            }

            // 如果在静态缓存中没有找到，说明类型未注册或是动态注册的新类型
            if (!detail::GlobalTypeManager::Instance().IsTypeRegistered(typeName))
            {
                throw std::runtime_error("RTTM: 类型未注册: " + typeName);
            }

            // 处理动态注册的新类型（热插拔支持）
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

            // 创建新类型原型并添加到缓存
            auto prototype = std::make_shared<RType>(
                typeName,
                typeInfo->type,
                typeInfo->typeIndex,
                membersName,
                funcsName,
                nullptr
            );

            // 线程安全地添加到静态缓存
            {
                std::unique_lock<std::shared_mutex> lock(staticCacheMutex);
                staticTypeCache[typeName] = prototype;
            }

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
            if (instance)
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
            try
            {
                // 只在有效且已创建且类型不为空时才重置
                if (valid && created && !type.empty())
                {
                    Reset(); // 调用Reset而不是直接调用Destructor
                }
            }
            catch (...)
            {
                // 析构函数中不能抛出异常
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
