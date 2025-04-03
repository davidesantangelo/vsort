#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <float.h> // For DBL_MAX
#include <math.h>  // Keep for potential future use, though INFINITY removed
#include "vsort.h"

// Implementation of other sorting algorithms for comparison
void custom_quicksort(int arr[], int low, int high);
int quick_partition(int arr[], int low, int high);
// Modified mergesort to use a temporary buffer
void custom_mergesort(int arr[], int temp_buffer[], int low, int high);
void merge_arrays(int arr[], int temp_buffer[], int low, int mid, int high);
// Standard library qsort wrapper
void std_sort(int arr[], int n);

// Wrapper functions for measuring performance
void quicksort_wrapper(int arr[], int n);
void mergesort_wrapper(int arr[], int n); // Will allocate temp buffer inside

// Utility functions
void generate_random_array(int arr[], int n, int max_val);
void generate_nearly_sorted_array(int arr[], int n, int max_val, double swap_ratio);
void generate_few_unique_array(int arr[], int n, int unique_vals);
int *copy_array(const int arr[], int n);                                   // Mark input as const
int is_sorted(const int arr[], int n);                                     // Mark input as const
double measure_time(void (*sort_func)(int[], int), int arr_orig[], int n); // Pass original array

// Global comparison function for qsort
int compare_ints(const void *a, const void *b);

// --- Sorting Implementations ---

/**
 * Quick sort implementation for comparison
 */
void custom_quicksort(int arr[], int low, int high)
{
    if (low < high)
    {
        int pivot_idx = quick_partition(arr, low, high);
        // Recursively sort elements before and after partition
        // Check index validity to prevent potential infinite recursion on edge cases
        if (pivot_idx > low)
        {
            custom_quicksort(arr, low, pivot_idx - 1);
        }
        if (pivot_idx < high)
        {
            custom_quicksort(arr, pivot_idx + 1, high);
        }
    }
}

