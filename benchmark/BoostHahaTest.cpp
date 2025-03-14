#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <boost/hana.hpp>
#include <nlohmann/json.hpp>
#include "gtest/gtest.h"

#ifdef __linux__
#include <malloc.h>
#endif

using json = nlohmann::json;
namespace hana = boost::hana;

//-------------------------------------------------------
// Classes with Hana Adaptations - Same structure as RTTR example
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

// Boost.Hana adaptations for our classes
BOOST_HANA_ADAPT_STRUCT(Vector3, x, y, z);
BOOST_HANA_ADAPT_STRUCT(TestClass, C, D, E, F);
BOOST_HANA_ADAPT_STRUCT(NestedObject, numbers, name, value, inner);
BOOST_HANA_ADAPT_STRUCT(JsonSerializable, A, B, D, position, complex, points);

//-------------------------------------------------------
// Serialization/Deserialization Functions for Boost.Hana
//-------------------------------------------------------

// Forward declaration
class Serializer {
public:
    // Main serialization methods
    static json serialize(const int& value) {
        return value;
    }

    static json serialize(const float& value) {
        return value;
    }

    static json serialize(const double& value) {
        return value;
    }

    static json serialize(const bool& value) {
        return value;
    }

    static json serialize(const std::string& value) {
        return value;
    }

    // Vector serialization
    template<typename T>
    static json serialize(const std::vector<T>& vec) {
        json result = json::array();
        for (const auto& item : vec) {
            result.push_back(serialize(item));
        }
        return result;
    }

    // Hana struct serialization
    template<typename T>
    static auto serialize(const T& obj) ->
        std::enable_if_t<
            !std::is_arithmetic_v<T> &&
            !std::is_same_v<T, std::string> &&
            !hana::is_a<hana::string_tag, T>,
            json
        >
    {
        json result;

        hana::for_each(hana::accessors<T>(), [&](auto pair) {
            auto key = hana::to<const char*>(hana::first(pair));
            auto value = hana::second(pair)(obj);

            result[key] = serialize(value);
        });

        return result;
    }

    // Deserialization methods
    static bool deserialize(int& obj, const json& js) {
        if (js.is_number_integer()) {
            obj = js.get<int>();
            return true;
        }
        return false;
    }

    static bool deserialize(float& obj, const json& js) {
        if (js.is_number()) {
            obj = js.get<float>();
            return true;
        }
        return false;
    }

    static bool deserialize(double& obj, const json& js) {
        if (js.is_number()) {
            obj = js.get<double>();
            return true;
        }
        return false;
    }

    static bool deserialize(bool& obj, const json& js) {
        if (js.is_boolean()) {
            obj = js.get<bool>();
            return true;
        }
        return false;
    }

    static bool deserialize(std::string& obj, const json& js) {
        if (js.is_string()) {
            obj = js.get<std::string>();
            return true;
        }
        return false;
    }

    // Vector deserialization
    template<typename T>
    static bool deserialize(std::vector<T>& vec, const json& js) {
        if (!js.is_array()) return false;

        vec.clear();
        vec.reserve(js.size());

        for (const auto& item : js) {
            T value{};
            if (deserialize(value, item)) {
                vec.push_back(std::move(value));
            }
        }
        return true;
    }

    // Hana struct deserialization
    template<typename T>
    static auto deserialize(T& obj, const json& js) ->
        std::enable_if_t<
            !std::is_arithmetic_v<T> &&
            !std::is_same_v<T, std::string> &&
            !hana::is_a<hana::string_tag, T>,
            bool
        >
    {
        if (!js.is_object()) return false;

        hana::for_each(hana::accessors<T>(), [&](auto pair) {
            auto key = hana::to<const char*>(hana::first(pair));

            if (js.contains(key)) {
                auto& member = hana::second(pair)(obj);
                deserialize(member, js[key]);
            }
        });

        return true;
    }
};

//-------------------------------------------------------
// Memory Utility - Same as in RTTR example
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
// Benchmark Utilities - Same as in RTTR example
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

// Initialize a complex object with test data - same as RTTR example
void InitializeComplexObject(JsonSerializable& obj)
{
    obj.A = 42;
    obj.B = "Complex Hana Test";
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

    // Initialize vector if needed
    obj.complex.numbers = {1, 2, 3, 4, 5};

    obj.points.push_back({1.0f, 2.0f, 3.0f});
    obj.points.push_back({4.0f, 5.0f, 6.0f});
}

//-------------------------------------------------------
// GTest Test Suite: Comprehensive Performance Benchmarks for Boost.Hana
//-------------------------------------------------------

TEST(HANA_PerformanceBenchmark, SingleObjectSerializationTest)
{
    Benchmark benchmark;
    const int iterations = 1000000; // 1M iterations

    // Create and initialize object
    JsonSerializable obj;
    InitializeComplexObject(obj);

    // Benchmark single-threaded serialization
    json output;
    benchmark.run("Single-Object Serialization", [&]()
    {
        for (int i = 0; i < iterations; ++i)
        {
            output = Serializer::serialize(obj);
        }
    }, iterations, 1);

    // Verify serialization worked
    ASSERT_TRUE(output.contains("A"));
    EXPECT_EQ(output["A"], 42);
    EXPECT_EQ(output["B"], "Complex Hana Test");

    // Benchmark single-threaded deserialization
    JsonSerializable deserObj;

    benchmark.run("Single-Object Deserialization", [&]()
    {
        for (int i = 0; i < iterations; ++i)
        {
            Serializer::deserialize(deserObj, output);
        }
    }, iterations, 1);

    // Verify deserialization worked
    EXPECT_EQ(deserObj.A, 42);
    EXPECT_EQ(deserObj.B, "Complex Hana Test");

    benchmark.printResults();
}

