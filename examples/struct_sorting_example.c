#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../vsort.h"

/**
 * Example demonstrating sorting of structures using vsort
 *
 * @author Davide Santangelo <https://github.com/davidesantangelo>
 * @license MIT
 *
 */

// Define a simple structure
typedef struct
{
    char name[50];
    int age;
    float height;
} Person;

// Comparator function for sorting by age
int compare_by_age(const void *a, const void *b)
{
    Person *personA = (Person *)a;
    Person *personB = (Person *)b;
    return personA->age - personB->age;
}

// Comparator function for sorting by name
int compare_by_name(const void *a, const void *b)
{
    Person *personA = (Person *)a;
    Person *personB = (Person *)b;
    return strcmp(personA->name, personB->name);
}

int main()
{
    // Create an array of Person structures
    Person people[] = {
        {"John", 25, 1.75},
        {"Alice", 22, 1.65},
        {"Bob", 30, 1.80},
        {"Eve", 20, 1.70},
        {"Charlie", 35, 1.85}};
    int n = sizeof(people) / sizeof(people[0]);

    // Print unsorted array of people
    printf("Unsorted people:\n");
    for (int i = 0; i < n; i++)
    {
        printf("%s, age %d, height %.2f\n", people[i].name, people[i].age, people[i].height);
    }
    printf("\n");

    // Sort by age
    vsort_with_comparator(people, n, sizeof(Person), compare_by_age);
    printf("People sorted by age:\n");
    for (int i = 0; i < n; i++)
    {
        printf("%s, age %d, height %.2f\n", people[i].name, people[i].age, people[i].height);
    }
    printf("\n");

    // Sort by name
    vsort_with_comparator(people, n, sizeof(Person), compare_by_name);
    printf("People sorted by name:\n");
    for (int i = 0; i < n; i++)
    {
        printf("%s, age %d, height %.2f\n", people[i].name, people[i].age, people[i].height);
    }
    printf("\n");

    return 0;
}
