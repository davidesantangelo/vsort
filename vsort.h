/**
 * VSort: High-Performance Sorting Algorithm for Apple Silicon
 *
 * Version 0.4.0
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

#include <stddef.h> // For size_t

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
#define VSORT_API // Default for non-Windows
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
#define VSORT_VERSION_MINOR 4
#define VSORT_VERSION_PATCH 0
#define VSORT_VERSION_STRING "0.4.0"

    /**
     * @brief Sorts an array of integers in ascending order.
     *
     * Automatically selects the optimal sorting strategy based on array size,
     * data distribution (detects nearly sorted), and hardware characteristics.
     * Uses optimizations like NEON (if available and implemented), GCD parallelism
     * (including parallel merge passes), radix sort, quicksort, and insertion sort.
     *
     * @param arr The integer array to be sorted.
     * @param n The number of elements in the array.
     */
    VSORT_API void vsort(int arr[], int n);

    /**
     * @brief Sorts an array of floats in ascending order.
     *
     * Applies similar optimization strategies as vsort() for floats,
     * including parallelism (with parallel merge passes) and adaptive algorithm selection.
     * Note: Radix sort is not typically used for floats; primarily uses
     * optimized quicksort/insertion sort. NEON optimizations for floats
     * are possible but not yet implemented.
     *
     * @param arr The float array to be sorted.
     * @param n The number of elements in the array.
     */
    VSORT_API void vsort_float(float arr[], int n);

    /**
     * @brief Sorts an array of chars in ascending order.
     *
     * Currently falls back to standard qsort. For optimal performance,
     * this should be adapted using the library's internal sorting logic.
     *
     * @param arr The char array to be sorted.
     * @param n The number of elements in the array.
     */
    VSORT_API void vsort_char(char arr[], int n);

    /**
     * @brief Generic sorting function with a custom comparator.
     *
     * Currently falls back to standard qsort. For optimal performance,
     * this should be adapted using the library's internal sorting logic,
     * potentially requiring modifications to quicksort/merge to handle
     * the custom comparator efficiently.
     *
     * @param arr Pointer to the start of the array.
     * @param n Number of elements in the array.
     * @param size Size of each element in bytes.
     * @param compare Pointer to the comparison function (like qsort).
     */
    VSORT_API void vsort_with_comparator(void *arr, int n, size_t size, int (*compare)(const void *, const void *));

    /**
     * @brief Gets the number of physical processor cores available.
     *
     * @return The number of physical cores detected.
     */
    VSORT_API int get_num_processors(void);

    /**
     * @brief Initializes the vsort library.
     *
     * Detects hardware characteristics (cores, cache, NEON support) and
     * calibrates internal thresholds for optimal sorting algorithm selection.
     * This is called automatically on the first sort operation but can be
     * called explicitly if needed. Calling multiple times has no effect
     * after the first successful initialization.
     */
    VSORT_API void vsort_init(void);

#ifdef __cplusplus
}
#endif

#endif /* VSORT_H */
