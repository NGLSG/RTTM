#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <atomic>
#include "RTTM/RTTM.hpp"
#include <nlohmann/json.hpp>
#include "gtest/gtest.h"

#ifdef __linux__
#include <malloc.h>
#endif

using json = nlohmann::json;
using namespace RTTM;

//-------------------------------------------------------
// Reflection Classes and Registration with Varying Complexity
//-------------------------------------------------------

// Simple struct - one level
struct Vector3
{
    float x, y, z;
};

// Medium complexity - multiple primitive types
class TestClass
{
public:
    float C;
    int D;
    bool E;
    std::string F;
};

// More complex object with nested properties
class NestedObject
{
public:
    std::vector<int> numbers;
    std::string name;
    double value;
    TestClass inner;
};

// Top level class with various property types
class JsonSerializable
{
public:
    int A;
    std::string B;
    TestClass D;
    Vector3 position;
    NestedObject complex;
    std::vector<Vector3> points;
};

// Register all classes with RTTM
RTTM_REGISTRATION
{
    Registry_<Vector3>()
        .property("x", &Vector3::x)
        .property("y", &Vector3::y)
        .property("z", &Vector3::z);

    Registry_<TestClass>()
        .property("C", &TestClass::C)
        .property("D", &TestClass::D)
        .property("E", &TestClass::E)
        .property("F", &TestClass::F);

    Registry_<NestedObject>()
        .property("name", &NestedObject::name)
        .property("value", &NestedObject::value)
        .property("inner", &NestedObject::inner);

    Registry_<JsonSerializable>()
        .property("A", &JsonSerializable::A)
        .property("B", &JsonSerializable::B)
        .property("D", &JsonSerializable::D)
        .property("position", &JsonSerializable::position)
        .property("complex", &JsonSerializable::complex);
}

//-------------------------------------------------------
// Enhanced Serialization/Deserialization Functions
//-------------------------------------------------------

// Forward declaration
json SerializeInternal(const Ref<RType>& type);

// Handle vector serialization
template <typename T>
json SerializeVector(const std::vector<T>& vec)
{
    json array = json::array();
    for (const auto& item : vec)
    {
        if constexpr (std::is_same_v<T, int> ||
            std::is_same_v<T, float> ||
            std::is_same_v<T, double> ||
            std::is_same_v<T, bool>)
        {
            array.push_back(item);
        }
        else
        {
            // For complex types, create a RType and serialize
            auto itemType = RType::Get<T>();
            itemType->Attach(const_cast<T&>(item));
            array.push_back(SerializeInternal(itemType));
        }
    }
    return array;
}

// Main serialization function with type-specific optimizations
json SerializeInternal(const Ref<RType>& type)
{
    json j;

    // Fast path for primitive types
    if (type->IsPrimitiveType())
    {
        if (type->Is<int>()) return type->As<int>();
        if (type->Is<float>()) return type->As<float>();
        if (type->Is<double>()) return type->As<double>();
        if (type->Is<bool>()) return type->As<bool>();
        // Add more primitive types as needed
    }

    // Special handling for std::string
    if (type->Is<std::string>())
    {
        return type->As<std::string>();
    }

    // Handle containers
    // Note: In a real implementation we'd handle vectors and other containers here

    // For complex objects, process properties
    for (const auto& name : type->GetPropertyNames())
    {
        auto prop = type->GetProperty(name);

        // Primitive types
        if (prop->Is<int>())
        {
            j[name] = prop->As<int>();
        }
        else if (prop->Is<std::string>())
        {
            j[name] = prop->As<std::string>();
        }
        else if (prop->Is<float>())
        {
            j[name] = prop->As<float>();
        }
        else if (prop->Is<double>())
        {
            j[name] = prop->As<double>();
        }
        else if (prop->Is<bool>())
        {
            j[name] = prop->As<bool>();
        }
        else if (prop->Is<Vector3>())
        {
            auto& vec = prop->As<Vector3>();
            j[name] = {{"x", vec.x}, {"y", vec.y}, {"z", vec.z}};
        }
        else if (prop->IsClass())
        {
            j[name] = SerializeInternal(prop);
        }
        // Add more types as needed
    }
    return j;
}

// Public serialization function
json Serialize(const Ref<RType>& type)
{
    return SerializeInternal(type);
}

// Forward declaration
void DeserializeInternal(const Ref<RType>& tp, const json& js);

