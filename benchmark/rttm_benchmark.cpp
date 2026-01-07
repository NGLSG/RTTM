/**
 * @file rttm_benchmark.cpp
 * @brief RTTM reflection library comprehensive benchmark
 * 
 * Benchmark categories:
 * 1. Type Lookup - Static and dynamic type resolution
 * 2. Object Creation - Instance construction
 * 3. Property Access - Read/write operations
 * 4. Method Invocation - Function calls
 * 5. Full Reflection Path - Complete dynamic workflow
 * 6. Batch Operations - Bulk processing
 */

#include <benchmark/benchmark.h>
#include "RTTM/RTTM.hpp"
#include "benchmark_common.hpp"

using namespace rttm;

// ============================================================================
// Type Registration
// ============================================================================

RTTM_REGISTRATION {
    Registry<Vector3>()
        .property("x", &Vector3::x)
        .property("y", &Vector3::y)
        .property("z", &Vector3::z)
        .method("length", &Vector3::length)
        .method("dot", &Vector3::dot);

    Registry<SimpleClass>()
        .property("intValue", &SimpleClass::intValue)
        .property("floatValue", &SimpleClass::floatValue)
        .property("stringValue", &SimpleClass::stringValue)
        .method("getInt", &SimpleClass::getInt)
        .method("setInt", &SimpleClass::setInt)
        .method("getFloat", &SimpleClass::getFloat)
        .method("setFloat", &SimpleClass::setFloat);

    Registry<ComplexClass>()
        .property("id", &ComplexClass::id)
        .property("name", &ComplexClass::name)
        .property("position", &ComplexClass::position)
        .property("scores", &ComplexClass::scores)
        .method("getId", &ComplexClass::getId)
        .method("setId", &ComplexClass::setId)
        .method("getName", &ComplexClass::getName)
        .method("setName", &ComplexClass::setName)
        .method("getPosition", &ComplexClass::getPosition);

    Registry<DeepClass>()
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
static void RTTM_TypeLookup_Static(benchmark::State& state) {
    for (auto _ : state) {
        auto handle = RTypeHandle::get<SimpleClass>();
        benchmark::DoNotOptimize(handle);
    }
}
BENCHMARK(RTTM_TypeLookup_Static);

// Dynamic type lookup via string
static void RTTM_TypeLookup_Dynamic(benchmark::State& state) {
    std::string_view name = "SimpleClass";
    for (auto _ : state) {
        auto handle = RTypeHandle::get(name);
        benchmark::DoNotOptimize(handle);
    }
}
BENCHMARK(RTTM_TypeLookup_Dynamic);

// ============================================================================
// 2. Object Creation Benchmarks
// ============================================================================

static void RTTM_ObjectCreation_Simple(benchmark::State& state) {
    auto handle = RTypeHandle::get<SimpleClass>();
    for (auto _ : state) {
        auto obj = handle.create();
        benchmark::DoNotOptimize(obj);
    }
}
BENCHMARK(RTTM_ObjectCreation_Simple);

static void RTTM_ObjectCreation_Complex(benchmark::State& state) {
    auto handle = RTypeHandle::get<ComplexClass>();
    for (auto _ : state) {
        auto obj = handle.create();
        benchmark::DoNotOptimize(obj);
    }
}
BENCHMARK(RTTM_ObjectCreation_Complex);

// ============================================================================
// 3. Property Access Benchmarks
// ============================================================================

// Level 1: Full dynamic - lookup inside loop (slowest)
static void RTTM_PropertyRead_FullDynamic(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    auto handle = RTypeHandle::get<SimpleClass>();
    
    int sum = 0;
    for (auto _ : state) {
        auto prop = handle.get_property<int>("intValue");
        sum += prop.get(obj);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_PropertyRead_FullDynamic);

// Level 2: Cached handle - lookup once (fast)
static void RTTM_PropertyRead_Cached(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    auto handle = RTypeHandle::get<SimpleClass>();
    auto prop = handle.get_property<int>("intValue");
    
    int sum = 0;
    for (auto _ : state) {
        sum += prop.get(obj);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_PropertyRead_Cached);

// Level 1: Full dynamic write
static void RTTM_PropertyWrite_FullDynamic(benchmark::State& state) {
    SimpleClass obj;
    
    auto handle = RTypeHandle::get<SimpleClass>();
    
    int i = 0;
    for (auto _ : state) {
        auto prop = handle.get_property<int>("intValue");
        prop.set(obj, i++);
        benchmark::DoNotOptimize(obj.intValue);
    }
}
BENCHMARK(RTTM_PropertyWrite_FullDynamic);

// Level 2: Cached handle write (with dependency chain for accurate measurement)
static void RTTM_PropertyWrite_Cached(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 0;
    
    auto handle = RTypeHandle::get<SimpleClass>();
    auto prop = handle.get_property<int>("intValue");
    
    int i = 0;
    for (auto _ : state) {
        i += obj.intValue;  // dependency chain
        prop.set(obj, i);
        benchmark::DoNotOptimize(obj.intValue);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_PropertyWrite_Cached);

// Multiple property access - cached handles
static void RTTM_PropertyAccess_Multiple(benchmark::State& state) {
    ComplexClass obj;
    obj.id = 42;
    obj.name = "TestObject";
    obj.position = {1.0f, 2.0f, 3.0f};
    
    auto handle = RTypeHandle::get<ComplexClass>();
    auto prop_id = handle.get_property<int>("id");
    auto prop_name = handle.get_property<std::string>("name");
    auto prop_pos = handle.get_property<Vector3>("position");
    
    for (auto _ : state) {
        auto id = prop_id.get(obj);
        auto& name = prop_name.get(obj);
        auto& pos = prop_pos.get(obj);
        benchmark::DoNotOptimize(id);
        benchmark::DoNotOptimize(name);
        benchmark::DoNotOptimize(pos);
    }
}
BENCHMARK(RTTM_PropertyAccess_Multiple);

// Deep property access - cached handles
static void RTTM_PropertyAccess_Deep(benchmark::State& state) {
    DeepClass obj;
    obj.level1 = 1; obj.level2 = 2; obj.level3 = 3;
    obj.level4 = 4; obj.level5 = 5;
    
    auto handle = RTypeHandle::get<DeepClass>();
    auto p1 = handle.get_property<int>("level1");
    auto p2 = handle.get_property<int>("level2");
    auto p3 = handle.get_property<int>("level3");
    auto p4 = handle.get_property<int>("level4");
    auto p5 = handle.get_property<int>("level5");
    
    for (auto _ : state) {
        int sum = p1.get(obj);
        sum += p2.get(obj);
        sum += p3.get(obj);
        sum += p4.get(obj);
        sum += p5.get(obj);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_PropertyAccess_Deep);

// ============================================================================
// 4. Method Invocation Benchmarks
// ============================================================================

// Level 1: Full dynamic - lookup inside loop
static void RTTM_MethodCall_FullDynamic(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    auto handle = RTypeHandle::get<SimpleClass>();
    
    int sum = 0;
    for (auto _ : state) {
        auto meth = handle.get_method("getInt", 0);
        sum += meth.call<int>(&obj);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_MethodCall_FullDynamic);

// Level 2: Cached handle - lookup once
static void RTTM_MethodCall_Cached(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    auto handle = RTypeHandle::get<SimpleClass>();
    auto meth = handle.get_method("getInt", 0);
    
    int sum = 0;
    for (auto _ : state) {
        sum += meth.call<int>(&obj);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_MethodCall_Cached);

// Level 1: Full dynamic with arg
static void RTTM_MethodCall_WithArg_FullDynamic(benchmark::State& state) {
    SimpleClass obj;
    
    auto handle = RTypeHandle::get<SimpleClass>();
    
    int i = 0;
    for (auto _ : state) {
        auto meth = handle.get_method("setInt", 1);
        meth.call<void>(&obj, i++);
        benchmark::DoNotOptimize(obj.intValue);
    }
}
BENCHMARK(RTTM_MethodCall_WithArg_FullDynamic);

// Level 2: Cached handle with arg
static void RTTM_MethodCall_WithArg_Cached(benchmark::State& state) {
    SimpleClass obj;
    
    auto handle = RTypeHandle::get<SimpleClass>();
    auto meth = handle.get_method("setInt", 1);
    
    int i = 0;
    for (auto _ : state) {
        meth.call<void>(&obj, i++);
        benchmark::DoNotOptimize(obj.intValue);
    }
}
BENCHMARK(RTTM_MethodCall_WithArg_Cached);

// Method call - complex return type
static void RTTM_MethodCall_ComplexReturn(benchmark::State& state) {
    ComplexClass obj;
    obj.position = {1.0f, 2.0f, 3.0f};
    
    auto handle = RTypeHandle::get<ComplexClass>();
    auto meth = handle.get_method("getPosition", 0);
    
    for (auto _ : state) {
        auto pos = meth.call<Vector3>(&obj);
        benchmark::DoNotOptimize(pos);
    }
}
BENCHMARK(RTTM_MethodCall_ComplexReturn);

// ============================================================================
// 5. Full Reflection Path Benchmarks
// ============================================================================

// Complete dynamic path: type lookup + bind + property access
static void RTTM_FullPath_PropertyAccess(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    std::string_view type_name = "SimpleClass";
    std::string_view prop_name = "intValue";
    
    int sum = 0;
    for (auto _ : state) {
        auto handle = RTypeHandle::get(type_name);
        auto bound = handle.bind(obj);
        sum += bound.get<int>(prop_name);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_FullPath_PropertyAccess);

// Complete dynamic path: type lookup + bind + method call
static void RTTM_FullPath_MethodCall(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    std::string_view type_name = "SimpleClass";
    std::string_view method_name = "getInt";
    
    int sum = 0;
    for (auto _ : state) {
        auto handle = RTypeHandle::get(type_name);
        auto bound = handle.bind(obj);
        sum += bound.call<int>(method_name);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_FullPath_MethodCall);

// Full path with object creation
static void RTTM_FullPath_CreateAndAccess(benchmark::State& state) {
    std::string_view type_name = "SimpleClass";
    
    for (auto _ : state) {
        auto handle = RTypeHandle::get(type_name);
        auto instance = handle.create();
        auto* obj = static_cast<SimpleClass*>(instance.get());
        
        auto bound = handle.bind(*obj);
        bound.set("intValue", 42);
        int val = bound.get<int>("intValue");
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK(RTTM_FullPath_CreateAndAccess);

// ============================================================================
// 6. Batch Operations Benchmarks
// ============================================================================

// Batch object creation
static void RTTM_Batch_Creation(benchmark::State& state) {
    auto handle = RTypeHandle::get<SimpleClass>();
    
    for (auto _ : state) {
        std::vector<std::shared_ptr<void>> objects;
        objects.reserve(100);
        for (int i = 0; i < 100; ++i) {
            objects.push_back(handle.create());
        }
        benchmark::DoNotOptimize(objects);
    }
}
BENCHMARK(RTTM_Batch_Creation);

// Batch property access (cached property handle)
static void RTTM_Batch_PropertyAccess(benchmark::State& state) {
    std::vector<SimpleClass> objects(100);
    for (int i = 0; i < 100; ++i) {
        objects[i].intValue = i;
    }
    
    auto handle = RTypeHandle::get<SimpleClass>();
    auto prop = handle.get_property<int>("intValue");
    
    for (auto _ : state) {
        int sum = 0;
        for (auto& obj : objects) {
            sum += prop.get(obj);
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_Batch_PropertyAccess);

// Batch method calls (cached method handle)
static void RTTM_Batch_MethodCalls(benchmark::State& state) {
    std::vector<SimpleClass> objects(100);
    for (int i = 0; i < 100; ++i) {
        objects[i].intValue = i;
    }
    
    auto handle = RTypeHandle::get<SimpleClass>();
    auto meth = handle.get_method("getInt", 0);
    
    for (auto _ : state) {
        int sum = 0;
        for (auto& obj : objects) {
            sum += meth.call<int>(&obj);
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_Batch_MethodCalls);

// ============================================================================
// 7. Property Enumeration
// ============================================================================

static void RTTM_PropertyEnumeration(benchmark::State& state) {
    auto handle = RTypeHandle::get<ComplexClass>();
    
    for (auto _ : state) {
        const auto& props = handle.property_names();
        std::size_t hash = 0;
        for (const auto& name : props) {
            hash ^= std::hash<std::string_view>{}(name);
        }
        benchmark::DoNotOptimize(hash);
    }
}
BENCHMARK(RTTM_PropertyEnumeration);

static void RTTM_MethodEnumeration(benchmark::State& state) {
    auto handle = RTypeHandle::get<ComplexClass>();
    
    for (auto _ : state) {
        const auto& methods = handle.method_names();
        std::size_t hash = 0;
        for (const auto& name : methods) {
            hash ^= std::hash<std::string_view>{}(name);
        }
        benchmark::DoNotOptimize(hash);
    }
}
BENCHMARK(RTTM_MethodEnumeration);

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
