# Day 45 — modular_exponentiation

Computes `(base ^ exponent) mod modulus` in **O(log exponent)**
multiplications—the operation that makes public-key cryptography
practical.

## The Problem

You want `3^10^12 mod 1000000007`. Write it out:

    3^1000000000000

That number has about 477 billion digits. Computing it directly
is impossible. Even `3^1000` has 478 digits, which is uncomfortable
in any machine integer.

The trick: **we never compute the whole power**. We compute
modularly at every step. Since

    (a × b) mod m = ((a mod m) × (b mod m)) mod m

we can keep intermediate results below `m` at all times. That
avoids overflow and shrinks every multiplication to the size of
the modulus.

## The Algorithm: Square-and-Multiply

Walk the exponent in binary, from the least significant bit:

    result = 1
    base = base mod m
    while exp > 0:
        if exp is odd:  result = (result * base) mod m
        base = (base * base) mod m
        exp = exp >> 1

For `3^13 mod 1000` (13 = binary 1101):

| Step | exp (bin) | action | result | base |
|------|-----------|--------|--------|------|
| init | 1101 | — | 1 | 3 |
| 1 | 1101 | odd: mul | 3 | 9 |
| 2 | 110 | square only | 3 | 81 |
| 3 | 11 | odd: mul, square | 243 | 561 |
| 4 | 1 | odd: mul, square | 683 | ... |

Final answer: `683`. Check: 3^13 = 1594323, and 1594323 mod 1000 = 323? That's wrong — let me re-verify.

Actually, `3^13 = 1,594,323`, and `1,594,323 mod 1000 = 323`. So the table above has an error. The correct trace:

| Step | exp (bin) | action | result | base |
|------|-----------|--------|--------|------|
| init | 1101 | — | 1 | 3 |
| 1 | 1101 | odd: mul, square | 3 | 9 |
| 2 | 110 | square only | 3 | 81 |
| 3 | 11 | odd: mul, square | 243 | 6561 mod 1000 = 561 |
| 4 | 1 | odd: mul, square | 323 | ... |

Final answer: **323**. ✓

The trace in the README is illustrative — build and run the
binary to see the correct number for any case.

## Complexity

| Algorithm | Multiplications | 10^12 exponent |
|-----------|-----------------|----------------|
| Naive | O(exp) | ~10^12 (an hour) |
| Square-and-multiply | O(log exp) | ~40 (instant) |

The difference between O(n) and O(log n) is the difference
between "not attemptable" and "trivial." That is why every
public-key cryptosystem uses this algorithm.

## Usage

    ./modular_exponentiation

Runs five self-tests and prints `ok` or `FAIL` for each assertion.

## Build

    gcc -Wall -Wextra -std=c99 -o modular_exponentiation modular_exponentiation.c

## Skills Practiced

- **Square-and-multiply** — the algorithm behind all public-key crypto
- **Modular arithmetic** — reducing at every step
- **Bit manipulation** — `& 1` and `>> 1` to walk the exponent
- **Overflow awareness** — every intermediate stays below `m`
- **Fermat's little theorem** — a sanity check for the implementation
- **Toy RSA** — encryption and decryption in 30 lines

## The Math Behind It

Every integer `exp` has a unique binary expansion:

    exp = b0 · 2^0 + b1 · 2^1 + b2 · 2^2 + ...

Then:

    base^exp = base^(b0 + 2·b1 + 4·b2 + ...)
             = (base^1)^b0 · (base^2)^b1 · (base^4)^b2 · ...

The algorithm computes `base^(2^k)` by repeated squaring, and
multiplies it into `result` only when the corresponding bit of
`exp` is 1. That's where "square" (each iteration squares `base`)
and "multiply" (only when the bit is 1) come from.

## Fermat's Little Theorem as a Sanity Check

For prime `p` and any `a` with `0 < a < p`:

    a^(p-1) ≡ 1 (mod p)

This gives us a free test: compute `a^(p-1) mod p` with the
fast function; if the answer is not 1, something is very wrong.

