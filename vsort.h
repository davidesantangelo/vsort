/**
 * VSort: High-Performance Sorting Algorithm for Apple Silicon
 *
 * Version 0.2.1
 *
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 */

#ifndef VSORT_H
#define VSORT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

// Platform detection
#if defined(_WIN32) || defined(_MSC_VER)
#define VSORT_WINDOWS 1
// Windows DLL export/import macros
#ifdef VSORT_EXPORTS
#define VSORT_API __declspec(dllexport)
#else
#define VSORT_API __declspec(dllimport)
#endif
#else
#define VSORT_API
#if defined(__APPLE__)
#define VSORT_APPLE 1
#elif defined(__linux__)
#define VSORT_LINUX 1
#endif
#endif

/**
 * Version information
 */
#define VSORT_VERSION_MAJOR 0
#define VSORT_VERSION_MINOR 2
#define VSORT_VERSION_PATCH 1
#define VSORT_VERSION_STRING "0.2.1"

    /**
     * HyperSort - A revolutionary sorting algorithm
     *
     * HyperSort analyzes the input data and automatically selects the optimal
     * sorting strategy based on array size, data distribution, and hardware
     * characteristics. It combines multiple sorting techniques with advanced
     * optimizations to achieve superior performance across a wide range of inputs.
     *
     * @param arr Array to be sorted
     * @param n Length of the array
     */
    VSORT_API void vsort(int arr[], int n);

    /*
     * Generic sorting function with custom comparator
     * Takes a comparison function pointer similar to qsort
     */
    VSORT_API void vsort_with_comparator(void *arr, int n, size_t size, int (*compare)(const void *, const void *));

    /*
     * Type-specific sorting functions
     */
    VSORT_API void vsort_float(float arr[], int n);
    VSORT_API void vsort_char(char arr[], int n);

    // Utility functions
    VSORT_API int get_num_processors();

#ifdef __cplusplus
}
#endif

#endif /* VSORT_H */
