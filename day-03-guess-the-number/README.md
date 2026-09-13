# Day 03 — guess_the_number

A simple number-guessing game written in C.

## How It Works

- The program picks a random number between **1 and 100**.
- The player has **7 attempts** to guess it.
- After each guess, the program says:
  - `Too low!`
  - `Too high!`
  - `Correct!`
- If the player runs out of attempts, the secret number is revealed.

## Usage

    ./guess_the_number

## Example Session

    ====================================
       GUESS THE NUMBER (1 to 100)
       You have 7 attempts.
    ====================================

    Attempt 1/7 — Enter your guess: 50
    Too low!

    Attempt 2/7 — Enter your guess: 75
    Too high!

    Attempt 3/7 — Enter your guess: 62
    Too low!

    Attempt 4/7 — Enter your guess: 68
    🎉 Correct! You guessed it in 4 attempt(s).

    Thanks for playing!

## Build

    gcc -o guess_the_number guess_the_number.c

## Skills Practiced

- `rand()` and `srand()` for random numbers
- Seeding with `time(NULL)` for different results each run
- Loops and conditionals
- `scanf()` for user input
- Input validation (non-numeric input, out-of-range values)
- Game state tracking (`won`, `attempts`)
- Clear, user-friendly output

## Notes

- The number range is defined by `MIN_NUMBER` and `MAX_NUMBER`.
- The number of attempts is defined by `MAX_ATTEMPTS`.
- Change these macros to adjust difficulty.
