/* huffman.c
 *
 * Huffman coding: variable-length prefix codes based on symbol
 * frequency.
 *
 * Huffman coding assigns shorter bit-strings to more frequent
 * symbols and longer ones to rarer symbols, minimizing the total
 * encoded length. It is the compression layer inside DEFLATE
 * (gzip, zlib, PNG, ZIP).
 *
 * The algorithm:
 *
 *   1. Count frequency of each byte in the input.
 *   2. Build a min-heap of leaf nodes (one per symbol).
 *   3. Repeatedly pop two smallest nodes, create a parent with
 *      their combined frequency, push it back.
 *   4. The last remaining node is the root of the Huffman tree.
 *   5. Walk the tree: left = bit 0, right = bit 1.
 *   6. The code for each symbol is the path from root to leaf.
 *
 * Because no code is a prefix of another, the code is uniquely
 * decodable without delimiters. This is the "prefix-free"
 * property that makes Huffman codes work.
 *
 * This implementation:
 *   - reads from stdin, writes to stdout
 *   - compresses to a self-describing format (frequency table + bits)
 *   - can be built into a working compress/decompress pair
 *
 * Usage:
 *   ./huffman encode < input.txt > output.huf
 *   ./huffman decode < output.huf > restored.txt
 *   ./huffman test              (self-tests)
 *
 * Skills practiced:
 *   - Min-heaps implemented from scratch
 *   - Binary tree construction and traversal
 *   - Recursive tree walks for code generation
 *   - Bit packing (writing single bits into bytes)
 *   - Frequency analysis
 *   - File I/O with fread/fwrite
 *   - Variable-length encoding and decoding
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_SYMS  256
#define MAX_CODE  32   /* longest possible Huffman code for 256 symbols */

/* ---------- Huffman tree ---------- */

typedef struct node
{
    unsigned char symbol;      /* only meaningful at leaves    */
    unsigned long freq;
    struct node *left;
    struct node *right;
    struct node *next;         /* used by the min-heap list   */
} node;

/* Free a tree recursively */
static void free_tree(node *n)
{
    if (n == NULL) return;
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}

/* ---------- min-heap as a sorted linked list ---------- */

/* A proper implementation would use an array-based heap.
 * For 256 symbols, a sorted linked list is simpler and
 * perfectly fast enough. */

static node *heap_insert(node *head, node *n)
{
    if (head == NULL || n->freq < head->freq)
    {
        n->next = head;
        return n;
    }

    node *cur = head;
    while (cur->next != NULL && cur->next->freq <= n->freq)
        cur = cur->next;

    n->next = cur->next;
    cur->next = n;
    return head;
}

static node *heap_pop(node **head)
{
    node *n = *head;
    if (n != NULL)
        *head = n->next;
    return n;
}

/* ---------- build the Huffman tree ---------- */

static node *build_tree(const unsigned long freqs[MAX_SYMS])
{
    node *head = NULL;

    for (int i = 0; i < MAX_SYMS; i++)
    {
        if (freqs[i] == 0)
            continue;
        node *n = calloc(1, sizeof(*n));
        if (n == NULL) { perror("calloc"); exit(EXIT_FAILURE); }
        n->symbol = (unsigned char)i;
        n->freq = freqs[i];
        head = heap_insert(head, n);
    }

    /* Edge case: empty input */
    if (head == NULL)
    {
        node *n = calloc(1, sizeof(*n));
        if (n == NULL) { perror("calloc"); exit(EXIT_FAILURE); }
        n->symbol = 0;
        n->freq = 1;
        return n;
    }

    /* Edge case: single distinct symbol -> make a two-node tree
     * so every code has at least one bit. */
    if (head->next == NULL)
    {
        node *parent = calloc(1, sizeof(*parent));
        if (parent == NULL) { perror("calloc"); exit(EXIT_FAILURE); }
        parent->left = head;
        parent->right = NULL;
        parent->freq = head->freq;
        return parent;
    }

    while (head->next != NULL)
    {
        node *a = heap_pop(&head);
        node *b = heap_pop(&head);

        node *parent = calloc(1, sizeof(*parent));
        if (parent == NULL) { perror("calloc"); exit(EXIT_FAILURE); }
        parent->freq = a->freq + b->freq;
        parent->left = a;
        parent->right = b;

        head = heap_insert(head, parent);
    }

    return head;
}

