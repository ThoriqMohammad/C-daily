/* lz77.c
 *
 * LZ77: the sliding-window compression algorithm behind gzip, PNG,
 * ZIP, and every modern general-purpose compressor.
 *
 * The idea:
 *
 *   When you see a repeated substring, emit a backreference to it
 *   instead of the substring itself.
 *
 * LZ77 is the "LZ" in LZW, LZMA, LZ4, deflate, and dozens of
 * others. It is the workhorse of the compression world.
 *
 * The algorithm:
 *
 *   1. Keep a sliding window of the last WINDOW_SIZE bytes.
 *   2. At each position, find the longest match of the upcoming
 *      data within the window.
 *   3. Emit a (offset, length, next_byte) triple:
 *        offset    = how far back the match starts
 *        length    = how many bytes the match covers
 *        next_byte = the byte after the match (the "literal")
 *   4. Advance by length + 1 and repeat.
 *
 * Decoding is trivial: for each triple, copy `length` bytes from
 * `offset` positions back, then append `next_byte`.
 *
 * Our format (simplified, uncompressed):
 *
 *   Each triple is written as three bytes:
 *     - offset  (1 byte, 0..255)
 *     - length  (1 byte, 0..255)
 *     - literal (1 byte)
 *
 *   With a 255-byte window and 255-byte match, we get compression
 *   only on repeats of 3 or more (a 3-byte literal is 3 bytes, a
 *   3-byte match is 3 bytes — no savings below 3).
 *
 * Production LZ77 uses bit-packed triples (Huffman-coded via
 * Day 47's technique — together they form DEFLATE).
 *
 * Usage:
 *   ./lz77 encode < input > output.lz77
 *   ./lz77 decode < output.lz77 > restored
 *   ./lz77 test
 *
 * Skills practiced:
 *   - Sliding window algorithm
 *   - Backreference encoding
 *   - Naive O(n * window * match) matching (fast enough here)
 *   - Binary file format design
 *   - LZ77 + Huffman = DEFLATE (conceptual bridge to Day 47)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define WINDOW_SIZE 255   /* max backreference offset */
#define MAX_MATCH   255   /* max match length         */
#define MIN_MATCH   3     /* no point below 3 bytes   */

/* ---------- helpers ---------- */

/* Find the longest match for data[pos..pos+MAX_MATCH) within
 * data[pos-WINDOW_SIZE..pos). Returns (best_offset, best_length). */
static void find_longest_match(const uint8_t *data, size_t size, size_t pos,
                               int *out_offset, int *out_length)
{
    *out_offset = 0;
    *out_length = 0;

    if (pos == 0)
        return;

    size_t window_start = (pos > WINDOW_SIZE) ? pos - WINDOW_SIZE : 0;
    size_t max_match_len = size - pos;
    if (max_match_len > MAX_MATCH)
        max_match_len = MAX_MATCH;

    for (size_t start = window_start; start < pos; start++)
    {
        size_t len = 0;
        while (len < max_match_len &&
               data[start + len] == data[pos + len])
        {
            len++;
        }

        /* If we've used all available data, stop early */
        if (len > (size_t)*out_length)
        {
            *out_length = (int)len;
            *out_offset = (int)(pos - start);
        }
    }
}

/* ---------- encode ---------- */

typedef struct
{
    FILE *fp;
} triple_writer;

static void tw_triple(triple_writer *tw,
                      uint8_t offset, uint8_t length, uint8_t literal)
{
    fputc(offset, tw->fp);
    fputc(length, tw->fp);
    fputc(literal, tw->fp);
}

static int lz77_encode(FILE *in, FILE *out)
{
    /* Read the whole input. LZ77 is streaming in theory, but
     * the reader is much simpler if we buffer everything. */
    size_t cap = 4096;
    size_t size = 0;
    uint8_t *data = malloc(cap);
    if (!data) { perror("malloc"); return -1; }

    int c;
    while ((c = fgetc(in)) != EOF)
    {
        if (size == cap)
        {
            cap *= 2;
            uint8_t *tmp = realloc(data, cap);
            if (!tmp) { perror("realloc"); free(data); return -1; }
            data = tmp;
        }
        data[size++] = (uint8_t)c;
    }

    /* Header: original size so decode knows when to stop */
    fwrite(&size, sizeof(size), 1, out);

    triple_writer tw = { .fp = out };

    size_t pos = 0;
    while (pos < size)
    {
        int offset, length;
        find_longest_match(data, size, pos, &offset, &length);

        if (length < MIN_MATCH)
        {
            /* Emit the current byte as a literal, no backreference */
            tw_triple(&tw, 0, 0, data[pos]);
            pos++;
        }
        else
        {
            /* Emit a backreference plus the next literal byte.
             * The literal must be a byte that follows the match;
             * if the match reaches the end of the input, use 0
             * (the decoder stops when it has produced `size` bytes). */
            uint8_t literal = (pos + length < size) ? data[pos + length] : 0;
            tw_triple(&tw, (uint8_t)offset, (uint8_t)length, literal);
            pos += length;
            if (pos < size)
                pos++;   /* consume the literal byte */
        }
    }

    free(data);
    return 0;
}

