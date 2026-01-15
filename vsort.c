/**
 * VSort: High-Performance Sorting Algorithm (1.0.0)
 *
 * Modernized runtime with adaptive calibration, hybrid sorting strategies,
 * optional parallel execution on Apple Silicon, and an extensible public API.
 */

#if defined(__APPLE__)
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#endif

#if !defined(_WIN32) && !defined(_MSC_VER)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif
#endif

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vsort.h"
#include "vsort_logger.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(_WIN32) || defined(_MSC_VER)
#include <windows.h>
#else
#include <stdatomic.h>
#include <sched.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(VSORT_APPLE)
#include <dispatch/dispatch.h>
#include <sys/sysctl.h>
#endif
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#define VSORT_ALIGN 16
#define VSORT_UNUSED(x) ((void)(x))
#define VSORT_MIN(a, b) ((a) < (b) ? (a) : (b))
#define VSORT_MAX(a, b) ((a) > (b) ? (a) : (b))
#define VSORT_CLAMP(x, lo, hi) (VSORT_MAX((lo), VSORT_MIN((x), (hi))))

static unsigned int vsort_clz32(unsigned int value)
{
#if defined(_MSC_VER)
    unsigned long index;
    if (_BitScanReverse(&index, value))
        return 31u - index;
    return 32u;
#else
    return value ? (unsigned int)__builtin_clz(value) : 32u;
#endif
}

typedef struct
{
    size_t insertion_threshold;
    size_t parallel_threshold;
    size_t radix_threshold;
    size_t sample_size;
    size_t cache_optimal_elements;
} vsort_thresholds_t;

typedef struct
{
    int total_cores;
    int performance_cores;
    int efficiency_cores;
    size_t l1_cache;
    size_t l2_cache;
    size_t l3_cache;
    size_t cache_line;
    int simd_width;
    bool has_neon;
    bool has_simd;
    char cpu_model[128];
} vsort_hardware_t;

typedef struct
{
    size_t int_size;
    int *int_buffer;
    size_t float_size;
    float *float_buffer;
#if defined(_WIN32) || defined(_MSC_VER)
    volatile LONG int_in_use;
    volatile LONG float_in_use;
#else
    atomic_flag int_in_use;
    atomic_flag float_in_use;
#endif
} vsort_merge_pool_t;

typedef struct
{
    vsort_thresholds_t thresholds;
    vsort_hardware_t hardware;
    unsigned int default_flags;
    vsort_log_level_t log_level;
    bool logger_ready;
    vsort_merge_pool_t merge_pool;
} vsort_runtime_t;

static vsort_runtime_t g_runtime = {
    .thresholds = {
        .insertion_threshold = 32,
        .parallel_threshold = 1u << 17,
        .radix_threshold = 1u << 18,
        .sample_size = 96,
        .cache_optimal_elements = 8192},
    .hardware = {
        .total_cores = 1,
        .performance_cores = 1,
        .efficiency_cores = 0,
        .l1_cache = 32768,
        .l2_cache = 2097152,
        .l3_cache = 0,
        .cache_line = 64,
        .simd_width = 0,
        .has_neon = false,
        .has_simd = false,
        .cpu_model = "Generic CPU"},
    .default_flags = VSORT_FLAG_ALLOW_PARALLEL | VSORT_FLAG_ALLOW_RADIX | VSORT_FLAG_PREFER_THROUGHPUT,
    .log_level = VSORT_LOG_WARNING,
    .logger_ready = false,
#if defined(_WIN32) || defined(_MSC_VER)
    .merge_pool = {.int_size = 0, .int_buffer = NULL, .float_size = 0, .float_buffer = NULL, .int_in_use = 0, .float_in_use = 0},
#else
    .merge_pool = {.int_size = 0, .int_buffer = NULL, .float_size = 0, .float_buffer = NULL, .int_in_use = ATOMIC_FLAG_INIT, .float_in_use = ATOMIC_FLAG_INIT},
#endif
};

#if defined(_WIN32) || defined(_MSC_VER)
static INIT_ONCE g_runtime_once = INIT_ONCE_STATIC_INIT;
#else
static atomic_bool g_runtime_init_requested = ATOMIC_VAR_INIT(false);
static atomic_bool g_runtime_ready = ATOMIC_VAR_INIT(false);
#endif

static vsort_runtime_t *vsort_runtime(void);
static void vsort_runtime_initialize(void);
static void vsort_detect_hardware(vsort_runtime_t *rt);
static void vsort_calibrate_thresholds(vsort_runtime_t *rt);
static int vsort_detect_physical_core_count(void);

static void *vsort_aligned_malloc(size_t size);
static void vsort_aligned_free(void *ptr);

static void vsort_counting_sort_char(unsigned char *data, size_t count);

static void vsort_insertion_sort_int32(int *data, size_t count);
static void vsort_insertion_sort_float32(float *data, size_t count);

static void vsort_heapsort_int32(int *data, size_t count);
static void vsort_heapsort_float32(float *data, size_t count);

static void vsort_introsort_int32_impl(int *data, size_t count, size_t depth_limit, unsigned int flags);
static void vsort_introsort_float32_impl(float *data, size_t count, size_t depth_limit);
static void vsort_introsort_int32(int *data, size_t count, unsigned int flags);
static void vsort_introsort_float32(float *data, size_t count);

static bool vsort_mergesort_int32(int *data, size_t count);
static bool vsort_mergesort_float32(float *data, size_t count);
static void vsort_mergesort_int32_impl(int *data, int *buffer, size_t left, size_t right);
static void vsort_mergesort_float32_impl(float *data, float *buffer, size_t left, size_t right);
static void vsort_merge_int32(int *data, int *buffer, size_t left, size_t mid, size_t right);
static void vsort_merge_float32(float *data, float *buffer, size_t left, size_t mid, size_t right);

static bool vsort_radix_sort_int32(int *data, size_t count);

static bool vsort_is_nearly_sorted_int32(const int *data, size_t count, size_t sample_hint);
static bool vsort_is_nearly_sorted_float32(const float *data, size_t count, size_t sample_hint);

static size_t vsort_floor_log2(size_t value);
static size_t vsort_partition_int32(int *data, size_t count, unsigned int flags);
static size_t vsort_partition_float32(float *data, size_t count);

