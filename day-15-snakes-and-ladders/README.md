# Day 15 — snakes_and_ladders

A simple **simulation** of the classic Snakes and Ladders board game.

## How It Works

- The board is 10×10 (squares 1–100).
- Two players roll a die and move forward.
- Ladders move you **up**; snakes move you **down**.
- The first player to reach square 100 wins.

## Board Layout

| Feature | Count |
|---------|-------|
| Ladders | 9 |
| Snakes | 10 |

The program uses two pairs of arrays to represent ladders and snakes:

    ladder_start[] → ladder_end[]
    snake_start[]  → snake_end[]

## Usage

    ./snakes_and_ladders

## Example Output

    ====================================
           SNAKES AND LADDERS
         First to square 100 wins.
    ====================================

    Turn 1, Player 1: rolled 4, moved to 4
    Turn 1, Player 2: rolled 6, moved to 6
    Turn 2, Player 1: rolled 3, moved to 14
    ...

    Player 1 wins in 42 turns!
    Final positions: Player 1 = 100, Player 2 = 87

## Build

    gcc -Wall -Wextra -std=c99 -o snakes_and_ladders snakes_and_ladders.c

## Skills Practiced

- **Arrays** — the board and the snakes/ladders tables
- **Randomness** — dice rolls with `rand()` and `srand()`
- **Simulation** — modeling a game step by step
- **Loops and conditionals** — game loop
- **Expected value** — how many turns to win?
- **`const` arrays** — read-only data

## Why Simulation Matters

Simulations are used everywhere:

| Domain | Example |
|--------|---------|
| Physics | Particle interactions |
| Finance | Monte Carlo pricing |
| Biology | Population models |
| Games | AI training |
| Security | Attack/defense modeling |

Snakes and Ladders is a **simple but complete** simulation.

## The Magic Numbers

The snake and ladder positions are the standard board layout. If
you want a different board, just modify the arrays.

## Notes

- Rolling a 6 doesn't grant an extra turn (unlike some rules).
- Overshooting 100 means you stay put (a common rule).
- The game is capped at `MAX_TURNS` to avoid infinite loops.

## What You Can Extend

- Add a third player.
- Show the board visually.
- Track how often each square is landed on.
- Run 1000 games and report the average number of turns.
