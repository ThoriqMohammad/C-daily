# Day 48 — prime_factorization

Factors a positive integer into its prime components using two
algorithms: **trial division** (simple, slow) and **Pollard's
rho** (clever, fast).

## Why Factorization Is Interesting

Every integer > 1 has a unique prime factorization:

    360 = 2^3 * 3^2 * 5
    12345 = 3 * 5 * 823
    1000000007 * 1000000009 = 1000000016000000063

Finding that factorization is **easy for small numbers** and
**genuinely hard for large ones**. That asymmetry — multiply
two 300-digit primes in microseconds, but factoring the product
back is beyond any classical computer — is the security foundation
of RSA, Diffie-Hellman, and much of modern cryptography.

This program shows two algorithms that sit at opposite ends of
the difficulty spectrum for different sizes.

## Trial Division

The simplest possible approach:

    for p in 2, 3, 5, 7, 9, 11, ...:
        while n % p == 0:
            output p
            n /= p

Each factor found shrinks `n`. We only need to try primes up to
`sqrt(n)` at any point — any remaining composite has to have a
factor less than or equal to its square root, so once `p * p > n`,
`n` must be prime.

**Complexity:** O(sqrt n) divisions in the worst case (when `n`
is prime). For `n = 10^12`, that's ~1,000,000 divisions — about
10 milliseconds. For `n = 10^18`, it's ~1,000,000,000 divisions —
about 10 seconds. For `n = 10^30`, it's untenable.

Trial division is used as a **preprocessing step** in real
algorithms: strip off small factors first, then switch to a
smarter algorithm for the remaining large composite.

## Pollard's Rho

A **randomized** algorithm that finds a non-trivial factor in
about O(n^(1/4)) steps. For `n = 10^18`, that's about 30,000
steps — instant.

The idea, briefly:

1. Pick a simple polynomial `f(x) = x^2 + c mod n`.
2. Generate a sequence `x_1, x_2, ...` with `x_{i+1} = f(x_i)`.
3. The sequence eventually repeats (it's finite and deterministic).
4. If `p` is a factor of `n`, the sequence mod `p` repeats *earlier*
   than the sequence mod `n` — because there are fewer residues.
5. Floyd's cycle-finding algorithm (two pointers, one moving at
   double speed) finds the repeat using `gcd(x - y, n) != 1`.

When `gcd(x - y, n)` gives a number between `1` and `n`, that's a
factor. If it gives `1`, keep going. If it gives `n`, retry with
a different `c`.

The `pollard_rho` in this file tries `c = 1, 2, 3, ...` until it
succeeds. In practice, a random `c` works on the first try about
half the time; trying a few small values is enough.

## The Full Algorithm

For factoring a 64-bit number:

1. **Strip small primes** with trial division (2, 3, 5, ..., 349).
   This catches the vast majority of factors.
2. **Check if what's left is prime** with Miller-Rabin.
3. **If not**, run Pollard's rho to find a factor `d`.
4. **Recurse** on `d` and `n / d`.
5. **Stop** when each subproblem is prime.

The recursion depth is the number of prime factors, at most ~64
(since each factor is >= 2).

## Miller-Rabin Primality Test

Before we can factor `n`, we need to know if `n` is *already*
prime — trial division's loop condition `p * p <= n` becomes
wrong once `n` is prime but large.

Miller-Rabin is a **probabilistic** primality test: given a
composite `n`, each round detects it with probability >= 3/4.
After `k` rounds, the chance of missing it is `(1/4)^k`.

**For 64-bit integers**, using the specific bases
`{2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37}` makes the test
**fully deterministic**. There are no 64-bit composites that
pass all 12 rounds. So for this program, `is_prime` is exact.

For larger inputs (128-bit, 256-bit, RSA-sized numbers), no
fixed set of bases is known to be sufficient; instead you use
random bases and accept a tiny error probability.

## Usage

    ./prime_factorization 12345
    ./prime_factorization 1000000007
    ./prime_factorization 1000000016000000063
    ./prime_factorization 18446744073709551557   (largest 64-bit prime)
    ./prime_factorization test

The first form factors the number and prints timing for both
algorithms; the `test` form runs self-tests.

## Example

    $ ./prime_factorization 360
    Factoring 360

    Trial division: 5 factors in 0.001 ms
      2 * 2 * 2 * 3 * 3

    Pollard's rho: 5 factors in 0.002 ms
      360 = 2^3 * 3^2

    Verified: yes

    $ ./prime_factorization 1000000016000000063
    Factoring 1000000016000000063

    Trial division: skipped (n >= 10^7, would take too long)

    Pollard's rho: 2 factors in 0.015 ms
      1000000016000000063 = 1000000007 * 1000000009

    Verified: yes

