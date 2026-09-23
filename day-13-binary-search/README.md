# Day 13 — binary_search

Implements **binary search** on a sorted array of integers.

## What Is Binary Search?

Binary search finds a target value in a **sorted array** in
**O(log n)** time by repeatedly halving the search interval.

| Array Size | Linear Search | Binary Search |
|-----------|---------------|---------------|
| 10 | 10 steps | 4 steps |
| 100 | 100 steps | 7 steps |
| 1,000 | 1,000 steps | 10 steps |
| 1,000,000 | 1,000,000 steps | 20 steps |

## Usage

    ./binary_search <target> <n> <value1> <value2> ...

- `target`: the value to search for
- `n`: the number of values
- values: the integers (will be sorted automatically)

## Examples

**Found:**

    $ ./binary_search 7 10 1 3 5 7 9 11 13 15 17 19
    Sorted array: 1 3 5 7 9 11 13 15 17 19
    Iterative: found 7 at index 3
    Recursive: found 7 at index 3

**Not found:**

    $ ./binary_search 4 10 1 3 5 7 9 11 13 15 17 19
    Sorted array: 1 3 5 7 9 11 13 15 17 19
    Iterative: 4 not found
    Recursive: 4 not found

## Build

    gcc -Wall -Wextra -std=c99 -o binary_search binary_search.c

## Skills Practiced

- **Binary search algorithm**
- **Iterative vs. recursive** implementations
- **`qsort`** for sorting
- **Function pointers** (via `qsort`'s comparator)
- **`argc` / `argv`**
- **`malloc` / `free`**
- **O(log n) vs. O(n)** thinking

## The Key Insight

At each step, binary search **halves the search space**:

    n → n/2 → n/4 → n/8 → ... → 1

That's why it takes **log₂(n)** steps.

## Why It Works

1. The array is **sorted**.
2. We compare the target to the **middle element**.
3. If equal, we're done.
4. If target is smaller, search the **left half**.
5. If target is larger, search the **right half**.
6. Repeat until found or the interval is empty.

## Common Pitfalls

| Pitfall | Fix |
|---------|-----|
| `(low + high) / 2` overflows | Use `low + (high - low) / 2` |
| `low < high` vs. `low <= high` | Use `<=` for inclusive bounds |
| Infinite loop | Ensure `low` or `high` always moves |

## Notes

- The array is sorted with `qsort` before searching.
- The comparator returns `(x > y) - (x < y)` to avoid overflow.
- Both iterative and recursive versions are provided.
