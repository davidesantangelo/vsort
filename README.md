# VSort: Advanced Sorting Algorithm Optimized for Apple Silicon

VSort is a high-performance sorting library that leverages the unique architecture of Apple Silicon processors to deliver exceptional performance. By intelligently utilizing ARM NEON vector instructions, Grand Central Dispatch, and the heterogeneous core design of M-series chips, VSort achieves remarkable efficiency particularly for partially sorted data collections.

VSort represents an advanced sorting solution for Apple Silicon, combining ARM NEON, GCD, heterogeneous core management, and adaptive algorithms. Its performance is competitive, with significant improvements for partially sorted or large datasets, making it suitable for high-performance computing tasks on macOS.

**Current Version: 0.3.0**

**Author: [Davide Santangelo](https://github.com/davidesantangelo)**

## Apple Silicon Optimizations

VSort's optimizations are designed to maximize performance on Apple Silicon, with the following key aspects:

1. **ARM NEON SIMD Vectorization**:  
   ARM NEON allows for 128-bit vector registers, enabling simultaneous processing of multiple elements. This is particularly effective for sorting large datasets, as vectorized operations can reduce the time complexity of partitioning and comparison steps.

2. **Grand Central Dispatch (GCD) Integration**:  
   GCD, Apple's task scheduling system, is used for parallelizing sorting tasks across multiple cores. This is crucial for leveraging Apple Silicon's multi-core architecture, distributing work to both P-cores and E-cores.

3. **Performance & Efficiency Core Awareness**:  
   VSort now intelligently detects and utilizes both Performance (P) and Efficiency (E) cores on Apple Silicon chips, assigning workloads appropriately for optimal speed and power efficiency. The algorithm dynamically adjusts thread allocation based on array size and core availability.

4. **Cache-Optimized Memory Access**:  
   VSort uses adaptive chunk sizing based on cache characteristics, with optimal chunk sizes for L2 cache on Apple Silicon (typically 128KB per core). This minimizes cache misses and improves throughput.

5. **Branch Prediction Optimization**:  
   Sorting algorithms often suffer from branch mispredictions, especially in quicksort. VSort reduces these by optimizing branch-heavy operations.

6. **Adaptive Algorithm Selection**:  
   VSort selects algorithms based on array size and data patterns, using insertion sort for small arrays (threshold around 16 elements) and quicksort for larger ones, with parallel processing for very large arrays.

## Parallel Workload Management

For optimal performance across all cores, VSort 0.3.0 features:

- Work-stealing queue structure for better load balancing
- Balanced binary tree approach for parallel merging
- Adaptive chunk sizing that balances cache efficiency and parallelism
- Optimized thread count allocation based on array size and core types
- Vectorized merge operations using NEON when possible

## Performance Characteristics

The latest benchmark results on Apple Silicon (M4) show impressive performance:

```
Array Size     Random (ms)    Nearly Sorted (ms) Reverse (ms)   
----------------------------------------------------------------
1000           0.03           0.01               0.01           
10000          0.42           0.36               0.23           
100000         7.63           4.12               2.32           
1000000        51.24          25.83              14.78          
```

VSort demonstrates:
- Near-instantaneous sorting for small arrays (<10K elements)
- Excellent performance for already sorted or reverse-sorted data
- Up to 3.5x speedup for reverse-sorted data compared to random data
- Efficient scaling from small to large array sizes


## Algorithm Comparison

When compared to traditional sorting algorithms on Apple Silicon:

```
┌────────────┬─────────────────┬────────────────┬────────────────┬─────────────────┐
│ Size       │ vsort (ms)      │ quicksort (ms) │ mergesort (ms) │ std::qsort (ms) │
├────────────┼─────────────────┼────────────────┼────────────────┼─────────────────┤
│ 10,000     │ 0.26            │ 0.30           │ 0.27           │ 0.40            │
│            │ (baseline)      │ (1.15×)        │ (1.04×)        │ (1.54×)         │
├────────────┼─────────────────┼────────────────┼────────────────┼─────────────────┤
│ 100,000    │ 3.36            │ 3.84           │ 3.41           │ 5.13            │
│            │ (baseline)      │ (1.14×)        │ (1.01×)        │ (1.53×)         │
├────────────┼─────────────────┼────────────────┼────────────────┼─────────────────┤
│ 1,000,000  │ 39.52           │ 44.16          │ 39.86          │ 60.35           │
│            │ (baseline)      │ (1.12×)        │ (1.01×)        │ (1.53×)         │
└────────────┴─────────────────┴────────────────┴────────────────┴─────────────────┘
```

VSort provides:
- Competitive performance with standard library implementations
- Consistent scaling across different array sizes
- Optimized memory usage vs traditional merge sort
- Predictable performance characteristics

## Benchmark Results

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

## Latest Optimization Highlights

The latest version of VSort includes several key optimizations:

1. **Enhanced NEON Vectorization**: Improved SIMD implementation for partitioning that processes 4 integers at once
2. **Adaptive Algorithm Selection**: Specialized handling for different data patterns with fast-path optimizations
3. **Optimized Thread Management**: Better work distribution based on array size and core characteristics
4. **Cache Line Alignment**: Memory access patterns aligned with Apple Silicon's cache architecture
5. **Compiler-specific Optimization Flags**: Taking advantage of Clang's Apple Silicon optimizations

These optimizations help VSort consistently outperform other sorting algorithms, especially on Apple Silicon hardware, while maintaining low memory overhead compared to mergesort.

## Large Array Performance

VSort performs exceptionally well with large arrays:

```
Large Array Test
----------------
Attempting with 2000000 elements... SUCCESS
Initializing array... DONE
Sorting 2000000 elements... DONE (32.55 ms)
Verifying (sampling)... PASSED
```

## Key Technical Features

- **Vectorized Partitioning**: Uses NEON SIMD instructions to accelerate the partitioning step
- **Parallelized Sorting**: Distributes work across multiple cores for large arrays
- **Optimized Insertion Sort**: Enhanced for Apple's branch prediction units
- **Three-Way Partitioning**: Efficiently handles duplicate elements
- **QoS Integration**: Uses Apple's Quality of Service APIs for optimal core utilization

## Usage

```c
#include "vsort.h"

// Sort an array of integers
int array[] = {5, 2, 9, 1, 5, 6};
int size = sizeof(array) / sizeof(array[0]);
vsort(array, size);
```

## Building and Testing

### System Requirements

- **Recommended**: Apple Silicon Mac (M1/M2/M3/M4) running macOS 11+
- **Compatible**: Any modern UNIX-like system with a C compiler
- **Dependencies**: Standard C libraries only (no external dependencies)

### Building the Library

```bash
# Clone the repository
git clone https://github.com/davidesantangelo/vsort.git
cd vsort

# Build everything (library, tests, and examples)
make

# Build just the library
make libvsort.a

# Check compiler and build settings
make compiler-info
```

The unified Makefile automatically detects your hardware and applies appropriate optimizations:

- On Apple Silicon, NEON vector instructions and GCD optimizations are enabled
- OpenMP parallelization is used when available (install GCC or LLVM with OpenMP for best results)
- Standard optimizations are applied on other platforms

### Running Tests

```bash
# Run all tests
make test

# Build tests without running them
make tests

# Run individual tests directly
./tests/test_basic         # Basic functionality tests
./tests/test_performance   # Performance benchmark tests
./tests/test_apple_silicon # Tests specific to Apple Silicon
```

### Running Benchmarks

```bash
# Build all examples
make examples

# Run the standard benchmark with custom parameters
./examples/benchmark --size 1000000 --algorithms "vsort,quicksort,mergesort,std::sort"

# Run the Apple Silicon specific benchmark
./examples/apple_silicon_test

# Run a basic example
./examples/basic_example
```

### Examples

The project includes several example programs demonstrating different use cases for the vsort library:

### Basic Examples

- **basic_example.c**: Simple demonstration of sorting an integer array
- **float_sorting_example.c**: Shows how to sort floating-point numbers
- **char_sorting_example.c**: Demonstrates sorting character arrays

### Advanced Examples

- **custom_comparator_example.c**: Shows how to use a custom comparator function to sort in descending order
- **struct_sorting_example.c**: Demonstrates sorting structures based on different fields
- **performance_benchmark.c**: Benchmarks vsort against the standard library's qsort
- **benchmark.c**: More detailed performance testing

### Platform-Specific Examples

- **apple_silicon_test.c**: Tests optimizations specific to Apple Silicon (only available on macOS/arm64)

### Building and Running the Examples

To build all examples:

```bash
make examples
```

To run a specific example:

```bash
./examples/basic_example
./examples/struct_sorting_example
# etc.
```

### Cleaning Up

```bash
# Remove all generated files
make clean
```

### Troubleshooting Build Issues

- **Missing OpenMP**: This warning is expected with Apple's default Clang, but doesn't affect basic functionality
- **Apple Silicon optimizations unavailable**: Will occur on non-ARM64 macOS systems
- **Compilation errors**: Usually due to incompatible compiler flags; try `make compiler-info` to diagnose

## Computational Complexity

VSort is based on an optimized hybrid sorting algorithm with the following complexity characteristics:

- **Time Complexity**:
  - **Best Case**: O(n log n) - When the array is already nearly sorted
  - **Average Case**: O(n log n) - Expected performance for random input
  - **Worst Case**: O(n²) - Mitigated by median-of-three pivot selection

- **Space Complexity**: 
  - O(log n) - Iterative implementation uses a stack for managing partitions
  - O(1) additional memory for in-place sorting operations

While the asymptotic complexity matches traditional quicksort, VSort's optimization techniques significantly improve performance constants:

- Insertion sort for small subarrays reduces overhead
- Vectorized operations process multiple elements in parallel
- Parallel execution on multiple cores reduces effective time complexity
- Adaptive algorithm selection optimizes for different data patterns
- Cache-friendly memory access patterns minimize performance bottlenecks

These optimizations don't change the mathematical complexity but deliver substantial real-world performance improvements, especially on Apple Silicon hardware where the vectorization and parallelization benefits are maximized.

## Performance Tuning

VSort automatically optimizes for:

- **Array size**: Different algorithms for small vs. large arrays
- **Data patterns**: Optimizations for sorted or nearly-sorted data
- **Hardware capabilities**: Adaptation to available cores and vector units
- **Memory constraints**: Balance between memory usage and speed

Advanced users can tune performance by:

- Adjusting thresholds for insertion sort vs. quicksort
- Configuring parallelization parameters
- Setting custom memory management options
- Enabling specific SIMD optimizations

## License

This project is licensed under the MIT License - see the LICENSE file for details.

---
© 2025 Davide Santangelo
