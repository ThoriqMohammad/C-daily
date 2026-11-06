/* sha256.c
 *
 * SHA-256: the hash function behind Bitcoin, TLS, Git, and
 * basically everything else that needs a cryptographic digest.
 *
 * SHA-256 takes any input and produces a 256-bit (32-byte)
 * output. The output looks random but is deterministic: the
 * same input always gives the same output. Even one bit of
 * change in the input produces a completely different output
 * (the "avalanche effect").
 *
 * SHA-256 is one of the SHA-2 family, standardized by NIST in
 * 2001. It is used to:
 *   - verify file downloads (checksums)
 *   - sign TLS certificates
 *   - derive keys in PBKDF2, HKDF, HMAC
 *   - mine Bitcoin (double-SHA-256)
 *   - name commits in Git (SHA-1 historically, but SHA-256
 *     is now an option)
 *
 * What makes SHA-256 "secure":
 *   - Pre-image resistance: given H, hard to find x with H(x)=H
 *   - Second pre-image: given x, hard to find y with H(x)=H(y)
 *   - Collision resistance: hard to find any x, y with H(x)=H(y)
 *
 * SHA-256 is currently considered secure. SHA-1 was broken in
 * 2017 (SHAttered attack); SHA-256 has no known practical attacks.
 *
 * Usage:
 *   ./sha256 <file>
 *   ./sha256 -                  (hash stdin)
 *   ./sha256 test               (run self-tests)
 *
 * Skills practiced:
 *   - Bitwise operations at scale (SHA-256 uses AND/OR/XOR/NOT/ROTR)
 *   - Bit rotation (right-rotate, not just shift)
 *   - Message padding with length encoding
 *   - Big-endian byte order
 *   - Working with byte arrays as state (h0..h7)
 *   - Test vectors: verifying against known hashes
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- SHA-256 constants ---------- */

/* First 32 bits of the fractional parts of the cube roots of
 * the first 64 primes (2..311). These are the round constants. */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/* Initial hash values: first 32 bits of the fractional parts
 * of the square roots of the first 8 primes (2, 3, 5, 7, 11,
 * 13, 17, 19). These are the starting state. */
static const uint32_t H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/* ---------- bit operations ---------- */

/* Right-rotate (circular shift) by n bits */
static uint32_t rotr(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

/* SHA-256 functions */
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SIGMA0(x)    (rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22))
#define SIGMA1(x)    (rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25))
#define sigma0(x)    (rotr(x, 7) ^ rotr(x, 18) ^ ((x) >> 3))
#define sigma1(x)    (rotr(x, 17) ^ rotr(x, 19) ^ ((x) >> 10))

/* ---------- SHA-256 context ---------- */

typedef struct
{
    uint32_t state[8];       /* current hash state */
    uint64_t bit_count;      /* total bits processed */
    uint8_t  buffer[64];     /* partial block buffer */
    size_t   buffer_len;     /* bytes currently in buffer */
} sha256_ctx;

static void sha256_init(sha256_ctx *ctx)
{
    memcpy(ctx->state, H0, sizeof(H0));
    ctx->bit_count = 0;
    ctx->buffer_len = 0;
}

