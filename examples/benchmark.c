#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "../vsort.h"

// Implementation of other sorting algorithms for comparison
void custom_quicksort(int arr[], int low, int high);
int quick_partition(int arr[], int low, int high);
void custom_mergesort(int arr[], int low, int high);
void merge_arrays(int arr[], int low, int mid, int high);
void std_sort(int arr[], int n); // Using qsort from stdlib

// Wrapper functions for measuring performance
void quicksort_wrapper(int arr[], int n);
void mergesort_wrapper(int arr[], int n);

// Utility functions
void generate_random_array(int arr[], int n, int max_val);
void generate_nearly_sorted_array(int arr[], int n, int max_val, double swap_ratio);
void generate_few_unique_array(int arr[], int n, int unique_vals);
int *copy_array(int arr[], int n);
int is_sorted(int arr[], int n);
double measure_time(void (*sort_func)(int[], int), int arr[], int n);

/**
 * Wrapper function for quicksort
 */
void quicksort_wrapper(int arr[], int n)
{
    custom_quicksort(arr, 0, n - 1);
}

/**
 * Wrapper function for mergesort
 */
void mergesort_wrapper(int arr[], int n)
{
    custom_mergesort(arr, 0, n - 1);
}

/**
 * Main benchmark program
 */
int main(int argc, char *argv[])
{
    // Default settings
    long size = 10000;
    int max_val = 1000000;
    int runs = 3;
    int data_type = 0; // 0: random, 1: nearly sorted, 2: few unique
    char algorithms[256] = "vsort,quicksort,mergesort,std::sort";

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--size") == 0 && i + 1 < argc)
        {
            size = atol(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "--max-val") == 0 && i + 1 < argc)
        {
            max_val = atoi(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "--runs") == 0 && i + 1 < argc)
        {
            runs = atoi(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "--data-type") == 0 && i + 1 < argc)
        {
            if (strcmp(argv[i + 1], "random") == 0)
            {
                data_type = 0;
            }
            else if (strcmp(argv[i + 1], "nearly-sorted") == 0)
            {
                data_type = 1;
            }
            else if (strcmp(argv[i + 1], "few-unique") == 0)
            {
                data_type = 2;
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
            printf("  --algorithms ALGS     Comma-separated list of algorithms to test (default: vsort,quicksort,mergesort,std::sort)\n");
            printf("  --help                Display this help message\n");
            return 0;
        }
    }

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
        fprintf(stderr, "Failed to allocate memory for array\n");
        return 1;
    }

    // Generate the appropriate type of data
    printf("Generating test data...\n");
    if (data_type == 0)
    {
        generate_random_array(initial_array, size, max_val);
    }
    else if (data_type == 1)
    {
        generate_nearly_sorted_array(initial_array, size, max_val, 0.05); // 5% swaps
    }
    else
    {
        generate_few_unique_array(initial_array, size, 100); // 100 unique values
    }
    printf("Test data generated.\n\n");

    // Parse the algorithms list
    char alg_copy[256];
    strncpy(alg_copy, algorithms, sizeof(alg_copy) - 1);
    alg_copy[sizeof(alg_copy) - 1] = '\0';

    printf("%-15s %-15s %-15s %-15s\n", "Algorithm", "Avg Time (ms)", "Min Time (ms)", "Verification");
    printf("--------------------------------------------------------------\n");

    char *alg = strtok(alg_copy, ",");
    while (alg != NULL)
    {
        double total_time = 0;
        double min_time = INFINITY;
        int verified = 1;

        for (int run = 0; run < runs; run++)
        {
            int *arr = copy_array(initial_array, size);
            if (!arr)
            {
                fprintf(stderr, "Failed to allocate memory for test array\n");
                free(initial_array);
                return 1;
            }

            double time_ms;

            if (strcmp(alg, "vsort") == 0)
            {
                time_ms = measure_time(vsort, arr, size);
            }
            else if (strcmp(alg, "quicksort") == 0)
            {
                time_ms = measure_time(quicksort_wrapper, arr, size);
            }
            else if (strcmp(alg, "mergesort") == 0)
            {
                time_ms = measure_time(mergesort_wrapper, arr, size);
            }
            else if (strcmp(alg, "std::sort") == 0)
            {
                time_ms = measure_time(std_sort, arr, size);
            }
            else
            {
                fprintf(stderr, "Unknown algorithm: %s\n", alg);
                free(arr);
                continue;
            }

            // Verify the array is sorted
            if (!is_sorted(arr, size))
            {
                verified = 0;
            }

            total_time += time_ms;
            if (time_ms < min_time)
                min_time = time_ms;

            free(arr);
        }

        double avg_time = total_time / runs;
        printf("%-15s %-15.2f %-15.2f %-15s\n", alg, avg_time, min_time, verified ? "PASSED" : "FAILED");

        alg = strtok(NULL, ",");
    }

    free(initial_array);
    return 0;
}