// Handle vector deserialization
template <typename T>
void DeserializeVector(std::vector<T>& vec, const json& jsonArray)
{
    vec.clear();
    vec.reserve(jsonArray.size());

    for (const auto& item : jsonArray)
    {
        if constexpr (std::is_same_v<T, int> ||
            std::is_same_v<T, float> ||
            std::is_same_v<T, double> ||
            std::is_same_v<T, bool>)
        {
            vec.push_back(item.get<T>());
        }
        else
        {
            T newItem;
            auto itemType = RType::Get<T>();
            itemType->Attach(newItem);
            DeserializeInternal(itemType, item);
            vec.push_back(newItem);
        }
    }
}

// Optimized deserialization function
void DeserializeInternal(const Ref<RType>& tp, const json& js)
{
    // Fast path for primitive types
    if (tp->IsPrimitiveType())
    {
        if (tp->Is<int>())
        {
            tp->SetValue(js.get<int>());
            return;
        }
        else if (tp->Is<float>())
        {
            tp->SetValue(js.get<float>());
            return;
        }
        else if (tp->Is<double>())
        {
            tp->SetValue(js.get<double>());
            return;
        }
        else if (tp->Is<bool>())
        {
            tp->SetValue(js.get<bool>());
            return;
        }
        // Handle more primitive types similarly
    }

    // Special handling for std::string
    if (tp->Is<std::string>())
    {
        tp->SetValue(js.get<std::string>());
        return;
    }

    // Handle std::vector and other containers

    // For complex objects, process all properties
    for (auto& name : tp->GetPropertyNames())
    {
        if (js.find(name) == js.end())
            continue;

        auto prop = tp->GetProperty(name);
        auto& value = js[name];

        if (prop->Is<int>())
            prop->SetValue(value.get<int>());
        else if (prop->Is<std::string>())
            prop->SetValue(value.get<std::string>());
        else if (prop->Is<float>())
            prop->SetValue(value.get<float>());
        else if (prop->Is<double>())
            prop->SetValue(value.get<double>());
        else if (prop->Is<bool>())
            prop->SetValue(value.get<bool>());
        else if (prop->Is<Vector3>())
        {
            Vector3& vec = prop->As<Vector3>();
            vec.x = value["x"].get<float>();
            vec.y = value["y"].get<float>();
            vec.z = value["z"].get<float>();
        }
        else if (prop->IsClass())
            DeserializeInternal(prop, value);
    }
}

// Public deserialization function
void Deserialize(const Ref<RType>& tp, const json& js)
{
    DeserializeInternal(tp, js);
}

//-------------------------------------------------------
// Memory Utility
//-------------------------------------------------------
#ifdef __linux__
size_t GetCurrentRSS() {
    struct mallinfo mi = mallinfo();
    return static_cast<size_t>(mi.uordblks);
}
#elif _WIN32
#include <windows.h>
#include <psapi.h>

size_t GetCurrentRSS()
{
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return static_cast<size_t>(pmc.WorkingSetSize);
    }
    return 0;
}
#else
size_t GetCurrentRSS() {
    return 0;
}
#endif

//-------------------------------------------------------
// Benchmark Utilities
//-------------------------------------------------------

class Benchmark
{
private:
    struct Result
    {
        std::string name;
        double timeMs;
        size_t memoryBytes;
        uint64_t iterations;
        uint64_t objectCount;

        double timePerIteration() const { return timeMs / iterations; }
        double memoryPerObject() const { return static_cast<double>(memoryBytes) / objectCount; }
    };

    std::vector<Result> results;
    std::mutex resultsMutex;

public:
    template <typename Func>
    void run(const std::string& name, Func&& func, uint64_t iterations, uint64_t objectCount)
    {
        // Warm-up phase
        for (int i = 0; i < 5; i++)
        {
            func();
        }

        // Actual measurement
        size_t memBefore = GetCurrentRSS();
        auto start = std::chrono::high_resolution_clock::now();

        func();

        auto end = std::chrono::high_resolution_clock::now();
        size_t memAfter = GetCurrentRSS();

        double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        size_t memoryDiff = (memAfter > memBefore) ? (memAfter - memBefore) : 0;

        Result r{name, elapsed, memoryDiff, iterations, objectCount};

        {
            std::lock_guard<std::mutex> lock(resultsMutex);
            results.push_back(r);
        }
    }

