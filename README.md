<div align="center">
  <img src="imgs/logo.svg" alt="Logo" />
</div>

[EN](README_EN.md) | [中文](README.md)

# RTTM (Runtime Turbo Mirror)

RTTM (Runtime Turbo Mirror) 是一个基于 C++17 标准库、无外部依赖的轻量级动态反射库，支持 MSVC、GCC 和 Clang 编译器。该库允许在运行时对类、结构体、枚举、变量和函数进行反射，并支持动态对象创建和方法调用。

## 特性

- [x] 基于 C++17 标准库，无外部依赖
- [x] 支持 MSVC、GCC 和 Clang 编译
- [x] 注册枚举、结构体、模板类以及全局变量和函数
- [x] 动态对象创建和成员函数调用
- [x] 高性能：在基准测试中显示出比 RTTR 更低的调用延迟
- [x] 易用性：简单易懂的 API，支持链式调用

## 使用示例

本部分展示了该库各个模块的使用方法，并提供了完整的示例文件。

### 1. 动态反射注册及对象操作

对于 `struct` 的操作与 `class` 操作均相同。

#### 引入头文件

```cpp
#include <iostream>
#include "RTTM/Serializable.hpp" // 引入 RTTM 头文件
using namespace RTTM;             // 使用 RTTM 命名空间
```

#### 枚举类型注册

```cpp
enum class TypeEnum {
    CLASS = -1,
    VARIABLE,
};

SERIALIZABLE_REGISTRATION {
    Enum_<TypeEnum>()
        .value("CLASS", TypeEnum::CLASS)
        .value("VARIABLE", TypeEnum::VARIABLE);
}

int main() {
    IEnum type = IEnum::Get<TypeEnum>();
    TypeEnum variable = type->GetValue<TypeEnum>("VARIABLE");
    return 0;
}
```

#### 创建全局变量

```cpp
// 创建全局变量
Serializable::RegisterVariable("var", 0);
// 获取全局变量的值
int var = *Serializable::GetVariable<int>("var");
// 设置全局变量的值
*Serializable::GetVariable<int>("var") = 20;
```

#### 创建全局函数

```cpp
// 定义普通函数
int foo(int a) { 
    return a; 
}
Serializable::RegisterGlobalMethod("foo", foo); // 注册全局函数
Function fooFunc = Serializable::GetGlobalMethod("foo"); // 获取全局函数
int result = fooFunc.Invoke<int>(10); // 调用全局函数

// 使用 lambda 表达式注册全局函数
Serializable::RegisterGlobalMethod("fooLambda", [](int a) { return a; });
```

#### 创建需要反射的类

```cpp
class A : public Serializable {
public:
    A() = default;
    A(int num) : num(num) {}
    int num = 0;
    int foo() { return num; }
    int mul(int a) { return num * a; }
};
```

#### 注册方式

1. **通过宏在函数外注册**

```cpp
SERIALIZABLE_REGISTRATION {
    Structure_<A>()          // 注册类 A
        .property("num", &A::num)  // 注册属性 num
        .method("foo", &A::foo)    // 注册方法 foo
        .method("mul", &A::mul)    // 注册方法 mul
        .constructor<>()           // 注册默认构造函数
        .constructor<int>();       // 注册带参数的构造函数
}
```

2. **在函数内注册**

```cpp
int main(){
    Structure_<A>()          // 注册类 A
        .property("num", &A::num)  // 注册属性 num
        .method("foo", &A::foo)    // 注册方法 foo
        .method("mul", &A::mul)    // 注册方法 mul
        .constructor<>()           // 注册默认构造函数
        .constructor<int>();       // 注册带参数的构造函数
    return 0;
}
```

#### 创建对象

- **通过类型名称创建对象**

```cpp
RTTIType type = Serializable::GetByName("A"); // 通过名称获取类型
```

- **通过类型创建对象**

```cpp
RTTIType type = Serializable::GetByType<A>(); // 通过类型获取类型
```

#### 实例化对象

```cpp
type->Create();    // 使用默认构造函数创建对象
type->Create(10);  // 使用带参数构造函数创建对象
```

#### 获取属性

```cpp
RTTIType numProp = type->GetProperty("num");    // 获取属性
int numValue = type->GetProperty<int>("num");     // 获取属性值
int numValue2 = numProp->As<int>();               // 获取属性值
```

#### 调用方法

1. **通过方法名称调用**

```cpp
int result = type->Invoke<int>("foo"); // 调用方法 foo
```

2. **通过获取方法对象调用**

```cpp
Function mul = type->GetMethod("mul");  // 获取方法 mul
int result = mul.Invoke<int>(2);          // 调用方法 mul
```

### 2. 基准测试

以下示例展示了 RTTM 与 RTTR 在方法调用性能方面的对比。

#### RTTM 基准测试

```cpp
#include <chrono>
#include <iostream>
#include "RTTM/Serializable.hpp"

using namespace RTTM;

class A : public Serializable {
public:
    int num = 0;
    A() = default;
    A(int num) { this->num = num; }
    int Add(int a) {
        num += a;
        return num;
    }
};

class B : public Serializable {
public:
    A a;
    B() = default;
};

class C : public Serializable {
public:
    B b;
    C() = default;
};

SERIALIZABLE_REGISTRATION {
    Structure_<A>()
        .property("num", &A::num)
        .constructor<>()
        .constructor<int>()
        .method("Add", &A::Add);
    Structure_<B>()
        .property("a", &B::a)
        .constructor<>();
}

int main() {
    Structure_<C>().property("b", &C::b).constructor<>();
    RTTIType ct = Serializable::GetByName("C");
    if (!ct) {
        std::cerr << "无法获取类型 C" << std::endl;
        return -1;
    }
    ct->Create();
    Function add = ct->GetProperty("b")->GetProperty("a")->GetMethod("Add");
    if (!add.IsValid()) {
        std::cerr << "无法获取方法 Add" << std::endl;
        return -1;
    }
    const int iterations = 1000000000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        int result = add.Invoke<int>(1);
        (void)result;
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    std::cout << "最终结果: " 
              << ct->GetProperty("b")->GetProperty("a")->GetProperty("num")->As<int>() 
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

## Benchmark 结果

```shell
.\RttrTest.exe
最终结果: 1000000000
[RTTR] 执行 1000000000 次方法调用耗时: 36538.7 毫秒
[RTTR] 平均每次调用耗时: 3.65387e-05 毫秒

.\RTTMTest.exe
最终结果: 1000000000
[RTTM] 执行 1000000000 次方法调用耗时: 19256.9 毫秒
[RTTM] 平均每次调用耗时: 1.92569e-05 毫秒
```

## 编译要求

- 使用 C++17 标准
- 支持 MSVC、GCC 与 Clang 编译器
- 无外部依赖，仅基于标准库

## 构建与使用

1. 将 RTTM (Runtime Turbo Mirror) 的源码包含到项目中。

2. 配置编译选项，确保启用 C++17 标准支持，例如：
    - 对于 GCC 和 Clang 使用： `-std=c++17`
    - 对于 MSVC 使用： `/std:c++17`

3. 请参考上述示例代码，学习如何注册类型、变量和方法，以及如何利用反射进行动态操作。

## 许可

本项目采用 MIT 许可协议。

## 贡献

欢迎任何形式的贡献和反馈！