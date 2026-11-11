/* lru_cache.c
 *
 * An LRU (Least Recently Used) cache with O(1) get and put.
 *
 * An LRU cache stores a fixed number of key-value pairs. When it
 * reaches capacity and a new item is inserted, the "least recently
 * used" item is evicted to make room.
 *
 * "Least recently used" means: the item that was accessed (get or
 * put) furthest in the past. Every get or put "refreshes" the
 * item's recency.
 *
 * To achieve O(1) for both get and put, we combine two structures:
 *
 *   1. A HASH TABLE mapping key -> node pointer, for O(1) lookup.
 *   2. A DOUBLY-LINKED LIST ordered by recency, with the most
 *      recently used at the head and the least recently used at
 *      the tail.
 *
 * Neither structure alone is sufficient:
 *
 *   - Hash table only: O(1) lookup, but evicting the LRU item
 *     requires finding it, which is O(n).
 *   - Linked list only: O(1) eviction (just pop the tail), but
 *     lookup is O(n).
 *
 * Together they give O(1) for everything.
 *
 * This is the same design that powers:
 *   - the Linux page cache
 *   - CPU cache replacement policies (roughly)
 *   - Redis's maxmemory eviction
 *   - database buffer pools
 *   - HTTP response caches in browsers and CDNs
 *   - DNS resolvers
 *
 * Usage:
 *   ./lru_cache
 *
 * Skills practiced:
 *   - Doubly-linked list manipulation with sentinel nodes
 *   - Hash table with chaining (from Day 28)
 *   - Combining two data structures for amortized O(1)
 *   - Moving a node to the front without reallocating
 *   - Careful invariant maintenance under eviction
 *   - Manual memory management with clear ownership
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- hash table ---------- */

#define HT_BUCKETS 64

/* FNV-1a hash for the key strings */
static uint32_t hash_string(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s)
    {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return h;
}

/* ---------- node ---------- */

typedef struct lru_node
{
    char *key;
    int   value;
    struct lru_node *prev;
    struct lru_node *next;
    struct lru_node *hash_next;  /* chain in the bucket */
} lru_node;

/* ---------- LRU cache ---------- */

typedef struct
{
    size_t capacity;
    size_t size;

    /* Doubly-linked list with sentinels:
     *   head.next -> most recently used
     *   tail.prev -> least recently used
     * Sentinels eliminate edge cases in link/unlink. */
    lru_node head;
    lru_node tail;

    /* Hash table buckets */
    lru_node *buckets[HT_BUCKETS];
} lru_cache;

/* ---------- hash table operations ---------- */

static lru_node *ht_find(lru_cache *c, const char *key)
{
    uint32_t h = hash_string(key) % HT_BUCKETS;
    for (lru_node *n = c->buckets[h]; n; n = n->hash_next)
        if (strcmp(n->key, key) == 0)
            return n;
    return NULL;
}

static void ht_insert(lru_cache *c, lru_node *n)
{
    uint32_t h = hash_string(n->key) % HT_BUCKETS;
    n->hash_next = c->buckets[h];
    c->buckets[h] = n;
}

static void ht_remove(lru_cache *c, lru_node *n)
{
    uint32_t h = hash_string(n->key) % HT_BUCKETS;
    lru_node **link = &c->buckets[h];
    while (*link)
    {
        if (*link == n)
        {
            *link = n->hash_next;
            n->hash_next = NULL;
            return;
        }
        link = &(*link)->hash_next;
    }
}

/* ---------- linked list operations ---------- */

/* Unlink a node from the list (does NOT free it) */
static void list_unlink(lru_node *n)
{
    n->prev->next = n->next;
    n->next->prev = n->prev;
}

/* Insert a node right after the sentinel head (MRU position) */
static void list_push_front(lru_cache *c, lru_node *n)
{
    n->next = c->head.next;
    n->prev = &c->head;
    c->head.next->prev = n;
    c->head.next = n;
}