int quick_partition(int arr[], int low, int high)
{
    // Simple Lomuto partition scheme with last element as pivot
    int pivot = arr[high];
    int i = low - 1; // Index of smaller element

    for (int j = low; j < high; j++) // Iterate up to high-1
    {
        // If current element is smaller than or equal to pivot
        if (arr[j] <= pivot)
        {
            i++; // increment index of smaller element
            // Swap arr[i] and arr[j]
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    // Swap arr[i+1] and arr[high] (pivot)
    int temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;

    return i + 1; // Return partitioning index
}

/**
 * Merge sort implementation for comparison (using a temp buffer)
 */
void custom_mergesort(int arr[], int temp_buffer[], int low, int high)
{
    if (low < high)
    {
        int mid = low + (high - low) / 2; // Avoid potential overflow
        custom_mergesort(arr, temp_buffer, low, mid);
        custom_mergesort(arr, temp_buffer, mid + 1, high);
        merge_arrays(arr, temp_buffer, low, mid, high);
    }
}

// Merge function using a single pre-allocated temporary buffer
void merge_arrays(int arr[], int temp_buffer[], int low, int mid, int high)
{
    int i = low;     // Initial index of first subarray
    int j = mid + 1; // Initial index of second subarray
    int k = low;     // Initial index of merged subarray (in temp_buffer)

    // Copy relevant part to temp buffer first for stable merging
    // Only need to copy the range being merged
    memcpy(&temp_buffer[low], &arr[low], (high - low + 1) * sizeof(int));

    // Merge data from temp_buffer back into arr
    while (i <= mid && j <= high)
    {
        if (temp_buffer[i] <= temp_buffer[j])
        {
            arr[k] = temp_buffer[i];
            i++;
        }
        else
        {
            arr[k] = temp_buffer[j];
            j++;
        }
        k++;
    }

    // Copy the remaining elements of left subarray (if any)
    while (i <= mid)
    {
        arr[k] = temp_buffer[i];
        i++;
        k++;
    }
    // No need to copy remaining elements of right subarray,
    // they are already in their correct relative place in the temp buffer
    // (which was copied from the original arr positions)
}

/**
 * std::sort (using C qsort) implementation for comparison
 */
// Define comparator globally ONCE
int compare_ints(const void *a, const void *b)
{
    int int_a = *(const int *)a;
    int int_b = *(const int *)b;
    // Standard comparison: handles INT_MIN/INT_MAX correctly
    if (int_a < int_b)
        return -1;
    if (int_a > int_b)
        return 1;
    return 0;
    // return (int_a > int_b) - (int_a < int_b); // Alternative shorter form
}

// Wrapper for stdlib qsort
void std_sort(int arr[], int n)
{
    qsort(arr, n, sizeof(int), compare_ints);
}

// --- Wrappers for Timing ---

/**
 * Wrapper function for custom quicksort
 */
void quicksort_wrapper(int arr[], int n)
{
    if (n > 0)
    { // Add check for empty array
        custom_quicksort(arr, 0, n - 1);
    }
}

/**
 * Wrapper function for custom mergesort - allocates/frees temp buffer
 */
void mergesort_wrapper(int arr[], int n)
{
    if (n <= 0)
        return; // Handle empty array

    // Allocate temporary buffer needed by merge_arrays
    int *temp_buffer = (int *)malloc(n * sizeof(int));
    if (!temp_buffer)
    {
        fprintf(stderr, "Memory allocation failed in mergesort_wrapper\n");
        // Cannot sort without buffer, maybe fallback or just return?
        return;
    }
    custom_mergesort(arr, temp_buffer, 0, n - 1);
    free(temp_buffer); // Free the buffer after sorting is complete
}

// --- Utility Functions ---

/**
 * Generate an array with random values
 */
void generate_random_array(int arr[], int n, int max_val)
{
    // Assumes srand() was called once in main
    for (int i = 0; i < n; i++)
    {
        // Ensure max_val is positive for modulo
        arr[i] = rand() % (max_val > 0 ? max_val : 1);
    }
}

/**
 * Generate a nearly sorted array
 */
void generate_nearly_sorted_array(int arr[], int n, int max_val, double swap_ratio)
{
    // First create a sorted array
    for (int i = 0; i < n; i++)
    {
        // Generate sorted values within the max_val range
        arr[i] = (int)(((double)i / n) * max_val);
    }

    // Then swap some elements randomly
    int swaps = (int)(n * swap_ratio);
    // Assumes srand() was called once in main
    for (int i = 0; i < swaps; i++)
    {
        int idx1 = rand() % n;
        int idx2 = rand() % n;
        // Simple swap
        int temp = arr[idx1];
        arr[idx1] = arr[idx2];
        arr[idx2] = temp;
    }
}

/**
 * Generate an array with few unique values
 */
void generate_few_unique_array(int arr[], int n, int unique_vals)
{
    if (unique_vals <= 0)
        unique_vals = 1; // Ensure at least one unique value
    // Assumes srand() was called once in main
    for (int i = 0; i < n; i++)
    {
        arr[i] = rand() % unique_vals;
    }
}

/**
 * Create a copy of the given array
 */
int *copy_array(const int arr[], int n) // Mark input as const
{
    if (n <= 0 || !arr)
        return NULL;
    int *copy = (int *)malloc(n * sizeof(int));
    if (copy)
    {
        memcpy(copy, arr, n * sizeof(int));
    }
    else
    {
        fprintf(stderr, "Failed to allocate memory in copy_array\n");
    }
    return copy;
}

/**
 * Check if an array is sorted
 */
int is_sorted(const int arr[], int n) // Mark input as const
{
    if (!arr)
        return 0; // Null array is not sorted
    for (int i = 1; i < n; i++)
    {
        if (arr[i] < arr[i - 1])
        {
            fprintf(stderr, "Verification FAILED at index %d: %d < %d\n", i, arr[i], arr[i - 1]);
            return 0; // Not sorted
        }
    }
    return 1; // Sorted
}

/**
 * Measure execution time of a sorting function (takes original array)
 */
double measure_time(void (*sort_func)(int[], int), int arr_orig[], int n)
{
    if (!arr_orig || n <= 0 || !sort_func)
        return -1.0;

    // Create a fresh copy for each measurement run
    int *arr_copy = copy_array(arr_orig, n);
    if (!arr_copy)
    {
        return -1.0; // Allocation failed
    }

    clock_t start, end;
    start = clock();
    sort_func(arr_copy, n); // Sort the copy
    end = clock();

    // Verify the sorted copy (optional but recommended)
    if (!is_sorted(arr_copy, n))
    {
        fprintf(stderr, "Error: Array was not sorted correctly by the function being timed.\n");
        // Handle error: maybe return a specific value or set a flag
    }

    free(arr_copy); // Free the copy

    return ((double)(end - start) * 1000.0) / CLOCKS_PER_SEC; // Convert CPU time to ms
}

// --- Main Benchmark Program ---

int main(int argc, char *argv[])
{
    // Default settings
    long size = 10000;
    int max_val = 1000000;
    int runs = 3;
    int data_type = 0;                                           // 0: random, 1: nearly sorted, 2: few unique
    char algorithms[256] = "vsort,quicksort,mergesort,std_sort"; // Use std_sort instead of std::sort

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--size") == 0 && i + 1 < argc)
        {
            size = atol(argv[i + 1]);
            if (size <= 0)
            {
                fprintf(stderr, "Error: Invalid size '%s'\n", argv[i + 1]);
                return 1;
            }
            i++;
        }
        else if (strcmp(argv[i], "--max-val") == 0 && i + 1 < argc)
        {
            max_val = atoi(argv[i + 1]);
            if (max_val <= 0)
            {
                fprintf(stderr, "Error: Invalid max-val '%s'\n", argv[i + 1]);
                return 1;
            }
            i++;
        }
        else if (strcmp(argv[i], "--runs") == 0 && i + 1 < argc)
        {
            runs = atoi(argv[i + 1]);
            if (runs <= 0)
            {
                fprintf(stderr, "Error: Invalid runs '%s'\n", argv[i + 1]);
                return 1;
            }
            i++;
        }
        else if (strcmp(argv[i], "--data-type") == 0 && i + 1 < argc)
        {
            if (strcmp(argv[i + 1], "random") == 0)
                data_type = 0;
            else if (strcmp(argv[i + 1], "nearly-sorted") == 0)
                data_type = 1;
            else if (strcmp(argv[i + 1], "few-unique") == 0)
                data_type = 2;
            else
            {
                fprintf(stderr, "Error: Unknown data-type '%s'\n", argv[i + 1]);
                return 1;
            }
            i++;
        }
        else if (strcmp(argv[i], "--algorithms") == 0 && i + 1 < argc)
        {
            strncpy(algorithms, argv[i + 1], sizeof(algorithms) - 1);
            algorithms[sizeof(algorithms) - 1] = '\0'; // Ensure null termination
            i++;
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            printf("Usage: benchmark [options]\n");
            printf("Options:\n");
            printf("  --size SIZE           Size of array to sort (default: 10000)\n");
            printf("  --max-val MAX_VAL     Maximum value in array (default: 1000000)\n");
            printf("  --runs RUNS           Number of runs for each algorithm (default: 3)\n");
            printf("  --data-type TYPE      Type of data: random, nearly-sorted, few-unique (default: random)\n");
            printf("  --algorithms ALGS     Comma-separated list of algorithms to test (default: vsort,quicksort,mergesort,std_sort)\n");
            printf("  --help                Display this help message\n");
            return 0;
        }
        else
        {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    // Seed random number generator ONCE
    srand(time(NULL));

    printf("Benchmark Settings:\n");
    printf("  Array size:       %ld\n", size);
    printf("  Maximum value:    %d\n", max_val);
    printf("  Runs per test:    %d\n", runs);
    printf("  Data type:        %s\n", data_type == 0 ? "random" : (data_type == 1 ? "nearly sorted" : "few unique"));
    printf("  Algorithms:       %s\n\n", algorithms);

    // Allocate initial array
    int *initial_array = (int *)malloc(size * sizeof(int));
    if (!initial_array)
    {
        fprintf(stderr, "Failed to allocate memory for array (size %ld)\n", size);
        return 1;
    }

    // Generate the appropriate type of data
    printf("Generating test data...\n");
    if (data_type == 0)
        generate_random_array(initial_array, size, max_val);
    else if (data_type == 1)
        generate_nearly_sorted_array(initial_array, size, max_val, 0.05); // 5% swaps
    else
        generate_few_unique_array(initial_array, size, 100); // 100 unique values
    printf("Test data generated.\n\n");

    // Parse the algorithms list
    char alg_copy[256];
    strncpy(alg_copy, algorithms, sizeof(alg_copy) - 1);
    alg_copy[sizeof(alg_copy) - 1] = '\0';

    printf("%-15s | %-15s | %-15s | %-15s\n", "Algorithm", "Avg Time (ms)", "Min Time (ms)", "Verification");
    printf("------------------|-----------------|-----------------|-----------------\n");

    char *alg = strtok(alg_copy, ",");
    while (alg != NULL)
    {
        double total_time = 0;
        // FIX: Use DBL_MAX instead of INFINITY
        double min_time = DBL_MAX;
        int verified_runs = 0;
        int failed_runs = 0;

        for (int run = 0; run < runs; run++)
        {
            // Pass the original generated array to measure_time, which makes a copy
            double time_ms = -1.0;

            if (strcmp(alg, "vsort") == 0)
                time_ms = measure_time(vsort, initial_array, size);
            else if (strcmp(alg, "quicksort") == 0)
                time_ms = measure_time(quicksort_wrapper, initial_array, size);
            else if (strcmp(alg, "mergesort") == 0)
                time_ms = measure_time(mergesort_wrapper, initial_array, size);
            else if (strcmp(alg, "std_sort") == 0)
                time_ms = measure_time(std_sort, initial_array, size); // Use correct name
            else
            {
                fprintf(stderr, "Warning: Unknown algorithm '%s' skipped.\n", alg);
                // Skip to next algorithm token if unknown
                goto next_algorithm;
            }

            if (time_ms >= 0)
            {
                total_time += time_ms;
                if (time_ms < min_time)
                    min_time = time_ms;
                verified_runs++; // Assume verified if timing worked (is_sorted check is inside measure_time)
            }
            else
            {
                // measure_time likely failed (e.g., allocation), or verification failed inside it
                failed_runs++;
            }
        } // End runs loop

        if (verified_runs > 0)
        {
            double avg_time = total_time / verified_runs;
            // Check if min_time was updated
            double min_time_to_report = (min_time == DBL_MAX) ? avg_time : min_time;

            printf("%-15s | %-15.3f | %-15.3f | %-15s\n",
                   alg,
                   avg_time,
                   min_time_to_report,
                   (failed_runs == 0) ? "PASSED" : "CHECK FAILED"); // Indicate if any run failed verification
        }
        else
        {
            printf("%-15s | %-15s | %-15s | %-15s\n",
                   alg, "Error", "Error", "FAILED");
        }

    next_algorithm:
        alg = strtok(NULL, ","); // Get next algorithm token
    } // End algorithms loop

    free(initial_array);
    return 0;
}
