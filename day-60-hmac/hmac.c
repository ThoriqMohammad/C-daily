/* hmac.c
 *
 * HMAC: Hash-based Message Authentication Code.
 *
 * HMAC is a construction that takes a hash function (like SHA-256)
 * and adds a key, producing a MAC: a tag that proves both the
 * authenticity and the integrity of a message.
 *
 * Why you can't just hash the key into the message:
 *
 *   H(key || message)          <- length-extension attack
 *   H(message || key)          <- same problem
 *   H(key || message || key)   <- vulnerable to other tricks
 *
 * HMAC (RFC 2104) solves this cleanly:
 *
 *   HMAC(K, m) = H((K ^ opad) || H((K ^ ipad) || m))
 *
 * where:
 *   K     is the key (padded or hashed to the hash's block size)
 *   ipad  is the byte 0x36 repeated block-size times
 *   opad  is the byte 0x5c repeated block-size times
 *   ||    is concatenation
 *
 * This "double hash" structure is secure under the assumption
 * that H is a collision-resistant hash. It's been analyzed for
 * decades and is standard.
 *
 * HMAC-SHA256 is the most common HMAC in use today. It appears in:
 *   - TLS: for every handshake and every record
 *   - JWT: for signing claims
 *   - AWS signatures
 *   - PBKDF2: as the underlying hash
 *   - TOTP / HOTP: for one-time passwords
 *   - Signal: for message authentication
 *
 * Usage:
 *   ./hmac                        run self-tests
 *   ./hmac <key> <message>        compute HMAC-SHA256
 *
 * Skills practiced:
 *   - HMAC construction from a hash
 *   - Key padding to block size
 *   - RFC 2104 verification against official test vectors
 *   - Constant-time comparison (timing-attack defense)
 *   - Reusing a hash via the streaming interface
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- SHA-256 (from Day 57) ---------- */

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

static const uint32_t H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

static uint32_t rotr32(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SIGMA0(x)    (rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22))
#define SIGMA1(x)    (rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25))
#define sigma0(x)    (rotr32(x, 7) ^ rotr32(x, 18) ^ ((x) >> 3))
#define sigma1(x)    (rotr32(x, 17) ^ rotr32(x, 19) ^ ((x) >> 10))

typedef struct
{
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t  buffer[64];
    size_t   buffer_len;
} sha256_ctx;

static void sha256_init(sha256_ctx *ctx)
{
    memcpy(ctx->state, H0, sizeof(H0));
    ctx->bit_count = 0;
    ctx->buffer_len = 0;
}

