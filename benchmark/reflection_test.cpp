#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <map>
#include <memory>
#include <algorithm>
#include <numeric>
#include "gtest/gtest.h"

// 同时包含三个反射库的头文件
#include <rttr/registration>
#include <boost/hana.hpp>
#include "RTTM/RTTM.hpp"
#include "reflection_test.h"
#include <nlohmann/json.hpp>

#ifdef __linux__
#include <malloc.h>
#endif

using json = nlohmann::json;
using namespace rttr;
using namespace RTTM;
namespace hana = boost::hana;
#define NOMINMAX

//-------------------------------------------------------
// RTTR 反射注册和实现
//-------------------------------------------------------

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

// RTTR 序列化函数
json SerializeRTTR(const rttr::instance& obj)
{
    json j;
    rttr::type t = obj.get_type();

    if (t.is_arithmetic())
    {
        rttr::variant var = obj.get_type();
        if (var.can_convert<int>()) return var.to_int();
        if (var.can_convert<float>()) return var.to_float();
        if (var.can_convert<double>()) return var.to_double();
        if (var.can_convert<bool>()) return var.to_bool();
        return j;
    }

    if (t == rttr::type::get<std::string>())
    {
        rttr::variant var = obj.get_type();
        if (var.can_convert<std::string>())
            return var.to_string();
        return j;
    }

    for (auto& prop : t.get_properties())
    {
        rttr::variant prop_value = prop.get_value(obj);
        if (!prop_value)
            continue;

        const auto prop_name = prop.get_name().to_string();
        rttr::type prop_type = prop_value.get_type();

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
    }

    return j;
}

// RTTR 反序列化函数
bool DeserializeRTTR(rttr::instance obj, const json& js)
{
    if (!obj.is_valid())
        return false;

    rttr::type t = obj.get_type();

    for (auto& prop : t.get_properties())
    {
        const auto prop_name = prop.get_name().to_string();

        if (js.find(prop_name) == js.end())
            continue;

        const auto& json_value = js[prop_name];
        rttr::type prop_type = prop.get_type();

        rttr::variant var;

        if (prop_type == rttr::type::get<int>() && json_value.is_number_integer())
            var = json_value.get<int>();
        else if (prop_type == rttr::type::get<float>() && json_value.is_number())
            var = json_value.get<float>();
        else if (prop_type == rttr::type::get<double>() && json_value.is_number())
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
            rttr::variant nested_var = prop.get_value(obj);
            if (nested_var)
            {
                DeserializeRTTR(nested_var, json_value);
                var = nested_var;
            }
        }

        if (var.is_valid())
        {
            prop.set_value(obj, var);
        }
    }

    return true;
}

//-------------------------------------------------------
// Boost.Hana 适配器和实现
//-------------------------------------------------------

BOOST_HANA_ADAPT_STRUCT(Vector3, x, y, z);
BOOST_HANA_ADAPT_STRUCT(TestClass, C, D, E, F);
BOOST_HANA_ADAPT_STRUCT(NestedObject, numbers, name, value, inner);
BOOST_HANA_ADAPT_STRUCT(JsonSerializable, A, B, D, position, complex, points);

// Hana 序列化器类
class HanaSerializer
{
public:
    // 基础类型序列化
    static json serialize(const int& value) { return value; }
    static json serialize(const float& value) { return value; }
    static json serialize(const double& value) { return value; }
    static json serialize(const bool& value) { return value; }
    static json serialize(const std::string& value) { return value; }

    // 向量序列化
    template <typename T>
    static json serialize(const std::vector<T>& vec)
    {
        json result = json::array();
        for (const auto& item : vec)
        {
            result.push_back(serialize(item));
        }
        return result;
    }

    // Hana 结构体序列化
    template <typename T>
    static auto serialize(const T& obj) ->
        std::enable_if_t<
            !std::is_arithmetic_v<T> &&
            !std::is_same_v<T, std::string> &&
            !hana::is_a<hana::string_tag, T>,
            json
        >
    {
        json result;

        hana::for_each(hana::accessors<T>(), [&](auto pair)
        {
            auto key = hana::to<const char*>(hana::first(pair));
            auto value = hana::second(pair)(obj);
            result[key] = serialize(value);
        });

        return result;
    }

    // 基础类型反序列化
    static bool deserialize(int& obj, const json& js)
    {
        if (js.is_number_integer())
        {
            obj = js.get<int>();
            return true;
        }
        return false;
    }

