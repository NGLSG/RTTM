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
    static const std::string STRINGTYPE = "std::string";

    enum class RTTMTypeBits
    {
        Class,
        Enum,
        Variable,
        Sequence,
        Associative,
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

    template <typename... Args>
    inline static const std::string& CachedTypeName()
    {
        static const std::string name = Object::GetTypeName<Args...>();
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
            if constexpr (sizeof(T) <= MAX_OBJECT_SIZE)
            {
                std::lock_guard<std::mutex> lock(poolMutex);

                if (availableObjects.empty() && currentCapacity.load() < MAX_POOL_SIZE)
                {
                    expandPool();
                }

                if (!availableObjects.empty())
                {
                    auto availableObj = std::move(availableObjects.back());
                    availableObjects.pop_back();
                    usedCount.fetch_add(1, std::memory_order_relaxed);

                    // 返回指向未初始化内存的智能指针
                    // 注意：这里不构造对象，只返回内存块
                    return std::shared_ptr<T>(
                        availableObj.slot->GetObjectPtr(),
                        [this, slot = availableObj.slot](T* obj)
                        {
                            // 归还对象到池中，不调用析构函数（由外部管理）
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
                                               //Exception 0xc0000005 encountered at address 0x7ff61e6e0c72: Access violation reading location 0xffffffffffffffff
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

    class RType;

    namespace detail
    {
        // 容器类型检测
        template <typename T>
        struct is_sequential_container : std::false_type
        {
        };

        template <typename T>
        struct is_sequential_container<std::vector<T>> : std::true_type
        {
        };

        template <typename T>
        struct is_sequential_container<std::list<T>> : std::true_type
        {
        };

        template <typename T>
        struct is_sequential_container<std::deque<T>> : std::true_type
        {
        };

        template <typename T, size_t N>
        struct is_sequential_container<std::array<T, N>> : std::true_type
        {
        };

        template <typename T>
        struct is_associative_container : std::false_type
        {
        };

        template <typename K, typename V>
        struct is_associative_container<std::map<K, V>> : std::true_type
        {
        };

        template <typename K, typename V>
        struct is_associative_container<std::unordered_map<K, V>> : std::true_type
        {
        };

        template <typename T>
        struct is_set_container : std::false_type
        {
        };

        template <typename T>
        struct is_set_container<std::set<T>> : std::true_type
        {
        };

        template <typename T>
        struct is_set_container<std::unordered_set<T>> : std::true_type
        {
        };

        // 运行时类型信息解析器
        class RuntimeTypeParser
        {
        private:
            inline static std::unordered_map<std::string, std::function<void*()>> typeAllocators;
            inline static std::unordered_map<std::string, std::function<void(void*)>> typeDestructors;
            inline static std::unordered_map<std::string, std::function<void(void*, const void*)>> typeCopiers;
            inline static std::unordered_map<std::string, std::function<size_t()>> typeSizes;

        public:
            // 解析复杂类型名称为组件
            struct TypeInfo
            {
                std::string baseType; // 基础类型名 如: vector, map, list
                std::vector<std::string> templateParams; // 模板参数
                bool isContainer = false;
                bool isSequential = false;
                bool isAssociative = false;
                bool isSet = false;
            };

            static TypeInfo ParseType(const std::string& typeName)
            {
                TypeInfo info;

                // 移除空格和多余的"class "前缀
                std::string cleanType = removeClassPrefix(typeName);

                // 特殊处理：将std::basic_string转换为std::string
                cleanType = normalizeStringType(cleanType);

                size_t templateStart = cleanType.find('<');
                if (templateStart == std::string::npos)
                {
                    // 简单类型
                    info.baseType = cleanType;
                    return info;
                }

                info.baseType = cleanType.substr(0, templateStart);
                info.isContainer = true;

                // 判断容器类型
                if (info.baseType == "std::vector" || info.baseType == "std::list" ||
                    info.baseType == "std::deque" || info.baseType.find("std::array") == 0)
                {
                    info.isSequential = true;
                }
                else if (info.baseType == "std::map" || info.baseType == "std::unordered_map")
                {
                    info.isAssociative = true;
                }
                else if (info.baseType == "std::set" || info.baseType == "std::unordered_set")
                {
                    info.isSet = true;
                }

                // 解析模板参数
                info.templateParams = parseTemplateParams(cleanType.substr(templateStart));

                return info;
            }

        private:
            // 标准化字符串类型名称
            static std::string normalizeStringType(const std::string& typeName)
            {
                // 检查是否是std::basic_string<char, std::char_traits<char>, std::allocator<char>>
                if (typeName.find("std::basic_string<char,std::char_traits<char>,std::allocator<char>") == 0)
                {
                    return "std::string";
                }

                // 检查是否是std::basic_string<wchar_t, ...>
                if (typeName.find("std::basic_string<wchar_t") == 0)
                {
                    return "std::wstring";
                }

                // 检查容器中的字符串类型
                std::string result = typeName;
                size_t pos = 0;

                // 替换容器中的basic_string类型
                while ((pos = result.find("std::basic_string<char,std::char_traits<char>,std::allocator<char>>", pos))
                    != std::string::npos)
                {
                    result.replace(pos, 68, "std::string");
                    pos += 11; // "std::string"的长度
                }

                return result;
            }

            // 移除"class "前缀的辅助函数
            static std::string removeClassPrefix(const std::string& typeName)
            {
                std::string result = typeName;

                // 移除开头的"class "
                if (result.substr(0, 6) == "class ")
                {
                    result = result.substr(6);
                }

                // 移除其他位置的"class "（在模板参数中）
                size_t pos = 0;
                while ((pos = result.find(",class ", pos)) != std::string::npos)
                {
                    result.replace(pos, 7, ",");
                    pos += 1;
                }

                // 移除模板参数开始处的"class "
                pos = 0;
                while ((pos = result.find("<class ", pos)) != std::string::npos)
                {
                    result.replace(pos, 7, "<");
                    pos += 1;
                }

                return result;
            }

            static std::vector<std::string> parseTemplateParams(const std::string& templatePart)
            {
                std::vector<std::string> params;
                if (templatePart.empty() || templatePart[0] != '<')
                    return params;

                int bracketLevel = 0;
                size_t start = 1; // 跳过开始的 '<'

                for (size_t i = 1; i < templatePart.length(); ++i)
                {
                    char c = templatePart[i];

                    if (c == '<')
                    {
                        bracketLevel++;
                    }
                    else if (c == '>')
                    {
                        if (bracketLevel == 0)
                        {
                            // 最外层的结束括号
                            if (i > start)
                            {
                                std::string param = templatePart.substr(start, i - start);
                                // 移除参数前后的空格
                                param = trimWhitespace(param);
                                // 移除参数中的"class "前缀
                                param = removeClassPrefix(param);
                                // 标准化字符串类型
                                param = normalizeStringType(param);
                                if (!param.empty())
                                {
                                    params.push_back(param);
                                }
                            }
                            break;
                        }
                        bracketLevel--;
                    }
                    else if (c == ',' && bracketLevel == 0)
                    {
                        // 顶层逗号，分割参数
                        std::string param = templatePart.substr(start, i - start);
                        // 移除参数前后的空格
                        param = trimWhitespace(param);
                        // 移除参数中的"class "前缀
                        param = removeClassPrefix(param);
                        // 标准化字符串类型
                        param = normalizeStringType(param);
                        if (!param.empty())
                        {
                            params.push_back(param);
                        }
                        start = i + 1;
                    }
                }

                return params;
            }

            // 移除字符串前后空格的辅助函数
            static std::string trimWhitespace(const std::string& str)
            {
                size_t start = str.find_first_not_of(" \t\n\r");
                if (start == std::string::npos)
                    return "";

                size_t end = str.find_last_not_of(" \t\n\r");
                return str.substr(start, end - start + 1);
            }

        public:
            // 注册类型创建函数
            template <typename T>
            static void RegisterType()
            {
                // 防止void类型的注册
                if constexpr (std::is_void_v<T>)
                {
                    return;
                }

                // 防止引用类型的注册
                if constexpr (std::is_reference_v<T>)
                {
                    return;
                }

                // 防止函数类型的注册
                if constexpr (std::is_function_v<T>)
                {
                    return;
                }

                // 防止数组类型的注册
                if constexpr (std::is_array_v<T>)
                {
                    return;
                }

                // 防止抽象类型的注册
                if constexpr (std::is_abstract_v<T>)
                {
                    return;
                }

                std::string typeName = CachedTypeName<T>();

                // 只有当类型可以默认构造时才注册分配器
                if constexpr (std::is_default_constructible_v<T> && !std::is_void_v<T>)
                {
                    typeAllocators[typeName] = []() -> void*
                    {
                        return new T();
                    };
                }

                // 只有当类型可以销毁时才注册析构器
                if constexpr (std::is_destructible_v<T> && !std::is_void_v<T>)
                {
                    typeDestructors[typeName] = [](void* ptr)
                    {
                        delete static_cast<T*>(ptr);
                    };
                }

                // 只有当类型可以复制时才注册复制器
                if constexpr (std::is_copy_assignable_v<T> && !std::is_void_v<T>)
                {
                    typeCopiers[typeName] = [](void* dest, const void* src)
                    {
                        *static_cast<T*>(dest) = *static_cast<const T*>(src);
                    };
                }

                // 只为非void类型注册大小信息
                if constexpr (!std::is_void_v<T>)
                {
                    typeSizes[typeName] = []() -> size_t
                    {
                        return sizeof(T);
                    };
                }
            }

            // 创建类型实例
            static void* CreateInstance(const std::string& typeName)
            {
                auto it = typeAllocators.find(typeName);
                if (it != typeAllocators.end())
                {
                    return it->second();
                }
                return nullptr;
            }

            // 销毁类型实例
            static void DestroyInstance(const std::string& typeName, void* ptr)
            {
                auto it = typeDestructors.find(typeName);
                if (it != typeDestructors.end() && ptr)
                {
                    it->second(ptr);
                }
            }

            // 复制类型实例
            static void CopyInstance(const std::string& typeName, void* dest, const void* src)
            {
                auto it = typeCopiers.find(typeName);
                if (it != typeCopiers.end() && dest && src)
                {
                    it->second(dest, src);
                }
            }

            // 获取类型大小
            static size_t GetTypeSize(const std::string& typeName)
            {
                auto it = typeSizes.find(typeName);
                if (it != typeSizes.end())
                {
                    return it->second();
                }
                return 0;
            }

            // 检查类型是否已注册
            static bool IsTypeRegistered(const std::string& typeName)
            {
                return typeSizes.find(typeName) != typeSizes.end();
            }

            // 检查类型是否支持创建实例
            static bool CanCreateInstance(const std::string& typeName)
            {
                return typeAllocators.find(typeName) != typeAllocators.end();
            }
        };

        // 修改后的通用容器接口 - 移除会产生冲突的纯虚函数
        class IUniversalContainer
        {
        public:
            virtual ~IUniversalContainer() = default;
            virtual size_t Size() const = 0;
            virtual bool Empty() const = 0;
            virtual void Clear() = 0;
            virtual std::string GetContainerTypeName() const = 0;
            virtual std::string GetElementTypeName() const = 0;
            virtual void* GetRawPointer() const = 0;
            // 移除这个会产生冲突的纯虚函数
            // virtual RTTMTypeBits GetContainerType() const = 0;
        };

        // 通用顺序容器接口
        class IUniversalSequentialContainer : public IUniversalContainer
        {
        public:
            virtual _Ref_<RType> GetElement(size_t index) const = 0;
            virtual bool SetElement(size_t index, const _Ref_<RType>& value) = 0;
            virtual bool InsertElement(size_t index, const _Ref_<RType>& value) = 0;
            virtual bool RemoveElement(size_t index) = 0;
            virtual bool PushBack(const _Ref_<RType>& value) = 0;
            virtual bool PushFront(const _Ref_<RType>& value) = 0;
            virtual bool PopBack() = 0;
            virtual bool PopFront() = 0;

            // 在这里直接提供实现，不再是纯虚函数
            virtual RTTMTypeBits GetContainerType() const { return RTTMTypeBits::Sequence; }

            // 通用迭代器
            class Iterator
            {
            public:
                virtual ~Iterator() = default;
                virtual _Ref_<RType> Current() const = 0;
                virtual bool Next() = 0;
                virtual bool HasNext() const = 0;
                virtual void Reset() = 0;
            };

            virtual std::unique_ptr<Iterator> CreateIterator() const = 0;
        };

        // 通用关联容器接口
        class IUniversalAssociativeContainer : public IUniversalContainer
        {
        public:
            virtual _Ref_<RType> GetValue(const _Ref_<RType>& key) const = 0;
            virtual bool SetValue(const _Ref_<RType>& key, const _Ref_<RType>& value) = 0;
            virtual bool InsertValue(const _Ref_<RType>& key, const _Ref_<RType>& value) = 0;
            virtual bool RemoveValue(const _Ref_<RType>& key) = 0;
            virtual bool ContainsKey(const _Ref_<RType>& key) const = 0;
            virtual std::string GetKeyTypeName() const = 0;
            virtual std::string GetValueTypeName() const = 0;

            // 在这里直接提供实现，不再是纯虚函数
            virtual RTTMTypeBits GetContainerType() const { return RTTMTypeBits::Associative; }

            // 键值对迭代器
            class KeyValueIterator
            {
            public:
                virtual ~KeyValueIterator() = default;
                virtual _Ref_<RType> CurrentKey() const = 0;
                virtual _Ref_<RType> CurrentValue() const = 0;
                virtual bool Next() = 0;
                virtual bool HasNext() const = 0;
                virtual void Reset() = 0;
            };

            virtual std::unique_ptr<KeyValueIterator> CreateKeyValueIterator() const = 0;
            virtual std::vector<_Ref_<RType>> GetAllKeys() const = 0;
            virtual std::vector<_Ref_<RType>> GetAllValues() const = 0;
        };

        struct Member
        {
            size_t offset;
            RTTMTypeBits type;
            std::type_index typeIndex = std::type_index(typeid(void));
            std::string elementType; // 用于容器类型，存储元素类型名称

            std::string keyType; // 用于关联容器类型，存储键类型名称
            std::string valueType; // 用于关联容器类型，存储值类型名称
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

        void destructor() const;

        void copier() const;

    public:
        Registry_();

        template <typename U>
        Registry_& base();

        template <typename... Args>
        Registry_& constructor();

        // 只有当 T 是类类型时才启用 property 方法
        template <typename U>
        typename std::enable_if_t<std::is_class_v<T>, Registry_&>
        property(const char* name, U T::* value);

        // 只有当 T 是类类型时才启用 method 方法（非const版本）
        template <typename R, typename... Args>
        typename std::enable_if_t<std::is_class_v<T>, Registry_&>
        method(const std::string& name, R (T::*memFunc)(Args...));

        // 只有当 T 是类类型时才启用 method 方法（const版本）
        template <typename R, typename... Args>
        typename std::enable_if_t<std::is_class_v<T>, Registry_&>
        method(const std::string& name, R (T::*memFunc)(Args...) const);
    };

    template <typename T>
    inline static void BasicRegister()
    {
        auto _typeIndex = std::type_index(typeid(T));
        auto _typeName = CachedTypeName<T>();
        auto _type = RTTMTypeBits::Variable;
        if (detail::GlobalTypeManager::Instance().IsTypeRegistered(_typeName))
        {
            RTTM_DEBUG("RTTM: 类型已注册: " + _typeName);
            return;
        }

        detail::TypeBaseInfo typeInfo;
        typeInfo.type = _type;
        typeInfo.size = sizeof(T);
        typeInfo.typeIndex = _typeIndex;

        detail::GlobalTypeManager::Instance().RegisterType(_typeName, typeInfo);
        detail::RegisteredTypes.insert(_typeName);

        RType::RegisterContainerSupportForType<T>();
        auto factory = Factory<T>::CreateFactory();
        typeInfo.factories["default"] = factory;
        auto factory2 = Factory<T, T>::CreateFactory();
        typeInfo.factories[CachedTypeName<T>()] = factory2;
        detail::GlobalTypeManager::Instance().RegisterType(_typeName, typeInfo);
        RTTM_DEBUG("RTTM: 注册类型: " + _typeName + " 大小: " + std::to_string(sizeof(T)));
    }

    template <typename T>
    class Enum_
    {
    public:
        Enum_()
        {
            if (detail::Enums.find(CachedTypeName<T>()) != detail::Enums.end())
            {
                detail::Enums.insert({CachedTypeName<T>(), {}});
            }
        }

        Enum_& value(const std::string& name, T value)
        {
            if constexpr (!std::is_enum_v<T>)
            {
                std::cerr << "RTTM: T 必须是枚举类型" << std::endl;
                return *this;
            }
            detail::Enums[CachedTypeName<T>()][name] = static_cast<int>(value);
            return *this;
        }

        T GetValue(const std::string& name)
        {
            if constexpr (!std::is_enum_v<T>)
            {
                std::cerr << "RTTM: T 必须是枚举类型" << std::endl;
                return T();
            }
            return static_cast<T>(detail::Enums[CachedTypeName<T>()][name]);
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
                            "函数签名不匹配: " + name + " 参数类型: " + CachedTypeName<ConvertedArgs...>());

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
                            "函数签名不匹配: " + name + " 参数类型: " + CachedTypeName<ConvertedArgs...>());

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
                            "函数签名不匹配: " + name + " 参数类型: " + CachedTypeName<ConvertedArgs...>());

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
                            "函数签名不匹配: " + name + " 参数类型: " + CachedTypeName<ConvertedArgs...>());

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
        // 参数哈希计算优化 - 编译时优化
        template <typename... Args>
        static constexpr size_t getArgsHash() noexcept;

        template <typename Value>
        auto convertValueIfNeeded(const Value& value) const;

        // 超高性能工厂查找 - 多级缓存策略
        template <typename... Args>
        _Ref_<IFactory> findFactoryOptimized() const;

        // 超快速有效性检查
        bool checkValidOptimized() const;

        void copyFrom(const _Ref_<char>& newInst) const;

        template <typename FuncType>
        auto getMethodImpl(const std::string& name) const;

    private:
        template <typename ReturnType, typename ArgsTuple>
        struct getMethodImplHelperStruct;

        template <typename ReturnType, typename... Args>
        struct getMethodImplHelperStruct<ReturnType, std::tuple<Args...>>
        {
            template <typename Self>
            static auto call(Self* self, const std::string& name);
        };

        template <typename ReturnType, typename ArgsTuple>
        auto getMethodImplHelper(const std::string& name) const;

        template <typename T, typename... Args>
        Method<T> getMethodOrig(const std::string& name) const;

        // 属性预加载优化
        void preloadAllPropertiesOptimized() const;
        // 递归创建顺序容器视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateSequentialViewForType(
            const detail::RuntimeTypeParser::TypeInfo& typeInfo, void* containerPtr,
            const std::string& fullTypeName) const;


        // 为不同的元素类型创建vector视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateVectorView(
            const std::string& elementType, void* containerPtr, const std::string& full) const;

        // 为不同的元素类型创建list视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateListView(
            const std::string& elementType, void* containerPtr, const std::string& full) const;

        // 为不同的元素类型创建deque视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateDequeView(
            const std::string& elementType, void* containerPtr, const std::string& full) const;


        // 创建嵌套vector视图（用于处理vector<vector<T>>等复杂类型）
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateNestedVectorView(
            const detail::RuntimeTypeParser::TypeInfo& elementTypeInfo, void* containerPtr,
            const std::string& full) const;

        // 创建嵌套list视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateNestedListView(
            const detail::RuntimeTypeParser::TypeInfo& elementTypeInfo, void* containerPtr) const;

        // 创建嵌套deque视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateNestedDequeView(
            const detail::RuntimeTypeParser::TypeInfo& elementTypeInfo, void* containerPtr) const;


        // 为自定义类型创建vector视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateCustomTypeVectorView(
            const std::string& elementType, void* containerPtr, const std::string& full) const;

        // 为自定义类型创建list视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateCustomTypeListView(
            const std::string& elementType, void* containerPtr, const std::string& full) const;

        // 为自定义类型创建deque视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateCustomTypeDequeView(
            const std::string& elementType, void* containerPtr, const std::string& full) const;

        // 通用vector视图（用于任意已注册类型）
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateGenericVectorView(
            const std::string& elementType, void* containerPtr, const std::string& full) const;

        // 通用list视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateGenericListView(
            const std::string& elementType, void* containerPtr, const std::string& full) const;

        // 通用deque视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateGenericDequeView(
            const std::string& elementType, void* containerPtr, const std::string& full) const;
        static int CalculateNestingLevel(const std::string& typeName);

    private:
        // 原子变量优化，减少锁竞争
        mutable std::atomic<bool> lastValidityCheck{false};
        mutable std::atomic<bool> validityCacheValid{false};
        mutable std::atomic<bool> propertiesPreloaded{false};

        // 默认工厂缓存
        mutable _Ref_<IFactory> defaultFactory;
        mutable std::atomic<bool> defaultFactoryValid{false};

    protected:
        friend class DynamicRTypePool;
        mutable std::unordered_set<std::string> membersName;
        mutable std::unordered_set<std::string> funcsName;
        mutable std::unordered_map<std::string, _Ref_<RType>> membersCache;

        // 优化的属性查找缓存
        struct PropertyLookupCache
        {
            std::string lastPropertyName;
            size_t offset = 0;
            std::atomic<bool> valid{false};

            PropertyLookupCache();

            PropertyLookupCache(const PropertyLookupCache& other);

            PropertyLookupCache(PropertyLookupCache&& other) noexcept;

            PropertyLookupCache& operator=(const PropertyLookupCache& other);

            PropertyLookupCache& operator=(PropertyLookupCache&& other) noexcept;
        };

        mutable PropertyLookupCache propCache;

    public:
        std::string type;
        RTTMTypeBits typeEnum;
        std::type_index typeIndex = std::type_index(typeid(void));
        mutable _Ref_<char> instance;
        mutable bool valid = false;
        mutable bool created = false;
        RType();

        RType(std::string _type);

        RType(const std::string& _type, RTTMTypeBits _typeEnum, const std::type_index& _typeIndex,
              const std::unordered_set<std::string>& _membersName = {},
              const std::unordered_set<std::string>& _funcsName = {},
              const _Ref_<char>& _instance = nullptr);

        RType(const RType& rtype) noexcept;

        static _Ref_<RType> CreateTypeOptimized(const std::string& type, RTTMTypeBits typeEnum,
                                                const std::type_index& typeIndex,
                                                const std::unordered_set<std::string>& membersName = {},
                                                const std::unordered_set<std::string>& funcsName = {},
                                                const _Ref_<char>& instance = nullptr);

        static _Ref_<RType> CreateType(const std::string& type, RTTMTypeBits typeEnum, const std::type_index& typeIndex,
                                       const std::unordered_set<std::string>& membersName = {},
                                       const std::unordered_set<std::string>& funcsName = {},
                                       const _Ref_<char>& instance = nullptr);

        // 初始化通用容器系统
        static void InitializeUniversalContainerSystem();

        // 为特定类型动态注册容器支持
        template <typename T>
        static void RegisterContainerSupportForType();

        // 检查是否支持复杂嵌套类型
        bool IsComplexNestedContainer() const;

        // 获取嵌套层级
        int GetNestingLevel() const;

        // 超高性能Create方法 - 核心优化
        template <typename... Args>
        bool Create(Args... args) const;

        // 批量创建优化 - 支持高并发场景
        template <typename... Args>
        std::vector<_Ref_<RType>> CreateBatch(size_t count, Args... args) const;

        template <typename T>
        void Attach(T& inst) const;

        template <typename R, typename... Args>
        R Invoke(const std::string& name, Args... args) const;

        template <typename FuncType>
        auto GetMethod(const std::string& name) const;

        template <typename T>
        T& GetProperty(const std::string& name) const;

        _Ref_<RType> GetProperty(const std::string& name) const;

        std::unordered_map<std::string, _Ref_<RType>> GetProperties() const;

        template <typename T>
        bool SetValue(const T& value) const;


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

        bool IsClass() const;
        bool IsEnum() const;
        bool IsVariable() const;

        bool IsValid() const;

        template <typename T>
        bool Is() const;

        bool IsPrimitiveType() const;

        const std::unordered_set<std::string>& GetPropertyNames() const;

        const std::unordered_set<std::string>& GetMethodNames() const;

        void* GetRaw() const;

        template <typename T>
        T& As() const;

        const std::string& GetTypeName() const;

        // 超高性能Get方法 - 核心优化点，三级缓存系统
        static _Ref_<RType> Get(const std::string& typeName);

        // 获取泛型类型信息
        template <typename T>
        static _Ref_<RType> Get();

        // 重置方法 - 清理资源和缓存
        void Reset() const;

        // 检查是否为顺序容器
        bool IsSequentialContainer() const;

        // 检查是否为关联容器
        bool IsAssociativeContainer() const;
        // 创建通用顺序容器视图
        std::unique_ptr<detail::IUniversalSequentialContainer> CreateUniversalSequentialView() const;

        // 析构函数
        ~RType();
    };

    // 首先添加SFINAE检测辅助类（用于C++17）
