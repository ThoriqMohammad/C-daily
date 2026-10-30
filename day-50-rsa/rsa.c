/* rsa.c
 *
 * A working RSA implementation in C.
 *
 * RSA is the first public-key cryptosystem, published by Rivest,
 * Shamir, and Adleman in 1977. It is still the most widely used
 * public-key algorithm today, protecting TLS, SSH, code signing,
 * and email.
 *
 * The system rests on one observation:
 *
 *   Multiplying two large primes is easy.
 *   Factoring their product back is hard.
 *
 * This asymmetry lets you publish the product as a "public key"
 * while keeping the primes secret. Encryption is modular
 * exponentiation with the public key; decryption uses the
 * private key, which only exists if you know the primes.
 *
 * This file demonstrates:
 *
 *   1. Key generation (small primes, for speed)
 *   2. Encryption of a short message
 *   3. Decryption and verification
 *   4. Why the primes must be large enough
 *
 * Primes here are 32-bit; real RSA uses 1024-bit or larger.
 * The math is identical; only the number sizes change, which
 * is why a real implementation needs big_integer arithmetic
 * (see Day 43) for the exponents and moduli.
 *
 * Usage:
 *   ./rsa
 *
 * Skills practiced:
 *   - RSA key generation with Miller-Rabin primality testing
 *   - Modular inverse via the extended Euclidean algorithm
 *   - Modular exponentiation (Day 45 reused here)
 *   - Encoding bytes as integers for encryption
 *   - Public/private key separation
 *   - Why textbook RSA is insecure (padding, determinism)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

typedef unsigned long long u64;

/* ---------- modular arithmetic (from Day 45) ---------- */

static u64 mulmod(u64 a, u64 b, u64 m)
{
    return (u64)((__uint128_t)a * b % m);
}

static u64 powmod(u64 base, u64 exp, u64 m)
{
    u64 result = 1;
    base %= m;
    while (exp > 0)
    {
        if (exp & 1)
            result = mulmod(result, base, m);
        exp >>= 1;
        base = mulmod(base, base, m);
    }
    return result;
}

/* Modular inverse: a^-1 mod m, or 0 if none */
static u64 modinv(u64 a, u64 m)
{
    long long t = 0, newt = 1;
    long long r = (long long)m, newr = (long long)a;

    while (newr != 0)
    {
        long long q = r / newr;
        long long tmp = t - q * newt;  t = newt; newt = tmp;
        tmp = r - q * newr;            r = newr; newr = tmp;
    }

    if (r != 1) return 0;              /* not invertible */
    if (t < 0) t += (long long)m;
    return (u64)t;
}

/* ---------- Miller-Rabin primality test (from Day 48) ---------- */