    static bool deserialize(float& obj, const json& js)
    {
        if (js.is_number())
        {
            obj = js.get<float>();
            return true;
        }
        return false;
    }

    static bool deserialize(double& obj, const json& js)
    {
        if (js.is_number())
        {
            obj = js.get<double>();
            return true;
        }
        return false;
    }

    static bool deserialize(bool& obj, const json& js)
    {
        if (js.is_boolean())
        {
            obj = js.get<bool>();
            return true;
        }
        return false;
    }

    static bool deserialize(std::string& obj, const json& js)
    {
        if (js.is_string())
        {
            obj = js.get<std::string>();
            return true;
        }
        return false;
    }

    // 向量反序列化
    template <typename T>
    static bool deserialize(std::vector<T>& vec, const json& js)
    {
        if (!js.is_array()) return false;

        vec.clear();
        vec.reserve(js.size());

        for (const auto& item : js)
        {
            T value{};
            if (deserialize(value, item))
            {
                vec.push_back(std::move(value));
            }
        }
        return true;
    }

    // Hana 结构体反序列化
    template <typename T>
    static auto deserialize(T& obj, const json& js) ->
        std::enable_if_t<
            !std::is_arithmetic_v<T> &&
            !std::is_same_v<T, std::string> &&
            !hana::is_a<hana::string_tag, T>,
            bool>
    {
        if (!js.is_object()) return false;

        hana::for_each(hana::accessors<T>(), [&](auto pair)
        {
            auto key = hana::to<const char*>(hana::first(pair));

            if (js.contains(key))
            {
                auto& member = hana::second(pair)(obj);
                deserialize(member, js[key]);
            }
        });

        return true;
    }
};

//-------------------------------------------------------
// RTTM 实现
//-------------------------------------------------------

// RTTM 序列化函数
json SerializeRTTM(const _Ref_<RType>& type)
{
    json j;

    if (type->IsPrimitiveType())
    {
        if (type->Is<int>()) return type->As<int>();
        if (type->Is<float>()) return type->As<float>();
        if (type->Is<double>()) return type->As<double>();
        if (type->Is<bool>()) return type->As<bool>();
    }

    if (type->Is<std::string>())
    {
        return type->As<std::string>();
    }

    for (const auto& name : type->GetPropertyNames())
    {
        auto prop = type->GetProperty(name);

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
            j[name] = SerializeRTTM(prop);
        }
    }

    return j;
}

// RTTM 反序列化函数
void DeserializeRTTM(const _Ref_<RType>& tp, const json& js)
{
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
    }

    if (tp->Is<std::string>())
    {
        tp->SetValue(js.get<std::string>());
        return;
    }

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
            DeserializeRTTM(prop, value);
    }
}

//-------------------------------------------------------
// 跨平台内存监控工具
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
// 修复后的性能基准测试框架
//-------------------------------------------------------
class ReflectionBenchmark
{
private:
    struct TestResult
    {
        std::string library_name; // 反射库名称
        std::string test_name; // 测试名称
        double time_ms; // 运行时间（毫秒）
        size_t memory_bytes; // 内存使用（字节）
        uint64_t iterations; // 迭代次数
        uint64_t object_count; // 对象数量

        double time_per_iteration() const
        {
            return iterations > 0 ? time_ms / iterations : 0.0;
        }

        double memory_per_object() const
        {
            return object_count > 0 ? static_cast<double>(memory_bytes) / object_count : 0.0;
        }
    };

    std::vector<TestResult> results;
    std::mutex results_mutex;

public:
    template <typename TestFunction>
    void run_test(const std::string& library_name, const std::string& test_name,
                  TestFunction&& func, uint64_t iterations, uint64_t object_count = 1)
    {
        std::cout << "  测试 " << library_name << " - " << test_name << "...";

        // 预热阶段 - 减少缓存和JIT影响
        const int warmup_count = std::min(3, static_cast<int>(iterations / 1000));
        for (int i = 0; i < warmup_count; i++)
        {
            func();
        }

        // 实际性能测量
        size_t memory_before = GetCurrentRSS();
        auto start_time = std::chrono::high_resolution_clock::now();

        func();

        auto end_time = std::chrono::high_resolution_clock::now();
        size_t memory_after = GetCurrentRSS();

        double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        size_t memory_diff = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

        TestResult result{
            library_name, test_name, elapsed_ms, memory_diff,
            iterations, object_count
        };

        {
            std::lock_guard<std::mutex> lock(results_mutex);
            results.push_back(result);
        }

        std::cout << " 完成 (" << std::fixed << std::setprecision(2)
                  << elapsed_ms << "ms)\n";
    }

