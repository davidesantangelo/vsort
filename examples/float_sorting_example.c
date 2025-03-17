#include <stdio.h>
#include <stdlib.h>
#include "../vsort.h"

/**
 * Example demonstrating vsort with floating point numbers
 *
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 *
 */
int main(void)
{
    // Sample array of floats
    float arr[] = {9.5, 3.1, 5.7, 1.2, 8.9, 2.3, 7.6, 6.4, 4.8, 0.5};
    int n = sizeof(arr) / sizeof(arr[0]);

    // Print unsorted array
    printf("Unsorted float array: ");
    for (int i = 0; i < n; i++)
    {
        printf("%.1f ", arr[i]);
    }
    printf("\n");

    // Sort the array using vsort_float instead of vsort
    vsort_float(arr, n);

    // Print sorted array
    printf("Sorted float array: ");
    for (int i = 0; i < n; i++)
    {
        printf("%.1f ", arr[i]);
    }
    printf("\n");

    return 0;
}
