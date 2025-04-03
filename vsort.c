/**
 * VSort: High-Performance Sorting Algorithm for Apple Silicon
 *
 * A sorting library specifically optimized for Apple Silicon processors,
 * leveraging ARM NEON vector instructions and Grand Central Dispatch.
 *
 * Version 0.4.0
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
#include <math.h>   // For fabsf in float comparison, log2
#include <limits.h> // For UINT_MAX
#include <float.h>  // For FLT_MAX, DBL_MAX etc.

#include "vsort.h"
#include "vsort_logger.h"

// Platform-specific includes and definitions
#if defined(_WIN32) || defined(_MSC_VER)
// Windows-specific headers
#include <windows.h>
// Define POSIX sysconf constants for Windows compatibility (return 0 or reasonable default)
#define _SC_NPROCESSORS_ONLN 1 // Placeholder
#define _SC_NPROCESSORS_CONF 1 // Placeholder
static long sysconf(int name)
{
    if (name == _SC_NPROCESSORS_ONLN || name == _SC_NPROCESSORS_CONF)
    {
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return (long)sysinfo.dwNumberOfProcessors;
    }
    return -1; // Indicate error or unsupported parameter
}
// Windows sleep function (milliseconds)
#define sleep(x) Sleep((x) * 1000)
#else
// Unix/POSIX headers
#include <unistd.h> // For sysconf
#include <errno.h>  // For errno with posix_memalign
#if defined(VSORT_APPLE)
#include <sys/sysctl.h> // For sysctlbyname
#include <dispatch/dispatch.h>
#endif // VSORT_APPLE
// Define _POSIX_C_SOURCE if not already defined, needed for posix_memalign declaration
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#include <stdlib.h> // Ensure stdlib is included after _POSIX_C_SOURCE define
#endif              // _WIN32 || _MSC_VER

// Memory alignment for SIMD operations
#define VSORT_ALIGN 16 // 128-bit (16-byte) alignment for NEON
#define VSORT_ALIGNED __attribute__((aligned(VSORT_ALIGN)))

// Helper macro for min/max (Renamed to avoid conflicts)
#define VSORT_MIN(a, b) ((a) < (b) ? (a) : (b))
#define VSORT_MAX(a, b) ((a) > (b) ? (a) : (b))

// Mark variables as intentionally unused
#define UNUSED(x) ((void)(x))

// Apple Silicon specific includes
#if defined(VSORT_APPLE) && defined(__arm64__)
#include <arm_neon.h> // For NEON SIMD operations (definitions)
#define HAS_NEON_SUPPORT 1
// For __builtin_clz
#if defined(__clang__) || defined(__GNUC__)
#include <limits.h> // For CHAR_BIT
#else
// Define fallback or error if __builtin_clz is not available
#define __builtin_clz(x) vsort_fallback_clz(x) // Requires implementing vsort_fallback_clz
static inline int vsort_fallback_clz(unsigned int n)
{
    if (n == 0)
        return sizeof(unsigned int) * 8;
    int count = 0;
    unsigned int mask = 1U << (sizeof(unsigned int) * 8 - 1);
    while ((n & mask) == 0)
    {
        count++;
        mask >>= 1;
    }
    return count;
}
#endif                                         // __clang__ || __GNUC__
#else                                          // Not Apple Silicon arm64
#define HAS_NEON_SUPPORT 0
// Define fallback for __builtin_clz if needed on other platforms
#if !(defined(__clang__) || defined(__GNUC__))
#define __builtin_clz(x) vsort_fallback_clz(x)
static inline int vsort_fallback_clz(unsigned int n)
{
    if (n == 0)
        return sizeof(unsigned int) * 8;
    int count = 0;
    unsigned int mask = 1U << (sizeof(unsigned int) * 8 - 1);
    while ((n & mask) == 0)
    {
        count++;
        mask >>= 1;
    }
    return count;
}
#elif !defined(__builtin_clz) // If compiler defines it but not via includes
// Assume compiler provides it intrinsically
#endif                        // !(defined(__clang__) || defined(__GNUC__))
#endif                        // VSORT_APPLE && __arm64__

// --- Global Configuration and State ---

/**
 * Dynamic threshold determination based on CPU characteristics
 */
typedef struct
{
    int insertion_threshold;    // Switch to insertion sort below this
    int vector_threshold;       // Switch to vectorized operations above this (NEEDS IMPLEMENTATION)
    int parallel_threshold;     // Switch to parallel operations above this
    int radix_threshold;        // Switch to radix sort above this (for integers)
    int cache_optimal_elements; // Approx elements fitting in L1/L2 cache
} vsort_thresholds_t;

// Global thresholds - initialized by vsort_init_thresholds()
// Defaults are placeholders, calibration is key.
static vsort_thresholds_t thresholds = {
    .insertion_threshold = 16,
    .vector_threshold = 64,
    .parallel_threshold = 100000,
    .radix_threshold = 1000000,
    .cache_optimal_elements = 16384};

// Hardware characteristics structure
typedef struct
{
    int l1_cache_size;     // L1 data cache size in bytes
    int l2_cache_size;     // L2 cache size in bytes
    int l3_cache_size;     // L3 cache size in bytes (if available)
    int cache_line_size;   // Cache line size in bytes
    int simd_width;        // SIMD register width in bytes (e.g., 16 for NEON)
    int performance_cores; // Number of performance cores
    int efficiency_cores;  // Number of efficiency cores
    int total_cores;       // Total number of physical CPU cores
    bool has_simd;         // SIMD support flag (generic)
    bool has_neon;         // ARM NEON support flag
    char cpu_model[128];   // CPU model string
} vsort_hardware_t;

// Global hardware characteristics - initialized by vsort_detect_hardware_characteristics()
static vsort_hardware_t hardware = {
    .l1_cache_size = 32768,   // Default: 32KB L1d cache
    .l2_cache_size = 2097152, // Default: 2MB L2 cache
    .l3_cache_size = 0,       // Default: 0MB L3 cache (often shared, detection varies)
    .cache_line_size = 64,    // Default: 64-byte cache lines
    .simd_width = 0,          // Default: Unknown
    .performance_cores = 1,   // Default: Assume at least 1 core
    .efficiency_cores = 0,    // Default: Assume no E-cores initially
    .total_cores = 1,         // Default: Assume at least 1 core
    .has_simd = false,
    .has_neon = false,
    .cpu_model = "Unknown"};

// Initialization flag
static bool vsort_library_initialized = false;

// --- Forward Declarations ---

// Core Initialization
static void vsort_init_thresholds(void);
static void vsort_detect_hardware_characteristics(void);
static void vsort_calibrate_thresholds(void);

// Memory Management
static void *vsort_aligned_malloc(size_t size);
static void vsort_aligned_free(void *ptr);