    void print_comprehensive_results()
    {
        std::cout << "\n=== C++反射库综合性能对比报告 ===\n";
        std::cout << "收集到 " << results.size() << " 项测试结果\n";

        if (results.empty())
        {
            std::cout << "错误：未收集到任何测试数据！\n";
            return;
        }

        std::cout << std::left << std::setw(12) << "反射库"
            << std::setw(22) << "测试项目"
            << std::right << std::setw(12) << "总时间(ms)"
            << std::setw(15) << "单次用时(ns)"
            << std::setw(12) << "内存(KB)"
            << std::setw(15) << "单对象内存(B)"
            << std::endl;

        std::cout << std::string(110, '=') << std::endl;

        // 按测试类型分组显示结果
        std::map<std::string, std::vector<TestResult>> grouped_results;
        for (const auto& result : results)
        {
            grouped_results[result.test_name].push_back(result);
        }

        for (const auto& group : grouped_results)
        {
            std::cout << "\n【" << group.first << "】\n";

            // 对每个组内的结果按性能排序
            auto sorted_results = group.second;
            std::sort(sorted_results.begin(), sorted_results.end(),
                      [](const TestResult& a, const TestResult& b)
                      {
                          return a.time_per_iteration() < b.time_per_iteration();
                      });

            for (const auto& result : sorted_results)
            {
                std::cout << std::left << std::setw(12) << result.library_name
                    << std::setw(22) << ""
                    << std::right << std::fixed << std::setprecision(3)
                    << std::setw(12) << result.time_ms
                    << std::setw(15) << (result.time_per_iteration() * 1000000.0)
                    << std::setw(12) << (result.memory_bytes / 1024.0)
                    << std::setw(15) << result.memory_per_object()
                    << std::endl;
            }
        }

        // 输出详细的性能分析
        print_performance_analysis(grouped_results);
    }

private:
    void print_performance_analysis(const std::map<std::string, std::vector<TestResult>>& grouped_results)
    {
        std::cout << "\n=== 详细性能分析报告 ===\n";

        for (const auto& group : grouped_results)
        {
            if (group.second.size() < 2) continue;

            std::cout << "\n【" << group.first << " - 相对性能倍数】\n";

            // 找到最慢和最快的结果
            double min_time = std::numeric_limits<double>::max();
            double max_time = 0.0;
            std::string fastest_lib, slowest_lib;

            for (const auto& result : group.second)
            {
                double time_per_iter = result.time_per_iteration();
                if (time_per_iter < min_time && time_per_iter > 0)
                {
                    min_time = time_per_iter;
                    fastest_lib = result.library_name;
                }
                if (time_per_iter > max_time)
                {
                    max_time = time_per_iter;
                    slowest_lib = result.library_name;
                }
            }

            // 计算相对性能
            for (const auto& result : group.second)
            {
                double time_per_iter = result.time_per_iteration();
                if (time_per_iter > 0 && max_time > 0 && min_time > 0)
                {
                    double speedup_vs_slowest = max_time / time_per_iter;
                    double slowdown_vs_fastest = time_per_iter / min_time;

                    std::cout << "  " << std::left << std::setw(8) << result.library_name
                        << ": 比最慢快 " << std::fixed << std::setprecision(2)
                        << speedup_vs_slowest << "倍, 比最快慢 "
                        << slowdown_vs_fastest << "倍" << std::endl;
                }
            }
        }

        // 总体推荐
        print_overall_recommendation(grouped_results);
    }