/* ---------- generate the code table ---------- */

static void build_codes_recursive(node *n, char code[MAX_SYMS][MAX_CODE],
                                  int lengths[MAX_SYMS],
                                  char *buf, int depth)
{
    if (n == NULL)
        return;

    if (n->left == NULL && n->right == NULL)
    {
        /* Leaf: record the code. A single-symbol tree has depth
         * 0, so we give it an empty code and special-case it
         * during encoding. */
        lengths[n->symbol] = depth;
        if (depth == 0)
            code[n->symbol][0] = '\0';
        else
        {
            buf[depth] = '\0';
            memcpy(code[n->symbol], buf, depth + 1);
        }
        return;
    }

    if (n->left != NULL)
    {
        buf[depth] = '0';
        build_codes_recursive(n->left, code, lengths, buf, depth + 1);
    }
    if (n->right != NULL)
    {
        buf[depth] = '1';
        build_codes_recursive(n->right, code, lengths, buf, depth + 1);
    }
}

static void build_codes(node *root, char code[MAX_SYMS][MAX_CODE],
                        int lengths[MAX_SYMS])
{
    char buf[MAX_CODE + 1];
    memset(buf, 0, sizeof(buf));
    for (int i = 0; i < MAX_SYMS; i++)
        lengths[i] = 0;
    build_codes_recursive(root, code, lengths, buf, 0);
}

/* ---------- bit writer ---------- */

typedef struct
{
    FILE *fp;
    uint8_t cur;
    int bits;    /* number of bits already in cur (0..7) */
} bit_writer;

static void bw_init(bit_writer *bw, FILE *fp)
{
    bw->fp = fp;
    bw->cur = 0;
    bw->bits = 0;
}

static void bw_put_bit(bit_writer *bw, int bit)
{
    bw->cur = (uint8_t)((bw->cur << 1) | (bit ? 1 : 0));
    bw->bits++;
    if (bw->bits == 8)
    {
        fputc(bw->cur, bw->fp);
        bw->cur = 0;
        bw->bits = 0;
    }
}

static void bw_put_code(bit_writer *bw, const char *code)
{
    for (int i = 0; code[i] != '\0'; i++)
        bw_put_bit(bw, code[i] == '1');
}

static void bw_flush(bit_writer *bw)
{
    if (bw->bits > 0)
    {
        /* Pad with zeros to fill the last byte */
        bw->cur <<= (8 - bw->bits);
        fputc(bw->cur, bw->fp);
        bw->cur = 0;
        bw->bits = 0;
    }
}

/* ---------- bit reader ---------- */

typedef struct
{
    FILE *fp;
    uint8_t cur;
    int bits;    /* bits remaining in cur (0..8) */
} bit_reader;

static void br_init(bit_reader *br, FILE *fp)
{
    br->fp = fp;
    br->cur = 0;
    br->bits = 0;
}

/* Returns -1 on EOF */
static int br_get_bit(bit_reader *br)
{
    if (br->bits == 0)
    {
        int c = fgetc(br->fp);
        if (c == EOF)
            return -1;
        br->cur = (uint8_t)c;
        br->bits = 8;
    }
    int bit = (br->cur >> 7) & 1;
    br->cur <<= 1;
    br->bits--;
    return bit;
}

/* ---------- file header ---------- */

/* Format on disk (all integers big-endian):
 *
 *   4 bytes: magic "HUF1"
 *   8 bytes: original file length (unsigned long long)
 *   256 * 4 bytes: frequency for each symbol
 *
 * Then: the bit-packed Huffman stream.
 */

static void write_u32(FILE *fp, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16),
                     (uint8_t)(v >> 8),  (uint8_t)v };
    fwrite(b, 1, 4, fp);
}

static uint32_t read_u32(FILE *fp)
{
    uint8_t b[4];
    if (fread(b, 1, 4, fp) != 4)
        return 0;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] << 8)  |  (uint32_t)b[3];
}

