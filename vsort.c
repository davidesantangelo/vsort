/**
 * VSort: High-Performance Sorting Algorithm for Apple Silicon
 *
 * A sorting library specifically optimized for Apple Silicon processors,
 * leveraging ARM NEON vector instructions and Grand Central Dispatch.
 *
 * Version 0.3.2
 *
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "vsort.h"
#include "vsort_logger.h"

// Platform-specific includes
#if defined(_WIN32) || defined(_MSC_VER)
// Windows-specific headers
#include <windows.h>
#define sysconf(x) 0
#define _SC_NPROCESSORS_ONLN 0
// Windows sleep function (milliseconds)
#define sleep(x) Sleep((x) * 1000)
#else
// Unix/POSIX headers
#include <unistd.h>
#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif
#endif

// Memory alignment for SIMD operations
#define VSORT_ALIGN 16 // 128-bit (16-byte) alignment for NEON
#define VSORT_ALIGNED __attribute__((aligned(VSORT_ALIGN)))

// Helper macro for min/max
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Mark variables as intentionally unused
#define UNUSED(x) ((void)(x))

// Apple Silicon specific includes
#if defined(__APPLE__) && defined(__arm64__)
#include <arm_neon.h>          // For NEON SIMD operations
#include <dispatch/dispatch.h> // For Grand Central Dispatch
#endif

/**
 * Dynamic threshold determination based on CPU characteristics
 */
typedef struct
{
    int insertion_threshold;    // Switch to insertion sort below this
    int vector_threshold;       // Switch to vectorized operations above this
    int parallel_threshold;     // Switch to parallel operations above this
    int radix_threshold;        // Switch to radix sort above this (for integers)
    int cache_optimal_elements; // Elements that fit in L1/L2 cache
} vsort_thresholds_t;

// Global thresholds - initialized by vsort_init_thresholds()
static vsort_thresholds_t thresholds = {
    .insertion_threshold = 16,
    .vector_threshold = 64,
    .parallel_threshold = 100000,
    .radix_threshold = 1000000,
    .cache_optimal_elements = 16384};

// Forward declarations for all internal functions
static void vsort_init_thresholds(void);
#if defined(__APPLE__) && defined(__arm64__)
static void *vsort_aligned_malloc(size_t size) __attribute__((unused));
static void vsort_aligned_free(void *ptr) __attribute__((unused));
#else
static void *vsort_aligned_malloc(size_t size);
static void vsort_aligned_free(void *ptr);
#endif
static void swap(int *a, int *b);
static void insertion_sort(int arr[], int low, int high);
static int partition(int arr[], int low, int high);
static void quicksort(int arr[], int low, int high);
static bool vsort_is_nearly_sorted(const int *arr, int size);
static int get_physical_core_count(void);
static void merge_sorted_arrays(int arr[], int temp[], int left, int mid, int right);
#if defined(__APPLE__) && defined(__arm64__)
static void parallel_merge(int arr[], int temp[], int left, int mid, int right) __attribute__((unused));
#else
static void parallel_merge(int arr[], int temp[], int left, int mid, int right);
#endif
static void radix_sort(int arr[], int n);
static void vsort_sequential(int *arr, int size);
static void vsort_parallel(int *arr, int size);

/**
 * Initialize thresholds based on system characteristics
 */
static void vsort_init_thresholds(void)
{
    static bool initialized = false;

    if (initialized)
    {
        return;
    }

    // Default values are already set in the static initialization

    // Adjust based on CPU cores
    int cores = get_physical_core_count();

    // L1/L2 cache considerations - typical L1 cache is 32KB per core
    // Assuming 4-byte integers, around 8K elements would fit in L1
    thresholds.cache_optimal_elements = 8192;

    // Lower parallel threshold with more cores
    if (cores >= 8)
    {
        thresholds.parallel_threshold = 50000;
    }
    else if (cores >= 4)
    {
        thresholds.parallel_threshold = 75000;
    }

    // Adjust insertion threshold for branch prediction efficiency
    // Modern CPUs typically do well with insertion sort up to 16-24 elements
    thresholds.insertion_threshold = 16;

// Vector threshold should be set based on SIMD width and overhead
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    // For NEON (128-bit), processing 4 integers at once
    thresholds.vector_threshold = 32;
#endif

    vsort_log_debug("Initialized thresholds - insertion: %d, vector: %d, parallel: %d, radix: %d",
                    thresholds.insertion_threshold, thresholds.vector_threshold,
                    thresholds.parallel_threshold, thresholds.radix_threshold);

    initialized = true;
}