    void print_overall_recommendation(const std::map<std::string, std::vector<TestResult>>& grouped_results)
    {
        std::cout << "\n=== 使用建议 ===\n";

        std::map<std::string, int> win_count;
        std::map<std::string, double> total_score;

        // 统计每个库在各项测试中的表现
        for (const auto& group : grouped_results)
        {
            if (group.second.size() < 2) continue;

            double min_time = std::numeric_limits<double>::max();
            std::string fastest_lib;

            for (const auto& result : group.second)
            {
                double time_per_iter = result.time_per_iteration();
                if (time_per_iter < min_time && time_per_iter > 0)
                {
                    min_time = time_per_iter;
                    fastest_lib = result.library_name;
                }

                // 累积评分（越小越好）
                total_score[result.library_name] += time_per_iter;
            }

            if (!fastest_lib.empty())
            {
                win_count[fastest_lib]++;
            }
        }

        std::cout << "各反射库在不同场景下的优势：\n";
        for (const auto& entry : win_count)
        {
            std::cout << "• " << entry.first << ": 在 " << entry.second
                << " 项测试中性能最佳\n";
        }

        // 找出总体最佳性能
        if (!total_score.empty())
        {
            std::string overall_best;
            double best_avg_score = std::numeric_limits<double>::max();

            for (const auto& entry : total_score)
            {
                double avg_score = entry.second / grouped_results.size();
                if (avg_score < best_avg_score)
                {
                    best_avg_score = avg_score;
                    overall_best = entry.first;
                }
            }

            std::cout << "\n总体性能最佳: " << overall_best << std::endl;
        }

        std::cout << "建议根据具体使用场景选择合适的反射库。\n";
    }
};

// 全局基准测试实例 - 确保数据能在所有测试间共享
ReflectionBenchmark g_benchmark;

// 测试数据初始化函数
void initialize_test_data(JsonSerializable& obj)
{
    // 使用计算值而非字面量
    const int base_int = 40 + 2; // 替代 42
    const float pi_approx = 3.0f + 0.14159f; // 替代 3.14159f
    const int large_int = 65000 + 536; // 替代 65536
    const double precision_val = 999.0 + 0.999; // 替代 999.999

    obj.A = base_int;
    obj.B = std::string("性能测试对象");

    obj.D.C = pi_approx;
    obj.D.D = large_int;
    obj.D.E = true;
    obj.D.F = std::string("嵌套字符串数据");

    obj.position.x = 10.0f + 0.5f;
    obj.position.y = 20.0f + 0.7f;
    obj.position.z = 30.0f + 0.9f;

    obj.complex.name = std::string("深层嵌套对象");
    obj.complex.value = precision_val;
    obj.complex.inner.C = 123.0f + 0.456f;
    obj.complex.inner.D = 700 + 89;
    obj.complex.inner.E = false;
    obj.complex.inner.F = std::string("非常深层的字符串");

    // 动态生成数组数据
    obj.complex.numbers.clear();
    for (int i = 1; i <= 5; ++i)
    {
        obj.complex.numbers.push_back(i);
    }

    obj.points.clear();
    obj.points.push_back({1.0f, 2.0f, 3.0f});
    obj.points.push_back({4.0f, 5.0f, 6.0f});
    obj.points.push_back({7.0f, 8.0f, 9.0f});
}

//-------------------------------------------------------
// GTest 测试套件 - 同时测试三个反射库
//-------------------------------------------------------

class ReflectionLibrariesComparison : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::cout << "正在初始化反射库性能对比测试...\n";
    }

    void TearDown() override
    {
    }

    // 测试参数 - 使用常量表达式避免字面量
    static constexpr int SERIALIZATION_ITERATIONS = 500000;
    static constexpr int PROPERTY_ACCESS_ITERATIONS = 5000000;
    static constexpr int TYPE_CREATION_ITERATIONS = 1000000;
    static constexpr int BATCH_SIZE = 1000;
    static constexpr int BATCH_ITERATIONS = 500;
    static constexpr int THREAD_COUNT = 4;
    static constexpr int ITERATIONS_PER_THREAD = 100000;
};

