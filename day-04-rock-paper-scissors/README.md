# Day 04 — rock_paper_scissors

A simple Rock Paper Scissors game against the computer.

## Rules

- **Rock** beats **Scissors**
- **Scissors** beats **Paper**
- **Paper** beats **Rock**
- Same choice = draw

## How It Works

- The player chooses `R`, `P`, or `S`.
- The computer chooses randomly.
- The round result is displayed.
- The score is tracked across rounds.
- Type `Q` to quit and see the final score.

## Usage

    ./rock_paper_scissors

## Example Session

    ====================================
           ROCK PAPER SCISSORS
       Enter R, P, or S. Q to quit.
    ====================================

    Your choice: R
    You chose Rock. Computer chose Scissors.
    You win this round!
    Score — You: 1 | Computer: 0 | Draws: 0

    Your choice: P
    You chose Paper. Computer chose Scissors.
    Computer wins this round.
    Score — You: 1 | Computer: 1 | Draws: 0

    Your choice: Q

    ====================================
    Final Score
    You:      1
    Computer: 1
    Draws:    0

    The match is a draw. 🤝
    Thanks for playing!

## Build

    gcc -o rock_paper_scissors rock_paper_scissors.c

## Skills Practiced

- `rand()` and `srand()` for random choices
- Character input with `scanf(" %c", ...)`
- Input validation with `toupper()`
- `switch` statements for mapping choices
- Game loop with score tracking
- `const char *` return types
- Helper functions with clear responsibilities

## Notes

- The computer's choice is truly random.
- The `judge()` function returns `1`, `-1`, or `0` for win/loss/draw.
- The `choice_name()` function converts `'R'` → `"Rock"`, etc.
