<div align="center">
  <img src="imgs/logo.svg" alt="Logo" />
</div>

[EN](README_EN.md) | [中文](README.md)

# RTTM (Runtime Turbo Mirror)

RTTM (Runtime Turbo Mirror) is a lightweight dynamic reflection library built on the C++17 standard library with no external dependencies. It compiles with MSVC, GCC, and Clang. The library enables runtime reflection for classes, structures, enums, variables, and functions, and supports dynamic object creation and method invocation.

## Features

- [x] Built on the C++17 standard library with no external dependencies.
- [x] Compatible with MSVC, GCC, and Clang.
- [x] Register enums, structures, template classes, as well as global variables and functions.
- [x] Dynamic object creation and member function invocation.
- [x] Superior performance: benchmark tests show lower invocation overhead compared to RTTR.
- [x] User-friendly: simple and intuitive API with support for chain calls.

## Example Usage

This section demonstrates the usage of the library modules and provides complete example files.

### 1. Reflection Registration and Object Operations

The operations for `struct` and `class` are the same.

#### Include Header

```cpp name=examples/reflect_usage.cpp
#include <iostream>
#include "RTTM/Serializable.hpp" // Include RTTM header
using namespace RTTM;             // Use RTTM namespace
```

#### Enum Type Registration

```cpp name=examples/reflect_usage.cpp
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

#### Creating a Global Variable

```cpp name=examples/reflect_usage.cpp
// Create a global variable
Serializable::RegisterVariable("var", 0);
// Get the value of the global variable
int var = *Serializable::GetVariable<int>("var");
// Set the value of the global variable
*Serializable::GetVariable<int>("var") = 20;
```

#### Creating a Global Function

```cpp name=examples/reflect_usage.cpp
// Define a regular function
int foo(int a) {
    return a;
}
Serializable::RegisterGlobalMethod("foo", foo); // Register global function
Function fooFunc = Serializable::GetGlobalMethod("foo"); // Get global function
int result = fooFunc.Invoke<int>(10); // Call global function

// Register a lambda function as a global function
Serializable::RegisterGlobalMethod("fooLambda", [](int a) { return a; });
```

#### Creating a Reflectable Class

```cpp name=examples/reflect_usage.cpp
class A : public Serializable {
public:
    A() = default;
    A(int num) : num(num) {}
    int num = 0;
    int foo() { return num; }
    int mul(int a) { return num * a; }
};
```

#### Registration Methods

1. **Register Outside a Function**

```cpp name=examples/reflect_usage.cpp
SERIALIZABLE_REGISTRATION {
    Structure_<A>()          // Register class A
        .property("num", &A::num)  // Register property num
        .method("foo", &A::foo)    // Register method foo
        .method("mul", &A::mul)    // Register method mul
        .constructor<>()           // Register default constructor
        .constructor<int>();       // Register constructor with parameters
}
```

2. **Register Inside a Function**

```cpp name=examples/reflect_usage.cpp
int main(){
    Structure_<A>()          // Register class A
        .property("num", &A::num)  // Register property num
        .method("foo", &A::foo)    // Register method foo
        .method("mul", &A::mul)    // Register method mul
        .constructor<>()           // Register default constructor
        .constructor<int>();       // Register constructor with parameters
    return 0;
}
```

#### Creating an Object

- **By Type Name**

```cpp name=examples/reflect_usage.cpp
RTTIType type = Serializable::GetByName("A"); // Get type by name
```

- **By Type**

```cpp name=examples/reflect_usage.cpp
RTTIType type = Serializable::Get<A>(); // Get type using template
```

#### Instantiating an Object

```cpp name=examples/reflect_usage.cpp
type->Create();    // Create object using default constructor
type->Create(10);  // Create object using constructor with parameters
```

#### Getting a Property

```cpp name=examples/reflect_usage.cpp
RTTIType numProp = type->GetProperty("num");    // Get property
int numValue = type->GetProperty<int>("num");     // Get property value
int numValue2 = numProp->As<int>();               // Get property value
```

#### Invoking a Method

1. **By Method Name**

```cpp name=examples/reflect_usage.cpp
int result = type->Invoke<int>("foo"); // Call method foo
```

2. **By Obtaining the Method Object**

```cpp name=examples/reflect_usage.cpp
Function mul = type->GetMethod("mul");  // Get method mul
int result = mul.Invoke<int>(2);          // Call method mul
```

### 2. Benchmark Tests

The following examples compare the performance of RTTM and RTTR in method invocation.

#### RTTM Benchmark Test

```cpp name=examples/benchmark_rttm.cpp
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
        std::cerr << "Failed to get type C" << std::endl;
        return -1;
    }
    ct->Create();
    Function add = ct->GetProperty("b")->GetProperty("a")->GetMethod("Add");
    if (!add.IsValid()) {
        std::cerr << "Failed to get method Add" << std::endl;
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
    
    std::cout << "Final result: " 
              << ct->GetProperty("b")->GetProperty("a")->GetProperty("num")->As<int>() 
              << std::endl;
    std::cout << "[RTTM] Execution time for " << iterations 
              << " calls: " << elapsed_ms << " ms" << std::endl;
    std::cout << "[RTTM] Average time per call: " 
              << (elapsed_ms / iterations) << " ms" << std::endl;
    return 0;
}
```

#### RTTR Benchmark Test

```cpp name=examples/benchmark_rttr.cpp
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
        cerr << "Failed to create an instance of C" << endl;
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
        cerr << "Failed to create an instance of A with parameter" << endl;
        return -1;
    }
    propA.set_value(varB, varA_new);
    varA = propA.get_value(varB);
    method methAdd = typeA.get_method("Add");
    if (!methAdd.is_valid()) {
        cerr << "Failed to get method Add" << endl;
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
    cout << "[RTTR] Execution time for " << iterations 
         << " calls: " << elapsed_ms << " ms" << endl;
    cout << "[RTTR] Average time per call: " 
         << (elapsed_ms / iterations) << " ms" << endl;
    return 0;
}
```

## Benchmark Results

```shell
.\RttrTest.exe
Final result: 1000000000
[RTTR] Execution time for 1000000000 calls: 36538.7 ms
[RTTR] Average time per call: 3.65387e-05 ms

.\RTTMTest.exe
Final result: 1000000000
[RTTM] Execution time for 1000000000 calls: 19256.9 ms
[RTTM] Average time per call: 1.92569e-05 ms
```

## Build Requirements

- Requires C++17 or later.
- Compatible with MSVC, GCC, and Clang.
- No external dependencies; built solely on the C++ standard library.

## Building and Usage

1. Include the RTTM (Runtime Turbo Mirror) source in your project.

2. Configure your compiler to enable C++17 support, for example:
   - For GCC and Clang: `-std=c++17`
   - For MSVC: `/std:c++17`

3. Refer to the example code above to learn how to register types, variables, and functions, and to perform dynamic reflection operations.

## License

This project is licensed under the MIT License.

## Contributing

Contributions and feedback are welcome!