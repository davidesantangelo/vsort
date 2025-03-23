/**
 * VSort: High-Performance Sorting Algorithm for Apple Silicon
 *
 * A sorting library specifically optimized for Apple Silicon processors,
 * leveraging ARM NEON vector instructions and Grand Central Dispatch.
 *
 * Version 0.3.4
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
static void vsort_detect_hardware_characteristics(void);
static void vsort_calibrate_thresholds(void);
#if defined(__APPLE__) && defined(__arm64__)
static void *vsort_aligned_malloc(size_t size) __attribute__((unused));
static void vsort_aligned_free(void *ptr) __attribute__((unused));
static dispatch_queue_t create_p_core_queue(void);
static dispatch_queue_t create_e_core_queue(void);
static void distribute_work_by_core_type(int arr[], int size, int chunk_size, int num_chunks);
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

// Hardware characteristics structure
typedef struct
{
    int l1_cache_size;     // L1 cache size in bytes
    int l2_cache_size;     // L2 cache size in bytes
    int l3_cache_size;     // L3 cache size in bytes (if available)
    int cache_line_size;   // Cache line size in bytes
    int simd_width;        // SIMD register width in bytes
    int performance_cores; // Number of performance cores
    int efficiency_cores;  // Number of efficiency cores
    int total_cores;       // Total number of CPU cores
    bool has_simd;         // SIMD support flag
    bool has_neon;         // ARM NEON support flag
    double cpu_freq_ghz;   // CPU frequency in GHz (if available)
    char cpu_model[128];   // CPU model string
} vsort_hardware_t;

// Global hardware characteristics
static vsort_hardware_t hardware = {
    .l1_cache_size = 32768,   // Default: 32KB L1 cache
    .l2_cache_size = 2097152, // Default: 2MB L2 cache
    .l3_cache_size = 8388608, // Default: 8MB L3 cache
    .cache_line_size = 64,    // Default: 64-byte cache lines
    .simd_width = 16,         // Default: 128-bit SIMD (16 bytes)
    .performance_cores = 4,   // Default: 4 performance cores
    .efficiency_cores = 4,    // Default: 4 efficiency cores
    .total_cores = 8,         // Default: 8 total cores
    .has_simd = false,
    .has_neon = false,
    .cpu_freq_ghz = 2.0, // Default: 2.0 GHz
    .cpu_model = "Unknown"};

/**
 * Detect hardware characteristics to optimize thresholds
 */