/* Move an existing node to the MRU position */
static void list_move_to_front(lru_cache *c, lru_node *n)
{
    list_unlink(n);
    list_push_front(c, n);
}

/* ---------- cache operations ---------- */

lru_cache *lru_create(size_t capacity)
{
    lru_cache *c = calloc(1, sizeof(*c));
    if (!c) return NULL;

    c->capacity = capacity;
    c->size = 0;

    /* Wire up the sentinels to form an empty list */
    c->head.next = &c->tail;
    c->head.prev = NULL;
    c->tail.prev = &c->head;
    c->tail.next = NULL;

    return c;
}

void lru_destroy(lru_cache *c)
{
    if (!c) return;

    /* Walk the list and free every node */
    lru_node *n = c->head.next;
    while (n != &c->tail)
    {
        lru_node *next = n->next;
        free(n->key);
        free(n);
        n = next;
    }
    free(c);
}

/* Get the value for a key. On hit, moves the node to MRU.
 * Returns 1 if found, 0 if not. */
int lru_get(lru_cache *c, const char *key, int *out_value)
{
    lru_node *n = ht_find(c, key);
    if (!n)
        return 0;

    list_move_to_front(c, n);
    if (out_value) *out_value = n->value;
    return 1;
}

/* Put a key-value pair. If the key exists, update it and move
 * to MRU. If it's new, insert at MRU. If full, evict LRU. */
void lru_put(lru_cache *c, const char *key, int value)
{
    lru_node *n = ht_find(c, key);

    if (n)
    {
        /* Update in place */
        n->value = value;
        list_move_to_front(c, n);
        return;
    }

    /* New entry: allocate a node */
    n = calloc(1, sizeof(*n));
    if (!n) return;
    n->key = strdup(key);
    if (!n->key) { free(n); return; }
    n->value = value;

    /* Insert at MRU and into the hash table */
    list_push_front(c, n);
    ht_insert(c, n);
    c->size++;

    /* Evict if over capacity */
    if (c->size > c->capacity)
    {
        lru_node *victim = c->tail.prev;
        list_unlink(victim);
        ht_remove(c, victim);
        c->size--;
        free(victim->key);
        free(victim);
    }
}

/* Peek without touching recency — useful for tests and debugging */
int lru_peek(lru_cache *c, const char *key, int *out_value)
{
    lru_node *n = ht_find(c, key);
    if (!n) return 0;
    if (out_value) *out_value = n->value;
    return 1;
}

