# Day 77 — numerical_integration

Three methods for estimating a definite integral: **trapezoidal
rule**, **Simpson's rule**, and **Monte Carlo integration**.
Each has a different accuracy characteristic, and the
comparison between them is the interesting part.

## The Problem

The definite integral `∫[a,b] f(x) dx` is the area under the
curve from `a` to `b`. For simple `f(x)` (polynomials, trig
functions, exponentials) it has a closed-form expression
you can compute exactly.

For arbitrary `f(x)` (a spline through measured data, a
complex physics formula, a function defined only at discrete
points) no closed form exists. Numerical methods approximate
the integral from finitely many evaluations of `f(x)`.

The three methods in this file cover the three main approaches:

| Method | Type | Error rate | Needs |
|--------|------|-----------|-------|
| Trapezoidal | Deterministic | O(h²) | f(x) on a grid |
| Simpson's | Deterministic | O(h⁴) | f(x) on a grid, even n, smooth f |
| Monte Carlo | Stochastic | O(1/√n) | f(x) at random points |

Here `h = (b-a)/n` is the step size, and `n` is the number
of subintervals (or samples).

## Method 1: Trapezoidal Rule

Divide `[a,b]` into `n` equal-width subintervals of width `h`.
Approximate `f(x)` on each subinterval by a straight line
between the endpoints, giving a trapezoid. Sum the trapezoid
areas.

The formula:

```text
∫[a,b] f dx ≈ h * [ f(a)/2 + f(a+h) + f(a+2h) + ... + f(b)/2 ]
```

The endpoints `a` and `b` have coefficient 1/2; all
interior points have coefficient 1.

Example: `f(x) = x²` over `[0,1]` with `n=4`:

```text
h      = 0.25
Points : 0, 0.25, 0.5, 0.75, 1
Values : 0, 0.0625, 0.25, 0.5625, 1

Sum    = 0/2 + 0.0625 + 0.25 + 0.5625 + 1/2
       = 1.375
Result = 0.25 * 1.375 = 0.34375

Exact  : 1/3 ≈ 0.333333
Error  : 0.010417
```

The error decreases as `O(h²)`: when `n` grows 10x, the error
drops about 100x.

## Method 2: Simpson's Rule

Divide `[a,b]` into `n` subintervals (`n` even). Instead of
approximating each subinterval by a straight line, group pairs
of subintervals and fit a **parabola** through the three points.
Sum the areas.

The formula:

```text
∫[a,b] f dx ≈ h/3 * [ f(a) + 4f(a+h) + 2f(a+2h) + 4f(a+3h) + ... + f(b) ]
```

Coefficients alternate 4 and 2 for interior points, with 1
for the endpoints. The `h/3` factor comes from the
antiderivative of the quadratic fit.

Same example: `f(x) = x²` over `[0,1]` with `n=4`:

```text
h      = 0.25
Values : f(0)=0, f(0.25)=0.0625, f(0.5)=0.25, f(0.75)=0.5625, f(1)=1

Sum    = 0 + 4(0.0625) + 2(0.25) + 4(0.5625) + 1
       = 0 + 0.25 + 0.5 + 2.25 + 1 = 4.0
Result = 0.25/3 * 4.0 = 0.333333...

Exact  : 1/3 ≈ 0.333333
Error  : ~1e-17 (machine precision)
```

Simpson's is exact for polynomials up to degree 3. That's
why for `x²`, a quadratic, it's already at machine precision
with just 4 subintervals. In general, the error is `O(h⁴)`:
when `n` grows 10x, error drops about 10,000x.

This uses nearly the same number of function evaluations as the
trapezoidal rule, but gives a vastly better estimate. The
tradeoff: Simpson's requires `n` even and assumes `f` is smooth
(for rough or discontinuous `f`, it can be worse than trapezoidal).

## Method 3: Monte Carlo Integration

Don't use a grid at all. Instead:

1. Sample `n` points `x_i` uniformly from `[a,b]`.
2. Compute the average value of `f(x_i)`.
3. Multiply by `(b-a)`.

