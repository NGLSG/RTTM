/**
 * @file rttr_benchmark.cpp
 * @brief RTTR reflection library comprehensive benchmark
 * 
 * Benchmark categories (matching RTTM benchmark):
 * 1. Type Lookup - Static and dynamic type resolution
 * 2. Object Creation - Instance construction
 * 3. Property Access - Read/write operations
 * 4. Method Invocation - Function calls
 * 5. Full Reflection Path - Complete dynamic workflow
 * 6. Batch Operations - Bulk processing
 */

#include <benchmark/benchmark.h>
#include <rttr/registration>
#include <rttr/type>
#include "benchmark_common.hpp"

using namespace rttr;

// ============================================================================
// Type Registration
// ============================================================================

RTTR_REGISTRATION {
    registration::class_<Vector3>("Vector3")
        .constructor<>()
        .constructor<float, float, float>()
        .property("x", &Vector3::x)
        .property("y", &Vector3::y)
        .property("z", &Vector3::z)
        .method("length", &Vector3::length)
        .method("dot", &Vector3::dot);

    registration::class_<SimpleClass>("SimpleClass")
        .constructor<>()
        .property("intValue", &SimpleClass::intValue)
        .property("floatValue", &SimpleClass::floatValue)
        .property("stringValue", &SimpleClass::stringValue)
        .method("getInt", &SimpleClass::getInt)
        .method("setInt", &SimpleClass::setInt)
        .method("getFloat", &SimpleClass::getFloat)
        .method("setFloat", &SimpleClass::setFloat);

    registration::class_<ComplexClass>("ComplexClass")
        .constructor<>()
        .property("id", &ComplexClass::id)
        .property("name", &ComplexClass::name)
        .property("position", &ComplexClass::position)
        .property("scores", &ComplexClass::scores)
        .method("getId", &ComplexClass::getId)
        .method("setId", &ComplexClass::setId)
        .method("getName", &ComplexClass::getName)
        .method("setName", &ComplexClass::setName)
        .method("getPosition", &ComplexClass::getPosition);

    registration::class_<DeepClass>("DeepClass")
        .constructor<>()
        .property("level1", &DeepClass::level1)
        .property("level2", &DeepClass::level2)
        .property("level3", &DeepClass::level3)
        .property("level4", &DeepClass::level4)
        .property("level5", &DeepClass::level5)
        .property("data", &DeepClass::data)
        .method("compute", &DeepClass::compute);
}

// ============================================================================
// 1. Type Lookup Benchmarks
// ============================================================================

// Static type lookup via template
static void RTTR_TypeLookup_Static(benchmark::State& state) {
    for (auto _ : state) {
        auto t = type::get<SimpleClass>();
        benchmark::DoNotOptimize(t);
    }
}
BENCHMARK(RTTR_TypeLookup_Static);

// Dynamic type lookup via string
static void RTTR_TypeLookup_Dynamic(benchmark::State& state) {
    std::string_view name = "SimpleClass";
    for (auto _ : state) {
        auto t = type::get_by_name(name.data());
        benchmark::DoNotOptimize(t);
    }
}
BENCHMARK(RTTR_TypeLookup_Dynamic);

// ============================================================================
// 2. Object Creation Benchmarks
// ============================================================================

static void RTTR_ObjectCreation_Simple(benchmark::State& state) {
    auto t = type::get<SimpleClass>();
    for (auto _ : state) {
        variant obj = t.create();
        benchmark::DoNotOptimize(obj);
    }
}
BENCHMARK(RTTR_ObjectCreation_Simple);

static void RTTR_ObjectCreation_Complex(benchmark::State& state) {
    auto t = type::get<ComplexClass>();
    for (auto _ : state) {
        variant obj = t.create();
        benchmark::DoNotOptimize(obj);
    }
}
BENCHMARK(RTTR_ObjectCreation_Complex);

// ============================================================================
// 3. Property Access Benchmarks
// ============================================================================

