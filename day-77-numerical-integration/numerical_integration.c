/* numerical_integration.c
 *
 * Step order (what this program does):
 *
 *   1. Define the problem: given f(x), estimate the definite
 *      integral of f over [a, b], where f is a C function
 *      passed in as a function pointer.
 *   2. Implement three methods:
 *      a. TRAPEZOIDAL: divide [a,b] into n subintervals,
 *         approximate f on each by a straight line, sum areas.
 *         Error: O(h^2) where h = (b-a)/n.
 *      b. SIMPSON: divide [a,b] into n subintervals, approximate
 *         f on each pair by a parabola, sum areas.
 *         Error: O(h^4). Requires n even.
 *      c. MONTE CARLO: sample n random points uniformly in
 *         [a,b] x [0, max_f], count how many fall under f,
 *         multiply by rectangle area. Error: O(1/sqrt(n)).
 *   3. For each method, run with n = 10, 100, 1000, 10000
 *      subinterval counts and record the estimated value and
 *      actual error against the known exact integral.
 *   4. Print a table comparing methods.
 *   5. Demonstrate the convergence order: trapezoidal error
 *      should drop ~100x when n grows 10x (since O(h^2));
 *      Simpson's ~10000x (O(h^4)); Monte Carlo's ~3x
 *      (O(1/sqrt(n))).
 *
 * What numerical integration is:
 *
 *   The definite integral ∫[a,b] f(x) dx is the area under
 *   the curve. For simple f, it's computable in closed form.
 *   For arbitrary f (measured data, complicated formulas, or
 *   no closed form), it must be approximated.
 *
 *   Numerical integration (or "quadrature") is the family of
 *   techniques for approximating the integral from f evaluated
 *   at finitely many points.
 *
 *   The three methods here cover the space:
 *     - Trapezoidal:  O(h^2), deterministic, needs f(x) only
 *     - Simpson's:    O(h^4), deterministic, needs f(x) only
 *                     but requires n even and assumes smoothness
 *     - Monte Carlo:  O(1/sqrt(n)), stochastic, works in ANY
 *                     number of dimensions, not just 1D
 *
 *   The famous result: for smooth 1D functions, Simpson's
 *   beats Monte Carlo by orders of magnitude for the same
 *   number of evaluations. But Monte Carlo's error rate is
 *   independent of dimension — in 100 dimensions, Simpson's
 *   becomes infeasible (curse of dimensionality) and Monte
 *   Carlo is the only practical choice.
 *
 * What this implementation does NOT do:
 *   - Adaptive step size (real quadrature libraries adjust h)
 *   - Higher-order methods (Romberg, Gauss-Legendre)
 *   - Error estimation without knowing the true answer
 *   - Multi-dimensional Monte Carlo (though the idea generalizes)
 *
 * Usage:
 *   ./numerical_integration         run tests
 *
 * Skills practiced:
 *   - Function pointers as first-class data
 *   - Iterative numerical methods with tunable precision
 *   - Error analysis (O(h^2) vs O(h^4) vs O(1/sqrt(n)))
 *   - Empirical convergence verification
 *   - Random sampling (Monte Carlo)
 *   - Comparing methods against the known answer
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* ---------- test functions and their exact integrals ---------- */

/* Each test function f(x) has a known antiderivative F(x),
 * so we can compute the exact integral F(b) - F(a). */

/* f(x) = x^2 */
static double f_square(double x) { return x * x; }
/* ∫[0,1] x^2 dx = 1/3 */

/* f(x) = sin(x) */
static double f_sin(double x) { return sin(x); }
/* ∫[0,π] sin(x) dx = 2 */

/* f(x) = exp(-x^2), the Gaussian kernel
 * No elementary antiderivative, but ∫[-∞,∞] = sqrt(π).
 * The finite integral has no closed form, so we use a known
 * numerical value (or compute it independently). */
static double f_gaussian(double x) { return exp(-x * x); }
/* ∫[0,1] exp(-x^2) dx ≈ 0.7468241328... */

