/* matrix_operations.c
 *
 * Performs basic matrix operations:
 *   - Addition
 *   - Subtraction
 *   - Multiplication
 *   - Transpose
 *   - Identity
 *   - Determinant (2x2 and 3x3)
 *
 * Matrices are stored as 2D arrays with a maximum size of 10x10.
 *
 * Usage:
 *   ./matrix_operations
 *
 * Skills practiced:
 *   - 2D arrays
 *   - Nested loops
 *   - Functions with 2D array parameters
 *   - Linear algebra
 *   - Math (determinants)
 */

#include <stdio.h>
#include <stdlib.h>

#define MAX 10

/* Print a matrix */
void print_matrix(const char *label, int m[MAX][MAX], int rows, int cols)
{
    printf("%s\n", label);
    for (int i = 0; i < rows; i++)
    {
        printf("  ");
        for (int j = 0; j < cols; j++)
            printf("%4d", m[i][j]);
        printf("\n");
    }
    printf("\n");
}

/* Initialize a matrix with values */
void init_matrix(int m[MAX][MAX], int rows, int cols, const int *values)
{
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            m[i][j] = values[i * cols + j];
}

/* Add two matrices: result = a + b */
void add_matrix(int a[MAX][MAX], int b[MAX][MAX],
                int result[MAX][MAX], int rows, int cols)
{
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            result[i][j] = a[i][j] + b[i][j];
}

/* Subtract: result = a - b */
void sub_matrix(int a[MAX][MAX], int b[MAX][MAX],
                int result[MAX][MAX], int rows, int cols)
{
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            result[i][j] = a[i][j] - b[i][j];
}

/* Multiply: result = a * b
 * a is rows_a x cols_a
 * b is cols_a x cols_b
 * result is rows_a x cols_b
 */
void mul_matrix(int a[MAX][MAX], int b[MAX][MAX],
                int result[MAX][MAX],
                int rows_a, int cols_a, int cols_b)
{
    for (int i = 0; i < rows_a; i++)
    {
        for (int j = 0; j < cols_b; j++)
        {
            int sum = 0;
            for (int k = 0; k < cols_a; k++)
                sum += a[i][k] * b[k][j];
            result[i][j] = sum;
        }
    }
}

/* Transpose: result = a^T */
void transpose(int a[MAX][MAX], int result[MAX][MAX],
               int rows, int cols)
{
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
            result[j][i] = a[i][j];
}

/* Identity matrix */
void identity(int m[MAX][MAX], int n)
{
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            m[i][j] = (i == j) ? 1 : 0;
}

/* Determinant of 2x2 */
int det2x2(int m[MAX][MAX])
{
    return m[0][0] * m[1][1] - m[0][1] * m[1][0];
}

/* Determinant of 3x3 */
int det3x3(int m[MAX][MAX])
{
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
         - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
         + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

int main(void)
{
    int a[MAX][MAX] = {0};
    int b[MAX][MAX] = {0};
    int result[MAX][MAX] = {0};

    printf("====================================\n");
    printf("        MATRIX OPERATIONS           \n");
    printf("====================================\n\n");

    /* --- 2x2 addition and subtraction --- */
    int a_vals[] = {1, 2, 3, 4};
    int b_vals[] = {5, 6, 7, 8};

    init_matrix(a, 2, 2, a_vals);
    init_matrix(b, 2, 2, b_vals);

    print_matrix("Matrix A:", a, 2, 2);
    print_matrix("Matrix B:", b, 2, 2);

    add_matrix(a, b, result, 2, 2);
    print_matrix("A + B:", result, 2, 2);

    sub_matrix(a, b, result, 2, 2);
    print_matrix("A - B:", result, 2, 2);

    /* --- Multiplication --- */
    int m1_vals[] = {1, 2, 3, 4, 5, 6};     /* 2x3 */
    int m2_vals[] = {7, 8, 9, 10, 11, 12};  /* 3x2 */

    init_matrix(a, 2, 3, m1_vals);
    init_matrix(b, 3, 2, m2_vals);

    print_matrix("Matrix A (2x3):", a, 2, 3);
    print_matrix("Matrix B (3x2):", b, 3, 2);

    mul_matrix(a, b, result, 2, 3, 2);
    print_matrix("A * B (2x2):", result, 2, 2);

    /* --- Transpose --- */
    int t_vals[] = {1, 2, 3, 4, 5, 6};   /* 2x3 */

    init_matrix(a, 2, 3, t_vals);
    print_matrix("Matrix (2x3):", a, 2, 3);

    transpose(a, result, 2, 3);
    print_matrix("Transpose (3x2):", result, 3, 2);

    /* --- Identity --- */
    identity(result, 4);
    print_matrix("Identity (4x4):", result, 4, 4);

    /* --- Determinants --- */
    int d2_vals[] = {1, 2, 3, 4};
    init_matrix(a, 2, 2, d2_vals);
    print_matrix("2x2 Matrix:", a, 2, 2);
    printf("Determinant: %d\n\n", det2x2(a));

    int d3_vals[] = {6, 1, 1, 4, -2, 5, 2, 8, 7};
    init_matrix(a, 3, 3, d3_vals);
    print_matrix("3x3 Matrix:", a, 3, 3);
    printf("Determinant: %d\n", det3x3(a));

    return 0;
}
