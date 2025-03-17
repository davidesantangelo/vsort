/**
 * VSort: High-Performance Sorting Algorithm for Apple Silicon
 *
 * Version 0.1.1
 *
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 */

#ifndef VSORT_H
#define VSORT_H

#include <stddef.h>

/**
 * Version information
 */
#define VSORT_VERSION_MAJOR 0
#define VSORT_VERSION_MINOR 1
#define VSORT_VERSION_PATCH 0
#define VSORT_VERSION_STRING "0.1.1"

/**
 * HyperSort - A revolutionary sorting algorithm
 *
 * HyperSort analyzes the input data and automatically selects the optimal
 * sorting strategy based on array size, data distribution, and hardware
 * characteristics. It combines multiple sorting techniques with advanced
 * optimizations to achieve superior performance across a wide range of inputs.
 *
 * @param arr Array to be sorted
 * @param n Length of the array
 */
void vsort(int arr[], int n);

/*
 * Generic sorting function with custom comparator
 * Takes a comparison function pointer similar to qsort
 */
void vsort_with_comparator(void *arr, int n, size_t size, int (*compare)(const void *, const void *));

/*
 * Type-specific sorting functions
 */
void vsort_float(float arr[], int n);
void vsort_char(char arr[], int n);

#endif /* VSORT */