static void vsort_detect_hardware_characteristics(void)
{
    // Get total number of cores
    hardware.total_cores = get_physical_core_count();

#if defined(__APPLE__) && defined(__arm64__)
    // On Apple Silicon, try to detect P-cores and E-cores
    if (hardware.total_cores >= 8)
    {
        // For M-series chips, improve P/E core detection
        FILE *cmd;
        char buffer[256];
        int detected_p_cores = 0;
        int detected_e_cores = 0;

        // Try to get precise core counts using sysctl
        cmd = popen("sysctl -n hw.perflevel0.logicalcpu 2>/dev/null", "r");
        if (cmd && fgets(buffer, sizeof(buffer), cmd))
        {
            detected_p_cores = atoi(buffer);
        }
        if (cmd)
            pclose(cmd);

        cmd = popen("sysctl -n hw.perflevel1.logicalcpu 2>/dev/null", "r");
        if (cmd && fgets(buffer, sizeof(buffer), cmd))
        {
            detected_e_cores = atoi(buffer);
        }
        if (cmd)
            pclose(cmd);

        // If we got valid values, use them
        if (detected_p_cores > 0 && detected_e_cores > 0)
        {
            hardware.performance_cores = detected_p_cores;
            hardware.efficiency_cores = detected_e_cores;
            vsort_log_info("Detected precise core counts: %d P-cores, %d E-cores",
                           hardware.performance_cores, hardware.efficiency_cores);
        }
        else
        {
            // Fallback to approximation based on total cores
            hardware.performance_cores = hardware.total_cores / 2;
            hardware.efficiency_cores = hardware.total_cores - hardware.performance_cores;
        }
    }
    else
    {
        // If fewer cores, assume they're all performance cores
        hardware.performance_cores = hardware.total_cores;
        hardware.efficiency_cores = 0;
    }

    hardware.has_neon = true;
    hardware.has_simd = true;

// Detect cache information
#ifdef __APPLE__
    // Try to get cache line size and L1/L2 cache sizes from sysctl
    FILE *cmd;
    char buffer[128];

    // Get cache line size
    cmd = popen("sysctl -n hw.cachelinesize 2>/dev/null", "r");
    if (cmd && fgets(buffer, sizeof(buffer), cmd))
    {
        hardware.cache_line_size = atoi(buffer);
    }
    if (cmd)
        pclose(cmd);

    // Get L1 data cache size
    cmd = popen("sysctl -n hw.l1dcachesize 2>/dev/null", "r");
    if (cmd && fgets(buffer, sizeof(buffer), cmd))
    {
        hardware.l1_cache_size = atoi(buffer);
    }
    if (cmd)
        pclose(cmd);

    // Get L2 cache size
    cmd = popen("sysctl -n hw.l2cachesize 2>/dev/null", "r");
    if (cmd && fgets(buffer, sizeof(buffer), cmd))
    {
        hardware.l2_cache_size = atoi(buffer);
    }
    if (cmd)
        pclose(cmd);

    // Get L3 cache size
    cmd = popen("sysctl -n hw.l3cachesize 2>/dev/null", "r");
    if (cmd && fgets(buffer, sizeof(buffer), cmd))
    {
        hardware.l3_cache_size = atoi(buffer);
        if (hardware.l3_cache_size == 0)
        {
            // Some systems return 0 if L3 doesn't exist
            hardware.l3_cache_size = 8388608; // Default to 8MB
        }
    }
    if (cmd)
        pclose(cmd);

    // Get CPU model
    cmd = popen("sysctl -n machdep.cpu.brand_string 2>/dev/null", "r");
    if (cmd && fgets(buffer, sizeof(buffer), cmd))
    {
        strncpy(hardware.cpu_model, buffer, sizeof(hardware.cpu_model) - 1);
        hardware.cpu_model[sizeof(hardware.cpu_model) - 1] = '\0';

        // Remove trailing newline
        size_t len = strlen(hardware.cpu_model);
        if (len > 0 && hardware.cpu_model[len - 1] == '\n')
        {
            hardware.cpu_model[len - 1] = '\0';
        }
    }
    if (cmd)
        pclose(cmd);
#endif

#elif defined(__linux__)
#ifdef __ARM_NEON
    hardware.has_neon = true;
    hardware.has_simd = true;
#endif

    // On Linux, try to parse /proc/cpuinfo for cache details
    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo)
    {
        char line[256];
        while (fgets(line, sizeof(line), cpuinfo))
        {
            if (strstr(line, "model name"))
            {
                char *model = strchr(line, ':');
                if (model)
                {
                    model++; // Skip past the colon
                    while (*model == ' ' || *model == '\t')
                        model++; // Skip whitespace
                    strncpy(hardware.cpu_model, model, sizeof(hardware.cpu_model) - 1);
                    hardware.cpu_model[sizeof(hardware.cpu_model) - 1] = '\0';

                    // Remove trailing newline
                    size_t len = strlen(hardware.cpu_model);
                    if (len > 0 && hardware.cpu_model[len - 1] == '\n')
                    {
                        hardware.cpu_model[len - 1] = '\0';
                    }
                    break;
                }
            }
        }
        fclose(cpuinfo);
    }

    // Try to read cache information from sysfs
    FILE *cache_info;
    char file_path[256];
    char buffer[64];

    // For L1 cache
    snprintf(file_path, sizeof(file_path), "/sys/devices/system/cpu/cpu0/cache/index0/size");
    cache_info = fopen(file_path, "r");
    if (cache_info)
    {
        if (fgets(buffer, sizeof(buffer), cache_info))
        {
            int size;
            char unit;
            if (sscanf(buffer, "%d%c", &size, &unit) == 2)
            {
                if (unit == 'K' || unit == 'k')
                {
                    hardware.l1_cache_size = size * 1024;
                }
                else if (unit == 'M' || unit == 'm')
                {
                    hardware.l1_cache_size = size * 1024 * 1024;
                }
            }
        }
        fclose(cache_info);
    }

    // For L2 cache
    snprintf(file_path, sizeof(file_path), "/sys/devices/system/cpu/cpu0/cache/index1/size");
    cache_info = fopen(file_path, "r");
    if (cache_info)
    {
        if (fgets(buffer, sizeof(buffer), cache_info))
        {
            int size;
            char unit;
            if (sscanf(buffer, "%d%c", &size, &unit) == 2)
            {
                if (unit == 'K' || unit == 'k')
                {
                    hardware.l2_cache_size = size * 1024;
                }
                else if (unit == 'M' || unit == 'm')
                {
                    hardware.l2_cache_size = size * 1024 * 1024;
                }
            }
        }
        fclose(cache_info);
    }

    // For cache line size
    snprintf(file_path, sizeof(file_path), "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
    cache_info = fopen(file_path, "r");
    if (cache_info)
    {
        if (fgets(buffer, sizeof(buffer), cache_info))
        {
            hardware.cache_line_size = atoi(buffer);
        }
        fclose(cache_info);
    }
#endif

    vsort_log_info("Detected hardware: %s with %d cores (%d P-cores, %d E-cores)",
                   hardware.cpu_model, hardware.total_cores,
                   hardware.performance_cores, hardware.efficiency_cores);
    vsort_log_debug("Cache: L1=%dKB, L2=%dKB, L3=%dKB, Line=%d bytes, SIMD=%d bytes%s%s",
                    hardware.l1_cache_size / 1024, hardware.l2_cache_size / 1024,
                    hardware.l3_cache_size / 1024, hardware.cache_line_size, hardware.simd_width,
                    hardware.has_simd ? ", SIMD" : "",
                    hardware.has_neon ? ", NEON" : "");
}

