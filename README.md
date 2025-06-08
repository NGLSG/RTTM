<div align="center">
  <img src="imgs/logo.svg" alt="RTTM Logo"/>

# RTTM
**Runtime Turbo Mirror**

  <p><em>高性能、轻量级的C++17动态反射库</em></p>

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()
[![Compiler](https://img.shields.io/badge/compiler-MSVC%20%7C%20GCC%20%7C%20Clang-orange.svg)]()

<a href="README_EN.md">🌐 English</a> • <a href="README.md">🇨🇳 中文</a>
</div>

---

## 🎯 概述

RTTM是一个专为**游戏引擎**和**性能敏感应用**设计的现代C++反射库。基于C++17标准，零外部依赖，提供运行时类型信息、动态对象创建和方法调用等核心功能。

## ✨ 核心特性

<table>
<tr>
<td width="33%">

### 🚀 高性能
- 比主流库快 **51%**
- 内存占用减少 **50%**
- 多线程优化设计

</td>
<td width="33%">

### 🔧 零依赖
- 仅需 C++17 标准库
- 跨平台兼容
- 支持 MSVC/GCC/Clang

</td>
<td width="33%">

### 💡 易用性
- 直观的 API 设计
- 链式调用支持
- 自动注册机制

</td>
</tr>
</table>

**支持反射**：枚举 • 类/结构体 • 模板类 • 全局变量 • 全局函数

## 📊 性能基准

<details>
<summary><strong>🏆 与主流库性能对比</strong></summary>

| 测试维度 | RTTM                                                    | Boost.Hana                                       | RTTR                                              |
|---------|---------------------------------------------------------|--------------------------------------------------|---------------------------------------------------|
| **序列化时间** | **2946ms**                                              | 3343ms <span style="color:#e74c3c">(+13%)</span> | 4450ms <span style="color:#e74c3c">(+51%)</span>  |
| **属性访问** | **1.5ns**                                               | 1.5ns                                            | 13.7ns <span style="color:#e74c3c">(+813%)</span> |
| **多线程吞吐** | **1354 ops/ms**                                         | 896 ops/ms                                       | 459 ops/ms                                        |
| **内存效率** | **4 KB/千对象**                                            | 8 KB/千对象                                         | 8 KB/千对象                                          |
|**对象创建**| 271us/千对象 <span style="color:#e74c3c">(+3387.5%)</span> | **8us/千对象**                                      | **7us/千对象**                                       |

> 🔬 **测试环境**：MSVC Release模式，基于100w对象操作场景
</details>

## 🚀 快速开始

### 1️⃣ 引入头文件

```cpp
#include "RTTM/RTTM.hpp"
using namespace RTTM;
```

### 2️⃣ 注册类型

```cpp
class Person {
public:
    std::string name;
    int age = 0;
    
    Person() = default;
    Person(const std::string& n, int a) : name(n), age(a) {}
    
    std::string greeting() { return "Hello, I'm " + name; }
};

// 注册反射信息
RTTM_REGISTRATION {
    Registry_<Person>()
        .property("name", &Person::name)
        .property("age", &Person::age)
        .method("greeting", &Person::greeting)
        .constructor<>()
        .constructor<const std::string&, int>();
}
```

### 3️⃣ 动态操作

```cpp
// 获取类型并创建实例
auto personType = RType::Get<Person>();
auto result = personType->Create("Alice", 30);

// 属性操作
personType->GetProperty<std::string>("name") = "Bob";
int age = personType->GetProperty<int>("age");

// 方法调用
std::string greeting = personType->Invoke<std::string>("greeting");
```

## 🎮 ECS系统示例

<details>
<summary><strong>💡 查看完整的实体组件系统实现</strong></summary>

```cpp
#include "RTTM/Entity.hpp"

// 健康组件
class Health : public RTTM::Component<Health> {
public:
    int hp = 100;
    Health(int h = 100) : hp(h) {}
    
    std::string GetTypeName() const override { return "Health"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Health)); }
};

// 武器系统（抽象组件）
class WeaponSystem : public RTTM::SingletonComponent<WeaponSystem> {
public:
    COMPONENT_DEPENDENCIES(Health) // 声明依赖
    
    int damage = 10;
    virtual void Attack() = 0;
    
    std::string GetTypeName() const override { return "WeaponSystem"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(WeaponSystem)); }
};

// 具体武器实现
class Sword : public WeaponSystem {
public:
    Sword() { damage = 30; }
    void Attack() override { std::cout << "剑击！伤害:" << damage << std::endl; }
    std::string GetTypeName() const override { return "Sword"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Sword)); }
};

// 战士实体
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

// 使用示例
int main() {
    Fighter player;
    player.AddComponent<Health>(80);
    player.AddComponent<Sword>();
    
    player.Attack();              // 剑击！伤害:30
    player.ChangeWeapon<Gun>();   // 动态切换武器
    player.Attack();              // 射击！伤害:20
}
```
</details>

## 🔄 序列化支持

<details>
<summary><strong>📝 JSON序列化示例</strong></summary>

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// 通用序列化函数
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

// 通用反序列化函数
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

## ⚙️ 构建集成

### 系统要求
- **C++17** 或更高版本
- **编译器**：MSVC 2019+ / GCC 7+ / Clang 5+
- **平台**：Windows / Linux / macOS

### CMake集成

```cmake
# 添加RTTM
add_executable(MyProject main.cpp)
target_link_libraries(MyProject PRIVATE RTTM)

# 启用自动反射生成
include(<RTTM_PATH>/cmake/reflection.cmake)
rttm_add_reflection(MyProject)
```

### 手动集成

```bash
# 1. 克隆仓库
git clone https://github.com/NGLSG/RTTM.git

# 2. 添加到项目
# 将RTTM文件夹复制到项目中

# 3. 编译选项
# GCC/Clang: -std=c++17
# MSVC: /std:c++17
```

## 📚 高级特性

<div align="center">

| 特性 | 说明 | 示例 |
|------|------|------|
| **枚举反射** | 支持枚举值的动态访问 | `Enum::Get<MyEnum>()` |
| **模板类** | 支持模板类型反射 | `Registry_<Vec<int>>()` |
| **全局函数** | 注册和调用全局函数 | `Global::RegisterMethod()` |
| **继承支持** | 支持类继承关系反射 | `base()` 链式调用 |
| **自动依赖** | ECS组件自动依赖管理 | `COMPONENT_DEPENDENCIES()` |

</div>

## 🤝 贡献指南

我们欢迎所有形式的贡献！

1. 🍴 **Fork** 本仓库
2. 🌿 创建特性分支：`git checkout -b feature/amazing-feature`
3. 💾 提交更改：`git commit -m 'Add amazing feature'`
4. 📤 推送分支：`git push origin feature/amazing-feature`
5. 🔄 创建 **Pull Request**

## 📄 许可证

本项目采用 [MIT许可证](LICENSE) - 查看文件了解详情

---

<div align="center">
  <h3>🌟 为高性能应用打造的现代C++反射解决方案</h3>

**Made with ❤️ by [NGLSG](https://github.com/NGLSG)**

[![Star](https://img.shields.io/github/stars/NGLSG/RTTM?style=social)](https://github.com/NGLSG/RTTM)
[![Fork](https://img.shields.io/github/forks/NGLSG/RTTM?style=social)](https://github.com/NGLSG/RTTM/fork)
[![Watch](https://img.shields.io/github/watchers/NGLSG/RTTM?style=social)](https://github.com/NGLSG/RTTM)
</div>