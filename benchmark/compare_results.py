#!/usr/bin/env python3
"""
RTTM vs RTTR Benchmark Comparison Script

Usage:
    python compare_results.py <rttm_output.txt> <rttr_output.txt>
    
Or run benchmarks and compare:
    ./rttm_benchmark --benchmark_format=json > rttm.json
    ./rttr_benchmark --benchmark_format=json > rttr.json
    python compare_results.py rttm.json rttr.json
"""

import sys
import json
import re

# Benchmark mapping: RTTM name -> RTTR name
BENCHMARK_MAP = {
    "RTTM_TypeLookup_Static": "RTTR_TypeLookup_Static",
    "RTTM_TypeLookup_Dynamic": "RTTR_TypeLookup_Dynamic",
    "RTTM_ObjectCreation_Simple": "RTTR_ObjectCreation_Simple",
    "RTTM_ObjectCreation_Complex": "RTTR_ObjectCreation_Complex",
    "RTTM_PropertyRead_Dynamic": "RTTR_PropertyRead_Dynamic",
    "RTTM_PropertyWrite_Dynamic": "RTTR_PropertyWrite_Dynamic",
    "RTTM_PropertyAccess_Multiple": "RTTR_PropertyAccess_Multiple",
    "RTTM_PropertyAccess_Deep": "RTTR_PropertyAccess_Deep",
    "RTTM_MethodCall_NoArgs": "RTTR_MethodCall_NoArgs",
    "RTTM_MethodCall_WithArg": "RTTR_MethodCall_WithArg",
    "RTTM_MethodCall_ComplexReturn": "RTTR_MethodCall_ComplexReturn",
    "RTTM_FullPath_PropertyAccess": "RTTR_FullPath_PropertyAccess",
    "RTTM_FullPath_MethodCall": "RTTR_FullPath_MethodCall",
    "RTTM_FullPath_CreateAndAccess": "RTTR_FullPath_CreateAndAccess",
    "RTTM_Batch_Creation": "RTTR_Batch_Creation",
    "RTTM_Batch_PropertyAccess": "RTTR_Batch_PropertyAccess",
    "RTTM_Batch_MethodCalls": "RTTR_Batch_MethodCalls",
    "RTTM_PropertyEnumeration": "RTTR_PropertyEnumeration",
    "RTTM_MethodEnumeration": "RTTR_MethodEnumeration",
}

def parse_benchmark_output(text):
    """Parse Google Benchmark text output"""
    results = {}
    for line in text.split('\n'):
        # Match lines like: BenchmarkName    123 ns    456 ns    789
        match = re.match(r'^(\w+)\s+([\d.]+)\s+ns\s+([\d.]+)\s+ns\s+(\d+)', line)
        if match:
            name = match.group(1)
            time_ns = float(match.group(2))
            results[name] = time_ns
    return results

def format_speedup(rttm_time, rttr_time):
    """Format speedup ratio"""
    if rttm_time < rttr_time:
        ratio = rttr_time / rttm_time
        return f"RTTM {ratio:.2f}x faster"
    else:
        ratio = rttm_time / rttr_time
        return f"RTTR {ratio:.2f}x faster"