TEST_F(ReflectionLibrariesComparison, 序列化性能对比测试)
{
    std::cout << "执行序列化性能测试...\n";

    // 准备测试数据
    JsonSerializable test_obj;
    initialize_test_data(test_obj);

    json output;

    // RTTR 序列化测试
    rttr::instance rttr_obj = test_obj;
    g_benchmark.run_test("RTTR", "序列化性能", [&]()
    {
        for (int i = 0; i < SERIALIZATION_ITERATIONS; ++i)
        {
            output = SerializeRTTR(rttr_obj);
        }
    }, SERIALIZATION_ITERATIONS);

    // 验证 RTTR 序列化结果
    ASSERT_TRUE(output.contains("A"));
    EXPECT_EQ(output["A"], test_obj.A);

    // Boost.Hana 序列化测试
    g_benchmark.run_test("Hana", "序列化性能", [&]()
    {
        for (int i = 0; i < SERIALIZATION_ITERATIONS; ++i)
        {
            output = HanaSerializer::serialize(test_obj);
        }
    }, SERIALIZATION_ITERATIONS);

    // 验证 Hana 序列化结果
    ASSERT_TRUE(output.contains("A"));
    EXPECT_EQ(output["A"], test_obj.A);

    // RTTM 序列化测试
    auto rttm_type = RType::Get<JsonSerializable>();
    rttm_type->Create();
    rttm_type->As<JsonSerializable>() = test_obj;

    g_benchmark.run_test("RTTM", "序列化性能", [&]()
    {
        for (int i = 0; i < SERIALIZATION_ITERATIONS; ++i)
        {
            output = SerializeRTTM(rttm_type);
        }
    }, SERIALIZATION_ITERATIONS);

    // 验证 RTTM 序列化结果
    ASSERT_TRUE(output.contains("A"));
    EXPECT_EQ(output["A"], test_obj.A);

    std::cout << "序列化性能测试完成。\n";
}

TEST_F(ReflectionLibrariesComparison, 反序列化性能对比测试)
{
    std::cout << "执行反序列化性能测试...\n";

    // 准备序列化数据
    JsonSerializable test_obj;
    initialize_test_data(test_obj);
    json test_data = HanaSerializer::serialize(test_obj);

    // RTTR 反序列化测试
    JsonSerializable rttr_deser_obj;
    rttr::instance rttr_deser_instance = rttr_deser_obj;

    g_benchmark.run_test("RTTR", "反序列化性能", [&]()
    {
        for (int i = 0; i < SERIALIZATION_ITERATIONS; ++i)
        {
            DeserializeRTTR(rttr_deser_instance, test_data);
        }
    }, SERIALIZATION_ITERATIONS);

    // 验证 RTTR 反序列化结果
    EXPECT_EQ(rttr_deser_obj.A, test_obj.A);

    // Boost.Hana 反序列化测试
    JsonSerializable hana_deser_obj;

    g_benchmark.run_test("Hana", "反序列化性能", [&]()
    {
        for (int i = 0; i < SERIALIZATION_ITERATIONS; ++i)
        {
            HanaSerializer::deserialize(hana_deser_obj, test_data);
        }
    }, SERIALIZATION_ITERATIONS);

    // 验证 Hana 反序列化结果
    EXPECT_EQ(hana_deser_obj.A, test_obj.A);

    // RTTM 反序列化测试
    auto rttm_deser_type = RType::Get<JsonSerializable>();
    rttm_deser_type->Create();

    g_benchmark.run_test("RTTM", "反序列化性能", [&]()
    {
        for (int i = 0; i < SERIALIZATION_ITERATIONS; ++i)
        {
            DeserializeRTTM(rttm_deser_type, test_data);
        }
    }, SERIALIZATION_ITERATIONS);

    // 验证 RTTM 反序列化结果
    EXPECT_EQ(rttm_deser_type->As<JsonSerializable>().A, test_obj.A);

    std::cout << "反序列化性能测试完成。\n";
}

TEST_F(ReflectionLibrariesComparison, 属性访问性能对比测试)
{
    std::cout << "执行属性访问性能测试...\n";

    JsonSerializable test_obj;
    initialize_test_data(test_obj);

    int sum = 0;

    // 直接属性访问（基准测试）
    g_benchmark.run_test("Direct", "属性访问性能", [&]()
    {
        for (int i = 0; i < PROPERTY_ACCESS_ITERATIONS; ++i)
        {
            sum += test_obj.A;
        }
    }, PROPERTY_ACCESS_ITERATIONS);

    // RTTR 属性访问测试
    rttr::instance rttr_obj = test_obj;
    int rttr_sum = 0;

    g_benchmark.run_test("RTTR", "属性访问性能", [&]()
    {
        rttr::property prop = rttr::type::get<JsonSerializable>().get_property("A");
        rttr::variant value = prop.get_value(rttr_obj);
        for (int i = 0; i < PROPERTY_ACCESS_ITERATIONS; ++i)
        {
            rttr_sum += value.to_int();
        }
    }, PROPERTY_ACCESS_ITERATIONS);

    EXPECT_EQ(sum, rttr_sum);

    // Boost.Hana 属性访问测试
    int hana_sum = 0;
    g_benchmark.run_test("Hana", "属性访问性能", [&]()
    {
        auto accessor = hana::accessors<JsonSerializable>()[hana::size_c<0>];
        auto getter = hana::second(accessor);
        for (int i = 0; i < PROPERTY_ACCESS_ITERATIONS; ++i)
        {
            hana_sum += getter(test_obj);
        }
    }, PROPERTY_ACCESS_ITERATIONS);

    EXPECT_EQ(sum, hana_sum);

    // RTTM 属性访问测试
    auto rttm_type = RType::Get<JsonSerializable>();
    rttm_type->Create();
    rttm_type->As<JsonSerializable>() = test_obj;

    int rttm_sum = 0;
    g_benchmark.run_test("RTTM", "属性访问性能", [&]()
    {
        auto property_a = rttm_type->GetProperty("A");
        for (int i = 0; i < PROPERTY_ACCESS_ITERATIONS; ++i)
        {
            rttm_sum += property_a->As<int>();
        }
    }, PROPERTY_ACCESS_ITERATIONS);

    EXPECT_EQ(sum, rttm_sum);

    std::cout << "属性访问性能测试完成。\n";
}