#if __cplusplus < 202002L
    namespace detail
    {
        // SFINAE检测是否支持下标操作符
        template <typename T>
        class has_subscript_operator
        {
            template <typename U>
            static auto test(U* u) -> decltype((*u)[0], std::true_type{});

            template <typename>
            static std::false_type test(...);

        public:
            using type = decltype(test<T>(nullptr));
            static constexpr bool value = type::value;
        };

        // SFINAE检测是否支持push_back
        template <typename T>
        class has_push_back
        {
            template <typename U>
            static auto test(U* u) -> decltype(u->push_back(typename U::value_type{}), std::true_type{});

            template <typename>
            static std::false_type test(...);

        public:
            using type = decltype(test<T>(nullptr));
            static constexpr bool value = type::value;
        };

        // SFINAE检测是否支持push_front
        template <typename T>
        class has_push_front
        {
            template <typename U>
            static auto test(U* u) -> decltype(u->push_front(typename U::value_type{}), std::true_type{});

            template <typename>
            static std::false_type test(...);

        public:
            using type = decltype(test<T>(nullptr));
            static constexpr bool value = type::value;
        };

        // SFINAE检测是否支持pop_back
        template <typename T>
        class has_pop_back
        {
            template <typename U>
            static auto test(U* u) -> decltype(u->pop_back(), std::true_type{});

            template <typename>
            static std::false_type test(...);

        public:
            using type = decltype(test<T>(nullptr));
            static constexpr bool value = type::value;
        };

        // SFINAE检测是否支持pop_front
        template <typename T>
        class has_pop_front
        {
            template <typename U>
            static auto test(U* u) -> decltype(u->pop_front(), std::true_type{});

            template <typename>
            static std::false_type test(...);

        public:
            using type = decltype(test<T>(nullptr));
            static constexpr bool value = type::value;
        };

        // SFINAE检测是否有value_type
        template <typename T>
        class has_value_type
        {
            template <typename U>
            static auto test(U*) -> decltype(typename U::value_type{}, std::true_type{});

            template <typename>
            static std::false_type test(...);

        public:
            using type = decltype(test<T>(nullptr));
            static constexpr bool value = type::value;
        };
    }
