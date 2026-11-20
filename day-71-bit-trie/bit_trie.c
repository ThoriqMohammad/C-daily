/* bit_trie.c
 *
 * Step order (what this program does):
 *
 *   1. Define the bit-trie node: two children (0 and 1) plus
 *      an optional "value" slot
 *   2. Implement insert(key, value): walk the key's bits from
 *      most significant to least, creating child nodes as needed,
 *      and store the value at the leaf
 *   3. Implement lookup(key): walk the bits, return the value
 *      if the exact key exists
 *   4. Implement longest_prefix_match(key): walk the bits,
 *      remembering the last node with a value, return that
 *   5. Implement delete(key): walk the bits, clear the value,
 *      and prune empty branches on the way back up
 *   6. Free all memory with a recursive destroy
 *   7. Test all four operations with IPv4-style prefixes
 *      and plain integers
 *
 * What a bit trie is:
 *
 *   A trie (pronounced "tree", from reTRIEval) stores keys as
 *   paths through a tree. Each node branches on one bit of the
 *   key. A character trie branches on bytes; a bit trie branches
 *   on individual bits.
 *
 *   For a 32-bit key, a bit trie has at most 33 levels (one
 *   per bit plus the root). Every lookup, insert, or delete
 *   visits at most 33 nodes — O(key_bits), independent of how
 *   many keys are stored.
 *
 *   The trie's superpower is LONGEST PREFIX MATCHING: given a
 *   key, find the stored key that shares the longest prefix.
 *   This is exactly what an IP router does: "which route entry
 *   matches the destination address most specifically?"
 *
 *   Bit tries are the data structure behind:
 *   - Linux routing tables (the "LC-trie")
 *   - BGP route lookup
 *   - Network intrusion detection (Snort, Suricata)
 *   - Firewall rule matching
 *   - Packet classification (Open vSwitch)
 *
 * What this implementation does NOT do:
 *   - Compress single-child chains (a "Patricia trie" or
 *     "radix tree" does this; a plain bit trie wastes nodes)
 *   - Support 128-bit IPv6 keys (would need 128 levels or
 *     a different representation)
 *   - Store multiple values per key
 *   - Thread safety
 *   Each is a real feature of production implementations;
 *   none are needed for understanding the core algorithm.
 *
 * Usage:
 *   ./bit_trie
 *
 * Skills practiced:
 *   - Binary trie structure (node has bit_0 and bit_1 children)
 *   - Bit extraction with shifts and masks
 *   - Recursive tree traversal
 *   - Insert / lookup / delete / destroy
 *   - Longest-prefix-match as a variant of lookup
 *   - Recursive pruning on delete
 *   - Manual memory management in a tree
 *   - IP prefix notation ("10.0.0.0/8") as a test case
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- node structure ---------- */

typedef struct bit_node
{
    struct bit_node *child[2];   /* child[0] for bit 0, child[1] for bit 1 */
    int has_value;               /* 1 if this node holds a value */
    int value;                   /* the stored value */
} bit_node;

/* ---------- creation ---------- */

static bit_node *node_new(void)
{
    bit_node *n = calloc(1, sizeof(*n));
    if (!n) { perror("calloc"); exit(EXIT_FAILURE); }
    return n;
}

/* Free an entire subtree */
static void trie_destroy(bit_node *root)
{
    if (!root) return;
    trie_destroy(root->child[0]);
    trie_destroy(root->child[1]);
    free(root);
}

/* ---------- insert / lookup ---------- */

/* Insert a key of `bits` bits with the given value.
 * Bits are read from most significant (bit bits-1) to least
 * significant (bit 0). */
static void trie_insert(bit_node *root, uint32_t key, int bits, int value)
{
    bit_node *cur = root;
    for (int i = bits - 1; i >= 0; i--)
    {
        int b = (key >> i) & 1;
        if (!cur->child[b])
            cur->child[b] = node_new();
        cur = cur->child[b];
    }
    cur->has_value = 1;
    cur->value = value;
}

/* Look up an exact key. Returns 1 and sets *out_value if found,
 * 0 otherwise. */
static int trie_lookup(const bit_node *root, uint32_t key, int bits,
                       int *out_value)
{
    const bit_node *cur = root;
    for (int i = bits - 1; i >= 0; i--)
    {
        int b = (key >> i) & 1;
        cur = cur->child[b];
        if (!cur) return 0;
    }
    if (cur->has_value)
    {
        if (out_value) *out_value = cur->value;
        return 1;
    }
    return 0;
}

/* ---------- longest prefix match ---------- */

