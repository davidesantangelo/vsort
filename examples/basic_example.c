#include <stdio.h>
#include <stdlib.h>
#include "../vsort.h"

/**
 * Basic example demonstrating the usage of VSort
 *
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 */
int main()
{
    // Sample array for testing
    int arr[] = {9, 3, 5, 1, 8, 2, 7, 6, 4, 0};
    int n = sizeof(arr) / sizeof(arr[0]);

    // Print unsorted array
    printf("Unsorted array: ");
    for (int i = 0; i < n; i++)
    {
        printf("%d ", arr[i]);
    }
    printf("\n");

    // Sort the array using vsort
    vsort(arr, n);

    // Print sorted array
    printf("Sorted array: ");
    for (int i = 0; i < n; i++)
    {
        printf("%d ", arr[i]);
    }
    printf("\n");

    return 0;
}
