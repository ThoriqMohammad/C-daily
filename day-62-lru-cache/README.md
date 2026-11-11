# Day 62 — lru_cache

An **LRU (Least Recently Used) cache** with O(1) get and put,
combining a hash table and a doubly-linked list.

## What Is an LRU Cache?

An LRU cache stores a fixed number of key-value pairs. When it
fills up and a new item arrives, it evicts the item that was
accessed **furthest in the past** — the "least recently used."

The policy is simple: on every `get` or `put`, the accessed
key becomes the most recently used. When eviction is needed,
the least recently used key goes.

This is the standard replacement policy when:

- You have a fast storage layer (cache) and a slow one (disk, database, network)
- Access patterns are **skewed** — some keys are hot, some are cold
- You want to keep the hot ones and discard the cold ones

That's why every caching system in existence uses some variant
of LRU: Linux page cache, CPU caches, Redis, browsers, DNS
resolvers, database buffer pools, HTTP CDNs.

## The O(1) Challenge

LRU needs three operations, all O(1):

1. **Lookup** key → value
2. **Evict** the least recently used key
3. **Update** recency on access

A hash table gives you (1) in O(1), but (2) requires knowing
which key is LRU, and finding it in a hash table takes O(n).

A linked list gives you (2) in O(1) if the LRU is at the tail,
but (1) requires scanning the whole list, O(n).

**Neither alone is enough.** You need both, working together.

## The Dual-Structure Design

    ┌───────────────────────────────────────────┐
    │           HASH TABLE (key → node)         │
    │                                           │
    │  "a" ──▶ ┐                                │
    │  "b" ──▶ │                                │
    │  "c" ──▶ │                                │
    └──────────┼────────────────────────────────┘
               │
    ┌──────────▼────────────────────────────────┐
    │   DOUBLY-LINKED LIST (recency order)      │
    │                                           │
    │  ┌────┐   ┌────┐   ┌────┐   ┌────┐        │
    │  │head│◀─▶│ a  │◀─▶│ c  │◀─▶│ b  │◀─▶tail │
    │  └────┘   └────┘   └────┘   └────┘        │
    │   MRU                              LRU    │
    └───────────────────────────────────────────┘

The hash table stores **pointers to nodes** in the linked list.
The linked list stores the nodes themselves, in recency order.

- **Lookup** (`get`): hash table gives O(1) node access. Then
  we unlink the node and relink it at the head — O(1) because
  we already have the node's `prev` and `next` pointers.
- **Insert** (`put`): create a node, link at head, insert into
  hash table. O(1).
- **Evict**: the LRU node is at the tail; unlink it, remove from
  hash table, free. O(1).

The trick that makes recency update O(1) is that the hash table
stores **node pointers**, not just values. We already have a
handle on the node in the list, so moving it is just pointer
surgery.

## Sentinel Nodes

The doubly-linked list has two **sentinel nodes** at the ends:

    head (sentinel) ◀──▶ [a] ◀──▶ [b] ◀──▶ [c] ◀──▶ tail (sentinel)

`head.next` is the MRU node; `tail.prev` is the LRU node. The
sentinels are not part of the data — they exist to eliminate
edge cases in link/unlink:

- Empty list: `head.next == &tail` — no special-case check
  needed in `list_unlink`
- Insert at head: `n->next = head.next` — works whether the
  list was empty or not

Without sentinels, every operation would need "is this the
first node?" or "is this the last node?" checks. With them,
all operations are uniform.

## Usage

    ./lru_cache

Runs six self-tests and prints `ok` or `FAIL` for each assertion.

## Build

    gcc -Wall -Wextra -std=c99 -o lru_cache lru_cache.c

## The Core Operations, in Code

**Move a node to MRU:**

```c
n->prev->next = n->next;
n->next->prev = n->prev;
n->next = head.next;
n->prev = &head;
head.next->prev = n;
head.next = n;
