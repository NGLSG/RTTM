# Requirements Document

## Introduction

本文档定义了RTTM (Runtime Turbo Mirror) C++20动态反射库的重构需求。当前库存在架构复杂、API不一致、内存管理混乱等问题，需要重构以提升易用性、性能和可扩展性。重构后将专注于核心反射功能，移除ECS相关功能，并全面采用C++20现代特性。

## Glossary

- **RTTM**: Runtime Turbo Mirror，运行时动态反射库
- **RType**: 运行时类型信息包装类，用于动态访问类型的属性和方法
- **Registry_**: 类型注册器，用于注册类的属性、方法和构造函数
- **Factory**: 对象工厂，负责动态创建类型实例
- **TypeInfo**: 类型元信息结构，存储类型的成员、方法、工厂等信息
- **GlobalTypeManager**: 全局类型信息管理器，存储所有已注册类型的元信息
- **Concept**: C++20概念，用于约束模板参数
- **Coroutine**: C++20协程，用于异步操作

## Requirements

### Requirement 1: 采用C++20现代特性

**User Story:** As a developer, I want to use modern C++20 features, so that the code is cleaner, safer, and more expressive.

#### Acceptance Criteria

1. THE System SHALL require C++20 standard as minimum compiler requirement
2. THE System SHALL use concepts to constrain template parameters for type safety
3. THE System SHALL use std::span for array-like parameter passing where appropriate
4. THE System SHALL use std::format for string formatting instead of stringstream
5. THE System SHALL use constexpr and consteval for compile-time computations where possible
6. THE System SHALL use requires clauses instead of SFINAE for template constraints
7. THE System SHALL use designated initializers for struct initialization
8. THE System SHALL use three-way comparison operator (<=>) where applicable

### Requirement 2: 简化核心API设计

**User Story:** As a developer, I want a simple and intuitive API, so that I can quickly integrate reflection capabilities without learning complex interfaces.

#### Acceptance Criteria

1. THE Registry_ API SHALL provide a fluent interface with chainable methods for property, method, and constructor registration
2. WHEN registering a type, THE System SHALL automatically register default constructor if available
3. THE RType API SHALL provide type-safe property access through template methods
4. WHEN accessing a non-existent property, THE System SHALL throw a descriptive exception with the property name and type
5. THE System SHALL provide a single header include for basic reflection functionality

### Requirement 3: 统一内存管理策略

**User Story:** As a developer, I want consistent memory management, so that I can avoid memory leaks and undefined behavior.

#### Acceptance Criteria

1. THE System SHALL use std::shared_ptr consistently for all dynamically allocated objects
2. WHEN creating an RType instance, THE System SHALL return a shared_ptr wrapper
3. THE Factory SHALL support both pool-based and standard allocation strategies
4. WHEN an object is no longer referenced, THE System SHALL automatically release its memory
5. THE System SHALL NOT expose raw pointers in public APIs except for performance-critical paths with clear ownership semantics

### Requirement 4: 精简模块结构

**User Story:** As a developer, I want a focused reflection library, so that I can use it without unnecessary dependencies.

#### Acceptance Criteria

1. THE System SHALL provide core reflection functionality in a single main header (RTTM.hpp)
2. THE System SHALL remove ECS-related code (Entity.hpp and related components)
3. THE Core_Reflection module SHALL be self-contained without external dependencies beyond C++20 standard library
4. THE System SHALL organize internal implementation into clearly separated utility headers
5. THE Public API SHALL be minimal and focused on reflection use cases only

### Requirement 5: 优化类型注册机制

**User Story:** As a developer, I want efficient type registration, so that startup time is minimized and runtime performance is optimal.

#### Acceptance Criteria

1. THE System SHALL support lazy type registration that defers work until first use
2. WHEN a type is registered multiple times, THE System SHALL ignore duplicate registrations without error
3. THE System SHALL cache type information to avoid repeated lookups
4. THE GlobalTypeManager SHALL use thread-safe data structures for concurrent access
5. THE System SHALL use constexpr type name generation at compile-time

### Requirement 6: 改进属性访问性能

**User Story:** As a developer, I want fast property access, so that reflection does not become a performance bottleneck.

#### Acceptance Criteria

