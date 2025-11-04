/**
 * VSort: High-Performance Sorting Algorithm for Apple Silicon
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

// GCC/Clang restrict and branch‐prediction hints
#if defined(__GNUC__) || defined(__clang__)
#define VSORT_RESTRICT __restrict__
#define VSORT_LIKELY(x) __builtin_expect(!!(x), 1)
#define VSORT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define VSORT_RESTRICT
#define VSORT_LIKELY(x) (x)
#define VSORT_UNLIKELY(x) (x)
#endif

// Add hot‐attribute macro for portability
#if defined(_MSC_VER)
#define VSORT_HOT
#else
#define VSORT_HOT __attribute__((hot))
#endif

/**
 * Version information
 */
#define VSORT_VERSION_MAJOR 1
#define VSORT_VERSION_MINOR 0
#define VSORT_VERSION_PATCH 0
#define VSORT_VERSION_STRING "1.0.0"

// -----------------------------------------------------------------------------
// Public configuration API
// -----------------------------------------------------------------------------

typedef enum
{
    VSORT_KIND_INT32 = 0,
    VSORT_KIND_FLOAT32,
    VSORT_KIND_CHAR8,
    VSORT_KIND_GENERIC
} vsort_data_kind_t;

typedef enum
{
    VSORT_OK = 0,
    VSORT_ERR_INVALID_ARGUMENT = -1,
    VSORT_ERR_ALLOCATION_FAILED = -2,
    VSORT_ERR_UNSUPPORTED_TYPE = -3
} vsort_result_t;

#define VSORT_FLAG_ALLOW_PARALLEL (1u << 0)
#define VSORT_FLAG_ALLOW_RADIX (1u << 1)
#define VSORT_FLAG_FORCE_STABLE (1u << 2)

typedef struct
{
    void *data;                              /**< Pointer to the data to be sorted */
    size_t length;                           /**< Number of elements in the buffer */
    size_t element_size;                     /**< Size of each element (bytes) */
    vsort_data_kind_t kind;                  /**< Data classification */
    int (*comparator)(const void *, const void *); /**< Comparator for generic paths */
    unsigned int flags;                      /**< Behavioural flags (VSORT_FLAG_*) */
} vsort_options_t;

VSORT_API vsort_result_t vsort_sort(const vsort_options_t *options);
VSORT_API void vsort_set_default_flags(unsigned int flags);
VSORT_API unsigned int vsort_default_flags(void);
VSORT_API const char *vsort_version(void);

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
    VSORT_API void vsort(int *VSORT_RESTRICT arr, int n) VSORT_HOT;

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
    VSORT_API void vsort_float(float *VSORT_RESTRICT arr, int n) VSORT_HOT;

    /**
     * @brief Sorts an array of chars in ascending order.
     *
     * Currently falls back to standard qsort. For optimal performance,
     * this should be adapted using the library's internal sorting logic.
     *
     * @param arr The char array to be sorted.
     * @param n The number of elements in the array.
     */
    VSORT_API void vsort_char(char *VSORT_RESTRICT arr, int n) VSORT_HOT;

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
    VSORT_API void vsort_with_comparator(void *VSORT_RESTRICT arr, int n, size_t size,
                                         int (*compare)(const void *, const void *)) VSORT_HOT;

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