// Sorting Algorithms (Internal Implementations)
static void swap_int(int *a, int *b);
static void swap_float(float *a, float *b);
static void insertion_sort_int(int arr[], int low, int high);
static void insertion_sort_float(float arr[], int low, int high);
static int partition_int(int arr[], int low, int high);
static int partition_float(float arr[], int low, int high);
static void quicksort_int(int arr[], int low, int high);
static void quicksort_float(float arr[], int low, int high);
static void radix_sort_int(int arr[], int n); // Radix sort primarily for integers
static void merge_sorted_arrays_int(int arr[], int temp[], int left, int mid, int right);
static void merge_sorted_arrays_float(float arr[], float temp[], int left, int mid, int right);
static bool vsort_is_nearly_sorted_int(const int *arr, int size);
static bool vsort_is_nearly_sorted_float(const float *arr, int size);

// Parallelism (GCD specific for Apple)
#if defined(VSORT_APPLE) && defined(__arm64__)
static void vsort_parallel_int(int *arr, int size);
static void vsort_parallel_float(float *arr, int size);
#endif // VSORT_APPLE && __arm64__

// Sequential Driver
static void vsort_sequential_int(int *arr, int size);
static void vsort_sequential_float(float *arr, int size);

// Utility
static int get_physical_core_count(void);
static int default_char_comparator(const void *a, const void *b); // Keep for vsort_char fallback

// --- Hardware Detection & Calibration ---

/**
 * Gets a system value using sysctlbyname (macOS specific)
 */
#if defined(VSORT_APPLE)
static bool get_sysctl_value(const char *name, void *value, size_t *size)
{
    if (sysctlbyname(name, value, size, NULL, 0) == -1)
    {
        // Use logger only if initialized, otherwise risk recursion during init
        // vsort_log_warning("sysctlbyname failed for %s: %s", name, strerror(errno));
        return false;
    }
    return true;
}
#endif // VSORT_APPLE

/**
 * Detect hardware characteristics to optimize thresholds
 * IMPROVED: Uses sysctlbyname on Apple platforms, reducing popen overhead.
 */
static void vsort_detect_hardware_characteristics(void)
{
    // Get total number of physical cores
    hardware.total_cores = get_physical_core_count();
    if (hardware.total_cores <= 0)
        hardware.total_cores = 1; // Ensure at least 1 core

#if defined(VSORT_APPLE) && defined(__arm64__)
    // --- Apple Silicon Specific Detection ---
    hardware.has_neon = true; // Assume NEON on arm64 Apple platforms
    hardware.has_simd = true;
    hardware.simd_width = 16; // NEON is 128-bit (16 bytes)

    size_t size;
    int p_cores = 0;
    int e_cores = 0;

    // Get P-cores count
    size = sizeof(p_cores);
    // Use hw.perflevel0.physicalcpu for physical cores if available, fallback to logicalcpu
    if (!get_sysctl_value("hw.perflevel0.physicalcpu", &p_cores, &size) || p_cores <= 0)
    {
        size = sizeof(p_cores);                                        // Reset size
        get_sysctl_value("hw.perflevel0.logicalcpu", &p_cores, &size); // Fallback to logical
    }
    hardware.performance_cores = p_cores > 0 ? p_cores : 0;

    // Get E-cores count
    size = sizeof(e_cores);
    if (!get_sysctl_value("hw.perflevel1.physicalcpu", &e_cores, &size) || e_cores <= 0)
    {
        size = sizeof(e_cores);                                        // Reset size
        get_sysctl_value("hw.perflevel1.logicalcpu", &e_cores, &size); // Fallback to logical
    }
    hardware.efficiency_cores = e_cores > 0 ? e_cores : 0;

    // If detection failed or seems inconsistent, use total cores as a basis
    if (hardware.performance_cores <= 0 && hardware.efficiency_cores <= 0 && hardware.total_cores > 0)
    {
        // Simple split if P/E core detection failed entirely
        hardware.performance_cores = hardware.total_cores / 2 > 0 ? hardware.total_cores / 2 : 1;
        hardware.efficiency_cores = hardware.total_cores - hardware.performance_cores;
    }
    else if (hardware.performance_cores <= 0 && hardware.efficiency_cores > 0)
    {
        hardware.performance_cores = hardware.total_cores - hardware.efficiency_cores;
    }
    else if (hardware.efficiency_cores <= 0 && hardware.performance_cores > 0)
    {
        hardware.efficiency_cores = hardware.total_cores - hardware.performance_cores;
    }
    else if (hardware.performance_cores > 0 && hardware.efficiency_cores > 0 && (hardware.performance_cores + hardware.efficiency_cores != hardware.total_cores))
    {
        // If counts don't add up (e.g., only got logical counts which might differ)
        // Trust total_cores and adjust E-cores based on detected P-cores
        hardware.efficiency_cores = hardware.total_cores - hardware.performance_cores;
    }
    // Ensure non-negative counts
    if (hardware.performance_cores < 0)
        hardware.performance_cores = 0;
    if (hardware.efficiency_cores < 0)
        hardware.efficiency_cores = 0;
    // Ensure at least one core is assigned if total > 0
    if (hardware.total_cores > 0 && hardware.performance_cores == 0 && hardware.efficiency_cores == 0)
    {
        hardware.performance_cores = hardware.total_cores;
    }

    // Get cache info
    size = sizeof(hardware.cache_line_size);
    if (!get_sysctl_value("hw.cachelinesize", &hardware.cache_line_size, &size))
        hardware.cache_line_size = 64; // Default

    size = sizeof(hardware.l1_cache_size);
    if (!get_sysctl_value("hw.l1dcachesize", &hardware.l1_cache_size, &size)) // L1 Data Cache
        hardware.l1_cache_size = 32768;                                       // Default

    size = sizeof(hardware.l2_cache_size);
    if (!get_sysctl_value("hw.l2cachesize", &hardware.l2_cache_size, &size))
        hardware.l2_cache_size = 2097152; // Default

    size = sizeof(hardware.l3_cache_size);
    // L3 might not exist or sysctl might fail, default to 0
    if (!get_sysctl_value("hw.l3cachesize", &hardware.l3_cache_size, &size))
        hardware.l3_cache_size = 0;

    // Get CPU model
    size = sizeof(hardware.cpu_model);
    if (!get_sysctl_value("machdep.cpu.brand_string", hardware.cpu_model, &size))
        strncpy(hardware.cpu_model, "Apple Silicon (Unknown)", sizeof(hardware.cpu_model) - 1);

    // Ensure null termination
    hardware.cpu_model[sizeof(hardware.cpu_model) - 1] = '\0';
    // Remove potential trailing newline
    char *newline = strchr(hardware.cpu_model, '\n');
    if (newline)
        *newline = '\0';

#elif defined(VSORT_LINUX)
    // --- Linux Specific Detection (using /sys/, simplified) ---
    // NOTE: This is a basic implementation. Robust parsing of /proc/cpuinfo
    // and /sys/devices/system/cpu/* is complex.
    hardware.performance_cores = hardware.total_cores; // Assume all cores are P-cores on Linux for simplicity
    hardware.efficiency_cores = 0;
    hardware.simd_width = 0; // Default unless NEON detected

#ifdef __ARM_NEON // Check if compiler defines NEON support
    hardware.has_neon = true;
    hardware.has_simd = true;
    hardware.simd_width = 16; // NEON is 128-bit
#endif

    // Basic cache info reading from sysfs (example for cpu0)
    FILE *f;
    char path[256];
    int val;

    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
    if ((f = fopen(path, "r")) && fscanf(f, "%d", &val) == 1)
        hardware.cache_line_size = val;
    else
        hardware.cache_line_size = 64;
    if (f)
        fclose(f);

    // L1d cache index can vary (often index1 for data cache) - check common indices
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index1/size");
    if ((f = fopen(path, "r")) && fscanf(f, "%dK", &val) == 1)
        hardware.l1_cache_size = val * 1024;
    else
    {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index0/size"); // Fallback check index 0
        if ((f = fopen(path, "r")) && fscanf(f, "%dK", &val) == 1)
            hardware.l1_cache_size = val * 1024;
        else
            hardware.l1_cache_size = 32768;
        if (f)
            fclose(f);
    }
    if (f)
        fclose(f);

    // L2 cache index can vary (often index2 or index3)
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index2/size");
    if ((f = fopen(path, "r")) && fscanf(f, "%dK", &val) == 1)
        hardware.l2_cache_size = val * 1024;
    else
    {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index3/size"); // Fallback check index 3
        if ((f = fopen(path, "r")) && fscanf(f, "%dK", &val) == 1)
            hardware.l2_cache_size = val * 1024;
        else
            hardware.l2_cache_size = 2097152;
        if (f)
            fclose(f);
    }
    if (f)
        fclose(f);

    // L3 detection is more complex (shared cache) - skipping for brevity

    // CPU Model from /proc/cpuinfo
    if ((f = fopen("/proc/cpuinfo", "r")))
    {
        char line[256];
        while (fgets(line, sizeof(line), f))
        {
            if (strncmp(line, "model name", 10) == 0 || strncmp(line, "Processor", 9) == 0)
            { // Accept "Processor" too
                char *colon = strchr(line, ':');
                if (colon)
                {
                    char *model = colon + 1;
                    while (*model == ' ' || *model == '\t')
                        model++; // Skip whitespace
                    strncpy(hardware.cpu_model, model, sizeof(hardware.cpu_model) - 1);
                    hardware.cpu_model[sizeof(hardware.cpu_model) - 1] = '\0';
                    char *newline = strchr(hardware.cpu_model, '\n');
                    if (newline)
                        *newline = '\0';
                    break; // Found model name
                }
            }
        }
        fclose(f);
    }
    else
    {
        strncpy(hardware.cpu_model, "Linux CPU (Unknown)", sizeof(hardware.cpu_model) - 1);
        hardware.cpu_model[sizeof(hardware.cpu_model) - 1] = '\0';
    }

#else
    // --- Default/Fallback for other platforms ---
    hardware.performance_cores = hardware.total_cores;
    hardware.efficiency_cores = 0;
    // Cannot assume NEON or specific SIMD width
    hardware.has_neon = false;
    hardware.has_simd = false; // Could add checks for SSE/AVX here if needed
    hardware.simd_width = 0;
    strncpy(hardware.cpu_model, "Generic CPU", sizeof(hardware.cpu_model) - 1);
    hardware.cpu_model[sizeof(hardware.cpu_model) - 1] = '\0';
#endif

    // Log detected characteristics (only if logger is initialized)
    if (vsort_library_initialized)
    {
        vsort_log_info("Detected hardware: %s", hardware.cpu_model);
        vsort_log_info("Cores: %d total (%d P-core(s), %d E-core(s))",
                       hardware.total_cores, hardware.performance_cores, hardware.efficiency_cores);
        vsort_log_debug("Cache: L1d=%dKB, L2=%dKB, L3=%dKB, Line=%d bytes",
                        hardware.l1_cache_size / 1024, hardware.l2_cache_size / 1024,
                        hardware.l3_cache_size / 1024, hardware.cache_line_size);
        vsort_log_debug("SIMD: %s%s (Width: %d bytes)",
                        hardware.has_simd ? "Yes" : "No",
                        hardware.has_neon ? " (NEON)" : "", hardware.simd_width);
    }
}

