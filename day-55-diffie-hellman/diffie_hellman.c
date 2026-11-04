/* diffie_hellman.c
 *
 * Diffie-Hellman key exchange: two parties agree on a shared
 * secret over an insecure channel, without ever transmitting
 * the secret.
 *
 * Published in 1976, Diffie-Hellman was the first public-key
 * protocol, predating RSA by a year. It solved a problem that
 * had seemed impossible: how can two people who have never met
 * agree on a secret, while an eavesdropper hears everything?
 *
 * The protocol:
 *
 *   1. Alice and Bob agree publicly on a prime p and a base g.
 *   2. Alice picks a secret a. She sends A = g^a mod p to Bob.
 *   3. Bob picks a secret b. He sends B = g^b mod p to Alice.
 *   4. Alice computes s = B^a mod p.
 *   5. Bob computes s = A^b mod p.
 *   6. Both have the same s, because:
 *
 *        (g^b)^a = g^(ab) = (g^a)^b  (mod p)
 *
 * An eavesdropper sees p, g, A, B — but not a or b. Recovering
 * s from A = g^a mod p is the DISCRETE LOGARITHM problem, which
 * is believed to be hard for large p (2048+ bits).
 *
 * This implementation:
 *   - uses 64-bit integers (toy-sized, for clarity)
 *   - demonstrates the full exchange between two simulated parties
 *   - shows what an eavesdropper sees
 *   - verifies that both sides derive the same key
 *
 * Usage:
 *   ./diffie_hellman
 *
 * Skills practiced:
 *   - Diffie-Hellman key exchange protocol
 *   - Modular exponentiation (reused from Day 45)
 *   - The discrete logarithm problem
 *   - Small subgroup attacks and safe primes
 *   - Why authenticated DH is necessary (MITM attack)
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

/* ---------- Miller-Rabin (from Day 48) ---------- */

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

/* ---------- Diffie-Hellman ---------- */

/* The public parameters shared by everyone. In real DH, these
 * are standardized constants. Here we use a 61-bit prime with
 * a small generator, because it's large enough to be interesting
 * but small enough to print. */
static const u64 DH_PRIME = 2305843009213693951ULL;  /* 2^61 - 1 */
static const u64 DH_BASE  = 2;

/* A party in the exchange: has a secret and a public value */
typedef struct
{
    u64 secret;
    u64 public_value;
} party;

/* Generate a random secret in [2, p-2] */
static u64 random_secret(void)
{
    /* 61 bits is fine for a demo; real DH uses 2048-bit secrets */
    u64 s = ((u64)rand() << 32) | (u64)rand();
    return 2 + (s % (DH_PRIME - 3));
}

/* Alice/Bob each: pick secret, compute public value = g^secret mod p */
static void party_init(party *p)
{
    p->secret = random_secret();
    p->public_value = powmod(DH_BASE, p->secret, DH_PRIME);
}

/* Alice/Bob each: given the other's public value, derive shared */
static u64 party_derive(const party *me, const party *other)
{
    return powmod(other->public_value, me->secret, DH_PRIME);
}

/* ---------- self-tests ---------- */

static void check(const char *label, int condition)
{
    printf("  %-50s %s\n", label, condition ? "ok" : "FAIL");
}

static void demo_key_exchange(void)
{
    printf("--- 1. Full key exchange ---\n\n");

    printf("Public parameters:\n");
    printf("  p (prime) = %llu\n", DH_PRIME);
    printf("  g (base)  = %llu\n\n", DH_BASE);

    party alice, bob;
    party_init(&alice);
    party_init(&bob);

    printf("Alice:\n");
    printf("  secret a  = %llu\n", alice.secret);
    printf("  public A  = g^a mod p = %llu\n\n", alice.public_value);

    printf("Bob:\n");
    printf("  secret b  = %llu\n", bob.secret);
    printf("  public B  = g^b mod p = %llu\n\n", bob.public_value);

    printf("Exchange: Alice sends A to Bob, Bob sends B to Alice.\n\n");

    u64 alice_shared = party_derive(&alice, &bob);
    u64 bob_shared   = party_derive(&bob, &alice);

    printf("Alice computes s = B^a mod p = %llu\n", alice_shared);
    printf("Bob   computes s = A^b mod p = %llu\n\n", bob_shared);

    check("shared secrets match", alice_shared == bob_shared);
    printf("\n");
}

static void demo_eavesdropper(void)
{
    printf("--- 2. What an eavesdropper sees ---\n\n");

    party alice, bob;
    party_init(&alice);
    party_init(&bob);

    printf("Eve observes the public channel:\n");
    printf("  p = %llu\n", DH_PRIME);
    printf("  g = %llu\n", DH_BASE);
    printf("  A = %llu  (Alice's public value)\n", alice.public_value);
    printf("  B = %llu  (Bob's public value)\n\n", bob.public_value);

    printf("Eve knows A = g^a mod p. To recover a, she must solve\n");
    printf("the DISCRETE LOGARITHM problem for a 61-bit prime.\n");
    printf("With a real 2048-bit prime, this is believed infeasible\n");
    printf("on classical computers.\n\n");

    /* For a 61-bit prime, brute force is actually feasible in
     * a demo (2^60 ~ 10^18; too slow). So we don't try. But we
     * can show the difference in problem statements. */
    printf("Without a, Eve cannot compute B^a mod p.\n");
    printf("Without b, Eve cannot compute A^b mod p.\n");
    printf("Eve has no way to derive the shared secret.\n\n");
}

