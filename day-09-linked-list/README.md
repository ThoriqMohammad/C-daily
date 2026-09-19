# Day 09 — linked_list

A singly linked list implementation in C.

## Operations

| Operation | Description |
|-----------|-------------|
| `insert_head` | Insert a new node at the beginning |
| `insert_tail` | Insert a new node at the end |
| `delete_value` | Delete the first node with a given value |
| `search` | Check if a value exists |
| `print_list` | Print the list |
| `free_list` | Free all nodes |

## Usage

    ./linked_list

## Example Output

    === Linked List Demo ===

    After insert_head 10, 20, 30:
    [30 -> 20 -> 10]

    After insert_tail 40, 50:
    [30 -> 20 -> 10 -> 40 -> 50]

    Search 10: found
    Search 99: not found

    Deleting 20...
    [30 -> 10 -> 40 -> 50]

    Deleting 30 (head)...
    [10 -> 40 -> 50]

    Deleting 99 (not present)...
    [10 -> 40 -> 50]

    After free_list:
    []

## Build

    gcc -o linked_list linked_list.c

## Skills Practiced

- `struct` for nodes
- `malloc` / `free` for dynamic memory
- **Pointer-to-pointer** (`Node **head`) for modifying the head pointer
- Traversal with `cur = cur->next`
- Memory safety (freeing all nodes)
- Handling edge cases (empty list, deleting head)

## Why the Double Pointer?

`insert_head` and `delete_value` need to **modify the head pointer**.
If we passed `Node *head`, changes would be local to the function.
Passing `Node **head` lets the function update the caller's pointer.

## Notes

- The list is **singly linked**—each node points to the next.
- Memory is allocated with `malloc` and freed with `free`.
- `free_list` walks the list and frees each node.
