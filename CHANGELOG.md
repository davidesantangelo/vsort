# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2025-03-24

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
