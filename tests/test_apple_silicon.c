/**
 * test_apple_silicon.c - Tests specific to Apple Silicon optimizations
 *
 * This test verifies that the Apple Silicon specific optimizations
 * are working as expected.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../vsort.h"

#if defined(__APPLE__) && defined(__arm64__)
#include <mach/mach_time.h>
#include <dispatch/dispatch.h>

// High-precision timing for macOS
static uint64_t get_time()
{
    return mach_absolute_time();
}

static double time_to_ms(uint64_t start, uint64_t end)
{
    static mach_timebase_info_data_t timebase_info;
    if (timebase_info.denom == 0)
    {
        mach_timebase_info(&timebase_info);
    }

    uint64_t elapsed = end - start;
    uint64_t nanos = elapsed * timebase_info.numer / timebase_info.denom;
    return nanos / 1000000.0;
}

// Test array creation
static int *create_random_array(int n)
{
    int *arr = (int *)malloc(n * sizeof(int));
    if (!arr)
        return NULL;

    for (int i = 0; i < n; i++)
    {
        arr[i] = rand() % n;
    }
    return arr;
}

static int is_sorted(int *arr, int n)
{
    for (int i = 1; i < n; i++)
    {
        if (arr[i] < arr[i - 1])
            return 0;
    }
    return 1;
}

// Test cases
static int test_vectorization_threshold()
{
    printf("Testing vectorization threshold effect... ");

    // Arrays around the vectorization threshold
    int sizes[] = {32, 48, 64, 96, 128, 192, 256};
    double times[7] = {0};

    for (int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        int n = sizes[i];
        int *arr = create_random_array(n);
        if (!arr)
        {
            printf("FAILED: Memory allocation error\n");
            return 0;
        }

        uint64_t start = get_time();
        vsort(arr, n);
        uint64_t end = get_time();

        if (!is_sorted(arr, n))
        {
            printf("FAILED: Array of size %d not sorted correctly\n", n);
            free(arr);
            return 0;
        }

        times[i] = time_to_ms(start, end);
        free(arr);
    }

    printf("PASSED\n");

    // Print the timing results
    printf("  Size  |  Time (ms)\n");
    printf("-----------------\n");
    for (int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        printf(" %5d  |  %.5f\n", sizes[i], times[i]);
    }

    return 1;
}

static int test_parallel_sorting()
{
    printf("Testing parallel sorting... ");

    // Create an array large enough to trigger parallelization
    int n = 1000000; // Should be above PARALLEL_THRESHOLD
    int *arr = create_random_array(n);

    if (!arr)
    {
        printf("FAILED: Memory allocation error\n");
        return 0;
    }

    uint64_t start = get_time();
    vsort(arr, n);
    uint64_t end = get_time();

    if (!is_sorted(arr, n))
    {
        printf("FAILED: Array not sorted correctly\n");
        free(arr);
        return 0;
    }

    double time_ms = time_to_ms(start, end);
    printf("PASSED (%.2f ms)\n", time_ms);

    free(arr);
    return 1;
}

int main()
{
    printf("Running Apple Silicon specific tests...\n\n");

    // Check we're actually running on Apple Silicon
    printf("Processor information:\n");
    system("sysctl -n machdep.cpu.brand_string");
    printf("\n");

    srand(time(NULL));

    int passed = 1;
    passed &= test_vectorization_threshold();
    passed &= test_parallel_sorting();

    printf("\nApple Silicon specific test summary: %s\n",
           passed ? "ALL PASSED" : "SOME TESTS FAILED");

    return passed ? 0 : 1;
}

#else
int main()
{
    printf("This test is only for Apple Silicon devices.\n");
    return 0;
}
#endif
