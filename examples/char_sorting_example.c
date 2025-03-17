#include <stdio.h>
#include <stdlib.h>
#include "../vsort.h"

/**
 * Example demonstrating vsort with character arrays
 *
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 *
 */
int main(void)
{
    // Sample character array
    char arr[] = {'z', 'b', 'k', 'a', 'r', 'f', 'm', 'p', 'c', 'e'};
    int n = sizeof(arr) / sizeof(arr[0]);

    // Print unsorted array
    printf("Unsorted character array: ");
    for (int i = 0; i < n; i++)
    {
        printf("%c ", arr[i]);
    }
    printf("\n");

    // Sort the array using vsort_char instead of vsort
    vsort_char(arr, n);

    // Print sorted array
    printf("Sorted character array: ");
    for (int i = 0; i < n; i++)
    {
        printf("%c ", arr[i]);
    }
    printf("\n");

    return 0;
}
