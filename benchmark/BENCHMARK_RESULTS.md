# RTTM vs RTTR Benchmark Results

**Date**: 2026-01-08  
**Platform**: Windows, LLVM/Clang Release build  
**CPU**: 20 cores @ 3878 MHz, L3 30720 KiB

## Summary

RTTM provides multiple API levels for different use cases:
1. **Semi-static API** (PropertyHandle/MethodHandle) - Fastest, requires compile-time type knowledge
2. **Pure Dynamic API** (Instance/Variant) - DLL-compatible, no compile-time type knowledge needed
3. **Cached Dynamic API** (DynamicProperty/DynamicMethod) - Best of both worlds

## Performance Comparison

### Type Lookup

| Operation | RTTM | RTTR | Winner |
|-----------|------|------|--------|
| Static (by type) | 0.61 ns | 0.63 ns | ~equal |
| Dynamic (by name) | 5.74 ns | 11.5 ns | **RTTM 2x** |

### Object Creation

| Operation | RTTM | RTTR | Winner |
|-----------|------|------|--------|
| Simple class | 30.7 ns | 93.9 ns | **RTTM 3x** |
| Complex class | 32.4 ns | 101 ns | **RTTM 3x** |

### Property Access (Semi-static API)

| Operation | RTTM | RTTR | Winner |
|-----------|------|------|--------|
| Read (full dynamic) | 7.14 ns | 24.7 ns | **RTTM 3.5x** |
| Read (cached) | 0.36 ns | 14.3 ns | **RTTM 40x** |
| Write (full dynamic) | 7.22 ns | 14.2 ns | **RTTM 2x** |
| Write (cached) | 0.32 ns | 4.89 ns | **RTTM 15x** |
| Multiple properties | 0.32 ns | 105 ns | **RTTM 328x** |
| Deep access (5 props) | 0.83 ns | 75.8 ns | **RTTM 91x** |

### Method Invocation (Semi-static API)

| Operation | RTTM | RTTR | Winner |
|-----------|------|------|--------|
| No args (full dynamic) | 21.1 ns | 27.9 ns | **RTTM 1.3x** |
| No args (cached) | 3.12 ns | 14.3 ns | **RTTM 4.6x** |
| With arg (full dynamic) | 21.9 ns | 16.7 ns | RTTR 1.3x |
| With arg (cached) | 5.50 ns | 6.80 ns | **RTTM 1.2x** |
| Complex return | 3.30 ns | 35.2 ns | **RTTM 10.7x** |

### TypedMethodHandle (Zero-overhead)

| Operation | RTTM | Direct Call | Overhead |
|-----------|------|-------------|----------|
| No args | 0.22 ns | 0.24 ns | ~0% |
| With arg | 0.13 ns | 0.17 ns | ~0% |

### Pure Dynamic API (Instance + Variant)

This API is comparable to RTTR's dynamic API - no compile-time type knowledge needed.

| Operation | RTTM | RTTR | Winner |
|-----------|------|------|--------|
| Instance Create | 43.2 ns | 93.9 ns | **RTTM 2.2x** |
| Property Read (Variant) | 18.5 ns | 24.7 ns | **RTTM 1.3x** |
| Property Write (Variant) | 19.8 ns | 14.2 ns | RTTR 1.4x |
| Method Call (no args) | 19.3 ns | 27.9 ns | **RTTM 1.4x** |
| Method Call (with arg) | 20.0 ns | 16.7 ns | RTTR 1.2x |

### Cached Dynamic API (DynamicProperty/DynamicMethod)

Cache the property/method handle once, then use repeatedly without string lookup.

| Operation | RTTM | RTTR | Winner |
|-----------|------|------|--------|
| Property Read | 1.52 ns | 14.3 ns | **RTTM 9.4x** |
| Property Write | 1.60 ns | 4.89 ns | **RTTM 3.1x** |
| Method Call (no args) | 4.86 ns | 14.3 ns | **RTTM 2.9x** |
| Method Call (with arg) | 6.14 ns | 6.80 ns | **RTTM 1.1x** |

### Batch Operations

| Operation | RTTM | RTTR | Winner |
|-----------|------|------|--------|
| Creation (100 objects) | 4040 ns | 10876 ns | **RTTM 2.7x** |
| Property Access (100) | 11.4 ns | 1641 ns | **RTTM 144x** |
| Method Calls (100) | 246 ns | 1619 ns | **RTTM 6.6x** |

### Enumeration

| Operation | RTTM | RTTR | Winner |
|-----------|------|------|--------|
| Property names | 7.81 ns | 45.4 ns | **RTTM 5.8x** |
| Method names | 14.1 ns | 68.0 ns | **RTTM 4.8x** |

## Key Findings

1. **Semi-static API is extremely fast**: Cached property access is 40x faster than RTTR
2. **Pure Dynamic API is competitive**: RTTM's Instance/Variant API matches or beats RTTR in most cases
3. **Cached Dynamic API is the sweet spot**: 9.4x faster property reads than RTTR while remaining fully dynamic
4. **TypedMethodHandle achieves zero overhead**: Direct function pointer calls with no type erasure
5. **Object creation is 3x faster**: RTTM's factory system is more efficient
6. **Batch operations show massive gains**: 144x faster for batch property access

## API Recommendations

| Use Case | Recommended API | Performance |
|----------|-----------------|-------------|
| Hot loops with known types | PropertyHandle/TypedMethodHandle | ~0.3 ns |
| Repeated access, unknown types | DynamicProperty/DynamicMethod | ~1.5-6 ns |
| One-off access, DLL plugins | Instance::get_property/invoke | ~18-20 ns |
| Serialization/deserialization | Instance + Variant | ~18-20 ns |