/* Find the longest stored prefix of `key`.
 * Returns the number of matching bits (0 if none) and sets
 * *out_value to the value stored at that prefix.
 *
 * This is the operation routers use: "given a destination IP,
 * which route entry matches most specifically?" */
static int trie_longest_prefix(const bit_node *root, uint32_t key,
                               int bits, int *out_value)
{
    const bit_node *cur = root;
    int best_bits = 0;
    int best_value = 0;

    for (int i = bits - 1; i >= 0; i--)
    {
        int b = (key >> i) & 1;
        cur = cur->child[b];
        if (!cur) break;

        if (cur->has_value)
        {
            /* This prefix has a stored value. It's a candidate
             * for the answer. Keep going to see if a longer
             * prefix matches. */
            best_bits = bits - i;
            best_value = cur->value;
        }
    }

    if (out_value) *out_value = best_value;
    return best_bits;
}

/* ---------- delete ---------- */

/* Delete an exact key. Returns 1 if the key existed, 0 otherwise.
 * Prunes nodes on the way back up if they become useless (no
 * value and no children). */
static int trie_delete(bit_node *root, uint32_t key, int bits)
{
    /* Recurse with a stack of (node, bit) pairs.
     * We need to walk down and then walk back up to prune. */
    bit_node *path[64];   /* keys are at most 64 bits in this file */
    int bit_path[64];
    if (bits > 64) return 0;

    bit_node *cur = root;
    for (int i = bits - 1; i >= 0; i--)
    {
        int b = (key >> i) & 1;
        path[bits - 1 - i] = cur;
        bit_path[bits - 1 - i] = b;
        cur = cur->child[b];
        if (!cur) return 0;
    }

    if (!cur->has_value) return 0;
    cur->has_value = 0;

    /* Walk back up, freeing empty nodes */
    for (int i = bits - 1; i >= 0; i--)
    {
        bit_node *parent = path[i];
        int b = bit_path[i];
        bit_node *child = parent->child[b];

        if (child && !child->has_value &&
            !child->child[0] && !child->child[1])
        {
            free(child);
            parent->child[b] = NULL;
        }
        else
        {
            /* This node is still in use; all ancestors are too */
            break;
        }
    }

    return 1;
}

/* ---------- traversal (for debugging) ---------- */

