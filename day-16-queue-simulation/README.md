# Day 16 — queue_simulation

A queue implementation using a **circular buffer**.

## What Is a Queue?

A queue is a **FIFO** (First In, First Out) data structure.
Elements are added at the **rear** and removed from the **front**.

Think of a line at a store:

    Front                          Rear
    [ 10 | 20 | 30 | 40 | 50 ]
    ↑                          ↑
    dequeue here               enqueue here

## Operations

| Operation | What It Does |
|-----------|--------------|
| `enqueue` | Add an element at the rear |
| `dequeue` | Remove an element from the front |
| `peek` | Look at the front without removing |
| `is_empty` | Check if the queue has no elements |
| `is_full` | Check if the queue is at capacity |

## Usage

    ./queue_simulation

## Build

    gcc -Wall -Wextra -std=c99 -o queue_simulation queue_simulation.c

## Skills Practiced

- **Arrays and indices** — the queue is an array
- **Circular buffer** — wrapping around with `%`
- **Modulo arithmetic** — `(index + 1) % SIZE`
- **FIFO data structure** — enqueue at rear, dequeue at front
- **State tracking** — `front`, `rear`, and `count`
- **Boolean return values** — `enqueue` and `dequeue` return success

## How the Circular Buffer Works

The queue uses a fixed-size array and two indices:

    front → where to dequeue
    rear  → where to enqueue

When an index reaches the end of the array, it wraps around:

    rear = (rear + 1) % QUEUE_SIZE

This reuses space that was previously vacated by dequeues.

## Why Three Fields?

| Field | Purpose |
|-------|---------|
| `front` | Where the next dequeue will read |
| `rear` | Where the next enqueue will write |
| `count` | How many elements are currently in the queue |

Using `count` makes `is_empty` and `is_full` trivial.

## Real-World Uses

| Domain | Example |
|--------|---------|
| Operating systems | Process scheduling |
| Networking | Packet queues |
| Printers | Print job queue |
| Web servers | Request queue |
| Games | Event queue |
| Simulations | Customer queues |

## Notes

- The queue size is fixed at `QUEUE_SIZE`.
- `enqueue` and `dequeue` return `bool` to signal success.
- `peek` returns the front without removing it.
- The program tests wrap-around by enqueuing after dequeuing.

## What You Can Extend

- Make the queue dynamically sized.
- Add a priority queue.
- Simulate a real-world queue (e.g., customers at a bank).
- Track statistics (average wait time, queue length).
