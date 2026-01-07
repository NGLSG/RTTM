# Implementation Plan: RTTM Refactor

## Overview

本实现计划将RTTM反射库从C++17重构为C++20，移除ECS功能，专注于核心反射能力。采用增量开发方式，每个阶段都有可验证的里程碑。

## Tasks

- [x] 1. 项目结构重组和基础设施
  - [x] 1.1 创建新的目录结构和头文件骨架
    - 创建 include/rttm/ 目录
    - 创建 RTTM.hpp 主入口文件
    - 创建 detail/ 子目录用于内部实现
    - _Requirements: 4.1, 4.4_
  - [x] 1.2 配置CMake支持C++20
    - 更新CMakeLists.txt设置C++20标准
    - 配置编译器选项
    - _Requirements: 1.1_
  - [x] 1.3 创建C++20 Concepts定义
    - 实现 Reflectable concept
    - 实现 SequentialContainer concept
    - 实现 AssociativeContainer concept
    - 实现 MemberFunction concept
    - _Requirements: 1.2, 1.6_

- [x] 2. 核心类型系统实现
  - [x] 2.1 实现TypeInfo数据结构
    - 定义 MemberInfo 结构
    - 定义 MethodInfo 结构
    - 定义 TypeInfo 结构
    - _Requirements: 11.5_
  - [x] 2.2 实现TypeManager单例
    - 实现线程安全的类型存储
    - 实现类型注册方法
    - 实现类型查询方法
    - 实现线程本地缓存
    - _Requirements: 5.1, 5.3, 5.4, 12.1_
  - [ ]* 2.3 编写TypeManager属性测试
    - **Property 6: Duplicate Registration Idempotence**
    - **Property 7: Type Info Caching Consistency**
    - **Property 8: Thread-Safe Type Operations**
    - **Validates: Requirements 5.2, 5.3, 5.4, 12.1, 12.2, 12.3**

- [ ] 3. Checkpoint - 确保类型系统测试通过
  - 确保所有测试通过，如有问题请询问用户

- [x] 4. Registry类型注册器实现
  - [x] 4.1 实现Registry<T>基础结构
    - 实现构造函数和类型信息初始化
    - 实现自动默认构造函数注册
    - _Requirements: 2.1, 2.2_
  - [x] 4.2 实现property()方法
    - 计算成员偏移量
    - 检测成员类型类别
    - 支持链式调用
    - _Requirements: 2.1_
  - [x] 4.3 实现method()方法
    - 支持普通成员函数
    - 支持const成员函数
    - 实现参数类型自动转换
    - _Requirements: 7.1, 7.3_
  - [x] 4.4 实现constructor()方法
    - 支持多参数构造函数
    - 注册工厂函数
    - _Requirements: 2.1_
  - [x] 4.5 实现base()继承支持
    - 合并基类属性和方法
    - 支持多级继承
    - _Requirements: 10.1, 10.2, 10.3_
  - [ ]* 4.6 编写Registry属性测试
    - **Property 1: Registration Chain Completeness**
    - **Property 2: Auto Default Constructor Registration**
    - **Property 21: Inheritance Property Access**
    - **Property 22: Multi-Level Inheritance**
    - **Validates: Requirements 2.1, 2.2, 10.1, 10.2, 10.3, 10.4**

- [ ] 5. Checkpoint - 确保注册器测试通过
  - 确保所有测试通过，如有问题请询问用户

- [x] 6. RType运行时类型句柄实现
  - [x] 6.1 实现RType基础结构
    - 实现静态get()方法
    - 实现模板get<T>()方法
    - 实现类型信息查询方法
    - _Requirements: 3.2_
  - [x] 6.2 实现create()对象创建
    - 实现模板参数构造
    - 实现创建失败清理
    - _Requirements: 9.5_
  - [x] 6.3 实现property()属性访问
    - 实现模板版本类型安全访问
    - 实现动态版本返回RType
    - 实现偏移量缓存
    - _Requirements: 2.3, 6.1, 6.3_
  - [x] 6.4 实现invoke()方法调用
    - 实现参数类型自动转换
    - 实现void返回类型支持
    - 实现方法缓存
    - _Requirements: 7.1, 7.5_
  - [x] 6.5 实现属性枚举方法
    - 实现property_names()
    - 实现method_names()
    - _Requirements: 11.1, 11.2_
  - [ ]* 6.6 编写RType属性测试
    - **Property 3: Type-Safe Property Access**
    - **Property 5: RType Shared Pointer Return**
    - **Property 9: Template and Dynamic Property Equivalence**
    - **Property 10: Argument Type Auto-Conversion**
    - **Property 12: Const and Non-Const Method Support**
    - **Property 13: Void Return Type Support**
    - **Property 23: Property Enumeration Completeness**
    - **Property 24: Property Category Information**
    - **Validates: Requirements 2.3, 3.2, 6.3, 7.1, 7.3, 7.5, 11.1, 11.2, 11.3, 11.5**