static void write_u64(FILE *fp, uint64_t v)
{
    write_u32(fp, (uint32_t)(v >> 32));
    write_u32(fp, (uint32_t)v);
}

static uint64_t read_u64(FILE *fp)
{
    uint64_t hi = read_u32(fp);
    uint64_t lo = read_u32(fp);
    return (hi << 32) | lo;
}

/* ---------- encode ---------- */

static int huffman_encode(FILE *in, FILE *out)
{
    /* Read the entire input to count frequencies and keep it for
     * a second pass. Huffman encoding needs two passes. */
    size_t cap = 4096;
    size_t len = 0;
    uint8_t *data = malloc(cap);
    if (data == NULL) { perror("malloc"); return -1; }

    unsigned long freqs[MAX_SYMS] = {0};
    int c;
    while ((c = fgetc(in)) != EOF)
    {
        if (len == cap)
        {
            cap *= 2;
            uint8_t *tmp = realloc(data, cap);
            if (tmp == NULL) { perror("realloc"); free(data); return -1; }
            data = tmp;
        }
        data[len++] = (uint8_t)c;
        freqs[(uint8_t)c]++;
    }

    /* Build the tree and code table */
    node *root = build_tree(freqs);
    char code[MAX_SYMS][MAX_CODE];
    int lengths[MAX_SYMS];
    build_codes(root, code, lengths);

    /* Header */
    fwrite("HUF1", 1, 4, out);
    write_u64(out, (uint64_t)len);
    for (int i = 0; i < MAX_SYMS; i++)
        write_u32(out, (uint32_t)freqs[i]);

    /* Bit-pack the encoded data */
    bit_writer bw;
    bw_init(&bw, out);
    for (size_t i = 0; i < len; i++)
    {
        uint8_t sym = data[i];
        if (lengths[sym] == 0)
        {
            /* Single-symbol file: emit a single 0 bit */
            bw_put_bit(&bw, 0);
        }
        else
        {
            bw_put_code(&bw, code[sym]);
        }
    }
    bw_flush(&bw);

    free_tree(root);
    free(data);
    return 0;
}

/* ---------- decode ---------- */

static int huffman_decode(FILE *in, FILE *out)
{
    char magic[5] = {0};
    if (fread(magic, 1, 4, in) != 4 || strcmp(magic, "HUF1") != 0)
    {
        fprintf(stderr, "Not a HUF1 file.\n");
        return -1;
    }

    uint64_t orig_len = read_u64(in);

    unsigned long freqs[MAX_SYMS];
    for (int i = 0; i < MAX_SYMS; i++)
        freqs[i] = (unsigned long)read_u32(in);

    node *root = build_tree(freqs);

    /* Single distinct symbol: original file was just N copies of it */
    int single = (root->left == NULL || root->right == NULL);

    bit_reader br;
    br_init(&br, in);

    for (uint64_t i = 0; i < orig_len; i++)
    {
        if (single)
        {
            /* Consume one padding bit per symbol, emit the symbol */
            br_get_bit(&br);
            unsigned char s = root->left ? root->left->symbol
                                         : root->symbol;
            fputc(s, out);
            continue;
        }

        node *cur = root;
        while (cur->left != NULL || cur->right != NULL)
        {
            int bit = br_get_bit(&br);
            if (bit == -1)
            {
                fprintf(stderr, "Unexpected EOF at symbol %llu.\n",
                        (unsigned long long)i);
                free_tree(root);
                return -1;
            }
            cur = bit ? cur->right : cur->left;
        }
        fputc(cur->symbol, out);
    }

    free_tree(root);
    return 0;
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-50s %s\n", label, condition ? "ok" : "FAIL");
}

static int round_trip(const char *text, size_t len)
{
    /* Encode to memory via tmpfile, decode back, compare */
    FILE *enc = tmpfile();
    FILE *dec = tmpfile();
    FILE *input = tmpfile();
    if (!enc || !dec || !input) return 0;

    fwrite(text, 1, len, input);
    rewind(input);

    if (huffman_encode(input, enc) != 0) goto fail;
    rewind(enc);
    if (huffman_decode(enc, dec) != 0) goto fail;
    rewind(dec);

    for (size_t i = 0; i < len; i++)
    {
        int c = fgetc(dec);
        if (c != (unsigned char)text[i])
            goto fail;
    }

    fclose(enc); fclose(dec); fclose(input);
    return 1;

fail:
    fclose(enc); fclose(dec); fclose(input);
    return 0;
}