/**
 * Calibrate thresholds based on detected hardware characteristics.
 */
static void vsort_calibrate_thresholds(void)
{
    // Insertion Sort Threshold: Related to cache line size.
    thresholds.insertion_threshold = VSORT_MAX(8, VSORT_MIN(32, hardware.cache_line_size > 0 ? hardware.cache_line_size / (int)sizeof(int) : 16));

    // Vector Threshold: Minimum size where vectorization (NEON) might be beneficial.
    if (hardware.has_neon && hardware.simd_width > 0)
    {
        thresholds.vector_threshold = VSORT_MAX(32, (hardware.simd_width / (int)sizeof(int)) * 4); // e.g., 4*4=16 for NEON
    }
    else
    {
        thresholds.vector_threshold = 64; // Higher threshold if no SIMD
    }

    // Parallel Threshold: Size where parallel overhead is likely overcome.
    int effective_cores = hardware.performance_cores > 0 ? hardware.performance_cores : hardware.total_cores;
    if (effective_cores <= 0)
        effective_cores = 1; // Ensure at least 1

    if (effective_cores >= 8)
    {
        thresholds.parallel_threshold = 50000;
    }
    else if (effective_cores >= 4)
    {
        thresholds.parallel_threshold = 75000;
    }
    else
    {
        thresholds.parallel_threshold = 150000;
    }
    // Refine based on L2 cache size per core (approx)
    if (hardware.l2_cache_size > 0)
    {
        int cache_per_core = hardware.l2_cache_size / effective_cores;
        thresholds.parallel_threshold = VSORT_MAX(10000, VSORT_MIN(thresholds.parallel_threshold, cache_per_core > 0 ? cache_per_core / (int)sizeof(int) * 4 : thresholds.parallel_threshold));
    }
    else
    {
        // Fallback if L2 size unknown
        thresholds.parallel_threshold = VSORT_MAX(10000, thresholds.parallel_threshold);
    }

    // Radix Sort Threshold (Integers only): Size where radix sort typically outperforms quicksort.
    if (hardware.l2_cache_size > 0)
    {
        thresholds.radix_threshold = VSORT_MAX(100000, hardware.l2_cache_size / (int)sizeof(int));
    }
    else
    {
        thresholds.radix_threshold = 500000; // Fallback
    }

    // Cache Optimal Elements: Approx number of elements fitting in L1 data cache.
    if (hardware.l1_cache_size > 0)
    {
        thresholds.cache_optimal_elements = hardware.l1_cache_size / sizeof(int);
    }
    else
    {
        thresholds.cache_optimal_elements = 8192; // Fallback (e.g., 32KB / 4 bytes)
    }

    // Log calibrated thresholds (only if logger is initialized)
    if (vsort_library_initialized)
    {
        vsort_log_info("Calibrated thresholds - insertion: %d, vector: %d (NEEDS IMPL), parallel: %d, radix: %d, cache_optimal: %d",
                       thresholds.insertion_threshold, thresholds.vector_threshold,
                       thresholds.parallel_threshold, thresholds.radix_threshold,
                       thresholds.cache_optimal_elements);
    }
}