- [ ] 7. Checkpoint - 确保RType测试通过
  - 确保所有测试通过，如有问题请询问用户

- [x] 8. 错误处理系统实现
  - [x] 8.1 实现异常类层次结构
    - 实现 ReflectionError 基类
    - 实现 TypeNotRegisteredError
    - 实现 PropertyNotFoundError
    - 实现 MethodSignatureMismatchError
    - 实现 ObjectNotCreatedError
    - _Requirements: 9.1, 9.2, 9.3_
  - [x] 8.2 集成错误处理到各组件
    - 在TypeManager中集成类型未注册错误
    - 在RType中集成属性未找到错误
    - 在方法调用中集成签名不匹配错误
    - _Requirements: 2.4, 9.1, 9.2, 9.3_
  - [ ]* 8.3 编写错误处理属性测试
    - **Property 4: Non-Existent Property Exception**
    - **Property 17: Unregistered Type Exception**
    - **Property 18: Property Not Found Exception Details**
    - **Property 19: Method Call Failure Exception Details**
    - **Property 20: Creation Failure Cleanup**
    - **Validates: Requirements 2.4, 9.1, 9.2, 9.3, 9.5**

- [ ] 9. Checkpoint - 确保错误处理测试通过
  - 确保所有测试通过，如有问题请询问用户

- [x] 10. 容器反射支持实现
  - [x] 10.1 实现ISequentialContainer接口
    - 实现size(), empty(), clear()
    - 实现at(), push_back(), pop_back()
    - 实现Iterator
    - _Requirements: 8.2_
  - [x] 10.2 实现IAssociativeContainer接口
    - 实现size(), empty(), clear()
    - 实现find(), contains(), insert(), erase()
    - 实现KeyValueIterator
    - _Requirements: 8.3_
  - [x] 10.3 实现容器类型检测和包装
    - 实现顺序容器检测 (vector, list, deque)
    - 实现关联容器检测 (map, unordered_map, set, unordered_set)
    - 实现容器属性访问返回正确接口
    - _Requirements: 8.4, 8.5, 8.6_
  - [ ]* 10.4 编写容器反射属性测试
    - **Property 14: Sequential Container Operations**
    - **Property 15: Associative Container Operations**
    - **Property 16: Container Category Detection**
    - **Validates: Requirements 8.2, 8.3, 8.4, 8.5, 8.6**

- [ ] 11. Checkpoint - 确保容器反射测试通过
  - 确保所有测试通过，如有问题请询问用户

- [x] 12. 清理和集成
  - [x] 12.1 删除旧的ECS相关代码
    - 删除 Entity.hpp
    - 删除 ECS相关的测试和示例
    - _Requirements: 4.2_
  - [x] 12.2 创建RTTM.hpp主入口文件
    - 包含所有公共API
    - 提供RTTM_REGISTRATION宏
    - _Requirements: 2.5, 4.1_
  - [x] 12.3 更新文档和示例
    - 更新README.md
    - 创建使用示例
    - _Requirements: 2.5_

- [ ] 13. Final Checkpoint - 确保所有测试通过
  - 运行完整测试套件
  - 确保所有属性测试通过
  - 如有问题请询问用户

## Notes

- 任务标记 `*` 为可选测试任务，可跳过以加快MVP开发
- 每个任务引用具体需求以保证可追溯性
- Checkpoint任务确保增量验证
- 属性测试验证通用正确性属性
- 单元测试验证具体示例和边界情况