1. THE System SHALL cache property offsets after first access
2. WHEN accessing the same property repeatedly, THE System SHALL use cached offset without map lookup
3. THE GetProperty method SHALL support both type-safe template version and dynamic version
4. THE System SHALL provide direct memory access for primitive types without virtual calls
5. THE Property_Access latency SHALL be within 10x of direct member access for cached properties

### Requirement 7: 简化方法调用机制

**User Story:** As a developer, I want straightforward method invocation, so that I can call reflected methods without complex type casting.

#### Acceptance Criteria

1. THE Invoke method SHALL automatically convert compatible argument types (e.g., const char* to std::string)
2. WHEN method signature does not match, THE System SHALL provide clear error message with expected and actual signatures
3. THE System SHALL support both member functions and const member functions
4. THE Method wrapper SHALL cache function pointers for repeated calls
5. THE System SHALL support void return types without special handling

### Requirement 8: 统一容器反射支持

**User Story:** As a developer, I want to reflect STL containers through unified abstractions, so that I can manipulate container members dynamically without knowing specific container types.

#### Acceptance Criteria

1. THE System SHALL provide two abstract container interfaces: Sequential_Container and Associative_Container
2. THE Sequential_Container interface SHALL support size(), at(), push_back(), pop_back(), begin(), end() operations
3. THE Associative_Container interface SHALL support size(), find(), insert(), erase(), contains() operations
4. WHEN a container property is accessed, THE System SHALL return the appropriate container interface based on container category
5. THE System SHALL automatically detect and categorize std::vector, std::list, std::deque as Sequential_Container
6. THE System SHALL automatically detect and categorize std::map, std::unordered_map, std::set, std::unordered_set as Associative_Container
7. THE Container interfaces SHALL use concepts to constrain element types

### Requirement 9: 改进错误处理

**User Story:** As a developer, I want clear error messages, so that I can quickly diagnose and fix reflection-related issues.

#### Acceptance Criteria

1. WHEN a type is not registered, THE System SHALL throw an exception with the type name
2. WHEN a property is not found, THE System SHALL include available property names in the error message
3. WHEN a method call fails, THE System SHALL report the method name, expected signature, and actual arguments
4. THE System SHALL provide a validation mode that checks all registrations at startup
5. IF an error occurs during object creation, THEN THE System SHALL clean up partial state and report the failure point

### Requirement 10: 支持继承关系反射

**User Story:** As a developer, I want to reflect class hierarchies, so that I can access base class members through derived class instances.

#### Acceptance Criteria

1. THE Registry_ SHALL support base() method to declare inheritance relationships
2. WHEN a derived type is registered with base(), THE System SHALL include base class properties and methods
3. THE System SHALL support multiple levels of inheritance
4. WHEN accessing a base class property through derived instance, THE System SHALL calculate correct offset
5. THE System SHALL NOT require re-registration of base class members in derived classes

### Requirement 11: 提供序列化支持接口

**User Story:** As a developer, I want serialization hooks, so that I can easily implement JSON/XML serialization for reflected types.

#### Acceptance Criteria

1. THE RType SHALL provide GetProperties() method returning all property names and types
2. THE System SHALL support iteration over all registered properties of a type
3. WHEN serializing, THE System SHALL provide property type information (primitive, class, enum, container)
4. THE System SHALL support custom serialization callbacks for complex types
5. THE Property_Info SHALL include type name, offset, and category (class, enum, variable, sequence, associative)

### Requirement 12: 线程安全保证

**User Story:** As a developer, I want thread-safe reflection, so that I can use reflection in multi-threaded applications.

#### Acceptance Criteria

1. THE GlobalTypeManager SHALL use read-write locks for type registration and lookup
2. WHEN multiple threads access the same RType, THE System SHALL ensure data consistency
3. THE Factory creation SHALL be thread-safe without global locks on the hot path
4. THE System SHALL support thread-local caching for frequently accessed type information
5. THE System SHALL document which operations are thread-safe and which require external synchronization

### Requirement 13: 减少头文件依赖

**User Story:** As a developer, I want minimal header dependencies, so that compilation times are reduced.

#### Acceptance Criteria

1. THE Core headers SHALL NOT include unnecessary STL headers
2. THE System SHALL use forward declarations where possible
3. WHEN only type registration is needed, THE System SHALL NOT require full type definitions
4. THE Public API headers SHALL be self-contained without requiring specific include order
5. THE System SHALL use C++20 modules where supported by the build system