#endif

    // 通用容器系统 - 支持任意嵌套的容器类型
    namespace detail
    {
        // 通用顺序容器实现 - 直接继承IUniversalSequentialContainer
        template <typename ContainerType>
        class UniversalSequentialContainerImpl : public IUniversalSequentialContainer
        {
        private:
            using ElementType = typename ContainerType::value_type;

            // 容器相关成员变量（原本在UniversalContainerImpl中）
            ContainerType* container;
            std::string containerTypeName;
            std::string elementTypeName;

            // 顺序容器迭代器实现
            class SequentialIteratorImpl : public IUniversalSequentialContainer::Iterator
            {
            private:
                typename ContainerType::iterator current;
                typename ContainerType::iterator end;
                const UniversalSequentialContainerImpl* owner;

            public:
                SequentialIteratorImpl(typename ContainerType::iterator begin, typename ContainerType::iterator endIter,
                                       const UniversalSequentialContainerImpl* ownerPtr)
                    : current(begin), end(endIter), owner(ownerPtr)
                {
                }

                _Ref_<RType> Current() const override
                {
                    if (current == end)
                        return nullptr;

                    // 创建或获取元素的RType
                    auto elementRType = createElementRType(*current);
                    return elementRType;
                }

                bool Next() override
                {
                    if (current != end)
                    {
                        ++current;
                        return true;
                    }
                    return false;
                }

                bool HasNext() const override
                {
                    return current != end;
                }

                void Reset() override
                {
                    if (owner && owner->container)
                    {
                        current = owner->container->begin();
                        end = owner->container->end();
                    }
                }

            private:
                _Ref_<RType> createElementRType(const ElementType& element) const
                {
                    // 尝试从已注册类型中获取
                    auto elementRType = RType::Get(CachedTypeName<ElementType>());
                    if (elementRType && elementRType->IsValid())
                    {
                        elementRType->Attach(const_cast<ElementType&>(element));
                        return elementRType;
                    }

                    // 如果类型未注册，创建通用包装
                    return createUnregisteredTypeWrapper(element);
                }

                _Ref_<RType> createUnregisteredTypeWrapper(const ElementType& element) const
                {
                    // 为未注册类型创建包装
                    std::string typeName = CachedTypeName<ElementType>();

                    // 检查是否是容器类型
                    auto typeInfo = RuntimeTypeParser::ParseType(typeName);
                    if (typeInfo.isContainer)
                    {
                        // 递归处理嵌套容器
                        return createNestedContainerWrapper(element, typeInfo);
                    }
                    else
                    {
                        // 普通类型包装
                        return createSimpleTypeWrapper(element);
                    }
                }

                _Ref_<RType> createNestedContainerWrapper(const ElementType& element,
                                                          const RuntimeTypeParser::TypeInfo& typeInfo) const
                {
                    std::string typeName = CachedTypeName<ElementType>();

                    // 创建一个RType来包装嵌套容器
                    auto containerRType = RType::CreateType(
                        typeName,
                        typeInfo.isSequential
                            ? RTTMTypeBits::Sequence
                            : (typeInfo.isAssociative ? RTTMTypeBits::Associative : RTTMTypeBits::Variable),
                        std::type_index(typeid(ElementType))
                    );

                    if (containerRType)
                    {
                        containerRType->Attach(const_cast<ElementType&>(element));
                    }

                    return containerRType;
                }

                _Ref_<RType> createSimpleTypeWrapper(const ElementType& element) const
                {
                    std::string typeName = CachedTypeName<ElementType>();

                    auto simpleRType = RType::CreateType(
                        typeName,
                        std::is_class_v<ElementType> ? RTTMTypeBits::Class : RTTMTypeBits::Variable,
                        std::type_index(typeid(ElementType))
                    );

                    if (simpleRType)
                    {
                        simpleRType->Attach(const_cast<ElementType&>(element));
                    }

                    return simpleRType;
                }
            };

        public:
            explicit UniversalSequentialContainerImpl(ContainerType* cont)
                : container(cont)
                  , containerTypeName(CachedTypeName<ContainerType>())
            {
                // C++版本检测并初始化元素类型名称
#if __cplusplus >= 202002L
        if constexpr (requires { typename ContainerType::value_type; })
        {
            elementTypeName = CachedTypeName<typename ContainerType::value_type>();
        }
#else
                if constexpr (detail::has_value_type<ContainerType>::value)
                {
                    elementTypeName = CachedTypeName<typename ContainerType::value_type>();
                }
#endif
            }

            // 实现IUniversalContainer接口（原本在基类中的方法）
            size_t Size() const override
            {
                return container ? container->size() : 0;
            }

            bool Empty() const override
            {
                return container ? container->empty() : true;
            }

            void Clear() override
            {
                if (container)
                {
                    container->clear();
                }
            }

            std::string GetContainerTypeName() const override
            {
                return containerTypeName;
            }

            std::string GetElementTypeName() const override
            {
                return elementTypeName;
            }

            void* GetRawPointer() const override
            {
                return container;
            }

            // 实现IUniversalSequentialContainer特有的接口
            _Ref_<RType> GetElement(size_t index) const override
            {
                if (!container || index >= container->size())
                    return nullptr;

                // 根据C++版本使用不同的检测方式
#if __cplusplus >= 202002L
        // C++20版本 - 使用requires表达式
        if constexpr (requires { (*container)[index]; })
        {
            const auto& element = (*container)[index];
            return createElementRType(element);
        }
        else
        {
            // 不支持随机访问的容器（如list）
            auto it = container->begin();
            std::advance(it, index);
            if (it != container->end())
            {
                return createElementRType(*it);
            }
        }
#else
                // C++17版本 - 使用SFINAE
                return getElementImpl(index, typename detail::has_subscript_operator<ContainerType>::type{});
#endif
                return nullptr;
            }

            bool SetElement(size_t index, const _Ref_<RType>& value) override
            {
                if (!container || !value || index >= container->size())
                    return false;

                try
                {
#if __cplusplus >= 202002L
            // C++20版本
            if constexpr (requires { (*container)[index]; })
            {
                (*container)[index] = extractElementValue(value);
                return true;
            }
            else
            {
                auto it = container->begin();
                std::advance(it, index);
                if (it != container->end())
                {
                    *it = extractElementValue(value);
                    return true;
                }
            }
#else
                    // C++17版本
                    return setElementImpl(index, value, typename detail::has_subscript_operator<ContainerType>::type{});
#endif
                }
                catch (...)
                {
                    return false;
                }
                return false;
            }

            bool InsertElement(size_t index, const _Ref_<RType>& value) override
            {
                if (!container || !value)
                    return false;

                try
                {
                    ElementType elementValue = extractElementValue(value);

                    if (index >= container->size())
                    {
                        container->insert(container->end(), elementValue);
                    }
                    else
                    {
                        auto it = container->begin();
                        std::advance(it, index);
                        container->insert(it, elementValue);
                    }
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }

            bool RemoveElement(size_t index) override
            {
                if (!container || index >= container->size())
                    return false;

                try
                {
                    auto it = container->begin();
                    std::advance(it, index);
                    container->erase(it);
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }

            bool PushBack(const _Ref_<RType>& value) override
            {
                if (!container || !value)
                    return false;

#if __cplusplus >= 202002L
        // C++20版本
        if constexpr (requires { container->push_back(ElementType{}); })
        {
            try
            {
                ElementType elementValue = extractElementValue(value);
                container->push_back(elementValue);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
#else
                // C++17版本
                return pushBackImpl(value, typename detail::has_push_back<ContainerType>::type{});
#endif
                return false;
            }

            bool PushFront(const _Ref_<RType>& value) override
            {
                if (!container || !value)
                    return false;

#if __cplusplus >= 202002L
        // C++20版本
        if constexpr (requires { container->push_front(ElementType{}); })
        {
            try
            {
                ElementType elementValue = extractElementValue(value);
                container->push_front(elementValue);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
#else
                // C++17版本
                return pushFrontImpl(value, typename detail::has_push_front<ContainerType>::type{});
#endif
                return false;
            }

            bool PopBack() override
            {
                if (!container || container->empty())
                    return false;

#if __cplusplus >= 202002L
        // C++20版本
        if constexpr (requires { container->pop_back(); })
        {
            try
            {
                container->pop_back();
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
#else
                // C++17版本
                return popBackImpl(typename detail::has_pop_back<ContainerType>::type{});
#endif
                return false;
            }

            bool PopFront() override
            {
                if (!container || container->empty())
                    return false;

#if __cplusplus >= 202002L
        // C++20版本
        if constexpr (requires { container->pop_front(); })
        {
            try
            {
                container->pop_front();
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
#else
                // C++17版本
                return popFrontImpl(typename detail::has_pop_front<ContainerType>::type{});
#endif
                return false;
            }

            std::unique_ptr<IUniversalSequentialContainer::Iterator> CreateIterator() const override
            {
                if (!container)
                    return nullptr;

                return std::make_unique<SequentialIteratorImpl>(
                    container->begin(),
                    container->end(),
                    this
                );
            }

        private:
#if __cplusplus < 202002L
            // C++17版本的SFINAE辅助函数
            _Ref_<RType> getElementImpl(size_t index, std::true_type) const
            {
                const auto& element = (*container)[index];
                return createElementRType(element);
            }

            _Ref_<RType> getElementImpl(size_t index, std::false_type) const
            {
                auto it = container->begin();
                std::advance(it, index);
                if (it != container->end())
                {
                    return createElementRType(*it);
                }
                return nullptr;
            }

            bool setElementImpl(size_t index, const _Ref_<RType>& value, std::true_type)
            {
                (*container)[index] = extractElementValue(value);
                return true;
            }

            bool setElementImpl(size_t index, const _Ref_<RType>& value, std::false_type)
            {
                auto it = container->begin();
                std::advance(it, index);
                if (it != container->end())
                {
                    *it = extractElementValue(value);
                    return true;
                }
                return false;
            }

            bool pushBackImpl(const _Ref_<RType>& value, std::true_type)
            {
                try
                {
                    ElementType elementValue = extractElementValue(value);
                    container->push_back(elementValue);
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }

            bool pushBackImpl(const _Ref_<RType>&, std::false_type)
            {
                return false;
            }

            bool pushFrontImpl(const _Ref_<RType>& value, std::true_type)
            {
                try
                {
                    ElementType elementValue = extractElementValue(value);
                    container->push_front(elementValue);
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }

            bool pushFrontImpl(const _Ref_<RType>&, std::false_type)
            {
                return false;
            }

            bool popBackImpl(std::true_type)
            {
                try
                {
                    container->pop_back();
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }

            bool popBackImpl(std::false_type)
            {
                return false;
            }

            bool popFrontImpl(std::true_type)
            {
                try
                {
                    container->pop_front();
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }

            bool popFrontImpl(std::false_type)
            {
                return false;
            }
#endif

            _Ref_<RType> createElementRType(const ElementType& element) const
            {
                // 尝试从已注册类型中获取
                auto elementRType = RType::Get(CachedTypeName<ElementType>());
                elementRType->Create();
                if (elementRType && elementRType->IsValid())
                {
                    elementRType->Attach(const_cast<ElementType&>(element));
                    return elementRType;
                }

                // 如果类型未注册，创建通用包装
                return createUnregisteredTypeWrapper(element);
            }

            _Ref_<RType> createUnregisteredTypeWrapper(const ElementType& element) const
            {
                std::string typeName = CachedTypeName<ElementType>();

                // 检查是否是容器类型
                auto typeInfo = RuntimeTypeParser::ParseType(typeName);
                if (typeInfo.isContainer)
                {
                    // 递归处理嵌套容器
                    return createNestedContainerWrapper(element, typeInfo);
                }
                else
                {
                    // 普通类型包装
                    return createSimpleTypeWrapper(element);
                }
            }

            _Ref_<RType> createNestedContainerWrapper(const ElementType& element,
                                                      const RuntimeTypeParser::TypeInfo& typeInfo) const
            {
                std::string typeName = CachedTypeName<ElementType>();

                auto containerRType = RType::CreateType(
                    typeName,
                    typeInfo.isSequential
                        ? RTTMTypeBits::Sequence
                        : (typeInfo.isAssociative ? RTTMTypeBits::Associative : RTTMTypeBits::Variable),
                    std::type_index(typeid(ElementType))
                );

                if (containerRType)
                {
                    containerRType->Attach(const_cast<ElementType&>(element));
                }

                return containerRType;
            }

            _Ref_<RType> createSimpleTypeWrapper(const ElementType& element) const
            {
                std::string typeName = CachedTypeName<ElementType>();

                auto simpleRType = RType::CreateType(
                    typeName,
                    std::is_class_v<ElementType> ? RTTMTypeBits::Class : RTTMTypeBits::Variable,
                    std::type_index(typeid(ElementType))
                );

                if (simpleRType)
                {
                    simpleRType->Attach(const_cast<ElementType&>(element));
                }

                return simpleRType;
            }

            ElementType extractElementValue(const _Ref_<RType>& rtype) const
            {
                if (!rtype)
                    throw std::runtime_error("空的RType对象");

                // 尝试直接转换
                try
                {
                    return rtype->As<ElementType>();
                }
                catch (...)
                {
                    // 如果直接转换失败，尝试其他方法
                    if (rtype->GetRaw())
                    {
                        return *static_cast<ElementType*>(rtype->GetRaw());
                    }
                    throw std::runtime_error("无法提取元素值");
                }
            }
        };

        // 通用关联容器实现的构造函数也需要类似修改
        template <typename ContainerType>
        class UniversalAssociativeContainerImpl : public IUniversalAssociativeContainer
        {
        private:
            using KeyType = typename ContainerType::key_type;
            using ValueType = typename ContainerType::mapped_type;

            // 容器相关成员变量（原本在UniversalContainerImpl中）
            ContainerType* container;
            std::string containerTypeName;
            std::string elementTypeName;
            std::string keyTypeName;
            std::string valueTypeName;

            // 关联容器迭代器实现
            class AssociativeIteratorImpl : public IUniversalAssociativeContainer::KeyValueIterator
            {
            private:
                typename ContainerType::iterator current;
                typename ContainerType::iterator end;
                const UniversalAssociativeContainerImpl* owner;

            public:
                AssociativeIteratorImpl(typename ContainerType::iterator begin,
                                        typename ContainerType::iterator endIter,
                                        const UniversalAssociativeContainerImpl* ownerPtr)
                    : current(begin), end(endIter), owner(ownerPtr)
                {
                }

                _Ref_<RType> CurrentKey() const override
                {
                    if (current == end)
                        return nullptr;

                    return owner->createKeyRType(current->first);
                }

                _Ref_<RType> CurrentValue() const override
                {
                    if (current == end)
                        return nullptr;

                    return owner->createValueRType(current->second);
                }

                bool Next() override
                {
                    if (current != end)
                    {
                        ++current;
                        return true;
                    }
                    return false;
                }

                bool HasNext() const override
                {
                    return current != end;
                }

                void Reset() override
                {
                    if (owner && owner->container)
                    {
                        current = owner->container->begin();
                        end = owner->container->end();
                    }
                }
            };

        public:
            explicit UniversalAssociativeContainerImpl(ContainerType* cont)
                : container(cont)
                  , containerTypeName(CachedTypeName<ContainerType>())
                  , keyTypeName(CachedTypeName<KeyType>())
                  , valueTypeName(CachedTypeName<ValueType>())
            {
                // C++版本检测并初始化元素类型名称
#if __cplusplus >= 202002L
        if constexpr (requires { typename ContainerType::value_type; })
        {
            elementTypeName = CachedTypeName<typename ContainerType::value_type>();
        }
#else
                if constexpr (detail::has_value_type<ContainerType>::value)
                {
                    elementTypeName = CachedTypeName<typename ContainerType::value_type>();
                }
#endif
            }

            // 其余的public方法保持不变...
            size_t Size() const override
            {
                return container ? container->size() : 0;
            }

            bool Empty() const override
            {
                return container ? container->empty() : true;
            }

            void Clear() override
            {
                if (container)
                {
                    container->clear();
                }
            }

            std::string GetContainerTypeName() const override
            {
                return containerTypeName;
            }

            std::string GetElementTypeName() const override
            {
                return elementTypeName;
            }

            void* GetRawPointer() const override
            {
                return container;
            }

            std::string GetKeyTypeName() const override
            {
                return keyTypeName;
            }

            std::string GetValueTypeName() const override
            {
                return valueTypeName;
            }

            _Ref_<RType> GetValue(const _Ref_<RType>& key) const override
            {
                if (!container || !key)
                    return nullptr;

                try
                {
                    KeyType keyValue = extractKeyValue(key);
                    auto it = container->find(keyValue);
                    if (it != container->end())
                    {
                        return createValueRType(it->second);
                    }
                }
                catch (...)
                {
                }
                return nullptr;
            }

            bool SetValue(const _Ref_<RType>& key, const _Ref_<RType>& value) override
            {
                if (!container || !key || !value)
                    return false;

                try
                {
                    KeyType keyValue = extractKeyValue(key);
                    ValueType valueValue = extractValueValue(value);
                    (*container)[keyValue] = valueValue;
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }

            bool InsertValue(const _Ref_<RType>& key, const _Ref_<RType>& value) override
            {
                if (!container || !key || !value)
                    return false;

                try
                {
                    KeyType keyValue = extractKeyValue(key);
                    ValueType valueValue = extractValueValue(value);
                    auto result = container->insert({keyValue, valueValue});
                    return result.second;
                }
                catch (...)
                {
                    return false;
                }
            }

            bool RemoveValue(const _Ref_<RType>& key) override
            {
                if (!container || !key)
                    return false;

                try
                {
                    KeyType keyValue = extractKeyValue(key);
                    return container->erase(keyValue) > 0;
                }
                catch (...)
                {
                    return false;
                }
            }

            bool ContainsKey(const _Ref_<RType>& key) const override
            {
                if (!container || !key)
                    return false;

                try
                {
                    KeyType keyValue = extractKeyValue(key);
                    return container->find(keyValue) != container->end();
                }
                catch (...)
                {
                    return false;
                }
            }

            std::unique_ptr<IUniversalAssociativeContainer::KeyValueIterator> CreateKeyValueIterator() const override
            {
                if (!container)
                    return nullptr;

                return std::make_unique<AssociativeIteratorImpl>(
                    container->begin(),
                    container->end(),
                    this
                );
            }

            std::vector<_Ref_<RType>> GetAllKeys() const override
            {
                std::vector<_Ref_<RType>> keys;
                if (!container)
                    return keys;

                keys.reserve(container->size());
                for (const auto& pair : *container)
                {
                    auto keyRType = createKeyRType(pair.first);
                    if (keyRType)
                    {
                        keys.push_back(keyRType);
                    }
                }
                return keys;
            }

            std::vector<_Ref_<RType>> GetAllValues() const override
            {
                std::vector<_Ref_<RType>> values;
                if (!container)
                    return values;

                values.reserve(container->size());
                for (const auto& pair : *container)
                {
                    auto valueRType = createValueRType(pair.second);
                    if (valueRType)
                    {
                        values.push_back(valueRType);
                    }
                }
                return values;
            }

        private:
            friend class AssociativeIteratorImpl;

            _Ref_<RType> createKeyRType(const KeyType& key) const
            {
                // 尝试从已注册类型中获取
                auto keyRType = RType::Get(CachedTypeName<KeyType>());
                if (keyRType && keyRType->IsValid())
                {
                    keyRType->Attach(const_cast<KeyType&>(key));
                    return keyRType;
                }

                // 如果类型未注册，创建通用包装
                return createUnregisteredKeyTypeWrapper(key);
            }

            _Ref_<RType> createValueRType(const ValueType& value) const
            {
                // 尝试从已注册类型中获取
                auto valueRType = RType::Get(CachedTypeName<ValueType>());
                if (valueRType && valueRType->IsValid())
                {
                    valueRType->Attach(const_cast<ValueType&>(value));
                    return valueRType;
                }

                // 如果类型未注册，创建通用包装
                return createUnregisteredValueTypeWrapper(value);
            }

            _Ref_<RType> createUnregisteredKeyTypeWrapper(const KeyType& key) const
            {
                std::string typeName = CachedTypeName<KeyType>();

                auto typeInfo = RuntimeTypeParser::ParseType(typeName);
                if (typeInfo.isContainer)
                {
                    return createNestedContainerWrapper(key, typeInfo);
                }
                else
                {
                    return createSimpleTypeWrapper(key);
                }
            }

            _Ref_<RType> createUnregisteredValueTypeWrapper(const ValueType& value) const
            {
                std::string typeName = CachedTypeName<ValueType>();

                auto typeInfo = RuntimeTypeParser::ParseType(typeName);
                if (typeInfo.isContainer)
                {
                    return createNestedContainerWrapper(value, typeInfo);
                }
                else
                {
                    return createSimpleTypeWrapper(value);
                }
            }

            template <typename T>
            _Ref_<RType> createNestedContainerWrapper(const T& element,
                                                      const RuntimeTypeParser::TypeInfo& typeInfo) const
            {
                std::string typeName = CachedTypeName<T>();

                auto containerRType = RType::CreateType(
                    typeName,
                    typeInfo.isSequential
                        ? RTTMTypeBits::Sequence
                        : (typeInfo.isAssociative ? RTTMTypeBits::Associative : RTTMTypeBits::Variable),
                    std::type_index(typeid(T))
                );

                if (containerRType)
                {
                    containerRType->Attach(const_cast<T&>(element));
                }

                return containerRType;
            }

            template <typename T>
            _Ref_<RType> createSimpleTypeWrapper(const T& element) const
            {
                std::string typeName = CachedTypeName<T>();

                auto simpleRType = RType::CreateType(
                    typeName,
                    std::is_class_v<T> ? RTTMTypeBits::Class : RTTMTypeBits::Variable,
                    std::type_index(typeid(T))
                );

                if (simpleRType)
                {
                    simpleRType->Attach(const_cast<T&>(element));
                }

                return simpleRType;
            }

            KeyType extractKeyValue(const _Ref_<RType>& rtype) const
            {
                if (!rtype)
                    throw std::runtime_error("空的RType对象");

                try
                {
                    return rtype->As<KeyType>();
                }
                catch (...)
                {
                    if (rtype->GetRaw())
                    {
                        return *static_cast<KeyType*>(rtype->GetRaw());
                    }
                    throw std::runtime_error("无法提取键值");
                }
            }

            ValueType extractValueValue(const _Ref_<RType>& rtype) const
            {
                if (!rtype)
                    throw std::runtime_error("空的RType对象");

                try
                {
                    return rtype->As<ValueType>();
                }
                catch (...)
                {
                    if (rtype->GetRaw())
                    {
                        return *static_cast<ValueType*>(rtype->GetRaw());
                    }
                    throw std::runtime_error("无法提取值");
                }
            }
        };

        // 容器操作注册表 - 用于管理各种容器类型的操作函数
    class ContainerOperationRegistry
    {
    private:
        // 顺序容器操作函数类型定义
        using SizeOperation = std::function<size_t(void*)>;
        using ClearOperation = std::function<void(void*)>;
        using ElementOperation = std::function<void*(void*, size_t)>;
        using SetElementOperation = std::function<bool(void*, size_t, void*)>;
        using InsertOperation = std::function<bool(void*, size_t, void*)>;
        using RemoveOperation = std::function<bool(void*, size_t)>;
        using PushBackOperation = std::function<bool(void*, void*)>;
        using PushFrontOperation = std::function<bool(void*, void*)>;
        using PopBackOperation = std::function<bool(void*)>;
        using PopFrontOperation = std::function<bool(void*)>;

        // 顺序容器操作注册表
        std::unordered_map<std::string, SizeOperation> sizeOperations;
        std::unordered_map<std::string, ClearOperation> clearOperations;
        std::unordered_map<std::string, ElementOperation> elementOperations;
        std::unordered_map<std::string, SetElementOperation> setElementOperations;
        std::unordered_map<std::string, InsertOperation> insertOperations;
        std::unordered_map<std::string, RemoveOperation> removeOperations;
        std::unordered_map<std::string, PushBackOperation> pushBackOperations;
        std::unordered_map<std::string, PushFrontOperation> pushFrontOperations;
        std::unordered_map<std::string, PopBackOperation> popBackOperations;
        std::unordered_map<std::string, PopFrontOperation> popFrontOperations;

        // 元素类型信息缓存
        std::unordered_map<std::string, std::pair<std::string, std::string>> containerElementTypes;


        mutable std::shared_mutex operationsMutex;

    public:
        static ContainerOperationRegistry& Instance()
        {
            static ContainerOperationRegistry instance;
            return instance;
        }

        // 注册容器操作
        template <typename ContainerType>
        void RegisterContainerOperations()
        {
            std::string typeName = CachedTypeName<ContainerType>();

            std::unique_lock<std::shared_mutex> lock(operationsMutex);

            // 注册大小操作
            sizeOperations[typeName] = [](void* container) -> size_t
            {
                auto* cont = static_cast<ContainerType*>(container);
                return cont ? cont->size() : 0;
            };

            // 注册清空操作
            clearOperations[typeName] = [](void* container)
            {
                auto* cont = static_cast<ContainerType*>(container);
                if (cont) cont->clear();
            };

            // 特殊处理 std::vector<bool>
            if constexpr (std::is_same_v<ContainerType, std::vector<bool>>)
            {
                // std::vector<bool> 的特殊处理
                elementOperations[typeName] = [](void* container, size_t index) -> void*
                {
                    auto* cont = static_cast<std::vector<bool>*>(container);
                    if (cont && index < cont->size())
                    {
                        // 因为 vector<bool> 不能返回引用，我们需要特殊处理
                        static thread_local bool tempValue;
                        tempValue = (*cont)[index];
                        return &tempValue;
                    }
                    return nullptr;
                };

                setElementOperations[typeName] = [](void* container, size_t index, void* value) -> bool
                {
                    auto* cont = static_cast<std::vector<bool>*>(container);
                    if (cont && index < cont->size() && value)
                    {
                        (*cont)[index] = *static_cast<bool*>(value);
                        return true;
                    }
                    return false;
                };
            }
            else
            {
                // 根据C++版本注册元素访问操作
#if __cplusplus >= 202002L
        // C++20版本 - 使用requires表达式
        if constexpr (requires { std::declval<ContainerType>()[0]; })
        {
            // 支持随机访问的容器
            elementOperations[typeName] = [](void* container, size_t index) -> void*
            {
                auto* cont = static_cast<ContainerType*>(container);
                if (cont && index < cont->size())
                {
                    return &((*cont)[index]);
                }
                return nullptr;
            };

            setElementOperations[typeName] = [](void* container, size_t index, void* value) -> bool
            {
                auto* cont = static_cast<ContainerType*>(container);
                if (cont && index < cont->size() && value)
                {
                    (*cont)[index] = *static_cast<typename ContainerType::value_type*>(value);
                    return true;
                }
                return false;
            };
        }
        else
        {
            // 不支持随机访问的容器（如list）
            elementOperations[typeName] = [](void* container, size_t index) -> void*
            {
                auto* cont = static_cast<ContainerType*>(container);
                if (cont && index < cont->size())
                {
                    auto it = cont->begin();
                    std::advance(it, index);
                    return &(*it);
                }
                return nullptr;
            };

            setElementOperations[typeName] = [](void* container, size_t index, void* value) -> bool
            {
                auto* cont = static_cast<ContainerType*>(container);
                if (cont && index < cont->size() && value)
                {
                    auto it = cont->begin();
                    std::advance(it, index);
                    *it = *static_cast<typename ContainerType::value_type*>(value);
                    return true;
                }
                return false;
            };
        }
#else
                // C++17版本 - 使用SFINAE
                if constexpr (detail::has_subscript_operator<ContainerType>::value)
                {
                    // 支持随机访问的容器
                    elementOperations[typeName] = [](void* container, size_t index) -> void*
                    {
                        auto* cont = static_cast<ContainerType*>(container);
                        if (cont && index < cont->size())
                        {
                            return &((*cont)[index]);
                        }
                        return nullptr;
                    };

                    setElementOperations[typeName] = [](void* container, size_t index, void* value) -> bool
                    {
                        auto* cont = static_cast<ContainerType*>(container);
                        if (cont && index < cont->size() && value)
                        {
                            (*cont)[index] = *static_cast<typename ContainerType::value_type*>(value);
                            return true;
                        }
                        return false;
                    };
                }
                else
                {
                    // 不支持随机访问的容器（如list）
                    elementOperations[typeName] = [](void* container, size_t index) -> void*
                    {
                        auto* cont = static_cast<ContainerType*>(container);
                        if (cont && index < cont->size())
                        {
                            auto it = cont->begin();
                            std::advance(it, index);
                            return &(*it);
                        }
                        return nullptr;
                    };

                    setElementOperations[typeName] = [](void* container, size_t index, void* value) -> bool
                    {
                        auto* cont = static_cast<ContainerType*>(container);
                        if (cont && index < cont->size() && value)
                        {
                            auto it = cont->begin();
                            std::advance(it, index);
                            *it = *static_cast<typename ContainerType::value_type*>(value);
                            return true;
                        }
                        return false;
                    };
                }
#endif
            }

            // 注册插入操作
            insertOperations[typeName] = [](void* container, size_t index, void* value) -> bool
            {
                auto* cont = static_cast<ContainerType*>(container);
                if (cont && value)
                {
                    try
                    {
                        auto val = *static_cast<typename ContainerType::value_type*>(value);
                        if (index >= cont->size())
                        {
                            cont->insert(cont->end(), val);
                        }
                        else
                        {
                            auto it = cont->begin();
                            std::advance(it, index);
                            cont->insert(it, val);
                        }
                        return true;
                    }
                    catch (...)
                    {
                        return false;
                    }
                }
                return false;
            };

            // 注册删除操作
            removeOperations[typeName] = [](void* container, size_t index) -> bool
            {
                auto* cont = static_cast<ContainerType*>(container);
                if (cont && index < cont->size())
                {
                    try
                    {
                        auto it = cont->begin();
                        std::advance(it, index);
                        cont->erase(it);
                        return true;
                    }
                    catch (...)
                    {
                        return false;
                    }
                }
                return false;
            };

            // 根据C++版本注册push_back操作
#if __cplusplus >= 202002L
    // C++20版本
    if constexpr (requires
    {
        std::declval<ContainerType>().push_back(std::declval<typename ContainerType::value_type>());
    })
    {
        pushBackOperations[typeName] = [](void* container, void* value) -> bool
        {
            auto* cont = static_cast<ContainerType*>(container);
            if (cont && value)
            {
                try
                {
                    cont->push_back(*static_cast<typename ContainerType::value_type*>(value));
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            return false;
        };
    }
#else
            // C++17版本
            if constexpr (detail::has_push_back<ContainerType>::value)
            {
                pushBackOperations[typeName] = [](void* container, void* value) -> bool
                {
                    auto* cont = static_cast<ContainerType*>(container);
                    if (cont && value)
                    {
                        try
                        {
                            cont->push_back(*static_cast<typename ContainerType::value_type*>(value));
                            return true;
                        }
                        catch (...)
                        {
                            return false;
                        }
                    }
                    return false;
                };
            }
#endif

            // 根据C++版本注册push_front操作
#if __cplusplus >= 202002L
    // C++20版本
    if constexpr (requires
    {
        std::declval<ContainerType>().push_front(std::declval<typename ContainerType::value_type>());
    })
    {
        pushFrontOperations[typeName] = [](void* container, void* value) -> bool
        {
            auto* cont = static_cast<ContainerType*>(container);
            if (cont && value)
            {
                try
                {
                    cont->push_front(*static_cast<typename ContainerType::value_type*>(value));
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            return false;
        };
    }
#else
            // C++17版本
            if constexpr (detail::has_push_front<ContainerType>::value)
            {
                pushFrontOperations[typeName] = [](void* container, void* value) -> bool
                {
                    auto* cont = static_cast<ContainerType*>(container);
                    if (cont && value)
                    {
                        try
                        {
                            cont->push_front(*static_cast<typename ContainerType::value_type*>(value));
                            return true;
                        }
                        catch (...)
                        {
                            return false;
                        }
                    }
                    return false;
                };
            }
#endif

            // 根据C++版本注册pop_back操作
#if __cplusplus >= 202002L
    // C++20版本
    if constexpr (requires { std::declval<ContainerType>().pop_back(); })
    {
        popBackOperations[typeName] = [](void* container) -> bool
        {
            auto* cont = static_cast<ContainerType*>(container);
            if (cont && !cont->empty())
            {
                try
                {
                    cont->pop_back();
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            return false;
        };
    }
#else
            // C++17版本
            if constexpr (detail::has_pop_back<ContainerType>::value)
            {
                popBackOperations[typeName] = [](void* container) -> bool
                {
                    auto* cont = static_cast<ContainerType*>(container);
                    if (cont && !cont->empty())
                    {
                        try
                        {
                            cont->pop_back();
                            return true;
                        }
                        catch (...)
                        {
                            return false;
                        }
                    }
                    return false;
                };
            }
#endif

            // 根据C++版本注册pop_front操作
#if __cplusplus >= 202002L
    // C++20版本
    if constexpr (requires { std::declval<ContainerType>().pop_front(); })
    {
        popFrontOperations[typeName] = [](void* container) -> bool
        {
            auto* cont = static_cast<ContainerType*>(container);
            if (cont && !cont->empty())
            {
                try
                {
                    cont->pop_front();
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }
            return false;
        };
    }
#else
            // C++17版本
            if constexpr (detail::has_pop_front<ContainerType>::value)
            {
                popFrontOperations[typeName] = [](void* container) -> bool
                {
                    auto* cont = static_cast<ContainerType*>(container);
                    if (cont && !cont->empty())
                    {
                        try
                        {
                            cont->pop_front();
                            return true;
                        }
                        catch (...)
                        {
                            return false;
                        }
                    }
                    return false;
                };
            }
#endif
        }

        // 获取顺序容器操作函数
        SizeOperation GetSizeOperation(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = sizeOperations.find(typeName);
            return it != sizeOperations.end() ? it->second : nullptr;
        }

        ClearOperation GetClearOperation(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = clearOperations.find(typeName);
            return it != clearOperations.end() ? it->second : nullptr;
        }

        ElementOperation GetElementOperation(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = elementOperations.find(typeName);
            return it != elementOperations.end() ? it->second : nullptr;
        }

        SetElementOperation GetSetElementOperation(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = setElementOperations.find(typeName);
            return it != setElementOperations.end() ? it->second : nullptr;
        }

        InsertOperation GetInsertOperation(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = insertOperations.find(typeName);
            return it != insertOperations.end() ? it->second : nullptr;
        }

        RemoveOperation GetRemoveOperation(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = removeOperations.find(typeName);
            return it != removeOperations.end() ? it->second : nullptr;
        }

        PushBackOperation GetPushBackOperation(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = pushBackOperations.find(typeName);
            return it != pushBackOperations.end() ? it->second : nullptr;
        }

        PushFrontOperation GetPushFrontOperation(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = pushFrontOperations.find(typeName);
            return it != pushFrontOperations.end() ? it->second : nullptr;
        }

        PopBackOperation GetPopBackOperation(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = popBackOperations.find(typeName);
            return it != popBackOperations.end() ? it->second : nullptr;
        }

        PopFrontOperation GetPopFrontOperation(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = popFrontOperations.find(typeName);
            return it != popFrontOperations.end() ? it->second : nullptr;
        }


        // 获取容器的键值类型信息
        std::pair<std::string, std::string> GetContainerElementTypes(const std::string& typeName) const
        {
            std::shared_lock<std::shared_mutex> lock(operationsMutex);
            auto it = containerElementTypes.find(typeName);
            return it != containerElementTypes.end() ? it->second : std::make_pair("", "");
        }

        // 批量注册常用容器类型
        void RegisterCommonContainerTypes()
        {
            // 注册基本类型的常用容器
            RegisterContainerOperations<std::vector<int>>();
            RegisterContainerOperations<std::vector<float>>();
            RegisterContainerOperations<std::vector<double>>();
            RegisterContainerOperations<std::vector<std::string>>();
            RegisterContainerOperations<std::vector<bool>>();

            RegisterContainerOperations<std::list<int>>();
            RegisterContainerOperations<std::list<float>>();
            RegisterContainerOperations<std::list<double>>();
            RegisterContainerOperations<std::list<std::string>>();

            RegisterContainerOperations<std::deque<int>>();
            RegisterContainerOperations<std::deque<float>>();
            RegisterContainerOperations<std::deque<double>>();
            RegisterContainerOperations<std::deque<std::string>>();

            // 注册嵌套容器类型
            RegisterContainerOperations<std::vector<std::vector<int>>>();
            RegisterContainerOperations<std::vector<std::list<int>>>();
            RegisterContainerOperations<std::list<std::vector<int>>>();
            RegisterContainerOperations<std::vector<std::map<std::string, int>>>();
            RegisterContainerOperations<std::list<std::map<int, std::string>>>();

        }

        // 动态注册容器类型（用于自定义类型）
        template <typename ElementType>
        void RegisterContainerTypesForElement()
        {
            RegisterContainerOperations<std::vector<ElementType>>();
            RegisterContainerOperations<std::list<ElementType>>();
            RegisterContainerOperations<std::deque<ElementType>>();
        }
    };

        // 通用顺序容器包装器 - 使用类型擦除技术
        class GenericSequentialContainerWrapper : public IUniversalSequentialContainer
        {
        public:
            enum class ContainerType
            {
                Vector,
                List,
                Deque
            };

        private:
            void* containerPtr;
            std::string elementTypeName;
            std::string containerTypeName;
            ContainerType containerType;
            std::string fullTypeName;

            // 函数指针用于类型擦除操作
            std::function<size_t(void*)> sizeFunc;
            std::function<bool(void*)> emptyFunc;
            std::function<void(void*)> clearFunc;
            std::function<void*(void*, size_t)> getElementFunc;
            std::function<bool(void*, size_t, void*)> setElementFunc;
            std::function<bool(void*, size_t, void*)> insertElementFunc;
            std::function<bool(void*, size_t)> removeElementFunc;
            std::function<bool(void*, void*)> pushBackFunc;
            std::function<bool(void*, void*)> pushFrontFunc;
            std::function<bool(void*)> popBackFunc;
            std::function<bool(void*)> popFrontFunc;

        public:
            GenericSequentialContainerWrapper(void* container, const std::string& elementType,
                                              const std::string& containerName, ContainerType type,
                                              const std::string& fullType)
                : containerPtr(container)
                  , elementTypeName(elementType)
                  , containerTypeName(containerName + "<" + elementType + ">")
                  , containerType(type), fullTypeName(fullType)
            {
                InitializeFunctions();
            }

            size_t Size() const override
            {
                return sizeFunc ? sizeFunc(containerPtr) : 0;
            }

            bool Empty() const override
            {
                return emptyFunc ? emptyFunc(containerPtr) : true;
            }

            void Clear() override
            {
                if (clearFunc)
                {
                    clearFunc(containerPtr);
                }
            }

            std::string GetContainerTypeName() const override
            {
                return containerTypeName;
            }

            std::string GetElementTypeName() const override
            {
                return elementTypeName;
            }

            RTTMTypeBits GetContainerType() const
            {
                return RTTMTypeBits::Sequence;
            }

            void* GetRawPointer() const override
            {
                return containerPtr;
            }

            _Ref_<RType> GetElement(size_t index) const override
            {
                if (!getElementFunc)
                    return nullptr;

                void* elementPtr = getElementFunc(containerPtr, index);
                if (!elementPtr)
                    return nullptr;

                // 创建元素的RType包装
                auto elementRType = RType::Get(elementTypeName);
                if (!elementRType)
                {
                    // 如果类型未注册，创建通用包装
                    elementRType = RType::CreateType(
                        elementTypeName,
                        RTTMTypeBits::Variable,
                        std::type_index(typeid(void))
                    );
                }
                elementRType->Create();

                if (elementRType)
                {
                    // 直接设置原始指针，避免复制
                    elementRType->instance = _Ref_<char>(static_cast<char*>(elementPtr), [](char*)
                    {
                    });
                    elementRType->valid = true;
                }

                return elementRType;
            }

            bool SetElement(size_t index, const _Ref_<RType>& value) override
            {
                if (!setElementFunc || !value)
                    return false;

                void* valuePtr = value->GetRaw();
                if (!valuePtr)
                    return false;

                return setElementFunc(containerPtr, index, valuePtr);
            }

            bool InsertElement(size_t index, const _Ref_<RType>& value) override
            {
                if (!insertElementFunc || !value)
                    return false;

                void* valuePtr = value->GetRaw();
                if (!valuePtr)
                    return false;

                return insertElementFunc(containerPtr, index, valuePtr);
            }

            bool RemoveElement(size_t index) override
            {
                return removeElementFunc ? removeElementFunc(containerPtr, index) : false;
            }

            bool PushBack(const _Ref_<RType>& value) override
            {
                if (!pushBackFunc || !value)
                    return false;

                void* valuePtr = value->GetRaw();
                if (!valuePtr)
                    return false;

                return pushBackFunc(containerPtr, valuePtr);
            }

            bool PushFront(const _Ref_<RType>& value) override
            {
                if (!pushFrontFunc || !value)
                    return false;

                void* valuePtr = value->GetRaw();
                if (!valuePtr)
                    return false;

                return pushFrontFunc(containerPtr, valuePtr);
            }

            bool PopBack() override
            {
                return popBackFunc ? popBackFunc(containerPtr) : false;
            }

            bool PopFront() override
            {
                return popFrontFunc ? popFrontFunc(containerPtr) : false;
            }

            std::unique_ptr<IUniversalSequentialContainer::Iterator> CreateIterator() const override
            {
                // 实现通用迭代器
                return std::make_unique<GenericIterator>(this);
            }

        private:
            void InitializeFunctions()
            {
                // 根据容器类型和元素类型初始化函数指针
                // 这里使用运行时类型信息来创建类型擦除的操作函数

                // 获取元素类型的运行时信息
                const auto* elementTypeInfo = GlobalTypeManager::Instance().GetTypeInfo(elementTypeName);
                if (!elementTypeInfo)
                {
                    // 如果元素类型未注册，尝试注册基本操作
                    RegisterBasicElementOperations();
                }

                // 根据容器类型设置操作函数
                switch (containerType)
                {
                case ContainerType::Vector:
                    InitializeVectorFunctions();
                    break;
                case ContainerType::List:
                    InitializeListFunctions();
                    break;
                case ContainerType::Deque:
                    InitializeDequeFunctions();
                    break;
                }
            }

            void RegisterBasicElementOperations()
            {
                // 为未注册的元素类型注册基本操作
                // 这里假设元素类型支持默认构造、复制构造和析构
                RuntimeTypeParser::RegisterType<void>();
            }

            void InitializeVectorFunctions()
            {
                // 为vector类型初始化操作函数
                sizeFunc = [this](void* container) -> size_t
                {
                    // 使用类型擦除的方式获取容器大小
                    return GetGenericContainerSize(container, "vector");
                };

                emptyFunc = [this](void* container) -> bool
                {
                    return GetGenericContainerSize(container, "vector") == 0;
                };

                clearFunc = [this](void* container)
                {
                    ClearGenericContainer(container, "vector");
                };

                getElementFunc = [this](void* container, size_t index) -> void*
                {
                    return GetGenericContainerElement(container, index, "vector");
                };

                setElementFunc = [this](void* container, size_t index, void* value) -> bool
                {
                    return SetGenericContainerElement(container, index, value, "vector");
                };

                insertElementFunc = [this](void* container, size_t index, void* value) -> bool
                {
                    return InsertGenericContainerElement(container, index, value, "vector");
                };

                removeElementFunc = [this](void* container, size_t index) -> bool
                {
                    return RemoveGenericContainerElement(container, index, "vector");
                };

                pushBackFunc = [this](void* container, void* value) -> bool
                {
                    return PushBackGenericContainer(container, value, "vector");
                };

                pushFrontFunc = [this](void* container, void* value) -> bool
                {
                    return PushFrontGenericContainer(container, value, "vector");
                };

                popBackFunc = [this](void* container) -> bool
                {
                    return PopBackGenericContainer(container, "vector");
                };

                popFrontFunc = [this](void* container) -> bool
                {
                    return PopFrontGenericContainer(container, "vector");
                };
            }

            void InitializeListFunctions()
            {
                // 为list类型初始化操作函数
                sizeFunc = [this](void* container) -> size_t
                {
                    return GetGenericContainerSize(container, "list");
                };

                emptyFunc = [this](void* container) -> bool
                {
                    return GetGenericContainerSize(container, "list") == 0;
                };

                clearFunc = [this](void* container)
                {
                    ClearGenericContainer(container, "list");
                };

                getElementFunc = [this](void* container, size_t index) -> void*
                {
                    return GetGenericContainerElement(container, index, "list");
                };

                setElementFunc = [this](void* container, size_t index, void* value) -> bool
                {
                    return SetGenericContainerElement(container, index, value, "list");
                };

                insertElementFunc = [this](void* container, size_t index, void* value) -> bool
                {
                    return InsertGenericContainerElement(container, index, value, "list");
                };

                removeElementFunc = [this](void* container, size_t index) -> bool
                {
                    return RemoveGenericContainerElement(container, index, "list");
                };

                pushBackFunc = [this](void* container, void* value) -> bool
                {
                    return PushBackGenericContainer(container, value, "list");
                };

                pushFrontFunc = [this](void* container, void* value) -> bool
                {
                    return PushFrontGenericContainer(container, value, "list");
                };

                popBackFunc = [this](void* container) -> bool
                {
                    return PopBackGenericContainer(container, "list");
                };

                popFrontFunc = [this](void* container) -> bool
                {
                    return PopFrontGenericContainer(container, "list");
                };
            }

            void InitializeDequeFunctions()
            {
                // 为deque类型初始化操作函数
                sizeFunc = [this](void* container) -> size_t
                {
                    return GetGenericContainerSize(container, "deque");
                };

                emptyFunc = [this](void* container) -> bool
                {
                    return GetGenericContainerSize(container, "deque") == 0;
                };

                clearFunc = [this](void* container)
                {
                    ClearGenericContainer(container, "deque");
                };

                getElementFunc = [this](void* container, size_t index) -> void*
                {
                    return GetGenericContainerElement(container, index, "deque");
                };

                setElementFunc = [this](void* container, size_t index, void* value) -> bool
                {
                    return SetGenericContainerElement(container, index, value, "deque");
                };

                insertElementFunc = [this](void* container, size_t index, void* value) -> bool
                {
                    return InsertGenericContainerElement(container, index, value, "deque");
                };

                removeElementFunc = [this](void* container, size_t index) -> bool
                {
                    return RemoveGenericContainerElement(container, index, "deque");
                };

                pushBackFunc = [this](void* container, void* value) -> bool
                {
                    return PushBackGenericContainer(container, value, "deque");
                };

                pushFrontFunc = [this](void* container, void* value) -> bool
                {
                    return PushFrontGenericContainer(container, value, "deque");
                };

                popBackFunc = [this](void* container) -> bool
                {
                    return PopBackGenericContainer(container, "deque");
                };

                popFrontFunc = [this](void* container) -> bool
                {
                    return PopFrontGenericContainer(container, "deque");
                };
            }

            // 通用容器操作实现（使用函数指针注册机制）
            size_t GetGenericContainerSize(void* container, const std::string& containerType) const
            {
                auto sizeOperation = ContainerOperationRegistry::Instance().GetSizeOperation(fullTypeName);
                return sizeOperation ? sizeOperation(container) : 0;
            }

            void ClearGenericContainer(void* container, const std::string& containerType) const
            {
                auto clearOperation = ContainerOperationRegistry::Instance().GetClearOperation(fullTypeName);
                if (clearOperation)
                {
                    clearOperation(container);
                }
            }

            void* GetGenericContainerElement(void* container, size_t index, const std::string& containerType) const
            {
                auto getOperation = ContainerOperationRegistry::Instance().GetElementOperation(fullTypeName);
                return getOperation ? getOperation(container, index) : nullptr;
            }

            bool SetGenericContainerElement(void* container, size_t index, void* value,
                                            const std::string& containerType) const
            {
                auto setOperation = ContainerOperationRegistry::Instance().GetSetElementOperation(fullTypeName);
                return setOperation ? setOperation(container, index, value) : false;
            }

            bool InsertGenericContainerElement(void* container, size_t index, void* value,
                                               const std::string& containerType) const
            {
                auto insertOperation = ContainerOperationRegistry::Instance().GetInsertOperation(fullTypeName);
                return insertOperation ? insertOperation(container, index, value) : false;
            }

            bool RemoveGenericContainerElement(void* container, size_t index, const std::string& containerType) const
            {
                auto removeOperation = ContainerOperationRegistry::Instance().GetRemoveOperation(fullTypeName);
                return removeOperation ? removeOperation(container, index) : false;
            }

            bool PushBackGenericContainer(void* container, void* value, const std::string& containerType) const
            {
                auto pushBackOperation = ContainerOperationRegistry::Instance().GetPushBackOperation(fullTypeName);
                return pushBackOperation ? pushBackOperation(container, value) : false;
            }

            bool PushFrontGenericContainer(void* container, void* value, const std::string& containerType) const
            {
                auto pushFrontOperation = ContainerOperationRegistry::Instance().GetPushFrontOperation(fullTypeName);
                return pushFrontOperation ? pushFrontOperation(container, value) : false;
            }

            bool PopBackGenericContainer(void* container, const std::string& containerType) const
            {
                auto popBackOperation = ContainerOperationRegistry::Instance().GetPopBackOperation(fullTypeName);
                return popBackOperation ? popBackOperation(container) : false;
            }

            bool PopFrontGenericContainer(void* container, const std::string& containerType) const
            {
                auto popFrontOperation = ContainerOperationRegistry::Instance().GetPopFrontOperation(fullTypeName);
                return popFrontOperation ? popFrontOperation(container) : false;
            }

        public:
            // 通用迭代器实现
            class GenericIterator : public IUniversalSequentialContainer::Iterator
            {
            private:
                const GenericSequentialContainerWrapper* owner;
                size_t currentIndex;
                size_t containerSize;

            public:
                explicit GenericIterator(const GenericSequentialContainerWrapper* ownerPtr)
                    : owner(ownerPtr), currentIndex(0)
                {
                    containerSize = owner ? owner->Size() : 0;
                }

                _Ref_<RType> Current() const override
                {
                    if (!owner || currentIndex >= containerSize)
                        return nullptr;

                    return owner->GetElement(currentIndex);
                }

                bool Next() override
                {
                    if (currentIndex < containerSize)
                    {
                        ++currentIndex;
                        return true;
                    }
                    return false;
                }

                bool HasNext() const override
                {
                    return currentIndex < containerSize;
                }

                void Reset() override
                {
                    currentIndex = 0;
                    containerSize = owner ? owner->Size() : 0;
                }
            };
        };

    }

    template <typename T>
    void Registry_<T>::destructor() const
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

    template <typename T>
    void Registry_<T>::copier() const
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

    template <typename T>
    Registry_<T>::Registry_(): typeIndex(std::type_index(typeid(T))), type(RTTMTypeBits::Variable),
                               typeName(CachedTypeName<T>())
    {
        if (detail::GlobalTypeManager::Instance().IsTypeRegistered(typeName))
        {
            RTTM_DEBUG("RTTM: 类型已注册: " + typeName);
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
            type = RTTMTypeBits::Variable;
        }
        detail::TypeBaseInfo typeInfo;
        typeInfo.type = type;
        typeInfo.size = sizeof(T);
        typeInfo.typeIndex = typeIndex;

        detail::GlobalTypeManager::Instance().RegisterType(typeName, typeInfo);
        detail::RegisteredTypes.insert(typeName);

        if constexpr (std::is_class_v<T>)
        {
            destructor();
            constructor();
            copier();
        }
        RType::RegisterContainerSupportForType<T>();
    }

    template <typename T>
    template <typename U>
    Registry_<T>& Registry_<T>::base()
    {
        auto base = CachedTypeName<U>();
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

    template <typename T>
    template <typename... Args>
    Registry_<T>& Registry_<T>::constructor()
    {
        const static std::string cc_strType = CachedTypeName<const char*>();
        const static std::string strType = CachedTypeName<std::string>();
        const static std::string cwc_strType = CachedTypeName<const wchar_t*>();
        const static std::string wstrType = CachedTypeName<std::wstring>();

        std::string name = CachedTypeName<Args...>();
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

    template <typename T>
    template <typename U>
    typename std::enable_if_t<std::is_class_v<T>, Registry_<T>&>
    Registry_<T>::property(const char* name, U T::* value)
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
        else if constexpr (detail::is_sequential_container<U>())
        {
            member = {
                offset, RTTMTypeBits::Sequence, std::type_index(typeid(U)),
                CachedTypeName<typename U::value_type>()
            };
        }
        else if constexpr (detail::is_associative_container<U>())
        {
            member = {offset, RTTMTypeBits::Associative, std::type_index(typeid(U))};
            member.keyType = CachedTypeName<typename U::key_type>();
            member.valueType = CachedTypeName<typename U::mapped_type>();
        }
        else
        {
            member = {offset, RTTMTypeBits::Variable, std::type_index(typeid(U))};
        }

        auto& typeInfo = detail::GlobalTypeManager::Instance().GetMutableTypeInfo(typeName);
        typeInfo.members[name] = member;
        typeInfo.membersType[name] = CachedTypeName<U>();
        return *this;
    }

    template <typename T>
    template <typename R, typename... Args>
    typename std::enable_if_t<std::is_class_v<T>, Registry_<T>&>
    Registry_<T>::method(const std::string& name, R (T::*memFunc)(Args...))
    {
        std::string signature = name + CachedTypeName<Args...>();

        // 创建lambda表达式
        auto lambda = [memFunc](void* obj, Args... args) -> R
        {
            T* _obj = static_cast<T*>(obj);
            return (_obj->*memFunc)(std::forward<Args>(args)...);
        };

        // 先包装为std::function，然后传递给FunctionWrapper
        std::function<R(void*, Args...)> wrappedFunc = lambda;
        auto func = Create_Ref_<FunctionWrapper<R, void*, Args...>>(std::move(wrappedFunc));

        auto& typeInfo = detail::GlobalTypeManager::Instance().GetMutableTypeInfo(typeName);
        typeInfo.functions[signature] = func;
        return *this;
    }

    template <typename T>
    template <typename R, typename... Args>
    typename std::enable_if_t<std::is_class_v<T>, Registry_<T>&>
    Registry_<T>::method(const std::string& name, R (T::*memFunc)(Args...) const)
    {
        std::string signature = name + CachedTypeName<Args...>();

        // 创建lambda表达式
        auto lambda = [memFunc](void* obj, Args... args) -> R
        {
            const T* _obj = static_cast<const T*>(obj);
            return (_obj->*memFunc)(std::forward<Args>(args)...);
        };

        // 先包装为std::function，然后传递给FunctionWrapper
        std::function<R(void*, Args...)> wrappedFunc = lambda;
        auto func = Create_Ref_<FunctionWrapper<R, void*, Args...>>(std::move(wrappedFunc));

        auto& typeInfo = detail::GlobalTypeManager::Instance().GetMutableTypeInfo(typeName);
        typeInfo.functions[signature] = func;
        return *this;
    }

    template <typename... Args>
    constexpr size_t RType::getArgsHash() noexcept
    {
        if constexpr (sizeof...(Args) == 0)
        {
            return 0; // 默认构造函数的特殊哈希值
        }
        else
        {
            constexpr auto typeNames = CachedTypeName<Args...>();
            return std::hash<std::string_view>{}(typeNames);
        }
    }


    template <typename Value>
    auto RType::convertValueIfNeeded(const Value& value) const
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

    template <typename... Args>
    _Ref_<IFactory> RType::findFactoryOptimized() const
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
        std::string signature = CachedTypeName<Args...>();
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

    inline bool RType::checkValidOptimized() const
    {
        // 使用位操作优化布尔检查
        const bool isValidCached = validityCacheValid.load(std::memory_order_acquire);
        if (isValidCached)
        {
            const bool lastCheck = lastValidityCheck.load(std::memory_order_acquire);

            if (!lastCheck)
            {
                RTTM_DEBUG("RTTM: 对象状态无效");
                throw std::runtime_error("RTTM: 对象状态无效");
            }
            return lastCheck;
        }

        const bool isValid = valid && created;
        lastValidityCheck.store(isValid, std::memory_order_release);
        validityCacheValid.store(true, std::memory_order_release);

        if (!isValid)
        {
            if (!valid)
            {
                RTTM_DEBUG("RTTM: 结构未注册: " + type);
                throw std::runtime_error("RTTM: 结构未注册");
            }
            if (!created)
            {
                RTTM_DEBUG("RTTM: 对象未创建: " + type);
                throw std::runtime_error("RTTM: 对象未创建");
            }
        }
        return isValid;
    }

    inline void RType::copyFrom(const _Ref_<char>& newInst) const
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
    auto RType::getMethodImpl(const std::string& name) const
    {
        using traits = detail::function_traits<FuncType>;
        using ReturnType = typename traits::return_type;

        return getMethodImplHelper<ReturnType, typename traits::arguments>(name);
    }

    template <typename ReturnType, typename... Args>
    template <typename Self>
    auto RType::getMethodImplHelperStruct<ReturnType, std::tuple<Args...>>::call(Self* self, const std::string& name)
    {
        return self->template getMethodOrig<ReturnType, std::decay_t<Args>...>(name);
    }

    template <typename ReturnType, typename ArgsTuple>
    auto RType::getMethodImplHelper(const std::string& name) const
    {
        return getMethodImplHelperStruct<ReturnType, ArgsTuple>::call(this, name);
    }

    template <typename T, typename... Args>
    Method<T> RType::getMethodOrig(const std::string& name) const
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
            tn = name + CachedTypeName<Args...>();

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
                "函数签名不匹配: " + name + " 参数类型: " + CachedTypeName<Args...>());
        }

        return Method<T>(_f, name, static_cast<void*>(instance.get()), true);
    }

    inline void RType::preloadAllPropertiesOptimized() const
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

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateSequentialViewForType(
        const detail::RuntimeTypeParser::TypeInfo& typeInfo, void* containerPtr, const std::string& fullTypeName) const
    {
        const std::string& baseType = typeInfo.baseType;
        const auto& templateParams = typeInfo.templateParams;

        if (templateParams.empty())
            return nullptr;

        // 获取元素类型
        std::string elementType = templateParams[0];

        // 根据基类型和元素类型创建对应的容器视图
        if (baseType == "std::vector")
        {
            return CreateVectorView(elementType, containerPtr, fullTypeName);
        }
        else if (baseType == "std::list")
        {
            return CreateListView(elementType, containerPtr, fullTypeName);
        }
        else if (baseType == "std::deque")
        {
            return CreateDequeView(elementType, containerPtr, fullTypeName);
        }

        return nullptr;
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateVectorView(
        const std::string& elementType, void* containerPtr, const std::string& full) const
    {
        // 基本类型
        if (elementType == "int")
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::vector<int>>>(
                static_cast<std::vector<int>*>(containerPtr));
        else if (elementType == "float")
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::vector<float>>>(
                static_cast<std::vector<float>*>(containerPtr));
        else if (elementType == "double")
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::vector<double>>>(
                static_cast<std::vector<double>*>(containerPtr));
        else if (elementType == STRINGTYPE)
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::vector<std::string>>>(
                static_cast<std::vector<std::string>*>(containerPtr));
        else if (elementType == "bool")
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::vector<bool>>>(
                static_cast<std::vector<bool>*>(containerPtr));

        // 嵌套容器类型
        auto elementTypeInfo = detail::RuntimeTypeParser::ParseType(elementType);
        if (elementTypeInfo.isContainer)
        {
            return CreateNestedVectorView(elementTypeInfo, containerPtr, full);
        }

        // 自定义类型（通过动态注册）
        return CreateCustomTypeVectorView(elementType, containerPtr, full);
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateListView(const std::string& elementType,
        void* containerPtr, const std::string& full) const
    {
        // 基本类型
        if (elementType == "int")
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::list<int>>>(
                static_cast<std::list<int>*>(containerPtr));
        else if (elementType == "float")
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::list<float>>>(
                static_cast<std::list<float>*>(containerPtr));
        else if (elementType == "double")
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::list<double>>>(
                static_cast<std::list<double>*>(containerPtr));
        else if (elementType == STRINGTYPE)
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::list<std::string>>>(
                static_cast<std::list<std::string>*>(containerPtr));

        // 嵌套容器类型
        auto elementTypeInfo = detail::RuntimeTypeParser::ParseType(elementType);
        if (elementTypeInfo.isContainer)
        {
            return CreateNestedListView(elementTypeInfo, containerPtr);
        }

        // 自定义类型
        return CreateCustomTypeListView(elementType, containerPtr, full);
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateDequeView(const std::string& elementType,
        void* containerPtr, const std::string& full) const
    {
        // 基本类型
        if (elementType == "int")
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::deque<int>>>(
                static_cast<std::deque<int>*>(containerPtr));
        else if (elementType == "float")
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::deque<float>>>(
                static_cast<std::deque<float>*>(containerPtr));
        else if (elementType == "double")
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::deque<double>>>(
                static_cast<std::deque<double>*>(containerPtr));
        else if (elementType == STRINGTYPE)
            return std::make_unique<detail::UniversalSequentialContainerImpl<std::deque<std::string>>>(
                static_cast<std::deque<std::string>*>(containerPtr));

        // 嵌套容器类型
        auto elementTypeInfo = detail::RuntimeTypeParser::ParseType(elementType);
        if (elementTypeInfo.isContainer)
        {
            return CreateNestedDequeView(elementTypeInfo, containerPtr);
        }

        // 自定义类型
        return CreateCustomTypeDequeView(elementType, containerPtr, full);
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateNestedVectorView(
        const detail::RuntimeTypeParser::TypeInfo& elementTypeInfo, void* containerPtr, const std::string& full) const
    {
        const std::string& elementBaseType = elementTypeInfo.baseType;
        const auto& elementTemplateParams = elementTypeInfo.templateParams;

        if (elementBaseType == "std::vector" && !elementTemplateParams.empty())
        {
            const std::string& innerType = elementTemplateParams[0];

            if (innerType == "int")
                return std::make_unique<detail::UniversalSequentialContainerImpl<std::vector<std::vector<int>>>>(
                    static_cast<std::vector<std::vector<int>>*>(containerPtr));
            else if (innerType == STRINGTYPE)
                return std::make_unique<detail::UniversalSequentialContainerImpl<std::vector<std::vector<
                    std::string>>>>(static_cast<std::vector<std::vector<std::string>>*>(containerPtr));
        }
        else if (elementBaseType == "std::list" && !elementTemplateParams.empty())
        {
            const std::string& innerType = elementTemplateParams[0];

            if (innerType == "int")
                return std::make_unique<detail::UniversalSequentialContainerImpl<std::vector<std::list<int>>>>(
                    static_cast<std::vector<std::list<int>>*>(containerPtr));
            else if (innerType == STRINGTYPE)
                return std::make_unique<detail::UniversalSequentialContainerImpl<std::vector<std::list<std::string>>>>(
                    static_cast<std::vector<std::list<std::string>>*>(containerPtr));
        }
        else if (elementBaseType == "std::map" && elementTemplateParams.size() >= 2)
        {
            const std::string& keyType = elementTemplateParams[0];
            const std::string& valueType = elementTemplateParams[1];

            if (keyType == STRINGTYPE && valueType == "int")
                return std::make_unique<detail::UniversalSequentialContainerImpl<std::vector<std::map<
                    std::string, int>>>>(static_cast<std::vector<std::map<std::string, int>>*>(containerPtr));
        }

        return nullptr;
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateNestedListView(
        const detail::RuntimeTypeParser::TypeInfo& elementTypeInfo, void* containerPtr) const
    {
        const std::string& elementBaseType = elementTypeInfo.baseType;
        const auto& elementTemplateParams = elementTypeInfo.templateParams;

        if (elementBaseType == "std::vector" && !elementTemplateParams.empty())
        {
            const std::string& innerType = elementTemplateParams[0];

            if (innerType == "int")
                return std::make_unique<detail::UniversalSequentialContainerImpl<std::list<std::vector<int>>>>(
                    static_cast<std::list<std::vector<int>>*>(containerPtr));
        }
        else if (elementBaseType == "std::map" && elementTemplateParams.size() >= 2)
        {
            const std::string& keyType = elementTemplateParams[0];
            const std::string& valueType = elementTemplateParams[1];

            if (keyType == "int" && valueType == STRINGTYPE)
                return std::make_unique<detail::UniversalSequentialContainerImpl<std::list<std::map<
                    int, std::string>>>>(static_cast<std::list<std::map<int, std::string>>*>(containerPtr));
        }

        return nullptr;
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateNestedDequeView(
        const detail::RuntimeTypeParser::TypeInfo& elementTypeInfo, void* containerPtr) const
    {
        const std::string& elementBaseType = elementTypeInfo.baseType;
        const auto& elementTemplateParams = elementTypeInfo.templateParams;

        if (elementBaseType == "std::vector" && !elementTemplateParams.empty())
        {
            const std::string& innerType = elementTemplateParams[0];

            if (innerType == "int")
                return std::make_unique<detail::UniversalSequentialContainerImpl<std::deque<std::vector<int>>>>(
                    static_cast<std::deque<std::vector<int>>*>(containerPtr));
        }

        return nullptr;
    }


    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateCustomTypeVectorView(
        const std::string& elementType, void* containerPtr, const std::string& full) const
    {
        // 这里需要使用类型擦除技术来处理未知的自定义类型
        // 由于C++的模板系统限制，我们无法在运行时实例化任意类型的模板
        // 但可以通过注册机制来支持

        // 尝试从类型注册表中查找
        if (detail::GlobalTypeManager::Instance().IsTypeRegistered(elementType))
        {
            // 如果类型已注册，我们可以创建一个通用的包装器
            return CreateGenericVectorView(elementType, containerPtr, full);
        }

        return nullptr;
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateCustomTypeListView(
        const std::string& elementType, void* containerPtr, const std::string& full) const
    {
        if (detail::GlobalTypeManager::Instance().IsTypeRegistered(elementType))
        {
            return CreateGenericListView(elementType, containerPtr, full);
        }

        return nullptr;
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateCustomTypeDequeView(
        const std::string& elementType, void* containerPtr, const std::string& full) const
    {
        if (detail::GlobalTypeManager::Instance().IsTypeRegistered(elementType))
        {
            return CreateGenericDequeView(elementType, containerPtr, full);
        }

        return nullptr;
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateGenericVectorView(
        const std::string& elementType, void* containerPtr, const std::string& full) const
    {
        // 创建一个通用的容器包装器，使用类型擦除技术
        return std::make_unique<detail::GenericSequentialContainerWrapper>(
            containerPtr, elementType, "std::vector",
            detail::GenericSequentialContainerWrapper::ContainerType::Vector, full
        );
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateGenericListView(
        const std::string& elementType, void* containerPtr, const std::string& full) const
    {
        return std::make_unique<detail::GenericSequentialContainerWrapper>(
            containerPtr, elementType, "std::list",
            detail::GenericSequentialContainerWrapper::ContainerType::List, full
        );
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateGenericDequeView(
        const std::string& elementType, void* containerPtr, const std::string& full) const
    {
        return std::make_unique<detail::GenericSequentialContainerWrapper>(
            containerPtr, elementType, "std::deque",
            detail::GenericSequentialContainerWrapper::ContainerType::Deque, full
        );
    }

    inline int RType::CalculateNestingLevel(const std::string& typeName)
    {
        auto typeInfo = detail::RuntimeTypeParser::ParseType(typeName);
        if (!typeInfo.isContainer)
            return 0;

        int maxLevel = 0;
        for (const auto& param : typeInfo.templateParams)
        {
            int paramLevel = CalculateNestingLevel(param);
            maxLevel = std::max(maxLevel, paramLevel);
        }

        return maxLevel + 1;
    }

    inline void RType::InitializeUniversalContainerSystem()
    {
        // 注册常用容器操作
        detail::ContainerOperationRegistry::Instance().RegisterCommonContainerTypes();

        // 注册运行时类型解析器的基本类型
        detail::RuntimeTypeParser::RegisterType<int>();
        detail::RuntimeTypeParser::RegisterType<float>();
        detail::RuntimeTypeParser::RegisterType<double>();
        detail::RuntimeTypeParser::RegisterType<std::string>();
        detail::RuntimeTypeParser::RegisterType<bool>();
    }

    template <typename T>
    void RType::RegisterContainerSupportForType()
    {
        // 为类型T注册所有容器操作
        detail::ContainerOperationRegistry::Instance().RegisterContainerTypesForElement<T>();

        // 注册类型T本身
        detail::RuntimeTypeParser::RegisterType<T>();
    }

    inline bool RType::IsComplexNestedContainer() const
    {
        auto typeInfo = detail::RuntimeTypeParser::ParseType(GetTypeName());
        if (!typeInfo.isContainer)
            return false;

        // 检查模板参数是否包含容器类型
        for (const auto& param : typeInfo.templateParams)
        {
            auto paramInfo = detail::RuntimeTypeParser::ParseType(param);
            if (paramInfo.isContainer)
                return true;
        }

        return false;
    }

    inline int RType::GetNestingLevel() const
    {
        return CalculateNestingLevel(GetTypeName());
    }

    inline std::unique_ptr<detail::IUniversalSequentialContainer> RType::CreateUniversalSequentialView() const
    {
        if (!instance)
            return nullptr;

        std::string typeName = GetTypeName();
        auto typeInfo = detail::RuntimeTypeParser::ParseType(typeName);

        if (!typeInfo.isSequential)
            return nullptr;

        void* containerPtr = instance.get();

        // 使用模板展开来处理各种可能的容器类型
        return CreateSequentialViewForType(typeInfo, containerPtr, typeName);
    }

    inline RType::PropertyLookupCache::PropertyLookupCache() = default;

    inline RType::PropertyLookupCache::PropertyLookupCache(const PropertyLookupCache& other):
        lastPropertyName(other.lastPropertyName),
        offset(other.offset),
        valid(other.valid.load(std::memory_order_acquire))
    {
    }

    inline RType::PropertyLookupCache::PropertyLookupCache(PropertyLookupCache&& other) noexcept:
        lastPropertyName(std::move(other.lastPropertyName)),
        offset(other.offset),
        valid(other.valid.load(std::memory_order_acquire))
    {
    }

    inline RType::PropertyLookupCache& RType::PropertyLookupCache::operator=(const PropertyLookupCache& other)
    {
        if (this != &other)
        {
            lastPropertyName = other.lastPropertyName;
            offset = other.offset;
            valid.store(other.valid.load(std::memory_order_acquire), std::memory_order_release);
        }
        return *this;
    }

    inline RType::PropertyLookupCache& RType::PropertyLookupCache::operator=(PropertyLookupCache&& other) noexcept
    {
        if (this != &other)
        {
            lastPropertyName = std::move(other.lastPropertyName);
            offset = other.offset;
            valid.store(other.valid.load(std::memory_order_acquire), std::memory_order_release);
        }
        return *this;
    }

    inline RType::RType() = default;

    inline RType::RType(std::string _type): type(std::move(_type)), valid(!_type.empty())
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

    inline RType::RType(const std::string& _type, RTTMTypeBits _typeEnum, const std::type_index& _typeIndex,
                        const std::unordered_set<std::string>& _membersName,
                        const std::unordered_set<std::string>& _funcsName,
                        const _Ref_<char>& _instance):
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

    inline RType::RType(const RType& rtype) noexcept:
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

    inline _Ref_<RType> RType::CreateTypeOptimized(const std::string& type, RTTMTypeBits typeEnum,
                                                   const std::type_index& typeIndex,
                                                   const std::unordered_set<std::string>& membersName,
                                                   const std::unordered_set<std::string>& funcsName,
                                                   const _Ref_<char>& instance)
    {
        // 线程安全的静态类型缓存，使用智能锁策略
        static std::unordered_map<std::string, std::pair<
                                      std::unordered_set<std::string>, std::unordered_set<std::string>>> typeCache;
        static std::shared_mutex typeCacheMutex;
        auto rtypePtr = Create_Ref_<RType>(type, typeEnum, typeIndex, membersName, funcsName, instance);

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

    inline _Ref_<RType> RType::CreateType(const std::string& type, RTTMTypeBits typeEnum,
                                          const std::type_index& typeIndex,
                                          const std::unordered_set<std::string>& membersName,
                                          const std::unordered_set<std::string>& funcsName, const _Ref_<char>& instance)
    {
        return CreateTypeOptimized(type, typeEnum, typeIndex, membersName, funcsName, instance);
    }

    template <typename... Args>
    bool RType::Create(Args... args) const
    {
        if (!valid)
        {
            throw std::runtime_error("RTTM: 无效类型: " + type);
        }

        // 简化的工厂查找，去除多级缓存
        const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
        if (!typeInfo)
        {
            throw std::runtime_error("RTTM: 类型信息未找到: " + type);
        }

        std::string signature = CachedTypeName<Args...>();
        auto factoryIt = typeInfo->factories.find(signature);
        if (factoryIt == typeInfo->factories.end())
        {
            throw std::runtime_error("RTTM: 未注册工厂: " + type + " 参数: " + signature);
        }

        // 创建实例
        auto newInst = factoryIt->second->Create(std::forward<Args>(args)...);
        if (!newInst)
        {
            throw std::runtime_error("RTTM: 创建实例失败: " + type);
        }

        // 简单赋值，减少原子操作
        if (instance)
        {
            copyFrom(newInst);
        }
        else
        {
            instance = newInst;
        }

        created = true;
        return true;
    }

    template <typename... Args>
    std::vector<_Ref_<RType>> RType::CreateBatch(size_t count, Args... args) const
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
    void RType::Attach(T& inst) const
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
    R RType::Invoke(const std::string& name, Args... args) const
    {
        // 超高性能签名缓存 - 使用编译时优化
        static thread_local std::unordered_map<std::string, std::string> signatureCache;
        static constexpr size_t MAX_SIG_CACHE_SIZE = 512;
        static std::unordered_map<std::string, Method<R>> methodCache;

        if (methodCache.find(type + "::" + name) != methodCache.end())
        {
            return methodCache[type + "::" + name].Invoke(std::forward<Args>(args)...);
        }
        else
        {
            methodCache[type + "::" + name] = GetMethod<R(Args...)>(name);
            if (!methodCache[type + "::" + name].IsValid())
            {
                throw std::runtime_error("RTTM: 方法未找到: " + name + " 类型: " + type);
            }
            return methodCache[type + "::" + name].Invoke(std::forward<Args>(args)...);
        }

        /*std::string cacheKey = name + "|" + type;
        std::string tn;
        auto sigIt = signatureCache.find(cacheKey);
        if (sigIt != signatureCache.end())
        {
            tn = sigIt->second;
        }
        else
        {
            tn = name + CachedTypeName<Args...>();

            if (signatureCache.size() > MAX_SIG_CACHE_SIZE)
            {
                // 智能清理：保留最近使用的一半
                auto it = signatureCache.begin();
                std::advance(it, signatureCache.size() / 2);
                signatureCache.erase(it, signatureCache.end());
            }
            signatureCache[cacheKey] = tn;
        }

        if constexpr (!std::is_void_v<R>)
        {
            if (!checkValidOptimized<R>())
            {
                return R();
            }
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

        return funcPtr->operator()(static_cast<void*>(instance.get()), std::forward<Args>(args)...);*/
    }

    template <typename FuncType>
    auto RType::GetMethod(const std::string& name) const
    {
        return getMethodImpl<FuncType>(name);
    }

    template <typename T>
    T& RType::GetProperty(const std::string& name) const
    {
        if (!valid || !created)
        {
            throw std::runtime_error("RTTM: 无效对象访问: " + type);
        }

        // 简化的单级缓存
        thread_local std::unordered_map<std::string, size_t> offsetCache;

        auto it = offsetCache.find(type + "::" + name);
        if (it != offsetCache.end())
        {
            return *reinterpret_cast<T*>(instance.get() + it->second);
        }

        const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(type);
        if (!typeInfo)
        {
            throw std::runtime_error("RTTM: 类型信息未找到: " + type);
        }

        auto memberIt = typeInfo->members.find(name);
        if (memberIt == typeInfo->members.end())
        {
            throw std::runtime_error("RTTM: 成员未找到: " + name);
        }

        size_t offset = memberIt->second.offset;
        offsetCache[type + "::" + name] = offset;

        return *reinterpret_cast<T*>(instance.get() + offset);
    }

    inline _Ref_<RType> RType::GetProperty(const std::string& name) const
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

    inline std::unordered_map<std::string, _Ref_<RType>> RType::GetProperties() const
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
    bool RType::SetValue(const T& value) const
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

    inline bool RType::IsClass() const
    {
        return typeEnum == RTTMTypeBits::Class;
    }

    inline bool RType::IsEnum() const
    {
        return typeEnum == RTTMTypeBits::Enum;
    }

    inline bool RType::IsVariable() const
    {
        return typeEnum == RTTMTypeBits::Variable;
    }

    inline bool RType::IsValid() const
    {
        return checkValidOptimized();
    }

    template <typename T>
    bool RType::Is() const
    {
        static const std::type_index cachedTypeIndex = std::type_index(typeid(T));
        return typeIndex == cachedTypeIndex;
    }

    inline bool RType::IsPrimitiveType() const
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

    inline const std::unordered_set<std::string>& RType::GetPropertyNames() const
    {
        return membersName;
    }

    inline const std::unordered_set<std::string>& RType::GetMethodNames() const
    {
        return funcsName;
    }

    inline void* RType::GetRaw() const
    {
        return valid && created ? instance.get() : nullptr;
    }

    template <typename T>
    T& RType::As() const
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

    inline const std::string& RType::GetTypeName() const
    {
        return type;
    }


    inline _Ref_<RType> RType::Get(const std::string& typeName)
    {
        // 简化的单级缓存系统
        thread_local std::unordered_map<std::string, std::shared_ptr<RType>> tlsPrototypeCache;

        // 检查线程局部缓存
        auto it = tlsPrototypeCache.find(typeName);
        if (it != tlsPrototypeCache.end())
        {
            // 直接返回缓存的原型，不进行克隆
            return it->second;
        }

        // 缓存未命中，创建新的原型
        const auto* typeInfo = detail::GlobalTypeManager::Instance().GetTypeInfo(typeName);
        if (!typeInfo)
        {
            throw std::runtime_error("RTTM: 类型未注册: " + typeName);
        }

        // 预构建成员和函数名称集合（只在第一次构建）
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

        // 创建并缓存原型
        auto prototype = std::make_shared<RType>(
            typeName,
            typeInfo->type,
            typeInfo->typeIndex,
            std::move(membersName),
            std::move(funcsName),
            nullptr
        );

        tlsPrototypeCache[typeName] = prototype;
        return prototype;
    }

    template <typename T>
    _Ref_<RType> RType::Get()
    {
        return Get(CachedTypeName<T>());
    }

    inline void RType::Reset() const
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

    inline bool RType::IsSequentialContainer() const
    {
        return typeEnum == RTTMTypeBits::Sequence ||
            type.find("std::vector<") != std::string::npos ||
            type.find("vector<") != std::string::npos ||
            type.find("std::list<") != std::string::npos ||
            type.find("list<") != std::string::npos ||
            type.find("std::deque<") != std::string::npos ||
            type.find("deque<") != std::string::npos ||
            type.find("std::array<") != std::string::npos ||
            type.find("array<") != std::string::npos;
    }

    inline bool RType::IsAssociativeContainer() const
    {
        return typeEnum == RTTMTypeBits::Associative ||
            type.find("std::map<") != std::string::npos ||
            type.find("map<") != std::string::npos ||
            type.find("std::unordered_map<") != std::string::npos ||
            type.find("unordered_map<") != std::string::npos ||
            type.find("std::set<") != std::string::npos ||
            type.find("set<") != std::string::npos ||
            type.find("std::unordered_set<") != std::string::npos ||
            type.find("unordered_set<") != std::string::npos;
    }

    inline RType::~RType()
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
                    " 参数类型: " + CachedTypeName<Args...>() +
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
RTTM_REGISTRATION
{
    RTTM::Registry_<std::string>().constructor<std::string>().constructor<const char*>();
    RTTM::BasicRegister<int>();
    RTTM::BasicRegister<float>();
    RTTM::BasicRegister<double>();
    RTTM::BasicRegister<bool>();
    RTTM::BasicRegister<char>();
}
#endif //RTTM_H
