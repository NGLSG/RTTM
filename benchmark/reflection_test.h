#pragma once
#include <string>
#include <vector>
#define NOMINMAX



//-------------------------------------------------------
// 测试数据结构定义
//-------------------------------------------------------

// 三维向量结构体
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vector3(const Vector3&) = default;
    Vector3& operator=(const Vector3&) = default;
    ~Vector3() = default;
};

// 测试类结构体
struct TestClass {
    float C = 0.0f;
    int D = 0;
    bool E = false;
    std::string F;

    TestClass() = default;
    TestClass(const TestClass&) = default;
    TestClass& operator=(const TestClass&) = default;
    ~TestClass() = default;
};

// 嵌套对象结构体
struct NestedObject {
    std::string name;
    double value = 0.0;
    TestClass inner;
    std::vector<int> numbers;

    NestedObject() = default;
    NestedObject(const NestedObject&) = default;
    NestedObject& operator=(const NestedObject&) = default;
    ~NestedObject() = default;
};

// 复杂的可序列化对象结构体
struct JsonSerializable {
    int A = 0;
    std::string B;
    TestClass D;
    Vector3 position;
    NestedObject complex;
    std::vector<Vector3> points;

    JsonSerializable() = default;
    JsonSerializable(const JsonSerializable&) = default;
    JsonSerializable& operator=(const JsonSerializable&) = default;
    ~JsonSerializable() = default;
};
//-------------------------------------------------------
// 统一的测试数据结构 - 简化复杂度，确保公平性
//-------------------------------------------------------

struct SimpleTestData {
    int intValue;
    float floatValue;
    std::string stringValue;
    bool boolValue;
};

struct MediumTestData {
    int id;
    std::string name;
    float score;
    bool active;
    SimpleTestData nested;
};