/* ---------- trapezoidal rule ---------- */

/* Estimate ∫[a,b] f(x) dx using n trapezoids.
 *
 * Error: O(h^2) where h = (b-a)/n. */
static double trapezoidal(double (*f)(double), double a, double b, int n)
{
    if (n < 1) return 0.0;

    double h = (b - a) / n;
    double sum = 0.5 * (f(a) + f(b));

    for (int i = 1; i < n; i++)
        sum += f(a + i * h);

    return h * sum;
}

/* ---------- Simpson's rule ---------- */

/* Estimate ∫[a,b] f(x) dx using n/2 parabolas (requires n even).
 *
 * Simpson's rule uses three-point quadratic fit per pair of
 * intervals:
 *     ∫[x_i, x_{i+2}] f(x) dx ≈ h/3 * (f(x_i) + 4*f(x_{i+1}) + f(x_{i+2}))
 *
 * Error: O(h^4). Much more accurate than trapezoidal for
 * smooth functions, same number of function evaluations. */
static double simpson(double (*f)(double), double a, double b, int n)
{
    if (n < 2) n = 2;
    if (n % 2 != 0) n++;   /* Simpson needs even n */

    double h = (b - a) / n;
    double sum = f(a) + f(b);

    for (int i = 1; i < n; i += 2)
        sum += 4.0 * f(a + i * h);
    for (int i = 2; i < n; i += 2)
        sum += 2.0 * f(a + i * h);

    return (h / 3.0) * sum;
}

/* ---------- Monte Carlo integration ---------- */

/* Estimate ∫[a,b] f(x) dx using n random samples.
 *
 * Method: pick n uniformly random x in [a,b], average f(x),
 * multiply by (b-a). By the law of large numbers, this
 * converges to the integral.
 *
 * Error: O(1/sqrt(n)) — converges much slower than the
 * deterministic methods for smooth functions, but works
 * uniformly well regardless of dimension. */
static double monte_carlo(double (*f)(double), double a, double b, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; i++)
    {
        double x = a + (b - a) * ((double)rand() / RAND_MAX);
        sum += f(x);
    }
    return (b - a) * sum / n;
}

/* ---------- test harness ---------- */

typedef double (*integrator_t)(double (*)(double), double, double, int);

typedef struct
{
    const char *name;
    integrator_t method;
    double (*f)(double);
    double a, b;
    double exact;
    const char *fname;
} test_case;

static test_case cases[] = {
    { "trapezoidal", trapezoidal, f_square, 0.0, 1.0, 1.0 / 3.0,
      "x^2 over [0,1]" },
    { "simpson",     simpson,     f_square, 0.0, 1.0, 1.0 / 3.0,
      "x^2 over [0,1]" },
    { "monte_carlo", monte_carlo, f_square, 0.0, 1.0, 1.0 / 3.0,
      "x^2 over [0,1]" },

    { "trapezoidal", trapezoidal, f_sin, 0.0, M_PI, 2.0,
      "sin(x) over [0,pi]" },
    { "simpson",     simpson,     f_sin, 0.0, M_PI, 2.0,
      "sin(x) over [0,pi]" },
    { "monte_carlo", monte_carlo, f_sin, 0.0, M_PI, 2.0,
      "sin(x) over [0,pi]" },

    { "trapezoidal", trapezoidal, f_gaussian, 0.0, 1.0, 0.746824132812427,
      "exp(-x^2) over [0,1]" },
    { "simpson",     simpson,     f_gaussian, 0.0, 1.0, 0.746824132812427,
      "exp(-x^2) over [0,1]" },
    { "monte_carlo", monte_carlo, f_gaussian, 0.0, 1.0, 0.746824132812427,
      "exp(-x^2) over [0,1]" },
};

static int nc = sizeof(cases) / sizeof(cases[0]);

/* ---------- test: basic correctness ---------- */

