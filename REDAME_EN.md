```markdown
<div align="center">
  <img src="imgs/logo.svg" alt="Logo" />
</div>

[EN](README_EN.md) | [中文](README.md)

# RTTM (Runtime Turbo Mirror)

RTTM (Runtime Turbo Mirror) is a lightweight dynamic reflection library built using the C++17 standard library with no external dependencies. It is designed for game engines or any other high-performance applications that require runtime reflection. RTTM supports MSVC, GCC, and Clang compilers. The library allows reflection on classes, structures, enums, variables, and functions at runtime, and supports dynamic object creation and method invocation.

## Features

- **Built on C++17 with no external dependencies**
- **Compatible with MSVC, GCC, and Clang**
- **Register enums, structures, templated classes, as well as global variables and functions**
- **Dynamic object creation and member function calls**
- **High performance: benchmarks show lower call latency compared to RTTR**
- **Ease of Use: Simple and intuitive API with support for method chaining**

## Usage Examples

This section demonstrates how to use various modules of RTTM with complete code examples to help you get started quickly.

### 1. Dynamic Reflection Registration and Object Manipulation

Reflection for both structures and classes is handled in the same way.

#### Include Header File

```cpp
#include <iostream>
#include "RTTM/RTTM.hpp" // Include the RTTM header
using namespace RTTM;    // Use the RTTM namespace
```

#### Enum Registration

```cpp
enum class TypeEnum {
    CLASS = -1,
    VARIABLE,
};

RTTM_REGISTRATION {
    Enum_<TypeEnum>()
        .value("CLASS", TypeEnum::CLASS)
        .value("VARIABLE", TypeEnum::VARIABLE);
}

int main() {
    auto type = Enum::Get<TypeEnum>();
    TypeEnum variable = type.GetValue("VARIABLE");
    return 0;
}
```

#### Global Variables and Global Functions

```cpp
// Register a global variable
Global::RegisterVariable("var", 0);

// Get and set the global variable's value
int var = Global::GetVariable<int>("var");
Global::GetVariable<int>("var") = 20;

// Define and register a global function
int foo(int a) {
    return a;
}
Global::RegisterGlobalMethod("foo", foo);

Method<int> fooFunc = Global::GetMethod<int(int)>("foo");
int result = fooFunc(10); // Invoke the global function

// Register a global function using a lambda expression
Global::RegisterGlobalMethod("fooLambda", [](int a) { return a; });
```

#### Registering and Using Reflectable Classes

Suppose we have a class that requires reflection:

```cpp
class A {
public:
    A() = default;
    A(int num) : num(num) {}
    int num = 0;
    int foo() { return num; }
    int mul(int a) { return num * a; }
};
```

**Registration:**

1. **Using the macro outside a function:**

    ```cpp
    RTTM_REGISTRATION {
        Registry_<A>()          // Register class A
            .property("num", &A::num)  // Register property num
            .method("foo", &A::foo)    // Register method foo
            .method("mul", &A::mul)    // Register method mul
            .constructor<int>();       // Register constructor with an integer parameter
    }
    ```

2. **Registering within a function:**

    ```cpp
    int main(){
        Registry_<A>()          // Register class A
            .property("num", &A::num)  // Register property num
            .method("foo", &A::foo)    // Register method foo
            .method("mul", &A::mul)    // Register method mul     
            .constructor<int>();       // Register constructor with an integer parameter
        return 0;
    }
    ```

**Creating and Using Objects:**

```cpp
// Create objects by type name
Ref<RType> typeByName = RType::Get("A");

// Create objects by type
Ref<RType> typeByType = RType::Get<A>();

// Instantiate objects (using default or parameterized constructors)
typeByName->Create();
typeByName->Create(10);

// Get property values
Ref<RType> numProp = typeByName->GetProperty("num");
int numValue = typeByName->GetProperty<int>("num");
int numValue2 = numProp->As<int>();

// Invoke methods by name
int resultFoo = typeByName->Invoke<int>("foo");

// Or, obtain a method object and invoke it
auto mulMethod = typeByName->GetMethod<int(int)>("mul");
int resultMul = mulMethod(2);
```

#### ECS Implementation Example

Below is an example demonstrating how to define components and entities, and retrieve component instances using reflection.

```cpp
#include <RTTM/RTTM.hpp>
#include <iostream>
#include <cmath>
#include <RTTM/Entity.hpp>

class Transform {
public:
    struct {
        float x, y, z;
    } position;

    struct {
        float x, y, z;
    } rotation;

    struct {
        float x, y, z;
    } scale;

    // Compute the transformation matrix combining translation, rotation (using ZYX order for Euler angles), and scaling.
    float* GetMatrix() {
        float* m = new float[16];

        float radX = rotation.x;
        float radY = rotation.y;
        float radZ = rotation.z;

        float cx = std::cos(radX);
        float sx = std::sin(radX);
        float cy = std::cos(radY);
        float sy = std::sin(radY);
        float cz = std::cos(radZ);
        float sz = std::sin(radZ);

        float r00 = cz * cy;
        float r01 = cz * sy * sx - sz * cx;
        float r02 = cz * sy * cx + sz * sx;

        float r10 = sz * cy;
        float r11 = sz * sy * sx + cz * cx;
        float r12 = sz * sy * cx - cz * sx;

        float r20 = -sy;
        float r21 = cy * sx;
        float r22 = cy * cx;

        // Apply scaling to the rotation matrix
        r00 *= scale.x; r01 *= scale.x; r02 *= scale.x;
        r10 *= scale.y; r11 *= scale.y; r12 *= scale.y;
        r20 *= scale.z; r21 *= scale.z; r22 *= scale.z;

        // Fill the 4x4 matrix in row-major order
        m[0]  = r00; m[1]  = r01; m[2]  = r02; m[3]  = 0.0f;
        m[4]  = r10; m[5]  = r11; m[6]  = r12; m[7]  = 0.0f;
        m[8]  = r20; m[9]  = r21; m[10] = r22; m[11] = 0.0f;
        m[12] = position.x; m[13] = position.y; m[14] = position.z; m[15] = 1.0f;

        return m;
    }
};

REQUIRE_COMPONENT(Transform)

class GameObject : public RTTM::Entity {
public:
    Transform& transform() {
        return GetComponent<Transform>();
    }
};

int main() {
    try {
        GameObject gameObject;
        Transform& transform = gameObject.transform();

        transform.position = {1.0f, 2.0f, 3.0f};
        transform.rotation = {0.0f, 0.0f, 0.0f};
        transform.scale    = {1.0f, 1.0f, 1.0f};

        float* matrix = transform.GetMatrix();

        std::cout << "Transformation Matrix:" << std::endl;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                std::cout << matrix[i * 4 + j] << " ";
            }
            std::cout << std::endl;
        }

        // Sample output:
        // Transformation Matrix:
        // 1 0 0 0
        // 0 1 0 0
        // -0 0 1 0
        // 1 2 3 1

        delete[] matrix;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

### 2. Benchmark Tests

The benchmark tests compare RTTM with RTTR in terms of performance for reflection-based method calls and serialization/deserialization tasks.

#### RTTM Benchmark Test

```cpp
#include <chrono>
#include <iostream>
#include "RTTM/RTTM.hpp"

using namespace RTTM;

class A {
public:
    int num = 0;
    A() = default;
    A(int num) : num(num) {}

    int Add(int a) {
        num += a;
        return num;
    }
};

class B {
public:
    A a;
};

class C {
public:
    B b;
};

RTTM_REGISTRATION {
    Registry_<A>()
        .property("num", &A::num)
        .constructor<int>()
        .method("Add", &A::Add);
    Registry_<B>()
        .property("a", &B::a);
}

int main() {
    Registry_<C>().property("b", &C::b);
    Ref<RType> ct = RType::Get("C");
    if (!ct) {
        std::cerr << "Unable to retrieve type C" << std::endl;
        return -1;
    }
    ct->Create();
    auto add = ct->GetProperty("b")
                  ->GetProperty("a")
                  ->GetMethod<int(int)>("Add");
    if (!add.IsValid()) {
        std::cerr << "Unable to retrieve method Add" << std::endl;
        return -1;
    }
    const int iterations = 1000000000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        int result = add(1);
        (void)result;
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Final result: " 
              << ct->GetProperty("b")
                    ->GetProperty("a")
                    ->GetProperty("num")
                    ->As<int>() 
              << std::endl;
    std::cout << "[RTTM] Executing " << iterations 
              << " method calls took " << elapsed_ms << " ms" << std::endl;
    std::cout << "[RTTM] Average time per call: " 
              << (elapsed_ms / iterations) << " ms" << std::endl;
    return 0;
}
```

#### RTTR Benchmark Test

```cpp
#include <chrono>
#include <iostream>
#include <rttr/registration>
#include <rttr/type>

using namespace std;
using namespace rttr;

class A {
public:
    int num = 0;
    A() = default;
    A(int num) : num(num) { }
    int Add(int a) {
        num += a;
        return num;
    }
};

class B {
public:
    A a;
};

class C {
public:
    B b;
};

RTTR_REGISTRATION {
    registration::class_<A>("A")
        .constructor<>()()
        .constructor<int>()()
        .property("num", &A::num)
        .method("Add", &A::Add);
    registration::class_<B>("B")
        .constructor<>()()
        .property("a", &B::a);
    registration::class_<C>("C")
        .constructor<>()()
        .property("b", &C::b);
}

int main() {
    const int iterations = 1000000000;
    type typeC = type::get_by_name("C");
    variant varC = typeC.create();
    if (!varC.is_valid()) {
        cerr << "Unable to create an instance of type C" << endl;
        return -1;
    }
    property propB = typeC.get_property("b");
    variant varB = propB.get_value(varC);
    type typeB = type::get_by_name("B");
    property propA = typeB.get_property("a");
    variant varA = propA.get_value(varB);
    type typeA = type::get_by_name("A");
    variant varA_new = typeA.create({10});
    if (!varA_new.is_valid()) {
        cerr << "Unable to create an instance of type A with parameters" << endl;
        return -1;
    }
    propA.set_value(varB, varA_new);
    varA = propA.get_value(varB);
    method methAdd = typeA.get_method("Add");
    if (!methAdd.is_valid()) {
        cerr << "Unable to retrieve method Add" << endl;
        return -1;
    }
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        variant result = methAdd.invoke(varA, 1);
        (void)result;
    }
    auto end = chrono::high_resolution_clock::now();
    double elapsed_ms = chrono::duration<double, chrono::milliseconds::period>(end - start).count();
    property propNum = typeA.get_property("num");
    variant varNum = propNum.get_value(varA);
    int finalResult = varNum.get_value<int>();

    cout << "Final result: " << finalResult << endl;
    cout << "[RTTR] Executing " << iterations 
         << " method calls took " << elapsed_ms << " ms" << endl;
    cout << "[RTTR] Average time per call: " 
         << (elapsed_ms / iterations) << " ms" << endl;
    return 0;
}
```

### 3. Serialization and Deserialization

RTTM includes built-in support for serializing objects to JSON and deserializing JSON back to objects.

#### RTTM Serialization Example

