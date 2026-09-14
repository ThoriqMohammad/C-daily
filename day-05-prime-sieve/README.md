# Day 05 — prime_sieve

Finds all prime numbers up to a given limit using the
**Sieve of Eratosthenes**.

## What Is the Sieve of Eratosthenes?

An ancient algorithm (over 2,000 years old) for finding all primes
up to a given limit. It works by:

1. Assuming every number is prime.
2. Starting from 2, marking all multiples of each prime as composite.
3. The numbers left unmarked are prime.

## Usage

    ./prime_sieve n

`n` must be at least 2.

## Example

    $ ./prime_sieve 50
    Primes up to 50:
    2 3 5 7 11 13 17 19 23 29 31 37 41 43 47
    Total: 15 primes

    $ ./prime_sieve 10
    Primes up to 10:
    2 3 5 7
    Total: 4 primes

## Build

    gcc -o prime_sieve prime_sieve.c

## Skills Practiced

- Arrays
- Nested loops
- Number theory (primes, multiples)
- `argc` / `argv`
- `atoi` for string-to-int conversion
- Dynamic memory allocation (`malloc`, `free`)
- Error handling
- Algorithmic thinking (sieve vs. trial division)

## Why the Sieve?

The naive way to test if a number is prime is trial division (O(√n)
per number). The sieve finds **all** primes up to n in O(n log log n)
time—much faster for large n.

## Notes

- The array is allocated with `malloc` to handle large n safely.
- Memory is freed with `free` before exit.
- The sieve starts marking at `p * p` because smaller multiples
  have already been marked by smaller primes.