static void test_correctness(void)
{
    printf("Test 1: each method with n=10000 should be close\n\n");

    int steps[] = { 10000 };
    for (size_t c = 0; c < (size_t)nc; c++)
    {
        double est = cases[c].method(cases[c].f, cases[c].a, cases[c].b,
                                     steps[0]);
        double err = fabs(est - cases[c].exact);
        double rel = err / fabs(cases[c].exact) * 100.0;
        printf("  %-12s %-22s est=%.10f exact=%.10f  rel.err=%.6f%%  %s\n",
               cases[c].name, cases[c].fname, est, cases[c].exact, rel,
               rel < 0.01 ? "ok" : (rel < 1.0 ? "ok" : "FAIL"));
    }
    printf("\n");
}

/* ---------- test: convergence order ---------- */

static void test_convergence(const char *method_name,
                             double (*f)(double), double a, double b,
                             double exact,
                             int base_n, int max_growth)
{
    printf("  %s convergence:\n", method_name);

    int n = base_n;
    double prev_err = 0.0;
    for (int step = 0; step < max_growth; step++)
    {
        double est;
        if (method_name[0] == 'm')
            est = monte_carlo(f, a, b, n);
        else if (method_name[0] == 't')
            est = trapezoidal(f, a, b, n);
        else
            est = simpson(f, a, b, n);

        double err = fabs(est - exact);
        double ratio = (prev_err > 0 && err > 0) ? prev_err / err : 0;
        printf("    n=%-6d  est=%.10f  err=%.3e  ratio=%.2f\n",
               n, est, err, ratio);
        prev_err = err;
        n *= 10;
    }
}

static void test_convergence_all(void)
{
    printf("Test 2: convergence order\n");

    /* Expected ratios (previous_err / current_err) when n -> 10n:
     *   trapezoidal: h^2, so error ~ 1/n^2, ratio ~ 100
     *   simpson:     h^4, so error ~ 1/n^4, ratio ~ 10000
     *   monte_carlo: 1/sqrt(n), so error ~ 1/sqrt(n), ratio ~ sqrt(10) ~ 3.16
     */

    printf("\n  Function: x^2 over [0,1] (exact = 1/3)\n\n");

    test_convergence("trapezoidal", f_square, 0.0, 1.0, 1.0/3.0, 10, 4);
    test_convergence("simpson",     f_square, 0.0, 1.0, 1.0/3.0, 10, 4);
    test_convergence("monte_carlo", f_square, 0.0, 1.0, 1.0/3.0, 1000, 4);

    printf("\n");
}

/* ---------- test: Monte Carlo dimensionality ---------- */

/* Demonstrate the key property of Monte Carlo: error rate is
 * independent of dimension. Compare to a 2D grid method to
 * see the difference. */

static double f_2d(double x, double y) { return x * y; }

/* Trapezoidal-style 2D integration: O(h^2), but with n^2
 * function evaluations for a grid of n x n points. */
static double trapezoidal_2d(double (*f)(double, double),
                             double ax, double bx,
                             double ay, double by, int n)
{
    double hx = (bx - ax) / n;
    double hy = (by - ay) / n;
    double sum = 0.0;

    /* For simplicity use a midpoint rule per cell */
    for (int i = 0; i < n; i++)
    {
        double x = ax + (i + 0.5) * hx;
        for (int j = 0; j < n; j++)
        {
            double y = ay + (j + 0.5) * hy;
            sum += f(x, y);
        }
    }
    return sum * hx * hy;
}

/* 2D Monte Carlo: same number of evaluations, but in a 2D
 * hypercube, error stays O(1/sqrt(n)) in total evaluations. */
static double monte_carlo_2d(double (*f)(double, double),
                             double ax, double bx,
                             double ay, double by, int total_samples)
{
    double sum = 0.0;
    for (int i = 0; i < total_samples; i++)
    {
        double x = ax + (bx - ax) * ((double)rand() / RAND_MAX);
        double y = ay + (by - ay) * ((double)rand() / RAND_MAX);
        sum += f(x, y);
    }
    return (bx - ax) * (by - ay) * sum / total_samples;
}