```cpp
#include <chrono>
#include <iostream>
#include "RTTM/RTTM.hpp"
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
using namespace RTTM;

class TestClass {
public:
    float C;
};

class JsonSerializable {
public:
    int A;
    std::string B;
    TestClass D;
};

RTTM_REGISTRATION {
    Registry_<TestClass>().property("C", &TestClass::C);
    Registry_<JsonSerializable>()
        .property("A", &JsonSerializable::A)
        .property("B", &JsonSerializable::B)
        .property("D", &JsonSerializable::D);
}

json Serialize(const RType& type) {
    json j;
    for (const auto& name : type.GetPropertyNames()) {
        auto prop = type.GetProperty(name);
        if (prop->Is<int>()) {
            j[name] = prop->As<int>();
        } else if (prop->Is<std::string>()) {
            j[name] = prop->As<std::string>();
        } else if (prop->Is<float>()) {
            j[name] = prop->As<float>();
        } else if (prop->Is<double>()) {
            j[name] = prop->As<double>();
        } else if (prop->Is<long>()) {
            j[name] = prop->As<long>();
        } else if (prop->Is<unsigned long>()) {
            j[name] = prop->As<unsigned long>();
        } else if (prop->Is<long long>()) {
            j[name] = prop->As<long long>();
        } else if (prop->Is<unsigned long long>()) {
            j[name] = prop->As<unsigned long long>();
        } else if (prop->Is<short>()) {
            j[name] = prop->As<short>();
        } else if (prop->Is<unsigned short>()) {
            j[name] = prop->As<unsigned short>();
        } else if (prop->Is<bool>()) {
            j[name] = prop->As<bool>();
        } else if (prop->Is<size_t>()) {
            j[name] = prop->As<size_t>();
        } else if (prop->IsClass()) {
            j[name] = Serialize(*prop);
        }
    }
    return j;
}

void Deserialize(const RType& tp, const json& js) {
    for (auto& name : tp.GetPropertyNames()) {
        auto prop = tp.GetProperty(name);
        if (js.find(name) == js.end())
            continue;
        auto value = js[name];
        if (prop->Is<int>())
            prop->SetValue(value.get<int>());
        else if (prop->Is<std::string>())
            prop->SetValue(value.get<std::string>());
        else if (prop->Is<float>())
            prop->SetValue(value.get<float>());
        else if (prop->Is<double>())
            prop->SetValue(value.get<double>());
        else if (prop->Is<long>())
            prop->SetValue(value.get<long>());
        else if (prop->Is<unsigned long>())
            prop->SetValue(value.get<unsigned long>());
        else if (prop->Is<long long>())
            prop->SetValue(value.get<long long>());
        else if (prop->Is<unsigned long long>())
            prop->SetValue(value.get<unsigned long long>());
        else if (prop->Is<short>())
            prop->SetValue(value.get<short>());
        else if (prop->Is<unsigned short>())
            prop->SetValue(value.get<unsigned short>());
        else if (prop->Is<bool>())
            prop->SetValue(value.get<bool>());
        else if (prop->Is<size_t>())
            prop->SetValue(value.get<size_t>());
        else if (prop->IsClass())
            Deserialize(*prop, value);
    }
}

int main(int argc, char** argv) {
    try {
        int iterations = std::stoi(argv[1]);
        auto jst = RType::Get("JsonSerializable");
        jst.Create();
        auto& inst = jst.As<JsonSerializable>();

        inst.A = 1;
        inst.B = "Hello RTTM";
        inst.D.C = 10000.f;

        std::cout << "Serializing " << iterations << " times..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            (void)Serialize(jst);
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << Serialize(jst).dump() << std::endl;
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "[RTTM] Serializing " << iterations << " times took: "
                  << elapsed_ms << " ms\n";
        std::cout << "[RTTM] Average per serialization: "
                  << (elapsed_ms / iterations) << " ms\n\n";

        auto js = Serialize(jst);
        std::cout << "Deserializing " << iterations << " times..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        auto nt = RType::Get("JsonSerializable");
        nt.Create();
        for (int i = 0; i < iterations; i++) {
            Deserialize(nt, js);
        }
        end = std::chrono::high_resolution_clock::now();
        elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "[RTTM] Deserializing " << iterations << " times took: "
                  << elapsed_ms << " ms\n";
        std::cout << "[RTTM] Average per deserialization: "
                  << (elapsed_ms / iterations) << " ms\n\n";

        auto res = RType::Get("JsonSerializable");
        res.Create();
        Deserialize(res, js);
        std::cout << res.As<JsonSerializable>().A << std::endl;
        std::cout << res.As<JsonSerializable>().B << std::endl;
        std::cout << res.As<JsonSerializable>().D.C << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
```

### 4. Build Requirements and Guidelines

- Requires C++17.
- Compatible with MSVC, GCC, and Clang.
- No external dependencies – only the standard library is used.

#### Build Steps

1. Include the RTTM source into your project.
2. Enable C++17 in your compiler:
   - For GCC and Clang: add `-std=c++17`
   - For MSVC: add `/std:c++17`
3. Refer to the examples above to learn how to register types, variables, and methods, and to perform dynamic operations via reflection.

### 5. License

This project is licensed under the MIT License. Please refer to the LICENSE file for more details.

### 6. Contribution

Contributions and feedback are welcome! To contribute:
1. Fork the project.
2. Create a branch and commit your changes.
3. Submit a pull request, and we will review and merge it.

---

RTTM is dedicated to providing developers with a simple, fast, and extensible reflection solution for game engines and other high-performance applications. Thank you for your interest and support!