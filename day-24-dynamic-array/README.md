# Day 24 — dynamic_array

A **dynamic array** (growable array) implementation in C.

## What Is a Dynamic Array?

A regular array has a **fixed size**. A dynamic array grows
as needed, doubling its capacity when full.

| Operation | Fixed Array | Dynamic Array |
|-----------|-------------|---------------|
| Append | ❌ Fixed size | ✅ Amortized O(1) |
| Grow | ❌ Not possible | ✅ Doubles |
| Shrink | ❌ Not possible | ✅ Shrink-to-fit |
| Access | O(1) | O(1) |

## Operations

| Operation | Time |
|-----------|------|
| `append` | Amortized O(1) |
| `get` / `set` | O(1) |
| `insert` | O(n) |
| `remove` | O(n) |
| `index_of` | O(n) |

## Usage

    ./dynamic_array

## Build

    gcc -Wall -Wextra -std=c99 -o dynamic_array dynamic_array.c

## Skills Practiced

- **Structs** — the `DynArray` type
- **Dynamic memory** — `malloc`, `realloc`, `free`
- **Amortized analysis** — why doubling is O(1)
- **Array manipulation** — shifting elements
- **Error handling** — bounds checks

## Amortized O(1) Append

When the array is full, we double the capacity.

| Appends | Doubling Cost | Total Cost |
|---------|---------------|------------|
| 1 | 1 | 1 |
| 2 | 2 | 3 |
| 3 | 0 | 3 |
| 4 | 4 | 7 |
| 5–8 | 8 | 15 |

**Total cost for n appends: O(n).**
**Average cost per append: O(1).**

This is amortized analysis.

## Why This Matters

Dynamic arrays are the foundation of many data structures:

| Structure | Built On |
|-----------|----------|
| `std::vector` (C++) | Dynamic array |
| `ArrayList` (Java) | Dynamic array |
| Python `list` | Dynamic array |
| Go `slice` | Dynamic array |

**Understanding this makes all of them clearer.**

## Notes

- Initial capacity is 2.
- Capacity doubles when full.
- `realloc` may move the block—always use the return value.
- `shrink_to_fit` is optional but reduces waste.
- All bounds are checked.

## What You Can Extend

- Generic version with `void *`
- Iterators
- Map / filter / reduce operations
- Sorting with `qsort`
- Binary search on sorted arrays
  | 24  | dynamic_array | ⚙️ Pure C | Dynamic memory, amortized O(1) |