static void test_roundtrip(void)
{
    printf("Test 1: encode/decode round-trip\n");

    check("empty input round-trips",
          round_trip("", 0));

    check("single byte round-trips",
          round_trip("A", 1));

    check("repeated byte round-trips",
          round_trip("AAAAAAAAAA", 10));

    check("all distinct bytes round-trip",
          round_trip("ABCDEFGHIJ", 10));

    check("hello world round-trips",
          round_trip("hello, world\n", 13));

    check("pangram round-trips",
          round_trip("the quick brown fox jumps over the lazy dog", 43));

    printf("\n");
}

static void test_compression(void)
{
    printf("Test 2: compression ratio on skewed input\n");

    /* 1000 'A's, 500 'B's, 250 'C's, 100 'D's */
    char input[2000];
    size_t n = 0;
    for (int i = 0; i < 1000; i++) input[n++] = 'A';
    for (int i = 0; i <  500; i++) input[n++] = 'B';
    for (int i = 0; i <  250; i++) input[n++] = 'C';
    for (int i = 0; i <  100; i++) input[n++] = 'D';

    FILE *enc = tmpfile();
    FILE *in = tmpfile();
    fwrite(input, 1, n, in);
    rewind(in);
    huffman_encode(in, enc);
    long encoded_size = ftell(enc);

    printf("  original: %zu bytes, encoded: %ld bytes (%.1f%% of original)\n",
           n, encoded_size - 4 - 8 - 256 * 4, /* minus header bytes */
           100.0 * (encoded_size - 4 - 8 - 256 * 4) / (double)n);

    check("encoded smaller than original",
          encoded_size < (long)(n + 4 + 8 + 256 * 4));

    check("round-trip works on skewed input",
          round_trip(input, n));

    fclose(enc);
    fclose(in);
    printf("\n");
}

static void test_code_property(void)
{
    printf("Test 3: Huffman codes are prefix-free\n");

    unsigned long freqs[MAX_SYMS] = {0};
    freqs['A'] = 45;
    freqs['B'] = 13;
    freqs['C'] = 12;
    freqs['D'] = 16;
    freqs['E'] = 9;
    freqs['F'] = 5;

    node *root = build_tree(freqs);
    char code[MAX_SYMS][MAX_CODE];
    int lengths[MAX_SYMS];
    build_codes(root, code, lengths);

    int prefix_free = 1;
    for (int i = 0; i < MAX_SYMS; i++)
    {
        if (lengths[i] == 0) continue;
        for (int j = 0; j < MAX_SYMS; j++)
        {
            if (i == j || lengths[j] == 0) continue;
            if (lengths[i] <= lengths[j] &&
                strncmp(code[i], code[j], lengths[i]) == 0)
            {
                printf("    prefix conflict: '%c' (%.*s) vs '%c' (%s)\n",
                       (char)i, lengths[i], code[i], (char)j, code[j]);
                prefix_free = 0;
            }
        }
    }

    check("no code is a prefix of another", prefix_free);

    /* Also verify the most frequent gets the shortest code */
    check("'A' has shorter or equal code than 'F'",
          lengths['A'] <= lengths['F']);

    free_tree(root);
    printf("\n");
}

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "test") == 0)
    {
        printf("====================================\n");
        printf("          HUFFMAN CODING            \n");
        printf("====================================\n\n");

        test_roundtrip();
        test_compression();
        test_code_property();

        printf("All tests complete.\n");
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "encode") == 0)
        return huffman_encode(stdin, stdout) == 0 ? 0 : 1;

    if (argc == 2 && strcmp(argv[1], "decode") == 0)
        return huffman_decode(stdin, stdout) == 0 ? 0 : 1;

    fprintf(stderr,
            "Usage:\n"
            "  %s encode < input > output.huf\n"
            "  %s decode < input.huf > output\n"
            "  %s test\n",
            argv[0], argv[0], argv[0]);
    return 1;
}
