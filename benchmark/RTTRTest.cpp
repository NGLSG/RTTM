#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <rttr/registration>
#include <nlohmann/json.hpp>
#include "gtest/gtest.h"

#ifdef __linux__
#include <malloc.h>
#endif

using json = nlohmann::json;
using namespace rttr;

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

// Register all classes with RTTR
RTTR_REGISTRATION
{
    rttr::registration::class_<Vector3>("Vector3")
        .property("x", &Vector3::x)
        .property("y", &Vector3::y)
        .property("z", &Vector3::z);

    rttr::registration::class_<TestClass>("TestClass")
        .property("C", &TestClass::C)
        .property("D", &TestClass::D)
        .property("E", &TestClass::E)
        .property("F", &TestClass::F);

    rttr::registration::class_<NestedObject>("NestedObject")
        .property("name", &NestedObject::name)
        .property("value", &NestedObject::value)
        .property("inner", &NestedObject::inner)
        .property("numbers", &NestedObject::numbers);

    rttr::registration::class_<JsonSerializable>("JsonSerializable")
        .property("A", &JsonSerializable::A)
        .property("B", &JsonSerializable::B)
        .property("D", &JsonSerializable::D)
        .property("position", &JsonSerializable::position)
        .property("complex", &JsonSerializable::complex)
        .property("points", &JsonSerializable::points);
}

//-------------------------------------------------------
// Enhanced Serialization/Deserialization Functions for RTTR
//-------------------------------------------------------

// Forward declaration
json SerializeRTTR(const rttr::instance& obj);

// Main serialization function with type-specific optimizations
json SerializeRTTR(const rttr::instance& obj)
{
    json j;

    // Get the type of the instance
    rttr::type t = obj.get_type();

    // Handle fundamental types
    if (t.is_arithmetic())
    {
        rttr::variant var = obj.get_type();
        if (var.can_convert<int>()) return var.to_int();
        if (var.can_convert<float>()) return var.to_float();
        if (var.can_convert<double>()) return var.to_double();
        if (var.can_convert<bool>()) return var.to_bool();
        return j; // Empty JSON if no conversion possible
    }

    // Special handling for std::string
    if (t == rttr::type::get<std::string>())
    {
        rttr::variant var = obj.get_type();
        if (var.can_convert<std::string>())
            return var.to_string();
        return j;
    }

    // For std::vector - would implement specific handlers here

    // For complex objects, process properties
    for (auto& prop : t.get_properties())
    {
        rttr::variant prop_value = prop.get_value(obj);
        if (!prop_value)
            continue;

        const auto prop_name = prop.get_name().to_string();
        rttr::type prop_type = prop_value.get_type();

        // Handle fundamental types
        if (prop_type.is_arithmetic())
        {
            if (prop_value.can_convert<int>())
                j[prop_name] = prop_value.to_int();
            else if (prop_value.can_convert<float>())
                j[prop_name] = prop_value.to_float();
            else if (prop_value.can_convert<double>())
                j[prop_name] = prop_value.to_double();
            else if (prop_value.can_convert<bool>())
                j[prop_name] = prop_value.to_bool();
        }
        else if (prop_type == rttr::type::get<std::string>())
        {
            j[prop_name] = prop_value.to_string();
        }
        else if (prop_type == rttr::type::get<Vector3>())
        {
            Vector3 vec = prop_value.get_value<Vector3>();
            j[prop_name] = {{"x", vec.x}, {"y", vec.y}, {"z", vec.z}};
        }
        else if (prop_type.is_class())
        {
            j[prop_name] = SerializeRTTR(prop_value);
        }
        // Add more specific handlers for vectors and other containers as needed
    }

    return j;
}

// Forward declaration
bool DeserializeRTTR(rttr::instance obj, const json& js);

// Optimized deserialization function
bool DeserializeRTTR(rttr::instance obj, const json& js)
{
    if (!obj.is_valid())
        return false;

    rttr::type t = obj.get_type();

    // Process all properties
    for (auto& prop : t.get_properties())
    {
        const auto prop_name = prop.get_name().to_string();

        if (js.find(prop_name) == js.end())
            continue;

        const auto& json_value = js[prop_name];
        rttr::type prop_type = prop.get_type();

        // Create appropriate variant value based on property type
        rttr::variant var;

        if (prop_type == rttr::type::get<int>() && json_value.is_number_integer())
            var = json_value.get<int>();
        else if (prop_type == rttr::type::get<float>() && json_value.is_number_float())
            var = json_value.get<float>();
        else if (prop_type == rttr::type::get<double>() && json_value.is_number_float())
            var = json_value.get<double>();
        else if (prop_type == rttr::type::get<bool>() && json_value.is_boolean())
            var = json_value.get<bool>();
        else if (prop_type == rttr::type::get<std::string>() && json_value.is_string())
            var = json_value.get<std::string>();
        else if (prop_type == rttr::type::get<Vector3>() && json_value.is_object())
        {
            Vector3 vec;
            vec.x = json_value["x"].get<float>();
            vec.y = json_value["y"].get<float>();
            vec.z = json_value["z"].get<float>();
            var = vec;
        }
        else if (prop_type.is_class() && json_value.is_object())
        {
            // For nested objects, recursively deserialize
            rttr::variant nested_var = prop.get_value(obj);
            if (nested_var)
            {
                DeserializeRTTR(nested_var, json_value);
                var = nested_var;
            }
        }

        // Try to set the property value if we have a valid variant
        if (var.is_valid())
        {
            prop.set_value(obj, var);
        }
    }

    return true;
}

