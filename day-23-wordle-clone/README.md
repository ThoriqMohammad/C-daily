# Day 23 — wordle_clone

A terminal-based Wordle clone in C.

## How It Works

- The computer picks a random 5-letter word.
- The player has 6 attempts to guess it.
- After each guess, each letter is marked:

| Marker | Meaning |
|--------|---------|
| **G** | Green — correct letter, correct position |
| **Y** | Yellow — correct letter, wrong position |
| **.** | Gray — letter not in the word |

## Usage

    ./wordle_clone

## Example Session

    ====================================
               WORDLE CLONE
    ====================================

    Guess the 5-letter word.
    You have 6 attempts.

    Markers:
      G = correct letter, correct position
      Y = correct letter, wrong position
      . = letter not in the word

    Attempt 1/6
    Enter your guess: crane
      c [.]  r [Y]  a [.]  n [.]  e [G]

    Attempt 2/6
    Enter your guess: brave
      b [.]  r [Y]  a [.]  v [.]  e [G]

    ...

    🎉 You won in 4 attempt(s)!

## Build

    gcc -Wall -Wextra -std=c99 -o wordle_clone wordle_clone.c

## Skills Practiced

- **Strings** — comparing, modifying
- **Arrays** — frequency counting
- **Loops** — multiple passes
- **Conditionals** — marking logic
- **Randomness** — picking a word
- **Game state** — attempts, win condition
- **Input validation** — length, letters

## The Tricky Part: Duplicate Letters

If the secret is `"apple"` and the guess is `"eerie"`, how many `e`s
should be marked?

- Secret has **one** `e`.
- Guess has **two** `e`s.

**Only one `e` should be marked.**

The algorithm:

| Pass | What It Does |
|------|--------------|
| **1** | Mark greens. Count remaining letters in the secret. |
| **2** | Mark yellows only if the count is greater than 0. |

This prevents over-marking duplicates.

## Why Wordle Is Interesting

| Aspect | Why It Matters |
|--------|----------------|
| **Duplicate handling** | Real algorithmic challenge |
| **Feedback loop** | Player learns from each guess |
| **Limited attempts** | Strategy matters |
| **Simple rules** | Easy to understand |

## Notes

- The word list is built into the program.
- Guesses are converted to lowercase.
- Non-letter input is rejected.
- Duplicate letters are handled correctly.

## What You Can Extend

- Larger word list
- Colored terminal output (ANSI codes)
- Difficulty levels (more attempts for beginners)
- Hint system
- Score tracking across multiple games
