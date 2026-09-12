# Day 02 — gcd_lcm

Computes the **GCD** and **LCM** of two positive integers.

## Math

- **GCD** (Greatest Common Divisor) — found using the **Euclidean algorithm**:
  gcd(a, b) = gcd(b, a % b), with gcd(a, 0) = a.

- **LCM** (Least Common Multiple) — derived from GCD:
  lcm(a, b) = (a / gcd(a, b)) * b

  The division is done first to avoid integer overflow.

## Usage

    ./gcd_lcm a b

Both `a` and `b` must be positive integers.

## Example

    $ ./gcd_lcm 48 18
    GCD(48, 18) = 6
    LCM(48, 18) = 144

    $ ./gcd_lcm 7 13
    GCD(7, 13) = 1
    LCM(7, 13) = 91

## Build

    gcc -o gcd_lcm gcd_lcm.c

## Skills Practiced

- Recursion (Euclidean algorithm)
- Integer arithmetic and overflow awareness
- `argc` / `argv`
- `atoi` for string-to-int conversion
- Input validation
- Number theory fundamentals