    void printResults()
    {
        std::cout << "\n=== BENCHMARK RESULTS ===\n";
        std::cout << std::left << std::setw(30) << "Test"
            << std::right << std::setw(15) << "Time (ms)"
            << std::setw(15) << "Time/Iter (ns)"
            << std::setw(15) << "Memory (KB)"
            << std::setw(15) << "Mem/Obj (B)"
            << std::endl;

        std::cout << std::string(90, '-') << std::endl;

        for (const auto& result : results)
        {
            std::cout << std::left << std::setw(30) << result.name
                << std::right << std::fixed << std::setprecision(3)
                << std::setw(15) << result.timeMs
                << std::setw(15) << (result.timePerIteration() * 1000000)
                << std::setw(15) << (result.memoryBytes / 1024.0)
                << std::setw(15) << result.memoryPerObject()
                << std::endl;
        }
    }
};

// Initialize a complex object with test data
void InitializeComplexObject(JsonSerializable& obj)
{
    obj.A = 42;
    obj.B = "Complex RTTM Test";
    obj.D.C = 3.14159f;
    obj.D.D = 65536;
    obj.D.E = true;
    obj.D.F = "Nested string";

    obj.position.x = 10.5f;
    obj.position.y = 20.7f;
    obj.position.z = 30.9f;

    obj.complex.name = "Deep nested object";
    obj.complex.value = 999.999;
    obj.complex.inner.C = 123.456f;
    obj.complex.inner.D = 789;
    obj.complex.inner.E = false;
    obj.complex.inner.F = "Very deep string";
}

//-------------------------------------------------------
// GTest Test Suite: Comprehensive Performance Benchmarks
//-------------------------------------------------------

TEST(RTTM_PerformanceBenchmark, SingleObjectSerializationTest)
{
    Benchmark benchmark;
    const int iterations = 1000000; // 1M iterations

    // Create and initialize object
    auto type = RType::Get<JsonSerializable>();
    type->Create();
    InitializeComplexObject(type->As<JsonSerializable>());

    // Benchmark single-threaded serialization
    json output;
    benchmark.run("Single-Object Serialization", [&]()
    {
        for (int i = 0; i < iterations; ++i)
        {
            output = Serialize(type);
        }
    }, iterations, 1);

    // Verify serialization worked
    ASSERT_TRUE(output.contains("A"));
    EXPECT_EQ(output["A"], 42);
    EXPECT_EQ(output["B"], "Complex RTTM Test");

    // Benchmark single-threaded deserialization
    auto deserType = RType::Get<JsonSerializable>();
    deserType->Create();

    benchmark.run("Single-Object Deserialization", [&]()
    {
        for (int i = 0; i < iterations; ++i)
        {
            Deserialize(deserType, output);
        }
    }, iterations, 1);

    // Verify deserialization worked
    EXPECT_EQ(deserType->As<JsonSerializable>().A, 42);
    EXPECT_EQ(deserType->As<JsonSerializable>().B, "Complex RTTM Test");

    benchmark.printResults();
}

TEST(RTTM_PerformanceBenchmark, PropertyAccessBenchmark)
{
    Benchmark benchmark;
    const int iterations = 10000000; // 10M iterations for property access

    // Create test object
    auto type = RType::Get<JsonSerializable>();
    type->Create();
    InitializeComplexObject(type->As<JsonSerializable>());

    // Benchmark direct property access
    int sum = 0;
    benchmark.run("Direct Property Access", [&]()
    {
        auto obj = type->As<JsonSerializable>();
        for (int i = 0; i < iterations; ++i)
        {
            sum += obj.A;
        }
    }, iterations, 1);

    // Benchmark reflection property access
    int reflectSum = 0;
    benchmark.run("Reflection Property Access", [&]()
    {
        auto _A = type->GetProperty("A");
        for (int i = 0; i < iterations; ++i)
        {
            reflectSum += _A->As<int>();
        }
    }, iterations, 1);

    // Benchmark nested property access
    float nestedSum = 0;
    benchmark.run("Nested Property Access", [&]()
    {
        auto _C = type->GetProperty("D")->GetProperty("C");
        for (int i = 0; i < iterations; ++i)
        {
            nestedSum += _C->As<float>();
        }
    }, iterations, 1);

    // Verify results
    EXPECT_EQ(sum, reflectSum);

    benchmark.printResults();
}

