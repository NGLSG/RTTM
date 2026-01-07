# RTTM vs RTTR Benchmark Results

**Environment:** 20-core CPU @ 3878 MHz, Clang/LLVM -O3  
**Methodology:** All benchmarks use `DoNotOptimize` + `ClobberMemory` for write operations with dependency chains to ensure accurate measurement.

## Performance Summary

### Property Access
| Level | RTTM | RTTR | RTTM Speedup |
|-------|------|------|--------------|
| Read FullDynamic | 5.53 ns | 21.3 ns | **3.9x** |
| Read Cached | 1.07 ns | 13.1 ns | **12.2x** |
| Write FullDynamic | 5.50 ns | 13.0 ns | **2.4x** |
| Write Cached | 1.50 ns | 3.98 ns | **2.7x** |
| Multiple (3 props) | 0.28 ns | 82.8 ns | **296x** |
| Deep (5 props) | 0.69 ns | 67.6 ns | **98x** |

### Method Invocation
| Level | RTTM | RTTR | RTTM Speedup |
|-------|------|------|--------------|
| NoArgs FullDynamic | 20.5 ns | 25.4 ns | **1.2x** |
| NoArgs Cached | 3.27 ns | 13.9 ns | **4.3x** |
| WithArg FullDynamic | 20.9 ns | 15.4 ns | 0.74x |
| WithArg Cached | 5.50 ns | 6.79 ns | **1.2x** |
| ComplexReturn | 3.28 ns | 25.2 ns | **7.7x** |

### Full Dynamic Path
| Benchmark | RTTM | RTTR | RTTM Speedup |
|-----------|------|------|--------------|
| TypeLookup_Dynamic | 5.74 ns | 11.4 ns | **2.0x** |
| ObjectCreation | 30.7 ns | 74.9 ns | **2.4x** |
| FullPath_Property | 20.6 ns | 33.9 ns | **1.6x** |
| FullPath_Method | 21.5 ns | 39.2 ns | **1.8x** |
| FullPath_Create+Access | 67.9 ns | 122 ns | **1.8x** |

### Batch Operations (100 objects)
| Benchmark | RTTM | RTTR | RTTM Speedup |
|-----------|------|------|--------------|
| Creation | 3692 ns | 9471 ns | **2.6x** |
| PropertyAccess | 9.94 ns | 1476 ns | **148x** |
| MethodCalls | 285 ns | 1428 ns | **5.0x** |

### Enumeration
| Benchmark | RTTM | RTTR | RTTM Speedup |
|-----------|------|------|--------------|
| Property | 5.95 ns | 40.1 ns | **6.7x** |
| Method | 10.3 ns | 59.1 ns | **5.7x** |

## Key Findings

1. **Cached property access**: RTTM 12x faster for read, 2.7x faster for write
2. **Multi-property access**: RTTM 98-296x faster (no variant conversion overhead)
3. **Full dynamic path**: RTTM 1.6-2.0x faster consistently
4. **Batch operations**: RTTM 2.6-148x faster
5. **Object creation**: RTTM 2.4x faster

**RTTR wins only in:** `MethodCall_WithArg_FullDynamic` (RTTM's method lookup with argc filtering is heavier)

## Overhead vs Direct Access

| Operation | RTTM Cached | Baseline | Overhead |
|-----------|-------------|----------|----------|
| PropertyRead | 1.07 ns | 0.21 ns | 5.1x |
| PropertyWrite | 1.50 ns | 0.10 ns | 15x |
| MethodCall | 3.27 ns | 0.21 ns | 16x |
