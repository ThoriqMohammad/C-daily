/* hash_table.c
 *
 * A hash table implementation in C using separate chaining.
 *
 * A hash table maps keys to values in O(1) average time.
 * It uses a hash function to compute an index into an array
 * of buckets. Collisions are handled with linked lists.
 *
 * Supports:
 *   - Insert / update
 *   - Lookup
 *   - Delete
 *   - Print
 *   - Free
 *
 * Usage:
 *   ./hash_table
 *
 * Skills practiced:
 *   - Structs
 *   - Linked lists
 *   - Hashing (djb2)
 *   - Dynamic memory (malloc, free)
 *   - Key-value data structures
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TABLE_SIZE 16

/* A single key-value pair */
typedef struct Entry {
    char *key;
    int value;
    struct Entry *next;   /* chaining */
} Entry;

/* The hash table */
typedef struct {
    Entry *buckets[TABLE_SIZE];
    int count;
} HashTable;

/* djb2 hash function */
static unsigned long hash(const char *key)
{
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char) *key++))
        h = ((h << 5) + h) + (unsigned long) c;
    return h;
}

/* Create a new hash table */
HashTable *ht_create(void)
{
    HashTable *ht = malloc(sizeof(HashTable));
    if (ht == NULL) return NULL;

    for (int i = 0; i < TABLE_SIZE; i++)
        ht->buckets[i] = NULL;

    ht->count = 0;
    return ht;
}

/* Free the hash table */
void ht_free(HashTable *ht)
{
    if (ht == NULL) return;

    for (int i = 0; i < TABLE_SIZE; i++)
    {
        Entry *e = ht->buckets[i];
        while (e != NULL)
        {
            Entry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(ht);
}

/* Insert or update a key-value pair */
bool ht_put(HashTable *ht, const char *key, int value)
{
    unsigned long index = hash(key) % TABLE_SIZE;

    /* Check if key already exists */
    for (Entry *e = ht->buckets[index]; e != NULL; e = e->next)
    {
        if (strcmp(e->key, key) == 0)
        {
            e->value = value;   /* update */
            return true;
        }
    }

    /* Create a new entry */
    Entry *e = malloc(sizeof(Entry));
    if (e == NULL) return false;

    e->key = malloc(strlen(key) + 1);
    if (e->key == NULL)
    {
        free(e);
        return false;
    }

    strcpy(e->key, key);
    e->value = value;
    e->next = ht->buckets[index];
    ht->buckets[index] = e;
    ht->count++;
    return true;
}

/* Look up a key. Returns true if found, stores value in *out. */
bool ht_get(const HashTable *ht, const char *key, int *out)
{
    unsigned long index = hash(key) % TABLE_SIZE;

    for (Entry *e = ht->buckets[index]; e != NULL; e = e->next)
    {
        if (strcmp(e->key, key) == 0)
        {
            if (out != NULL) *out = e->value;
            return true;
        }
    }
    return false;
}

/* Delete a key. Returns true if removed. */
bool ht_delete(HashTable *ht, const char *key)
{
    unsigned long index = hash(key) % TABLE_SIZE;
    Entry *e = ht->buckets[index];
    Entry *prev = NULL;

    while (e != NULL)
    {
        if (strcmp(e->key, key) == 0)
        {
            if (prev == NULL)
                ht->buckets[index] = e->next;
            else
                prev->next = e->next;

            free(e->key);
            free(e);
            ht->count--;
            return true;
        }
        prev = e;
        e = e->next;
    }
    return false;
}

/* Print the hash table */
void ht_print(const HashTable *ht)
{
    printf("Hash table (%d entries):\n", ht->count);
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        if (ht->buckets[i] == NULL)
            continue;

        printf("  [%2d] ", i);
        for (Entry *e = ht->buckets[i]; e != NULL; e = e->next)
        {
            printf("%s=%d", e->key, e->value);
            if (e->next != NULL) printf(" -> ");
        }
        printf("\n");
    }
}

int main(void)
{
    printf("====================================\n");
    printf("          HASH TABLE                \n");
    printf("====================================\n\n");

    HashTable *ht = ht_create();
    if (ht == NULL)
    {
        fprintf(stderr, "Error: could not create hash table.\n");
        return 1;
    }

    /* Insert some values */
    const char *keys[] = {"apple", "banana", "cherry", "date", "elderberry",
                          "fig", "grape", "honeydew"};
    int values[] = {10, 20, 30, 40, 50, 60, 70, 80};

    for (int i = 0; i < 8; i++)
        ht_put(ht, keys[i], values[i]);

    ht_print(ht);

    /* Lookups */
    printf("\nLookups:\n");
    int value;
    if (ht_get(ht, "cherry", &value))
        printf("  cherry = %d\n", value);
    if (ht_get(ht, "fig", &value))
        printf("  fig    = %d\n", value);
    if (!ht_get(ht, "kiwi", &value))
        printf("  kiwi   = not found\n");

    /* Update */
    printf("\nUpdating banana to 99\n");
    ht_put(ht, "banana", 99);
    if (ht_get(ht, "banana", &value))
        printf("  banana = %d\n", value);

    /* Delete */
    printf("\nDeleting cherry\n");
    ht_delete(ht, "cherry");
    if (!ht_get(ht, "cherry", &value))
        printf("  cherry = not found\n");

    printf("\n");
    ht_print(ht);

    ht_free(ht);
    return 0;
}