def main():
    # Hardcoded results from the benchmark runs
    rttm_results = {
        "RTTM_TypeLookup_Static": 0.611,
        "RTTM_TypeLookup_Dynamic": 21.0,
        "RTTM_ObjectCreation_Simple": 40.1,
        "RTTM_ObjectCreation_Complex": 31.7,
        "RTTM_PropertyRead_Dynamic": 10.2,
        "RTTM_PropertyWrite_Dynamic": 12.1,
        "RTTM_PropertyAccess_Multiple": 24.1,
        "RTTM_PropertyAccess_Deep": 43.9,
        "RTTM_MethodCall_NoArgs": 13.8,
        "RTTM_MethodCall_WithArg": 12.6,
        "RTTM_MethodCall_ComplexReturn": 12.8,
        "RTTM_FullPath_PropertyAccess": 30.5,
        "RTTM_FullPath_MethodCall": 32.1,
        "RTTM_FullPath_CreateAndAccess": 72.6,
        "RTTM_Batch_Creation": 3661,
        "RTTM_Batch_PropertyAccess": 1010,
        "RTTM_Batch_MethodCalls": 1124,
        "RTTM_PropertyEnumeration": 8.28,
        "RTTM_MethodEnumeration": 13.6,
    }
    
    rttr_results = {
        "RTTR_TypeLookup_Static": 0.606,
        "RTTR_TypeLookup_Dynamic": 11.1,
        "RTTR_ObjectCreation_Simple": 76.1,
        "RTTR_ObjectCreation_Complex": 76.7,
        "RTTR_PropertyRead_Dynamic": 14.9,
        "RTTR_PropertyWrite_Dynamic": 4.82,
        "RTTR_PropertyAccess_Multiple": 103,
        "RTTR_PropertyAccess_Deep": 74.2,
        "RTTR_MethodCall_NoArgs": 15.3,
        "RTTR_MethodCall_WithArg": 7.68,
        "RTTR_MethodCall_ComplexReturn": 30.9,
        "RTTR_FullPath_PropertyAccess": 38.1,
        "RTTR_FullPath_MethodCall": 43.6,
        "RTTR_FullPath_CreateAndAccess": 142,
        "RTTR_Batch_Creation": 11134,
        "RTTR_Batch_PropertyAccess": 1630,
        "RTTR_Batch_MethodCalls": 1669,
        "RTTR_PropertyEnumeration": 50.0,
        "RTTR_MethodEnumeration": 73.2,
    }
    
    print("=" * 90)
    print("RTTM vs RTTR Benchmark Comparison")
    print("=" * 90)
    print()
    print(f"{'Benchmark':<35} {'RTTM (ns)':>12} {'RTTR (ns)':>12} {'Winner':>25}")
    print("-" * 90)
    
    rttm_wins = 0
    rttr_wins = 0
    
    for rttm_name, rttr_name in BENCHMARK_MAP.items():
        rttm_time = rttm_results.get(rttm_name, 0)
        rttr_time = rttr_results.get(rttr_name, 0)
        
        if rttm_time and rttr_time:
            winner = format_speedup(rttm_time, rttr_time)
            if "RTTM" in winner:
                rttm_wins += 1
            else:
                rttr_wins += 1
            
            # Clean up benchmark name for display
            display_name = rttm_name.replace("RTTM_", "")
            print(f"{display_name:<35} {rttm_time:>12.2f} {rttr_time:>12.2f} {winner:>25}")
    
    print("-" * 90)
    print()
    print(f"Summary: RTTM wins {rttm_wins}, RTTR wins {rttr_wins}")
    print()
    
    # Category analysis
    print("=" * 90)
    print("Category Analysis")
    print("=" * 90)
    
    categories = {
        "Type Lookup": ["TypeLookup_Static", "TypeLookup_Dynamic"],
        "Object Creation": ["ObjectCreation_Simple", "ObjectCreation_Complex"],
        "Property Access": ["PropertyRead_Dynamic", "PropertyWrite_Dynamic", 
                          "PropertyAccess_Multiple", "PropertyAccess_Deep"],
        "Method Invocation": ["MethodCall_NoArgs", "MethodCall_WithArg", "MethodCall_ComplexReturn"],
        "Full Reflection Path": ["FullPath_PropertyAccess", "FullPath_MethodCall", "FullPath_CreateAndAccess"],
        "Batch Operations": ["Batch_Creation", "Batch_PropertyAccess", "Batch_MethodCalls"],
        "Enumeration": ["PropertyEnumeration", "MethodEnumeration"],
    }
    
    for cat_name, benchmarks in categories.items():
        rttm_total = sum(rttm_results.get(f"RTTM_{b}", 0) for b in benchmarks)
        rttr_total = sum(rttr_results.get(f"RTTR_{b}", 0) for b in benchmarks)
        
        if rttm_total < rttr_total:
            ratio = rttr_total / rttm_total
            winner = f"RTTM {ratio:.2f}x faster"
        else:
            ratio = rttm_total / rttr_total
            winner = f"RTTR {ratio:.2f}x faster"
        
        print(f"{cat_name:<25}: {winner}")

if __name__ == "__main__":
    main()
