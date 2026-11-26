# Day 77 — numerical_integration

Three methods for estimating a definite integral: **trapezoidal
rule**, **Simpson's rule**, and **Monte Carlo integration**.
Each has a different accuracy characteristic, and the
comparison between them is the interesting part.

## The Problem

The definite integral `∫[a,b] f(x) dx` is the area under the
curve from `a` to `b`. For simple `f(x)` — polynomials, trig
functions, exponentials — it has a closed-form expression
you can compute exactly.

For arbitrary `f(x)` — a spline through measured data, a
complex physics formula, a function defined only at discrete
points — no closed form exists. Numerical methods approximate
the integral from finitely many evaluations of `f(x)`.

The three methods in this file cover the three main approaches:

| Method | Type | Error rate | Needs |
|--------|------|-----------|-------|
| Trapezoidal | Deterministic | O(h²) | f(x) on a grid |
| Simpson's | Deterministic | O(h⁴) | f(x) on a grid, even n, smooth f |
| Monte Carlo | Stochastic | O(1/√n) | f(x) at random points |

Where `h = (b-a)/n` is the step size, and `n` is the number
of subintervals (or samples).

## Method 1: Trapezoidal Rule

Divide `[a,b]` into `n` equal-width subintervals of width `h`.
Approximate `f(x)` on each subinterval by a straight line
between the endpoints, giving a trapezoid. Sum the trapezoid
areas.

The formula:
