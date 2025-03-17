#include <stdio.h>
#include <stdlib.h>
#include "../vsort.h"

/**
 * Custom comparator function for descending order
 *
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 *
 */
int descending_compare(const void *a, const void *b)
{
    return (*(int *)b - *(int *)a);
}

/**
 * Example demonstrating vsort with a custom comparator
 */
int main()
{
    // Sample array for testing
    int arr[] = {9, 3, 5, 1, 8, 2, 7, 6, 4, 0};
    int n = sizeof(arr) / sizeof(arr[0]);

    // Print original array
    printf("Original array: ");
    for (int i = 0; i < n; i++)
    {
        printf("%d ", arr[i]);
    }
    printf("\n");

    // Sort the array using vsort with custom comparator
    vsort_with_comparator(arr, n, sizeof(int), descending_compare);

    // Print sorted array (descending order)
    printf("Sorted in descending order: ");
    for (int i = 0; i < n; i++)
    {
        printf("%d ", arr[i]);
    }
    printf("\n");

    return 0;
}
