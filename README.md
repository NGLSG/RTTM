<div align="center">
  <img src="imgs/logo.svg" alt="RTTM Logo"/>

# RTTM
**Runtime Turbo Mirror**

  <p><em>高性能 C++20 动态反射库 — 从纯动态到零开销，全场景覆盖</em></p>

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()

<a href="README_EN.md">🌐 English</a> • <a href="README.md">🇨🇳 中文</a>
</div>

---

## 🎯 概述

RTTM 是一个专为**游戏引擎**和**性能敏感应用**设计的 C++20 动态反射库。

**核心设计理念：多层次 API，按需选择性能**
- 纯动态 API：DLL 兼容，无需编译期类型知识
- 缓存动态 API：预查找 handle，重复访问极快
- 半静态 API：编译期类型 + 运行时名称
- 零开销 API：等同直接调用

## ⚡ 性能对比 (vs RTTR)

### 纯动态 API（每次查找，类似 RTTR FullDynamic）

| 操作 | RTTM | RTTR | 加速比 |
|------|------|------|--------|
| 对象创建 | 40.0 ns | 75.7 ns | **1.9x** |
| 属性读取 | 17.7 ns | 21.2 ns | **1.2x** |
| 属性写入 | 3.4 ns | 13.4 ns | **4.0x** |
| 方法调用 (无参) | 17.4 ns | 26.2 ns | **1.5x** |
| 方法调用 (带参) | 18.8 ns | 17.1 ns | 0.9x |

### 缓存动态 API（预查找 handle）

| 操作 | RTTM | RTTR | 加速比 |
|------|------|------|--------|
| 属性读取 | 1.4 ns | 13.4 ns | **10x** |
| 属性写入 | 1.3 ns | 4.0 ns | **3x** |
| 方法调用 (无参) | 4.5 ns | 13.7 ns | **3x** |
| 方法调用 (带参) | 5.1 ns | 6.7 ns | **1.3x** |

> 测试环境: 20核 CPU @ 3.9GHz, Clang -O3  
> 完整结果见 [benchmark/BENCHMARK_RESULTS.md](benchmark/BENCHMARK_RESULTS.md)

## ✨ 核心特性

- **高性能**: 纯动态比 RTTR 快 1.2-4x，缓存模式快 3-10x
- **多层次 API**: 从纯动态到零开销，按需选择
- **DLL 兼容**: `Instance` API 支持动态加载类型
- **零依赖**: 纯 C++20，无外部库依赖
- **类型安全**: 编译期类型检查，运行时错误提示

## 🚀 快速开始

### 1. 注册类型

```cpp
#include "RTTM/RTTM.hpp"
using namespace rttm;

class Player {
public:
    std::string name;
    int health = 100;
    
    int getHealth() const { return health; }
    void setHealth(int h) { health = h; }
};

RTTM_REGISTRATION {
    Registry<Player>()
        .property("name", &Player::name)
        .property("health", &Player::health)
        .method("getHealth", &Player::getHealth)
        .method("setHealth", &Player::setHealth);
}
```

### 2. 纯动态 API（DLL 兼容）

```cpp
// 通过类型名创建实例（无需编译期类型知识）
auto inst = Instance::create("Player");

// 属性访问
Variant v = inst.get_property("health");        // 返回 Variant
inst.set_property("health", 80);                // 模板重载，直接接受 int

// 方法调用
Variant r = inst.invoke("getHealth");           // 返回 Variant
inst.invoke("setInt", 100);                     // 模板重载，直接接受参数
```

### 3. 缓存动态 API（重复访问）

```cpp
auto inst = Instance::create("Player");

// 预查找 handle（一次性开销）
auto prop = inst.get_property_handle("health");
auto meth = inst.get_method_handle("getHealth", 0);
void* obj = inst.get_raw();

// 热路径访问（极快）
int hp = prop.get_value_direct<int>(obj);       // 1.4 ns
prop.set_value_direct(obj, 80);                 // 1.3 ns
Variant r = meth.invoke(obj);                   // 4.5 ns
```

### 4. 半静态 API（知道类型）

```cpp
auto handle = RTypeHandle::get<Player>();
auto prop = handle.get_property<int>("health");
auto meth = handle.get_method("getHealth", 0);

Player player;
prop.set(player, 80);                           // 0.3 ns
int hp = meth.call<int>(&player);               // 3.8 ns
```

### 5. 零开销 API（编译期签名）

```cpp
auto meth = TypedMethodHandle<int()>::from_const<Player, &Player::getHealth>();

Player player;
int hp = meth.call(player);                     // 0.2 ns - 等同直接调用
```

## 📊 API 层次

| 层次 | API | 性能 | 使用场景 |
|------|-----|------|----------|
| 1 | `TypedMethodHandle` | 0.2 ns | 热路径，编译期知道签名 |
| 2 | `PropertyHandle<T>` | 0.3 ns | 热路径，编译期知道类型 |
| 3 | `DynamicProperty` | 1.3 ns | 纯动态，缓存访问 |
| 4 | `set_property(name, value)` | 3.4 ns | 纯动态，无缓存 |
| 5 | `set_property(name, Variant)` | 18.5 ns | 完全类型擦除 |

## 🔧 API 概览

### Instance - 纯动态 API（DLL 兼容）

```cpp
auto inst = Instance::create("TypeName");       // 通过名称创建

// 属性访问
Variant v = inst.get_property("name");          // 返回 Variant
inst.set_property("name", value);               // 模板重载

// 方法调用
Variant r = inst.invoke("method");              // 无参
Variant r = inst.invoke("method", arg1, arg2);  // 模板重载

// 缓存 handle
auto prop = inst.get_property_handle("name");
auto meth = inst.get_method_handle("method", argc);
```

### RTypeHandle - 半静态 API

```cpp
auto handle = RTypeHandle::get<T>();            // 静态获取
auto handle = RTypeHandle::get("TypeName");     // 动态获取

handle.create();                                // 创建实例
handle.bind(obj);                               // 绑定对象
handle.get_property<T>("name");                 // 获取属性句柄
handle.get_method("name", argc);                // 获取方法句柄
```

### PropertyHandle - 缓存属性访问

```cpp
auto prop = handle.get_property<int>("health");

prop.get(obj);                                  // 读取 (~0.3ns)
prop.set(obj, value);                           // 写入 (~0.3ns)
```

### TypedMethodHandle - 零开销方法调用

```cpp
auto meth = TypedMethodHandle<int()>::from_const<T, &T::getHealth>();
auto meth = TypedMethodHandle<void(int)>::from<T, &T::setHealth>();

meth.call(obj);                                 // 0.2ns - 等同直接调用
meth.call(obj, arg);
```

## 🛡️ 错误处理

```cpp
try {
    auto inst = Instance::create("Unknown");
} catch (const TypeNotRegisteredError& e) {
    // 类型未注册
}

try {
    inst.get_property("unknown");
} catch (const PropertyNotFoundError& e) {
    // 属性未找到，e.available_properties() 返回可用属性列表
}
```

## ⚙️ 构建

```cmake
set(CMAKE_CXX_STANDARD 20)
add_subdirectory(RTTM)
target_link_libraries(MyProject PRIVATE RTTM)
```

**要求:** C++20, MSVC 2019+ / GCC 10+ / Clang 10+

## 📄 许可证

[MIT License](LICENSE)

---

<div align="center">

**Made with ❤️ by [NGLSG](https://github.com/NGLSG)**

[![Star](https://img.shields.io/github/stars/NGLSG/RTTM?style=social)](https://github.com/NGLSG/RTTM)

</div>