/**
 * Calibrate thresholds by running quick tests on sample arrays
 */
static void vsort_calibrate_thresholds(void)
{
    // For more complex calibration, we'd run benchmarks on different sized arrays
    // and adjust thresholds dynamically. This is a simpler approach that uses
    // hardware characteristics to make educated guesses.

    // Calculate best insertion sort threshold based on cache line size
    // One cache line is typically 64 bytes = 16 integers
    int insertion_threshold = hardware.cache_line_size / sizeof(int);

    // Clamp to reasonable range
    if (insertion_threshold < 8)
        insertion_threshold = 8;
    if (insertion_threshold > 32)
        insertion_threshold = 32;
    thresholds.insertion_threshold = insertion_threshold;

    // Vector threshold depends on SIMD width and whether NEON is available
    if (hardware.has_neon)
    {
        // For NEON on Apple Silicon, use wider vectors
        thresholds.vector_threshold = MAX(32, hardware.simd_width / sizeof(int) * 8);
    }
    else if (hardware.has_simd)
    {
        // For other SIMD, use more conservative threshold
        thresholds.vector_threshold = MAX(16, hardware.simd_width / sizeof(int) * 4);
    }
    else
    {
        // No SIMD, less benefit from vectorization
        thresholds.vector_threshold = 64;
    }

    // Calculate parallel threshold based on core count and L3 cache
    if (hardware.total_cores >= 8)
    {
        // Many cores, start parallelizing sooner
        thresholds.parallel_threshold = MIN(50000, hardware.l3_cache_size / sizeof(int) / 4);
    }
    else if (hardware.total_cores >= 4)
    {
        // Moderate core count
        thresholds.parallel_threshold = MIN(75000, hardware.l3_cache_size / sizeof(int) / 3);
    }
    else
    {
        // Few cores, delay parallelization until larger arrays
        thresholds.parallel_threshold = MIN(100000, hardware.l3_cache_size / sizeof(int) / 2);
    }

    // Elements that fit in L1 cache
    thresholds.cache_optimal_elements = hardware.l1_cache_size / sizeof(int) / 2;

    // Radix sort threshold - faster for large arrays
    thresholds.radix_threshold = MAX(500000, hardware.l3_cache_size / sizeof(int) * 2);

    vsort_log_info("Calibrated thresholds - insertion: %d, vector: %d, parallel: %d, radix: %d, cache_optimal: %d",
                   thresholds.insertion_threshold, thresholds.vector_threshold,
                   thresholds.parallel_threshold, thresholds.radix_threshold,
                   thresholds.cache_optimal_elements);
}

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

    // Detect hardware characteristics first
    vsort_detect_hardware_characteristics();

    // Then calibrate thresholds based on those characteristics
    vsort_calibrate_thresholds();

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

    // Sort chunks using P/E core distribution if we have both core types
    if (hardware.performance_cores > 0 && hardware.efficiency_cores > 0)
    {
        vsort_log_info("Using P/E core optimization with %d P-cores and %d E-cores",
                       hardware.performance_cores, hardware.efficiency_cores);
        distribute_work_by_core_type(arr, size, chunk_size, num_chunks);
    }
    else
    {
        // Fall back to standard parallel implementation
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
        dispatch_release(group);
        dispatch_release(queue);
    }

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