TEST(HANA_PerformanceBenchmark, PropertyAccessBenchmark)
{
    Benchmark benchmark;
    const int iterations = 10000000; // 10M iterations for property access

    // Create test object
    JsonSerializable obj;
    InitializeComplexObject(obj);

    // Benchmark direct property access
    int sum = 0;
    benchmark.run("Direct Property Access", [&]()
    {
        for (int i = 0; i < iterations; ++i)
        {
            sum += obj.A;
        }
    }, iterations, 1);

    // Benchmark Hana property access
    int hanaSum = 0;
    benchmark.run("Hana Property Access", [&]()
    {
        auto accessor = hana::accessors<JsonSerializable>()[hana::size_c<0>];
        auto getter = hana::second(accessor);
        for (int i = 0; i < iterations; ++i)
        {
            hanaSum += getter(obj);
        }
    }, iterations, 1);

    // Benchmark nested property access
    float nestedSum = 0;
    benchmark.run("Nested Property Access", [&]()
    {
        auto dAccessor = hana::accessors<JsonSerializable>()[hana::size_c<2>];
        auto dGetter = hana::second(dAccessor);

        auto cAccessor = hana::accessors<TestClass>()[hana::size_c<0>];
        auto cGetter = hana::second(cAccessor);

        for (int i = 0; i < iterations; ++i)
        {
            auto d = dGetter(obj);
            nestedSum += cGetter(d);
        }
    }, iterations, 1);

    // Verify results
    EXPECT_EQ(sum, hanaSum);

    benchmark.printResults();
}

TEST(HANA_PerformanceBenchmark, BatchProcessingTest)
{
    Benchmark benchmark;
    const int objectCount = 1000;
    const int iterations = 1000; // Process the batch 1000 times

    // Create a batch of objects
    std::vector<JsonSerializable> objects(objectCount);

    for (int i = 0; i < objectCount; i++)
    {
        InitializeComplexObject(objects[i]);
        // Add some variation
        objects[i].A = i;
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
                serialized.push_back(Serializer::serialize(obj));
            }
        }
    }, iterations * objectCount, objectCount);

    // Benchmark batch deserialization
    std::vector<JsonSerializable> deserObjects(objectCount);

    benchmark.run("Batch Deserialization", [&]()
    {
        for (int iter = 0; iter < iterations; iter++)
        {
            for (int i = 0; i < objectCount; i++)
            {
                Serializer::deserialize(deserObjects[i], serialized[i]);
            }
        }
    }, iterations * objectCount, objectCount);

    // Verify results
    for (int i = 0; i < objectCount; i++)
    {
        EXPECT_EQ(deserObjects[i].A, i);
    }

    benchmark.printResults();
}

TEST(HANA_PerformanceBenchmark, MultithreadedTest)
{
    Benchmark benchmark;
    const int threadCount = 8;
    const int iterationsPerThread = 200000;
    const int totalIterations = threadCount * iterationsPerThread;

    // Create a test object
    JsonSerializable baseObj;
    InitializeComplexObject(baseObj);

    // Prepare serialized version for deserialization test
    json serialized = Serializer::serialize(baseObj);

    // Multithreaded serialization test
    benchmark.run("Multithreaded Serialization", [&]()
    {
        std::vector<std::thread> threads;
        std::atomic<int> completedThreads = 0;

        for (int t = 0; t < threadCount; t++)
        {
            threads.emplace_back([&, t]()
            {
                JsonSerializable threadObj;
                InitializeComplexObject(threadObj);
                threadObj.A = t; // Thread-specific value

                for (int i = 0; i < iterationsPerThread; i++)
                {
                    json output = Serializer::serialize(threadObj);
                }

                completedThreads++;
            });
        }

        for (auto& t : threads)
        {
            t.join();
        }

        EXPECT_EQ(completedThreads, threadCount);
    }, totalIterations, threadCount);

    // Multithreaded deserialization test
    benchmark.run("Multithreaded Deserialization", [&]()
    {
        std::vector<std::thread> threads;
        std::atomic<int> completedThreads = 0;

        for (int t = 0; t < threadCount; t++)
        {
            threads.emplace_back([&]()
            {
                JsonSerializable threadObj;

                for (int i = 0; i < iterationsPerThread; i++)
                {
                    Serializer::deserialize(threadObj, serialized);
                }

                completedThreads++;
            });
        }

        for (auto& t : threads)
        {
            t.join();
        }

        EXPECT_EQ(completedThreads, threadCount);
    }, totalIterations, threadCount);

    benchmark.printResults();
}

int main(int argc, char** argv)
{
    std::cout << "=== Boost.Hana Performance Benchmark ===\n";
    std::cout << "Testing Boost.Hana metaprogramming library implementation\n\n";

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}