/* ---------- decode ---------- */

static int lz77_decode(FILE *in, FILE *out)
{
    size_t expected_size;
    if (fread(&expected_size, sizeof(expected_size), 1, in) != 1)
    {
        fprintf(stderr, "Cannot read header.\n");
        return -1;
    }

    /* Buffer for the decoded output; backreferences need the
     * last WINDOW_SIZE bytes to be accessible. */
    uint8_t *buf = malloc(expected_size + 1);
    if (!buf) { perror("malloc"); return -1; }

    size_t out_len = 0;

    while (out_len < expected_size)
    {
        int offset = fgetc(in);
        int length = fgetc(in);
        int literal = fgetc(in);

        if (offset == EOF || length == EOF || literal == EOF)
        {
            fprintf(stderr, "Unexpected EOF.\n");
            free(buf);
            return -1;
        }

        /* Copy `length` bytes from `offset` back */
        for (int i = 0; i < length; i++)
        {
            size_t src = out_len - offset;
            buf[out_len++] = buf[src];
        }

        /* Append the literal */
        if (out_len < expected_size)
            buf[out_len++] = (uint8_t)literal;
    }

    fwrite(buf, 1, out_len, out);
    free(buf);
    return 0;
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-50s %s\n", label, condition ? "ok" : "FAIL");
}

/* Encode and decode in memory, return 1 if round-trip succeeds */
static int round_trip(const uint8_t *data, size_t len)
{
    FILE *enc = tmpfile();
    FILE *dec = tmpfile();
    FILE *input = tmpfile();
    if (!enc || !dec || !input) return 0;

    fwrite(data, 1, len, input);
    rewind(input);

    if (lz77_encode(input, enc) != 0) goto fail;
    rewind(enc);
    if (lz77_decode(enc, dec) != 0) goto fail;
    rewind(dec);

    for (size_t i = 0; i < len; i++)
    {
        int c = fgetc(dec);
        if (c != data[i]) goto fail;
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

    const char *cases[] = {
        "",
        "A",
        "AB",
        "hello",
        "hello, hello, hello",
        "aaaaaaaaaaaaaaaaaaaaaaaa",       /* very repetitive */
        "the quick brown fox jumps over the lazy dog",
        "ABCABCABCABCABCABCABCABC",        /* periodic */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        char label[64];
        snprintf(label, sizeof(label), "round-trip \"%s\"",
                 strlen(cases[i]) > 30 ? "(long)" : cases[i]);
        check(label, round_trip((const uint8_t *)cases[i], strlen(cases[i])));
    }
    printf("\n");
}

static void test_compression(void)
{
    printf("Test 2: compression ratio\n");

    /* Highly repetitive input — LZ77 should shine */
    size_t n = 5000;
    char *input = malloc(n);
    for (size_t i = 0; i < n; i++)
        input[i] = 'A' + (i % 3);  /* "ABCABCABC..." */

    FILE *enc = tmpfile();
    FILE *in = tmpfile();
    fwrite(input, 1, n, in);
    rewind(in);

    lz77_encode(in, enc);
    long encoded = ftell(enc);

    printf("  original: %zu bytes, encoded: %ld bytes (%.1f%%)\n",
           n, encoded, 100.0 * encoded / (double)n);

    check("compresses repetitive input below 50%",
          encoded < (long)(n / 2));

    check("round-trip on repetitive input",
          round_trip((const uint8_t *)input, n));

    fclose(enc); fclose(in);
    free(input);
    printf("\n");
}

static void test_short_matches(void)
{
    printf("Test 3: no false savings on random-ish data\n");

    /* Short random-looking string with no long repeats */
    const char *s = "The quick brown fox jumps";
    FILE *enc = tmpfile();
    FILE *in = tmpfile();
    fwrite(s, 1, strlen(s), in);
    rewind(in);

    lz77_encode(in, enc);
    long encoded = ftell(enc);

    printf("  original: %zu bytes, encoded: %ld bytes\n",
           strlen(s), encoded);

    /* Each literal is a 3-byte triple, so output is larger
     * than input for non-repetitive short strings */
    check("output is larger for non-repetitive data (expected)",
          encoded > (long)strlen(s));

    check("but it still round-trips",
          round_trip((const uint8_t *)s, strlen(s)));

    fclose(enc); fclose(in);
    printf("\n");
}

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "test") == 0)
    {
        printf("====================================\n");
        printf("              LZ77                  \n");
        printf("====================================\n\n");

        test_roundtrip();
        test_compression();
        test_short_matches();

        printf("All tests complete.\n");
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "encode") == 0)
        return lz77_encode(stdin, stdout) == 0 ? 0 : 1;

    if (argc == 2 && strcmp(argv[1], "decode") == 0)
        return lz77_decode(stdin, stdout) == 0 ? 0 : 1;

    fprintf(stderr,
            "Usage:\n"
            "  %s encode < input > output.lz77\n"
            "  %s decode < input.lz77 > restored\n"
            "  %s test\n",
            argv[0], argv[0], argv[0]);
    return 1;
}