static int *vsort_merge_buffer_int32(size_t count);
static void vsort_merge_buffer_release_int32(void);
static float *vsort_merge_buffer_float32(size_t count);
static void vsort_merge_buffer_release_float32(void);
static void vsort_merge_pool_release(void);

#if defined(VSORT_APPLE) && defined(__arm64__)
static bool vsort_parallel_int32(int *data, size_t count, unsigned int flags);
static bool vsort_parallel_float32(float *data, size_t count, unsigned int flags);
#endif

// -----------------------------------------------------------------------------
// Runtime helpers
// -----------------------------------------------------------------------------

static vsort_runtime_t *vsort_runtime(void)
{
    return &g_runtime;
}

VSORT_API const char *vsort_version(void)
{
    return VSORT_VERSION_STRING;
}

VSORT_API void vsort_set_default_flags(unsigned int flags)
{
    vsort_runtime()->default_flags = flags;
}

VSORT_API unsigned int vsort_default_flags(void)
{
    return vsort_runtime()->default_flags;
}

#if defined(VSORT_APPLE)
static bool vsort_sysctl_value(const char *name, void *value, size_t *size)
{
    if (sysctlbyname(name, value, size, NULL, 0) == -1)
        return false;
    return true;
}
#endif

static void vsort_detect_hardware(vsort_runtime_t *rt)
{
    vsort_hardware_t *hw = &rt->hardware;

    hw->total_cores = vsort_detect_physical_core_count();
    if (hw->total_cores <= 0)
        hw->total_cores = 1;
    hw->performance_cores = hw->total_cores;
    hw->efficiency_cores = 0;
    hw->simd_width = 0;
    hw->has_simd = false;
    hw->has_neon = false;
    hw->cache_line = hw->cache_line ? hw->cache_line : 64;

#if defined(VSORT_APPLE) && defined(__arm64__)
    hw->has_simd = true;
    hw->has_neon = true;
    hw->simd_width = 16;

    size_t size = 0;
    int p_cores = 0;
    int e_cores = 0;

    size = sizeof(p_cores);
    if (!vsort_sysctl_value("hw.perflevel0.physicalcpu", &p_cores, &size) || p_cores <= 0)
    {
        size = sizeof(p_cores);
        vsort_sysctl_value("hw.perflevel0.logicalcpu", &p_cores, &size);
    }

    size = sizeof(e_cores);
    if (!vsort_sysctl_value("hw.perflevel1.physicalcpu", &e_cores, &size) || e_cores <= 0)
    {
        size = sizeof(e_cores);
        vsort_sysctl_value("hw.perflevel1.logicalcpu", &e_cores, &size);
    }

    if (p_cores > 0)
        hw->performance_cores = p_cores;
    if (e_cores > 0)
        hw->efficiency_cores = e_cores;

    if (hw->performance_cores + hw->efficiency_cores != hw->total_cores)
    {
        hw->efficiency_cores = hw->total_cores - hw->performance_cores;
        if (hw->efficiency_cores < 0)
            hw->efficiency_cores = 0;
    }

    size = sizeof(hw->cache_line);
    if (!vsort_sysctl_value("hw.cachelinesize", &hw->cache_line, &size))
        hw->cache_line = 64;

    size = sizeof(hw->l1_cache);
    if (!vsort_sysctl_value("hw.l1dcachesize", &hw->l1_cache, &size))
        hw->l1_cache = 32768;

    size = sizeof(hw->l2_cache);
    if (!vsort_sysctl_value("hw.l2cachesize", &hw->l2_cache, &size))
        hw->l2_cache = 2097152;

    size = sizeof(hw->l3_cache);
    if (!vsort_sysctl_value("hw.l3cachesize", &hw->l3_cache, &size))
        hw->l3_cache = 0;

    size = sizeof(hw->cpu_model);
    if (!vsort_sysctl_value("machdep.cpu.brand_string", hw->cpu_model, &size))
    {
        strncpy(hw->cpu_model, "Apple Silicon", sizeof(hw->cpu_model) - 1);
        hw->cpu_model[sizeof(hw->cpu_model) - 1] = '\0';
    }
    else
    {
        hw->cpu_model[sizeof(hw->cpu_model) - 1] = '\0';
        char *newline = strchr(hw->cpu_model, '\n');
        if (newline)
            *newline = '\0';
    }

#elif defined(VSORT_LINUX)
    hw->performance_cores = hw->total_cores;
    hw->efficiency_cores = 0;

#ifdef __ARM_NEON
    hw->has_simd = true;
    hw->has_neon = true;
    hw->simd_width = 16;
#endif

    FILE *f = NULL;
    char path[256];
    int value = 0;

    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
    if ((f = fopen(path, "r")) != NULL)
    {
        if (fscanf(f, "%d", &value) == 1)
            hw->cache_line = (size_t)value;
        fclose(f);
        f = NULL;
    }

    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index0/size");
    if ((f = fopen(path, "r")) != NULL)
    {
        if (fscanf(f, "%dK", &value) == 1)
            hw->l1_cache = (size_t)value * 1024;
        fclose(f);
        f = NULL;
    }

    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu0/cache/index2/size");
    if ((f = fopen(path, "r")) != NULL)
    {
        if (fscanf(f, "%dK", &value) == 1)
            hw->l2_cache = (size_t)value * 1024;
        fclose(f);
        f = NULL;
    }

    if ((f = fopen("/proc/cpuinfo", "r")) != NULL)
    {
        char line[256];
        while (fgets(line, sizeof(line), f))
        {
            if (strncmp(line, "model name", 10) == 0 || strncmp(line, "Processor", 9) == 0)
            {
                const char *colon = strchr(line, ':');
                if (colon)
                {
                    colon++;
                    while (*colon == ' ' || *colon == '\t')
                        colon++;
                    strncpy(hw->cpu_model, colon, sizeof(hw->cpu_model) - 1);
                    hw->cpu_model[sizeof(hw->cpu_model) - 1] = '\0';
                    char *newline = strchr(hw->cpu_model, '\n');
                    if (newline)
                        *newline = '\0';
                    break;
                }
            }
        }
        fclose(f);
    }
    else
    {
        strncpy(hw->cpu_model, "Linux CPU", sizeof(hw->cpu_model) - 1);
        hw->cpu_model[sizeof(hw->cpu_model) - 1] = '\0';
    }
#else
    strncpy(hw->cpu_model, "Generic CPU", sizeof(hw->cpu_model) - 1);
    hw->cpu_model[sizeof(hw->cpu_model) - 1] = '\0';
#endif
}

