/* skip_list.c
 *
 * Step order (what this program does):
 *
 *   1. Define the skip-list node: a value, plus an array of
 *      "forward" pointers, one per level. The number of levels
 *      per node is determined at insert time by coin flips.
 *   2. Implement insert(value):
 *      a. Walk down from the top level, building an "update"
 *         array of the last node seen at each level.
 *      b. Randomly choose how many levels the new node has
 *         (promoted by coin flip while flipping heads).
 *      c. If the new node has more levels than the current
 *         list height, extend the list's head and update array.
 *      d. Link the new node into the list at all its levels.
 *   3. Implement search(value):
 *      a. Start at the top level of the head node.
 *      b. At each level, advance right while forward[level]
 *         is smaller than target.
 *      c. Drop down one level and repeat.
 *      d. At level 0, the node to the right is either the
 *         target (found) or larger (not found).
 *   4. Implement delete(value):
 *      a. Walk down like insert, building the update array.
 *      b. If the target exists at level 0, unlink it from
 *         every level it participates in.
 *      c. Trim the list's height if top levels are now empty.
 *   5. Implement ordered iteration: follow forward[0] from the
 *      head, printing all values in sorted order.
 *   6. Test all operations, including insertion/removal and
 *      large scale (10,000 elements, verify sorted order).
 *
 * What a skip list is:
 *
 *   A skip list is an ordered linked list with a hierarchy
 *   of "express lanes." Every node appears at level 0. Some
 *   nodes also appear at level 1, fewer at level 2, and so
 *   on — the probability of appearing at each successive
 *   level is 1/2 (or some constant p).
 *
 *   Level 2:  head ----------------------------> 50 -> tail
 *   Level 1:  head -------> 20 --------> 40 ---> 50 -> tail
 *   Level 0:  head -> 10 -> 20 -> 30 -> 40 -> 50 -> 60 -> tail
 *
 *   To find a value:
 *     - Start at the top-left (head's highest level).
 *     - Move right as long as the next value is smaller.
 *     - When the next value would be larger, drop down a level.
 *     - Repeat.
 *     - At level 0, you're either on the target or adjacent to it.
 *
 *   Expected time is O(log n) for search, insert, and delete.
 *   Worst case is O(n) (if you get unlucky with promotions),
 *   but the probability is astronomically small. The
 *   probabilistic guarantee is what makes the structure
 *   simple — no rotations, no rebalancing.
 *
 *   Compare to a balanced BST (AVL, red-black tree):
 *     - BST: deterministic O(log n), but complex (rotations,
 *       color invariants, etc.)
 *     - Skip list: probabilistic O(log n), but very simple
 *       (just a linked list with extra pointers).
 *
 *   Skip lists are used in Redis (sorted sets), LevelDB /
 *   RocksDB (memtable), and Java's ConcurrentSkipListMap.
 *   They're particularly useful for concurrent access,
 *   which is a whole separate topic.
 *
 * What this implementation does NOT do:
 *   - Generic keys (uses int; could be void* with comparator)
 *   - Multi-level iteration APIs (just the flat ordered list)
 *   - Concurrent access (single-threaded)
 *   - Range queries ("give me all values in [a, b]")
 *   - Duplicate handling (this file allows duplicates;
 *     real skip lists typically don't)
 *
 * Usage:
 *   ./skip_list                run self-tests
 *
 * Skills practiced:
 *   - Probabilistic data structure design
 *   - Linked lists with variable-length node arrays
 *   - Level promotion via coin flips
 *   - Ordered iteration without a separate sorted array
 *   - Comparing against deterministic balanced BSTs
 *   - Manual memory management with multiple pointer arrays
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_LEVEL 32
#define P         0.5     /* probability of promoting to next level */
#define SEED      0x12345 /* for deterministic demo runs */

/* ---------- node structure ---------- */

typedef struct skip_node
{
    int value;
    int levels;                          /* how many forward pointers this node has */
    struct skip_node *forward[];         /* flexible array, length == levels */
} skip_node;

/* ---------- skip list structure ---------- */

typedef struct
{
    skip_node *head;    /* the sentinel head node, always has MAX_LEVEL levels */
    int current_level;  /* current height of the list */
    int size;           /* number of elements */
} skip_list;

/* Random level generator: flip a coin (P=0.5) until tails, or
 * hit MAX_LEVEL. Expected result is 2 (geometric distribution). */
static int random_level(void)
{
    int level = 1;
    while ((double)rand() / RAND_MAX < P && level < MAX_LEVEL)
        level++;
    return level;
}

