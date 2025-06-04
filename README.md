
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
- **支持自动注册** - 编译期自动生成反射代码，简化注册流程

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
#include "RTTM/Entity.hpp"
#include <iostream>

// 数据组件
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

// 纯虚单例组件 - 不能直接实例化
class WeaponSystem : public RTTM::SingletonComponent<WeaponSystem>
{
public:
    COMPONENT_DEPENDENCIES(Health) // 依赖声明
    int damage = 10;
    virtual void Attack() = 0; // 纯虚函数，子类必须实现
    std::string GetTypeName() const override { return "WeaponSystem"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(WeaponSystem)); }
};

// 具体武器实现
class Sword : public WeaponSystem
{
public:
    Sword() { damage = 30; }
    void Attack() override { std::cout << "剑击！伤害:" << damage << std::endl; }
    std::string GetTypeName() const override { return "Sword"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Sword)); }
};

class Gun : public WeaponSystem
{
public:
    Gun() { damage = 20; }
    void Attack() override { std::cout << "射击！伤害:" << damage << std::endl; }
    std::string GetTypeName() const override { return "Gun"; }
    std::type_index GetTypeIndex() const override { return std::type_index(typeid(Gun)); }
};

// 战士实体
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
            std::cout << "攻击失败: " << e.what() << std::endl;
        }
    }

    template <typename T>
    void ChangeWeapon()
    {
        try
        {
            SwapComponent<WeaponSystem, T>();
            std::cout << "换武器为:" << GetComponentDynamic<WeaponSystem>().GetTypeName() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "换武器失败: " << e.what() << std::endl;
        }
    }

    void ShowInfo()
    {
        try
        {
            auto& h = GetComponent<Health>();
            auto& w = GetComponentDynamic<WeaponSystem>();
            std::cout << "血量:" << h.hp << " 武器:" << w.GetTypeName() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "显示信息失败: " << e.what() << std::endl;
        }
    }
};

int main()
{
    std::cout << "=== RTTM ECS 演示 ===" << std::endl;

    // 特性1: 正常的实体创建与组件添加
    std::cout << "\n1. 正常实体创建:" << std::endl;
    Fighter player;
    player.AddComponent<Health>(80);
    player.AddComponent<Sword>();

    // 特性2: 组件检测
    std::cout << "\n2. 组件检测:" << std::endl;
    std::cout << "有生命值:" << (player.HasComponent<Health>() ? "是" : "否") << std::endl;
    std::cout << "有武器:" << (player.HasComponentDynamic<WeaponSystem>() ? "是" : "否") << std::endl;

    // 特性3: 组件使用
    std::cout << "\n3. 组件使用:" << std::endl;
    player.ShowInfo();
    player.Attack();

    // 特性4: 单例组件替换
    std::cout << "\n4. 组件替换:" << std::endl;
    player.ChangeWeapon<Gun>();
    player.Attack();

    // 特性5: 自动依赖处理
    std::cout << "\n5. 自动依赖处理:" << std::endl;
    Fighter newPlayer;
    newPlayer.AddComponent<Gun>(); // 自动添加Health依赖
    std::cout << "新玩家自动有生命值:" << (newPlayer.HasComponent<Health>() ? "是" : "否") << std::endl;
    newPlayer.ShowInfo();

    // 特性6: 错误处理演示
    std::cout << "\n6. 错误处理:" << std::endl;
    try
    {
        //由于WeaponSystem是纯虚类，不能直接实例化,需要手动添加具体实现
        Fighter errorPlayer;
        //错误: errorPlayer.AddComponent<WeaponSystem>(); // 这行会编译失败，因为WeaponSystem是纯虚类
        errorPlayer.Attack();
        std::cout << "将会输出错误信息，因为WeaponSystem是纯虚类，不能自动添加,实体缺乏组件" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cout << "错误处理捕获: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "未知错误被捕获" << std::endl;
    }

    return 0;
}

```

## 🔄 自动化注册
```cmake
add_executable(Test main.cpp header.h)
target_link_libraries(Test PRIVATE RTTM)
include(<PATH_TO_RTTM>/RTTM/cmake/reflection.cmake)
rttm_add_reflection(Test)
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