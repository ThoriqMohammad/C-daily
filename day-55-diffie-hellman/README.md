# Day 55 — diffie_hellman

**Diffie-Hellman key exchange**: two parties agree on a shared
secret over a public channel, without ever transmitting the
secret.

Published in 1976 by Whitfield Diffie and Martin Hellman, DH
was the first working public-key protocol. It predates RSA by
a year, and it solved the problem that had blocked cryptography
since antiquity: **how do two people who have never met agree
on a secret, while an eavesdropper hears everything?**

The answer — modular exponentiation and the discrete logarithm
problem — is the foundation of every secure connection on the
internet today.

## The Problem

Alice wants to send Bob a secret message. She needs a key. But
she's never met Bob, and every message between them is
intercepted by Eve.

Traditional solution: meet in person, agree on a key, and
carry it in your head. Doesn't scale to the internet.

Diffie-Hellman solution: **agree on a key in public**.

## The Protocol

Public parameters (anyone can see):
- `p` — a large prime
- `g` — a generator mod `p`

Alice:
1. Picks a random secret `a`.
2. Computes `A = g^a mod p`.
3. Sends `A` to Bob over the public channel.

Bob:
1. Picks a random secret `b`.
2. Computes `B = g^b mod p`.
3. Sends `B` to Alice over the public channel.

Then:

**Alice** computes `s = B^a mod p`.
**Bob** computes `s = A^b mod p`.

These are the same number, because:

    (g^b)^a = g^(ab) = (g^a)^b  (mod p)

Both sides have `s`. Eve has `p`, `g`, `A`, and `B`, but not
`a` or `b`.

## Why Eve Can't Get `s`

Eve sees `A = g^a mod p`. To compute `s = B^a mod p`, she'd
need `a`. That means solving:

    a = log_g(A)  mod p

This is the **discrete logarithm problem**. For small `p` it's
easy (brute force). For 2048-bit `p`, no known classical
algorithm can solve it in reasonable time. This is exactly the
same security assumption as RSA's factoring hardness, but for
a different problem.

| Problem | Basis of | Best known attack |
|---------|----------|-------------------|
| Factoring | RSA | Number Field Sieve |
| Discrete log | Diffie-Hellman | Index Calculus, NFS |

