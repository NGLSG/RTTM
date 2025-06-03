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
#include <vector>
#include <memory>

// Base component definitions
class Transform : public RTTM::Component
{
public:
    float x = 0.0f, y = 0.0f;
    float rotation = 0.0f;

    Transform(float x = 0, float y = 0) : x(x), y(y)
    {
    }

    void Move(float dx, float dy)
    {
        x += dx;
        y += dy;
    }

    std::string GetTypeName() const override { return "Transform"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Transform)); }
};

class Health : public RTTM::Component
{
public:
    int maxHP = 100;
    int currentHP = 100;

    Health(int hp = 100) : maxHP(hp), currentHP(hp)
    {
    }

    void TakeDamage(int damage)
    {
        currentHP -= damage;
        if (currentHP < 0) currentHP = 0;
    }

    bool IsAlive() const { return currentHP > 0; }

    std::string GetTypeName() const override { return "Health"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Health)); }
};

// Base renderer class
class Renderer : public RTTM::Component
{
public:
    bool visible = true;
    virtual void Render() = 0;

    std::string GetTypeName() const override { return "Renderer"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Renderer)); }
};

class SpriteRenderer : public Renderer
{
public:
    std::string sprite;

    SpriteRenderer(const std::string& s) : sprite(s)
    {
    }

    void Render() override
    {
        std::cout << "Rendering sprite: " << sprite << std::endl;
    }

    std::string GetTypeName() const override { return "SpriteRenderer"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(SpriteRenderer)); }
};

// Weapon component
class Weapon : public RTTM::Component
{
public:
    int damage = 10;
    std::string weaponType;

    Weapon(const std::string& type, int dmg) : weaponType(type), damage(dmg)
    {
    }

    void Attack()
    {
        std::cout << "Attacking with " << weaponType << ", dealing " << damage << " damage!" << std::endl;
    }

    std::string GetTypeName() const override { return "Weapon"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Weapon)); }
};

// Special entity class - Warrior
class Warrior : public RTTM::Entity
{
public:
    Warrior(float x, float y)
    {
        AddComponent<Transform>(x, y);
        AddComponent<Health>(150); // Warrior has more HP
        AddComponent<SpriteRenderer>("WarriorSprite");
        AddComponent<Weapon>("Longsword", 25);
    }

    void Attack(RTTM::Entity& target)
    {
        auto& weapon = GetComponent<Weapon>();
        weapon.Attack();

        // If target has Health component, deal damage
        if (target.HasComponent<Health>())
        {
            auto& targetHealth = target.GetComponent<Health>();
            targetHealth.TakeDamage(weapon.damage);
            std::cout << "Target's remaining HP: " << targetHealth.currentHP << std::endl;
        }
    }
};

// Mage class requiring specific components
class Mage : REQUIRE_COMPONENTS(Transform, Health)
{
public:
    int mana = 100;

    Mage(float x, float y) : mana(100)
    {
        // Transform and Health are automatically added
        AddComponent<SpriteRenderer>("MageSprite");

        // Set mage attributes
        GetComponent<Health>().maxHP = 80; // Mage has less HP
        GetComponent<Health>().currentHP = 80;
    }

    void CastSpell(RTTM::Entity& target)
    {
        if (mana >= 20)
        {
            mana -= 20;
            std::cout << "Mage casts Fireball! Consumes 20 mana" << std::endl;

            if (target.HasComponent<Health>())
            {
                auto& targetHealth = target.GetComponent<Health>();
                targetHealth.TakeDamage(30);
                std::cout << "Fireball deals 30 magic damage! Target's remaining HP: " << targetHealth.currentHP << std::endl;
            }
        }
        else
        {
            std::cout << "Not enough mana!" << std::endl;
        }
    }
};

