/* binary_search.c
 *
 * Implements binary search on a sorted array of integers.
 *
 * Binary search finds a target value in O(log n) time by
 * repeatedly halving the search interval.
 *
 * The array MUST be sorted.
 *
 * Usage:
 *   ./binary_search <target> <n> <value1> <value2> ...
 *
 * Examples:
 *   ./binary_search 7 10 1 3 5 7 9 11 13 15 17 19
 *   ./binary_search 4 10 1 3 5 7 9 11 13 15 17 19
 *
 * Skills practiced:
 *   - Binary search algorithm
 *   - Sorting (qsort)
 *   - Arrays and pointers
 *   - argc / argv
 *   - Algorithmic thinking (O(log n))
 */

#include <stdio.h>
#include <stdlib.h>

/* Binary search: returns index of target, or -1 if not found */
int binary_search(const int arr[], int n, int target)
{
    int low = 0;
    int high = n - 1;

    while (low <= high)
    {
        int mid = low + ((high - low) / 2);

        if (arr[mid] == target)
            return mid;
        else if (arr[mid] < target)
            low = mid + 1;
        else
            high = mid - 1;
    }

    return -1;
}

/* Recursive binary search */
int binary_search_rec(const int arr[], int low, int high, int target)
{
    if (low > high)
        return -1;

    int mid = low + ((high - low) / 2);

    if (arr[mid] == target)
        return mid;
    else if (arr[mid] < target)
        return binary_search_rec(arr, mid + 1, high, target);
    else
        return binary_search_rec(arr, low, mid - 1, target);
}

/* Comparison function for qsort */
int compare(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <target> <n> <value1> <value2> ...\n", argv[0]);
        return 1;
    }

    int target = atoi(argv[1]);
    int n = atoi(argv[2]);

    if (n <= 0 || argc != n + 3)
    {
        fprintf(stderr, "Error: n must match the number of values.\n");
        return 1;
    }

    /* Allocate array */
    int *arr = malloc(n * sizeof(int));
    if (arr == NULL)
    {
        fprintf(stderr, "Error: malloc failed.\n");
        return 1;
    }

    /* Read values from argv */
    for (int i = 0; i < n; i++)
        arr[i] = atoi(argv[i + 3]);

    /* Sort the array */
    qsort(arr, n, sizeof(int), compare);

    /* Print the sorted array */
    printf("Sorted array: ");
    for (int i = 0; i < n; i++)
        printf("%d ", arr[i]);
    printf("\n");

    /* Iterative binary search */
    int index = binary_search(arr, n, target);
    if (index >= 0)
        printf("Iterative: found %d at index %d\n", target, index);
    else
        printf("Iterative: %d not found\n", target);

    /* Recursive binary search */
    index = binary_search_rec(arr, 0, n - 1, target);
    if (index >= 0)
        printf("Recursive: found %d at index %d\n", target, index);
    else
        printf("Recursive: %d not found\n", target);

    free(arr);
    return 0;
}
