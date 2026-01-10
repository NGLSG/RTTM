/**
 * @file reflcpp_benchmark.cpp
 * @brief refl-cpp reflection library benchmark
 * 
 * refl-cpp is a compile-time reflection library using macros.
 * This benchmark compares it with RTTM's semi-static approach.
 * 
 * NOTE: refl-cpp is pure compile-time, so most operations get inlined
 * to direct access. These benchmarks measure the "optimized away" cost.
 */

#include <benchmark/benchmark.h>
#include <refl.hpp>
#include <string>
#include <vector>

// ============================================================================
// Test Classes (must match RTTM benchmark)
// ============================================================================

struct Vector3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    
    Vector3() = default;
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    float length() const { return std::sqrt(x*x + y*y + z*z); }
    float dot(const Vector3& other) const { return x*other.x + y*other.y + z*other.z; }
};

struct SimpleClass {
    int intValue = 0;
    float floatValue = 0.0f;
    std::string stringValue;
    
    int getInt() const { return intValue; }
    void setInt(int v) { intValue = v; }
    float getFloat() const { return floatValue; }
    void setFloat(float v) { floatValue = v; }
};

struct ComplexClass {
    int id = 0;
    std::string name;
    Vector3 position;
    std::vector<int> scores;
    
    int getId() const { return id; }
    void setId(int v) { id = v; }
    const std::string& getName() const { return name; }
    void setName(const std::string& v) { name = v; }
    const Vector3& getPosition() const { return position; }
};

struct DeepClass {
    int level1 = 0, level2 = 0, level3 = 0, level4 = 0, level5 = 0;
    std::string data;
    
    int compute() const { return level1 + level2 + level3 + level4 + level5; }
};

// ============================================================================
// refl-cpp Type Registration (compile-time macros)
// ============================================================================

REFL_AUTO(
    type(Vector3),
    field(x), field(y), field(z),
    func(length), func(dot)
)

REFL_AUTO(
    type(SimpleClass),
    field(intValue), field(floatValue), field(stringValue),
    func(getInt), func(setInt), func(getFloat), func(setFloat)
)

REFL_AUTO(
    type(ComplexClass),
    field(id), field(name), field(position), field(scores),
    func(getId), func(setId), func(getName), func(setName), func(getPosition)
)

REFL_AUTO(
    type(DeepClass),
    field(level1), field(level2), field(level3), field(level4), field(level5),
    field(data), func(compute)
)

// ============================================================================
// 1. Type Info Access (with ClobberMemory to prevent hoisting)
// ============================================================================

