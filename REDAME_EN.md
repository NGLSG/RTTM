<div align="center">
  <img src="imgs/logo.svg" alt="Logo" />
  <h1>RTTM (Runtime Turbo Mirror)</h1>
  <p>High-performance, lightweight C++17 dynamic reflection library</p>
</div>

<div align="center">
  <a href="README_EN.md">English</a> | <a href="README.md">中文</a>
</div>

## Overview

RTTM is a high-performance dynamic reflection library based on C++17 standard with no external dependencies, designed
specifically for game engines and other performance-sensitive applications. It supports MSVC, GCC, and Clang compilers,
providing run-time type information, dynamic object creation, and method invocation capabilities.

## ✨ Core Features

- **Zero Dependencies** - Based solely on the C++17 standard library, no external dependencies
- **Cross-Compiler Compatibility** - Full support for MSVC, GCC, and Clang
- **Comprehensive Reflection** - Reflect enums, classes, structs, templates, global variables, and functions
- **Dynamic Instantiation** - Support for dynamic object creation and method invocation
- **High-Performance Design** - Significantly outperforms mainstream libraries in reflection calls, as demonstrated by
  benchmarks
- **User-Friendly API** - Intuitive API design with fluent chain-style calls
- **Memory Efficiency** - Optimized memory usage, 50% less than competing libraries

## 🚀 Performance Advantages

Performance comparison between RTTM and other popular reflection libraries (MSVC Release mode):

<table>
<tr>
  <th>Benchmark</th>
  <th>RTTM</th>
  <th>Boost.Hana</th>
  <th>RTTR</th>
</tr>
<tr>
  <td>Single Object Serialization Time</td>
  <td><b>2946ms</b></td>
  <td>3343ms <span style="color:#e74c3c">(+13%)</span></td>
  <td>4450ms <span style="color:#e74c3c">(+51%)</span></td>
</tr>
<tr>
  <td>Reflection Property Access Latency</td>
  <td><b>1.5ns</b></td>
  <td>1.5ns <span style="color:#2ecc71">(equivalent)</span></td>
  <td>13.7ns <span style="color:#e74c3c">(+813%)</span></td>
</tr>
<tr>
  <td>Multithreaded Throughput (ops/ms)</td>
  <td><b>1354</b></td>
  <td>896 <span style="color:#e74c3c">(-34%)</span></td>
  <td>459 <span style="color:#e74c3c">(-66%)</span></td>
</tr>
<tr>
  <td>Memory Efficiency (KB/1000 objects)</td>
  <td><b>4</b></td>
  <td>8 <span style="color:#e74c3c">(+100%)</span></td>
  <td>8 <span style="color:#e74c3c">(+100%)</span></td>
</tr>
<tr>
  <td>Nested Property Access Performance</td>
  <td><b>1.78ns</b></td>
  <td>2.07ns <span style="color:#e74c3c">(+16%)</span></td>
  <td>14.0ns <span style="color:#e74c3c">(+687%)</span></td>
</tr>
</table>

### Key Performance Insights

- **Serialization Efficiency**: 13% faster than Boost.Hana, 51% faster than RTTR
- **Property Access**: On par with Boost.Hana, but over 8x faster than RTTR
- **Multithreaded Performance**: Throughput leads competitors by 34%-66%
- **Memory Optimization**: Requires only half the memory of competing products
- **Nested Access**: Significantly superior performance when handling complex object graphs

## 📚 Usage Guide

### Including Headers

```cpp
#include <iostream>
#include "RTTM/RTTM.hpp"  // Include RTTM core header
using namespace RTTM;     // Use RTTM namespace
```

### Type Registration

#### Enumerations

```cpp
// Define enum
enum class TypeEnum {
    CLASS = -1,
    VARIABLE,
};

// Register enum
RTTM_REGISTRATION {
    Enum_<TypeEnum>()
        .value("CLASS", TypeEnum::CLASS)
        .value("VARIABLE", TypeEnum::VARIABLE);
}

// Use enum
auto type = Enum::Get<TypeEnum>();
TypeEnum variable = type.GetValue("VARIABLE");
```