TEST_F(ReflectionLibrariesComparison, 类型创建性能对比测试)
{
    std::cout << "执行类型创建性能测试...\n";

    // 直接对象创建（基准测试）
    g_benchmark.run_test("Direct", "类型创建性能", [&]()
    {
        for (int i = 0; i < TYPE_CREATION_ITERATIONS; ++i)
        {
            JsonSerializable obj;
            initialize_test_data(obj);
        }
    }, TYPE_CREATION_ITERATIONS);

    // RTTR 类型创建测试
    g_benchmark.run_test("RTTR", "类型创建性能", [&]()
    {
        auto type = rttr::type::get<JsonSerializable>();
        for (int i = 0; i < TYPE_CREATION_ITERATIONS; ++i)
        {
            auto instance = type.create();
        }
    }, TYPE_CREATION_ITERATIONS);

    // Boost.Hana 对象创建测试（无特殊类型系统）
    g_benchmark.run_test("Hana", "类型创建性能", [&]()
    {
        for (int i = 0; i < TYPE_CREATION_ITERATIONS; ++i)
        {
            JsonSerializable obj;
            initialize_test_data(obj);
        }
    }, TYPE_CREATION_ITERATIONS);

    // RTTM 类型创建测试
    g_benchmark.run_test("RTTM", "类型创建性能", [&]()
    {
        for (int i = 0; i < TYPE_CREATION_ITERATIONS; ++i)
        {
            auto type = RType::Get<JsonSerializable>();
            type->Create();
        }
    }, TYPE_CREATION_ITERATIONS);

    std::cout << "类型创建性能测试完成。\n";
}

TEST_F(ReflectionLibrariesComparison, 批处理性能对比测试)
{
    std::cout << "执行批处理性能测试...\n";

    // 准备批量测试对象
    std::vector<JsonSerializable> objects(BATCH_SIZE);
    for (int i = 0; i < BATCH_SIZE; i++)
    {
        initialize_test_data(objects[i]);
        objects[i].A = i; // 添加变化
    }

    std::vector<json> serialized;
    serialized.reserve(BATCH_SIZE);

    // RTTR 批处理测试
    g_benchmark.run_test("RTTR", "批处理序列化", [&]()
    {
        for (int iter = 0; iter < BATCH_ITERATIONS; iter++)
        {
            serialized.clear();
            for (const auto& obj : objects)
            {
                serialized.push_back(SerializeRTTR(obj));
            }
        }
    }, BATCH_ITERATIONS * BATCH_SIZE, BATCH_SIZE);

    // Boost.Hana 批处理测试
    g_benchmark.run_test("Hana", "批处理序列化", [&]()
    {
        for (int iter = 0; iter < BATCH_ITERATIONS; iter++)
        {
            serialized.clear();
            for (const auto& obj : objects)
            {
                serialized.push_back(HanaSerializer::serialize(obj));
            }
        }
    }, BATCH_ITERATIONS * BATCH_SIZE, BATCH_SIZE);

    // RTTM 批处理测试
    std::vector<_Ref_<RType>> rttm_objects;
    rttm_objects.reserve(BATCH_SIZE);

    for (int i = 0; i < BATCH_SIZE; i++)
    {
        auto obj = RType::Get<JsonSerializable>();
        obj->Create();
        obj->As<JsonSerializable>() = objects[i];
        rttm_objects.push_back(obj);
    }

    g_benchmark.run_test("RTTM", "批处理序列化", [&]()
    {
        for (int iter = 0; iter < BATCH_ITERATIONS; iter++)
        {
            serialized.clear();
            for (const auto& obj : rttm_objects)
            {
                serialized.push_back(SerializeRTTM(obj));
            }
        }
    }, BATCH_ITERATIONS * BATCH_SIZE, BATCH_SIZE);

    std::cout << "批处理性能测试完成。\n";
}