/**
 * Initialize thresholds based on system characteristics. Basic thread-safety for flag.
 */
static void vsort_init_thresholds(void)
{
    // Use a simple static flag for basic thread safety during initialization.
    // Assumes multiple threads calling init simultaneously is acceptable,
    // as the detection/calibration results should be idempotent.
    // For guaranteed single execution, a mutex or dispatch_once is needed.
    // static dispatch_once_t init_once_token = 0; // Requires <dispatch/dispatch.h>
    // dispatch_once(&init_once_token, ^{ ... });

    if (vsort_library_initialized)
    {
        return;
    }

    // Initialize logger first (assuming it's thread-safe or called before threads)
    // Ensure logger init doesn't call back into vsort_init
    vsort_log_init(VSORT_LOG_WARNING); // Set default log level

    // Detect hardware characteristics
    vsort_detect_hardware_characteristics();

    // Calibrate thresholds based on detected hardware
    vsort_calibrate_thresholds();

    // Log final detected/calibrated info *after* calibration
    vsort_log_info("VSort library initialized.");
    vsort_log_info("Final Hardware: %s, Cores: %d (%dP/%dE), L1d: %dKB, L2: %dKB, L3: %dKB, Line: %dB, NEON: %s",
                   hardware.cpu_model, hardware.total_cores, hardware.performance_cores, hardware.efficiency_cores,
                   hardware.l1_cache_size / 1024, hardware.l2_cache_size / 1024, hardware.l3_cache_size / 1024,
                   hardware.cache_line_size, hardware.has_neon ? "Yes" : "No");
    vsort_log_info("Final Thresholds - insertion: %d, vector: %d, parallel: %d, radix: %d, cache_optimal: %d",
                   thresholds.insertion_threshold, thresholds.vector_threshold,
                   thresholds.parallel_threshold, thresholds.radix_threshold,
                   thresholds.cache_optimal_elements);

    // Mark as initialized *at the very end*
    vsort_library_initialized = true;
}

// --- Memory Management ---

/**
 * Aligned memory allocation for SIMD operations.
 */
static void *vsort_aligned_malloc(size_t size)
{
    void *ptr = NULL;
    if (size == 0)
    {
        vsort_log_warning("Attempted to allocate 0 bytes.");
        return NULL;
    }

#if defined(_WIN32) || defined(_MSC_VER)
    ptr = _aligned_malloc(size, VSORT_ALIGN);
    if (!ptr)
    {
        vsort_log_error("Failed to allocate %zu aligned bytes (_aligned_malloc).", size);
    }
#else
    // Assume POSIX environment (macOS, Linux) has posix_memalign
    // Remove the compile-time check as it caused issues and the function is standard
    int ret = posix_memalign(&ptr, VSORT_ALIGN, size);
    if (ret != 0)
    {
        vsort_log_error("Failed to allocate %zu aligned bytes (posix_memalign: %s).", size, strerror(ret));
        ptr = NULL;
    }
#endif
    return ptr;
}

/**
 * Free aligned memory.
 */
static void vsort_aligned_free(void *ptr)
{
    if (!ptr)
        return;
#if defined(_WIN32) || defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr); // Standard free works for posix_memalign and malloc fallback
#endif
}

// --- Core Sorting Algorithms ---

// Swap functions
static inline void swap_int(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}
static inline void swap_float(float *a, float *b)
{
    float temp = *a;
    *a = *b;
    *b = temp;
}

// Insertion Sort (Int)
static void insertion_sort_int(int arr[], int low, int high)
{
    for (int i = low + 1; i <= high; i++)
    {
        int key = arr[i];
        int j = i - 1;
        int *pos = &arr[j]; // Pointer to comparison position
        while (j >= low && *pos > key)
        {
            *(pos + 1) = *pos; // Shift element right
            j--;
            pos--;
        }
        *(pos + 1) = key; // Insert key in correct position
    }
}

// Insertion Sort (Float)
static void insertion_sort_float(float arr[], int low, int high)
{
    for (int i = low + 1; i <= high; i++)
    {
        float key = arr[i];
        int j = i - 1;
        float *pos = &arr[j];
        // Move elements greater than key one position ahead
        while (j >= low && *pos > key)
        {
            *(pos + 1) = *pos;
            j--;
            pos--;
        }
        *(pos + 1) = key;
    }
}

// Partition (Int) - Lomuto scheme with median-of-three pivot
static int partition_int(int arr[], int low, int high)
{
    int mid = low + (high - low) / 2;
    // Sort low, mid, high to find median
    if (arr[low] > arr[mid])
        swap_int(&arr[low], &arr[mid]);
    if (arr[mid] > arr[high])
        swap_int(&arr[mid], &arr[high]);
    if (arr[low] > arr[mid])
        swap_int(&arr[low], &arr[mid]); // Ensure low <= mid
    // Median is now at mid, swap it to the end (high) to use as pivot
    swap_int(&arr[mid], &arr[high]);
    int pivot = arr[high];
    int i = low - 1; // Index of smaller element

    // TODO: Potential NEON optimization point (Suggestion 1c)
    // Compare blocks of elements against pivot using NEON.

    for (int j = low; j < high; j++)
    {
        if (arr[j] <= pivot)
        {
            i++;
            if (i != j)
                swap_int(&arr[i], &arr[j]); // Avoid swap if i==j
        }
    }
    swap_int(&arr[i + 1], &arr[high]); // Place pivot in correct spot
    return i + 1;                      // Return pivot index
}

// Partition (Float) - Lomuto scheme with median-of-three pivot
static int partition_float(float arr[], int low, int high)
{
    int mid = low + (high - low) / 2;
    // Sort low, mid, high to find median
    if (arr[low] > arr[mid])
        swap_float(&arr[low], &arr[mid]);
    if (arr[mid] > arr[high])
        swap_float(&arr[mid], &arr[high]);
    if (arr[low] > arr[mid])
        swap_float(&arr[low], &arr[mid]);
    // Median is now at mid, swap it to the end (high) to use as pivot
    swap_float(&arr[mid], &arr[high]);
    float pivot = arr[high];
    int i = low - 1; // Index of smaller element

    // TODO: Potential NEON optimization point for floats
    for (int j = low; j < high; j++)
    {
        if (arr[j] <= pivot)
        { // Handle NaN comparisons carefully if required
            i++;
            if (i != j)
                swap_float(&arr[i], &arr[j]);
        }
    }
    swap_float(&arr[i + 1], &arr[high]); // Place pivot in correct spot
    return i + 1;                        // Return pivot index
}

