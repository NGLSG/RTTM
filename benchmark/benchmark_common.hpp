/**
 * @file benchmark_common.hpp
 * @brief Common test classes for RTTM vs RTTR benchmark comparison
 */

#ifndef BENCHMARK_COMMON_HPP
#define BENCHMARK_COMMON_HPP

#include <string>
#include <vector>
#include <cmath>

// ============================================================================
// Test Classes - Identical for both RTTM and RTTR
// ============================================================================

struct Vector3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    
    Vector3() = default;
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float dot(const Vector3& other) const { return x * other.x + y * other.y + z * other.z; }
};

class SimpleClass {
public:
    int intValue = 0;
    float floatValue = 0.0f;
    std::string stringValue;
    
    int getInt() const { return intValue; }
    void setInt(int v) { intValue = v; }
    float getFloat() const { return floatValue; }
    void setFloat(float v) { floatValue = v; }
};

class ComplexClass {
public:
    int id = 0;
    std::string name;
    Vector3 position;
    std::vector<int> scores;
    
    int getId() const { return id; }
    void setId(int v) { id = v; }
    const std::string& getName() const { return name; }
    void setName(const std::string& n) { name = n; }
    const Vector3& getPosition() const { return position; }
};

class DeepClass {
public:
    int level1 = 0;
    int level2 = 0;
    int level3 = 0;
    int level4 = 0;
    int level5 = 0;
    std::string data;
    
    int compute() const { return level1 + level2 + level3 + level4 + level5; }
};

#endif // BENCHMARK_COMMON_HPP
