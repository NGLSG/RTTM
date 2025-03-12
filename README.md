<div align="center">
  <img src="imgs/logo.svg" alt="Logo" />
</div>

[EN](README_EN.md) | [中文](README.md)

# RTTM (Runtime Turbo Mirror)

RTTM (Runtime Turbo Mirror) 是一个基于 C++17 标准库、无外部依赖的轻量级动态反射库。它适用于游戏引擎或其他需要高性能反射的应用，支持 MSVC、GCC 和 Clang 编译器。该库允许在运行时对类、结构体、枚举、变量和函数进行反射，并支持动态对象创建和方法调用。

## 特性

- **基于 C++17 标准库，无外部依赖**
- **支持 MSVC、GCC 和 Clang 编译**
- **注册枚举、结构体、模板类以及全局变量和函数**
- **动态对象创建和成员函数调用**
- **高性能：在基准测试中显示出比 RTTR 更低的调用延迟**
- **易用性：API 简洁且支持链式调用**

## 使用示例

本部分展示了 RTTM 各模块的使用方法，提供了完整的示例代码，帮助开发者快速上手。

### 1. 动态反射注册及对象操作

对于 `struct` 和 `class` 的反射操作采用相同处理方式。

#### 引入头文件

```cpp
#include <iostream>
#include "RTTM/RTTM.hpp" // 引入 RTTM 头文件
using namespace RTTM;    // 使用 RTTM 命名空间
```

#### 枚举类型注册

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

#### 全局变量与全局函数注册

```cpp
// 创建全局变量
Global::RegisterVariable("var", 0);

// 获取和设置全局变量值
int var = Global::GetVariable<int>("var");
Global::GetVariable<int>("var") = 20;

// 定义普通函数并注册
int foo(int a) {
    return a;
}
Global::RegisterGlobalMethod("foo", foo);

Method<int> fooFunc = Global::GetMethod<int(int)>("foo");
int result = fooFunc(10); // 调用全局函数

// 使用 lambda 表达式注册全局函数
Global::RegisterGlobalMethod("fooLambda", [](int a) { return a; });
```

#### 反射类的注册与使用

假设有如下需要反射的类：

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

**注册反射信息：**

1. **通过宏在函数外注册**

    ```cpp
    RTTM_REGISTRATION {
        Registry_<A>()          // 注册类 A
            .property("num", &A::num)  // 注册属性 num
            .method("foo", &A::foo)    // 注册方法 foo
            .method("mul", &A::mul)    // 注册方法 mul
            .constructor<int>();       // 注册带参数构造函数
    }
    ```

2. **在函数内注册**

    ```cpp
    int main(){
        Registry_<A>()          // 注册类 A
            .property("num", &A::num)  // 注册属性 num
            .method("foo", &A::foo)    // 注册方法 foo
            .method("mul", &A::mul)    // 注册方法 mul     
            .constructor<int>();       // 注册带参数构造函数
        return 0;
    }
    ```

**对象创建与操作：**

```cpp
// 通过类型名称创建对象
Ref<RType> typeByName = RType::Get("A");

// 通过类型创建对象
Ref<RType> typeByType = RType::Get<A>();

// 实例化对象（使用默认构造函数或带参数构造函数）
typeByName->Create();
typeByName->Create(10);

// 获取属性值
Ref<RType> numProp = typeByName->GetProperty("num");
int numValue = typeByName->GetProperty<int>("num");
int numValue2 = numProp->As<int>();

// 通过名称调用方法
int resultFoo = typeByName->Invoke<int>("foo");

// 或者先获取方法对象后调用
auto mulMethod = typeByName->GetMethod<int(int)>("mul");
int resultMul = mulMethod(2);
```

#### ECS 实现示例

以下示例演示了如何定义组件和实体，以及如何使用反射获取组件实例。

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

    // 计算变换矩阵：依照平移、旋转（以 ZYX 顺序的欧拉角）和缩放来组合
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

        // 将缩放信息应用于旋转矩阵
        r00 *= scale.x; r01 *= scale.x; r02 *= scale.x;
        r10 *= scale.y; r11 *= scale.y; r12 *= scale.y;
        r20 *= scale.z; r21 *= scale.z; r22 *= scale.z;

        // 以行主序填充 4x4 变换矩阵
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

        // 输出示例：
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

### 2. 基准测试

RTTM 与 RTTR 的基准测试展示了两者在反射调用以及序列化/反序列化操作上的性能对比。

#### RTTM 基准测试

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
        std::cerr << "无法获取类型 C" << std::endl;
        return -1;
    }
    ct->Create();
    auto add = ct->GetProperty("b")
                  ->GetProperty("a")
                  ->GetMethod<int(int)>("Add");
    if (!add.IsValid()) {
        std::cerr << "无法获取方法 Add" << std::endl;
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

    std::cout << "最终结果: " 
              << ct->GetProperty("b")
                    ->GetProperty("a")
                    ->GetProperty("num")
                    ->As<int>() 
              << std::endl;
    std::cout << "[RTTM] 执行 " << iterations 
              << " 次方法调用耗时: " << elapsed_ms << " 毫秒" << std::endl;
    std::cout << "[RTTM] 平均每次调用耗时: " 
              << (elapsed_ms / iterations) << " 毫秒" << std::endl;
    return 0;
}
```

#### RTTR 基准测试

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
        cerr << "无法创建 C 类型的实例" << endl;
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
        cerr << "无法创建带参数的 A 类型实例" << endl;
        return -1;
    }
    propA.set_value(varB, varA_new);
    varA = propA.get_value(varB);
    method methAdd = typeA.get_method("Add");
    if (!methAdd.is_valid()) {
        cerr << "无法获取方法 Add" << endl;
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

    cout << "最终结果: " << finalResult << endl;
    cout << "[RTTR] 执行 " << iterations 
         << " 次方法调用耗时: " << elapsed_ms << " 毫秒" << endl;
    cout << "[RTTR] 平均每次调用耗时: " 
         << (elapsed_ms / iterations) << " 毫秒" << endl;
    return 0;
}
```

### 3. 序列化与反序列化

RTTM 提供了内置支持，用于将对象序列化为 JSON 格式，以及从 JSON 反序列化回对象。

#### RTTM 序列化示例

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

### 4. 编译要求与构建指导

- 使用 C++17 标准。
- 支持 MSVC、GCC 与 Clang 编译器。
- 无外部依赖，仅基于标准库（RTTM 内部本身即无依赖）。

#### 构建步骤

1. 将 RTTM 的源码包含到项目中。
2. 在编译选项中启用 C++17 支持：
   - 对于 GCC 和 Clang 使用： `-std=c++17`
   - 对于 MSVC 使用： `/std:c++17`
3. 参考上述示例代码，学习如何注册类型、变量和方法，以及如何利用反射进行动态操作。

### 5. 许可

本项目采用 MIT 许可协议。有关详细信息，请查阅 LICENSE 文件。

### 6. 贡献

欢迎任何形式的贡献与反馈！请通过以下步骤参与贡献：
1. Fork 本项目。
2. 创建分支并提交你的改进。
3. 提交 Pull Request，我们将尽快审阅并合并。

---

RTTM 致力于为开发者在游戏引擎和其他高性能应用中提供一个简单、快速、可扩展的反射解决方案。感谢你的关注与使用！