// Game system class
class GameSystem
{
public:
    // Movement system - processes all entities with Transform
    static void UpdateMovement(std::vector<std::unique_ptr<RTTM::Entity>>& entities, float deltaTime)
    {
        std::cout << "\n=== Movement System Update ===" << std::endl;
        for (auto& entity : entities)
        {
            if (entity->HasComponent<Transform>())
            {
                auto& transform = entity->GetComponent<Transform>();
                // Simple movement logic
                transform.Move(1.0f * deltaTime, 0.5f * deltaTime);
                std::cout << "Entity moved to: (" << transform.x << ", " << transform.y << ")" << std::endl;
            }
        }
    }

    // Rendering system - processes all entities with renderers
    static void Render(std::vector<std::unique_ptr<RTTM::Entity>>& entities)
    {
        std::cout << "\n=== Rendering System Update ===" << std::endl;
        for (auto& entity : entities)
        {
            // Using dynamic type lookup to support inheritance
            if (entity->HasComponentDynamic<Renderer>())
            {
                auto& renderer = entity->GetComponentDynamic<Renderer>();
                if (renderer.visible)
                {
                    renderer.Render();
                }
            }
        }
    }

    // Health display system
    static void ShowHealthStatus(std::vector<std::unique_ptr<RTTM::Entity>>& entities)
    {
        std::cout << "\n=== Health Status ===" << std::endl;
        for (auto& entity : entities)
        {
            if (entity->HasComponent<Health>())
            {
                auto& health = entity->GetComponent<Health>();
                std::cout << "Entity HP: " << health.currentHP << "/" << health.maxHP;
                if (!health.IsAlive())
                {
                    std::cout << " (DEAD)";
                }
                std::cout << std::endl;
            }
        }
    }
};

int main()
{
    std::cout << "=== ECS Game System Demo ===" << std::endl;

    // Create game entities
    std::vector<std::unique_ptr<RTTM::Entity>> gameEntities;

    // Create warrior
    auto warrior = std::make_unique<Warrior>(10.0f, 20.0f);

    // Create mage (required components are automatically added)
    auto mage = std::make_unique<Mage>(5.0f, 15.0f);

    std::cout << "\n=== Verifying Automatic Component Addition ===" << std::endl;
    std::cout << "Mage has Transform component: " << mage->HasComponent<Transform>() << std::endl;
    std::cout << "Mage has Health component: " << mage->HasComponent<Health>() << std::endl;

    // Demonstrate polymorphism
    std::cout << "\n=== Polymorphic Component Lookup ===" << std::endl;
    std::cout << "Warrior has renderer (dynamic lookup): " << warrior->HasComponentDynamic<Renderer>() << std::endl;
    std::cout << "Mage has renderer (dynamic lookup): " << mage->HasComponentDynamic<Renderer>() << std::endl;

    gameEntities.push_back(std::move(warrior));
    gameEntities.push_back(std::move(mage));

    // Get references for combat demo
    Warrior* warriorPtr = static_cast<Warrior*>(gameEntities[0].get());
    Mage* magePtr = static_cast<Mage*>(gameEntities[1].get());

    // Simulate game loop
    std::cout << "\n=== Game Start ===" << std::endl;

    // Initial state
    GameSystem::ShowHealthStatus(gameEntities);
    GameSystem::Render(gameEntities);

    // Combat round 1
    std::cout << "\n=== Combat Round 1 ===" << std::endl;
    warriorPtr->Attack(*magePtr);
    magePtr->CastSpell(*warriorPtr);

    // Movement and rendering
    GameSystem::UpdateMovement(gameEntities, 1.0f);
    GameSystem::ShowHealthStatus(gameEntities);

    // Combat round 2
    std::cout << "\n=== Combat Round 2 ===" << std::endl;
    magePtr->CastSpell(*warriorPtr);
    warriorPtr->Attack(*magePtr);

    GameSystem::ShowHealthStatus(gameEntities);
    GameSystem::Render(gameEntities);

    std::cout << "\n=== Game Over ===" << std::endl;

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