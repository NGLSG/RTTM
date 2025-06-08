<div align="center">
  <img src="imgs/logo.svg" alt="RTTM Logo"/>

# RTTM
**Runtime Turbo Mirror**

  <p><em>High-performance, lightweight C++17 dynamic reflection library</em></p>  

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)  
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)  
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()  
[![Compiler](https://img.shields.io/badge/compiler-MSVC%20%7C%20GCC%20%7C%20Clang-orange.svg)]()

<a href="README_EN.md">🌐 English</a> • <a href="README.md">🇨🇳 中文</a>
</div>  

---  

## 🎯 Overview

RTTM is a modern C++ reflection library designed for **game engines** and **performance-sensitive applications**. Based on the C++17 standard, it has zero external dependencies and provides core functionalities such as runtime type information, dynamic object creation, and method invocation.

## ✨ Core Features

<table>  
<tr>  
<td width="33%">  

### 🚀 High Performance
- **51% faster** than mainstream libraries
- **50% less** memory usage
- Multi-thread optimized design

</td>  
<td width="33%">  

### 🔧 Zero Dependencies
- Requires only C++17 standard library
- Cross-platform compatibility
- Supports MSVC/GCC/Clang

</td>  
<td width="33%">  

### 💡 Ease of Use
- Intuitive API design
- Chained call support
- Automatic registration mechanism

</td>  
</tr>  
</table>  

**Supported Reflections**: Enums • Classes/Structs • Template Classes • Global Variables • Global Functions

## 📊 Performance Benchmarks

<details>  
<summary><strong>🏆 Performance Comparison with Mainstream Libraries</strong></summary>  

| Test Dimension | RTTM                                                    | Boost.Hana                                       | RTTR                                              |  
|---------|---------------------------------------------------------|--------------------------------------------------|---------------------------------------------------|  
| **Serialization Time** | **2946ms**                                              | 3343ms <span style="color:#e74c3c">(+13%)</span> | 4450ms <span style="color:#e74c3c">(+51%)</span>  |  
| **Property Access** | **1.5ns**                                               | 1.5ns                                            | 13.7ns <span style="color:#e74c3c">(+813%)</span> |  
| **Multi-thread Throughput** | **1354 ops/ms**                                         | 896 ops/ms                                       | 459 ops/ms                                        |  
| **Memory Efficiency** | **4 KB/Thousand Objects**                                            | 8 KB/Thousand Objects                                         | 8 KB/Thousand Objects                                          |  
|**Object Creation**| 271us/Thousand Objects <span style="color:#e74c3c">(+3387.5%)</span> | **8us/Thousand Objects**                                      | **7us/Thousand Objects**                                       |  

> 🔬 **Test Environment**: MSVC Release mode, based on 1 million object operations
</details>  

## 🚀 Quick Start

### 1️⃣ Include Header

```cpp  
#include "RTTM/RTTM.hpp"  
using namespace RTTM;  
```  

### 2️⃣ Register Type

```cpp  
class Person {  
public:  
    std::string name;  
    int age = 0;  
      
    Person() = default;  
    Person(const std::string& n, int a) : name(n), age(a) {}  
      
    std::string greeting() { return "Hello, I'm " + name; }  
};  

// Register reflection info  
RTTM_REGISTRATION {  
    Registry_<Person>()  
        .property("name", &Person::name)  
        .property("age", &Person::age)  
        .method("greeting", &Person::greeting)  
        .constructor<>()  
        .constructor<const std::string&, int>();  
}  
```  

### 3️⃣ Dynamic Operations

```cpp  
// Get type and create instance  
auto personType = RType::Get<Person>();  
auto result = personType->Create("Alice", 30);  

// Property operations  
personType->GetProperty<std::string>("name") = "Bob";  
int age = personType->GetProperty<int>("age");  

// Method invocation  
std::string greeting = personType->Invoke<std::string>("greeting");  
```  

## 🎮 ECS System Example

<details>  
<summary><strong>💡 View Complete Entity Component System Implementation</strong></summary>  

```cpp  
#include "RTTM/Entity.hpp"  

// Health component  
class Health : public RTTM::Component<Health> {  
public:  
    int hp = 100;  
    Health(int h = 100) : hp(h) {}  
      
    std::string GetTypeName() const override { return "Health"; }  
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Health)); }  
};  

// Weapon system (abstract component)  
class WeaponSystem : public RTTM::SingletonComponent<WeaponSystem> {  
public:  
    COMPONENT_DEPENDENCIES(Health) // Declare dependencies  
      
    int damage = 10;  
    virtual void Attack() = 0;  
      
    std::string GetTypeName() const override { return "WeaponSystem"; }  
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(WeaponSystem)); }  
};  

// Concrete weapon implementation  
class Sword : public WeaponSystem {  
public:  
    Sword() { damage = 30; }  
    void Attack() override { std::cout << "Sword Slash! Damage:" << damage << std::endl; }  
    std::string GetTypeName() const override { return "Sword"; }  
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Sword)); }  
};  

// Fighter entity  
class Fighter : REQUIRE_COMPONENTS(WeaponSystem) {  
public:  
    void Attack() {  
        GetComponentDynamic<WeaponSystem>().Attack();  
    }  
      
    template<typename T>  
    void ChangeWeapon() {  
        SwapComponent<WeaponSystem, T>();  
    }  
};  

// Usage example  
int main() {  
    Fighter player;  
    player.AddComponent<Health>(80);  
    player.AddComponent<Sword>();  
      
    player.Attack();              // Sword Slash! Damage:30  
    player.ChangeWeapon<Gun>();   // Dynamically switch weapons  
    player.Attack();              // Gunshot! Damage:20  
}  
```  
</details>  

## 🔄 Serialization Support

<details>  
<summary><strong>📝 JSON Serialization Example</strong></summary>  

```cpp  
#include <nlohmann/json.hpp>  
using json = nlohmann::json;  

// Generic serialization function  
json ToJson(const RType& type) {  
    json j;  
    for (const auto& name : type.GetPropertyNames()) {  
        auto prop = type.GetProperty(name);  
        if (prop->Is<int>()) j[name] = prop->As<int>();  
        else if (prop->Is<std::string>()) j[name] = prop->As<std::string>();  
        else if (prop->IsClass()) j[name] = ToJson(*prop);  
    }  
    return j;  
}  

// Generic deserialization function  
void FromJson(const RType& type, const json& j) {  
    for (const auto& name : type.GetPropertyNames()) {  
        if (j.contains(name)) {  
            auto prop = type.GetProperty(name);  
            if (prop->Is<int>()) prop->SetValue(j[name].get<int>());  
            else if (prop->IsClass()) FromJson(*prop, j[name]);  
        }  
    }  
}  
```  
</details>  

## ⚙️ Build Integration

### System Requirements
- **C++17** or higher
- **Compiler**: MSVC 2019+ / GCC 7+ / Clang 5+
- **Platform**: Windows / Linux / macOS

### CMake Integration

```cmake  
# Add RTTM  
add_executable(MyProject main.cpp)  
target_link_libraries(MyProject PRIVATE RTTM)  

# Enable automatic reflection generation  
include(<RTTM_PATH>/cmake/reflection.cmake)  
rttm_add_reflection(MyProject)  
```  

### Manual Integration

```bash  
# 1. Clone repository  
git clone https://github.com/NGLSG/RTTM.git  

# 2. Add to project  
# Copy RTTM folder into project  

# 3. Compiler flags  
# GCC/Clang: -std=c++17  
# MSVC: /std:c++17  
```  

## 📚 Advanced Features

<div align="center">  

| Feature | Description | Example |  
|------|------|------|  
| **Enum Reflection** | Dynamic access to enum values | `Enum::Get<MyEnum>()` |  
| **Template Classes** | Supports template type reflection | `Registry_<Vec<int>>()` |  
| **Global Functions** | Register and invoke global functions | `Global::RegisterMethod()` |  
| **Inheritance Support** | Reflects class inheritance relationships | `base()` chained calls |  
| **Auto Dependencies** | ECS component auto-dependency management | `COMPONENT_DEPENDENCIES()` |  

</div>  

## 🤝 Contribution Guide

We welcome all forms of contributions!

1. 🍴 **Fork** this repository
2. 🌿 Create a feature branch: `git checkout -b feature/amazing-feature`
3. 💾 Commit changes: `git commit -m 'Add amazing feature'`
4. 📤 Push the branch: `git push origin feature/amazing-feature`
5. 🔄 Create a **Pull Request**

## 📄 License

This project is licensed under the [MIT License](LICENSE) - see the file for details

---  

<div align="center">  
  <h3>🌟 Modern C++ Reflection Solution for High-Performance Applications</h3>  

**Made with ❤️ by [NGLSG](https://github.com/NGLSG)**

[![Star](https://img.shields.io/github/stars/NGLSG/RTTM?style=social)](https://github.com/NGLSG/RTTM)  
[![Fork](https://img.shields.io/github/forks/NGLSG/RTTM?style=social)](https://github.com/NGLSG/RTTM/fork)  
[![Watch](https://img.shields.io/github/watchers/NGLSG/RTTM?style=social)](https://github.com/NGLSG/RTTM)
</div>