static void trie_dump_recursive(const bit_node *n, uint32_t prefix,
                                int depth, int bits)
{
    if (!n) return;
    if (n->has_value)
    {
        /* Print the key as bits, or as an integer */
        printf("  key=0x%08x bits=%d value=%d\n", prefix, depth, n->value);
    }
    /* Only recurse if we have bits remaining to shift in */
    if (depth < bits)
    {
        uint32_t child_prefix = prefix;
        trie_dump_recursive(n->child[0], child_prefix, depth + 1, bits);
        child_prefix = prefix | (1u << (bits - 1 - depth));
        trie_dump_recursive(n->child[1], child_prefix, depth + 1, bits);
    }
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-55s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_basic(void)
{
    printf("Test 1: insert and lookup\n");

    bit_node *t = node_new();

    trie_insert(t, 0x00000001u, 32, 1);
    trie_insert(t, 0x00000002u, 32, 2);
    trie_insert(t, 0x00000003u, 32, 3);
    trie_insert(t, 0xFFFFFFFFu, 32, 0xFF);

    int v;
    check("lookup 1 -> 1", trie_lookup(t, 1, 32, &v) && v == 1);
    check("lookup 2 -> 2", trie_lookup(t, 2, 32, &v) && v == 2);
    check("lookup 3 -> 3", trie_lookup(t, 3, 32, &v) && v == 3);
    check("lookup 0xFFFFFFFF -> 0xFF",
          trie_lookup(t, 0xFFFFFFFFu, 32, &v) && v == 0xFF);
    check("lookup 4 -> not found", !trie_lookup(t, 4, 32, &v));

    trie_destroy(t);
    printf("\n");
}

static void test_lpm(void)
{
    printf("Test 2: longest prefix match\n");

    bit_node *t = node_new();

    /* Store routes at several prefix lengths, as a router would.
     * In a real router, these correspond to:
     *   0.0.0.0/0     -> default, value 1
     *   10.0.0.0/8    -> value 10
     *   10.1.0.0/16   -> value 101
     *   10.1.2.0/24   -> value 1012
     *   10.1.2.3/32   -> value 10123
     *
     * Then longest_prefix(10.1.2.5) should return 10.1.2.0/24. */
    trie_insert(t, 0u, 0, 1);                               /* /0 */
    trie_insert(t, 0x0A000000u, 8, 10);                     /* 10.0.0.0/8 */
    trie_insert(t, 0x0A010000u, 16, 101);                   /* 10.1.0.0/16 */
    trie_insert(t, 0x0A010200u, 24, 1012);                  /* 10.1.2.0/24 */
    trie_insert(t, 0x0A010203u, 32, 10123);                 /* 10.1.2.3 */

    int v;
    int bits;

    bits = trie_longest_prefix(t, 0u, 32, &v);
    check("/0 matches anything (0 bits, value 1)",
          bits == 0 && v == 1);

    bits = trie_longest_prefix(t, 0x0B000001u, 32, &v);     /* 11.0.0.1 */
    check("11.0.0.1 -> /0 (value 1)", bits == 0 && v == 1);

    bits = trie_longest_prefix(t, 0x0AFFFFFFu, 32, &v);     /* 10.255.255.255 */
    check("10.255.255.255 -> /8 (value 10)", bits == 8 && v == 10);

    bits = trie_longest_prefix(t, 0x0A01FFFFu, 32, &v);     /* 10.1.255.255 */
    check("10.1.255.255 -> /16 (value 101)", bits == 16 && v == 101);

    bits = trie_longest_prefix(t, 0x0A0102FFu, 32, &v);     /* 10.1.2.255 */
    check("10.1.2.255 -> /24 (value 1012)", bits == 24 && v == 1012);

    bits = trie_longest_prefix(t, 0x0A010203u, 32, &v);     /* 10.1.2.3 */
    check("10.1.2.3 -> /32 (value 10123)", bits == 32 && v == 10123);

    trie_destroy(t);
    printf("\n");
}

static void test_delete(void)
{
    printf("Test 3: delete and prune\n");

    bit_node *t = node_new();

    trie_insert(t, 0x0A000001u, 32, 100);
    trie_insert(t, 0x0A000002u, 32, 200);

    int v;
    check("both present before delete",
          trie_lookup(t, 0x0A000001u, 32, &v) &&
          trie_lookup(t, 0x0A000002u, 32, &v));

    check("delete 1 succeeds", trie_delete(t, 0x0A000001u, 32));
    check("delete 1 again fails", !trie_delete(t, 0x0A000001u, 32));

    check("1 gone", !trie_lookup(t, 0x0A000001u, 32, &v));
    check("2 still present",
          trie_lookup(t, 0x0A000002u, 32, &v) && v == 200);

    trie_destroy(t);
    printf("\n");
}

static void test_overwrite(void)
{
    printf("Test 4: overwriting a key\n");

    bit_node *t = node_new();

    trie_insert(t, 42, 32, 1);
    trie_insert(t, 42, 32, 2);   /* overwrite */

    int v;
    check("42 -> 2 after overwrite",
          trie_lookup(t, 42, 32, &v) && v == 2);

    trie_destroy(t);
    printf("\n");
}

static void test_empty(void)
{
    printf("Test 5: empty trie\n");

    bit_node *t = node_new();

    int v;
    check("lookup on empty -> not found",
          !trie_lookup(t, 0, 32, &v));
    check("longest_prefix on empty -> 0 bits",
          trie_longest_prefix(t, 0xFFFFFFFFu, 32, &v) == 0);
    check("delete on empty -> fails",
          !trie_delete(t, 0, 32));

    trie_destroy(t);
    printf("\n");
}

static void test_short_keys(void)
{
    printf("Test 6: keys shorter than 32 bits\n");

    bit_node *t = node_new();

    /* Store an 8-bit key, then a 16-bit key that shares a prefix */
    trie_insert(t, 0x00ABu, 8, 100);           /* 8-bit key 0xAB */
    trie_insert(t, 0xABCDu, 16, 200);          /* 16-bit key 0xABCD */

    int v;
    check("8-bit lookup 0xAB -> 100",
          trie_lookup(t, 0x00ABu, 8, &v) && v == 100);
    check("16-bit lookup 0xABCD -> 200",
          trie_lookup(t, 0xABCDu, 16, &v) && v == 200);

    /* Note: these are different-length keys in the SAME trie.
     * In practice, real tries use one key length. This shows the
     * trie can handle mixed lengths if you pass the right `bits`. */

    trie_destroy(t);
    printf("\n");
}

int main(void)
{
    printf("====================================\n");
    printf("            BIT TRIE                \n");
    printf("====================================\n\n");

    test_basic();
    test_lpm();
    test_delete();
    test_overwrite();
    test_empty();
    test_short_keys();

    printf("All tests complete.\n");
    return 0;
}