/* Print the current list from MRU (head) to LRU (tail) */
void lru_dump(const lru_cache *c)
{
    printf("  [");
    int first = 1;
    for (lru_node *n = c->head.next; n != &c->tail; n = n->next)
    {
        if (!first) printf(", ");
        printf("%s=%d", n->key, n->value);
        first = 0;
    }
    printf("]\n");
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-52s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_basic(void)
{
    printf("Test 1: basic put/get\n");

    lru_cache *c = lru_create(3);
    int v;

    lru_put(c, "a", 1);
    lru_put(c, "b", 2);
    lru_put(c, "c", 3);

    check("get a = 1", lru_get(c, "a", &v) && v == 1);
    check("get b = 2", lru_get(c, "b", &v) && v == 2);
    check("get c = 3", lru_get(c, "c", &v) && v == 3);
    check("get d missing", !lru_get(c, "d", &v));

    lru_destroy(c);
    printf("\n");
}

static void test_eviction(void)
{
    printf("Test 2: LRU eviction\n");

    lru_cache *c = lru_create(3);
    int v;

    lru_put(c, "a", 1);
    lru_put(c, "b", 2);
    lru_put(c, "c", 3);
    printf("  after a,b,c:\n");
    lru_dump(c);

    /* Access a, making it MRU */
    lru_get(c, "a", &v);
    printf("  after get(a):\n");
    lru_dump(c);
    check("a moved to MRU (front)", c->head.next->value == 1);

    /* Insert d — should evict b (LRU) */
    lru_put(c, "d", 4);
    printf("  after put(d):\n");
    lru_dump(c);

    check("b was evicted", !lru_peek(c, "b", &v));
    check("a still present", lru_peek(c, "a", &v) && v == 1);
    check("c still present", lru_peek(c, "c", &v) && v == 3);
    check("d was inserted",  lru_peek(c, "d", &v) && v == 4);

    lru_destroy(c);
    printf("\n");
}

static void test_update(void)
{
    printf("Test 3: update existing key\n");

    lru_cache *c = lru_create(3);
    int v;

    lru_put(c, "x", 10);
    lru_put(c, "y", 20);
    lru_put(c, "z", 30);

    /* Update x to 100. x should become MRU, and no eviction
     * should happen (size stays at 3). */
    lru_put(c, "x", 100);
    lru_dump(c);

    check("x updated to 100", lru_get(c, "x", &v) && v == 100);
    check("size stayed at 3", c->size == 3);
    check("y still present", lru_peek(c, "y", &v) && v == 20);
    check("z still present", lru_peek(c, "z", &v) && v == 30);

    lru_destroy(c);
    printf("\n");
}

static void test_recency_order(void)
{
    printf("Test 4: recency order matches access order\n");

    lru_cache *c = lru_create(4);

    lru_put(c, "a", 1);
    lru_put(c, "b", 2);
    lru_put(c, "c", 3);
    lru_put(c, "d", 4);

    /* Access in a specific order to set recency */
    int v;
    lru_get(c, "b", &v);   /* b becomes MRU */
    lru_get(c, "d", &v);   /* d becomes MRU */

    /* Expected recency (MRU to LRU): d, b, c, a */
    const char *expected[] = { "d", "b", "c", "a" };
    lru_node *n = c->head.next;
    int ok = 1;
    for (int i = 0; i < 4; i++)
    {
        if (strcmp(n->key, expected[i]) != 0) { ok = 0; break; }
        n = n->next;
    }
    lru_dump(c);
    check("recency order is d, b, c, a", ok);

    /* Insert e — should evict a (LRU) */
    lru_put(c, "e", 5);
    check("a was evicted", !lru_peek(c, "a", &v));
    check("b, c, d still present",
          lru_peek(c, "b", &v) && lru_peek(c, "c", &v) && lru_peek(c, "d", &v));

    lru_destroy(c);
    printf("\n");
}

static void test_capacity_one(void)
{
    printf("Test 5: capacity = 1 (edge case)\n");

    lru_cache *c = lru_create(1);
    int v;

    lru_put(c, "a", 1);
    check("a present", lru_get(c, "a", &v) && v == 1);

    lru_put(c, "b", 2);
    check("a evicted", !lru_peek(c, "a", &v));
    check("b present", lru_get(c, "b", &v) && v == 2);

    lru_destroy(c);
    printf("\n");
}

static void test_stress(void)
{
    printf("Test 6: stress with 10,000 operations\n");

    lru_cache *c = lru_create(100);
    char key[16];
    int ok = 1;

    for (int i = 0; i < 10000; i++)
    {
        snprintf(key, sizeof(key), "k%d", i % 200);
        lru_put(c, key, i);
    }

    /* Cache should be exactly at capacity */
    check("size == capacity after stress", c->size == 100);

    /* Every get should succeed for the last 100 keys inserted */
    for (int i = 19900; i < 20000; i++)
    {
        snprintf(key, sizeof(key), "k%d", i % 200);
        int v;
        if (!lru_get(c, key, &v))
            ok = 0;
    }
    check("all recent keys still in cache", ok);

    lru_destroy(c);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("            LRU CACHE               \n");
    printf("====================================\n\n");

    test_basic();
    test_eviction();
    test_update();
    test_recency_order();
    test_capacity_one();
    test_stress();

    printf("All tests complete.\n");
    return 0;
}