The second case is the punchline: trial division would take
about **10 hours** on `10^18`; Pollard's rho finishes in
**15 microseconds**. That's the difference between
O(n^(1/2)) and O(n^(1/4)).

## Build

    gcc -Wall -Wextra -std=c99 -o prime_factorization prime_factorization.c

## Skills Practiced

- **Trial division** and its complexity analysis
- **Miller-Rabin** primality testing (deterministic for 64-bit)
- **Pollard's rho** factorization with Floyd's cycle detection
- **Modular arithmetic** with `__int128` to prevent overflow
- **GCD** with the Euclidean algorithm
- **Recursion** for divide-and-conquer factorization
- **Formatting** factored output with exponent notation

## The `__int128` Trick

Multiplying two 64-bit numbers can produce a 128-bit product
that overflows `unsigned long long`. GCC and Clang provide
`unsigned __int128` for exactly this purpose:

    u64 mulmod(u64 a, u64 b, u64 m) {
        return (u64)((u128)a * b % m);
    }

Without this, Miller-Rabin and Pollard's rho produce wrong
results for moduli near `2^64`. It's the standard idiom in
production bignum code on x86-64.

If you're on MSVC (no `__int128`), you'd fall back to Russian
peasant multiplication, which is slower but portable.

## Notes

- **The program assumes `n >= 2`.** Factoring 1 is the empty
  product, which we print as `1`.
- **Pollard's rho can fail** on very rare inputs, returning `n`
  itself. The `factor()` function treats that as a prime (which
  it isn't), so it would miss the factor. For 64-bit inputs
  with a proper `c` sequence, this never happens in practice.
- **The deterministic `c` sequence** in `pollard_rho` tries
  `c = 1, 2, ..., 99`. Each try has about a 50% chance of
  succeeding. The chance that all 99 fail is astronomically
  small.
- **`factor()` is recursive**, but the depth is bounded by the
  number of prime factors (≤ 64). No stack overflow risk.
- **The verifier** (`verify_factorization`) recomputes the product
  of factors and checks it equals `n`. This catches a whole
  class of bugs where factors are found but incorrect.
- **Sorting** the factors uses insertion sort — the array has at
  most 64 elements, so O(n²) is fine.

## What You Can Extend

- **128-bit factorization** — use a bignum library for inputs
  above 2^64
- **Quadratic sieve** — a faster algorithm than Pollard's rho
  for 100–150 digit numbers
- **General Number Field Sieve** — the algorithm used to break
  RSA-768 in 2009
- **`Fermat's factorization`** — an alternative for numbers whose
  factors are close together
- **A full factorization CLI** with multiple numbers per invocation
- **Output in JSON** for programmatic use
- **Extended precision** using `big_integer` from Day 43 for
  inputs beyond 2^64

## Why This Matters for Cybersecurity

- **RSA security = factorization hardness.** Given `n = p * q`,
  an attacker who can factor `n` recovers `p` and `q`, then
  derives the private key `d`. Every RSA key relies on the
  assumption that factoring is hard.
- **Pollard's rho is a real attack.** If your RSA key uses
  primes that are too close together, or too small, Pollard's
  rho or Fermat's factorization breaks it instantly. Modern
  RSA key generation uses 1024-bit primes specifically to make
  these attacks infeasible.
- **Miller-Rabin is used everywhere in cryptography.** Every
  key generation routine tests candidate primes with it. The
  `OpenSSL` source is full of Miller-Rabin calls.
- **Factorization as a benchmark.** The RSA Factoring Challenge
  tracked the state of the art from 1991 to 2007; RSA-768
  (232 decimal digits) was the last solved. RSA-2048 has never
  been factored.
- **The algorithms in this file are the same ones used by GMP,
  PARI/GP, SymPy, and every CAS (Computer Algebra System).**
  The implementations here are simplified but the algorithms
  are identical.

## Why This Matters for Math-Flavored Programming

- **Prime factorization is the "hello world" of number theory.**
  It connects to gcd, modular arithmetic, cryptography,
  combinatorics, and primality testing.
- **Randomized algorithms** like Pollard's rho are a different
  mental model from deterministic ones. They are fast on
  *average* and correct *with high probability*, but individual
  runs vary.
- **`__int128`** is a glimpse of what big-integer arithmetic
  feels like. Once you need it, you know you've stepped into
  a new regime of programming.
- **The "strip small factors, then recurse"** pattern is the
  same shape used in trial division, sieve-based algorithms,
  and many optimization problems.
