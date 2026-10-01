# Day 22 — matrix_operations

Performs basic matrix operations in C.

## Operations

| Operation | What It Does |
|-----------|--------------|
| **Addition** | `A + B` (element-wise) |
| **Subtraction** | `A - B` (element-wise) |
| **Multiplication** | `A * B` (row × column) |
| **Transpose** | Flip rows and columns |
| **Identity** | 1s on the diagonal |
| **Determinant (2x2)** | `ad - bc` |
| **Determinant (3x3)** | Cofactor expansion |

## Usage

    ./matrix_operations

## Build

    gcc -Wall -Wextra -std=c99 -o matrix_operations matrix_operations.c

## Skills Practiced

- **2D arrays** — matrices
- **Nested loops** — matrix traversal
- **Functions with 2D array parameters**
- **Linear algebra** — matrix operations
- **Math** — determinants

## The Math

### Addition / Subtraction

Element-by-element:

    C[i][j] = A[i][j] ± B[i][j]

### Multiplication

Row-by-column:

    C[i][j] = Σ A[i][k] × B[k][j]

For `A` (2×3) × `B` (3×2):

    [1 2 3]   [7  8 ]   [58  64]
    [4 5 6] × [9  10] = [139 154]
              [11 12]

### Transpose

Swap rows and columns:

    A[i][j] = Aᵀ[j][i]

### Determinant (2×2)

    |a b|
    |c d| = ad − bc

### Determinant (3×3)

    |a b c|
    |d e f| = a(ei − fh) − b(di − fg) + c(dh − eg)
    |g h i|

## Notes

- Maximum matrix size is 10×10.
- All matrices are `int` for simplicity.
- Multiplication handles non-square matrices.
- Transpose handles non-square matrices.
- Determinants are only implemented for 2×2 and 3×3.

## Real-World Uses

| Domain | Use |
|--------|-----|
| Graphics | Transformations, rotations |
| Machine learning | Neural networks |
| Physics | Linear systems |
| Cryptography | Hill cipher |
| Engineering | Structural analysis |

## What You Can Extend

- Determinant for any size (Gaussian elimination)
- Matrix inversion
- Scalar multiplication
- Matrix power
- `double` matrices instead of `int`