/* Process one 64-byte block */
static void sha256_process_block(sha256_ctx *ctx, const uint8_t *block)
{
    uint32_t w[64];

    /* Load 16 32-bit big-endian words from the block */
    for (int i = 0; i < 16; i++)
    {
        w[i] = ((uint32_t)block[i*4    ] << 24)
             | ((uint32_t)block[i*4 + 1] << 16)
             | ((uint32_t)block[i*4 + 2] << 8)
             |  (uint32_t)block[i*4 + 3];
    }

    /* Extend to 64 words */
    for (int i = 16; i < 64; i++)
        w[i] = sigma1(w[i-2]) + w[i-7] + sigma0(w[i-15]) + w[i-16];

    /* Working variables */
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];

    /* 64 rounds */
    for (int i = 0; i < 64; i++)
    {
        uint32_t t1 = h + SIGMA1(e) + CH(e, f, g) + K[i] + w[i];
        uint32_t t2 = SIGMA0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* Add the results back into the state */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

/* Update with more data (can be called multiple times for streaming) */
static void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len)
{
    ctx->bit_count += (uint64_t)len * 8;

    /* Fill any partial block first */
    if (ctx->buffer_len > 0)
    {
        size_t to_copy = 64 - ctx->buffer_len;
        if (to_copy > len)
            to_copy = len;
        memcpy(ctx->buffer + ctx->buffer_len, data, to_copy);
        ctx->buffer_len += to_copy;
        data += to_copy;
        len  -= to_copy;

        if (ctx->buffer_len == 64)
        {
            sha256_process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }

    /* Process full blocks directly */
    while (len >= 64)
    {
        sha256_process_block(ctx, data);
        data += 64;
        len  -= 64;
    }

    /* Save the partial block */
    if (len > 0)
    {
        memcpy(ctx->buffer, data, len);
        ctx->buffer_len = len;
    }
}

/* Finalize: apply padding and emit the 32-byte digest */
static void sha256_final(sha256_ctx *ctx, uint8_t digest[32])
{
    /* Padding: 0x80 followed by zeros, until total block length
     * is 56 mod 64. Then append the 64-bit big-endian bit count. */
    uint64_t bit_count = ctx->bit_count;

    /* Append 0x80 */
    uint8_t pad = 0x80;
    sha256_update(ctx, &pad, 1);

    /* Pad with zeros until 56 mod 64 */
    uint8_t zero = 0;
    while (ctx->buffer_len != 56)
        sha256_update(ctx, &zero, 1);

    /* Append the 64-bit length (big-endian) */
    uint8_t lenbuf[8];
    for (int i = 0; i < 8; i++)
        lenbuf[i] = (uint8_t)(bit_count >> (56 - i * 8));
    sha256_update(ctx, lenbuf, 8);

    /* Output the state as 32 bytes, big-endian */
    for (int i = 0; i < 8; i++)
    {
        digest[i*4    ] = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/* Convenience: hash a buffer in one call */
static void sha256(const uint8_t *data, size_t len, uint8_t digest[32])
{
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

/* ---------- output helpers ---------- */

static void print_hex(const uint8_t *bytes, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf("%02x", bytes[i]);
}

static void hash_string_to_hex(const char *s, char *out)
{
    uint8_t digest[32];
    sha256((const uint8_t *)s, strlen(s), digest);
    for (int i = 0; i < 32; i++)
        sprintf(out + i * 2, "%02x", digest[i]);
    out[64] = '\0';
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-55s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_known_vectors(void)
{
    printf("Test 1: known SHA-256 test vectors\n");

    struct { const char *input; const char *expected; } cases[] = {
        { "",
          "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
        { "abc",
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
        { "The quick brown fox jumps over the lazy dog",
          "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592" },
        { "The quick brown fox jumps over the lazy cog",
          "e4c4d8f3bf76b692de791a173e05321150f7a345b46484fe427f6acc7ecc81be" },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        char got[65];
        hash_string_to_hex(cases[i].input, got);
        char label[64];
        snprintf(label, sizeof(label), "SHA256(\"%.20s%s\")",
                 cases[i].input,
                 strlen(cases[i].input) > 20 ? "..." : "");
        check(label, strcmp(got, cases[i].expected) == 0);
    }

    printf("\n");
}

static void test_avalanche(void)
{
    printf("Test 2: avalanche effect\n");

    char h1[65], h2[65];

    hash_string_to_hex("abc", h1);
    hash_string_to_hex("abd", h2);   /* 1-bit change */

    printf("  SHA256(\"abc\") = %s\n", h1);
    printf("  SHA256(\"abd\") = %s\n", h2);

    /* Count differing characters (each hex digit is 4 bits) */
    int diff_bits = 0;
    for (int i = 0; i < 64; i++)
    {
        if (h1[i] != h2[i])
        {
            /* Count differing nibbles as a rough measure */
            unsigned a = (h1[i] >= '0' && h1[i] <= '9') ? h1[i] - '0' : h1[i] - 'a' + 10;
            unsigned b = (h2[i] >= '0' && h2[i] <= '9') ? h2[i] - '0' : h2[i] - 'a' + 10;
            unsigned x = a ^ b;
            while (x) { diff_bits += x & 1; x >>= 1; }
        }
    }

    printf("  differing bits: %d / 256 (%.1f%%)\n",
           diff_bits, 100.0 * diff_bits / 256);
    printf("  (a good hash should show ~50%%)\n");

    check("avalanche effect: 40-60% of bits differ",
          diff_bits >= 100 && diff_bits <= 160);

    printf("\n");
}

static void test_streaming(void)
{
    printf("Test 3: streaming matches one-shot\n");

    const char *msg = "streaming test: multiple updates";
    size_t len = strlen(msg);

    uint8_t one_shot[32];
    sha256((const uint8_t *)msg, len, one_shot);

    /* Stream in chunks */
    sha256_ctx ctx;
    sha256_init(&ctx);
    for (size_t i = 0; i < len; i++)
        sha256_update(&ctx, (const uint8_t *)msg + i, 1);

    uint8_t streamed[32];
    sha256_final(&ctx, streamed);

    check("byte-by-byte streaming == one-shot",
          memcmp(one_shot, streamed, 32) == 0);

    printf("\n");
}

static void test_empty_vs_single_byte(void)
{
    printf("Test 4: different inputs, different outputs\n");

    char h_empty[65], h_zero[65], h_one[65];

    uint8_t digest[32];
    sha256((const uint8_t *)"", 0, digest);
    for (int i = 0; i < 32; i++) sprintf(h_empty + i*2, "%02x", digest);

    uint8_t zero_byte = 0;
    sha256(&zero_byte, 1, digest);
    for (int i = 0; i < 32; i++) sprintf(h_zero + i*2, "%02x", digest);

    uint8_t one_byte = 1;
    sha256(&one_byte, 1, digest);
    for (int i = 0; i < 32; i++) sprintf(h_one + i*2, "%02x", digest);

    printf("  empty:  %s\n", h_empty);
    printf("  \\x00:   %s\n", h_zero);
    printf("  \\x01:   %s\n", h_one);

    check("empty != one zero byte", strcmp(h_empty, h_zero) != 0);
    check("zero byte != one byte",  strcmp(h_zero, h_one) != 0);

    printf("\n");
}

/* ---------- file hashing ---------- */

static int hash_file(FILE *fp)
{
    sha256_ctx ctx;
    sha256_init(&ctx);

    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        sha256_update(&ctx, buf, n);

    uint8_t digest[32];
    sha256_final(&ctx, digest);
    print_hex(digest, 32);
    printf("\n");
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "test") == 0)
    {
        printf("====================================\n");
        printf("             SHA-256                \n");
        printf("====================================\n\n");

        test_known_vectors();
        test_avalanche();
        test_streaming();
        test_empty_vs_single_byte();

        printf("All tests complete.\n");
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "-") == 0)
        return hash_file(stdin);

    if (argc == 2)
    {
        FILE *fp = fopen(argv[1], "rb");
        if (!fp) { perror(argv[1]); return 1; }
        int rc = hash_file(fp);
        fclose(fp);
        return rc;
    }

    fprintf(stderr,
            "Usage:\n"
            "  %s <file>    hash a file\n"
            "  %s -         hash stdin\n"
            "  %s test      run self-tests\n",
            argv[0], argv[0], argv[0]);
    return 1;
}
