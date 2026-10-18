# Day 38 — polynomial

A small polynomial toolkit for **integer coefficients**—evaluation,
addition, subtraction, and multiplication, with a clear cap on degree.

## What Is a Polynomial?

A polynomial is a sum of terms in a single variable:

    p(x) = a0 + a1*x + a2*x^2 + ... + an*x^n

The **degree** is the highest power with a non-zero coefficient.
A constant like `7` has degree 0; `3x + 2` has degree 1; `x^2 - 5x + 6`
has degree 2.

## How It Is Represented

`polynomial` stores two fields:

    typedef struct {
        long long coeff[17];  /* coeff[i] multiplies x^i */
        int       degree;     /* highest non-zero power  */
    } polynomial;

The array is **dense**: index `i` always holds the coefficient for
`x^i`, even if it is zero. That makes evaluation and arithmetic
straightforward at the cost of a fixed maximum degree
(`POLY_MAX_DEGREE = 16`).

An alternative representation uses **(coefficient, exponent)** pairs
to store only non-zero terms. Better for sparse polynomials; more
code for the dense operations we need here.

## What the Program Shows

| Test | Idea | Key Concept |
|------|------|-------------|
| 1. Evaluation | Compute `p(x)` for a given `x` | Horner's method |
| 2. Add / Sub | Combine like terms | Padding shorter polynomial |
| 3. Multiply | Convolve coefficients | Nested loops |
| 4. Trim | Strip leading zeros | Canonical form |

## Usage

    ./polynomial

Runs four tests and prints `ok` or `FAIL` for each assertion.

## Build

    gcc -Wall -Wextra -std=c99 -o polynomial polynomial.c

## Skills Practiced

- **Struct-based types** with a fixed-size array plus a metadata field
- **Horner's method** — O(n) evaluation instead of O(n²) via naive powers
- **Convolution** — the same nested-loop shape as polynomial multiplication
- **Canonical form** — trimming so `degree` is always meaningful
- **Cleanup patterns** — functions return a new polynomial by value

## Horner's Method

The naive way to evaluate a polynomial at `x` is:

    result = 0
    for i in 0..degree:
        result += coeff[i] * pow(x, i)

That requires recomputing `x^i` at every step, which is `O(n²)`
unless `pow` caches. The faster way nests the operations:

    p(x) = a0 + x*(a1 + x*(a2 + x*(... + x*an)))

So this loop is enough:

    result = coeff[degree]
    for i = degree-1 down to 0:
        result = result * x + coeff[i]

That is **n multiplications and n additions**, done in one pass.
This is exactly what the test for `p(10) = 56` exercises.

## Multiply = Convolve

When you multiply two polynomials, the coefficient of `x^k` in
the product is the sum of all `a[i] * b[k - i]` pairs:

    (a * b)[k] = sum over i of a[i] * b[k - i]

That is the same formula as **discrete convolution** — which is
why the code is just two nested loops:

    for i in a:
        for j in b:
            result[i + j] += a[i] * b[j]

The degree of the product is `a.degree + b.degree`. The code
checks for overflow of the fixed array and refuses rather than
producing garbage.

## Canonical Form

Because arithmetic can produce trailing zeros (`2x + 5` from
`0x^3 + 0x^2 + 2x + 5`), every constructor and operation ends
with a trim:

    while (degree > 0 && coeff[degree] == 0)
        degree--;

Without this, `degree` would be unreliable and comparisons
would silently diverge.

## Notes

- **Integer coefficients only.** Real coefficients need `double`
  and a tolerance for comparison; that is a different design.
- **Fixed max degree** of 16. Overflow of the array is caught
  at multiply time and reported, not silently truncated.
- **No division** or factoring. Those are separate, harder
  problems — polynomial division with integer coefficients
  requires care around exact divisibility.
- **Return by value.** `polynomial` is small (about 150 bytes)
  so the compiler copies it efficiently; no need for pointers
  to allocate on the heap.
- `poly_mul` **does not check** for `long long` overflow. For
  small coefficients that is fine; for crypto-scale numbers
  you would need big integers.

## What You Can Extend

- **Differentiation** — `derivative(p)` maps `coeff[i]` to `i * coeff[i-1]`
- **Integration** — needs rational coefficients or a scale factor
- **Polynomial division** — `p / q` with quotient and remainder
- **Synthetic substitution** — a specialized form of Horner's for testing roots
- **Modular coefficients** — wrap every operation in `% m` for
  a Crypto-themed follow-up (polynomials over GF(p))

## Why This Matters for Math-Flavored Programming

- **Horner's method** is the standard algorithm behind every
  scientific calculator and many DSP routines.
- **Convolution** is the same operation used in signal processing,
  image filtering, and (via FFT) fast multiplication of huge numbers.
- **Polynomials over finite fields** are the backbone of modern
  error-correcting codes (Reed-Solomon), cryptography (secret
  sharing), and hashing (CRC is literally polynomial division mod 2).
- The **trim** step is a microcosm of a much larger lesson: any
  time you store a size field next to a data array, you must
  keep the two in sync on every mutation.