static void sha256_process_block(sha256_ctx *ctx, const uint8_t *block)
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4    ] << 24)
             | ((uint32_t)block[i*4 + 1] << 16)
             | ((uint32_t)block[i*4 + 2] << 8)
             |  (uint32_t)block[i*4 + 3];

    for (int i = 16; i < 64; i++)
        w[i] = sigma1(w[i-2]) + w[i-7] + sigma0(w[i-15]) + w[i-16];

    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];

    for (int i = 0; i < 64; i++)
    {
        uint32_t t1 = h + SIGMA1(e) + CH(e, f, g) + K[i] + w[i];
        uint32_t t2 = SIGMA0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len)
{
    ctx->bit_count += (uint64_t)len * 8;

    if (ctx->buffer_len > 0)
    {
        size_t to_copy = 64 - ctx->buffer_len;
        if (to_copy > len) to_copy = len;
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

    while (len >= 64)
    {
        sha256_process_block(ctx, data);
        data += 64;
        len  -= 64;
    }

    if (len > 0)
    {
        memcpy(ctx->buffer, data, len);
        ctx->buffer_len = len;
    }
}

static void sha256_final(sha256_ctx *ctx, uint8_t digest[32])
{
    uint64_t bit_count = ctx->bit_count;
    uint8_t pad = 0x80;
    sha256_update(ctx, &pad, 1);
    uint8_t zero = 0;
    while (ctx->buffer_len != 56)
        sha256_update(ctx, &zero, 1);

    uint8_t lenbuf[8];
    for (int i = 0; i < 8; i++)
        lenbuf[i] = (uint8_t)(bit_count >> (56 - i * 8));
    sha256_update(ctx, lenbuf, 8);

    for (int i = 0; i < 8; i++)
    {
        digest[i*4    ] = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/* ---------- HMAC-SHA256 ---------- */

#define BLOCK_SIZE 64    /* SHA-256 block size in bytes */
#define HASH_SIZE  32    /* SHA-256 digest size in bytes */

/* Compute HMAC-SHA256(key, msg) into out[32].
 * Both key and msg may be NULL if their length is 0. */
void hmac_sha256(const uint8_t *key, size_t key_len,
                 const uint8_t *msg, size_t msg_len,
                 uint8_t out[32])
{
    uint8_t k_block[BLOCK_SIZE];
    memset(k_block, 0, BLOCK_SIZE);

    /* If key is longer than block size, hash it first.
     * Otherwise use it directly (padded with zeros). */
    if (key_len > BLOCK_SIZE)
    {
        sha256_ctx kctx;
        sha256_init(&kctx);
        sha256_update(&kctx, key, key_len);
        sha256_final(&kctx, k_block);
        /* remaining 32 bytes stay zero */
    }
    else if (key_len > 0)
    {
        memcpy(k_block, key, key_len);
    }

    /* Build ipad block and opad block */
    uint8_t ipad[BLOCK_SIZE];
    uint8_t opad[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        ipad[i] = k_block[i] ^ 0x36;
        opad[i] = k_block[i] ^ 0x5c;
    }

    /* Inner hash: H(K ^ ipad || message) */
    uint8_t inner[HASH_SIZE];
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, ipad, BLOCK_SIZE);
    if (msg_len > 0)
        sha256_update(&ctx, msg, msg_len);
    sha256_final(&ctx, inner);

    /* Outer hash: H(K ^ opad || inner) */
    sha256_init(&ctx);
    sha256_update(&ctx, opad, BLOCK_SIZE);
    sha256_update(&ctx, inner, HASH_SIZE);
    sha256_final(&ctx, out);
}

/* Constant-time comparison of two HMACs.
 * Regular memcmp() exits early on the first mismatch, which
 * leaks timing information. An attacker who can measure
 * comparison time can learn how much of the tag is correct
 * and forge the rest byte-by-byte. This function always
 * reads all `len` bytes. */
int hmac_verify(const uint8_t *expected, const uint8_t *actual, size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++)
        diff |= expected[i] ^ actual[i];
    return diff == 0;
}

/* ---------- output helpers ---------- */

static void print_hex(const uint8_t *bytes, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf("%02x", bytes[i]);
}

static void hex_string(const uint8_t *bytes, size_t len, char *out)
{
    for (size_t i = 0; i < len; i++)
        sprintf(out + i * 2, "%02x", bytes[i]);
    out[len * 2] = '\0';
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-55s %s\n", label, condition ? "ok" : "FAIL");
}

/* Test vectors from RFC 4231 (HMAC-SHA256 test cases) */
static void test_rfc4231(void)
{
    printf("Test 1: RFC 4231 test vectors\n");

    /* Test Case 1: key = 0x0b repeated 20 times, data = "Hi There" */
    uint8_t key1[20];
    memset(key1, 0x0b, sizeof(key1));
    uint8_t out[32];
    char hex[65];

    hmac_sha256(key1, sizeof(key1),
                (const uint8_t *)"Hi There", 8, out);
    hex_string(out, 32, hex);
    check("RFC 4231 TC1",
          strcmp(hex, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7") == 0);

    /* Test Case 2: key = "Jefe", data = "what do ya want for nothing?" */
    hmac_sha256((const uint8_t *)"Jefe", 4,
                (const uint8_t *)"what do ya want for nothing?", 28, out);
    hex_string(out, 32, hex);
    check("RFC 4231 TC2",
          strcmp(hex, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843") == 0);

    /* Test Case 3: key = 0xaa repeated 20 times, data = 0xdd repeated 50 times */
    uint8_t key3[20];
    memset(key3, 0xaa, sizeof(key3));
    uint8_t data3[50];
    memset(data3, 0xdd, sizeof(data3));
    hmac_sha256(key3, sizeof(key3), data3, sizeof(data3), out);
    hex_string(out, 32, hex);
    check("RFC 4231 TC3",
          strcmp(hex, "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe") == 0);

    printf("\n");
}

static void test_long_key(void)
{
    printf("Test 2: key longer than block size is hashed first\n");

    /* RFC 4231 TC6: key = 0xaa repeated 131 bytes (> 64) */
    uint8_t key[131];
    memset(key, 0xaa, sizeof(key));

    const char *msg = "Test Using Larger Than Block-Size Key - Hash Key First";
    uint8_t out[32];
    hmac_sha256(key, sizeof(key),
                (const uint8_t *)msg, strlen(msg), out);

    char hex[65];
    hex_string(out, 32, hex);
    check("RFC 4231 TC6 (long key)",
          strcmp(hex, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54") == 0);

    /* Verify that a 131-byte key and its 32-byte SHA-256 hash
     * produce the same HMAC. This is the key-hashing property. */
    uint8_t key_hash[32];
    sha256_ctx kctx;
    sha256_init(&kctx);
    sha256_update(&kctx, key, sizeof(key));
    sha256_final(&kctx, key_hash);

    uint8_t out2[32];
    hmac_sha256(key_hash, 32, (const uint8_t *)msg, strlen(msg), out2);

    check("long key equals its hashed form",
          memcmp(out, out2, 32) == 0);

    printf("\n");
}

static void test_key_sensitivity(void)
{
    printf("Test 3: different keys produce different HMACs\n");

    const char *msg = "the same message";
    uint8_t out1[32], out2[32];

    hmac_sha256((const uint8_t *)"key1", 4,
                (const uint8_t *)msg, strlen(msg), out1);
    hmac_sha256((const uint8_t *)"key2", 4,
                (const uint8_t *)msg, strlen(msg), out2);

    check("HMAC(key1, msg) != HMAC(key2, msg)",
          memcmp(out1, out2, 32) != 0);

    /* Same key and message -> same HMAC (deterministic) */
    uint8_t out3[32];
    hmac_sha256((const uint8_t *)"key1", 4,
                (const uint8_t *)msg, strlen(msg), out3);

    check("HMAC is deterministic",
          memcmp(out1, out3, 32) == 0);

    printf("\n");
}

static void test_constant_time(void)
{
    printf("Test 4: constant-time comparison\n");

    uint8_t a[32] = {0};
    uint8_t b[32] = {0};

    check("identical tags verify", hmac_verify(a, b, 32) == 1);

    b[31] = 1;
    check("single-bit difference fails", hmac_verify(a, b, 32) == 0);

    b[31] = 0;
    b[0] = 0xff;
    check("first-byte difference fails", hmac_verify(a, b, 32) == 0);

    printf("  (all differences fail regardless of position — same\n");
    printf("   time taken, which is what makes it safe)\n");

    printf("\n");
}

/* ---------- command-line mode ---------- */

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf("====================================\n");
        printf("            HMAC-SHA256             \n");
        printf("====================================\n\n");

        test_rfc4231();
        test_long_key();
        test_key_sensitivity();
        test_constant_time();

        printf("All tests complete.\n");
        return 0;
    }

    if (argc == 3)
    {
        const char *key = argv[1];
        const char *msg = argv[2];
        uint8_t out[32];
        hmac_sha256((const uint8_t *)key, strlen(key),
                    (const uint8_t *)msg, strlen(msg), out);
        print_hex(out, 32);
        printf("\n");
        return 0;
    }

    fprintf(stderr,
            "Usage:\n"
            "  %s                  run self-tests\n"
            "  %s <key> <message>  compute HMAC-SHA256\n",
            argv[0], argv[0]);
    return 1;
}