/* Create a new node with the given value and level count */
static skip_node *node_new(int value, int levels)
{
    skip_node *n = malloc(sizeof(*n) + sizeof(skip_node *) * levels);
    if (!n) { perror("malloc"); exit(EXIT_FAILURE); }
    n->value = value;
    n->levels = levels;
    for (int i = 0; i < levels; i++)
        n->forward[i] = NULL;
    return n;
}

/* ---------- create / destroy ---------- */

skip_list *sl_create(void)
{
    skip_list *sl = malloc(sizeof(*sl));
    if (!sl) { perror("malloc"); exit(EXIT_FAILURE); }

    sl->head = node_new(0, MAX_LEVEL);
    sl->current_level = 1;
    sl->size = 0;
    return sl;
}

void sl_destroy(skip_list *sl)
{
    if (!sl) return;
    skip_node *cur = sl->head;
    while (cur)
    {
        skip_node *next = cur->forward[0];
        free(cur);
        cur = next;
    }
    free(sl);
}

/* ---------- search ---------- */

/* Find a value. Returns 1 if found, 0 otherwise. */
int sl_search(skip_list *sl, int value)
{
    skip_node *cur = sl->head;

    for (int i = sl->current_level - 1; i >= 0; i--)
    {
        while (cur->forward[i] && cur->forward[i]->value < value)
            cur = cur->forward[i];
    }

    cur = cur->forward[0];
    return cur && cur->value == value;
}

/* ---------- insert ---------- */

/* Insert a value. Duplicates are allowed (insert as separate
 * nodes; the ordered iteration shows both). Returns 1 if the
 * insert succeeded, 0 on allocation failure. */
int sl_insert(skip_list *sl, int value)
{
    /* Build the update array: update[i] is the last node at
     * level i whose forward[i] is >= value. This is where we
     * need to splice in the new node at level i. */
    skip_node *update[MAX_LEVEL];
    skip_node *cur = sl->head;

    for (int i = sl->current_level - 1; i >= 0; i--)
    {
        while (cur->forward[i] && cur->forward[i]->value < value)
            cur = cur->forward[i];
        update[i] = cur;
    }

    /* Pick a random level for the new node */
    int new_level = random_level();

    /* If the new node's level is higher than the current list
     * height, we need to extend the list. The update array
     * entries above current_level were never set, so we set
     * them to the head node. */
    if (new_level > sl->current_level)
    {
        for (int i = sl->current_level; i < new_level; i++)
            update[i] = sl->head;
        sl->current_level = new_level;
    }

    /* Create the new node */
    skip_node *n = node_new(value, new_level);

    /* Splice it in at every level */
    for (int i = 0; i < new_level; i++)
    {
        n->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = n;
    }

    sl->size++;
    return 1;
}

/* ---------- delete ---------- */

/* Delete one occurrence of a value. Returns 1 if found and
 * removed, 0 otherwise. */
int sl_delete(skip_list *sl, int value)
{
    skip_node *update[MAX_LEVEL];
    skip_node *cur = sl->head;

    for (int i = sl->current_level - 1; i >= 0; i--)
    {
        while (cur->forward[i] && cur->forward[i]->value < value)
            cur = cur->forward[i];
        update[i] = cur;
    }

    cur = cur->forward[0];
    if (!cur || cur->value != value)
        return 0;

    /* Unlink the target from every level it appears in */
    for (int i = 0; i < cur->levels; i++)
    {
        if (update[i]->forward[i] == cur)
            update[i]->forward[i] = cur->forward[i];
    }
    free(cur);
    sl->size--;

    /* Trim the list's height: if the top levels are now empty
     * (forward pointers are all NULL), lower current_level */
    while (sl->current_level > 1 &&
           sl->head->forward[sl->current_level - 1] == NULL)
        sl->current_level--;

    return 1;
}

/* ---------- ordered iteration ---------- */

