# Day 17 — password_strength

Analyzes the strength of a password.

## What It Measures

| Metric | What It Means |
|--------|---------------|
| **Length** | Longer is stronger |
| **Character classes** | Lowercase, uppercase, digits, symbols |
| **Charset size** | Total possible characters |
| **Entropy (bits)** | log₂(charset_size) × length |
| **Score (0–100)** | Weighted combination |
| **Brute-force time** | Estimated time to crack |

## Usage

    ./password_strength

## Example

    $ ./password_strength
    Enter a password to analyze: Tr0ub4dour&3

    --- Analysis ---
    Length:           12
    Character classes: 4 (lower/upper/digit/symbol)
    Charset size:      95
    Entropy:           78.7 bits
    Score:             88/100
    Rating:            Very Strong
    Brute-force time:  381000000 years

## Build

    gcc -Wall -Wextra -std=c99 -o password_strength password_strength.c -lm

**Note:** `-lm` is needed for `log` and `pow`.

## Skills Practiced

- **String handling** — length, character access
- **Character classification** — `islower`, `isupper`, `isdigit`
- **Math** — `log2`, `pow`
- **Security concepts** — entropy, brute-force estimation
- **Scoring** — weighting multiple factors

## Entropy

Entropy measures the "unpredictability" of a password:

    entropy = length × log₂(charset_size)

|   Password     | Length | Charset |  Entropy  |
|----------------|--------|---------|-----------|
| `password`     |   8    |    26   | 37.6 bits |
| `Password1`    |   9    |    62   | 53.6 bits |
| `Tr0ub4dour&3` |   12   |    95   | 78.7 bits |

**More entropy = more combinations = harder to crack.**

## Brute-Force Time

Assuming 10 billion guesses per second:

| Entropy | Approximate Time |
|---------|------------------|
| 30 bits | Instant |
| 50 bits | Minutes |
| 70 bits | Years |
| 100 bits | Heat death of the universe |

## The Common-Password Penalty

Passwords containing common patterns lose 40 points.
The program checks against a small list: `password`, `123456`,
`qwerty`, etc.

## Notes

- This is an **educational** tool, not a security guarantee.
- Real password strength depends on the attack model.
- Never use the same password across sites.
- Use a password manager.