/**
 * Measure execution time of a sorting function
 */
double measure_time(void (*sort_func)(int[], int), int arr[], int n)
{
    clock_t start, end;

    start = clock();
    sort_func(arr, n);
    end = clock();

    return ((double)(end - start) * 1000.0) / CLOCKS_PER_SEC; // Convert to ms
}

/**
 * Generate an array with random values
 */
void generate_random_array(int arr[], int n, int max_val)
{
    srand(time(NULL));
    for (int i = 0; i < n; i++)
    {
        arr[i] = rand() % max_val;
    }
}

/**
 * Generate a nearly sorted array by starting with a sorted array
 * and randomly swapping some elements
 */
void generate_nearly_sorted_array(int arr[], int n, int max_val, double swap_ratio)
{
    // First create a sorted array
    for (int i = 0; i < n; i++)
    {
        arr[i] = i * (max_val / (double)n);
    }

    // Then swap some elements randomly
    int swaps = (int)(n * swap_ratio);
    srand(time(NULL));
    for (int i = 0; i < swaps; i++)
    {
        int idx1 = rand() % n;
        int idx2 = rand() % n;
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
    srand(time(NULL));
    for (int i = 0; i < n; i++)
    {
        arr[i] = rand() % unique_vals;
    }
}

/**
 * Create a copy of the given array
 */
int *copy_array(int arr[], int n)
{
    int *copy = (int *)malloc(n * sizeof(int));
    if (copy)
    {
        memcpy(copy, arr, n * sizeof(int));
    }
    return copy;
}

/**
 * Check if an array is sorted
 */
int is_sorted(int arr[], int n)
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

/**
 * Quick sort implementation for comparison
 */
void custom_quicksort(int arr[], int low, int high)
{
    if (low < high)
    {
        int pivot = quick_partition(arr, low, high);
        custom_quicksort(arr, low, pivot - 1);
        custom_quicksort(arr, pivot + 1, high);
    }
}

int quick_partition(int arr[], int low, int high)
{
    int pivot = arr[high];
    int i = low - 1;

    for (int j = low; j <= high - 1; j++)
    {
        if (arr[j] <= pivot)
        {
            i++;
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }

    int temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;

    return i + 1;
}

/**
 * Merge sort implementation for comparison
 */
void custom_mergesort(int arr[], int low, int high)
{
    if (low < high)
    {
        int mid = low + (high - low) / 2;
        custom_mergesort(arr, low, mid);
        custom_mergesort(arr, mid + 1, high);
        merge_arrays(arr, low, mid, high);
    }
}

void merge_arrays(int arr[], int low, int mid, int high)
{
    int n1 = mid - low + 1;
    int n2 = high - mid;

    // Create temp arrays
    int *left = (int *)malloc(n1 * sizeof(int));
    int *right = (int *)malloc(n2 * sizeof(int));

    if (!left || !right)
    {
        if (left)
            free(left);
        if (right)
            free(right);
        fprintf(stderr, "Memory allocation failed in merge_arrays\n");
        return;
    }

    // Copy data to temp arrays
    for (int i = 0; i < n1; i++)
    {
        left[i] = arr[low + i];
    }
    for (int j = 0; j < n2; j++)
    {
        right[j] = arr[mid + 1 + j];
    }

    // Merge the temp arrays back
    int i = 0, j = 0, k = low;
    while (i < n1 && j < n2)
    {
        if (left[i] <= right[j])
        {
            arr[k] = left[i];
            i++;
        }
        else
        {
            arr[k] = right[j];
            j++;
        }
        k++;
    }

    // Copy remaining elements
    while (i < n1)
    {
        arr[k] = left[i];
        i++;
        k++;
    }
    while (j < n2)
    {
        arr[k] = right[j];
        j++;
        k++;
    }

    free(left);
    free(right);
}

/**
 * std::sort (using C qsort) implementation for comparison
 */
int compare_ints(const void *a, const void *b)
{
    return (*(int *)a - *(int *)b);
}

void std_sort(int arr[], int n)
{
    qsort(arr, n, sizeof(int), compare_ints);
}