// Iterative Quicksort (Int)
static void quicksort_int(int arr[], int low, int high)
{
    if (low >= high)
        return; // Base case

    // Use insertion sort for small subarrays
    if (high - low + 1 < thresholds.insertion_threshold)
    {
        insertion_sort_int(arr, low, high);
        return;
    }

    // Estimate stack size needed - log2(N) partitions * 2 ints (low, high)
    // A fixed reasonably large size is often simpler than precise calculation.
    int stack_capacity = 2048; // Stores 1024 range pairs
    int *stack = malloc(stack_capacity * sizeof(int));
    if (!stack)
    {
        vsort_log_error("Memory allocation failed in quicksort_int stack, falling back to insertion sort");
        insertion_sort_int(arr, low, high); // Fallback to simpler sort
        return;
    }

    int top = -1;
    // Push initial task
    stack[++top] = low;
    stack[++top] = high;

    while (top >= 0)
    {
        // Check for stack safety before popping
        if (top < 1)
        { // Need at least low and high
            vsort_log_error("Quicksort stack underflow! Aborting.");
            goto stack_cleanup;
        }

        // Pop task
        high = stack[top--];
        low = stack[top--];

        // Use insertion sort for small subarrays popped from stack
        if (high - low + 1 < thresholds.insertion_threshold)
        {
            insertion_sort_int(arr, low, high);
            continue;
        }

        // Partition the array
        int p = partition_int(arr, low, high);

        // Check for stack capacity before pushing
        if (top >= stack_capacity - 4)
        { // Need space for 2 pairs (4 ints) worst case
            vsort_log_warning("Quicksort stack nearing capacity, potential overflow risk. Falling back for range [%d, %d].", low, high);
            // Fall back to insertion sort for the current range if stack is full
            insertion_sort_int(arr, low, high);
            continue; // Don't push sub-problems
        }

        // Push sub-problems onto stack (larger partition first to minimize stack depth)
        // Right partition: p+1 to high
        if (p + 1 < high)
        {
            stack[++top] = p + 1;
            stack[++top] = high;
        }

        // Left partition: low to p-1
        if (p - 1 > low)
        {
            stack[++top] = low;
            stack[++top] = p - 1;
        }
    } // end while

stack_cleanup:
    free(stack);
}

// Iterative Quicksort (Float)
static void quicksort_float(float arr[], int low, int high)
{
    if (low >= high)
        return; // Base case

    // Use insertion sort for small subarrays
    if (high - low + 1 < thresholds.insertion_threshold)
    {
        insertion_sort_float(arr, low, high);
        return;
    }

    int stack_capacity = 2048; // Stores 1024 range pairs
    int *stack = malloc(stack_capacity * sizeof(int));
    if (!stack)
    {
        vsort_log_error("Memory allocation failed in quicksort_float stack, falling back to insertion sort");
        insertion_sort_float(arr, low, high);
        return;
    }

    int top = -1;
    stack[++top] = low;
    stack[++top] = high;

    while (top >= 0)
    {
        // Check for stack safety before popping
        if (top < 1)
        {
            vsort_log_error("Quicksort stack underflow! Aborting (float).");
            goto stack_cleanup_float;
        }

        high = stack[top--];
        low = stack[top--];

        if (high - low + 1 < thresholds.insertion_threshold)
        {
            insertion_sort_float(arr, low, high);
            continue;
        }

        int p = partition_float(arr, low, high);

        // Check for stack capacity before pushing
        if (top >= stack_capacity - 4)
        {
            vsort_log_warning("Quicksort stack nearing capacity (float), potential overflow risk. Falling back for range [%d, %d].", low, high);
            insertion_sort_float(arr, low, high);
            continue;
        }

        // Push sub-problems in the same order as quicksort_int
        if (p + 1 < high)
        {
            stack[++top] = p + 1;
            stack[++top] = high;
        }

        if (p - 1 > low)
        {
            stack[++top] = low;
            stack[++top] = p - 1;
        }
    }

stack_cleanup_float:
    free(stack);
}

// Radix Sort (LSD for Integers)
static void radix_sort_int(int arr[], int n)
{
    if (n <= 1)
        return;

    // Find min and max to handle range and negative numbers
    int max_val = arr[0];
    int min_val = arr[0];
    for (int i = 1; i < n; i++)
    {
        if (arr[i] > max_val)
            max_val = arr[i];
        if (arr[i] < min_val)
            min_val = arr[i];
    }

    // Shift values to be non-negative for radix sort
    // Use unsigned int for intermediate calculations to avoid overflow with shift
    unsigned int *temp_arr = malloc(n * sizeof(unsigned int));
    if (!temp_arr)
    {
        vsort_log_error("Memory allocation failed in radix_sort_int temp_arr, falling back to quicksort");
        quicksort_int(arr, 0, n - 1);
        return;
    }
    unsigned int max_shifted = 0;
    long long current_val; // Use long long for intermediate addition
    unsigned int shift_amount = 0;

    // Determine shift amount carefully to avoid overflow
    if (min_val < 0)
    {
        // Need to shift by abs(min_val). Check if max_val + abs(min_val) overflows unsigned int
        current_val = (long long)max_val - (long long)min_val; // max_val + abs(min_val)
        if (current_val > UINT_MAX)
        {
            vsort_log_error("Radix sort range exceeds unsigned int capacity after shift, falling back to quicksort.");
            free(temp_arr);
            quicksort_int(arr, 0, n - 1);
            return;
        }
        shift_amount = (unsigned int)(-min_val);
        max_shifted = (unsigned int)current_val;
    }
    else
    {
        shift_amount = 0;
        max_shifted = (unsigned int)max_val;
    }

    // Apply shift
    for (int i = 0; i < n; ++i)
    {
        temp_arr[i] = (unsigned int)arr[i] + shift_amount;
    }

    // Temporary buffer for sorted output during passes
    unsigned int *output = malloc(n * sizeof(unsigned int));
    if (!output)
    {
        vsort_log_error("Memory allocation failed in radix_sort_int output, falling back to quicksort");
        free(temp_arr);
        quicksort_int(arr, 0, n - 1);
        return;
    }

    // Process 8 bits (1 byte) at a time
    const int bits_per_pass = 8;
    const int num_bins = 1 << bits_per_pass; // 256 bins
    const unsigned int mask = num_bins - 1;

    int count[256]; // Count array on stack

    // Determine number of passes needed based on max shifted value
    int passes = 0;
    if (max_shifted > 0)
    {
        // Calculate number of bits needed for max_shifted value using __builtin_clz
        int leading_zeros = __builtin_clz(max_shifted);
        int num_bits = sizeof(unsigned int) * 8 - leading_zeros;
        passes = (num_bits + bits_per_pass - 1) / bits_per_pass; // Ceiling division
    }
    else
    {
        passes = 1; // At least one pass even if all numbers are 0 after shift
    }
    if (passes <= 0)
        passes = 1; // Ensure at least one pass

    // LSD Radix Sort passes
    unsigned int *current_input = temp_arr;
    unsigned int *current_output = output;

    for (int p = 0; p < passes; ++p)
    {
        int bit_shift = p * bits_per_pass;

        // Clear count array
        memset(count, 0, sizeof(count));

        // Count occurrences of each byte value from current_input
        // TODO: Potential NEON optimization point (Suggestion 1d)
        for (int i = 0; i < n; i++)
        {
            unsigned int bin_index = (current_input[i] >> bit_shift) & mask;
            count[bin_index]++;
        }

        // Compute cumulative counts (prefix sum)
        for (int i = 1; i < num_bins; i++)
        {
            count[i] += count[i - 1];
        }

        // Build the output array based on counts
        // Iterate backwards for stability
        for (int i = n - 1; i >= 0; i--)
        {
            unsigned int bin_index = (current_input[i] >> bit_shift) & mask;
            current_output[--count[bin_index]] = current_input[i]; // Place in correct sorted position
        }

        // Swap input and output buffers for the next pass
        unsigned int *swap_temp = current_input;
        current_input = current_output;
        current_output = swap_temp;
    }

    // The final sorted data is in current_input (which might be temp_arr or output)
    // Undo shift and copy back to original int array
    for (int i = 0; i < n; ++i)
    {
        // Subtract shift amount carefully
        arr[i] = (int)(current_input[i] - shift_amount);
    }

    free(temp_arr); // Free original temp buffer
    free(output);   // Free the other buffer used for swapping
}

