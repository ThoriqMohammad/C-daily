# Day 10 — hangman

A simple Hangman game in C.

## How It Works

- The computer picks a random word from a built-in list.
- The player guesses one letter at a time.
- Correct letters are revealed; wrong letters count toward the hangman.
- The player has **6 wrong guesses** before losing.

## Usage

    ./hangman

## Example Session

    ====================================
                HANGMAN
       You have 6 wrong guesses.
    ====================================

      +---+
      |   |
          |
          |
          |
          |
    =========

    Word: _ _ _ _ _ _ _ _ _ _
    Wrong guesses: 0/6
    Guess a letter: p
    ...

## Build

    gcc -o hangman hangman.c

## Skills Practiced

- Strings and character arrays
- Loops and conditionals
- Randomness (`rand`, `srand`)
- Game state tracking
- Input validation
- ASCII art

## Notes

- The word list is defined at the top of the file.
- `MAX_WRONG` controls the difficulty.
- Guesses are case-insensitive.
- Duplicate guesses are rejected.
