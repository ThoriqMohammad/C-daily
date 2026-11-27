# Day 78 — nim_solver

A solver for **Nim**, the canonical impartial game. Nim's
solution — the XOR of pile sizes — is a closed-form game-theory
result that doesn't require any search, and it generalizes to
*every* impartial game via the Sprague-Grundy theorem.

## What Nim Is

Nim is a game of `N` piles of stones. On each turn, a player:

1. Picks one pile.
2. Removes any positive number of stones from it (up to its size).

The last player to move (i.e., the one who takes the final stone)
wins. That's called the **normal play convention**.

Example position: `[3, 4, 5]` (three piles, sizes 3, 4, 5).
The first player has many options:

- Take 2 from pile 0 → `[1, 4, 5]`
- Take 1 from pile 2 → `[3, 4, 4]`
- Take 3 from pile 1 → `[3, 1, 5]`
- ...and so on.

Which move wins? For small positions, you can search. For
large ones, you need theory.

## Bouton's Theorem (1901)

Charles Bouton proved that the first player wins if and only
if the XOR of all pile sizes is nonzero.
