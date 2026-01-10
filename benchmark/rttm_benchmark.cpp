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
#include "RTTM/detail/PropertyHandle.hpp"
#include "RTTM/detail/Instance.hpp"
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
// 4b. TypedMethodHandle - Zero overhead method calls
// ============================================================================

// TypedMethodHandle: compile-time typed, zero overhead
static void RTTM_TypedMethodCall_NoArgs(benchmark::State& state) {
    SimpleClass obj;
    obj.intValue = 42;
    
    // Create typed handle - stores direct function pointer
    auto meth = TypedMethodHandle<int()>::from_const<SimpleClass, &SimpleClass::getInt>();
    
    int sum = 0;
    for (auto _ : state) {
        sum += meth.call(obj);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_TypedMethodCall_NoArgs);

// TypedMethodHandle with argument
static void RTTM_TypedMethodCall_WithArg(benchmark::State& state) {
    SimpleClass obj;
    
    auto meth = TypedMethodHandle<void(int)>::from<SimpleClass, &SimpleClass::setInt>();
    
    int i = 0;
    for (auto _ : state) {
        meth.call(obj, i++);
        benchmark::DoNotOptimize(obj.intValue);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_TypedMethodCall_WithArg);

// Batch TypedMethodHandle calls
static void RTTM_Batch_TypedMethodCalls(benchmark::State& state) {
    std::vector<SimpleClass> objects(100);
    for (int i = 0; i < 100; ++i) {
        objects[i].intValue = i;
    }
    
    auto meth = TypedMethodHandle<int()>::from_const<SimpleClass, &SimpleClass::getInt>();
    
    for (auto _ : state) {
        int sum = 0;
        for (auto& obj : objects) {
            sum += meth.call(obj);
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(RTTM_Batch_TypedMethodCalls);

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
    state.SetItemsProcessed(state.iterations() * 100);
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
    state.SetItemsProcessed(state.iterations() * 100);
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
    state.SetItemsProcessed(state.iterations() * 100);
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
// 8. Pure Dynamic API (Instance + Variant) - RTTR-like
// ============================================================================

// Create instance by type name (no template)
static void RTTM_Instance_Create(benchmark::State& state) {
    std::string_view type_name = "SimpleClass";
    
    for (auto _ : state) {
        auto inst = Instance::create(type_name);
        benchmark::DoNotOptimize(inst.get_raw());
    }
}
BENCHMARK(RTTM_Instance_Create);

// Pure dynamic property read (returns Variant)
static void RTTM_Instance_PropertyRead(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    inst.set_property_value("intValue", 42);
    
    for (auto _ : state) {
        Variant v = inst.get_property("intValue");
        benchmark::DoNotOptimize(v.get_raw());
    }
}
BENCHMARK(RTTM_Instance_PropertyRead);

// Pure dynamic property read (direct value, no Variant)
static void RTTM_Instance_PropertyRead_Direct(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    inst.set_property_value("intValue", 42);
    
    int sum = 0;
    for (auto _ : state) {
        sum += inst.get_property_value<int>("intValue");
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_Instance_PropertyRead_Direct);

// Pure dynamic property write (from Variant)
static void RTTM_Instance_PropertyWrite(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    
    int i = 0;
    for (auto _ : state) {
        inst.set_property("intValue", Variant::create(i++));
        benchmark::DoNotOptimize(inst.get_raw());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_Instance_PropertyWrite);

// Pure dynamic property write (template overload, like RTTR's set_value)
static void RTTM_Instance_PropertyWrite_Template(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    
    int i = 0;
    for (auto _ : state) {
        inst.set_property("intValue", i++);  // 直接传 int，不需要 Variant
        benchmark::DoNotOptimize(inst.get_raw());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_Instance_PropertyWrite_Template);

// Pure dynamic property write (direct value, no Variant - like RTTR)
static void RTTM_Instance_PropertyWrite_Value(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    
    int i = 0;
    for (auto _ : state) {
        inst.set_property_value("intValue", i++);
        benchmark::DoNotOptimize(inst.get_raw());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_Instance_PropertyWrite_Value);

// Test just the member lookup overhead
static void RTTM_Instance_MemberLookup(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    const auto* type_info = inst.type_info();
    
    for (auto _ : state) {
        auto member = type_info->find_member("intValue");
        benchmark::DoNotOptimize(member);
    }
}
BENCHMARK(RTTM_Instance_MemberLookup);

// Test raw property write (no lookup, just write)
static void RTTM_Instance_RawPropertyWrite(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    const auto* type_info = inst.type_info();
    auto member = type_info->find_member("intValue");
    void* obj_ptr = inst.get_raw();
    void* prop_ptr = static_cast<char*>(obj_ptr) + member->offset;
    
    int i = 0;
    for (auto _ : state) {
        *static_cast<int*>(prop_ptr) = i++;
        benchmark::DoNotOptimize(obj_ptr);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_Instance_RawPropertyWrite);

// Test property write with lookup but no type check
static void RTTM_Instance_PropertyWrite_NoTypeCheck(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    const auto* type_info = inst.type_info();
    void* obj_ptr = inst.get_raw();
    
    int i = 0;
    for (auto _ : state) {
        auto member = type_info->find_member("intValue");
        void* prop_ptr = static_cast<char*>(obj_ptr) + member->offset;
        *static_cast<int*>(prop_ptr) = i++;
        benchmark::DoNotOptimize(obj_ptr);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_Instance_PropertyWrite_NoTypeCheck);

// Test property write with get_raw() call each time
static void RTTM_Instance_PropertyWrite_WithGetRaw(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    const auto* type_info = inst.type_info();
    
    int i = 0;
    for (auto _ : state) {
        auto member = type_info->find_member("intValue");
        void* obj_ptr = inst.get_raw();
        void* prop_ptr = static_cast<char*>(obj_ptr) + member->offset;
        *static_cast<int*>(prop_ptr) = i++;
        benchmark::DoNotOptimize(obj_ptr);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_Instance_PropertyWrite_WithGetRaw);

// Test set_property_direct directly
static void RTTM_Instance_PropertyWrite_Direct(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    
    int i = 0;
    for (auto _ : state) {
        inst.set_property_direct("intValue", i++);
        benchmark::DoNotOptimize(inst.get_raw());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_Instance_PropertyWrite_Direct);

// Cached DynamicProperty read
static void RTTM_DynamicProperty_Read_Cached(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    inst.set_property_value("intValue", 42);
    auto prop = inst.get_property_handle("intValue");
    void* obj = inst.get_raw();
    
    int sum = 0;
    for (auto _ : state) {
        sum += prop.get_value_direct<int>(obj);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_DynamicProperty_Read_Cached);

// Cached DynamicProperty write
static void RTTM_DynamicProperty_Write_Cached(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    auto prop = inst.get_property_handle("intValue");
    void* obj = inst.get_raw();
    
    int i = 0;
    for (auto _ : state) {
        prop.set_value_direct(obj, i++);
        benchmark::DoNotOptimize(obj);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_DynamicProperty_Write_Cached);

// Pure dynamic method call (returns Variant)
static void RTTM_Instance_MethodCall(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    inst.set_property_value("intValue", 42);
    
    int sum = 0;
    for (auto _ : state) {
        Variant result = inst.invoke("getInt");
        sum += result.get<int>();
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_Instance_MethodCall);

// Pure dynamic method call with args
static void RTTM_Instance_MethodCall_WithArg(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    
    int i = 0;
    for (auto _ : state) {
        std::array<Variant, 1> args = {Variant::create(i++)};
        inst.invoke("setInt", std::span<const Variant>{args});
        benchmark::DoNotOptimize(inst.get_raw());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_Instance_MethodCall_WithArg);

// Pure dynamic method call with args (template overload, like RTTR's invoke)
static void RTTM_Instance_MethodCall_WithArg_Template(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    
    int i = 0;
    for (auto _ : state) {
        inst.invoke("setInt", i++);  // 直接传 int，不需要 Variant
        benchmark::DoNotOptimize(inst.get_raw());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_Instance_MethodCall_WithArg_Template);

// Cached DynamicMethod call (no args)
static void RTTM_DynamicMethod_Call_Cached(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    inst.set_property_value("intValue", 42);
    auto meth = inst.get_method_handle("getInt", 0);
    void* obj = inst.get_raw();
    
    int sum = 0;
    for (auto _ : state) {
        Variant result = meth.invoke(obj);
        sum += result.get<int>();
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(RTTM_DynamicMethod_Call_Cached);

// Cached DynamicMethod call (with arg)
static void RTTM_DynamicMethod_Call_WithArg_Cached(benchmark::State& state) {
    auto inst = Instance::create("SimpleClass");
    auto meth = inst.get_method_handle("setInt", 1);
    void* obj = inst.get_raw();
    
    int i = 0;
    for (auto _ : state) {
        std::array<Variant, 1> args = {Variant::create(i++)};
        meth.invoke(obj, args);
        benchmark::DoNotOptimize(obj);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(RTTM_DynamicMethod_Call_WithArg_Cached);

// ============================================================================
// 8. Baseline - Direct Access (for comparison, 8x unrolled)
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
