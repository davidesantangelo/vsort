# VSort: Advanced Sorting Algorithm Optimized for Apple Silicon

VSort is a high-performance sorting library that leverages the unique architecture of Apple Silicon processors to deliver exceptional performance. By intelligently utilizing ARM NEON vector instructions, Grand Central Dispatch, and the heterogeneous core design of M-series chips, VSort achieves remarkable efficiency particularly for partially sorted data collections.

**Author: [Davide Santangelo](https://github.com/davidesantangelo)**

## Table of Contents
- [Features & Optimizations](#features--optimizations)
- [Performance](#performance)
- [Usage](#usage)
- [Building & Testing](#building-and-testing)
- [Technical Details](#technical-details)
- [Development](#development)

## Features & Optimizations

### Apple Silicon Optimizations

VSort's optimizations are designed to maximize performance on Apple Silicon, with the following key aspects:

1. **ARM NEON SIMD Vectorization**:  
   ARM NEON allows for 128-bit vector registers, enabling simultaneous processing of multiple elements. This is particularly effective for sorting large datasets, as vectorized operations can reduce the time complexity of partitioning and comparison steps.

2. **Grand Central Dispatch (GCD) Integration**:  
   GCD, Apple's task scheduling system, is used for parallelizing sorting tasks across multiple cores. This is crucial for leveraging Apple Silicon's multi-core architecture, distributing work to both P-cores and E-cores.

3. **Performance & Efficiency Core Awareness**:  
   VSort intelligently detects and utilizes both Performance (P) and Efficiency (E) cores on Apple Silicon chips, assigning workloads appropriately for optimal speed and power efficiency. Complex, disordered chunks are processed on high-performance cores while simpler, more ordered chunks are sent to efficiency cores for better overall throughput and power usage.

4. **Cache-Optimized Memory Access**:  
   VSort uses adaptive chunk sizing based on cache characteristics, with optimal chunk sizes for L2 cache on Apple Silicon (typically 128KB per core). This minimizes cache misses and improves throughput.

5. **Branch Prediction Optimization**:  
   Sorting algorithms often suffer from branch mispredictions, especially in quicksort. VSort reduces these by optimizing branch-heavy operations.

6. **Adaptive Algorithm Selection**:  
   VSort selects algorithms based on array size and data patterns, using insertion sort for small arrays (threshold around 16 elements) and quicksort for larger ones, with parallel processing for very large arrays.

### Recent Improvements

Recent updates have further enhanced VSort's reliability and performance:

1. **Dynamic Threshold Adjustment**: The new auto-tuning system detects hardware characteristics and automatically calibrates sorting thresholds for optimal performance on any device
2. **Hardware-Aware Optimization**: Added detection of CPU model, cache hierarchy, and core types to make intelligent performance decisions
3. **Improved Error Recovery**: The parallel merge implementation now includes robust validation with automatic error recovery
4. **Enhanced Memory Management**: Better handling of memory alignment for SIMD operations
5. **Optimized Build System**: Simplified build process with proper architecture detection and compiler flags
6. **Increased Stability**: Fixed boundary cases and edge conditions in parallel sorting
7. **Cleaner Codebase**: Fixed compiler warnings and improved code maintainability
8. **Robust Testing**: Enhanced performance tests to avoid memory issues with large arrays

### Key Technical Features

- **Vectorized Partitioning**: Uses NEON SIMD instructions to accelerate the partitioning step
- **Parallelized Sorting**: Distributes work across multiple cores for large arrays
- **Optimized Insertion Sort**: Enhanced for Apple's branch prediction units
- **Three-Way Partitioning**: Efficiently handles duplicate elements
- **QoS Integration**: Uses Apple's Quality of Service APIs for optimal core utilization

### Current Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| NEON Vectorization | Implemented | Complete vector operations for partitioning and merging with optimized paths for homogeneous data chunks |
| P/E Core Detection | Implemented | Automatic detection and utilization of different core types |
| P/E Core Workload Optimization | Implemented | Intelligent workload distribution based on chunk complexity |
| Dynamic Threshold Adjustment | Implemented | Auto-calibrates sorting thresholds based on CPU, cache, and core characteristics |
| Grand Central Dispatch | Implemented | Used for parallel workload distribution |
| Adaptive Algorithm Selection | Implemented | Intelligent switching between algorithms based on hardware and data patterns |
| Cache Optimization | Implemented | Cache-aware thresholds based on L1/L2/L3 cache sizes |
| Branch Prediction Optimization | Planned | To be implemented in next release |

### Parallel Workload Management

- Work distribution based on chunk complexity to P-cores and E-cores
- Work-stealing queue structure for better load balancing
- Balanced binary tree approach for parallel merging
- Adaptive chunk sizing that balances cache efficiency and parallelism
- Optimized thread count allocation based on array size and core types
- Vectorized merge operations using NEON when possible

## Performance

### Performance Characteristics

The latest benchmark results on Apple Silicon (M4) show impressive performance:

```
Array Size     Random (ms)    Nearly Sorted (ms) Reverse (ms)   
----------------------------------------------------------------
1000           0.06           0.03               0.02           
10000          0.72           0.32               0.26           
100000         2.74           1.29               0.50           
1000000        13.93          4.81               3.15           
```

VSort demonstrates:
- Near-instantaneous sorting for small arrays (<10K elements)
- Excellent performance for already sorted or reverse-sorted data
- Up to 4.4x speedup for reverse-sorted data compared to random data
- Efficient scaling from small to large array sizes

### Algorithm Comparison

When compared to traditional sorting algorithms on Apple Silicon:

```
┌────────────┬─────────────────┬────────────────┬────────────────┬─────────────────┐
│ Size       │ vsort (ms)      │ quicksort (ms) │ mergesort (ms) │ std::qsort (ms) │
├────────────┼─────────────────┼────────────────┼────────────────┼─────────────────┤
│ 10,000     │ 0.33            │ 0.36           │ 0.33           │ 0.48            │
│            │ (baseline)      │ (1.09×)        │ (1.00×)        │ (1.46×)         │
├────────────┼─────────────────┼────────────────┼────────────────┼─────────────────┤
│ 100,000    │ 1.20            │ 4.14           │ 3.62           │ 5.23            │
│            │ (baseline)      │ (3.45×)        │ (3.02×)        │ (4.36×)         │
├────────────┼─────────────────┼────────────────┼────────────────┼─────────────────┤
│ 1,000,000  │ 10.09           │ 44.87          │ 39.81          │ 59.88           │
│            │ (baseline)      │ (4.45×)        │ (3.95×)        │ (5.94×)         │
└────────────┴─────────────────┴────────────────┴────────────────┴─────────────────┘
```

VSort provides:
- Dramatic performance improvements over traditional algorithms, especially for large datasets
- Up to 5.94× faster than standard library sorting functions
- Performance parity with mergesort for small arrays, but significantly better with larger data
- Exceptional scaling advantage as dataset size increases

### Benchmark Results

Standard benchmark comparison with 1,000,000 random integers:

```
Algorithm       Avg Time (ms)   Min Time (ms)   Verification   
--------------------------------------------------------------
vsort           38.46           36.27           PASSED         
quicksort       45.33           45.14           PASSED         
mergesort       39.68           39.42           PASSED         
std::sort       60.70           60.46           PASSED         
```

VSort shows excellent minimum times (36.27ms), significantly better than all other algorithms including mergesort's best time (39.42ms). This indicates that VSort can achieve superior peak performance in optimal conditions.

### Large Array Performance

VSort performs exceptionally well with large arrays:

```
Large Array Test
----------------
Attempting with 2000000 elements... SUCCESS
Initializing array... DONE
Sorting 2000000 elements... DONE (6.36 ms)
Verifying (sampling)... PASSED
```

## Usage

```c
#include "vsort.h"

// Sort an array of integers
int array[] = {5, 2, 9, 1, 5, 6};
int size = sizeof(array) / sizeof(array[0]);
vsort(array, size);
```
### System Requirements

- **Recommended**: Apple Silicon Mac (M1/M2/M3/M4) running macOS 11+
- **Compatible**: Any modern UNIX-like system with a C compiler
- **Dependencies**: Standard C libraries only (no external dependencies)

### Building the Library

```bash
# Clone the repository
git clone https://github.com/davidesantangelo/vsort.git
cd vsort

# Option 1: Use CMake (recommended for cross-platform)
mkdir build
cd build
cmake ..
cmake --build .

# Option 2: Use Makefile (simpler for Unix-like systems)
make

# Option 3: Use the build script (for quick builds on macOS)
./build_script.sh

# Run all tests
make test   # If using make
ctest       # If using CMake
```

CMake automatically detects your hardware and applies appropriate optimizations:

- On Apple Silicon, NEON vector instructions and GCD optimizations are enabled
- OpenMP parallelization is used when available (install GCC or LLVM with OpenMP for best results)
- Standard optimizations are applied on other platforms

### Running Tests and Benchmarks

```bash
# From the build directory, run all tests
ctest

# Run specific tests
./tests/test_basic         # Basic functionality tests
./tests/test_performance   # Performance benchmark tests
./tests/test_apple_silicon # Tests specific to Apple Silicon

# Run the standard benchmark with custom parameters
./examples/benchmark --size 1000000 --algorithms "vsort,quicksort,mergesort,std::sort"

# Run the Apple Silicon specific benchmark
./examples/apple_silicon_test
```

### Examples

The project includes several example programs demonstrating different use cases:

#### Basic Examples
- **basic_example.c**: Simple demonstration of sorting an integer array
- **float_sorting_example.c**: Shows how to sort floating-point numbers
- **char_sorting_example.c**: Demonstrates sorting character arrays

#### Advanced Examples
- **custom_comparator_example.c**: Shows how to use a custom comparator function
- **struct_sorting_example.c**: Demonstrates sorting structures based on different fields
- **performance_benchmark.c**: Benchmarks vsort against standard library sorting
- **apple_silicon_test.c**: Tests optimizations specific to Apple Silicon

## Technical Details

### Latest Optimization Highlights

The latest version of VSort includes several key optimizations:

1. **Dynamic Threshold Adjustment**: Automatically detects CPU model, cache sizes, and core configuration to set optimal thresholds for insertion sort, vectorization, parallel processing, and radix sort
2. **Enhanced Hardware Detection**: Recognizes Apple Silicon processors and adjusts for their unique characteristics, with fallbacks for other platforms
3. **P/E Core Workload Optimization**: Analyzes chunk complexity to distribute work optimally between performance and efficiency cores
4. **Cache-Aware Processing**: Calibrates sorting parameters based on detected L1, L2, and L3 cache sizes to minimize cache misses
5. **Enhanced NEON Vectorization**: Complete SIMD implementation for partitioning and merging that processes 4 integers at once, with specialized fast paths for homogeneous data segments
6. **Adaptive Algorithm Selection**: Specialized handling for different data patterns with fast-path optimizations
7. **Optimized Thread Management**: Better work distribution based on array size and core characteristics
8. **Cache Line Alignment**: Memory access patterns aligned with Apple Silicon's cache architecture
9. **Compiler-specific Optimization Flags**: Taking advantage of Clang's Apple Silicon optimizations

### Computational Complexity

VSort is based on an optimized hybrid sorting algorithm with the following complexity characteristics:

- **Time Complexity**:
  - **Best Case**: O(n log n) - When the array is already nearly sorted
  - **Average Case**: O(n log n) - Expected performance for random input
  - **Worst Case**: O(n²) - Mitigated by median-of-three pivot selection

- **Space Complexity**: 
  - O(log n) - Iterative implementation uses a stack for managing partitions
  - O(1) additional memory for in-place sorting operations

While the asymptotic complexity matches traditional quicksort, VSort's optimization techniques significantly improve performance constants.

### Performance Tuning

VSort automatically optimizes for:

- **Hardware detection**: Identifies CPU model, cache sizes, and core configuration
- **Array size**: Different algorithms for small vs. large arrays with auto-calibrated thresholds
- **Data patterns**: Optimizations for sorted or nearly-sorted data
- **Hardware capabilities**: Adaptation to available cores and vector units
- **Memory constraints**: Balance between memory usage and speed

VSort's dynamic threshold adjustment means that the library works optimally without manual configuration, but advanced users can still override settings if needed.

## Development

### Roadmap

Upcoming improvements planned for VSort:

1. ~~**Enhanced NEON Implementation**: Complete vectorization of partition and merge operations~~ ✓ Implemented
2. ~~**Dynamic Threshold Adjustment**: Auto-tune thresholds based on hardware characteristics~~ ✓ Implemented
3. ~~**Better P/E Core Utilization**: Improved workload distribution between core types~~ ✓ Implemented
4. **Cache-Line Aligned Memory Access**: Further optimizing memory access patterns
5. **Branch Prediction Improvements**: Reducing branch mispredictions in comparison operations

### License

This project is licensed under the MIT License - see the LICENSE file for details.

---
© 2025 Davide Santangelo
