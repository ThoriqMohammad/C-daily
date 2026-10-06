# Day 26 — linear_algebra

Core linear algebra operations in C.

## Operations

| Operation | Formula |
|-----------|---------|
| **Dot product** | `Σ aᵢbᵢ` |
| **Cross product** | `a × b` (3D only) |
| **Magnitude** | `√(a · a)` |
| **Matrix-vector** | `Ax = b` |
| **Matrix-matrix** | `C[i][j] = Σ A[i][k] B[k][j]` |
| **Transpose** | `Aᵀ[j][i] = A[i][j]` |
| **Gaussian elimination** | Solve `Ax = b` |

## Usage

    ./linear_algebra

## Build

    gcc -Wall -Wextra -std=c99 -o linear_algebra linear_algebra.c -lm

**Note:** `-lm` is needed for `sqrt` and `fabs`.

## Skills Practiced

- **2D arrays** — matrix storage
- **Nested loops** — matrix traversal
- **Numerical computation** — `double` precision
- **Gaussian elimination** — solving systems
- **Math** — `sqrt`, `fabs`
- **Pivoting** — numerical stability

## The Math

### Dot Product

    a · b = a₁b₁ + a₂b₂ + ... + aₙbₙ

Example: `[1,2,3] · [4,5,6] = 4 + 10 + 18 = 32`

### Cross Product (3D)

    a × b = (a₂b₃ − a₃b₂, a₃b₁ − a₁b₃, a₁b₂ − a₂b₁)

Example: `[1,2,3] × [4,5,6] = (-3, 6, -3)`

### Matrix Multiplication

    C[i][j] = Σ A[i][k] × B[k][j]

The inner dimension must match: `(m × n) × (n × p) = (m × p)`

### Gaussian Elimination

Solves `Ax = b` by:

1. **Forward elimination** — zero out below the diagonal
2. **Back substitution** — solve from the bottom up

**Partial pivoting** swaps rows to avoid dividing by small numbers.

## Why This Matters

| Domain | Use |
|--------|-----|
| **Graphics** | 3D transformations |
| **Machine learning** | Neural networks |
| **Physics** | Linear systems |
| **Engineering** | Structural analysis |
| **Cryptography** | Hill cipher |
| **Quantitative finance** | Portfolio optimization |

## Notes

- Uses `double` for numerical accuracy.
- `EPSILON` is used to detect singular matrices.
- Partial pivoting improves numerical stability.
- Maximum matrix size is 10×10.