```text
∫[a,b] f dx ≈ (b-a) * (1/n) * Σ f(x_i)
```

This is justified by the **law of large numbers**: the sample
average converges to the true average (the integral divided by
`b-a`) as `n → ∞`.

The error is `O(1/√n)`: when `n` grows 10x, error drops about
√10 ≈ 3.16x.

For 1D smooth functions, Monte Carlo is much worse than
Simpson's. But it has one massive advantage: **the error rate
is independent of dimension**. In 10-dimensional integrals,
Simpson's requires `n^10` grid points for `n` per dimension,
which is infeasible. Monte Carlo still needs only `n` samples total.
That's why Monte Carlo is the standard method for:

- Physics simulations (particle interactions, quantum mechanics)
- Financial modeling (option pricing, risk analysis)
- Rendering (ray tracing with random ray samples)
- Bayesian inference (MCMC sampling)
- Machine learning (stochastic gradient descent uses the same idea)

## The Convergence Comparison

The demo in `test_convergence_all` prints the error at each step
as `n` grows 10x. Sample output for `f(x) = x²`:

```text
trapezoidal convergence:
  n=10     est=0.3350000000  err=1.667e-03
  n=100    est=0.3333500000  err=1.667e-05  ratio=100.00
  n=1000   est=0.3333335000  err=1.667e-07  ratio=100.00
  n=10000  est=0.3333333350  err=1.667e-09  ratio=100.00

simpson convergence:
  n=10     est=0.3333333333  err=~1e-17 (machine precision)
  n=100    est=0.3333333333  err=~1e-17
  ...

monte_carlo convergence:
  n=1000    est=0.3301830000  err=3.15e-03
  n=10000   est=0.3332310000  err=1.02e-04  ratio=~3
  n=100000  est=0.3334320000  err=9.87e-05  ratio=~1-3
  (Monte Carlo error varies; single-run ratios are noisy)
```

The key patterns:

- **Trapezoidal**: ratio ~100 per 10x n growth (O(h²))
- **Simpson's**: exact for `x²` (machine precision from the start)
- **Monte Carlo**: ratio ~3 per 10x n growth (O(1/√n)), with randomness

The ratios confirm the theoretical error orders. That's the
payoff of running the demo: theory predicts, code confirms.

## Why Three Methods

For 1D smooth functions, **always use Simpson's or better**
(adaptive quadrature). Trapezoidal is a teaching tool, and
Monte Carlo is inferior for this case.

But the interesting part is the "not always" cases:

- **Rough or discontinuous `f`**: Simpson's assumes smoothness;
  trapezoidal is more robust. For `f(x) = |x|` or a step
  function, Simpson's can be worse than trapezoidal.
- **Measured data**: if `f` is a table of experimental values,
  you can only use methods that don't assume a derivative.
  Trapezoidal is safe; Simpson's might oscillate.
- **High dimension**: Monte Carlo wins over any grid method
  beyond ~3-4 dimensions.
- **Noisy `f`**: if each evaluation of `f` has noise
  (Monte Carlo simulation of a physical system),
  deterministic methods amplify noise; Monte Carlo is
  more forgiving.

The rule of thumb: **Simpson's for smooth 1D, adaptive methods for
general 1D, Monte Carlo for 3+ dimensions.** This file
shows all three so the tradeoffs are concrete.

## Usage

```sh
./numerical_integration
```

Runs four demos:

1. Correctness: each method vs. known answer, n=10,000
2. Convergence: error as n grows 10x each step
3. Dimension: grid vs. Monte Carlo for a 2D integral
4. Accuracy table: all combinations of method/function/n

## Build

```sh
gcc -Wall -Wextra -std=c99 -o numerical_integration numerical_integration.c -lm
```

The `-lm` links the math library for `sin`, `exp`, and `erf`.
Note that `M_PI` is not defined by strict `-std=c99`; either define
it yourself or compile with `-std=gnu99`.