#### Classes/Structs

```cpp
class Person {
public:
    Person() = default;
    Person(const std::string& name, int age) : name(name), age(age) {}
    
    std::string name;
    int age = 0;
    
    std::string greeting() { return "Hello, I'm " + name; }
    int getAgeNextYear() { return age + 1; }
};

// Register class
RTTM_REGISTRATION {
    Registry_<Person>()
        .property("name", &Person::name)
        .property("age", &Person::age)
        .method("greeting", &Person::greeting)
        .method("getAgeNextYear", &Person::getAgeNextYear)
        .constructor<>()
        .constructor<const std::string&, int>();
}
```

#### Global Variables and Functions

```cpp
// Register global variable
Global::RegisterVariable("appVersion", "1.0.0");

// Get global variable
std::string version = Global::GetVariable<std::string>("appVersion");

// Define and register global function
int add(int a, int b) { return a + b; }
Global::RegisterGlobalMethod("add", add);

// Call global function
Method<int> addFunc = Global::GetMethod<int(int,int)>("add");
int result = addFunc(5, 3);  // Result is 8
```

### Object Operations

```cpp
// Get type
Ref<RType> personType = RType::Get<Person>();
// Or by name: Ref<RType> personType = RType::Get("Person");

// Create instance
auto instance = personType->Create("Alice", 30);

// Get property value
std::string name = personType->GetProperty<std::string>("name");
int age = personType->GetProperty<int>("age");

// Set property value
personType->GetProperty("name")->SetValue(std::string("Bob"));

// Call method
std::string greeting = personType->Invoke<std::string>("greeting");
```

### ECS Implementation Example

```cpp
#include "RTTM/Entity.hpp"
#include <iostream>

// Data Component
class Health : public RTTM::Component<Health>
{
public:
    int hp = 100;

    Health(int h = 100) : hp(h)
    {
    }

    std::string GetTypeName() const override { return "Health"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Health)); }
};

// Pure Virtual Singleton Component - Cannot be instantiated directly
class WeaponSystem : public RTTM::SingletonComponent<WeaponSystem>
{
public:
    COMPONENT_DEPENDENCIES(Health) // Dependency declaration
    int damage = 10;
    virtual void Attack() = 0; // Pure virtual function, must be implemented by subclasses
    std::string GetTypeName() const override { return "WeaponSystem"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(WeaponSystem)); }
};

// Concrete weapon implementation
class Sword : public WeaponSystem
{
public:
    Sword() { damage = 30; }
    void Attack() override { std::cout << "Sword Slash! Damage:" << damage << std::endl; }
    std::string GetTypeName() const override { return "Sword"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Sword)); }
};

class Gun : public WeaponSystem
{
public:
    Gun() { damage = 20; }
    void Attack() override { std::cout << "Gun Shot! Damage:" << damage << std::endl; }
    std::string GetTypeName() const override { return "Gun"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Gun)); }
};

// Fighter entity
class Fighter : REQUIRE_COMPONENTS(WeaponSystem)
{
public:
    void Attack()
    {
        try
        {
            GetComponentDynamic<WeaponSystem>().Attack();
        }
        catch (const std::exception& e)
        {
            std::cout << "Attack failed: " << e.what() << std::endl;
        }
    }

    template <typename T>
    void ChangeWeapon()
    {
        try
        {
            SwapComponent<WeaponSystem, T>();
            std::cout << "Weapon changed to:" << GetComponentDynamic<WeaponSystem>().GetTypeName() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "Weapon change failed: " << e.what() << std::endl;
        }
    }

    void ShowInfo()
    {
        try
        {
            auto& h = GetComponent<Health>();
            auto& w = GetComponentDynamic<WeaponSystem>();
            std::cout << "HP:" << h.hp << " Weapon:" << w.GetTypeName() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "Failed to display info: " << e.what() << std::endl;
        }
    }
};

int main()
{
    std::cout << "=== RTTM ECS Demo ===" << std::endl;

    // Feature 1: Normal entity creation and component addition
    std::cout << "\n1. Normal entity creation:" << std::endl;
    Fighter player;
    player.AddComponent<Health>(80);
    player.AddComponent<Sword>();

    // Feature 2: Component detection
    std::cout << "\n2. Component detection:" << std::endl;
    std::cout << "Has Health:" << (player.HasComponent<Health>() ? "Yes" : "No") << std::endl;
    std::cout << "Has Weapon:" << (player.HasComponentDynamic<WeaponSystem>() ? "Yes" : "No") << std::endl;

    // Feature 3: Component usage
    std::cout << "\n3. Component usage:" << std::endl;
    player.ShowInfo();
    player.Attack();

    // Feature 4: Singleton component replacement
    std::cout << "\n4. Component replacement:" << std::endl;
    player.ChangeWeapon<Gun>();
    player.Attack();

    // Feature 5: Automatic dependency handling
    std::cout << "\n5. Automatic dependency handling:" << std::endl;
    Fighter newPlayer;
    newPlayer.AddComponent<Gun>(); // Automatically adds Health dependency
    std::cout << "New player automatically has Health:" << (newPlayer.HasComponent<Health>() ? "Yes" : "No") <<
        std::endl;
    newPlayer.ShowInfo();

    // Feature 6: Error handling demonstration
    std::cout << "\n6. Error handling:" << std::endl;
    try
    {
        // Since WeaponSystem is an abstract class, it cannot be instantiated directly; concrete implementations must be added manually
        Fighter errorPlayer;
        // Error: errorPlayer.AddComponent<WeaponSystem>(); // This line would fail to compile because WeaponSystem is abstract
        errorPlayer.Attack();
        std::cout <<
            "This will output an error message because WeaponSystem is abstract and cannot be auto-added; the entity lacks components"
            << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cout << "Error caught: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Unknown error caught" << std::endl;
    }

    return 0;
}

```