TEST(RTTM_PerformanceBenchmark, BatchProcessingTest)
{
    Benchmark benchmark;
    const int objectCount = 1000;
    const int iterations = 1000; // Process the batch 1000 times

    // Create a batch of objects
    std::vector<Ref<RType>> objects;
    objects.reserve(objectCount);

    for (int i = 0; i < objectCount; i++)
    {
        auto obj = RType::Get<JsonSerializable>();
        obj->Create();
        InitializeComplexObject(obj->As<JsonSerializable>());
        // Add some variation
        obj->As<JsonSerializable>().A = i;
        objects.push_back(obj);
    }

    // Benchmark batch serialization
    std::vector<json> serialized;
    serialized.reserve(objectCount);

    benchmark.run("Batch Serialization", [&]()
    {
        for (int iter = 0; iter < iterations; iter++)
        {
            serialized.clear();
            for (const auto& obj : objects)
            {
                serialized.push_back(Serialize(obj));
            }
        }
    }, iterations * objectCount, objectCount);

    // Benchmark batch deserialization
    std::vector<Ref<RType>> deserObjects;
    deserObjects.reserve(objectCount);

    for (int i = 0; i < objectCount; i++)
    {
        auto obj = RType::Get<JsonSerializable>();
        obj->Create();
        deserObjects.push_back(obj);
    }

    benchmark.run("Batch Deserialization", [&]()
    {
        for (int iter = 0; iter < iterations; iter++)
        {
            for (int i = 0; i < objectCount; i++)
            {
                Deserialize(deserObjects[i], serialized[i]);
            }
        }
    }, iterations * objectCount, objectCount);

    // Verify results
    for (int i = 0; i < objectCount; i++)
    {
        EXPECT_EQ(deserObjects[i]->As<JsonSerializable>().A, i);
    }

    benchmark.printResults();
}


TEST(RTTM_PerformanceBenchmark, MultithreadedTest)
{
    Benchmark benchmark;
    const int threadCount = 8;
    // 减少迭代次数，先确认稳定性
    const int iterationsPerThread = 200000;
    const int totalIterations = threadCount * iterationsPerThread;

    // 测试根对象
    auto baseType = RType::Get<JsonSerializable>();
    baseType->Create();
    InitializeComplexObject(baseType->As<JsonSerializable>());

    json serialized = Serialize(baseType);

    // 多线程序列化测试
    benchmark.run("Multithreaded Serialization", [&]()
    {
        std::vector<std::thread> threads;
        std::atomic<int> completedThreads(0);

        for (int t = 0; t < threadCount; t++)
        {
            // 使用 try-catch 捕获异常，以便定位问题
            threads.emplace_back([t, iterationsPerThread, &completedThreads]()
            {
                try
                {
                    // 每个线程创建自己的对象，避免共享
                    auto threadType = RType::Get<JsonSerializable>();
                    threadType->Create();
                    auto& obj = threadType->As<JsonSerializable>();

                    // 初始化线程特定数据
                    obj.A = t + 100;
                    obj.B = "Thread " + std::to_string(t);
                    obj.D.C = static_cast<float>(t) * 1.5f;
                    obj.D.D = t * 10;
                    obj.D.E = (t % 2 == 0);
                    obj.position.x = static_cast<float>(t);
                    obj.position.y = static_cast<float>(t) * 2.0f;
                    obj.position.z = static_cast<float>(t) * 3.0f;

                    for (int i = 0; i < iterationsPerThread; i++)
                    {
                        json localOutput = Serialize(threadType);
                    }

                    completedThreads.fetch_add(1, std::memory_order_relaxed);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Thread " << t << " exception: " << e.what() << std::endl;
                }
            });
        }

        for (auto& t : threads)
        {
            if (t.joinable()) t.join();
        }

        EXPECT_EQ(completedThreads.load(), threadCount);
    }, totalIterations, threadCount);

    // 多线程反序列化测试
    benchmark.run("Multithreaded Deserialization", [&]()
    {
        std::vector<std::thread> threads;
        std::atomic<int> completedThreads(0);

        for (int t = 0; t < threadCount; t++)
        {
            threads.emplace_back([t, iterationsPerThread, &completedThreads, serializedCopy = serialized]()
            {
                try
                {
                    // 线程本地对象
                    auto threadType = RType::Get<JsonSerializable>();
                    threadType->Create();

                    for (int i = 0; i < iterationsPerThread; i++)
                    {
                        Deserialize(threadType, serializedCopy);
                    }

                    completedThreads.fetch_add(1, std::memory_order_relaxed);
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Thread " << t << " exception: " << e.what() << std::endl;
                }
            });
        }

        for (auto& t : threads)
        {
            if (t.joinable()) t.join();
        }

        EXPECT_EQ(completedThreads.load(), threadCount);
    }, totalIterations, threadCount);

    benchmark.printResults();
}

int main(int argc, char** argv)
{
    std::cout << "=== RTTM Performance Benchmark ===\n";
    std::cout << "Testing optimized RTTM implementation with memory pooling\n\n";

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
