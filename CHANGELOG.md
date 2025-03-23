# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.3.4] - 2025-03-23

### Added
- Improved P/E core utilization with intelligent workload distribution
- More precise detection of performance and efficiency cores on Apple Silicon
- Chunk complexity analysis to assign work to appropriate core types
- Separate dispatch queues with different QoS levels for different core types

### Improved
- More accurate detection of P-core and E-core counts using sysctl
- Better parallel sorting performance through optimized core utilization
- Power efficiency by directing appropriate work to efficiency cores

## [0.3.3] - 2025-03-20

### Added
- Dynamic threshold adjustment to auto-tune sorting parameters based on hardware
- Hardware detection system that identifies CPU model, cache sizes, and core types
- Adaptive calibration for sorting thresholds to optimize performance by hardware
- Enhanced CPU detection on both Apple Silicon and Linux/Windows platforms

### Improved
- Better utilization of CPU cache hierarchy for optimal sorting performance
- Refined parallel threshold calculation based on available cores and cache sizes
- More intelligent P-core vs E-core utilization on Apple Silicon
- Improved documentation of hardware detection and calibration process

## [0.3.2] - 2025-03-19

### Added
- CI testing on Apple Silicon hardware to validate NEON optimizations
- Logger system with configurable verbosity levels to replace printf debugging

### Improved
- Removed all printf debugging statements from production code
- Better error handling with proper logging instead of console output
- Simplified code paths for improved maintainability
- Enhanced parallel sorting strategy with better sequential merge for reliability
- Added more thorough error detection and validation throughout sorting process
- Improved memory alignment handling for NEON SIMD operations
- Added robust Makefile for simpler build process on all platforms
- Properly marked unused functions with attributes to silence compiler warnings

### Fixed
- Memory alignment issues in SIMD operations
- CI pipeline now tests on relevant hardware platforms
- Code cleanup for production readiness
- CMake build errors with `-mfpu=neon` flag on Apple Silicon platforms
- Corrected boundary checking in parallel merge algorithm to prevent sorting failures
- Addressed potential memory issues in performance tests with large arrays
- Improved verification checks to detect and recover from incorrect merges
- Fixed compiler warnings about unused functions in the codebase

## [0.3.1] - 2025-03-19

### Added
- Complete NEON vectorization implementation for partition operations
- Enhanced vectorized merge operation with optimized fast paths for homogeneous data segments
- Specialized vector processing for different data patterns

### Improved
- Partition function now processes 4 elements at once using NEON SIMD
- Merge function utilizes vectorized comparisons and bulk transfers
- Better detection of sequential data patterns for faster processing

## [0.3.0] - 2025-03-18

### Added
- Improved performance for Apple Silicon with P-core and E-core detection
- Adaptive chunk sizing based on cache characteristics
- New parallel merge function with vectorization support
- Work-stealing queue structure for better load balancing

### Changed
- Completely redesigned parallel sorting implementation
- Enhanced thread allocation strategy based on array size and core types
- Improved workload distribution for better performance
- Better balancing between cache efficiency and parallelism
- More efficient parallel merging using a balanced binary tree approach

### Fixed
- Memory management in parallel sorting operation
- Potential performance bottlenecks in larger arrays

## [0.2.1] - 2025-03-17

### Fixed
- Cross-platform compatibility with Windows systems
- Memory allocation handling in parallel sorting algorithm
- Build system improvements for cross-platform compilation
- CMake configuration for Visual Studio compiler flags

### Improved
- Error handling in quicksort algorithm
- Thread allocation for platforms without Grand Central Dispatch
- Cache utilization in sorting algorithm
- Documentation for Windows compatibility

## [0.2.0] - 2025-03-17

### Added
- Enhanced NEON vectorization for parallel processing
- Improved merging algorithm for parallel sorting
- Better memory management with fallbacks for allocation failures
- Advanced core detection for optimal thread distribution

### Fixed
- Parallel sorting algorithm now correctly merges sorted chunks
- Random array sorting reliability issues
- Edge cases in array bounds checking
- Memory leaks in parallel sorting implementation

## [0.1.0] - 2025-03-16

### Added
- Initial release of VSort library
- Core sorting algorithm implementation for integer arrays
- Type-specific sorting for floats and chars
- Custom comparator function support for generic sorting
- Basic examples and test suite

### Fixed
- Updated vsort_with_comparator function to include size_t parameter for element size
- Fixed custom comparator examples to pass correct element size
- Fixed struct sorting examples to use proper type names
