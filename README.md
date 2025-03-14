
<div align="center">
  <img src="imgs/logo.svg" alt="Logo" />
  <h1>RTTM (Runtime Turbo Mirror)</h1>
  <p>高性能、轻量级的C++17动态反射库</p>
</div>

<div align="center">
  <a href="README_EN.md">English</a> | <a href="README.md">中文</a>
</div>

## 概述

RTTM是一个基于C++17标准的高性能动态反射库，无外部依赖，专为游戏引擎和其他对性能敏感的应用设计。支持MSVC、GCC和Clang编译器，提供运行时类型信息、动态对象创建和方法调用等关键功能。

## ✨ 核心特性

- **零依赖** - 仅基于C++17标准库，无任何外部依赖
- **跨编译器兼容** - 全面支持MSVC、GCC和Clang
- **全面反射支持** - 反射枚举、类、结构体、模板类、全局变量和函数
- **动态实例化** - 支持动态创建对象和调用方法
- **高性能设计** - 经基准测试，在反射调用方面显著优于主流库
- **友好API** - 直观的API设计，支持流畅的链式调用
- **内存效率** - 优化的内存占用，比竞品库少50%

## 🚀 性能优势

RTTM与其他流行反射库的性能对比（MSVC Release模式）：

<table>
<tr>
  <th>测试维度</th>
  <th>RTTM</th>
  <th>Boost.Hana</th>
  <th>RTTR</th>
</tr>
<tr>
  <td>单对象序列化时间</td>
  <td><b>2946ms</b></td>
  <td>3343ms <span style="color:#e74c3c">(+13%)</span></td>
  <td>4450ms <span style="color:#e74c3c">(+51%)</span></td>
</tr>
<tr>
  <td>反射属性访问延迟</td>
  <td><b>1.5ns</b></td>
  <td>1.5ns <span style="color:#2ecc71">(相当)</span></td>
  <td>13.7ns <span style="color:#e74c3c">(+813%)</span></td>
</tr>
<tr>
  <td>多线程吞吐量（ops/ms）</td>
  <td><b>1354</b></td>
  <td>896 <span style="color:#e74c3c">(-34%)</span></td>
  <td>459 <span style="color:#e74c3c">(-66%)</span></td>
</tr>
<tr>
  <td>内存效率（KB/千对象）</td>
  <td><b>4</b></td>
  <td>8 <span style="color:#e74c3c">(+100%)</span></td>
  <td>8 <span style="color:#e74c3c">(+100%)</span></td>
</tr>
<tr>
  <td>嵌套属性访问性能</td>
  <td><b>1.78ns</b></td>
  <td>2.07ns <span style="color:#e74c3c">(+16%)</span></td>
  <td>14.0ns <span style="color:#e74c3c">(+687%)</span></td>
</tr>
</table>

### 性能分析要点

- **序列化效率**：比Boost.Hana快13%，比RTTR快51%
- **属性访问**：与Boost.Hana持平，但比RTTR快8倍以上
- **多线程性能**：吞吐量领先竞品34%-66%
- **内存优化**：仅占用竞品一半的内存空间
- **嵌套访问**：处理复杂对象图的性能显著优越

## 📚 使用指南

### 引入头文件

```cpp
#include <iostream>
#include "RTTM/RTTM.hpp"  // 包含RTTM核心头文件
using namespace RTTM;     // 使用RTTM命名空间
```

### 类型注册

#### 枚举类型

```cpp
// 定义枚举
enum class TypeEnum {
    CLASS = -1,
    VARIABLE,
};

// 注册枚举
RTTM_REGISTRATION {
    Enum_<TypeEnum>()
        .value("CLASS", TypeEnum::CLASS)
        .value("VARIABLE", TypeEnum::VARIABLE);
}

// 使用枚举
auto type = Enum::Get<TypeEnum>();
TypeEnum variable = type.GetValue("VARIABLE");
```

#### 类/结构体

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

// 注册类型
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

#### 全局变量与函数

```cpp
// 注册全局变量
Global::RegisterVariable("appVersion", "1.0.0");

// 获取全局变量
std::string version = Global::GetVariable<std::string>("appVersion");

// 定义并注册全局函数
int add(int a, int b) { return a + b; }
Global::RegisterGlobalMethod("add", add);

// 调用全局函数
Method<int> addFunc = Global::GetMethod<int(int,int)>("add");
int result = addFunc(5, 3);  // 结果为8
```

### 对象操作

```cpp
// 获取类型
Ref<RType> personType = RType::Get<Person>();
// 或通过名称: Ref<RType> personType = RType::Get("Person");

// 创建实例
auto instance = personType->Create("Alice", 30);

// 获取属性值
std::string name = personType->GetProperty<std::string>("name");
int age = personType->GetProperty<int>("age");

// 设置属性值
personType->GetProperty("name")->SetValue(std::string("Bob"));

// 调用方法
std::string greeting = personType->Invoke<std::string>("greeting");
```

### ECS 实现示例

```cpp
#include <RTTM/RTTM.hpp>
#include <RTTM/Entity.hpp>
#include <iostream>

// 定义组件
class Transform {
public:
    struct Vec3 { float x, y, z; };
    Vec3 position{0,0,0};
    Vec3 rotation{0,0,0};
    Vec3 scale{1,1,1};
    
    void translate(float x, float y, float z) {
        position.x += x;
        position.y += y;
        position.z += z;
    }
};

REQUIRE_COMPONENT(Transform)  // 注册组件

// 定义实体
class GameObject : public RTTM::Entity {
public:
    Transform& transform() {
        return GetComponent<Transform>();
    }
};

int main() {
    // 创建游戏对象并操作组件
    GameObject player;
    player.transform().position = {10, 5, 0};
    player.transform().translate(0, 0, 10);
    
    std::cout << "Player position: " 
              << player.transform().position.x << ", "
              << player.transform().position.y << ", "
              << player.transform().position.z << std::endl;
    return 0;
}
```

## 🔄 序列化示例

RTTM提供了对象的JSON序列化和反序列化支持：

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

// 序列化
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

// 反序列化
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

## ⚙️ 构建与集成

### 要求

- C++17或更高版本
- 兼容MSVC、GCC或Clang编译器
- 无外部依赖

### 集成步骤

1. 将RTTM源代码添加到项目中
2. 配置编译器支持C++17
   ```
   # GCC/Clang
   -std=c++17
   
   # MSVC
   /std:c++17
   ```
3. 包含头文件并使用`RTTM`命名空间

## 📜 许可

本项目采用MIT许可协议，详情请查看[LICENSE](LICENSE)文件。

## 👥 贡献

我们欢迎各种形式的贡献，包括但不限于功能请求、bug报告、文档改进、代码优化等。

1. Fork本仓库
2. 创建您的特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交您的更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 打开Pull Request

---

<div align="center">
  <p>RTTM - 为高性能应用打造的现代C++反射解决方案</p>
  <p>© 2025 Ryoshi/NGLSG - MIT许可</p>
</div>