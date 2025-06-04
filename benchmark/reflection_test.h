#pragma once
#include <string>
#include <vector>

//-------------------------------------------------------
// 测试数据结构定义 - 适用于所有反射库
//-------------------------------------------------------

// 简单向量结构
struct Vector3
{
    float x, y, z;

    Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

// 中等复杂度的测试类
class TestClass
{
public:
    float C;
    int D;
    bool E;
    std::string F;

    TestClass() : C(0.0f), D(0), E(false) {}
};

// 嵌套对象类
class NestedObject
{
public:
    std::vector<int> numbers;
    std::string name;
    double value;
    TestClass inner;

    NestedObject() : value(0.0) {}
};

// 顶层复杂对象
class JsonSerializable
{
public:
    int A;
    std::string B;
    TestClass D;
    Vector3 position;
    NestedObject complex;
    std::vector<Vector3> points;

    JsonSerializable() : A(0) {}
};