Both problems are believed to be hard for sufficiently large
parameters, and both would be broken by a large quantum
computer (Shor's algorithm).

## The Math, Step by Step

Small example with `p = 23`, `g = 5`, `a = 6`, `b = 15`:

| Step | Alice | Bob |
|------|-------|-----|
| 1. Secret | `a = 6` | `b = 15` |
| 2. Public value | `A = 5^6 mod 23 = 8` | `B = 5^15 mod 23 = 19` |
| 3. Exchange | sends `8` | sends `19` |
| 4. Compute shared | `B^a = 19^6 mod 23 = 2` | `A^b = 8^15 mod 23 = 2` |
| 5. Shared secret | **`s = 2`** | **`s = 2`** |

Both arrive at 2, using different computations. Eve sees
`(23, 5, 8, 19)` and cannot recover 2.

`test_commutativity` runs through this exact example.

## Usage

    ./diffie_hellman

Runs five demos:

1. **Full key exchange** — Alice and Bob agree on a secret
2. **What Eve sees** — the public values, and what's missing
3. **Commutativity** — why `g^(ab) = g^(ba)` works
4. **Three-party extension** — how DH can scale beyond two
5. **MITM attack** — why raw DH is insecure

## Build

    gcc -Wall -Wextra -std=c99 -o diffie_hellman diffie_hellman.c

## Skills Practiced

- **Diffie-Hellman protocol** — the four messages
- **Modular exponentiation** (reused from Day 45)
- **The discrete logarithm problem** — DH's hard problem
- **Safe primes** — why `p` matters
- **MITM attacks** — why DH needs authentication
- **Three-party key exchange** — a non-obvious extension

## The Small-Subgroup Attack

The prime `p` in this demo is `2^61 - 1`, a Mersenne prime.
It's a **safe prime** because `(p-1)/2 = 2^60 - 1` is also prime.
Safe primes are important because they prevent a **small-subgroup
attack**:

If `p-1` has small prime factors, an attacker can force the
shared secret into a small subgroup by choosing their public
value from that subgroup. Then the attacker can brute-force
the small subgroup and learn the secret modulo that factor.

For a 61-bit safe prime, this is impractical but not impossible.
For a 2048-bit safe prime (which is what real DH uses), it's
infeasible.

The common "RFC 3526" primes are all safe primes for this
reason.

## The MITM Attack

**This is the big one.** Raw Diffie-Hellman is vulnerable to a
**man-in-the-middle attack**:

1. Alice sends `A = g^a mod p` to Bob.
2. Mallory **intercepts** `A`. She sends her own `M_a = g^m1 mod p`
   to Bob (pretending to be Alice).
3. Bob sends `B = g^b mod p` back. Mallory intercepts it and
   sends `M_b = g^m2 mod p` to Alice.
4. Alice now thinks she shares a key with Bob, but actually
   shares one with Mallory. Bob thinks the same.
5. Mallory decrypts everything from Alice, re-encrypts it
   with the other key, sends to Bob. Both see normal traffic.

`test_mitm` demonstrates this directly: Alice and Mallory share
one key, Bob and Mallory share a different key, and Alice and
Bob don't share anything.

**This is why real DH is never used raw.** Every production
use of DH — in TLS, SSH, Signal, etc. — signs the DH exchange
with a certificate or a pre-shared key. The signature proves
that the `A` Alice received really came from Bob, not Mallory.

Terms you'll see:
- **Anonymous DH (ADH)** — raw DH, no authentication, **insecure**
- **Static DH** — the same secret is reused each session, less secure
- **Ephemeral DH (DHE)** — a fresh key per session, forward secrecy
- **Authenticated DH** — signed with a certificate, secure

The `DHE_RSA` cipher suite in TLS 1.2 is ephemeral DH
authenticated by an RSA certificate. The `ecdhe_ecdsa` suite
in TLS 1.3 is the same idea with elliptic curves.

## Why This Matters for Cybersecurity

- **Every TLS connection uses DH or ECDH.** That's every HTTPS
  session you've ever made.
- **SSH uses DH** for key exchange, both for the client-to-server
  and the internal re-keying.
- **Signal, WhatsApp, iMessage** use a DH variant (X3DH) to
  establish end-to-end encrypted sessions.
- **Perfect forward secrecy** comes from using ephemeral DH
  keys per session. If a server's long-term key is compromised
  later, past sessions remain secure — but only if ephemeral
  DH was used.
- **Post-quantum cryptography** is the race to replace DH
  before quantum computers can break it. ML-KEM (formerly
  Kyber) is the front-runner for key exchange.

## Notes

- **The prime in this demo is 61 bits.** Real DH uses 2048+
  bits. This is a toy for teaching, not for use.
- **`g = 2` is a generator for `2^61 - 1`.** Real DH uses larger
  generators or generators with specific properties. For a
  safe prime, `g = 2` is fine.
- **The secrets are `u64`.** In real DH they're 2048-bit
  integers, requiring bignum arithmetic (Day 43).
- **`rand()` is not cryptographically secure.** Real DH uses
  `getrandom()` or `/dev/urandom` for secrets. Using `rand()`
  would make the keys predictable.
- **The demo is not constant-time.** `powmod` has branches
  based on the secret exponent, which leaks timing information.
  Real DH uses **Montgomery ladder** or **fixed-exponent**
  implementations to avoid this.
- **`test_multiple_parties`** is a simplification, not the full
  **Joux protocol** (which uses pairing-based crypto). It shows
  the algebraic idea: `g^(abc)` derived by three parties in
  three rounds.

## What You Can Extend

- **Bignum DH** — use Day 43's `big_integer` for 2048-bit primes
- **ECDH** — Diffie-Hellman on elliptic curves, much shorter keys
- **X25519** — the specific ECDH curve used in TLS 1.3 and
  Signal
- **Authenticated DH** — sign the public values with a
  certificate to prevent MITM
- **Diffie-Hellman over a real socket** — two processes on
  localhost exchanging actual bytes
- **Timing attack demo** — measure `powmod`'s execution time
  with different secrets, showing why constant-time matters

## Why This Matters as a Portfolio Piece

- **Diffie-Hellman and RSA are the two great public-key
  algorithms.** Having both in the repo (Day 50 and Day 55)
  shows you understand the two complementary approaches:
  encryption vs key exchange.
- **The MITM attack demo** shows you understand not just
  the algorithm, but what it's missing. Being able to articulate
  "this is insecure, and here's why" is what distinguishes
  security engineers from programmers.
- **The safe-prime / small-subgroup discussion** shows you know
  the difference between "correct math" and "secure
  implementation." That distinction is the core skill of
  applied cryptography.
- **The three-party extension** shows the algebraic depth.
  DH isn't just a two-party trick; it's a family of protocols.

## The Crypto Portfolio Arc

Day 50 (RSA) and Day 55 (Diffie-Hellman) complete the modern
public-key story:

| Day | Project | Role |
|-----|---------|------|
| 07 | Caesar cipher | Historical starting point |
| 11 | XOR cipher | Stream cipher basics |
| 40 | Vigenère cipher | Polyalphabetic, pre-modern |
| 20 | Hash function | One-way functions |
| 25 | Base64 encoder | Encoding (not encryption) |
| 32 | Password manager | Applied hashing + storage |
| 45 | Modular exponentiation | The primitive |
| 48 | Prime factorization | RSA's hard problem |
| 50 | RSA | Public-key **encryption** |
| 55 | Diffie-Hellman | Public-key **key exchange** |

That's a complete curriculum in classical and modern
cryptography, from Caesar to TLS. Anyone reviewing the repo
can see both breadth and depth.
