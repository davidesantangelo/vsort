/**
 * test_basic.c - Basic functionality tests for vsort
 *
 * These tests verify that vsort correctly sorts arrays of different
 * sizes and patterns.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../vsort.h"

// Test helper functions
static int *create_random_array(int n, int max_val)
{
    int *arr = (int *)malloc(n * sizeof(int));
    if (!arr)
        return NULL;

    for (int i = 0; i < n; i++)
    {
        arr[i] = rand() % max_val;
    }
    return arr;
}

static int *create_sorted_array(int n)
{
    int *arr = (int *)malloc(n * sizeof(int));
    if (!arr)
        return NULL;

    for (int i = 0; i < n; i++)
    {
        arr[i] = i;
    }
    return arr;
}

static int *create_reverse_sorted_array(int n)
{
    int *arr = (int *)malloc(n * sizeof(int));
    if (!arr)
        return NULL;

    for (int i = 0; i < n; i++)
    {
        arr[i] = n - i - 1;
    }
    return arr;
}

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

// Test cases
static int test_sort_random_array()
{
    printf("Testing sorting random array... ");

    int sizes[] = {0, 1, 10, 100, 1000};
    for (int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        int n = sizes[i];
        int *arr = create_random_array(n, 1000);
        if (n > 0 && !arr)
        {
            printf("FAILED: Memory allocation error\n");
            return 0;
        }

        vsort(arr, n);

        if (!is_sorted(arr, n))
        {
            printf("FAILED: Array of size %d not sorted correctly\n", n);
            free(arr);
            return 0;
        }

        free(arr);
    }

    printf("PASSED\n");
    return 1;
}

static int test_sort_already_sorted_array()
{
    printf("Testing sorting already sorted array... ");

    int sizes[] = {0, 1, 10, 100, 1000};
    for (int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        int n = sizes[i];
        int *arr = create_sorted_array(n);
        if (n > 0 && !arr)
        {
            printf("FAILED: Memory allocation error\n");
            return 0;
        }

        vsort(arr, n);

        if (!is_sorted(arr, n))
        {
            printf("FAILED: Array of size %d not sorted correctly\n", n);
            free(arr);
            return 0;
        }

        free(arr);
    }

    printf("PASSED\n");
    return 1;
}

static int test_sort_reverse_sorted_array()
{
    printf("Testing sorting reverse sorted array... ");

    int sizes[] = {0, 1, 10, 100, 1000};
    for (int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        int n = sizes[i];
        int *arr = create_reverse_sorted_array(n);
        if (n > 0 && !arr)
        {
            printf("FAILED: Memory allocation error\n");
            return 0;
        }

        vsort(arr, n);

        if (!is_sorted(arr, n))
        {
            printf("FAILED: Array of size %d not sorted correctly\n", n);
            free(arr);
            return 0;
        }

        free(arr);
    }

    printf("PASSED\n");
    return 1;
}

static int test_sort_duplicate_values()
{
    printf("Testing sorting array with duplicate values... ");

    int test_array[] = {5, 2, 9, 1, 5, 6, 3, 5, 8, 9, 7, 5};
    int n = sizeof(test_array) / sizeof(test_array[0]);

    // Make a copy to sort
    int *arr = (int *)malloc(n * sizeof(int));
    if (!arr)
    {
        printf("FAILED: Memory allocation error\n");
        return 0;
    }

    memcpy(arr, test_array, n * sizeof(int));
    vsort(arr, n);

    if (!is_sorted(arr, n))
    {
        printf("FAILED: Array with duplicates not sorted correctly\n");
        free(arr);
        return 0;
    }

    free(arr);
    printf("PASSED\n");
    return 1;
}

static int test_edge_cases()
{
    printf("Testing edge cases... ");

    // Empty array
    vsort(NULL, 0);

    // Single element
    int single = 42;
    vsort(&single, 1);

    printf("PASSED\n");
    return 1;
}

int main()
{
    printf("Running basic vsort tests...\n\n");

    srand(time(NULL));

    // Run all tests
    int passed = 1;
    passed &= test_sort_random_array();
    passed &= test_sort_already_sorted_array();
    passed &= test_sort_reverse_sorted_array();
    passed &= test_sort_duplicate_values();
    passed &= test_edge_cases();

    printf("\nTest summary: %s\n", passed ? "ALL PASSED" : "SOME TESTS FAILED");

    return passed ? 0 : 1;
}