//-------------------------------------------------------
// Memory Utility - Same as in RTTM example
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
// Benchmark Utilities - Same as in RTTM example
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

// Initialize a complex object with test data - same as RTTM example
void InitializeComplexObject(JsonSerializable& obj)
{
    obj.A = 42;
    obj.B = "Complex RTTR Test";
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
// GTest Test Suite: Comprehensive Performance Benchmarks for RTTR
//-------------------------------------------------------

TEST(RTTR_PerformanceBenchmark, SingleObjectSerializationTest)
{
    Benchmark benchmark;
    const int iterations = 1000000; // 1M iterations

    // Create and initialize object
    JsonSerializable obj;
    InitializeComplexObject(obj);

    // RTTR instance
    rttr::instance rttr_obj = obj;

    // Benchmark single-threaded serialization
    json output;
    benchmark.run("Single-Object Serialization", [&]()
    {
        for (int i = 0; i < iterations; ++i)
        {
            output = SerializeRTTR(rttr_obj);
        }
    }, iterations, 1);

    // Verify serialization worked
    ASSERT_TRUE(output.contains("A"));
    EXPECT_EQ(output["A"], 42);
    EXPECT_EQ(output["B"], "Complex RTTR Test");

    // Benchmark single-threaded deserialization
    JsonSerializable deserObj;
    rttr::instance deserInstance = deserObj;

    benchmark.run("Single-Object Deserialization", [&]()
    {
        for (int i = 0; i < iterations; ++i)
        {
            DeserializeRTTR(deserInstance, output);
        }
    }, iterations, 1);

    // Verify deserialization worked
    EXPECT_EQ(deserObj.A, 42);
    EXPECT_EQ(deserObj.B, "Complex RTTR Test");

    benchmark.printResults();
}

TEST(RTTR_PerformanceBenchmark, PropertyAccessBenchmark)
{
    Benchmark benchmark;
    const int iterations = 10000000; // 10M iterations for property access

    // Create test object
    JsonSerializable obj;
    InitializeComplexObject(obj);
    rttr::instance rttr_obj = obj;

    // Benchmark direct property access
    int sum = 0;
    benchmark.run("Direct Property Access", [&]()
    {
        for (int i = 0; i < iterations; ++i)
        {
            sum += obj.A;
        }
    }, iterations, 1);

    // Benchmark reflection property access
    int reflectSum = 0;
    benchmark.run("Reflection Property Access", [&]()
    {
        rttr::property prop = rttr::type::get<JsonSerializable>().get_property("A");
        rttr::variant value = prop.get_value(rttr_obj);
        for (int i = 0; i < iterations; ++i)
        {

            reflectSum += value.to_int();
        }
    }, iterations, 1);

    // Benchmark nested property access
    float nestedSum = 0;
    benchmark.run("Nested Property Access", [&]()
    {
        rttr::property dProp = rttr::type::get<JsonSerializable>().get_property("D");
        rttr::property cProp = rttr::type::get<TestClass>().get_property("C");
        rttr::variant dValue = dProp.get_value(rttr_obj);
            rttr::variant cValue = cProp.get_value(dValue);
        for (int i = 0; i < iterations; ++i)
        {

            nestedSum += cValue.to_float();
        }
    }, iterations, 1);

    // Verify results
    EXPECT_EQ(sum, reflectSum);

    benchmark.printResults();
}

TEST(RTTR_PerformanceBenchmark, BatchProcessingTest)
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
                serialized.push_back(SerializeRTTR(obj));
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
                DeserializeRTTR(deserObjects[i], serialized[i]);
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

TEST(RTTR_PerformanceBenchmark, MultithreadedTest)
{
    Benchmark benchmark;
    const int threadCount = 8;
    const int iterationsPerThread = 200000;
    const int totalIterations = threadCount * iterationsPerThread;

    // Create a test object
    JsonSerializable baseObj;
    InitializeComplexObject(baseObj);

    // Prepare serialized version for deserialization test
    json serialized = SerializeRTTR(baseObj);

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
                    json output = SerializeRTTR(threadObj);
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
                    DeserializeRTTR(threadObj, serialized);
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
    std::cout << "=== RTTR Performance Benchmark ===\n";
    std::cout << "Testing RTTR reflection library implementation\n\n";

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