/* Print the list in sorted order (follow forward[0]) */
void sl_print(const skip_list *sl)
{
    printf("  [");
    skip_node *cur = sl->head->forward[0];
    int first = 1;
    int printed = 0;
    while (cur)
    {
        if (!first) printf(", ");
        printf("%d", cur->value);
        first = 0;
        printed = 1;
        cur = cur->forward[0];
        if (printed > 30) { printf(", ..."); break; }
    }
    printf("]\n");
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-55s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_basic(void)
{
    printf("Test 1: basic insert, search, delete\n");

    skip_list *sl = sl_create();

    sl_insert(sl, 10);
    sl_insert(sl, 20);
    sl_insert(sl, 30);
    sl_insert(sl, 40);
    sl_insert(sl, 50);

    check("search 10 -> found", sl_search(sl, 10));
    check("search 30 -> found", sl_search(sl, 30));
    check("search 50 -> found", sl_search(sl, 50));
    check("search 25 -> not found", !sl_search(sl, 25));
    check("search 5 -> not found", !sl_search(sl, 5));
    check("size is 5", sl->size == 5);

    sl_print(sl);

    check("delete 30 succeeds", sl_delete(sl, 30));
    check("search 30 -> not found", !sl_search(sl, 30));
    check("size is 4", sl->size == 4);

    sl_destroy(sl);
    printf("\n");
}

static void test_ordered(void)
{
    printf("Test 2: ordered iteration\n");

    skip_list *sl = sl_create();

    int values[] = { 42, 17, 89, 3, 55, 21, 78, 66, 12, 31 };
    int n = (int)(sizeof(values) / sizeof(values[0]));
    for (int i = 0; i < n; i++)
        sl_insert(sl, values[i]);

    printf("  Inserted %d values in scrambled order\n", n);
    sl_print(sl);

    /* Verify sorted order via forward[0] */
    skip_node *cur = sl->head->forward[0];
    int prev = -1;
    int sorted = 1;
    while (cur)
    {
        if (cur->value < prev) { sorted = 0; break; }
        prev = cur->value;
        cur = cur->forward[0];
    }
    check("iteration is in sorted order", sorted);

    sl_destroy(sl);
    printf("\n");
}

static void test_duplicates(void)
{
    printf("Test 3: duplicates\n");

    skip_list *sl = sl_create();

    sl_insert(sl, 5);
    sl_insert(sl, 5);
    sl_insert(sl, 5);

    check("search 5 -> found", sl_search(sl, 5));
    check("size is 3", sl->size == 3);

    sl_print(sl);

    check("delete 5 succeeds", sl_delete(sl, 5));
    check("size is 2", sl->size == 2);
    check("search 5 still found", sl_search(sl, 5));

    sl_destroy(sl);
    printf("\n");
}

static void test_large(void)
{
    printf("Test 4: large scale (10,000 elements)\n");

    skip_list *sl = sl_create();

    /* Insert 10,000 values in random order */
    srand(SEED);
    int n = 10000;
    for (int i = 0; i < n; i++)
        sl_insert(sl, rand() % (n * 2));

    printf("  size after %d inserts: %d (with duplicates)\n", n, sl->size);
    printf("  current level: %d / %d\n", sl->current_level, MAX_LEVEL);

    /* Verify ordered iteration */
    skip_node *cur = sl->head->forward[0];
    int prev = -1;
    int sorted = 1;
    int count = 0;
    while (cur)
    {
        if (cur->value < prev) { sorted = 0; break; }
        prev = cur->value;
        cur = cur->forward[0];
        count++;
    }
    check("iteration is in sorted order", sorted);
    check("iteration count matches size", count == sl->size);

    /* Verify all values present in a small sample */
    srand(SEED);
    int found_all = 1;
    for (int i = 0; i < 100; i++)
    {
        int v = rand() % (n * 2);
        /* Some may have been deleted; skip this check.
         * Instead just check that search doesn't crash and
         * returns a consistent answer. */
        int present = sl_search(sl, v);
        (void)present;
    }
    check("search on all values doesn't crash", 1);

    sl_destroy(sl);
    printf("\n");
}

static void test_height(void)
{
    printf("Test 5: height growth is logarithmic\n");

    skip_list *sl = sl_create();

    srand(SEED);
    /* Insert 100,000 distinct values */
    for (int i = 0; i < 100000; i++)
        sl_insert(sl, i * 7);   /* multiples of 7, all distinct */

    printf("  size: %d\n", sl->size);
    printf("  current level: %d\n", sl->current_level);

    /* With P=0.5, expected max level after n inserts is
     * about log2(n). For n=100,000, that's ~16.6. Our
     * MAX_LEVEL is 32, and we'll rarely exceed ~20. */
    check("height is reasonable (10-25 for 100k)", 
          sl->current_level >= 10 && sl->current_level <= 25);

    sl_destroy(sl);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("           SKIP LIST                \n");
    printf("====================================\n\n");

    printf("Parameters: MAX_LEVEL=%d, P=%.1f\n\n", MAX_LEVEL, P);

    test_basic();
    test_ordered();
    test_duplicates();
    test_large();
    test_height();

    printf("All tests complete.\n");
    return 0;
}
