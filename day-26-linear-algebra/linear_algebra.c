/* linear_algebra.c
 *
 * Core linear algebra operations:
 *   - Vector dot product
 *   - Vector cross product
 *   - Vector magnitude
 *   - Matrix-vector multiplication
 *   - Matrix-matrix multiplication
 *   - Matrix transpose
 *   - Gaussian elimination (solving Ax = b)
 *
 * Uses double precision for accuracy.
 *
 * Usage:
 *   ./linear_algebra
 *
 * Skills practiced:
 *   - 2D arrays
 *   - Nested loops
 *   - Numerical computation
 *   - Linear algebra
 *   - Gaussian elimination
 *   - Math (sqrt, fabs)
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#define MAX 10
#define EPSILON 1e-9

/* --- Vector operations --- */

double dot_product(const double a[], const double b[], int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += a[i] * b[i];
    return sum;
}

void cross_product(const double a[3], const double b[3], double result[3])
{
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
}

double magnitude(const double v[], int n)
{
    return sqrt(dot_product(v, v, n));
}

/* --- Matrix operations --- */

void print_vector(const char *label, const double v[], int n)
{
    printf("%s: [", label);
    for (int i = 0; i < n; i++)
    {
        printf("%8.3f", v[i]);
        if (i < n - 1) printf(", ");
    }
    printf("]\n");
}

void print_matrix(const char *label, double m[MAX][MAX], int rows, int cols)
{
    printf("%s:\n", label);
    for (int i = 0; i < rows; i++)
    {
        printf("  ");
        for (int j = 0; j < cols; j++)
            printf("%8.3f", m[i][j]);
        printf("\n");
    }
}

void mat_vec_mul(double m[MAX][MAX], const double v[], double result[],
                 int rows, int cols)
{
    for (int i = 0; i < rows; i++)
    {
        result[i] = 0.0;
        for (int j = 0; j < cols; j++)
            result[i] += m[i][j] * v[j];
    }
}

void mat_mul(double a[MAX][MAX], double b[MAX][MAX], double result[MAX][MAX],
             int a_rows, int a_cols, int b_cols)
{
    for (int i = 0; i < a_rows; i++)
        for (int j = 0; j < b_cols; j++)
        {
            result[i][j] = 0.0;
            for (int k = 0; k < a_cols; k++)
                result[i][j] += a[i][k] * b[k][j];
        }
}

void mat_transpose(double m[MAX][MAX], double result[MAX][MAX],
                   int rows, int cols)
{
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            result[j][i] = m[i][j];
}

/* --- Gaussian elimination --- */

/* Solve Ax = b using Gaussian elimination with partial pivoting.
 * A is n x n, b is length n. Result stored in x.
 * Returns true on success, false if singular.
 */
bool gaussian_solve(double A[MAX][MAX], double b[], double x[], int n)
{
    /* Build augmented matrix */
    double aug[MAX][MAX + 1];
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
            aug[i][j] = A[i][j];
        aug[i][n] = b[i];
    }

    /* Forward elimination with partial pivoting */
    for (int col = 0; col < n; col++)
    {
        /* Find pivot */
        int pivot = col;
        for (int row = col + 1; row < n; row++)
            if (fabs(aug[row][col]) > fabs(aug[pivot][col]))
                pivot = row;

        if (fabs(aug[pivot][col]) < EPSILON)
            return false;   /* singular */

        /* Swap rows */
        if (pivot != col)
        {
            for (int j = 0; j <= n; j++)
            {
                double tmp = aug[col][j];
                aug[col][j] = aug[pivot][j];
                aug[pivot][j] = tmp;
            }
        }

        /* Eliminate below */
        for (int row = col + 1; row < n; row++)
        {
            double factor = aug[row][col] / aug[col][col];
            for (int j = col; j <= n; j++)
                aug[row][j] -= factor * aug[col][j];
        }
    }

    /* Back substitution */
    for (int i = n - 1; i >= 0; i--)
    {
        double sum = aug[i][n];
        for (int j = i + 1; j < n; j++)
            sum -= aug[i][j] * x[j];
        x[i] = sum / aug[i][i];
    }

    return true;
}

int main(void)
{
    printf("====================================\n");
    printf("         LINEAR ALGEBRA             \n");
    printf("====================================\n\n");

    /* --- Vector operations --- */
    double v1[] = {1, 2, 3};
    double v2[] = {4, 5, 6};
    double cross[3];

    print_vector("v1", v1, 3);
    print_vector("v2", v2, 3);

    printf("\nDot product:    %.3f\n", dot_product(v1, v2, 3));
    printf("Magnitude v1:   %.3f\n", magnitude(v1, 3));

    cross_product(v1, v2, cross);
    print_vector("Cross product", cross, 3);

    /* --- Matrix-vector multiplication --- */
    double A[MAX][MAX] = {
        {2, 1, 0},
        {1, 3, 1},
        {0, 1, 2}
    };
    double x[] = {1, 2, 3};
    double Ax[3];

    printf("\n");
    print_matrix("Matrix A", A, 3, 3);
    print_vector("Vector x", x, 3);

    mat_vec_mul(A, x, Ax, 3, 3);
    print_vector("A * x", Ax, 3);

    /* --- Matrix multiplication --- */
    double B[MAX][MAX] = {
        {1, 0, 1},
        {0, 1, 0},
        {1, 0, 1}
    };
    double AB[MAX][MAX];

    printf("\n");
    print_matrix("Matrix B", B, 3, 3);

    mat_mul(A, B, AB, 3, 3, 3);
    print_matrix("A * B", AB, 3, 3);

    /* --- Transpose --- */
    double AT[MAX][MAX];
    mat_transpose(A, AT, 3, 3);
    printf("\n");
    print_matrix("A^T", AT, 3, 3);

    /* --- Solve linear system Ax = b --- */
    double M[MAX][MAX] = {
        {2, 1, -1},
        {-3, -1, 2},
        {-2, 1, 2}
    };
    double b[] = {8, -11, -3};
    double solution[3];

    printf("\n");
    print_matrix("System M", M, 3, 3);
    print_vector("Right-hand side b", b, 3);

    if (gaussian_solve(M, b, solution, 3))
    {
        printf("\nSolution (Gaussian elimination):\n");
        print_vector("x", solution, 3);

        /* Verify: M * x should equal b */
        double check[3];
        mat_vec_mul(M, solution, check, 3, 3);
        printf("\nVerification (M * x should equal b):\n");
        print_vector("M * x", check, 3);
        print_vector("b    ", b, 3);
    }
    else
    {
        printf("\nSystem is singular—no unique solution.\n");
    }

    return 0;
}