static void vsort_calibrate_thresholds(vsort_runtime_t *rt)
{
    const vsort_hardware_t *hw = &rt->hardware;
    vsort_thresholds_t *th = &rt->thresholds;

    size_t l1 = hw->l1_cache ? hw->l1_cache : 32768;
    size_t l2 = hw->l2_cache ? hw->l2_cache : 2097152;

    size_t insertion = l1 / (sizeof(int) * 4);
    insertion = VSORT_CLAMP(insertion, 16, 64);
    th->insertion_threshold = insertion;

    size_t sample = VSORT_CLAMP(insertion * 6, 48, 256);
    th->sample_size = sample;

    size_t parallel = l2 / sizeof(int);
    if (parallel < (size_t)1 << 15)
        parallel = (size_t)1 << 15;
    size_t effective_cores = (size_t)VSORT_MAX(1, hw->performance_cores);
    size_t total_cores = (size_t)VSORT_MAX(1, hw->total_cores);
    float perf_ratio = (float)effective_cores / (float)total_cores;
    parallel = (size_t)((float)parallel * perf_ratio);
    parallel *= effective_cores;
    if (parallel > (size_t)1 << 22)
        parallel = (size_t)1 << 22;
    th->parallel_threshold = parallel;

    size_t radix = (l2 / sizeof(int)) * 2;
    if (radix < (size_t)1 << 18)
        radix = (size_t)1 << 18;
    th->radix_threshold = radix;

    size_t cache_optimal = l1 / sizeof(int);
    if (cache_optimal < insertion * 4)
        cache_optimal = insertion * 4;
    th->cache_optimal_elements = cache_optimal;
}

static int vsort_detect_physical_core_count(void)
{
#if defined(_WIN32) || defined(_MSC_VER)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    if (sysinfo.dwNumberOfProcessors == 0)
        return 1;
    return (int)sysinfo.dwNumberOfProcessors;
#else
#ifdef _SC_NPROCESSORS_ONLN
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs < 1)
    {
#ifdef _SC_NPROCESSORS_CONF
        nprocs = sysconf(_SC_NPROCESSORS_CONF);
#endif
    }
    if (nprocs < 1)
        nprocs = 1;
    return (int)nprocs;
#else
    return 1;
#endif
#endif
}

static void vsort_runtime_initialize(void)
{
    vsort_runtime_t *rt = vsort_runtime();
    if (!rt->logger_ready)
    {
        vsort_log_init(rt->log_level);
        rt->logger_ready = true;
    }

    vsort_detect_hardware(rt);
    vsort_calibrate_thresholds(rt);

    vsort_log_info("VSort runtime initialized on %s with %d total core(s) (%d performance, %d efficiency).",
                   rt->hardware.cpu_model,
                   rt->hardware.total_cores,
                   rt->hardware.performance_cores,
                   rt->hardware.efficiency_cores);
    vsort_log_debug("Threshold configuration - insertion: %zu, sample: %zu, parallel: %zu, radix: %zu, cache-optimal: %zu",
                    rt->thresholds.insertion_threshold,
                    rt->thresholds.sample_size,
                    rt->thresholds.parallel_threshold,
                    rt->thresholds.radix_threshold,
                    rt->thresholds.cache_optimal_elements);

    rt->merge_pool.int_size = 0;
    rt->merge_pool.int_buffer = NULL;
    rt->merge_pool.float_size = 0;
    rt->merge_pool.float_buffer = NULL;
#if defined(_WIN32) || defined(_MSC_VER)
    rt->merge_pool.int_in_use = 0;
    rt->merge_pool.float_in_use = 0;
#else
    atomic_flag_clear(&rt->merge_pool.int_in_use);
    atomic_flag_clear(&rt->merge_pool.float_in_use);
#endif

#if !defined(_WIN32) && !defined(_MSC_VER)
    atomic_store(&g_runtime_ready, true);
#endif
}

#if defined(_WIN32) || defined(_MSC_VER)
static BOOL CALLBACK vsort_runtime_once_callback(PINIT_ONCE once, PVOID param, PVOID *context)
{
    VSORT_UNUSED(once);
    VSORT_UNUSED(param);
    VSORT_UNUSED(context);
    vsort_runtime_initialize();
    return TRUE;
}

VSORT_API void vsort_init(void)
{
    InitOnceExecuteOnce(&g_runtime_once, vsort_runtime_once_callback, NULL, NULL);
    static bool release_registered = false;
    if (!release_registered)
    {
        atexit(vsort_merge_pool_release);
        release_registered = true;
    }
}
#else
VSORT_API void vsort_init(void)
{
    bool expected = false;
    if (atomic_compare_exchange_strong(&g_runtime_init_requested, &expected, true))
    {
        vsort_runtime_initialize();
        atexit(vsort_merge_pool_release);
    }
    else
    {
        while (!atomic_load(&g_runtime_ready))
        {
            sched_yield();
        }
    }
}
#endif

// -----------------------------------------------------------------------------
// Memory helpers
// -----------------------------------------------------------------------------

static void *vsort_aligned_malloc(size_t size)
{
    if (size == 0)
        return NULL;

#if defined(_WIN32) || defined(_MSC_VER)
    void *ptr = _aligned_malloc(size, VSORT_ALIGN);
    if (!ptr)
        vsort_log_error("Failed to allocate %zu aligned bytes (_aligned_malloc).", size);
    return ptr;
#else
    void *ptr = NULL;
    int result = posix_memalign(&ptr, VSORT_ALIGN, size);
    if (result != 0)
    {
        vsort_log_error("Failed to allocate %zu aligned bytes (posix_memalign: %s).", size, strerror(result));
        return NULL;
    }
    return ptr;
#endif
}