/**
 * Aligned memory allocation for SIMD operations
 */
static void *vsort_aligned_malloc(size_t size)
{
    void *ptr = NULL;

#if defined(_MSC_VER)
    ptr = _aligned_malloc(size, VSORT_ALIGN);
#else
    // posix_memalign requires size to be a multiple of alignment
    size_t aligned_size = (size + VSORT_ALIGN - 1) & ~(VSORT_ALIGN - 1);
    if (posix_memalign(&ptr, VSORT_ALIGN, aligned_size))
    {
        return NULL;
    }
#endif

    return ptr;
}

/**
 * Free aligned memory
 */
static void vsort_aligned_free(void *ptr)
{
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

/**
 * Swap two integers
 */
static void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

/**
 * Insertion sort - efficient for small arrays
 */
static void insertion_sort(int arr[], int low, int high)
{
    for (int i = low + 1; i <= high; i++)
    {
        int key = arr[i];
        int j = i - 1;

        // Branch-optimized inner loop
        while (j >= low && arr[j] > key)
        {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

/**
 * Partitioning for quicksort with median-of-three pivot selection
 */
static int partition(int arr[], int low, int high)
{
    // Median-of-three pivot selection
    int mid = low + (high - low) / 2;

    // Sort low, mid, high elements
    if (arr[low] > arr[mid])
        swap(&arr[low], &arr[mid]);
    if (arr[mid] > arr[high])
        swap(&arr[mid], &arr[high]);
    if (arr[low] > arr[mid])
        swap(&arr[low], &arr[mid]);

    // Use median as pivot and move to end
    int pivot = arr[mid];
    swap(&arr[mid], &arr[high]);

    // Partition using Lomuto scheme
    int i = low - 1;

    for (int j = low; j < high; j++)
    {
        if (arr[j] <= pivot)
        {
            i++;
            swap(&arr[i], &arr[j]);
        }
    }

    swap(&arr[i + 1], &arr[high]);
    return i + 1;
}

/**
 * Iterative quicksort implementation to avoid stack overflow
 */
static void quicksort(int arr[], int low, int high)
{
    // Early exit for invalid or small ranges
    if (low >= high)
        return;

    // Use insertion sort for small arrays
    if (high - low < thresholds.insertion_threshold)
    {
        insertion_sort(arr, low, high);
        return;
    }

    // Use stack-based iterative implementation to avoid recursion overhead
    int *stack = malloc((high - low + 1) * sizeof(int));
    if (!stack)
    {
        vsort_log_error("Memory allocation failed in quicksort, falling back to recursive implementation");

        // Simple recursive fallback
        int pivot = partition(arr, low, high);
        if (pivot > low)
            quicksort(arr, low, pivot - 1);
        if (pivot < high)
            quicksort(arr, pivot + 1, high);
        return;
    }

    // Initialize stack for iterative approach
    int top = -1;
    stack[++top] = low;
    stack[++top] = high;

    while (top >= 0)
    {
        high = stack[top--];
        low = stack[top--];

        // Use insertion sort for small subarrays
        if (high - low < thresholds.insertion_threshold)
        {
            insertion_sort(arr, low, high);
            continue;
        }

        // Partition and push subarrays
        int pivot = partition(arr, low, high);

        // Push larger subarray first to reduce stack size
        if (pivot - low < high - pivot)
        {
            // Left subarray is smaller
            if (pivot + 1 < high)
            {
                stack[++top] = pivot + 1;
                stack[++top] = high;
            }
            if (low < pivot - 1)
            {
                stack[++top] = low;
                stack[++top] = pivot - 1;
            }
        }
        else
        {
            // Right subarray is smaller
            if (low < pivot - 1)
            {
                stack[++top] = low;
                stack[++top] = pivot - 1;
            }
            if (pivot + 1 < high)
            {
                stack[++top] = pivot + 1;
                stack[++top] = high;
            }
        }
    }

    free(stack);
}

/**
 * Check if array is nearly sorted - helps with adaptive algorithm selection
 */
static bool vsort_is_nearly_sorted(const int *arr, int size)
{
    if (size < 20)
        return false;

    int inversions = 0;
    int sample_size = MIN(100, size / 10);
    int step = MAX(1, size / sample_size);

    for (int i = 0; i < size - step; i += step)
    {
        if (arr[i] > arr[i + step])
            inversions++;
    }

    // Consider "nearly sorted" if less than 10% inversions
    return (inversions < sample_size / 10);
}

/**
 * Get physical core count
 */
static int get_physical_core_count(void)
{
#if defined(_WIN32) || defined(_MSC_VER)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__)
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
    return (int)sysconf(_SC_NPROCESSORS_CONF);
#endif
}

/**
 * Radix sort implementation for integers
 * Much faster than comparison-based sorts for large integer arrays
 */
static void radix_sort(int arr[], int n)
{
    if (n <= 1)
        return;

    // Find maximum element to know number of digits
    int max_val = arr[0];
    for (int i = 1; i < n; i++)
    {
        if (arr[i] > max_val)
            max_val = arr[i];
    }

    // Handle negative numbers by shifting
    int min_val = arr[0];
    for (int i = 1; i < n; i++)
    {
        if (arr[i] < min_val)
            min_val = arr[i];
    }

    // If we have negative values, shift all values to make them non-negative
    bool has_negative = (min_val < 0);
    int shift = has_negative ? -min_val : 0;

    // Create a temporary array for counting sort
    int *output = malloc(n * sizeof(int));
    if (!output)
    {
        vsort_log_error("Memory allocation failed in radix sort, falling back to quicksort");
        quicksort(arr, 0, n - 1);
        return;
    }

    // Apply shift if needed
    if (has_negative)
    {
        max_val += shift; // Update max_val with the shift
        for (int i = 0; i < n; i++)
        {
            arr[i] += shift;
        }
    }

    // Determine the number of bits required
    int num_bits = 0;
    while (max_val > 0)
    {
        max_val >>= 1;
        num_bits++;
    }

    // Process 8 bits at a time for cache efficiency
    const int bits_per_pass = 8;
    const int num_bins = 1 << bits_per_pass; // 256 bins
    const unsigned int mask = num_bins - 1;  // Mask for extracting bits

    // Allocate count array on stack for better performance
    int count[256];

    // Perform radix sort
    for (int shift = 0; shift < num_bits; shift += bits_per_pass)
    {
        // Clear count array
        memset(count, 0, num_bins * sizeof(int));

        // Count occurrences
        for (int i = 0; i < n; i++)
        {
            unsigned int bin = (arr[i] >> shift) & mask;
            count[bin]++;
        }

        // Compute prefix sum
        for (int i = 1; i < num_bins; i++)
        {
            count[i] += count[i - 1];
        }

        // Build output array
        for (int i = n - 1; i >= 0; i--)
        {
            unsigned int bin = (arr[i] >> shift) & mask;
            output[--count[bin]] = arr[i];
        }

        // Copy back to the original array
        memcpy(arr, output, n * sizeof(int));

        // If all elements are in the first bin, we're done
        if (count[0] == n)
            break;
    }

    // Undo the shift if we had negative numbers
    if (has_negative)
    {
        for (int i = 0; i < n; i++)
        {
            arr[i] -= shift;
        }
    }

    free(output);
}

/**
 * Merge sorted arrays - used as part of merge sort
 * Uses a temporary buffer for efficient merging
 */
static void merge_sorted_arrays(int arr[], int temp[], int left, int mid, int right)
{
    if (left > mid || mid >= right)
    {
        return; // Invalid range
    }

    // Copy both halves to temporary array for stability and cache efficiency
    int range_size = right - left + 1;
    if (range_size <= 0)
        return;

    // Using memmove instead of a loop for better performance
    memmove(&temp[left], &arr[left], range_size * sizeof(int));

    int i = left;    // Index for left subarray
    int j = mid + 1; // Index for right subarray
    int k = left;    // Index for merged array

    // Standard merge - using temp array for both sides for stability
    while (i <= mid && j <= right)
    {
        arr[k++] = (temp[i] <= temp[j]) ? temp[i++] : temp[j++];
    }

    // Copy remaining elements
    while (i <= mid)
        arr[k++] = temp[i++];
    while (j <= right)
        arr[k++] = temp[j++];

    // Verify the merge result is sorted (only in debug mode)
#ifdef DEBUG_VERIFICATION
    for (int v = left + 1; v <= right; v++)
    {
        if (arr[v] < arr[v - 1])
        {
            vsort_log_error("Merge verification failed at index %d, left=%d, mid=%d, right=%d",
                            v, left, mid, right);
            break;
        }
    }
#endif
}

// Simplify the parallel merge to make it more reliable
static void parallel_merge(int arr[], int temp[], int left, int mid, int right)
{
    // Just use the standard merge - it's more reliable
    merge_sorted_arrays(arr, temp, left, mid, right);
}

/**
 * Sequential sorting implementation - optimized for single-threaded operation
 */
static void vsort_sequential(int *arr, int size)
{
    if (!arr || size <= 1)
        return;

    // Check if array is nearly sorted - use insertion sort which performs well on such data
    if (vsort_is_nearly_sorted(arr, size))
    {
        vsort_log_info("Array appears nearly sorted, using optimized insertion sort");
        insertion_sort(arr, 0, size - 1);
        return;
    }

    // For very large integer arrays, radix sort is faster than comparison-based sorts
    if (size >= thresholds.radix_threshold)
    {
        vsort_log_info("Using radix sort for large array (size: %d)", size);
        radix_sort(arr, size);
        return;
    }

    // Use quicksort for most cases
    quicksort(arr, 0, size - 1);
}

/**
 * Parallel sorting implementation with GCD on Apple platforms
 */
static void vsort_parallel(int *arr, int size)
{
    // Fall back to sequential sort for small arrays
    if (!arr || size < thresholds.parallel_threshold)
    {
        vsort_sequential(arr, size);
        return;
    }

#if defined(__APPLE__) && defined(__arm64__)
    vsort_log_debug("Starting parallel sort with %d elements", size);

    // Get optimal thread count based on available cores
    int total_cores = get_physical_core_count();

    // Use fewer threads to avoid race conditions
    int thread_count = MAX(1, MIN(total_cores - 1, 4)); // Limit to 4 threads max

    vsort_log_debug("Using %d threads for parallel sort", thread_count);

    // Use larger chunks to reduce the number of merges needed
    // This is critical for stability
    int chunk_size = MAX(16384, size / (thread_count * 2));
    int num_chunks = (size + chunk_size - 1) / chunk_size;

    vsort_log_debug("Using %d chunks of size ~%d", num_chunks, chunk_size);

    // ALWAYS use heap allocation for temp array
    int *temp = malloc(size * sizeof(int));
    if (!temp)
    {
        vsort_log_error("Memory allocation failed in parallel sort, falling back to sequential");
        vsort_sequential(arr, size);
        return;
    }

    // Sort chunks in parallel first
    dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(
        DISPATCH_QUEUE_CONCURRENT, QOS_CLASS_USER_INITIATED, 0);
    dispatch_queue_t queue = dispatch_queue_create("com.vsort.parallel", attr);
    dispatch_group_t group = dispatch_group_create();

    for (int i = 0; i < num_chunks; i++)
    {
        dispatch_group_async(group, queue, ^{
          int start = i * chunk_size;
          int end = MIN(start + chunk_size - 1, size - 1);

          // Use standard quicksort for each chunk
          quicksort(arr, start, end);
        });
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    // Now we do a SEQUENTIAL merge - parallel merges are causing issues
    // This is slower but much more reliable
    for (int width = chunk_size; width < size; width *= 2)
    {
        vsort_log_debug("Merging with width %d", width);

        // Process each pair of chunks sequentially to avoid race conditions
        for (int left = 0; left < size; left += 2 * width)
        {
            int mid = MIN(left + width - 1, size - 1);
            int right = MIN(left + 2 * width - 1, size - 1);

            if (left < mid && mid < right)
            {
                // Standard merge - no parallel merge or SIMD for now
                merge_sorted_arrays(arr, temp, left, mid, right);

                // Quick verification of this segment
                bool segment_sorted = true;
                for (int v = left + 1; v <= right; v++)
                {
                    if (arr[v] < arr[v - 1])
                    {
                        segment_sorted = false;
                        vsort_log_error("Segment not properly merged at indices %d-%d", v - 1, v);
                        break;
                    }
                }

                // If segment isn't sorted, fix it immediately
                if (!segment_sorted)
                {
                    quicksort(arr, left, right);
                }
            }
        }
    }

    // Final verification of the entire array
    bool fully_sorted = true;
    for (int i = 1; i < size; i++)
    {
        if (arr[i] < arr[i - 1])
        {
            vsort_log_error("Final array validation failed at index %d", i);
            fully_sorted = false;
            break;
        }
    }

    if (!fully_sorted)
    {
        vsort_log_warning("Final verification failed, using sequential sort as fallback");
        vsort_sequential(arr, size);
    }

    // Clean up
    free(temp);
    dispatch_release(group);
    dispatch_release(queue);

#else
    // Fall back to sequential sort on non-Apple platforms
    vsort_log_info("Parallel sorting not available, using sequential sort");
    vsort_sequential(arr, size);
#endif
}

/**
 * Default comparator function for floats
 */
static int default_float_comparator(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa < fb) ? -1 : (fa > fb) ? 1
                                      : 0;
}

/**
 * Default comparator function for chars
 */
static int default_char_comparator(const void *a, const void *b)
{
    return (*(const char *)a - *(const char *)b);
}

/**
 * Public API implementation
 */

VSORT_API void vsort_init(void)
{
    static bool initialized = false;

    if (!initialized)
    {
        // Initialize logger
        vsort_log_init(VSORT_LOG_WARNING);

        // Initialize thresholds based on system capabilities
        vsort_init_thresholds();

        initialized = true;
    }
}

VSORT_API void vsort(int arr[], int n)
{
    // Initialize if not already done
    vsort_init();

    // Early exit for trivial cases
    if (!arr || n <= 1)
    {
        return;
    }

    vsort_log_debug("Starting vsort with array size: %d", n);

    // Select optimal algorithm based on array size and characteristics
    if (n <= thresholds.insertion_threshold)
    {
        // For very small arrays, just use insertion sort
        insertion_sort(arr, 0, n - 1);
    }
    else if (n >= thresholds.parallel_threshold)
    {
        // For large arrays, use parallel sorting if available
        vsort_parallel(arr, n);
    }
    else if (n >= thresholds.radix_threshold)
    {
        // For medium-large sized arrays, consider radix sort
        radix_sort(arr, n);
    }
    else
    {
        // For medium-sized arrays, use quicksort
        vsort_sequential(arr, n);
    }

    vsort_log_debug("vsort completed for array size: %d", n);
}

VSORT_API void vsort_with_comparator(void *arr, int n, size_t size, int (*compare)(const void *, const void *))
{
    // Generic sorting - fallback to standard qsort for now
    if (!arr || n <= 1 || size == 0 || !compare)
    {
        return;
    }

    qsort(arr, n, size, compare);
}

VSORT_API void vsort_float(float arr[], int n)
{
    if (!arr || n <= 1)
    {
        return;
    }

    qsort(arr, n, sizeof(float), default_float_comparator);
}

VSORT_API void vsort_char(char arr[], int n)
{
    if (!arr || n <= 1)
    {
        return;
    }

    qsort(arr, n, sizeof(char), default_char_comparator);
}

VSORT_API int get_num_processors(void)
{
    return get_physical_core_count();
}