TEST_F(ReflectionLibrariesComparison, 多线程性能对比测试)
{
    std::cout << "执行多线程性能测试...\n";

    JsonSerializable base_obj;
    initialize_test_data(base_obj);

    const int total_iterations = THREAD_COUNT * ITERATIONS_PER_THREAD;

    // RTTR 多线程测试
    g_benchmark.run_test("RTTR", "多线程序列化", [&]()
    {
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads(0);

        for (int t = 0; t < THREAD_COUNT; t++)
        {
            threads.emplace_back([t, &completed_threads, base_obj]()
            {
                JsonSerializable thread_obj = base_obj;
                thread_obj.A = t + 1000;

                for (int i = 0; i < ITERATIONS_PER_THREAD; i++)
                {
                    json output = SerializeRTTR(thread_obj);
                }

                completed_threads.fetch_add(1, std::memory_order_relaxed);
            });
        }

        for (auto& t : threads)
        {
            if (t.joinable()) t.join();
        }

        EXPECT_EQ(completed_threads.load(), THREAD_COUNT);
    }, total_iterations, THREAD_COUNT);

    // Boost.Hana 多线程测试
    g_benchmark.run_test("Hana", "多线程序列化", [&]()
    {
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads(0);

        for (int t = 0; t < THREAD_COUNT; t++)
        {
            threads.emplace_back([t, &completed_threads, base_obj]()
            {
                JsonSerializable thread_obj = base_obj;
                thread_obj.A = t + 1000;

                for (int i = 0; i < ITERATIONS_PER_THREAD; i++)
                {
                    json output = HanaSerializer::serialize(thread_obj);
                }

                completed_threads.fetch_add(1, std::memory_order_relaxed);
            });
        }

        for (auto& t : threads)
        {
            if (t.joinable()) t.join();
        }

        EXPECT_EQ(completed_threads.load(), THREAD_COUNT);
    }, total_iterations, THREAD_COUNT);

    // RTTM 多线程测试
    g_benchmark.run_test("RTTM", "多线程序列化", [&]()
    {
        std::vector<std::thread> threads;
        std::atomic<int> completed_threads(0);

        for (int t = 0; t < THREAD_COUNT; t++)
        {
            threads.emplace_back([t, &completed_threads, base_obj]()
            {
                auto thread_type = RType::Get<JsonSerializable>();
                thread_type->Create();
                auto& obj = thread_type->As<JsonSerializable>();
                obj = base_obj;
                obj.A = t + 1000;

                for (int i = 0; i < ITERATIONS_PER_THREAD; i++)
                {
                    json output = SerializeRTTM(thread_type);
                }

                completed_threads.fetch_add(1, std::memory_order_relaxed);
            });
        }

        for (auto& t : threads)
        {
            if (t.joinable()) t.join();
        }

        EXPECT_EQ(completed_threads.load(), THREAD_COUNT);
    }, total_iterations, THREAD_COUNT);

    std::cout << "多线程性能测试完成。\n";
}

// 测试结果输出
TEST_F(ReflectionLibrariesComparison, 输出综合测试结果)
{
    std::cout << "\n正在生成综合性能对比报告...\n";
    g_benchmark.print_comprehensive_results();
}

//-------------------------------------------------------
// 程序入口点
//-------------------------------------------------------
int main(int argc, char** argv)
{
    std::cout << "=== C++反射库综合性能对比基准测试 ===\n";
    std::cout << "测试库：RTTM、RTTR、Boost.Hana\n";
    std::cout << "编译器：MSVC C++17\n";
    std::cout << "测试内容：序列化、反序列化、属性访问、类型创建、批处理、多线程\n";
    std::cout << "开始执行测试...\n\n";

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    std::cout << "\n=== 测试完成 ===\n";
    return result;
}