static void vsort_aligned_free(void *ptr)
{
    if (!ptr)
        return;
#if defined(_WIN32) || defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

static void vsort_merge_pool_release(void)
{
    vsort_runtime_t *rt = vsort_runtime();
    vsort_aligned_free(rt->merge_pool.int_buffer);
    vsort_aligned_free(rt->merge_pool.float_buffer);
    rt->merge_pool.int_buffer = NULL;
    rt->merge_pool.float_buffer = NULL;
    rt->merge_pool.int_size = 0;
    rt->merge_pool.float_size = 0;
#if defined(_WIN32) || defined(_MSC_VER)
    rt->merge_pool.int_in_use = 0;
    rt->merge_pool.float_in_use = 0;
#else
    atomic_flag_clear(&rt->merge_pool.int_in_use);
    atomic_flag_clear(&rt->merge_pool.float_in_use);
#endif
}

static int *vsort_merge_buffer_int32(size_t count)
{
    vsort_runtime_t *rt = vsort_runtime();
#if defined(_WIN32) || defined(_MSC_VER)
    if (InterlockedExchange(&rt->merge_pool.int_in_use, 1) != 0)
        return NULL;
#else
    if (atomic_flag_test_and_set(&rt->merge_pool.int_in_use))
        return NULL;
#endif

    if (rt->merge_pool.int_size < count)
    {
        vsort_aligned_free(rt->merge_pool.int_buffer);
        rt->merge_pool.int_buffer = vsort_aligned_malloc(count * sizeof(int));
        if (!rt->merge_pool.int_buffer)
        {
            rt->merge_pool.int_size = 0;
#if defined(_WIN32) || defined(_MSC_VER)
            InterlockedExchange(&rt->merge_pool.int_in_use, 0);
#else
            atomic_flag_clear(&rt->merge_pool.int_in_use);
#endif
            return NULL;
        }
        rt->merge_pool.int_size = count;
    }
    return rt->merge_pool.int_buffer;
}

static void vsort_merge_buffer_release_int32(void)
{
    vsort_runtime_t *rt = vsort_runtime();
#if defined(_WIN32) || defined(_MSC_VER)
    InterlockedExchange(&rt->merge_pool.int_in_use, 0);
#else
    atomic_flag_clear(&rt->merge_pool.int_in_use);
#endif
}

static float *vsort_merge_buffer_float32(size_t count)
{
    vsort_runtime_t *rt = vsort_runtime();
#if defined(_WIN32) || defined(_MSC_VER)
    if (InterlockedExchange(&rt->merge_pool.float_in_use, 1) != 0)
        return NULL;
#else
    if (atomic_flag_test_and_set(&rt->merge_pool.float_in_use))
        return NULL;
#endif

    if (rt->merge_pool.float_size < count)
    {
        vsort_aligned_free(rt->merge_pool.float_buffer);
        rt->merge_pool.float_buffer = vsort_aligned_malloc(count * sizeof(float));
        if (!rt->merge_pool.float_buffer)
        {
            rt->merge_pool.float_size = 0;
#if defined(_WIN32) || defined(_MSC_VER)
            InterlockedExchange(&rt->merge_pool.float_in_use, 0);
#else
            atomic_flag_clear(&rt->merge_pool.float_in_use);
#endif
            return NULL;
        }
        rt->merge_pool.float_size = count;
    }
    return rt->merge_pool.float_buffer;
}

static void vsort_merge_buffer_release_float32(void)
{
    vsort_runtime_t *rt = vsort_runtime();
#if defined(_WIN32) || defined(_MSC_VER)
    InterlockedExchange(&rt->merge_pool.float_in_use, 0);
#else
    atomic_flag_clear(&rt->merge_pool.float_in_use);
#endif
}

// -----------------------------------------------------------------------------
// Sorting primitives
// -----------------------------------------------------------------------------

static inline void vsort_swap_int(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

static inline void vsort_swap_float(float *a, float *b)
{
    float tmp = *a;
    *a = *b;
    *b = tmp;
}

static void vsort_insertion_sort_int32(int *data, size_t count)
{
    for (size_t i = 1; i < count; ++i)
    {
        int value = data[i];
        size_t j = i;
        while (j > 0 && data[j - 1] > value)
        {
            data[j] = data[j - 1];
            --j;
        }
        data[j] = value;
    }
}

static void vsort_insertion_sort_float32(float *data, size_t count)
{
    for (size_t i = 1; i < count; ++i)
    {
        float value = data[i];
        size_t j = i;
        while (j > 0 && data[j - 1] > value)
        {
            data[j] = data[j - 1];
            --j;
        }
        data[j] = value;
    }
}

static void vsort_counting_sort_char(unsigned char *data, size_t count)
{
    size_t histogram[256] = {0};

    for (size_t i = 0; i < count; ++i)
        histogram[data[i]]++;

    size_t index = 0;
    for (size_t value = 0; value < 256; ++value)
    {
        size_t occurrences = histogram[value];
        while (occurrences--)
            data[index++] = (unsigned char)value;
    }
}

static size_t vsort_floor_log2(size_t value)
{
    size_t result = 0;
    while (value > 1)
    {
        value >>= 1;
        ++result;
    }
    return result;
}

static void vsort_heapify_down_int32(int *data, size_t count, size_t root)
{
    while (true)
    {
        size_t child = root * 2 + 1;
        if (child >= count)
            break;

        if (child + 1 < count && data[child] < data[child + 1])
            child++;

        if (data[root] >= data[child])
            break;

        vsort_swap_int(&data[root], &data[child]);
        root = child;
    }
}

static void vsort_heapsort_int32(int *data, size_t count)
{
    if (count < 2)
        return;

    for (size_t i = count / 2; i-- > 0;)
        vsort_heapify_down_int32(data, count, i);

    for (size_t i = count - 1; i > 0; --i)
    {
        vsort_swap_int(&data[0], &data[i]);
        vsort_heapify_down_int32(data, i, 0);
    }
}

static void vsort_heapify_down_float32(float *data, size_t count, size_t root)
{
    while (true)
    {
        size_t child = root * 2 + 1;
        if (child >= count)
            break;

        if (child + 1 < count && data[child] < data[child + 1])
            child++;

        if (data[root] >= data[child])
            break;

        vsort_swap_float(&data[root], &data[child]);
        root = child;
    }
}

static void vsort_heapsort_float32(float *data, size_t count)
{
    if (count < 2)
        return;

    for (size_t i = count / 2; i-- > 0;)
        vsort_heapify_down_float32(data, count, i);

    for (size_t i = count - 1; i > 0; --i)
    {
        vsort_swap_float(&data[0], &data[i]);
        vsort_heapify_down_float32(data, i, 0);
    }
}

static size_t vsort_partition_int32(int *data, size_t count, unsigned int flags)
{
    size_t last = count - 1;
    size_t mid = count / 2;

    if (data[0] > data[mid])
        vsort_swap_int(&data[0], &data[mid]);
    if (data[mid] > data[last])
        vsort_swap_int(&data[mid], &data[last]);
    if (data[0] > data[mid])
        vsort_swap_int(&data[0], &data[mid]);

    vsort_swap_int(&data[mid], &data[last]);
    int pivot = data[last];

    size_t i = 0;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    bool allow_simd = (flags & VSORT_FLAG_FORCE_SIMD) != 0u;
    allow_simd = allow_simd || (vsort_runtime()->hardware.has_neon && (flags & VSORT_FLAG_PREFER_THROUGHPUT));
    if (allow_simd && count >= 32)
    {
        int32x4_t pivot_vec = vdupq_n_s32(pivot);
        size_t j = 0;
        for (; j + 4 <= last; j += 4)
        {
            int32x4_t values = vld1q_s32(data + j);
            uint32x4_t mask = vcleq_s32(values, pivot_vec);
            uint64_t mask_bits = vgetq_lane_u64(vreinterpretq_u64_u32(mask), 0) |
                                 (vgetq_lane_u64(vreinterpretq_u64_u32(mask), 1) << 32);
            if (mask_bits == 0xFFFFFFFFFFFFFFFFULL)
            {
                if (i == j)
                {
                    i += 4;
                }
                else
                {
                    for (size_t k = 0; k < 4; ++k)
                    {
                        if (data[j + k] <= pivot)
                        {
                            vsort_swap_int(&data[i], &data[j + k]);
                            ++i;
                        }
                    }
                }
            }
            else if (mask_bits != 0)
            {
                for (size_t k = 0; k < 4; ++k)
                {
                    if (data[j + k] <= pivot)
                    {
                        vsort_swap_int(&data[i], &data[j + k]);
                        ++i;
                    }
                }
            }
        }
        for (; j < last; ++j)
        {
            if (data[j] <= pivot)
            {
                vsort_swap_int(&data[i], &data[j]);
                ++i;
            }
        }
        vsort_swap_int(&data[i], &data[last]);
        return i;
    }
#endif
    for (size_t j = 0; j < last; ++j)
    {
        if (data[j] <= pivot)
        {
            vsort_swap_int(&data[i], &data[j]);
            ++i;
        }
    }
    vsort_swap_int(&data[i], &data[last]);
    return i;
}

static size_t vsort_partition_float32(float *data, size_t count)
{
    size_t last = count - 1;
    size_t mid = count / 2;

    if (data[0] > data[mid])
        vsort_swap_float(&data[0], &data[mid]);
    if (data[mid] > data[last])
        vsort_swap_float(&data[mid], &data[last]);
    if (data[0] > data[mid])
        vsort_swap_float(&data[0], &data[mid]);

    vsort_swap_float(&data[mid], &data[last]);
    float pivot = data[last];

    size_t i = 0;
    for (size_t j = 0; j < last; ++j)
    {
        if (data[j] <= pivot)
        {
            vsort_swap_float(&data[i], &data[j]);
            ++i;
        }
    }
    vsort_swap_float(&data[i], &data[last]);
    return i;
}

static void vsort_introsort_int32_impl(int *data, size_t count, size_t depth_limit, unsigned int flags)
{
    size_t threshold = vsort_runtime()->thresholds.insertion_threshold;

    while (count > threshold)
    {
        if (depth_limit == 0)
        {
            vsort_heapsort_int32(data, count);
            return;
        }

        size_t pivot_index = vsort_partition_int32(data, count, flags);
        size_t left_count = pivot_index;
        size_t right_count = count - pivot_index - 1;

        if (left_count < right_count)
        {
            if (left_count > 0)
                vsort_introsort_int32_impl(data, left_count, depth_limit - 1, flags);
            data += pivot_index + 1;
            count = right_count;
        }
        else
        {
            if (right_count > 0)
                vsort_introsort_int32_impl(data + pivot_index + 1, right_count, depth_limit - 1, flags);
            count = left_count;
        }
    }

    vsort_insertion_sort_int32(data, count);
}

static void vsort_introsort_float32_impl(float *data, size_t count, size_t depth_limit)
{
    size_t threshold = vsort_runtime()->thresholds.insertion_threshold;

    while (count > threshold)
    {
        if (depth_limit == 0)
        {
            vsort_heapsort_float32(data, count);
            return;
        }

        size_t pivot_index = vsort_partition_float32(data, count);
        size_t left_count = pivot_index;
        size_t right_count = count - pivot_index - 1;

        if (left_count < right_count)
        {
            if (left_count > 0)
                vsort_introsort_float32_impl(data, left_count, depth_limit - 1);
            data += pivot_index + 1;
            count = right_count;
        }
        else
        {
            if (right_count > 0)
                vsort_introsort_float32_impl(data + pivot_index + 1, right_count, depth_limit - 1);
            count = left_count;
        }
    }

    vsort_insertion_sort_float32(data, count);
}

static void vsort_introsort_int32(int *data, size_t count, unsigned int flags)
{
    if (count <= 1)
        return;

    size_t depth_limit = 2 * vsort_floor_log2(count);
    if (depth_limit == 0)
        depth_limit = 1;
    vsort_introsort_int32_impl(data, count, depth_limit, flags);
}

static void vsort_introsort_float32(float *data, size_t count)
{
    if (count <= 1)
        return;

    size_t depth_limit = 2 * vsort_floor_log2(count);
    if (depth_limit == 0)
        depth_limit = 1;
    vsort_introsort_float32_impl(data, count, depth_limit);
}

static void vsort_merge_int32(int *data, int *buffer, size_t left, size_t mid, size_t right)
{
    size_t left_count = mid - left;
    if (left_count == 0)
        return;

    memcpy(buffer + left, data + left, left_count * sizeof(int));

    size_t i = 0;
    size_t j = mid;
    size_t dest = left;

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    VSORT_UNUSED(left_count);
#endif

    while (i < left_count && j < right)
    {
        if (buffer[left + i] <= data[j])
        {
            data[dest++] = buffer[left + i];
            ++i;
        }
        else
        {
            data[dest++] = data[j++];
        }
    }

    while (i < left_count)
        data[dest++] = buffer[left + i++];
}

static void vsort_merge_float32(float *data, float *buffer, size_t left, size_t mid, size_t right)
{
    size_t left_count = mid - left;
    if (left_count == 0)
        return;

    memcpy(buffer + left, data + left, left_count * sizeof(float));

    size_t i = 0;
    size_t j = mid;
    size_t dest = left;

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    VSORT_UNUSED(left_count);
#endif

    while (i < left_count && j < right)
    {
        if (buffer[left + i] <= data[j])
        {
            data[dest++] = buffer[left + i];
            ++i;
        }
        else
        {
            data[dest++] = data[j++];
        }
    }

    while (i < left_count)
        data[dest++] = buffer[left + i++];
}

static void vsort_mergesort_int32_impl(int *data, int *buffer, size_t left, size_t right)
{
    size_t count = right - left;
    if (count <= vsort_runtime()->thresholds.insertion_threshold)
    {
        vsort_insertion_sort_int32(data + left, count);
        return;
    }

    size_t mid = left + count / 2;
    vsort_mergesort_int32_impl(data, buffer, left, mid);
    vsort_mergesort_int32_impl(data, buffer, mid, right);

    if (data[mid - 1] <= data[mid])
        return;

    vsort_merge_int32(data, buffer, left, mid, right);
}

static void vsort_mergesort_float32_impl(float *data, float *buffer, size_t left, size_t right)
{
    size_t count = right - left;
    if (count <= vsort_runtime()->thresholds.insertion_threshold)
    {
        vsort_insertion_sort_float32(data + left, count);
        return;
    }

    size_t mid = left + count / 2;
    vsort_mergesort_float32_impl(data, buffer, left, mid);
    vsort_mergesort_float32_impl(data, buffer, mid, right);

    if (data[mid - 1] <= data[mid])
        return;

    vsort_merge_float32(data, buffer, left, mid, right);
}

static bool vsort_mergesort_int32(int *data, size_t count)
{
    if (count <= 1)
        return true;

    int *buffer = vsort_merge_buffer_int32(count);
    bool pooled = buffer != NULL;
    if (!buffer)
        buffer = vsort_aligned_malloc(count * sizeof(int));
    if (!buffer)
        return false;

    vsort_mergesort_int32_impl(data, buffer, 0, count);
    if (pooled)
        vsort_merge_buffer_release_int32();
    else
        vsort_aligned_free(buffer);
    return true;
}

static bool vsort_mergesort_float32(float *data, size_t count)
{
    if (count <= 1)
        return true;

    float *buffer = vsort_merge_buffer_float32(count);
    bool pooled = buffer != NULL;
    if (!buffer)
        buffer = vsort_aligned_malloc(count * sizeof(float));
    if (!buffer)
        return false;

    vsort_mergesort_float32_impl(data, buffer, 0, count);
    if (pooled)
        vsort_merge_buffer_release_float32();
    else
        vsort_aligned_free(buffer);
    return true;
}

static bool vsort_radix_sort_int32(int *data, size_t count)
{
    if (count <= 1)
        return true;

    int min_value = data[0];
    int max_value = data[0];
    for (size_t i = 1; i < count; ++i)
    {
        if (data[i] < min_value)
            min_value = data[i];
        if (data[i] > max_value)
            max_value = data[i];
    }

    long long range = (long long)max_value - (long long)min_value;
    if (range > (long long)UINT_MAX)
    {
        vsort_log_debug("Radix sort skipped due to excessive value range.");
        return false;
    }

    unsigned int shift = 0;
    if (min_value < 0)
        shift = (unsigned int)(-min_value);

    unsigned int *shifted = vsort_aligned_malloc(count * sizeof(unsigned int));
    if (!shifted)
        return false;

    for (size_t i = 0; i < count; ++i)
        shifted[i] = (unsigned int)(data[i] + shift);

    unsigned int *buffer = vsort_aligned_malloc(count * sizeof(unsigned int));
    if (!buffer)
    {
        vsort_aligned_free(shifted);
        return false;
    }

    const size_t bits = 8;
    const size_t bins = 1u << bits;
    const unsigned int mask = (unsigned int)(bins - 1u);
    unsigned int histogram[256];

    unsigned int maximum = 0;
    for (size_t i = 0; i < count; ++i)
        if (shifted[i] > maximum)
            maximum = shifted[i];

    size_t passes = 0;
    if (maximum == 0)
    {
        passes = 1;
    }
    else
    {
        unsigned int leading = vsort_clz32(maximum);
        size_t bits_required = (sizeof(unsigned int) * CHAR_BIT) - leading;
        passes = (bits_required + bits - 1) / bits;
        if (passes == 0)
            passes = 1;
    }

    unsigned int *input = shifted;
    unsigned int *output = buffer;

    for (size_t pass = 0; pass < passes; ++pass)
    {
        size_t offset = pass * bits;
        memset(histogram, 0, sizeof(histogram));

        for (size_t i = 0; i < count; ++i)
        {
            unsigned int bucket = (input[i] >> offset) & mask;
            histogram[bucket]++;
        }

        unsigned int total = 0;
        for (size_t i = 0; i < bins; ++i)
        {
            unsigned int tmp = histogram[i];
            histogram[i] = total + tmp;
            total += tmp;
        }

        for (size_t i = count; i-- > 0;)
        {
            unsigned int value = input[i];
            unsigned int bucket = (value >> offset) & mask;
            output[--histogram[bucket]] = value;
        }

        unsigned int *swap = input;
        input = output;
        output = swap;
    }

    for (size_t i = 0; i < count; ++i)
        data[i] = (int)(input[i] - shift);

    vsort_aligned_free(shifted);
    vsort_aligned_free(buffer);
    return true;
}

static bool vsort_is_nearly_sorted_int32(const int *data, size_t count, size_t sample_hint)
{
    if (count < 32)
        return false;

    size_t samples = VSORT_MIN(sample_hint, count / 2);
    if (samples < 8)
        return false;

    size_t step = VSORT_MAX((size_t)1, count / samples);
    size_t inversions = 0;
    size_t observed = 0;

    for (size_t i = 0; i + step < count && observed < samples; i += step, ++observed)
        if (data[i] > data[i + step])
            ++inversions;

    if (observed == 0)
        return false;

    return inversions * 10 < observed;
}

static bool vsort_is_nearly_sorted_float32(const float *data, size_t count, size_t sample_hint)
{
    if (count < 32)
        return false;

    size_t samples = VSORT_MIN(sample_hint, count / 2);
    if (samples < 8)
        return false;

    size_t step = VSORT_MAX((size_t)1, count / samples);
    size_t inversions = 0;
    size_t observed = 0;

    for (size_t i = 0; i + step < count && observed < samples; i += step, ++observed)
        if (data[i] > data[i + step])
            ++inversions;

    if (observed == 0)
        return false;

    return inversions * 10 < observed;
}

// -----------------------------------------------------------------------------
// Parallel helpers (Apple Silicon)
// -----------------------------------------------------------------------------

#if defined(VSORT_APPLE) && defined(__arm64__)

static bool vsort_parallel_int32(int *data, size_t count, unsigned int flags)
{
    if (count < 2)
        return true;

    vsort_runtime_t *rt = vsort_runtime();
    size_t chunk = VSORT_MAX(rt->thresholds.cache_optimal_elements, rt->thresholds.insertion_threshold * 8);
    if (chunk == 0)
        chunk = 4096;

    size_t chunk_count = (count + chunk - 1) / chunk;
    if (chunk_count == 0)
        return false;

    dispatch_qos_class_t qos = QOS_CLASS_USER_INITIATED;
    if (flags & VSORT_FLAG_PREFER_EFFICIENCY)
        qos = QOS_CLASS_UTILITY;

    dispatch_queue_t queue = dispatch_get_global_queue(qos, 0);

    dispatch_apply(chunk_count, queue, ^(size_t index) {
      size_t begin = index * chunk;
      size_t end = VSORT_MIN(begin + chunk, count);
      size_t local = end - begin;

      if (local <= 1)
          return;

      if (local <= rt->thresholds.insertion_threshold)
      {
          vsort_insertion_sort_int32(data + begin, local);
          return;
      }

      size_t depth = 2 * vsort_floor_log2(local);
      if (depth == 0)
          depth = 1;
      vsort_introsort_int32_impl(data + begin, local, depth, flags);
    });

    int *buffer = vsort_merge_buffer_int32(count);
    bool pooled = buffer != NULL;
    if (!buffer)
        buffer = vsort_aligned_malloc(count * sizeof(int));
    if (!buffer)
        return false;

    for (size_t width = chunk; width < count; width *= 2)
    {
        size_t pair_count = (count + (width * 2) - 1) / (width * 2);
        dispatch_apply(pair_count, queue, ^(size_t pair) {
          size_t left = pair * width * 2;
          size_t mid = VSORT_MIN(left + width, count);
          size_t right = VSORT_MIN(left + width * 2, count);

          if (mid < right)
          {
              if (data[mid - 1] <= data[mid])
                  return;
              vsort_merge_int32(data, buffer, left, mid, right);
          }
        });
    }

    if (pooled)
        vsort_merge_buffer_release_int32();
    else
        vsort_aligned_free(buffer);
    return true;
}

static bool vsort_parallel_float32(float *data, size_t count, unsigned int flags)
{
    if (count < 2)
        return true;

    vsort_runtime_t *rt = vsort_runtime();
    size_t chunk = VSORT_MAX(rt->thresholds.cache_optimal_elements, rt->thresholds.insertion_threshold * 8);
    if (chunk == 0)
        chunk = 4096;

    size_t chunk_count = (count + chunk - 1) / chunk;
    if (chunk_count == 0)
        return false;

    dispatch_qos_class_t qos = QOS_CLASS_USER_INITIATED;
    if (flags & VSORT_FLAG_PREFER_EFFICIENCY)
        qos = QOS_CLASS_UTILITY;

    dispatch_queue_t queue = dispatch_get_global_queue(qos, 0);

    dispatch_apply(chunk_count, queue, ^(size_t index) {
      size_t begin = index * chunk;
      size_t end = VSORT_MIN(begin + chunk, count);
      size_t local = end - begin;

      if (local <= 1)
          return;

      if (local <= rt->thresholds.insertion_threshold)
      {
          vsort_insertion_sort_float32(data + begin, local);
          return;
      }

      size_t depth = 2 * vsort_floor_log2(local);
      if (depth == 0)
          depth = 1;
      vsort_introsort_float32_impl(data + begin, local, depth);
    });

    float *buffer = vsort_merge_buffer_float32(count);
    bool pooled = buffer != NULL;
    if (!buffer)
        buffer = vsort_aligned_malloc(count * sizeof(float));
    if (!buffer)
        return false;

    for (size_t width = chunk; width < count; width *= 2)
    {
        size_t pair_count = (count + (width * 2) - 1) / (width * 2);
        dispatch_apply(pair_count, queue, ^(size_t pair) {
          size_t left = pair * width * 2;
          size_t mid = VSORT_MIN(left + width, count);
          size_t right = VSORT_MIN(left + width * 2, count);

          if (mid < right)
          {
              if (data[mid - 1] <= data[mid])
                  return;
              vsort_merge_float32(data, buffer, left, mid, right);
          }
        });
    }

    if (pooled)
        vsort_merge_buffer_release_float32();
    else
        vsort_aligned_free(buffer);
    return true;
}

#endif

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

VSORT_API vsort_result_t vsort_sort(const vsort_options_t *options)
{
    if (!options)
        return VSORT_ERR_INVALID_ARGUMENT;

    if (!options->data && options->length > 0)
        return VSORT_ERR_INVALID_ARGUMENT;

    if (options->length <= 1)
        return VSORT_OK;

    vsort_init();

    vsort_runtime_t *rt = vsort_runtime();
    unsigned int flags = options->flags ? options->flags : rt->default_flags;
    if ((flags & VSORT_FLAG_PREFER_EFFICIENCY) && (flags & VSORT_FLAG_PREFER_THROUGHPUT))
        flags &= ~VSORT_FLAG_PREFER_EFFICIENCY;
    if (!(flags & VSORT_FLAG_PREFER_EFFICIENCY))
        flags |= VSORT_FLAG_PREFER_THROUGHPUT;

    switch (options->kind)
    {
    case VSORT_KIND_INT32:
    {
        int *data = (int *)options->data;
        size_t count = options->length;

        if ((flags & VSORT_FLAG_FORCE_STABLE))
        {
            if (!vsort_mergesort_int32(data, count))
            {
                vsort_log_warning("Stable sort allocation failed, falling back to introsort.");
                vsort_introsort_int32(data, count, flags);
            }
            return VSORT_OK;
        }

        if (vsort_is_nearly_sorted_int32(data, count, rt->thresholds.sample_size))
        {
            vsort_insertion_sort_int32(data, count);
            return VSORT_OK;
        }

        bool attempted_radix = false;
        if ((flags & VSORT_FLAG_ALLOW_RADIX) && count >= rt->thresholds.radix_threshold)
        {
            attempted_radix = true;
            if (vsort_radix_sort_int32(data, count))
                return VSORT_OK;
        }

        bool use_parallel = (flags & VSORT_FLAG_ALLOW_PARALLEL) && count >= rt->thresholds.parallel_threshold;
        if (flags & VSORT_FLAG_PREFER_EFFICIENCY)
            use_parallel = use_parallel && count >= (rt->thresholds.parallel_threshold * 2);
#if defined(VSORT_APPLE) && defined(__arm64__)
        if (use_parallel)
        {
            if (vsort_parallel_int32(data, count, flags))
                return VSORT_OK;
            vsort_log_debug("Parallel path unavailable, reverting to sequential sort for %zu int elements.", count);
        }
#else
        VSORT_UNUSED(use_parallel);
#endif

        if (attempted_radix)
            vsort_log_debug("Radix sort unavailable, using introsort for %zu int elements.", count);

        vsort_introsort_int32(data, count, flags);
        return VSORT_OK;
    }
    case VSORT_KIND_FLOAT32:
    {
        float *data = (float *)options->data;
        size_t count = options->length;

        if ((flags & VSORT_FLAG_FORCE_STABLE))
        {
            if (!vsort_mergesort_float32(data, count))
            {
                vsort_log_warning("Stable float sort allocation failed, falling back to introsort.");
                vsort_introsort_float32(data, count);
            }
            return VSORT_OK;
        }

        if (vsort_is_nearly_sorted_float32(data, count, rt->thresholds.sample_size))
        {
            vsort_insertion_sort_float32(data, count);
            return VSORT_OK;
        }

        bool use_parallel = (flags & VSORT_FLAG_ALLOW_PARALLEL) && count >= rt->thresholds.parallel_threshold;
        if (flags & VSORT_FLAG_PREFER_EFFICIENCY)
            use_parallel = use_parallel && count >= (rt->thresholds.parallel_threshold * 2);
#if defined(VSORT_APPLE) && defined(__arm64__)
        if (use_parallel)
        {
            if (vsort_parallel_float32(data, count, flags))
                return VSORT_OK;
            vsort_log_debug("Parallel path unavailable, reverting to sequential sort for %zu float elements.", count);
        }
#else
        VSORT_UNUSED(use_parallel);
#endif

        vsort_introsort_float32(data, count);
        return VSORT_OK;
    }
    case VSORT_KIND_CHAR8:
    {
        unsigned char *data = (unsigned char *)options->data;
        vsort_counting_sort_char(data, options->length);
        return VSORT_OK;
    }
    case VSORT_KIND_GENERIC:
    {
        if (!options->comparator || options->element_size == 0)
            return VSORT_ERR_INVALID_ARGUMENT;
        qsort(options->data, options->length, options->element_size, options->comparator);
        return VSORT_OK;
    }
    default:
        return VSORT_ERR_UNSUPPORTED_TYPE;
    }
}

VSORT_API void vsort(int arr[], int n)
{
    if (!arr || n <= 1)
        return;

    vsort_options_t options = {
        .data = arr,
        .length = (size_t)n,
        .element_size = sizeof(int),
        .kind = VSORT_KIND_INT32,
        .comparator = NULL,
        .flags = vsort_default_flags()};
    (void)vsort_sort(&options);
}

VSORT_API void vsort_float(float arr[], int n)
{
    if (!arr || n <= 1)
        return;

    vsort_options_t options = {
        .data = arr,
        .length = (size_t)n,
        .element_size = sizeof(float),
        .kind = VSORT_KIND_FLOAT32,
        .comparator = NULL,
        .flags = vsort_default_flags() & ~VSORT_FLAG_ALLOW_RADIX};
    (void)vsort_sort(&options);
}

VSORT_API void vsort_char(char arr[], int n)
{
    if (!arr || n <= 1)
        return;

    vsort_options_t options = {
        .data = arr,
        .length = (size_t)n,
        .element_size = sizeof(char),
        .kind = VSORT_KIND_CHAR8,
        .comparator = NULL,
        .flags = 0};
    (void)vsort_sort(&options);
}

VSORT_API void vsort_with_comparator(void *arr, int n, size_t size, int (*compare)(const void *, const void *))
{
    if (!arr || n <= 1 || size == 0 || !compare)
        return;

    vsort_options_t options = {
        .data = arr,
        .length = (size_t)n,
        .element_size = size,
        .kind = VSORT_KIND_GENERIC,
        .comparator = compare,
        .flags = 0};
    (void)vsort_sort(&options);
}

VSORT_API int get_num_processors(void)
{
    vsort_init();
    return vsort_runtime()->hardware.total_cores;
}