// Merge Sorted Arrays (Int) - Requires temporary buffer `temp` of size `n`
static void merge_sorted_arrays_int(int arr[], int temp[], int left, int mid, int right)
{
    if (left >= right || mid < left || mid >= right)
        return; // Invalid range or nothing to merge

    int left_size = mid - left + 1;
    int right_size = right - mid;

    // Copy data to temporary buffer using pointers for potentially better cache usage
    int *left_src = &arr[left];
    int *right_src = &arr[mid + 1];
    int *temp_dest_left = &temp[left];
    int *temp_dest_right = &temp[mid + 1];

    memcpy(temp_dest_left, left_src, left_size * sizeof(int));
    memcpy(temp_dest_right, right_src, right_size * sizeof(int));

    int *p_left = temp_dest_left;   // Pointer to current element in left temp part
    int *p_right = temp_dest_right; // Pointer to current element in right temp part
    int *p_dest = &arr[left];       // Pointer to destination in original array

    int *p_left_end = p_left + left_size;    // End marker for left part
    int *p_right_end = p_right + right_size; // End marker for right part

    // TODO: Potential NEON optimization point (Suggestion 1b)
    // Load vectors from p_left and p_right, compare, blend, store to p_dest

    // Merge the temp arrays back into arr[left..right] using pointers
    while (p_left < p_left_end && p_right < p_right_end)
    {
        if (*p_left <= *p_right)
        {
            *p_dest++ = *p_left++;
        }
        else
        {
            *p_dest++ = *p_right++;
        }
    }

    // Copy the remaining elements of left subarray, if any
    size_t remaining_left_count = p_left_end - p_left;
    if (remaining_left_count > 0)
    {
        memcpy(p_dest, p_left, remaining_left_count * sizeof(int));
    }

    // No need to copy remaining elements from the right subarray in temp,
    // as they originated from the correct final positions in arr.
}

// Merge Sorted Arrays (Float) - Requires temporary buffer `temp` of size `n`
static void merge_sorted_arrays_float(float arr[], float temp[], int left, int mid, int right)
{
    if (left >= right || mid < left || mid >= right)
        return; // Invalid range

    int left_size = mid - left + 1;
    int right_size = right - mid;

    // Copy data to temporary buffer using pointers
    float *left_src = &arr[left];
    float *right_src = &arr[mid + 1];
    float *temp_dest_left = &temp[left];
    float *temp_dest_right = &temp[mid + 1];

    memcpy(temp_dest_left, left_src, left_size * sizeof(float));
    memcpy(temp_dest_right, right_src, right_size * sizeof(float));

    float *p_left = temp_dest_left;
    float *p_right = temp_dest_right;
    float *p_dest = &arr[left];
    float *p_left_end = p_left + left_size;
    float *p_right_end = p_right + right_size;

    // TODO: Potential NEON optimization point for floats
    while (p_left < p_left_end && p_right < p_right_end)
    {
        // Handle NaN comparisons if necessary according to desired sort order
        if (*p_left <= *p_right)
        {
            *p_dest++ = *p_left++;
        }
        else
        {
            *p_dest++ = *p_right++;
        }
    }

    // Copy remaining elements of left subarray
    size_t remaining_left_count = p_left_end - p_left;
    if (remaining_left_count > 0)
    {
        memcpy(p_dest, p_left, remaining_left_count * sizeof(float));
    }
}

// Check if Nearly Sorted (Int)
static bool vsort_is_nearly_sorted_int(const int *arr, int size)
{
    if (size < 20)
        return false; // Too small to reliably tell

    int inversions = 0;
    // Sample about 5% of the array, max 100 samples
    int sample_size = VSORT_MIN(100, VSORT_MAX(10, size / 20));
    int step = VSORT_MAX(1, size / sample_size);

    // Limit loop iterations to avoid excessive sampling on huge arrays if step becomes large
    int max_iterations = sample_size;
    int current_iterations = 0;

    for (int i = 0; (i + step) < size && current_iterations < max_iterations; i += step, ++current_iterations)
    {
        if (arr[i] > arr[i + step])
        {
            inversions++;
        }
    }
    if (current_iterations == 0)
        return false; // Avoid division by zero if loop didn't run

    // Consider nearly sorted if inversions are less than 10% of samples checked
    return (inversions * 10 < current_iterations);
}

// Check if Nearly Sorted (Float)
static bool vsort_is_nearly_sorted_float(const float *arr, int size)
{
    if (size < 20)
        return false;

    int inversions = 0;
    int sample_size = VSORT_MIN(100, VSORT_MAX(10, size / 20));
    int step = VSORT_MAX(1, size / sample_size);
    int max_iterations = sample_size;
    int current_iterations = 0;

    for (int i = 0; (i + step) < size && current_iterations < max_iterations; i += step, ++current_iterations)
    {
        // Handle potential NaNs if necessary
        if (arr[i] > arr[i + step])
        {
            inversions++;
        }
    }
    if (current_iterations == 0)
        return false;

    // Consider nearly sorted if inversions are less than 10% of samples
    return (inversions * 10 < current_iterations);
}

// --- Parallel Sorting (Apple GCD) ---

#if defined(VSORT_APPLE) && defined(__arm64__)