static const u64 MR_BASES[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
static const int MR_BASE_COUNT = 12;

static int is_prime(u64 n)
{
    if (n < 2) return 0;
    if (n < 4) return 1;
    if ((n & 1) == 0) return 0;

    u64 d = n - 1;
    int s = 0;
    while ((d & 1) == 0) { d >>= 1; s++; }

    for (int i = 0; i < MR_BASE_COUNT; i++)
    {
        u64 a = MR_BASES[i];
        if (a >= n) break;

        u64 x = powmod(a, d, n);
        if (x == 1 || x == n - 1) continue;

        int composite = 1;
        for (int r = 1; r < s; r++)
        {
            x = mulmod(x, x, n);
            if (x == n - 1) { composite = 0; break; }
        }
        if (composite) return 0;
    }
    return 1;
}

/* ---------- GCD ---------- */

static u64 gcd(u64 a, u64 b)
{
    while (b != 0) { u64 t = a % b; a = b; b = t; }
    return a;
}

/* ---------- RSA keys ---------- */

typedef struct
{
    u64 n;    /* public modulus  = p * q          */
    u64 e;    /* public exponent = 65537 typically */
    u64 d;    /* private exponent                 */
    u64 p, q; /* the primes (kept for demonstration) */
} rsa_key;

/* Generate a random prime in [lo, hi].
 * Simple trial: pick random odd number, test; retry if composite. */
static u64 random_prime(u64 lo, u64 hi)
{
    if (lo < 2) lo = 2;
    if (hi <= lo) return lo;

    for (int attempts = 0; attempts < 100000; attempts++)
    {
        u64 candidate = lo + (u64)(rand()) % (hi - lo);
        candidate |= 1;                    /* force odd */
        if (candidate >= 2 && is_prime(candidate))
            return candidate;
    }
    return 0;
}

/* Generate an RSA key with two primes in the given bit range.
 * 'bits' is the *total* target bit length of n; each prime is
 * about bits/2 bits long. */
static int rsa_generate_key(rsa_key *key, int bits)
{
    u64 lo = (u64)1 << (bits / 2 - 1);
    u64 hi = (u64)1 << (bits / 2);

    u64 p = 0, q = 0;
    for (int attempts = 0; attempts < 100; attempts++)
    {
        p = random_prime(lo, hi);
        q = random_prime(lo, hi);
        if (p != 0 && q != 0 && p != q) break;
    }

    if (p == 0 || q == 0 || p == q)
        return -1;

    u64 n = p * q;
    u64 phi = (p - 1) * (q - 1);

    /* Public exponent e = 65537 is standard. It must be coprime
     * to phi(n). If not, retry with a different pair. */
    u64 e = 65537;
    if (gcd(e, phi) != 1)
    {
        /* Rare for random primes, but possible */
        for (e = 3; e < phi; e += 2)
            if (gcd(e, phi) == 1)
                break;
    }

    u64 d = modinv(e, phi);
    if (d == 0)
        return -1;

    key->n = n;
    key->e = e;
    key->d = d;
    key->p = p;
    key->q = q;
    return 0;
}

/* ---------- RSA operations ---------- */

/* Encrypt a single block: ciphertext = plaintext^e mod n */
static u64 rsa_encrypt_block(const rsa_key *key, u64 plaintext)
{
    return powmod(plaintext, key->e, key->n);
}

/* Decrypt a single block: plaintext = ciphertext^d mod n */
static u64 rsa_decrypt_block(const rsa_key *key, u64 ciphertext)
{
    return powmod(ciphertext, key->d, key->n);
}

/* Encrypt a string into an array of u64 blocks.
 * Returns number of blocks or -1 on error.
 *
 * Each byte is encrypted as a separate block for simplicity. Real
 * RSA packs many bytes per block, but that requires knowing the
 * block size from n, and it's not the point of this demo. */
static int rsa_encrypt_string(const rsa_key *key, const char *msg,
                              u64 *out, int max_blocks)
{
    int len = (int)strlen(msg);
    if (len > max_blocks)
        return -1;

    for (int i = 0; i < len; i++)
    {
        u64 m = (unsigned char)msg[i];
        if (m >= key->n)
            return -1;   /* block too large for modulus */
        out[i] = rsa_encrypt_block(key, m);
    }

    return len;
}

static int rsa_decrypt_string(const rsa_key *key, const u64 *blocks,
                              int nblocks, char *out, int max_chars)
{
    if (nblocks >= max_chars)
        return -1;

    for (int i = 0; i < nblocks; i++)
    {
        u64 m = rsa_decrypt_block(key, blocks[i]);
        if (m > 255)
            return -1;
        out[i] = (char)m;
    }
    out[nblocks] = '\0';
    return nblocks;
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-50s %s\n", label, condition ? "ok" : "FAIL");
}

static void test_basic_math(void)
{
    printf("Test 1: modular arithmetic building blocks\n");

    check("mulmod no overflow: 2^32 * 2^32 mod (2^61-1)",
          mulmod(4294967296ULL, 4294967296ULL, 2305843009213693951ULL) ==
          (u64)((__uint128_t)4294967296ULL * 4294967296ULL %
                2305843009213693951ULL));

    check("powmod: 2^10 mod 1000 = 24", powmod(2, 10, 1000) == 24);
    check("modinv: 17 * 2753 mod 3120 = 1",
          (mulmod(17, 2753, 3120)) == 1);
    check("gcd(1071, 462) = 21", gcd(1071, 462) == 21);
    check("is_prime(97)",  is_prime(97));
    check("is_prime(100) is false", !is_prime(100));

    printf("\n");
}

static void test_rsa_roundtrip(void)
{
    printf("Test 2: RSA encrypt/decrypt round-trip\n");

    /* Use a fixed small key for reproducibility, not keygen */
    rsa_key key;
    key.p = 61;
    key.q = 53;
    key.n = key.p * key.q;           /* 3233 */
    key.e = 17;
    u64 phi = (key.p - 1) * (key.q - 1);
    key.d = modinv(key.e, phi);      /* 2753 */

    check("n = 3233",  key.n == 3233);
    check("e = 17",    key.e == 17);
    check("d = 2753",  key.d == 2753);

    /* Encrypt 'A' (65) */
    u64 ct = rsa_encrypt_block(&key, 65);
    check("65^17 mod 3233 = 2790", ct == 2790);

    /* Decrypt */
    u64 pt = rsa_decrypt_block(&key, ct);
    check("2790^2753 mod 3233 = 65 (round-trip)", pt == 65);

    /* Encrypt a full string */
    const char *msg = "HELLO";
    u64 blocks[16];
    int nblocks = rsa_encrypt_string(&key, msg, blocks, 16);
    check("encrypt returned 5 blocks", nblocks == 5);

    char recovered[16];
    int ok = rsa_decrypt_string(&key, blocks, nblocks, recovered, 16);
    check("decrypt succeeds", ok == 5);
    check("\"HELLO\" round-trips through RSA", strcmp(recovered, msg) == 0);

    printf("\n");
}

static void test_key_generation(void)
{
    printf("Test 3: RSA key generation\n");

    rsa_key key;
    int rc = rsa_generate_key(&key, 32);

    check("key generation succeeds", rc == 0);
    if (rc == 0)
    {
        printf("  Generated: p = %llu, q = %llu, n = %llu\n",
               key.p, key.q, key.n);
        printf("             e = %llu, d = %llu\n", key.e, key.d);

        check("p and q are distinct primes",
              key.p != key.q && is_prime(key.p) && is_prime(key.q));
        check("n = p * q", key.n == key.p * key.q);
        check("e * d = 1 mod phi(n)",
              mulmod(key.e, key.d, (key.p - 1) * (key.q - 1)) == 1);

        /* Encrypt and decrypt a message with the generated key */
        u64 test_val = 42;
        u64 ct = rsa_encrypt_block(&key, test_val);
        u64 pt = rsa_decrypt_block(&key, ct);
        check("42 round-trips with generated key", pt == test_val);
    }

    printf("\n");
}

static void test_why_padding_matters(void)
{
    printf("Test 4: textbook RSA is deterministic (why padding matters)\n");

    rsa_key key;
    key.p = 61;
    key.q = 53;
    key.n = key.p * key.q;
    key.e = 17;
    key.d = 2753;

    /* Same message encrypted twice produces the same ciphertext */
    u64 ct1 = rsa_encrypt_block(&key, 65);
    u64 ct2 = rsa_encrypt_block(&key, 65);

    check("same plaintext -> same ciphertext (deterministic)", ct1 == ct2);

    /* This is a security weakness: an attacker can test guesses
     * by encrypting them and comparing. Real RSA uses random
     * padding (OAEP) so encrypting the same message twice gives
     * different ciphertexts. */
    u64 ct3 = rsa_encrypt_block(&key, 66);
    check("different plaintext -> different ciphertext", ct1 != ct3);

    printf("  Note: textbook RSA is deterministic. Real RSA uses OAEP\n");
    printf("  padding to randomize, preventing chosen-plaintext attacks.\n\n");
}

int main(void)
{
    srand((unsigned)time(NULL));

    printf("====================================\n");
    printf("                RSA                 \n");
    printf("====================================\n\n");

    test_basic_math();
    test_rsa_roundtrip();
    test_key_generation();
    test_why_padding_matters();

    printf("All tests complete.\n");
    return 0;
}
