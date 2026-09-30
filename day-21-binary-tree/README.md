# Day 21 — binary_tree

A **binary search tree (BST)** implementation in C.

## What Is a BST?

A binary tree where:
- Every node's **left** subtree has smaller values.
- Every node's **right** subtree has larger values.

       50
      /  \
    30    70
   / \   / \
  20 40 60  80

## Operations

| Operation | What It Does |
|-----------|--------------|
| `insert` | Add a value |
| `search` | Find a value |
| `inorder` | Print in sorted order |
| `preorder` | Print root first |
| `postorder` | Print root last |
| `minimum` | Smallest value |
| `maximum` | Largest value |
| `height` | Longest path from root |
| `count_nodes` | Total nodes |
| `free_tree` | Free all memory |

## Usage

    ./binary_tree

## Build

    gcc -Wall -Wextra -std=c99 -o binary_tree binary_tree.c

## Example Output

    ====================================
            BINARY SEARCH TREE
    ====================================

    Inserting: 50 30 70 20 40 60 80 10 25 35 45 55 65 75 85

    In-order   (sorted): 10 20 25 30 35 40 45 50 55 60 65 70 75 80 85
    Pre-order  (root first): 50 30 20 10 25 40 35 45 70 60 55 65 80 75 85
    Post-order (root last):  10 25 20 35 45 40 30 55 65 60 75 85 80 70 50

    Minimum:  10
    Maximum:  85
    Height:   3
    Count:    15

    Searching:
       40: found
      100: not found
       10: found
      999: not found

## Skills Practiced

- **Structs** — the `Node` type
- **Recursion** — every tree operation
- **Pointers** — linking nodes
- **Dynamic memory** — `malloc` / `free`
- **Tree algorithms** — traversal, search

## The Three Traversals

| Traversal | Order | Use |
|-----------|-------|-----|
| **In-order** | Left, root, right | Sorted output |
| **Pre-order** | Root, left, right | Copy a tree |
| **Post-order** | Left, right, root | Free a tree |

**In-order is the magic one—it prints sorted data.**

## Why BSTs Matter

| Domain | Use |
|--------|-----|
| Databases | Indexing |
| Compilers | Symbol tables |
| File systems | Directory trees |
| Networking | Routing tables |
| Security | Certificate chains |

## Complexity

| Operation | Average | Worst |
|-----------|---------|-------|
| Insert | O(log n) | O(n) |
| Search | O(log n) | O(n) |
| Delete | O(log n) | O(n) |

**Worst case happens with unbalanced trees.**

## Notes

- Duplicates are ignored.
- Height of an empty tree is `-1`.
- Height of a single node is `0`.
- `free_tree` uses post-order—the correct order for cleanup.
