<div align="center">
  <img src="imgs/logo.svg" alt="RTTM Logo"/>

# RTTM
**Runtime Turbo Mirror**

  <p><em>高性能 C++20 动态反射库 — 动态查找，静态访问</em></p>

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()

<a href="README_EN.md">🌐 English</a> • <a href="README.md">🇨🇳 中文</a>
</div>

---

## 🎯 概述

RTTM 是一个专为**游戏引擎**和**性能敏感应用**设计的 C++20 动态反射库。

**核心设计理念：动态查找 + 静态访问**
- 通过字符串动态查找类型/属性/方法（运行时灵活性）
- 使用 typed pointer arithmetic 访问数据（接近直接访问的性能）

## ⚡ 性能对比 (vs RTTR)

| 操作 | RTTM | RTTR | 加速比 |
|------|------|------|--------|
| 属性读取 (cached) | 1.07 ns | 13.1 ns | **12x** |
| 属性写入 (cached) | 1.50 ns | 3.98 ns | **2.7x** |
| 多属性访问 (3个) | 0.28 ns | 82.8 ns | **296x** |
| 方法调用 (cached) | 3.27 ns | 13.9 ns | **4.3x** |
| 对象创建 | 30.7 ns | 74.9 ns | **2.4x** |
| 完整动态路径 | 20.6 ns | 33.9 ns | **1.6x** |
| 批量属性访问 (100个) | 9.94 ns | 1476 ns | **148x** |

> 测试环境: 20核 CPU @ 3.9GHz, Clang -O3  
> 完整结果见 [benchmark/BENCHMARK_RESULTS.md](benchmark/BENCHMARK_RESULTS.md)

## ✨ 核心特性

- **高性能**: Cached 属性访问仅比直接访问慢 5x（RTTR 慢 65x）
- **零依赖**: 纯 C++20，无外部库依赖
- **类型安全**: 编译期类型检查，运行时错误提示
- **线程安全**: 读写锁 + TLS 缓存优化

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

### 2. 动态访问 (简单 API)

```cpp
// 获取类型
auto handle = RTypeHandle::get<Player>();

// 创建实例
auto instance = handle.create();
auto* player = static_cast<Player*>(instance.get());

// 绑定对象进行操作
auto bound = handle.bind(*player);
bound.set("name", std::string("Alice"));
bound.set("health", 80);

std::string name = bound.get<std::string>("name");  // "Alice"
int hp = bound.call<int>("getHealth");              // 80
```

### 3. 高性能访问 (缓存句柄)

```cpp
// 预缓存属性/方法句柄 (循环外)
auto handle = RTypeHandle::get<Player>();
auto propHealth = handle.get_property<int>("health");
auto methGetHealth = handle.get_method("getHealth", 0);

// 热路径访问 (循环内) - 接近直接访问性能
for (auto& player : players) {
    int hp = propHealth.get(player);      // ~1ns
    propHealth.set(player, hp - 10);      // ~1.5ns
    int current = methGetHealth.call<int>(&player);  // ~3ns
}
```

## 📊 性能层级

```
直接访问:        0.2 ns  (baseline)
RTTM Cached:     1-3 ns  (5-15x baseline)
RTTM FullDynamic: 5-20 ns (25-100x baseline)
RTTR Cached:     4-14 ns (20-70x baseline)
```

**选择建议:**
- 热路径/循环内: 使用 `PropertyHandle` / `MethodHandle`
- 一般场景: 使用 `BoundType.get/set/call`
- 完全动态: 使用字符串查找

## 🔧 API 概览

### RTypeHandle - 轻量类型句柄

```cpp
auto handle = RTypeHandle::get<T>();           // 静态获取
auto handle = RTypeHandle::get("TypeName");    // 动态获取

handle.create();                               // 创建实例
handle.bind(obj);                              // 绑定对象
handle.get_property<T>("name");                // 获取属性句柄
handle.get_method("name", argc);               // 获取方法句柄
```

### BoundType - 绑定对象操作

```cpp
auto bound = handle.bind(obj);

bound.get<T>("name");           // 读取属性
bound.set("name", value);       // 写入属性
bound.call<R>("method", args);  // 调用方法
```

### PropertyHandle - 缓存属性访问

```cpp
auto prop = handle.get_property<int>("health");

prop.get(obj);           // 读取 (~1ns)
prop.set(obj, value);    // 写入 (~1.5ns)
```

### MethodHandle - 缓存方法调用

```cpp
auto meth = handle.get_method("getHealth", 0);

meth.call<int>(&obj);              // 无参调用 (~3ns)
meth.call<void>(&obj, arg1);       // 带参调用 (~5ns)
```

## 🛡️ 错误处理

```cpp
try {
    auto handle = RTypeHandle::get("Unknown");
} catch (const TypeNotRegisteredError& e) {
    // 类型未注册
}

try {
    bound.get<int>("unknown");
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