static void ReflCpp_TypeInfo(benchmark::State& state) {
    for (auto _ : state) {
        auto type = refl::reflect<SimpleClass>();
        benchmark::DoNotOptimize(type.name.data);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(ReflCpp_TypeInfo);

// ============================================================================
// 2. Property Read - 8x unrolled to escape noise floor
// ============================================================================

static void ReflCpp_PropertyRead(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    using members = refl::member_list<SimpleClass>;
    using intValue_member = refl::trait::get_t<0, members>;
    constexpr intValue_member member{};
    
    int sum = 0;
    for (auto _ : state) {
        // 8x unroll to escape 0.2ns noise floor
        sum += member(obj);
        sum += member(obj);
        sum += member(obj);
        sum += member(obj);
        sum += member(obj);
        sum += member(obj);
        sum += member(obj);
        sum += member(obj);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * 8);
}
BENCHMARK(ReflCpp_PropertyRead);

// ============================================================================
// 3. Property Write - 8x unrolled with dependency chain
// ============================================================================

static void ReflCpp_PropertyWrite(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 0;
    
    using members = refl::member_list<SimpleClass>;
    using intValue_member = refl::trait::get_t<0, members>;
    constexpr intValue_member member{};
    
    int i = 0;
    for (auto _ : state) {
        // 8x unroll with dependency chain
        member(obj) = i; i = obj.intValue + 1;
        member(obj) = i; i = obj.intValue + 1;
        member(obj) = i; i = obj.intValue + 1;
        member(obj) = i; i = obj.intValue + 1;
        member(obj) = i; i = obj.intValue + 1;
        member(obj) = i; i = obj.intValue + 1;
        member(obj) = i; i = obj.intValue + 1;
        member(obj) = i; i = obj.intValue + 1;
        benchmark::DoNotOptimize(i);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * 8);
}
BENCHMARK(ReflCpp_PropertyWrite);

// ============================================================================
// 4. Multiple Property Access
// ============================================================================

static void ReflCpp_PropertyAccess_Multiple(benchmark::State& state) {
    ComplexClass obj;
    obj.id = 42;
    obj.name = "TestObject";
    obj.position = {1.0f, 2.0f, 3.0f};
    
    using members = refl::member_list<ComplexClass>;
    using id_member = refl::trait::get_t<0, members>;
    using name_member = refl::trait::get_t<1, members>;
    using pos_member = refl::trait::get_t<2, members>;
    
    constexpr id_member m_id{};
    constexpr name_member m_name{};
    constexpr pos_member m_pos{};
    
    for (auto _ : state) {
        auto id = m_id(obj);
        auto& name = m_name(obj);
        auto& pos = m_pos(obj);
        benchmark::DoNotOptimize(id);
        benchmark::DoNotOptimize(name.data());
        benchmark::DoNotOptimize(&pos);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(ReflCpp_PropertyAccess_Multiple);

// ============================================================================
// 5. Deep Property Access (5 properties)
// ============================================================================

static void ReflCpp_PropertyAccess_Deep(benchmark::State& state) {
    DeepClass obj;
    obj.level1 = 1; obj.level2 = 2; obj.level3 = 3;
    obj.level4 = 4; obj.level5 = 5;
    
    using members = refl::member_list<DeepClass>;
    using p1_t = refl::trait::get_t<0, members>;
    using p2_t = refl::trait::get_t<1, members>;
    using p3_t = refl::trait::get_t<2, members>;
    using p4_t = refl::trait::get_t<3, members>;
    using p5_t = refl::trait::get_t<4, members>;
    
    constexpr p1_t p1{}; constexpr p2_t p2{}; constexpr p3_t p3{};
    constexpr p4_t p4{}; constexpr p5_t p5{};
    
    for (auto _ : state) {
        int sum = p1(obj);
        sum += p2(obj);
        sum += p3(obj);
        sum += p4(obj);
        sum += p5(obj);
        benchmark::DoNotOptimize(sum);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(ReflCpp_PropertyAccess_Deep);

// ============================================================================
// 6. Method Call (no args) - 8x unrolled
// ============================================================================

static void ReflCpp_MethodCall_NoArgs(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    using members = refl::member_list<SimpleClass>;
    using getInt_member = refl::trait::get_t<3, members>;
    constexpr getInt_member getInt{};
    
    int sum = 0;
    for (auto _ : state) {
        sum += getInt(obj);
        sum += getInt(obj);
        sum += getInt(obj);
        sum += getInt(obj);
        sum += getInt(obj);
        sum += getInt(obj);
        sum += getInt(obj);
        sum += getInt(obj);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * 8);
}
BENCHMARK(ReflCpp_MethodCall_NoArgs);

// ============================================================================
// 7. Method Call (with arg) - 8x unrolled with dependency
// ============================================================================

static void ReflCpp_MethodCall_WithArg(benchmark::State& state) {
    SimpleClass obj;
    
    using members = refl::member_list<SimpleClass>;
    using setInt_member = refl::trait::get_t<4, members>;
    constexpr setInt_member setInt{};
    
    int i = 0;
    for (auto _ : state) {
        setInt(obj, i); i = obj.intValue + 1;
        setInt(obj, i); i = obj.intValue + 1;
        setInt(obj, i); i = obj.intValue + 1;
        setInt(obj, i); i = obj.intValue + 1;
        setInt(obj, i); i = obj.intValue + 1;
        setInt(obj, i); i = obj.intValue + 1;
        setInt(obj, i); i = obj.intValue + 1;
        setInt(obj, i); i = obj.intValue + 1;
        benchmark::DoNotOptimize(i);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * 8);
}
BENCHMARK(ReflCpp_MethodCall_WithArg);

// ============================================================================
// 8. Property Enumeration (compile-time)
// ============================================================================

static void ReflCpp_PropertyEnumeration(benchmark::State& state) {
    for (auto _ : state) {
        auto type = refl::reflect<ComplexClass>();
        std::size_t count = 0;
        refl::util::for_each(type.members, [&](auto member) {
            if constexpr (refl::descriptor::is_field(member)) {
                count++;
            }
        });
        benchmark::DoNotOptimize(count);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(ReflCpp_PropertyEnumeration);

// ============================================================================
// 9. Batch Property Access (100 objects)
// ============================================================================

static void ReflCpp_Batch_PropertyAccess(benchmark::State& state) {
    std::vector<SimpleClass> objects(100);
    for (int i = 0; i < 100; ++i) {
        objects[i].intValue = i;
    }
    
    using members = refl::member_list<SimpleClass>;
    using intValue_member = refl::trait::get_t<0, members>;
    constexpr intValue_member member{};
    
    for (auto _ : state) {
        int sum = 0;
        for (auto& obj : objects) {
            sum += member(obj);
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(ReflCpp_Batch_PropertyAccess);

// ============================================================================
// 10. Batch Method Calls (100 objects)
// ============================================================================

static void ReflCpp_Batch_MethodCalls(benchmark::State& state) {
    std::vector<SimpleClass> objects(100);
    for (int i = 0; i < 100; ++i) {
        objects[i].intValue = i;
    }
    
    using members = refl::member_list<SimpleClass>;
    using getInt_member = refl::trait::get_t<3, members>;
    constexpr getInt_member getInt{};
    
    for (auto _ : state) {
        int sum = 0;
        for (auto& obj : objects) {
            sum += getInt(obj);
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(ReflCpp_Batch_MethodCalls);

// ============================================================================
// 11. Baseline - Direct Access (8x unrolled for fair comparison)
// ============================================================================

static void Baseline_DirectPropertyRead(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    int sum = 0;
    for (auto _ : state) {
        sum += obj.intValue;
        sum += obj.intValue;
        sum += obj.intValue;
        sum += obj.intValue;
        sum += obj.intValue;
        sum += obj.intValue;
        sum += obj.intValue;
        sum += obj.intValue;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * 8);
}
BENCHMARK(Baseline_DirectPropertyRead);

static void Baseline_DirectPropertyWrite(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 0;
    
    int i = 0;
    for (auto _ : state) {
        obj.intValue = i; i = obj.intValue + 1;
        obj.intValue = i; i = obj.intValue + 1;
        obj.intValue = i; i = obj.intValue + 1;
        obj.intValue = i; i = obj.intValue + 1;
        obj.intValue = i; i = obj.intValue + 1;
        obj.intValue = i; i = obj.intValue + 1;
        obj.intValue = i; i = obj.intValue + 1;
        obj.intValue = i; i = obj.intValue + 1;
        benchmark::DoNotOptimize(i);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * 8);
}
BENCHMARK(Baseline_DirectPropertyWrite);

static void Baseline_DirectMethodCall(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    int sum = 0;
    for (auto _ : state) {
        sum += obj.getInt();
        sum += obj.getInt();
        sum += obj.getInt();
        sum += obj.getInt();
        sum += obj.getInt();
        sum += obj.getInt();
        sum += obj.getInt();
        sum += obj.getInt();
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * 8);
}
BENCHMARK(Baseline_DirectMethodCall);

BENCHMARK_MAIN();