// Level 1: Full dynamic - lookup inside loop
static void RTTR_PropertyRead_FullDynamic(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    auto t = type::get<SimpleClass>();
    instance inst(obj);
    
    int sum = 0;
    for (auto _ : state) {
        auto prop = t.get_property("intValue");
        sum += prop.get_value(inst).to_int();
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTR_PropertyRead_FullDynamic);

// Level 2: Cached handle - lookup once
static void RTTR_PropertyRead_Cached(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    auto t = type::get<SimpleClass>();
    auto prop = t.get_property("intValue");
    instance inst(obj);
    
    int sum = 0;
    for (auto _ : state) {
        sum += prop.get_value(inst).to_int();
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTR_PropertyRead_Cached);

// Level 1: Full dynamic write
static void RTTR_PropertyWrite_FullDynamic(benchmark::State& state) {
    SimpleClass obj;
    
    auto t = type::get<SimpleClass>();
    instance inst(obj);
    
    int i = 0;
    for (auto _ : state) {
        auto prop = t.get_property("intValue");
        prop.set_value(inst, i++);
        benchmark::DoNotOptimize(obj.intValue);
    }
}
BENCHMARK(RTTR_PropertyWrite_FullDynamic);

// Level 2: Cached handle write (with dependency chain)
static void RTTR_PropertyWrite_Cached(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 0;
    
    auto t = type::get<SimpleClass>();
    auto prop = t.get_property("intValue");
    instance inst(obj);
    
    int i = 0;
    for (auto _ : state) {
        i += obj.intValue;  // dependency chain
        prop.set_value(inst, i);
        benchmark::DoNotOptimize(obj.intValue);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTR_PropertyWrite_Cached);

// Multiple property access
static void RTTR_PropertyAccess_Multiple(benchmark::State& state) {
    ComplexClass obj;
    obj.id = 42;
    obj.name = "TestObject";
    obj.position = {1.0f, 2.0f, 3.0f};
    
    auto t = type::get<ComplexClass>();
    auto prop_id = t.get_property("id");
    auto prop_name = t.get_property("name");
    auto prop_pos = t.get_property("position");
    instance inst(obj);
    
    for (auto _ : state) {
        auto id = prop_id.get_value(inst).to_int();
        auto name = prop_name.get_value(inst).to_string();
        auto pos = prop_pos.get_value(inst).get_value<Vector3>();
        benchmark::DoNotOptimize(id);
        benchmark::DoNotOptimize(name);
        benchmark::DoNotOptimize(pos);
    }
}
BENCHMARK(RTTR_PropertyAccess_Multiple);

// Deep property access (5 properties)
static void RTTR_PropertyAccess_Deep(benchmark::State& state) {
    DeepClass obj;
    obj.level1 = 1; obj.level2 = 2; obj.level3 = 3;
    obj.level4 = 4; obj.level5 = 5;
    
    auto t = type::get<DeepClass>();
    auto p1 = t.get_property("level1");
    auto p2 = t.get_property("level2");
    auto p3 = t.get_property("level3");
    auto p4 = t.get_property("level4");
    auto p5 = t.get_property("level5");
    instance inst(obj);
    
    for (auto _ : state) {
        int sum = p1.get_value(inst).to_int();
        sum += p2.get_value(inst).to_int();
        sum += p3.get_value(inst).to_int();
        sum += p4.get_value(inst).to_int();
        sum += p5.get_value(inst).to_int();
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTR_PropertyAccess_Deep);

// ============================================================================
// 4. Method Invocation Benchmarks
// ============================================================================

// Level 1: Full dynamic - lookup inside loop
static void RTTR_MethodCall_FullDynamic(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    auto t = type::get<SimpleClass>();
    instance inst(obj);
    
    int sum = 0;
    for (auto _ : state) {
        auto meth = t.get_method("getInt");
        sum += meth.invoke(inst).to_int();
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTR_MethodCall_FullDynamic);

// Level 2: Cached handle - lookup once
static void RTTR_MethodCall_Cached(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    auto t = type::get<SimpleClass>();
    auto meth = t.get_method("getInt");
    instance inst(obj);
    
    int sum = 0;
    for (auto _ : state) {
        sum += meth.invoke(inst).to_int();
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTR_MethodCall_Cached);

// Level 1: Full dynamic with arg
static void RTTR_MethodCall_WithArg_FullDynamic(benchmark::State& state) {
    SimpleClass obj;
    
    auto t = type::get<SimpleClass>();
    instance inst(obj);
    
    int i = 0;
    for (auto _ : state) {
        auto meth = t.get_method("setInt");
        meth.invoke(inst, i++);
        benchmark::DoNotOptimize(obj.intValue);
    }
}
BENCHMARK(RTTR_MethodCall_WithArg_FullDynamic);

// Level 2: Cached handle with arg
static void RTTR_MethodCall_WithArg_Cached(benchmark::State& state) {
    SimpleClass obj;
    
    auto t = type::get<SimpleClass>();
    auto meth = t.get_method("setInt");
    instance inst(obj);
    
    int i = 0;
    for (auto _ : state) {
        meth.invoke(inst, i++);
        benchmark::DoNotOptimize(obj.intValue);
    }
}
BENCHMARK(RTTR_MethodCall_WithArg_Cached);

// Method call - complex return type
static void RTTR_MethodCall_ComplexReturn(benchmark::State& state) {
    ComplexClass obj;
    obj.position = {1.0f, 2.0f, 3.0f};
    
    auto t = type::get<ComplexClass>();
    auto meth = t.get_method("getPosition");
    instance inst(obj);
    
    for (auto _ : state) {
        auto pos = meth.invoke(inst).get_value<Vector3>();
        benchmark::DoNotOptimize(pos);
    }
}
BENCHMARK(RTTR_MethodCall_ComplexReturn);

// ============================================================================
// 5. Full Reflection Path Benchmarks
// ============================================================================

// Complete dynamic path: type lookup + property access
static void RTTR_FullPath_PropertyAccess(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    std::string_view type_name = "SimpleClass";
    std::string_view prop_name = "intValue";
    
    int sum = 0;
    for (auto _ : state) {
        auto t = type::get_by_name(type_name.data());
        auto prop = t.get_property(prop_name.data());
        instance inst(obj);
        sum += prop.get_value(inst).to_int();
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTR_FullPath_PropertyAccess);

// Complete dynamic path: type lookup + method call
static void RTTR_FullPath_MethodCall(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    std::string_view type_name = "SimpleClass";
    std::string_view method_name = "getInt";
    
    int sum = 0;
    for (auto _ : state) {
        auto t = type::get_by_name(type_name.data());
        auto meth = t.get_method(method_name.data());
        instance inst(obj);
        sum += meth.invoke(inst).to_int();
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTR_FullPath_MethodCall);

// Full path with object creation
static void RTTR_FullPath_CreateAndAccess(benchmark::State& state) {
    std::string_view type_name = "SimpleClass";
    
    for (auto _ : state) {
        auto t = type::get_by_name(type_name.data());
        variant var = t.create();
        auto prop = t.get_property("intValue");
        prop.set_value(var, 42);
        int val = prop.get_value(var).to_int();
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK(RTTR_FullPath_CreateAndAccess);

// ============================================================================
// 6. Batch Operations Benchmarks
// ============================================================================

// Batch object creation
static void RTTR_Batch_Creation(benchmark::State& state) {
    auto t = type::get<SimpleClass>();
    
    for (auto _ : state) {
        std::vector<variant> objects;
        objects.reserve(100);
        for (int i = 0; i < 100; ++i) {
            objects.push_back(t.create());
        }
        benchmark::DoNotOptimize(objects);
    }
}
BENCHMARK(RTTR_Batch_Creation);

// Batch property access
static void RTTR_Batch_PropertyAccess(benchmark::State& state) {
    std::vector<SimpleClass> objects(100);
    for (int i = 0; i < 100; ++i) {
        objects[i].intValue = i;
    }
    
    auto t = type::get<SimpleClass>();
    auto prop = t.get_property("intValue");
    
    for (auto _ : state) {
        int sum = 0;
        for (auto& obj : objects) {
            instance inst(obj);
            sum += prop.get_value(inst).to_int();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTR_Batch_PropertyAccess);

// Batch method calls
static void RTTR_Batch_MethodCalls(benchmark::State& state) {
    std::vector<SimpleClass> objects(100);
    for (int i = 0; i < 100; ++i) {
        objects[i].intValue = i;
    }
    
    auto t = type::get<SimpleClass>();
    auto meth = t.get_method("getInt");
    
    for (auto _ : state) {
        int sum = 0;
        for (auto& obj : objects) {
            instance inst(obj);
            sum += meth.invoke(inst).to_int();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTR_Batch_MethodCalls);

// ============================================================================
// 7. Property Enumeration
// ============================================================================

static void RTTR_PropertyEnumeration(benchmark::State& state) {
    auto t = type::get<ComplexClass>();
    
    for (auto _ : state) {
        auto props = t.get_properties();
        std::size_t hash = 0;
        for (const auto& prop : props) {
            hash ^= std::hash<std::string_view>{}(prop.get_name().to_string());
        }
        benchmark::DoNotOptimize(hash);
    }
}
BENCHMARK(RTTR_PropertyEnumeration);

static void RTTR_MethodEnumeration(benchmark::State& state) {
    auto t = type::get<ComplexClass>();
    
    for (auto _ : state) {
        auto methods = t.get_methods();
        std::size_t hash = 0;
        for (const auto& meth : methods) {
            hash ^= std::hash<std::string_view>{}(meth.get_name().to_string());
        }
        benchmark::DoNotOptimize(hash);
    }
}
BENCHMARK(RTTR_MethodEnumeration);

// ============================================================================
// 8. Baseline - Direct Access (for comparison)
// ============================================================================

static void Baseline_DirectPropertyRead(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    int sum = 0;
    for (auto _ : state) {
        sum += obj.intValue;
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(Baseline_DirectPropertyRead);

static void Baseline_DirectPropertyWrite(benchmark::State& state) {
    SimpleClass obj;
    
    int i = 0;
    for (auto _ : state) {
        obj.intValue = i++;
        benchmark::DoNotOptimize(obj.intValue);
    }
}
BENCHMARK(Baseline_DirectPropertyWrite);

static void Baseline_DirectMethodCall(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    int sum = 0;
    for (auto _ : state) {
        sum += obj.getInt();
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(Baseline_DirectMethodCall);

BENCHMARK_MAIN();
