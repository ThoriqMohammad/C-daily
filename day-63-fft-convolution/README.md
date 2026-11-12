# Day 63 — fft_convolution

Multiply two polynomials in **O(n log n)** using the FFT,
instead of the O(n²) schoolbook algorithm.

## The Problem

Multiplying two polynomials of degree n and m naively takes
`n × m` coefficient multiplications:

    for i in 0..n:
        for j in 0..m:
            C[i+j] += A[i] * B[j]

That's fine for small polynomials. For n = 100,000, it's 10^10
operations — a few seconds. For n = 1,000,000, it's 10^12 — a
few minutes. That's the wall that makes polynomial arithmetic
infeasible at scale.

The same wall appears in:

- **Big integer multiplication** — treat the digits of two
  huge integers as polynomial coefficients. Python, GMP, and
  every bignum library uses FFT for this.
- **Signal convolution** — applying a filter to a signal is
  polynomial multiplication in disguise.
- **Image convolution** — every blur, sharpen, or edge-detect
  is a polynomial multiplication.
- **Cryptography** — NTT (Number Theoretic Transform, an FFT
  variant) is used in some post-quantum schemes.

The FFT makes all of these **O(n log n)**.

## The Trick

The **convolution theorem** says:

> Convolution in the time domain equals pointwise
> multiplication in the frequency domain.

In symbols:

    FFT(A * B) = FFT(A) ⊙ FFT(B)

where `⊙` is pointwise multiplication. So to multiply A and B:

1. Compute `FA = FFT(A)`.
2. Compute `FB = FFT(B)`.
3. Compute `FC = FA ⊙ FB` (element-wise).
4. Compute `C = IFFT(FC)`.

The result `C` is the polynomial product. FFT and IFFT are each
O(n log n), and the pointwise multiplication is O(n). Total:
O(n log n).

That's the whole algorithm. It's short enough to fit on a napkin:

    FA = FFT(A)
    FB = FFT(B)
    C  = IFFT(FA ⊙ FB)

## The One Critical Detail: Zero-Padding

The FFT computes a **cyclic** convolution of length N. If you
just FFT the two coefficient arrays at their natural lengths,
the FFT treats them as wrapping around at index N — and the
high-degree terms of the product wrap around to the low end,
corrupting the result.

The fix: **zero-pad both arrays to a length N ≥ n + m - 1**,
the size of the full product. Then there's no wrapping — the
product fits entirely within [0, N).

And N must be a **power of two** for the radix-2 FFT. So:

    N = next_pow2(a_len + b_len - 1)

The padding costs memory (up to 2× for the array size) but the
computation stays O(n log n).

Forgetting to zero-pad is the #1 bug in FFT-based multiplication.
The output looks plausible but is wrong — it has the right
shape, but the coefficients are cyclic-wrapped. Always check
this.

## Comparison with Day 38's Polynomial

Day 38 (`polynomial`) implemented the schoolbook algorithm for
small polynomials — clean, simple, and correct, but O(n²).

Day 63 (`fft_convolution`) keeps the same input format (arrays
of coefficients) but uses FFT to compute the product much
faster. The two are the two ways to multiply polynomials:

| | Schoolbook | FFT |
|---|---|---|
| Complexity | O(n²) | O(n log n) |
| Code size | 10 lines | 200 lines |
| Precision | Exact (integers) | Floating-point (rounding) |
| Best for | Small polynomials | Large polynomials |
| Used in | Teaching, small inputs | Every fast library |

For n < 100, schoolbook is actually faster (constant factor).
For n > 1,000, FFT wins by an order of magnitude.

## Usage

    ./fft_convolution

Runs five tests:

1. **Small multiplication** — verifies a hand-computed result
2. **Identity** — multiplying by 1 leaves the polynomial unchanged
3. **Random comparison** — 20 random pairs, FFT vs schoolbook
4. **Large polynomial** (degree 500) — both algorithms, timed
5. **Huge polynomial** (degree 100,000) — FFT only, since
   schoolbook would take too long

## Build

    gcc -Wall -Wextra -std=c99 -o fft_convolution fft_convolution.c -lm

**Note the `-lm`** — this uses `cos`, `sin`, `fabs` from libm.

## Example Output

    Test 4: large polynomial (degree ~500)
      n = 500, result length = 999
      schoolbook: 1.83 ms
      FFT:        0.94 ms

    Test 5: huge polynomial (degree ~100,000) — FFT-only
      n = 100000, result length = 199999
      FFT time: 245.12 ms (schoolbook would be ~10000M ops)

For degree 500, schoolbook is 2× slower. For degree 100,000,
schoolbook would take **hours**; FFT takes 0.25 seconds.

## Skills Practiced

- **FFT and IFFT** (reused from Day 53)
- **The convolution theorem** — the mathematical heart
- **Zero-padding** to avoid cyclic wraparound
- **Comparing two algorithms** with the same output
- **Floating-point tolerance** — checking `|a - b| < 1e-6`
- **Complex to real** — discarding the imaginary parts of IFFT output
- **Timing** to demonstrate asymptotic advantage