#else
    // Fall back to sequential sort on non-Apple platforms
    vsort_log_info("Parallel sorting not available, using sequential sort");
    vsort_sequential(arr, size);
#endif
}

#if defined(__APPLE__) && defined(__arm64__)
/**
 * Creates a high-priority dispatch queue targeting performance cores
 */
static dispatch_queue_t create_p_core_queue(void)
{
    dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(
        DISPATCH_QUEUE_CONCURRENT, QOS_CLASS_USER_INITIATED, 0);
    return dispatch_queue_create("com.vsort.p_cores", attr);
}

/**
 * Creates a lower-priority dispatch queue targeting efficiency cores
 */
static dispatch_queue_t create_e_core_queue(void)
{
    dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(
        DISPATCH_QUEUE_CONCURRENT, QOS_CLASS_UTILITY, 0);
    return dispatch_queue_create("com.vsort.e_cores", attr);
}

/**
 * Analyzes a chunk's complexity to determine if it should be processed on P or E cores
 * Higher complexity (more disorder) -> P cores
 * Lower complexity (more ordered) -> E cores
 */
static bool is_complex_chunk(int arr[], int start, int end)
{
    // Sample a few elements to estimate chunk complexity
    const int sample_size = MIN(20, (end - start + 1) / 4);
    if (sample_size <= 1)
        return false;

    int inversions = 0;
    int step = MAX(1, (end - start + 1) / sample_size);

    for (int i = start; i < end - step; i += step)
    {
        if (arr[i] > arr[i + step])
        {
            inversions++;
        }
    }

    // Return true if more than 30% of sampled pairs are out of order
    return (inversions > (sample_size * 0.3));
}

/**
 * Distributes sorting work across P-cores and E-cores based on chunk complexity
 */
static void distribute_work_by_core_type(int arr[], int size, int chunk_size, int num_chunks)
{
    dispatch_queue_t p_core_queue = create_p_core_queue();
    dispatch_queue_t e_core_queue = create_e_core_queue();
    dispatch_group_t group = dispatch_group_create();

    vsort_log_debug("Distributing %d chunks between P-cores and E-cores", num_chunks);

    // Distribute chunks based on complexity
    for (int i = 0; i < num_chunks; i++)
    {
        int start = i * chunk_size;
        int end = MIN(start + chunk_size - 1, size - 1);

        // Get reference to arr and bounds for the block
        int *arr_ref = arr;
        int chunk_start = start;
        int chunk_end = end;

        // Analyze chunk complexity to determine which core type to use
        bool is_complex = is_complex_chunk(arr, start, end);
        dispatch_queue_t target_queue = is_complex ? p_core_queue : e_core_queue;

        dispatch_group_async(group, target_queue, ^{
          // Use standard quicksort for each chunk
          quicksort(arr_ref, chunk_start, chunk_end);
        });
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    // Clean up
    dispatch_release(group);
    dispatch_release(p_core_queue);
    dispatch_release(e_core_queue);
}
#endif

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