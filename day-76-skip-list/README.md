# Day 76 — skip_list

A **skip list**: an ordered linked list with a hierarchy of
"express lanes" that give O(log n) search, insert, and delete
without any of the complexity of balanced trees.

## What a Skip List Is

A skip list is a linked list augmented with multiple levels
of forward pointers. Every node appears at level 0. Some
nodes also appear at higher levels, chosen randomly with
probability p (typically 0.5). Higher levels act as "express
lanes" — they let you skip over many nodes at once.
Level 2: head ----------------------------------> 50 -> NULL
Level 1: head --------> 20 -----------> 40 ----> 50 -> NULL
Level 0: head -> 10 -> 20 -> 30 -> 40 -> 50 -> 60 -> NULL

To search for a value:

1. Start at the top level of the head node.
2. Advance right as long as the next value is smaller than
   the target.
3. When the next value would be larger (or we hit NULL),
   drop down one level.
4. Repeat from step 2.
5. At level 0, the node to the right is either the target
   or larger (meaning not found).

The search path snakes down and right through the levels,
visiting at most O(log n) nodes on average.

## Why Not Just a Linked List?

A plain linked list has O(n) search — you have to walk
every node. The skip list's extra levels let you skip
large ranges, reducing the search to O(log n) expected.

## Why Not a Balanced BST?

An AVL tree or red-black tree gives the same O(log n) — but
**deterministically**, and with more code. Every insert in a
balanced tree requires rotations and invariant maintenance.
The skip list has no rotations at all; the "balance" comes
from randomness.

| | Skip list | Balanced BST |
|---|---|---|
| Search (expected) | O(log n) | O(log n) |
| Search (worst) | O(n) w.p. 2^-Ω(log n) | O(log n) |
| Insert complexity | Simple (splice) | Complex (rotate, rebalance) |
| Delete complexity | Simple (unlink) | Complex (rotate, rebalance) |
| Code size | ~50 lines | ~200+ lines |
| Concurrency | Naturally lock-free | Hard to lock-free |

The probabilistic guarantee is strong enough for
real-world use: the probability of a skip list with n
nodes having search time worse than O(log n) is
astronomically small (roughly 2^-log²(n)). For n = 10^9,
that's about 10^-24 — more likely to win the lottery
twice on the same day.

## The Levels

Each new node is assigned a random level:

```c
int random_level(void) {
    int level = 1;
    while (rand() % 2 == 0 && level < MAX_LEVEL)
        level++;
    return level;
}
