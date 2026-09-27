/* statistics_calculator.c
 *
 * Computes basic statistics on a list of numbers:
 *   - Count
 *   - Sum
 *   - Mean (average)
 *   - Minimum
 *   - Maximum
 *   - Range
 *   - Variance
 *   - Standard deviation
 *   - Median
 *
 * Usage:
 *   ./statistics_calculator <n> <value1> <value2> ...
 *
 * Example:
 *   ./statistics_calculator 5 10 20 30 40 50
 *
 * Skills practiced:
 *   - Arrays
 *   - Sorting (qsort)
 *   - Math (sqrt, pow)
 *   - argc / argv
 *   - Statistical formulas
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Comparison for qsort */
int compare(const void *a, const void *b)
{
    double x = *(const double *)a;
    double y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Compute the mean */
double mean(const double arr[], int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += arr[i];
    return sum / n;
}

/* Compute the median (requires sorted array) */
double median(const double arr[], int n)
{
    if (n % 2 == 1)
        return arr[n / 2];
    else
        return (arr[n / 2 - 1] + arr[n / 2]) / 2.0;
}

/* Compute the variance (population variance) */
double variance(const double arr[], int n)
{
    double m = mean(arr, n);
    double sum = 0.0;

    for (int i = 0; i < n; i++)
    {
        double diff = arr[i] - m;
        sum += diff * diff;
    }

    return sum / n;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <n> <value1> <value2> ...\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);

    if (n <= 0 || argc != n + 2)
    {
        fprintf(stderr, "Error: n must match the number of values.\n");
        return 1;
    }

    double *arr = malloc(n * sizeof(double));
    if (arr == NULL)
    {
        fprintf(stderr, "Error: malloc failed.\n");
        return 1;
    }

    for (int i = 0; i < n; i++)
        arr[i] = atof(argv[i + 2]);

    /* Sum and min/max */
    double sum = 0.0;
    double min = arr[0], max = arr[0];
    for (int i = 0; i < n; i++)
    {
        sum += arr[i];
        if (arr[i] < min) min = arr[i];
        if (arr[i] > max) max = arr[i];
    }

    /* Sort for median */
    qsort(arr, n, sizeof(double), compare);

    /* Compute statistics */
    double m = mean(arr, n);
    double med = median(arr, n);
    double var = variance(arr, n);
    double sd = sqrt(var);

    printf("====================================\n");
    printf("       STATISTICS CALCULATOR        \n");
    printf("====================================\n\n");

    printf("Count:              %d\n", n);
    printf("Sum:                %.4f\n", sum);
    printf("Mean:               %.4f\n", m);
    printf("Median:             %.4f\n", med);
    printf("Minimum:            %.4f\n", min);
    printf("Maximum:            %.4f\n", max);
    printf("Range:              %.4f\n", max - min);
    printf("Variance:           %.4f\n", var);
    printf("Standard deviation: %.4f\n", sd);

    free(arr);
    return 0;
}
