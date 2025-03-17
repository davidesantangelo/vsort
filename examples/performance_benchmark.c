#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Add this header for memcpy
#include <time.h>
#include "../vsort.h"

/**
 * Performance benchmark comparing vsort with standard qsort
 *
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 *
 */

// Standard comparison function for integers
int compare_int(const void *a, const void *b)
{
    return (*(int *)a - *(int *)b);
}

// Function to generate random array
void generate_random_array(int arr[], int n)
{
    for (int i = 0; i < n; i++)
    {
        arr[i] = rand() % 10000;
    }
}

int main()
{
    srand(time(NULL));
    clock_t start, end;
    double cpu_time_used;

    // Test with different array sizes
    int sizes[] = {1000, 10000, 100000};

    for (int s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
    {
        int n = sizes[s];
        printf("Testing with array size: %d\n", n);

        // Allocate memory for arrays
        int *arr1 = (int *)malloc(n * sizeof(int));
        int *arr2 = (int *)malloc(n * sizeof(int));

        // Generate the same random array for both tests
        generate_random_array(arr1, n);
        memcpy(arr2, arr1, n * sizeof(int));

        // Benchmark vsort
        start = clock();
        vsort(arr1, n);
        end = clock();
        cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
        printf("vsort time: %f seconds\n", cpu_time_used);

        // Benchmark standard qsort
        start = clock();
        qsort(arr2, n, sizeof(int), compare_int);
        end = clock();
        cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
        printf("qsort time: %f seconds\n", cpu_time_used);

        // Verify both sorts produced the same result
        int is_same = 1;
        for (int i = 0; i < n; i++)
        {
            if (arr1[i] != arr2[i])
            {
                is_same = 0;
                break;
            }
        }
        printf("Results are %s\n\n", is_same ? "identical" : "different");

        // Free memory
        free(arr1);
        free(arr2);
    }

    return 0;
}
