/**
 * VSort: High-Performance Sorting Algorithm for Apple Silicon
 *
 * A sorting library specifically optimized for Apple Silicon processors,
 * leveraging ARM NEON vector instructions and Grand Central Dispatch.
 *
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// Apple Silicon specific includes
#if defined(__APPLE__) && defined(__arm64__)
#include <arm_neon.h>          // For NEON SIMD operations
#include <dispatch/dispatch.h> // For Grand Central Dispatch
#endif

// Performance-tuned thresholds
#define INSERTION_THRESHOLD 16    // Optimal for small arrays
#define PARALLEL_THRESHOLD 100000 // Threshold for parallel processing
#define VECTOR_THRESHOLD 64       // Threshold for vectorized operations

// Force inlining for critical functions - compiler-specific definitions
#if defined(_MSC_VER) // Microsoft Visual C++ Compiler
#define FORCE_INLINE static __forceinline
#elif defined(__GNUC__) || defined(__clang__) // GCC or Clang
#define FORCE_INLINE static inline __attribute__((always_inline))
#else
#define FORCE_INLINE static inline
#endif

// Simple utility functions
FORCE_INLINE void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

// Optimized insertion sort with branch optimization
FORCE_INLINE void insertion_sort(int arr[], int low, int high)
{
    for (int i = low + 1; i <= high; i++)
    {
        int key = arr[i];
        int j = i - 1;

        // Move elements greater than key to one position ahead
        while (j >= low && arr[j] > key)
        {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

// Simple but correct Lomuto partition scheme
FORCE_INLINE int partition(int arr[], int low, int high)
{
    // Select pivot as median of first, middle, and last elements
    int mid = low + (high - low) / 2;

    // Sort low, mid, high elements (3-element sort)
    if (arr[low] > arr[mid])
        swap(&arr[low], &arr[mid]);
    if (arr[mid] > arr[high])
        swap(&arr[mid], &arr[high]);
    if (arr[low] > arr[mid])
        swap(&arr[low], &arr[mid]);

    // Use middle value as pivot
    int pivot = arr[mid];

    // Move pivot to the end (will be restored later)
    swap(&arr[mid], &arr[high]);

    // Lomuto partition scheme
    int i = low - 1;

    for (int j = low; j <= high - 1; j++)
    {
        if (arr[j] <= pivot)
        {
            i++;
            swap(&arr[i], &arr[j]);
        }
    }

    // Put pivot in its final place
    swap(&arr[i + 1], &arr[high]);

    return (i + 1);
}

// Basic quicksort implementation - no optimization yet but correct
void quicksort(int arr[], int low, int high)
{
    // Create a stack for storing subarray boundaries - use dynamic allocation for MSVC compatibility
    int *stack = (int *)malloc((high - low + 1) * sizeof(int));
    if (!stack)
    {
        // Handle memory allocation failure
        fprintf(stderr, "Memory allocation failed in quicksort\n");
        return;
    }
    int top = -1;

    // Push initial values to stack
    stack[++top] = low;
    stack[++top] = high;

    // Keep popping from stack while it's not empty
    while (top >= 0)
    {
        // Pop high and low
        high = stack[top--];
        low = stack[top--];

        // Use insertion sort for small arrays
        if (high - low < INSERTION_THRESHOLD)
        {
            insertion_sort(arr, low, high);
            continue;
        }

        // Partition the array and get pivot position
        int p = partition(arr, low, high);

        // Push left subarray if there are elements on the left of pivot
        if (p - 1 > low)
        {
            stack[++top] = low;
            stack[++top] = p - 1;
        }

        // Push right subarray if there are elements on the right of pivot
        if (p + 1 < high)
        {
            stack[++top] = p + 1;
            stack[++top] = high;
        }
    }

    // Free the dynamically allocated memory
    free(stack);
}

// Main vsort entry point - simplified for correctness
void vsort(int arr[], int n)
{
    // Early exit for trivial cases
    if (n <= 1)
        return;

    quicksort(arr, 0, n - 1);
}

// Compatibility function
void adaptive_quick_sort(int arr[], int low, int high)
{
    quicksort(arr, low, high);
}

// Stub functions to maintain API compatibility
void bucket_sort(int arr[], int n) {}
void cache_aware_merge_sort(int arr[], int temp[], int low, int high) {}
void parallel_merge_sort(int arr[], int n) {}
void counting_sort(int arr[], int n, int min_val, int max_val) {}
int median_of_three(int arr[], int a, int b, int c) { return 0; }
int select_pivot(int arr[], int low, int high) { return 0; }
int partition_legacy(int arr[], int low, int high, int pivot) { return 0; }

// Implementation of vsort_with_comparator
void vsort_with_comparator(void *base, int n, size_t size, int (*compare)(const void *, const void *))
{
    qsort(base, n, size, compare);
}

// Forward declarations for comparator functions
static int default_float_comparator(const void *a, const void *b);
static int default_char_comparator(const void *a, const void *b);

// Implementation of vsort_float
void vsort_float(float arr[], int n)
{
    // This is a simple implementation - you may want to optimize for floats specifically
    qsort(arr, n, sizeof(float), default_float_comparator);
}

// Default float comparator
static int default_float_comparator(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;

    if (fa < fb)
        return -1;
    if (fa > fb)
        return 1;
    return 0;
}

// Implementation of vsort_char
void vsort_char(char arr[], int n)
{
    // This is a simple implementation - you may want to optimize for chars specifically
    qsort(arr, n, sizeof(char), default_char_comparator);
}

// Default char comparator
static int default_char_comparator(const void *a, const void *b)
{
    return (*(const char *)a - *(const char *)b);
}