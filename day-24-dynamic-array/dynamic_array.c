/* dynamic_array.c
 *
 * A dynamic array (growable array) implementation in C.
 *
 * Unlike a fixed-size array, a dynamic array can grow
 * as needed. It doubles its capacity when full, giving
 * amortized O(1) appends.
 *
 * Supports:
 *   - Create / destroy
 *   - Append
 *   - Get / set by index
 *   - Insert / remove at index
 *   - Search
 *   - Print
 *   - Reserve capacity
 *   - Shrink to fit
 *
 * Usage:
 *   ./dynamic_array
 *
 * Skills practiced:
 *   - Structs
 *   - Dynamic memory (malloc, realloc, free)
 *   - Amortized analysis
 *   - Array manipulation
 *   - Error handling
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* A dynamic array of ints */
typedef struct {
    int *data;       /* pointer to the heap block */
    int size;        /* number of elements in use */
    int capacity;    /* number of elements allocated */
} DynArray;

/* Create a new dynamic array with initial capacity */
DynArray *da_create(int initial_capacity)
{
    if (initial_capacity < 1)
        initial_capacity = 1;

    DynArray *da = malloc(sizeof(DynArray));
    if (da == NULL)
    {
        fprintf(stderr, "Error: malloc failed.\n");
        exit(EXIT_FAILURE);
    }

    da->data = malloc(initial_capacity * sizeof(int));
    if (da->data == NULL)
    {
        fprintf(stderr, "Error: malloc failed.\n");
        free(da);
        exit(EXIT_FAILURE);
    }

    da->size = 0;
    da->capacity = initial_capacity;
    return da;
}

/* Free the dynamic array */
void da_destroy(DynArray *da)
{
    if (da == NULL)
        return;
    free(da->data);
    free(da);
}

/* Grow the capacity (double it) */
static void da_grow(DynArray *da)
{
    int new_capacity = da->capacity * 2;
    int *new_data = realloc(da->data, new_capacity * sizeof(int));

    if (new_data == NULL)
    {
        fprintf(stderr, "Error: realloc failed.\n");
        exit(EXIT_FAILURE);
    }

    da->data = new_data;
    da->capacity = new_capacity;
}

/* Append a value to the end — amortized O(1) */
void da_append(DynArray *da, int value)
{
    if (da->size == da->capacity)
        da_grow(da);

    da->data[da->size++] = value;
}

/* Get value at index — O(1) */
int da_get(const DynArray *da, int index)
{
    if (index < 0 || index >= da->size)
    {
        fprintf(stderr, "Error: index %d out of bounds.\n", index);
        exit(EXIT_FAILURE);
    }
    return da->data[index];
}

/* Set value at index — O(1) */
void da_set(DynArray *da, int index, int value)
{
    if (index < 0 || index >= da->size)
    {
        fprintf(stderr, "Error: index %d out of bounds.\n", index);
        exit(EXIT_FAILURE);
    }
    da->data[index] = value;
}

/* Insert at index — O(n) */
void da_insert(DynArray *da, int index, int value)
{
    if (index < 0 || index > da->size)
    {
        fprintf(stderr, "Error: index %d out of bounds.\n", index);
        exit(EXIT_FAILURE);
    }

    if (da->size == da->capacity)
        da_grow(da);

    /* Shift elements right */
    for (int i = da->size; i > index; i--)
        da->data[i] = da->data[i - 1];

    da->data[index] = value;
    da->size++;
}

/* Remove at index — O(n) */
int da_remove(DynArray *da, int index)
{
    if (index < 0 || index >= da->size)
    {
        fprintf(stderr, "Error: index %d out of bounds.\n", index);
        exit(EXIT_FAILURE);
    }

    int removed = da->data[index];

    /* Shift elements left */
    for (int i = index; i < da->size - 1; i++)
        da->data[i] = da->data[i + 1];

    da->size--;
    return removed;
}

/* Linear search — O(n) */
int da_index_of(const DynArray *da, int value)
{
    for (int i = 0; i < da->size; i++)
        if (da->data[i] == value)
            return i;
    return -1;
}

/* Print the array */
void da_print(const DynArray *da)
{
    printf("[");
    for (int i = 0; i < da->size; i++)
    {
        printf("%d", da->data[i]);
        if (i < da->size - 1)
            printf(", ");
    }
    printf("]\n");
}

/* Reserve capacity */
void da_reserve(DynArray *da, int new_capacity)
{
    if (new_capacity <= da->capacity)
        return;

    int *new_data = realloc(da->data, new_capacity * sizeof(int));
    if (new_data == NULL)
    {
        fprintf(stderr, "Error: realloc failed.\n");
        exit(EXIT_FAILURE);
    }

    da->data = new_data;
    da->capacity = new_capacity;
}

/* Shrink to fit */
void da_shrink_to_fit(DynArray *da)
{
    if (da->size == da->capacity)
        return;

    int new_capacity = (da->size > 0) ? da->size : 1;
    int *new_data = realloc(da->data, new_capacity * sizeof(int));

    if (new_data == NULL)
    {
        fprintf(stderr, "Error: realloc failed.\n");
        exit(EXIT_FAILURE);
    }

    da->data = new_data;
    da->capacity = new_capacity;
}

int main(void)
{
    printf("====================================\n");
    printf("        DYNAMIC ARRAY               \n");
    printf("====================================\n\n");

    DynArray *da = da_create(2);

    printf("Appending: 10, 20, 30, 40, 50\n");
    da_append(da, 10);
    da_append(da, 20);
    da_append(da, 30);
    da_append(da, 40);
    da_append(da, 50);
    printf("Array:    ");
    da_print(da);
    printf("Size:     %d\n", da->size);
    printf("Capacity: %d\n\n", da->capacity);

    printf("Inserting 15 at index 1\n");
    da_insert(da, 1, 15);
    da_print(da);
    printf("\n");

    printf("Removing at index 3: %d\n", da_remove(da, 3));
    da_print(da);
    printf("\n");

    printf("Index of 40: %d\n", da_index_of(da, 40));
    printf("Index of 99: %d\n\n", da_index_of(da, 99));

    printf("Setting index 0 to 99\n");
    da_set(da, 0, 99);
    da_print(da);
    printf("\n");

    printf("Shrinking to fit\n");
    da_shrink_to_fit(da);
    printf("Size:     %d\n", da->size);
    printf("Capacity: %d\n", da->capacity);

    da_destroy(da);
    return 0;
}
