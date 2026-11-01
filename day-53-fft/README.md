# Day 53 — fft

The **Fast Fourier Transform**: convert a signal from the time
domain to the frequency domain in O(n log n) instead of O(n²).

Named by IEEE as one of the **top 10 algorithms of the 20th
century**, alongside the simplex method and Kalman filter, FFT
is the algorithm that makes modern signal processing possible.
Without it, audio compression, image compression, radar, MRI,
and 5G would be computationally infeasible.

## What Is the Fourier Transform?

Any signal — a sound wave, a stock price history, a radio
signal — can be decomposed into a sum of sine waves at
different frequencies. The **Fourier transform** computes
that decomposition. Given a time-domain signal, it produces
a **frequency-domain** representation: how much of each
frequency is present.

For a discrete signal of `n` samples `x[0], x[1], ..., x[n-1]`,
the **Discrete Fourier Transform** is:

    X[k] = sum_{j=0}^{n-1} x[j] * e^(-2πi jk/n)

Each `X[k]` is a complex number representing the amplitude and
phase of frequency `k`.

Computed directly, this is **O(n²)** — n outputs, each requiring
n multiplications. For n = 1 million, that's 10^12 operations,
about 10 minutes on a modern CPU. That's too slow for real-time
audio, image processing, or any interactive application.

## The Trick: Divide and Conquer

The FFT (Cooley-Tukey, 1965) notices that the DFT has structure.
Split the input into even and odd indices:

    E[j] = x[2j]      (n/2 samples)
    O[j] = x[2j+1]    (n/2 samples)

Compute their DFTs independently, then combine:

    X[k]       = E[k] + w^k * O[k]
    X[k + n/2] = E[k] - w^k * O[k]

    where w = e^(-2πi/n)

This is a **butterfly**: two outputs from two inputs and one
complex multiplication. The recursion is:

    FFT(n):
        if n == 1: return x
        E = FFT(x[0], x[2], ...)
        O = FFT(x[1], x[3], ...)
        combine with twiddle factors

The recursion depth is log₂(n), and each level does O(n)
work. Total: **O(n log n)**.

For n = 1,048,576:

| Algorithm | Operations | Time (rough) |
|-----------|-----------|--------------|
| DFT | ~10^12 | 10 minutes |
| FFT | ~2×10^7 | 20 milliseconds |

That's a **30,000× speedup**. That's why the FFT exists.

## Implementation Notes

This file uses the **iterative** version of the FFT, which is
equivalent to the recursive version but runs in-place and
faster:

1. **Bit-reversal permutation.** The recursive FFT splits input
   by even/odd indices, which is equivalent to bit-reversing
   the index. So we first reorder the array to the order the
   recursive version would visit it.
2. **Butterflies by level.** For each level (length 2, 4, 8, ...,
   n), apply butterflies with the appropriate twiddle factor.
3. **Scale by 1/n on the inverse.** The forward FFT doesn't
   scale, so the inverse does.

## Usage

    ./fft

Runs five self-tests and prints `ok` or `FAIL` for each assertion.

## Build

    gcc -Wall -Wextra -std=c99 -o fft fft.c -lm

**Note the `-lm`** — this program uses `cos`, `sin`, `hypot`,
and `fabs` from libm.

## Skills Practiced

- **Complex number arithmetic** without a complex library
- **Recursive to iterative** — the same algorithm written both ways
- **Bit-reversal permutation** — the tricky first step
- **Roots of unity and twiddle factors**
- **Floating-point tolerance** — comparing complex results
- **Signal analysis** — interpreting the output

## What the Tests Show

| Test | Idea |
|------|------|
| 1. FFT vs DFT | Both algorithms agree to 10⁻⁹ |
| 2. Round-trip | Forward then inverse recovers the input |
| 3. Impulse | `[1, 0, 0, ...]` has a flat spectrum of all 1s |
| 4. Single tone | A pure cosine at frequency `k` gives one peak at bin `k` |
| 5. Performance | FFT is at least 5× faster than DFT on n=4096 (usually 20×+) |

Test 4 is the most informative: it confirms that the FFT actually
identifies frequency content correctly, not just that two
algorithms agree.

## Bit-Reversal: The Tricky Part

The recursive FFT visits indices in a strange order:

    Level 0: [0, 1, 2, 3, 4, 5, 6, 7]
    Level 1: [0, 2, 4, 6] [1, 3, 5, 7]
    Level 2: [0, 4] [2, 6] [1, 5] [3, 7]
    Level 3: [0] [4] [2] [6] [1] [5] [3] [7]

Look at the leaf order: 0, 4, 2, 6, 1, 5, 3, 7.

Convert to binary:

    000 -> 000
    100 -> 001
    010 -> 010
    110 -> 011
    001 -> 100
    101 -> 101
    011 -> 110
    111 -> 111

Every index's binary is **bit-reversed**. That's why we call it
the bit-reversal permutation, and why we can do the whole FFT
iteratively: first bit-reverse the array once, then all butterflies
work on adjacent elements.

The `bit_reverse` function:

```c
unsigned bit_reverse(unsigned x, unsigned bits) {
    unsigned r = 0;
    for (unsigned i = 0; i < bits; i++) {
        r = (r << 1) | (x & 1);
        x >>= 1;
    }
    return r;
}
