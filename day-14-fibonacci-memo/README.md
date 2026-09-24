# Day 14 — fibonacci_memo

Computes Fibonacci numbers three ways, comparing their performance.

## The Three Approaches

| Approach | Time | Space | Description |
|----------|------|-------|-------------|
| **Naive recursion** | O(2^n) | O(n) | Recomputes everything |
| **Memoized recursion** | O(n) | O(n) | Caches results |
| **Iterative** | O(n) | O(1) | Bottom-up, no recursion |

## Usage

    ./fibonacci_memo n

`n` must be between 0 and 90.

## Example

    $ ./fibonacci_memo 40
    Fibonacci(40)
    ================

    Iterative:  102334155  (0.000001 s)
    Memoized:   102334155  (0.000002 s)
    Naive:      102334155  (1.234567 s)

## Build

    gcc -Wall -Wextra -std=c99 -o fibonacci_memo fibonacci_memo.c

## Skills Practiced

- **Dynamic programming** — memoization
- **Recursion** vs. iteration
- **Arrays** for caching
- **Time complexity** — O(2^n) vs. O(n)
- **`malloc` / `free`**
- **`clock()`** for timing
- **`argc` / `argv`**

## Why Memoization Works

The naive recursive Fibonacci recomputes the same values many times:

    fib(5)
    ├── fib(4)
    │   ├── fib(3)
    │   │   ├── fib(2)
    │   │   └── fib(1)
    │   └── fib(2)
    └── fib(3)
        ├── fib(2)
        └── fib(1)

`fib(3)` is computed twice, `fib(2)` three times, etc. — exponential blowup.

**Memoization** stores each result the first time it's computed. After that,
it's just a lookup — O(1).

## Performance Comparison

| n | Naive (O(2^n)) | Memoized (O(n)) |
|---|----------------|-----------------|
| 10 | instant | instant |
| 20 | 0.01 s | instant |
| 30 | 1 s | instant |
| 40 | 60 s | instant |
| 50 | years | instant |

## Why the Values Overflow

`long long` can hold up to about 9.2 × 10^18. Fibonacci(92) exceeds that.

The `MAX_N` is set to 90 to avoid overflow.

## Notes

- `memo[]` is initialized to `-1` to mark "not yet computed."
- The naive version is skipped for `n > 40` to avoid long waits.
- Timing uses `clock()` — CPU time, not wall time.
- The iterative version is the most efficient.