static void test_dimension(void)
{
    printf("Test 3: dimension independence of Monte Carlo\n");

    /* ∫[0,1]² x*y dx dy = (1/2)*(1/2) = 1/4 */
    double exact = 0.25;
    int grid_n = 100;   /* 10,000 evaluations */
    int mc_samples = 10000;

    double grid_est = trapezoidal_2d(f_2d, 0, 1, 0, 1, grid_n);
    double mc_est = monte_carlo_2d(f_2d, 0, 1, 0, 1, mc_samples);

    printf("  ∫[0,1]² x*y dx dy = 0.25 (exact)\n");
    printf("  grid %dx%d (10,000 evals):      %.8f  err=%.3e\n",
           grid_n, grid_n, grid_est, fabs(grid_est - exact));
    printf("  Monte Carlo (10,000 evals):    %.8f  err=%.3e\n",
           mc_est, fabs(mc_est - exact));

    printf("\n  (In 1D, the grid method would win by a factor of 100.\n");
    printf("   In 2D, they're comparable. In 10D, the grid method\n");
    printf("   needs 10^n points and becomes infeasible; Monte\n");
    printf("   Carlo still needs only 10,000 samples for the same\n");
    printf("   accuracy. That's why Monte Carlo is used for\n");
    printf("   high-dimensional integrals, from physics to finance.)\n\n");
}

/* ---------- test: accuracy table ---------- */

static void test_accuracy_table(void)
{
    printf("Test 4: accuracy comparison across n\n\n");

    printf("  %-12s %-8s %-14s %-14s %-14s\n",
           "method", "n", "x^2 err", "sin err", "gauss err");
    printf("  %-12s %-8s %-14s %-14s %-14s\n",
           "------------", "--------", "--------------",
           "--------------", "--------------");

    int ns[] = { 10, 100, 1000 };

    for (int ni = 0; ni < 3; ni++)
    {
        int n = ns[ni];

        double t1 = trapezoidal(f_square, 0, 1, n);
        double s1 = simpson(f_square, 0, 1, n);
        double m1 = monte_carlo(f_square, 0, 1, n);

        double t2 = trapezoidal(f_sin, 0, M_PI, n);
        double s2 = simpson(f_sin, 0, M_PI, n);
        double m2 = monte_carlo(f_sin, 0, M_PI, n);

        double t3 = trapezoidal(f_gaussian, 0, 1, n);
        double s3 = simpson(f_gaussian, 0, 1, n);
        double m3 = monte_carlo(f_gaussian, 0, 1, n);

        double e1 = fabs(t1 - 1.0/3.0);
        double e2 = fabs(t2 - 2.0);
        double e3 = fabs(t3 - 0.746824132812427);

        printf("  %-12s %-8d %-14.3e %-14.3e %-14.3e\n",
               "trapezoidal", n, e1, e2, e3);

        e1 = fabs(s1 - 1.0/3.0);
        e2 = fabs(s2 - 2.0);
        e3 = fabs(s3 - 0.746824132812427);
        printf("  %-12s %-8d %-14.3e %-14.3e %-14.3e\n",
               "simpson", n, e1, e2, e3);

        e1 = fabs(m1 - 1.0/3.0);
        e2 = fabs(m2 - 2.0);
        e3 = fabs(m3 - 0.746824132812427);
        printf("  %-12s %-8d %-14.3e %-14.3e %-14.3e\n",
               "monte_carlo", n, e1, e2, e3);

        printf("\n");
    }
}

/* ---------- main ---------- */

int main(void)
{
    srand((unsigned)time(NULL));

    printf("====================================\n");
    printf("       NUMERICAL INTEGRATION        \n");
    printf("====================================\n\n");

    test_correctness();
    test_convergence_all();
    test_dimension();
    test_accuracy_table();

    printf("All tests complete.\n");
    return 0;
}