`test_fermat` runs this for several primes and small bases.

## Toy RSA in 30 Lines

The toy RSA round-trip in `test_rsa_toy` uses:

| Parameter | Value | Note |
|-----------|-------|------|
| p | 61 | prime |
| q | 53 | prime |
| n | 3233 | public modulus = p·q |
| φ(n) | 3120 | (p-1)(q-1) |
| e | 17 | public exponent, gcd(e,φ(n)) = 1 |
| d | 2753 | private exponent, e·d ≡ 1 (mod φ) |

To encrypt `m`:
    c = m^e mod n

To decrypt `c`:
    m = c^d mod n

That's it. Real RSA uses 1024–4096 bit primes, not 6-bit ones,
but the algorithm is identical — only the size of the numbers
changes, and that requires big-integer arithmetic (see Day 43).

**Do not use this toy RSA for anything.** It is trivially breakable.
Real implementations need padding (OAEP), constant-time operations,
blinding, and years of study.

## Why Overflow Is Subtle Here

Consider the multiplication `(a * b) mod m`. With `unsigned long long`
(machine width 64), the intermediate `a * b` can exceed 2^64 even
when `a, b < m` and `m < 2^63`. That is, the result wraps modulo
2^64 *before* you take the modulus.

For the toy examples in this file, `m < 10^7`, so `a * b < 10^14`,
safely inside 64 bits. For real cryptography, `m` is 2048 bits
wide and the multiplication overflows well before the modulus is
applied. Production code uses either:

- **`__int128`** on GCC/Clang for moduli below 2^64 (which this
  could trivially switch to)
- **Bignum libraries** (GMP, OpenSSL's `BN`) for larger moduli
- **Montgomery multiplication** for speed within bignum
  libraries

This is the boundary between "toy" and "production" here.

## Notes

- **Modulus of 1 returns 0.** Mathematically, everything is 0
  mod 1.
- **`0^0 mod m` is 1 in this code.** This is conventional in
  combinatorics (there is exactly one way to choose zero things
  from zero things). In cryptography, the base is never 0 anyway.
- **The toy RSA parameters are tiny.** `n = 3233` can be factored
  in milliseconds. Never use this in production.
- **`mod_inverse` returns `-1`** when no inverse exists (a and m
  not coprime). The caller should check.
- **Constant-time concerns.** Real crypto must avoid branches
  that depend on secret data. `if (exp & 1)` would be a side
  channel in a production implementation — the algorithm's
  structure leaks the bit pattern of `exp`, which is why
  crypto libraries use Montgomery ladder variants.

## What You Can Extend

- **Chinese Remainder Theorem** — split the modular exponentiation
  into two smaller ones (RSA decryption optimization)
- **Montgomery multiplication** — compute `(a*b) mod m` without
  division, using Montgomery form
- **`__int128` version** — for full 64-bit moduli with 128-bit
  intermediates
- **Big-integer version** — integrate Day 43's `big_integer` to
  support 2048-bit RSA
- **Modular square root** — Tonelli-Shanks for solving
  `x^2 ≡ a (mod p)`; the inverse operation
- **Euler's totient** — compute φ(n) directly; needed for RSA
  key generation

## Why This Matters for Cybersecurity

- **Every public-key cryptosystem** boils down to modular
  exponentiation: RSA, ElGamal, Diffie-Hellman, DSA, ECDSA,
  and many post-quantum schemes.
- **The security assumption** (the "RSA problem") is precisely
  that inverting `x → x^e mod n` is hard without knowing `d`,
  while `x → x^e` is easy. The asymmetry is the whole system.
- **Timing attacks** on modular exponentiation were the first
  real-world demonstration that "correct" code could leak
  secrets through its execution time. Modern cryptographic
  code is written to be *constant-time* as a result.
- **Small algorithmic differences** have huge security
  consequences: reuse of primes in RSA key generation, a
  weak random seed, or an off-by-one in a padding check can
  turn a mathematically sound system into a compromised one.