## Skills Practiced

- **Function pointers as data**: `double (*f)(double)` is
  passed to each integrator
- **Numerical methods with tunable precision**: `n` controls
  accuracy, `h = (b-a)/n` is the step
- **Error analysis**: comparing observed error to `O(h²)`,
  `O(h⁴)`, `O(1/√n)`
- **Convergence verification**: measuring the ratio
  (prev_err / current_err) when n grows 10x
- **Random sampling**: Monte Carlo's core primitive
- **Dimensionality tradeoff**: why Monte Carlo wins
  in high dimensions
- **Comparing methods**: the right tool for the right
  problem, not "always use Simpson's"

## Test Cases

All three functions have known exact integrals:

| Function | Interval | Exact integral |
|----------|----------|----------------|
| `x²` | `[0, 1]` | `1/3 ≈ 0.3333...` |
| `sin(x)` | `[0, π]` | `2.0` |
| `exp(-x²)` | `[0, 1]` | `0.7468...` (via `erf`) |

The Gaussian integral has no elementary antiderivative, so
its value is computed independently (using `erf`, or
precomputed to high precision). Comparing to it verifies
the numerical methods work on a non-trivial function.

## Notes

- **`rand()` is not cryptographic.** For Monte Carlo, it's
  fine: we just need a roughly uniform distribution. For adversarial
  or security-sensitive simulations, use a proper PRNG.
- **The Monte Carlo error is probabilistic.** Different runs
  produce different estimates. The `test_convergence_all`
  numbers will vary run to run. The average convergence
  rate is `O(1/√n)`, but individual runs are noisy.
- **Simpson's needs even `n`.** The code adjusts `n` to be
  even if it isn't; that's a small simplification. A real
  implementation would either require even `n` and report an error
  otherwise, or use a composite rule that handles odd `n`
  gracefully.
- **The composite Simpson's formula** uses coefficients
  `[1, 4, 2, 4, 2, ..., 4, 1]`. The `4` coefficients are
  for odd-indexed points (i=1, 3, 5, ...), the `2` for
  even-indexed interior points (i=2, 4, 6, ...). This is
  because the parabolas share an endpoint at every second point.
- **The "exact" Gaussian integral is computed offline.**
  `exp(-x²)` has no elementary antiderivative, so the value
  0.746824132812427 was computed separately as
  `erf(1) * sqrt(π) / 2`. In a real system, you'd call `erf`
  at runtime.