## 🔄 Serialization Example

RTTM provides support for JSON serialization and deserialization of objects:

```cpp
#include "RTTM/RTTM.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace RTTM;

class User {
public:
    std::string username;
    int id;
    bool active;
};

RTTM_REGISTRATION {
    Registry_<User>()
        .property("username", &User::username)
        .property("id", &User::id)
        .property("active", &User::active);
}

// Serialization
json ToJson(const RType& type) {
    json j;
    for (const auto& name : type.GetPropertyNames()) {
        auto prop = type.GetProperty(name);
        if (prop->Is<int>()) j[name] = prop->As<int>();
        else if (prop->Is<std::string>()) j[name] = prop->As<std::string>();
        else if (prop->Is<bool>()) j[name] = prop->As<bool>();
        else if (prop->IsClass()) j[name] = ToJson(*prop);
    }
    return j;
}

// Deserialization
void FromJson(const RType& type, const json& j) {
    for (const auto& name : type.GetPropertyNames()) {
        if (j.contains(name)) {
            auto prop = type.GetProperty(name);
            if (prop->Is<int>()) prop->SetValue(j[name].get<int>());
            else if (prop->Is<std::string>()) prop->SetValue(j[name].get<std::string>());
            else if (prop->Is<bool>()) prop->SetValue(j[name].get<bool>());
            else if (prop->IsClass()) FromJson(*prop, j[name]);
        }
    }
}
```

## ⚙️ Build and Integration

### Requirements

- C++17 or higher
- Compatible with MSVC, GCC, or Clang compilers
- No external dependencies

### Integration Steps

1. Add RTTM source code to your project
2. Configure compiler to support C++17
   ```
   # GCC/Clang
   -std=c++17
   
   # MSVC
   /std:c++17
   ```
3. Include the headers and use the `RTTM` namespace

## 📜 License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## 👥 Contributing

We welcome all forms of contributions, including but not limited to feature requests, bug reports, documentation
improvements, code optimizations, etc.

1. Fork this repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

---

<div align="center">
  <p>RTTM - A modern C++ reflection solution for high-performance applications</p>
  <p>© 2025 Ryoshi/NGLSG - MIT License</p>
</div>