// Parallel Sort Driver (Int)
static void vsort_parallel_int(int *arr, int size)
{
    if (!arr || size < thresholds.parallel_threshold)
    {
        vsort_sequential_int(arr, size); // Fallback if below threshold
        return;
    }

    vsort_log_debug("Starting parallel sort (int) with %d elements", size);

    // Determine number of concurrent tasks to aim for
    int num_cores_to_use = hardware.performance_cores > 0 ? hardware.performance_cores : hardware.total_cores;
    if (num_cores_to_use <= 0)
        num_cores_to_use = 1;
    // Allow some oversubscription, but not excessive
    int max_concurrent_tasks = VSORT_MAX(2, num_cores_to_use * 2);

    vsort_log_debug("Targeting %d cores (%d concurrent tasks) for parallelism.", num_cores_to_use, max_concurrent_tasks);

    // Calculate chunk size for initial parallel sort
    int min_chunk_size = VSORT_MAX(thresholds.insertion_threshold * 2, thresholds.cache_optimal_elements);
    int desired_chunk_size = VSORT_MAX(min_chunk_size, size / max_concurrent_tasks);
    int num_chunks = (size + desired_chunk_size - 1) / desired_chunk_size;
    // Ensure num_chunks is reasonable, recalculate chunk_size if needed
    num_chunks = VSORT_MAX(1, VSORT_MIN(num_chunks, size / thresholds.insertion_threshold)); // Avoid tiny chunks
    int chunk_size = (size + num_chunks - 1) / num_chunks;

    vsort_log_debug("Phase 1: Sorting %d chunks of size ~%d in parallel", num_chunks, chunk_size);

    // Allocate temporary buffer for merging ONCE.
    int *temp = vsort_aligned_malloc(size * sizeof(int));
    if (!temp)
    {
        vsort_log_error("Memory allocation failed for temp buffer in parallel sort, falling back to sequential");
        vsort_sequential_int(arr, size);
        return;
    }

    // --- Phase 1: Parallel Sort Chunks ---
    dispatch_queue_t sort_queue = dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0);
    dispatch_group_t sort_group = dispatch_group_create();

    for (int i = 0; i < num_chunks; i++)
    {
        dispatch_group_async(sort_group, sort_queue, ^{
          int start = i * chunk_size;
          int end = VSORT_MIN(start + chunk_size - 1, size - 1);
          if (start <= end)
          {
              // Sort each chunk sequentially using the best method (usually quicksort)
              quicksort_int(arr, start, end);
          }
        });
    }

    // Wait for all chunks to be sorted
    dispatch_group_wait(sort_group, DISPATCH_TIME_FOREVER);
    dispatch_release(sort_group); // Release group

    vsort_log_debug("Parallel chunk sorting complete.");

    // --- Phase 2: Parallel Merge Passes ---
    vsort_log_debug("Phase 2: Starting parallel merge passes.");
    dispatch_queue_t merge_queue = dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0);

    for (int width = chunk_size; width < size; width *= 2)
    {
        vsort_log_debug("Merging chunks with width %d in parallel", width);
        // Calculate the number of merge operations needed for this width
        // Each operation merges two blocks of size `width`.
        size_t num_merges = 0;
        // Correctly calculate the number of pairs to merge
        for (int left = 0; left < size; left += 2 * width)
        {
            int mid = VSORT_MIN(left + width - 1, size - 1);
            int right = VSORT_MIN(left + 2 * width - 1, size - 1);
            if (mid < right)
            { // Only count if there's a right part to merge with
                num_merges++;
            }
        }

        if (num_merges == 0)
            continue; // No merges needed for this width

        vsort_log_debug("Dispatching %zu merge tasks for width %d", num_merges, width);

        // Use dispatch_apply to parallelize the merge operations for the current width
        dispatch_apply(num_merges, merge_queue, ^(size_t i) {
          // Calculate left, mid, right for the i-th merge operation
          // This calculation needs to map the linear index 'i' back to the
          // appropriate 'left' starting position.
          int left = (int)(i * (size_t)(2 * width)); // Calculate starting position based on index 'i'
          int mid = VSORT_MIN(left + width - 1, size - 1);
          int right = VSORT_MIN(left + 2 * width - 1, size - 1);

          // Ensure mid < right before merging, otherwise it's a single block
          // This check should already be implicitly handled by how num_merges was calculated
          if (mid < right)
          {
              // Use the standard merge function (which is internally sequential).
              // TODO: Implement NEON-accelerated merge here (Suggestion 1b)
              merge_sorted_arrays_int(arr, temp, left, mid, right);
          }
          else
          {
              // This case should ideally not be reached if num_merges is calculated correctly
              vsort_log_debug("Skipping merge for index %zu (left=%d, mid=%d, right=%d)", i, left, mid, right);
          }
        });
        // dispatch_apply automatically waits for all tasks to complete before proceeding
        vsort_log_debug("Merge tasks complete for width %d", width);

    } // End width loop

    vsort_log_debug("Parallel merge passes complete.");

    // Clean up temporary buffer
    vsort_aligned_free(temp);

    // Final verification (optional, for debugging)
#ifdef DEBUG_VERIFICATION
    bool fully_sorted = true;
    for (int i = 1; i < size; i++)
    {
        if (arr[i] < arr[i - 1])
        {
            vsort_log_error("Final array validation failed (int) at index %d: %d > %d", i, arr[i - 1], arr[i]);
            fully_sorted = false;
            // break; // Stop at first error
        }
    }
    if (fully_sorted)
    {
        vsort_log_info("Final array validation passed (int).");
    }
    else
    {
        vsort_log_error("FINAL ARRAY IS NOT SORTED (int)!");
    }
#endif
}

// Parallel Sort Driver (Float) - Structure mirrors Int version
static void vsort_parallel_float(float *arr, int size)
{
    if (!arr || size < thresholds.parallel_threshold)
    {
        vsort_sequential_float(arr, size); // Fallback
        return;
    }
    vsort_log_debug("Starting parallel sort (float) with %d elements", size);

    int num_cores_to_use = hardware.performance_cores > 0 ? hardware.performance_cores : hardware.total_cores;
    if (num_cores_to_use <= 0)
        num_cores_to_use = 1;
    int max_concurrent_tasks = VSORT_MAX(2, num_cores_to_use * 2);
    int min_chunk_size = VSORT_MAX(thresholds.insertion_threshold * 2, thresholds.cache_optimal_elements);
    int desired_chunk_size = VSORT_MAX(min_chunk_size, size / max_concurrent_tasks);
    int num_chunks = (size + desired_chunk_size - 1) / desired_chunk_size;
    num_chunks = VSORT_MAX(1, VSORT_MIN(num_chunks, size / thresholds.insertion_threshold));
    int chunk_size = (size + num_chunks - 1) / num_chunks;

    vsort_log_debug("Phase 1: Sorting %d float chunks of size ~%d in parallel", num_chunks, chunk_size);

    float *temp = vsort_aligned_malloc(size * sizeof(float));
    if (!temp)
    {
        vsort_log_error("Memory allocation failed for temp buffer in parallel sort (float), falling back to sequential");
        vsort_sequential_float(arr, size);
        return;
    }

    // --- Phase 1: Parallel Sort Chunks ---
    dispatch_queue_t sort_queue = dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0);
    dispatch_group_t sort_group = dispatch_group_create();

    for (int i = 0; i < num_chunks; i++)
    {
        dispatch_group_async(sort_group, sort_queue, ^{
          int start = i * chunk_size;
          int end = VSORT_MIN(start + chunk_size - 1, size - 1);
          if (start <= end)
          {
              quicksort_float(arr, start, end);
          }
        });
    }
    dispatch_group_wait(sort_group, DISPATCH_TIME_FOREVER);
    dispatch_release(sort_group);

    vsort_log_debug("Parallel chunk sorting complete (float).");

    // --- Phase 2: Parallel Merge Passes ---
    vsort_log_debug("Phase 2: Starting parallel merge passes (float).");
    dispatch_queue_t merge_queue = dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0);

    for (int width = chunk_size; width < size; width *= 2)
    {
        vsort_log_debug("Merging float chunks with width %d in parallel", width);
        size_t num_merges = 0;
        for (int left = 0; left < size; left += 2 * width)
        {
            int mid = VSORT_MIN(left + width - 1, size - 1);
            int right = VSORT_MIN(left + 2 * width - 1, size - 1);
            if (mid < right)
                num_merges++;
        }

        if (num_merges == 0)
            continue;

        vsort_log_debug("Dispatching %zu float merge tasks for width %d", num_merges, width);

        dispatch_apply(num_merges, merge_queue, ^(size_t i) {
          int left = (int)(i * (size_t)(2 * width));
          int mid = VSORT_MIN(left + width - 1, size - 1);
          int right = VSORT_MIN(left + 2 * width - 1, size - 1);

          if (mid < right)
          {
              // TODO: Implement NEON-accelerated merge for floats
              merge_sorted_arrays_float(arr, temp, left, mid, right);
          }
        });
        vsort_log_debug("Merge tasks complete for float width %d", width);
    }
    vsort_log_debug("Parallel merge passes complete (float).");

    vsort_aligned_free(temp);

    // Final verification (optional)