## Why the Imaginary Parts Are Ignored

When we compute `IFFT(FA ⊙ FB)`, the result should be **purely
real** — the imaginary parts should all be exactly zero.

In floating-point, they're not. They're small numerical noise
(around 10^-15 for the tests here, sometimes larger for big
inputs). We discard them by copying `fa[i].re` into the output.

For integer coefficients, the real parts come out very close
to integers. Rounding to the nearest integer is usually safe
when coefficients are small and N is modest. For very large N
or very large coefficients, you'd need either:

- **Higher precision** (long double, or quad-precision)
- **NTT** — Number Theoretic Transform, which works in a
  finite field and is exact
- **Multiple-modulus NTT** — run NTT in several primes, combine
  with CRT
- **Split coefficients** into low and high halves, run FFT on
  each, recombine — this is what big integer libraries do

## Real-World Uses

This algorithm is **not** an academic curiosity. It's used
literally billions of times per day:

- **Python's `int` multiplication** uses this (via Karatsuba
  for medium, FFT for huge numbers).
- **GMP** (GNU Multi-Precision) uses FFT for very large
  multiplications.
- **libavcodec** (FFmpeg) uses FFT-based convolution for
  audio and video filtering.
- **scipy.signal.fftconvolve** — same thing, in Python.
- **Computer algebra systems** (Mathematica, SageMath) use
  FFT for polynomial arithmetic.
- **Post-quantum cryptography** uses NTT for key exchange
  and signatures.

Every time your phone decodes an MP3, every time Python
computes `2^100000`, every time a cryptographic library
signs a message with Kyber — an FFT-based convolution runs
somewhere.

## Notes

- **The length must be a power of two.** For arbitrary lengths
  you'd pad to the next power of two, or use a non-radix-2 FFT.
- **Zero-padding wastes some computation.** For a = 500 and
  b = 500, the product has length 999, which needs N = 1024.
  We pad both arrays from 500 to 1024. That's a 2× overhead,
  but the asymptotic advantage still dominates.
- **The schoolbook test doesn't scale.** Test 5 skips it because
  a = b = 100,000 with O(n²) would be 10^10 operations, which
  takes ~30 seconds. Not impossible, but wasteful for a test.
- **Floating-point precision matters.** For N > 10^6, the FFT's
  internal error grows as O(√log N), and coefficients near the
  machine's epsilon can be corrupted. Real libraries either
  use NTT (exact) or split coefficients into small pieces.
- **No integer specialization.** This file works on `double[]`.
  A production library would have separate `fft_conv_int32`,
  `fft_conv_int64`, `fft_conv_mod_p` paths.
- **`next_pow2` overflows for very large inputs.** For
  result_len > 2^30, `p <<= 1` would overflow. Real code
  checks this.

## What You Can Extend

- **Big integer multiplication** — treat each integer digit
  as a polynomial coefficient; multiply; carry-propagate to
  get the integer result
- **NTT (Number Theoretic Transform)** — exact integer
  convolution in a finite field; no floating point
- **2D FFT** — apply FFT to rows then columns; the basis of
  image filtering
- **Fast convolution of signals** — audio FIR filters via FFT
- **Chirp-Z transform** — the FFT's generalization for
  non-power-of-two lengths
- **Overlap-add** — the standard technique for convolving a
  long signal with a short filter using fixed-size FFTs
- **Compare with Karatsuba** — a "halfway" algorithm that's
  O(n^1.58) with exact integer arithmetic

## Why This Matters for Math-Flavored Programming

- **The convolution theorem** is one of the most beautiful
  results in mathematics. It says that two operations (convolution
  and multiplication) that look totally unrelated are actually
  the same thing in different domains.
- **Polynomials over finite fields** — the mathematical basis
  for error-correcting codes (Reed-Solomon), cryptography
  (secret sharing), and hashing (CRC is polynomial division).
- **Fast integer multiplication** is what makes Python's
  arbitrary-precision integers practical. Without FFT,
  computing `2^1000000` would take hours.
- **Signal processing** is essentially applied FFT. Every
  filter, every frequency analysis, every modem is
  FFT-flavored.

## Why This Matters as a Portfolio Piece

- **"I wrote a polynomial multiplier faster than schoolbook"**
  is a concrete, verifiable, impressive claim. Anyone can
  read the code and understand the idea.
- **The comparison with schoolbook** is meaningful. It's not
  just "I wrote an algorithm"; it's "I wrote an algorithm
  and here's a direct demonstration of why it's better."
- **The Day 38 + Day 53 + Day 63 trio** tells a coherent
  story: polynomials, FFT, and then the combination. Each
  piece stands on its own but together they're a real
  contribution.
- **The real-world relevance** is undeniable. Anyone reading
  the README learns that this algorithm is inside their
  Python, inside their MP3s, inside their crypto. That's
  the kind of "aha" moment that sticks.