- **Adaptive quadrature** (which this file doesn't do) is
  the standard for real numerical integration. The idea:
  recursively subdivide intervals where the error is large,
  using two estimates (e.g., Simpson's with n and 2n) to
  estimate the error per interval. Used by `scipy.integrate.quad`
  and most numerical libraries.
- **The dimensionality argument**: grid methods require
  `O(n^d)` evaluations in `d` dimensions, while Monte Carlo
  requires `O(n)` regardless of `d`. For d=10, n=100,
  that's 10^20 vs. 100. This is the **curse of
  dimensionality**, and it's why Monte Carlo is the only
  practical choice for high-dimensional integrals.
- **Monte Carlo variance reduction**: plain MC has error
  `σ/√n`. Techniques like **importance sampling**,
  **stratified sampling**, and **antithetic variates**
  reduce `σ`, giving better accuracy for the same `n`.
  Out of scope for this file, but worth knowing.

## What You Can Extend

- **Adaptive Simpson's**: recursive subdivision until
  estimated error < tolerance
- **Romberg integration**: Richardson extrapolation over
  trapezoidal results; very fast convergence for smooth `f`
- **Gauss-Legendre quadrature**: optimal node placement
  for polynomial accuracy of degree `2n-1` with `n` points
- **Multi-dimensional Monte Carlo**: integrate over a
  hypercube in `d` dimensions
- **Importance sampling**: Monte Carlo with a biased
  distribution; reduces variance
- **Compare with a known library**: call `scipy.integrate.quad`
  from Python and compare
- **Plotting**: visualize the trapezoids, parabolas, and
  random points
- **Numerical derivative**: the dual problem, estimating
  `f'(x)` from samples of `f`

## Why This Matters for Systems Programming

- **Physical simulations** use numerical integration
  everywhere: orbital mechanics, fluid dynamics, particle
  physics. The methods here are the same ones used inside
  those engines (at higher orders).
- **Machine learning**: stochastic gradient descent is
  closely related to Monte Carlo estimation of the loss gradient.
  Same error rate `O(1/√n)`, same "noisy but unbiased"
  property.
- **Rendering**: ray tracers integrate the light equation
  via Monte Carlo. The "noise" in an un-denoised render
  is the `1/√n` error showing up pixel by pixel.
- **Financial modeling**: option pricing integrates over
  uncertain future states using Monte Carlo. Large amounts
  of money are priced with the same "average f(x) over
  random samples" technique.
- **The three methods here are three mental models.**
  Grid methods (deterministic, low-dim), Monte Carlo
  (stochastic, any-dim), and adaptive (recursive, robust).
  Most integration libraries pick one of these as a
  base and refine it.

## Why This Matters for Cybersecurity

- **Cryptographic randomness**: the same "sample random
  points" idea, but with cryptographic PRNGs instead of
  `rand()`. The math of statistical sampling is the same.
- **Side-channel analysis**: measuring power consumption
  or timing over many runs is essentially averaging a
  signal buried in noise. The `O(1/√n)` rate is why
  side-channel attacks often need thousands of traces.
- **Fuzzing**: coverage-guided fuzzing uses random
  sampling (with feedback) to explore input space.
  Not integration, but the same statistical thinking.
- **The "no closed form" experience**: many security
  models (e.g., probability of attacker success, expected
  loss) have no closed form. Estimating them via Monte
  Carlo is a standard technique.
- **Adaptive methods as an analogy**: adaptive quadrature
  is "spend more effort where the error is high." Adaptive
  fuzzers and adaptive rate limiters follow a similar
  pattern.

## Why This Matters as a Portfolio Piece

- **"I implemented three numerical integration methods"** is
  a concrete, verifiable achievement. Each method is a
  standard from numerical analysis textbooks, and comparing
  them empirically shows you understand the tradeoffs.
- **The convergence measurements are the interesting part.**
  Anyone can implement trapezoidal; empirically confirming
  the `O(h²)` and `O(h⁴)` error rates is more.
- **The dimension demo** connects numerical methods to
  real-world applicability (why Monte Carlo is used for
  finance, physics, etc.).
- **The code is small** (~400 lines including all the test
  output), but the concepts (function pointers, numerical
  stability, error analysis) are significant.
- **It pairs with Day 53 (`fft`).** Both are numerical
  methods. FFT handles frequency-domain transforms;
  integration handles area under a curve. Together they're
  a decent "numerical computing" mini-portfolio.

## The Numerical Methods Arc

| Day | Project | Domain |
|-----|---------|--------|
| 29 | `complex_numbers` | Complex arithmetic |
| 38 | `polynomial` | Polynomial arithmetic |
| 53 | `fft` | Frequency-domain transform |
| 63 | `fft_convolution` | Fast polynomial multiply |
| 73 | `hyperloglog` | Probabilistic counting |
| **77** | **`numerical_integration`** | **Numerical quadrature** |

Six Math entries, each with a different flavor of numerical
computation. Together they cover complex arithmetic,
polynomials, transforms, probability, and integration:
the numerical foundations for signal processing, machine
learning, and scientific computing.

## Distribution Note

After Day 77: **Math 15**, Game 14, Cyber 14, Kernel 16,
Pure C 18. Math is now slightly ahead of Game and Cyber,
and Pure C remains the overhang.

The schedule has been balancing Math, Game, and Cyber while
letting Kernel and Pure C coast. The distribution should
converge to around 15-17 across all themes by Day 80,
which is within a reasonable range for a portfolio.
