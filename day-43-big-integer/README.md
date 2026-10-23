# Day 43 — big_integer

**Arbitrary-precision non-negative integers** in C—numbers that
exceed the 64-bit limit of `unsigned long long`.

## The Problem

`unsigned long long` tops out at about 1.8×10¹⁹. That's plenty
for counters and timestamps, but not enough for:

- `100!` (about 10¹⁵⁸)
- `2^1000` (about 10³⁰¹)
- RSA keys (2048 bits, about 10⁶¹⁶)
- Exact values in combinatorics (`C(1000, 500)` has 300 digits)

A **big integer** (or **bignum**) stores digits in an array,
with an explicit length. Arithmetic becomes array arithmetic:
add columns with carry, multiply by nested loops.

## Representation

This implementation uses **base-10 digits**, least significant
first:

    12345 in memory:
    digits[0] = 5
    digits[1] = 4
    digits[2] = 3
    digits[3] = 2
    digits[4] = 1
    len = 5

The advantages:

- **Printing is trivial** — just iterate from `len-1` down to 0.
- **Parsing is trivial** — read the string backwards.
- **Reading the code is easy** — the algorithm matches pencil-and-paper
  arithmetic.

The disadvantages:

- **4 bits of storage per digit** instead of 32 or 64.
- **Slower** than word-based bignums by an order of magnitude.
- **More carries per operation** than necessary.

Production libraries (GMP, OpenSSL's `BN`, Python's `int`) use
base 2³² or 2⁶⁴ internally for speed, and only convert to decimal
for I/O. Base 10 is the pedagogical choice.

## What the Program Shows

| Test | Idea | Key Concept |
|------|------|-------------|
| 1. Parse/print | String ↔ number round-trip | Digit reversal |
| 2. Addition | Column-wise carry | `sum / 10`, `sum % 10` |
| 3. Multiplication | Schoolbook algorithm | Nested loops with carries |
| 4. Factorial | Realistic bignum workload | `10!`, `20!`, `30!` |
| 5. Comparison | Numeric ordering | Length, then digits |

## Usage

    ./big_integer

Runs five self-tests and prints `ok` or `FAIL` for each assertion.

## Build

    gcc -Wall -Wextra -std=c99 -o big_integer big_integer.c

## Skills Practiced

- **Dynamic allocation** sized to the actual number of digits
- **Carry propagation** — the heart of every bignum algorithm
- **Digit arithmetic** — working base 10 with `%` and `/`
- **String parsing and formatting** — reversal, leading whitespace, separators
- **Canonical form** — trimming leading zeros so `len` is meaningful
- **Test harness design** — comparing a `big_integer` to an expected string

## Addition in Detail

Adding `999999999999` and `1`:

    a    9 9 9 9 9 9 9 9 9 9 9 9
    b                      1
    ---
    carry 0

    i=0:  9 + 1 + 0 = 10  -> digit 0, carry 1
    i=1:  9 + 0 + 1 = 10  -> digit 0, carry 1
    ...
    i=11: 9 + 0 + 1 = 10  -> digit 0, carry 1

    result:
    digit 0..11 all zero, then a final digit of carry=1
    1 0 0 0 0 0 0 0 0 0 0 0 0

Key invariants:

- **Always reserve one extra digit** for the final carry.
- **`carry` is at most 1** for two base-10 digits plus a carry.
- **`result.len` is at most `max(a.len, b.len) + 1`**.

## Multiplication in Detail

Multiplying `12345` by `6789`:

    result starts as all zeros

    for each digit a[i] of a (least significant first):
        for each digit b[j] of b:
            prod = result[i+j] + a[i] * b[j] + carry
            result[i+j] = prod % 10
            carry = prod / 10
        push remaining carry into result[i+b.len], propagating

    max product digit is 81 + 9 + 8 = 98, so carry stays single-digit

Key invariants:

- **`result.len = a.len + b.len`** is the max size.
- **Inner loop carries propagate naturally** into the next digit.
- **The outer `while (carry > 0)`** is required when a whole row
  ends with a non-zero carry, because the carry could ripple
  through several digits.

## Why Base 10, Not Base 2^32?

| | Base 10 | Base 2^32 |
|---|---|---|
| Storage per digit | 1 byte | 4 bytes |
| Max digit | 9 | 4 294 967 295 |
| Multiplication | `%10` and `/10` | Harder — must use 64-bit intermediates |
| Printing | Direct | Needs repeated division |
| Pedagogical clarity | High | Lower |
| Real-world use | No | Yes |

The point of a teaching bignum is to **see the algorithm**, not
to be fast. Base 10 lets the code be read against pencil-and-paper.
Switching to base 2^32 later is a mechanical transformation, not
a new idea.

## Estimated Performance

For `n!`:

| n | Digits in result | Time (base 10, schoolbook) |
|---|------------------|---------------------------|
| 10 | 7 | instant |
| 100 | 158 | instant |
| 1000 | 2568 | <1 second |
| 10000 | 35660 | tens of seconds |

Real libraries use **Karatsuba** (O(n^1.58)) or **FFT-based**
multiplication (O(n log n)) for very large numbers. Schoolbook
is O(n²) and fine up to a few thousand digits.

## The Big Picture

Big integers appear wherever you need **exact** results beyond
machine limits:

- **Cryptography**: RSA, Diffie-Hellman, elliptic curves —
  every number is 256–4096 bits.
- **Combinatorics**: binomial coefficients, partitions,
  Catalan numbers — enormous, exact.
- **Scientific computing**: exact rational arithmetic,
  arbitrary-precision floats.
- **Language runtimes**: Python, Ruby, Java's `BigInteger`,
  Go's `math/big` all use bignum libraries.

Getting a clean `bi_add` and `bi_mul` is the foundation. The
next steps are **subtraction with borrow**, **division**, and
**modular arithmetic** (which is what makes RSA possible).

## Notes

- **Non-negative only.** Supporting negatives requires either a
  sign flag plus magnitude comparison, or two's complement on
  the digit array — both are significant work.
- **No division.** Long division on digit arrays is a
  nontrivial algorithm; it's a one-day project by itself.
- **`bi_from_string` is tolerant** — it skips leading whitespace,
  treats `_` as a digit separator (Python-style `1_000_000`),
  and ignores non-digit trailing characters.
- **`bi_destroy` is explicit.** Every allocated `big_integer`
  must be freed. In a larger program you would use RAII-style
  cleanup or a garbage collector.
- **Return by value, with a pointer inside.** `big_integer` is
  small (three fields, ~24 bytes), so returning it by value is
  cheap; the underlying `digits` buffer is not copied.

## What You Can Extend

- **Subtraction** with a "larger minus smaller" invariant and a
  sign flag for the rest
- **Long division** — the hard but satisfying step
- **`bi_mod(a, b)`** — return `a % b`; combined with multiplication
  it unlocks modular exponentiation
- **`bi_pow(a, n)`** — repeated squaring for fast exponentiation
- **`bi_pow_mod(a, n, m)`** — the core of RSA
- **Base 2^32 digits** — a real speedup, with `uint64_t` intermediates
- **Karatsuba multiplication** — subquadratic for large numbers
- **Signed arithmetic** — either sign-magnitude or two's complement

## Why This Matters for Math-Flavored Programming

- **Arithmetic overflow is a fundamental bug class.** Languages
  with native bignums (Python, Ruby, Go's `math/big`) avoid it;
  languages without (C, C++) require careful library use.
  Understanding bignums demystifies the difference.
- **Carry propagation** is not just a bignum technique — it is
  the shape of every per-digit arithmetic operation in CPUs,
  DSPs, and cryptographic hardware.
- **Base conversion** (decimal ↔ binary) is a task you will
  meet again in every serialization format, network protocol,
  and file format.
- **Exact arithmetic** is required for anything that must not
  round: money, cryptography, combinatorics.