static void test_commutativity(void)
{
    printf("--- 3. Mathematical property: g^(ab) = g^(ba) ---\n\n");

    /* Small example so the numbers are readable */
    u64 p = 23;   /* small prime */
    u64 g = 5;
    u64 a = 6;
    u64 b = 15;

    u64 A = powmod(g, a, p);
    u64 B = powmod(g, b, p);

    printf("With p = %llu, g = %llu, a = %llu, b = %llu:\n", p, g, a, b);
    printf("  A = g^a mod p = %llu\n", A);
    printf("  B = g^b mod p = %llu\n", B);

    u64 s1 = powmod(B, a, p);   /* (g^b)^a */
    u64 s2 = powmod(A, b, p);   /* (g^a)^b */

    printf("  B^a mod p = %llu\n", s1);
    printf("  A^b mod p = %llu\n", s2);
    printf("  g^(ab) mod p = %llu\n", powmod(g, a * b, p));

    check("B^a = A^b", s1 == s2);
    check("B^a = g^(ab)", s1 == powmod(g, a * b, p));

    printf("\n");
}

static void test_multiple_parties(void)
{
    printf("--- 4. Three-party key exchange (extension) ---\n\n");

    /* DH can be extended to three parties: each contributes a
     * secret, and everyone derives g^(abc) via three rounds.
     *
     * This demo is a simplification, not a full Joux protocol,
     * but it shows the algebraic idea. */

    party a, b, c;
    party_init(&a);
    party_init(&b);
    party_init(&c);

    /* Round 1: each sends their public value to the next */
    u64 A = a.public_value;
    u64 B = b.public_value;
    u64 C = c.public_value;

    /* Round 2: each raises the received value to their secret */
    u64 ab = powmod(B, a.secret, DH_PRIME);
    u64 bc = powmod(C, b.secret, DH_PRIME);
    u64 ca = powmod(A, c.secret, DH_PRIME);

    /* Round 3: continue the chain */
    u64 abc_from_a = powmod(bc, a.secret, DH_PRIME);
    u64 abc_from_b = powmod(ca, b.secret, DH_PRIME);
    u64 abc_from_c = powmod(ab, c.secret, DH_PRIME);

    printf("All three derive g^(abc) mod p = %llu\n", abc_from_a);

    check("all three parties agree (a->b->c)", abc_from_a == abc_from_b);
    check("all three parties agree (c->a->b)", abc_from_a == abc_from_c);

    printf("\n");
}

static void test_mitm(void)
{
    printf("--- 5. Why authenticated DH is necessary (MITM attack) ---\n\n");

    /* The MITM attack: an active attacker (Mallory) sits between
     * Alice and Bob, doing separate DH exchanges with each. She
     * shares a key with Alice and a different key with Bob.
     *
     * We show this because it is THE reason real-world DH uses
     * certificates and signatures, not raw DH. */

    party alice, bob, mallory_a, mallory_b;
    party_init(&alice);
    party_init(&bob);
    party_init(&mallory_a);   /* Mallory posing as Bob to Alice */
    party_init(&mallory_b);   /* Mallory posing as Alice to Bob */

    /* Alice thinks she's talking to Bob, derives a key with Mallory */
    u64 alice_thinks = party_derive(&alice, &mallory_a);
    u64 mallory_with_alice = party_derive(&mallory_a, &alice);

    /* Bob thinks he's talking to Alice, derives a key with Mallory */
    u64 bob_thinks = party_derive(&bob, &mallory_b);
    u64 mallory_with_bob = party_derive(&mallory_b, &bob);

    printf("Alice-Mallory shared key: %llu\n", alice_thinks);
    printf("Mallory-Alice shared key: %llu\n", mallory_with_alice);
    printf("Bob-Mallory shared key:   %llu\n", bob_thinks);
    printf("Mallory-Bob shared key:   %llu\n\n", mallory_with_bob);

    check("Alice and Mallory agree on one key",
          alice_thinks == mallory_with_alice);
    check("Bob and Mallory agree on a different key",
          bob_thinks == mallory_with_bob);
    check("Alice and Bob do NOT agree (Mallory intercepts)",
          alice_thinks != bob_thinks);

    printf("Note: without authentication, Mallory can read and modify\n");
    printf("all traffic. This is why real DH is authenticated\n");
    printf("(e.g., TLS uses signed ephemeral DH, not raw DH).\n\n");
}

int main(void)
{
    srand((unsigned)time(NULL));

    printf("====================================\n");
    printf("         DIFFIE-HELLMAN             \n");
    printf("====================================\n\n");

    demo_key_exchange();
    demo_eavesdropper();
    test_commutativity();
    test_multiple_parties();
    test_mitm();

    printf("All demos complete.\n");
    return 0;
}