#ifdef DEBUG_VERIFICATION
    bool fully_sorted = true;
    for (int i = 1; i < size; i++)
    {
        if (arr[i] < arr[i - 1])
        {
            // Handle NaN comparisons carefully if needed
            vsort_log_error("Final array validation failed (float) at index %d: %f > %f", i, arr[i - 1], arr[i]);
            fully_sorted = false;
            // break;
        }
    }
    if (fully_sorted)
    {
        vsort_log_info("Final array validation passed (float).");
    }
    else
    {
        vsort_log_error("FINAL ARRAY IS NOT SORTED (float)!");
    }
#endif
}

#endif // VSORT_APPLE && __arm64__

// --- Sequential Sort Drivers ---

// Sequential Sort (Int)
static void vsort_sequential_int(int *arr, int size)
{
    if (!arr || size <= 1)
        return;

    // Use insertion sort if nearly sorted (check only for modest sizes)
    if (size < thresholds.parallel_threshold / 2 && // Avoid check on very large arrays where it's less likely
        size < thresholds.radix_threshold &&        // Avoid if radix sort will be chosen anyway
        vsort_is_nearly_sorted_int(arr, size))
    {
        vsort_log_info("Array (int, size %d) appears nearly sorted, using insertion sort.", size);
        insertion_sort_int(arr, 0, size - 1);
        return;
    }

    // Use radix sort for large integer arrays
    if (size >= thresholds.radix_threshold)
    {
        vsort_log_info("Using radix sort for large int array (size: %d)", size);
        radix_sort_int(arr, size);
        return;
    }

    // Default to quicksort
    quicksort_int(arr, 0, size - 1);
}

// Sequential Sort (Float)
static void vsort_sequential_float(float *arr, int size)
{
    if (!arr || size <= 1)
        return;

    // Use insertion sort if nearly sorted (check only for modest sizes)
    if (size < thresholds.parallel_threshold / 2 &&
        vsort_is_nearly_sorted_float(arr, size))
    {
        vsort_log_info("Array (float, size %d) appears nearly sorted, using insertion sort.", size);
        insertion_sort_float(arr, 0, size - 1);
        return;
    }

    // Radix sort is not typically used for floats. Default to quicksort.
    // TODO: Could implement float-specific radix sort (complex) if needed.
    quicksort_float(arr, 0, size - 1);
}

// --- Utility Functions ---

/**
 * Get physical core count using sysconf.
 */
static int get_physical_core_count(void)
{
    long nprocs = -1;
#ifdef _SC_NPROCESSORS_ONLN
    nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs < 1)
    {
#ifdef _SC_NPROCESSORS_CONF
        nprocs = sysconf(_SC_NPROCESSORS_CONF);
        if (nprocs < 1)
        {
            // Don't log here, might be called before logger init
            return 1; // Fallback to 1 core
        }
#else
        return 1; // Fallback to 1 core
#endif
    }
    return (int)nprocs;
#else
    return 1; // Fallback
#endif
}

// Default char Comparator (used for qsort fallback)
static int default_char_comparator(const void *a, const void *b)
{
    // Compare as unsigned chars for consistent ordering
    return (*(const unsigned char *)a - *(const unsigned char *)b);
}

// --- Public API Implementation ---

VSORT_API void vsort_init(void)
{
    // Ensure initialization happens only once and is thread-safe if needed
    vsort_init_thresholds();
}

VSORT_API void vsort(int arr[], int n)
{
    vsort_init(); // Ensure library is initialized
    if (!arr || n <= 1)
        return;
    vsort_log_debug("Starting vsort (int) for %d elements.", n);

    // Choose strategy based on size and platform capabilities
    if (n < thresholds.parallel_threshold)
    {
        vsort_sequential_int(arr, n);
    }
    else
    {
#if defined(VSORT_APPLE) && defined(__arm64__)
        vsort_parallel_int(arr, n);
#else
        vsort_log_info("Parallel sort not available on this platform, using sequential.");
        vsort_sequential_int(arr, n);
#endif
    }
    vsort_log_debug("vsort (int) completed for %d elements.", n);
}

VSORT_API void vsort_float(float arr[], int n)
{
    vsort_init();
    if (!arr || n <= 1)
        return;
    vsort_log_debug("Starting vsort (float) for %d elements.", n);

    if (n < thresholds.parallel_threshold)
    {
        vsort_sequential_float(arr, n);
    }
    else
    {
#if defined(VSORT_APPLE) && defined(__arm64__)
        vsort_parallel_float(arr, n);
#else
        vsort_log_info("Parallel sort not available on this platform, using sequential.");
        vsort_sequential_float(arr, n);
#endif
    }
    vsort_log_debug("vsort (float) completed for %d elements.", n);
}

VSORT_API void vsort_char(char arr[], int n)
{
    vsort_init();
    if (!arr || n <= 1)
        return;
    vsort_log_debug("Starting vsort (char) for %d elements (using qsort fallback).", n);
    // TODO: Implement optimized char sort based on internal logic (Suggestion 5)
    // Could use radix sort (treating chars as 8-bit ints) or quicksort.
    qsort(arr, n, sizeof(char), default_char_comparator);
    vsort_log_debug("vsort (char) completed for %d elements.", n);
}

VSORT_API void vsort_with_comparator(void *arr, int n, size_t size, int (*compare)(const void *, const void *))
{
    vsort_init();
    if (!arr || n <= 1 || size == 0 || !compare)
        return;
    vsort_log_debug("Starting vsort (generic) for %d elements, size %zu (using qsort fallback).", n, size);
    // TODO: Implement optimized generic sort (Suggestion 5)
    // This would require adapting quicksort/merge to use the comparator
    // and potentially function pointers or macros. It's non-trivial.
    qsort(arr, n, size, compare);
    vsort_log_debug("vsort (generic) completed for %d elements.", n);
}

VSORT_API int get_num_processors(void)
{
    // Don't strictly need init for this, but ensures consistency if called before sort
    if (!vsort_library_initialized)
    {
        return get_physical_core_count(); // Get raw count if not initialized
    }
    return hardware.total_cores;
}