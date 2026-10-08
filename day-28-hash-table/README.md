# Day 28 — hash_table

A **hash table** implementation in C using separate chaining.

## What Is a Hash Table?

A hash table maps **keys** to **values** in **O(1) average time**.

    "apple"  →  10
    "banana" →  20
    "cherry" →  30

## How It Works

1. Compute a **hash** of the key.
2. Use the hash to find a **bucket** (index in an array).
3. Store the key-value pair in that bucket.
4. Handle **collisions** with a linked list.

## Operations

| Operation | Average | Worst |
|-----------|---------|-------|
| **put** | O(1) | O(n) |
| **get** | O(1) | O(n) |
| **delete** | O(1) | O(n) |

**Worst case happens when all keys collide.**

## Usage

    ./hash_table

## Build

    gcc -Wall -Wextra -std=c99 -o hash_table hash_table.c

## Skills Practiced

- **Structs** — `Entry`, `HashTable`
- **Linked lists** — chaining for collisions
- **Hashing** — djb2
- **Dynamic memory** — `malloc`, `free`
- **Key-value data structures**

## The Hash Function: djb2

```c
unsigned long h = 5381;
while ((c = *key++))
    h = ((h << 5) + h) + c;   /* h * 33 + c */
