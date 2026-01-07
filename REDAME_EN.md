<div align="center">
  <img src="imgs/logo.svg" alt="RTTM Logo"/>

# RTTM
**Runtime Turbo Mirror**

  <p><em>High-performance, lightweight C++20 dynamic reflection library</em></p>  

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)  
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)  
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()  
[![Compiler](https://img.shields.io/badge/compiler-MSVC%20%7C%20GCC%20%7C%20Clang-orange.svg)]()

<a href="README_EN.md">🌐 English</a> • <a href="README.md">🇨🇳 中文</a>
</div>  

---  

## 🎯 Overview

RTTM is a modern C++ reflection library designed for **game engines** and **performance-sensitive applications**. Based on the C++20 standard, it has zero external dependencies and provides core functionalities such as runtime type information, dynamic object creation, and method invocation.

## ✨ Core Features

<table>  
<tr>  
<td width="33%">  

### 🚀 High Performance
- Property access caching optimization
- Thread-safe design
- Smart pointer memory management

</td>  
<td width="33%">  

### 🔧 Modern C++20
- Concepts type constraints
- Zero external dependencies
- Supports MSVC/GCC/Clang

</td>  
<td width="33%">  

### 💡 Ease of Use
- Intuitive chained API
- Automatic registration mechanism
- Comprehensive error handling

</td>  
</tr>  
</table>  

**Supported Reflections**: Classes/Structs • Properties • Methods • Constructors • Inheritance • STL Containers

## 🚀 Quick Start

### 1️⃣ Include Header

```cpp  
#include "rttm/RTTM.hpp"  
using namespace rttm;  
```  

### 2️⃣ Register Type

```cpp  
class Person {  
public:  
    std::string name;  
    int age = 0;  
      
    Person() = default;  
    Person(const std::string& n, int a) : name(n), age(a) {}  
      
    std::string greeting() const { return "Hello, I'm " + name; }
    void setAge(int a) { age = a; }
};  

// Register reflection info  
RTTM_REGISTRATION {  
    Registry<Person>()  
        .property("name", &Person::name)  
        .property("age", &Person::age)  
        .method("greeting", &Person::greeting)
        .method("setAge", &Person::setAge)
        .constructor<>()  
        .constructor<const std::string&, int>();  
}  
```  

### 3️⃣ Dynamic Operations

```cpp  
// Get type and create instance  
auto personType = RType::get<Person>();  
personType->create("Alice", 30);  

// Property operations  
personType->property<std::string>("name") = "Bob";  
int age = personType->property<int>("age");  

// Method invocation  
std::string greeting = personType->invoke<std::string>("greeting");
personType->invoke<void>("setAge", 25);

// Property enumeration
for (const auto& propName : personType->property_names()) {
    std::cout << "Property: " << propName << std::endl;
}
```  

## 🔄 Inheritance Support

```cpp
class Animal {
public:
    std::string species;
    virtual void speak() const = 0;
};

class Dog : public Animal {
public:
    std::string name;
    void speak() const override { std::cout << "Woof!" << std::endl; }
};

RTTM_REGISTRATION {
    Registry<Animal>()
        .property("species", &Animal::species);
    
    Registry<Dog>()
        .base<Animal>()  // Inherit Animal's properties and methods
        .property("name", &Dog::name)
        .method("speak", &Dog::speak);
}

// Usage
auto dogType = RType::get<Dog>();
dogType->create();
dogType->property<std::string>("species") = "Canine";  // Access base class property
dogType->property<std::string>("name") = "Buddy";
dogType->invoke<void>("speak");  // Woof!
```

## 📦 Container Reflection

RTTM supports dynamic reflection of STL containers:

```cpp
class GameData {
public:
    std::vector<int> scores;
    std::map<std::string, int> playerStats;
};

RTTM_REGISTRATION {
    Registry<GameData>()
        .property("scores", &GameData::scores)
        .property("playerStats", &GameData::playerStats);
}

// Using sequential container
auto dataType = RType::get<GameData>();
dataType->create();
auto scoresContainer = dataType->sequential_container("scores");
scoresContainer->push_back(100);
scoresContainer->push_back(200);
std::cout << "Size: " << scoresContainer->size() << std::endl;

// Using associative container
auto statsContainer = dataType->associative_container("playerStats");
statsContainer->insert("player1", 1000);
bool hasPlayer = statsContainer->contains("player1");
```

## 🛡️ Error Handling

RTTM provides a clear exception hierarchy:

```cpp
try {
    auto type = RType::get("NonExistentType");
} catch (const TypeNotRegisteredError& e) {
    std::cerr << "Type not found: " << e.type_name() << std::endl;
}

try {
    auto personType = RType::get<Person>();
    personType->property<int>("nonexistent");
} catch (const PropertyNotFoundError& e) {
    std::cerr << "Property not found: " << e.what() << std::endl;
    // Error message includes list of available properties
}

try {
    auto personType = RType::get<Person>();
    personType->invoke<void>("greeting", 1, 2, 3);  // Parameter mismatch
} catch (const MethodSignatureMismatchError& e) {
    std::cerr << "Method signature mismatch: " << e.what() << std::endl;
}
```

## 🔄 Serialization Support

<details>  
<summary><strong>📝 JSON Serialization Example</strong></summary>  

```cpp  
#include <nlohmann/json.hpp>  
using json = nlohmann::json;  

// Generic serialization function  
json ToJson(RType& type) {  
    json j;  
    for (const auto& name : type.property_names()) {  
        auto prop = type.property(name);  
        if (prop->is<int>()) j[std::string(name)] = prop->as<int>();  
        else if (prop->is<std::string>()) j[std::string(name)] = prop->as<std::string>();  
        else if (prop->is_class()) j[std::string(name)] = ToJson(*prop);  
    }  
    return j;  
}  

// Generic deserialization function  
void FromJson(RType& type, const json& j) {  
    for (const auto& name : type.property_names()) {
        std::string nameStr(name);
        if (j.contains(nameStr)) {  
            auto prop = type.property(name);  
            if (prop->is<int>()) prop->as<int>() = j[nameStr].get<int>();  
            else if (prop->is_class()) FromJson(*prop, j[nameStr]);  
        }  
    }  
}  
```  
</details>  

## ⚙️ Build Integration

### System Requirements
- **C++20** or higher
- **Compiler**: MSVC 2019+ / GCC 10+ / Clang 10+
- **Platform**: Windows / Linux / macOS

### CMake Integration

```cmake  
# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add RTTM  
add_subdirectory(RTTM)
target_link_libraries(MyProject PRIVATE RTTM)  
```  

### Manual Integration

```bash  
# 1. Clone repository  
git clone https://github.com/NGLSG/RTTM.git  

# 2. Add to project  
# Copy include/rttm folder into project  

# 3. Compiler flags  
# GCC/Clang: -std=c++20  
# MSVC: /std:c++20  
```  

## 📚 API Reference

### Registry<T> - Type Registrar

| Method | Description |
|------|------|
| `property(name, member_ptr)` | Register member property |
| `method(name, method_ptr)` | Register member method |
| `constructor<Args...>()` | Register constructor |
| `base<Base>()` | Declare inheritance relationship |

### RType - Runtime Type Handle

| Method | Description |
|------|------|
| `get<T>()` / `get(name)` | Get type handle |
| `create(args...)` | Create instance |
| `property<T>(name)` | Type-safe property access |
| `property(name)` | Dynamic property access |
| `invoke<R>(name, args...)` | Invoke method |
| `property_names()` | Get all property names |
| `method_names()` | Get all method names |

### Exception Types

| Exception | Description |
|------|------|
| `TypeNotRegisteredError` | Type not registered |
| `PropertyNotFoundError` | Property not found |
| `MethodSignatureMismatchError` | Method signature mismatch |
| `ObjectNotCreatedError` | Object not created |

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
  <h3>🌟 Modern C++20 Reflection Solution for High-Performance Applications</h3>  

**Made with ❤️ by [NGLSG](https://github.com/NGLSG)**

[![Star](https://img.shields.io/github/stars/NGLSG/RTTM?style=social)](https://github.com/NGLSG/RTTM)  
[![Fork](https://img.shields.io/github/forks/NGLSG/RTTM?style=social)](https://github.com/NGLSG/RTTM/fork)  
[![Watch](https://img.shields.io/github/watchers/NGLSG/RTTM?style=social)](https://github.com/NGLSG/RTTM)
</div>
