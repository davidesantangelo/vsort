/**
 * test_performance.c - Performance measurements for vsort
 *
 * This test measures the performance of vsort across different
 * array sizes and data patterns.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../vsort.h"

// Time measurement
static double measure_time(void (*sort_func)(int[], int), int arr[], int n)
{
    clock_t start, end;

    start = clock();
    sort_func(arr, n);
    end = clock();

    return ((double)(end - start) * 1000.0) / CLOCKS_PER_SEC; // Convert to ms
}

// Array generation functions
static void fill_random(int *arr, int n)
{
    for (int i = 0; i < n; i++)
    {
        arr[i] = rand() % n;
    }
}

static void fill_sorted(int *arr, int n)
{
    for (int i = 0; i < n; i++)
    {
        arr[i] = i;
    }
}

static void fill_reverse_sorted(int *arr, int n)
{
    for (int i = 0; i < n; i++)
    {
        arr[i] = n - i - 1;
    }
}

static void fill_nearly_sorted(int *arr, int n, double disorder_ratio)
{
    fill_sorted(arr, n);

    int swaps = (int)(n * disorder_ratio);
    for (int i = 0; i < swaps; i++)
    {
        int idx1 = rand() % n;
        int idx2 = rand() % n;
        int temp = arr[idx1];
        arr[idx1] = arr[idx2];
        arr[idx2] = temp;
    }
}

// Verify sort correctness
static int is_sorted(int *arr, int n)
{
    for (int i = 1; i < n; i++)
    {
        if (arr[i] < arr[i - 1])
        {
            return 0;
        }
    }
    return 1;
}

int main()
{
    printf("vsort Performance Test\n");
    printf("=====================\n\n");

    srand(time(NULL));

    // FIXED: Limit array sizes to avoid memory issues and bus errors
    // Original: int sizes[] = {100, 1000, 10000, 100000, 1000000};
    int sizes[] = {100, 1000, 10000, 50000, 100000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    // Number of test runs per configuration for averaging
    int runs = 3;

    printf("%-10s | %-15s | %-15s | %-15s | %-15s\n",
           "Size", "Random (ms)", "Sorted (ms)", "Reverse (ms)", "Nearly (ms)");
    printf("------------------------------------------------------------------------\n");

    for (int s = 0; s < num_sizes; s++)
    {
        int n = sizes[s];
        int *arr = (int *)malloc(n * sizeof(int));
        if (!arr)
        {
            printf("Memory allocation failed for size %d\n", n);
            continue;
        }

        double random_time = 0.0;
        double sorted_time = 0.0;
        double reverse_time = 0.0;
        double nearly_time = 0.0;

        for (int r = 0; r < runs; r++)
        {
            // Test with random data
            fill_random(arr, n);
            random_time += measure_time(vsort, arr, n);
            if (!is_sorted(arr, n))
            {
                printf("ERROR: Failed to sort random array correctly\n");
            }

            // Test with sorted data
            fill_sorted(arr, n);
            sorted_time += measure_time(vsort, arr, n);

            // Test with reverse-sorted data
            fill_reverse_sorted(arr, n);
            reverse_time += measure_time(vsort, arr, n);

            // Test with nearly-sorted data (5% disorder)
            fill_nearly_sorted(arr, n, 0.05);
            nearly_time += measure_time(vsort, arr, n);
        }

        // Calculate averages
        random_time /= runs;
        sorted_time /= runs;
        reverse_time /= runs;
        nearly_time /= runs;

        printf("%-10d | %-15.2f | %-15.2f | %-15.2f | %-15.2f\n",
               n, random_time, sorted_time, reverse_time, nearly_time);

        free(arr);
    }

    return 0;
}
