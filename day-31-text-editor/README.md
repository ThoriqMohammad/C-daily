# Day 31 — text_editor

A minimal line-based text editor in C.

## Features

| Command | Action |
|---------|--------|
| `a <text>` | Append a line |
| `i <n> <text>` | Insert at line `n` (0-based) |
| `d <n>` | Delete line `n` |
| `r <n> <text>` | Replace line `n` |
| `p` | Print all lines |
| `p <s> <e>` | Print lines `s` to `e` |
| `s <file>` | Save to file |
| `l <file>` | Load from file |
| `q` | Quit |

## Usage

    ./text_editor

## Example Session

    ====================================
             TEXT EDITOR
    ====================================

    > a Hello, world
    Appended.
    > a This is a test.
    Appended.
    > p
      0 | Hello, world
      1 | This is a test.
    > i 1 Inserted line
    Inserted at 1.
    > p
      0 | Hello, world
      1 | Inserted line
      2 | This is a test.
    > d 0
    Deleted line 0.
    > p
      0 | Inserted line
      1 | This is a test.
    > s test.txt
    Saved to test.txt.
    > q
    Goodbye.

## Build

    gcc -Wall -Wextra -std=c99 -o text_editor text_editor.c

## Skills Practiced

- **Dynamic arrays** — growable list of lines
- **Strings** — `strdup`, `strcspn`
- **File I/O** — `fopen`, `fgets`, `fprintf`
- **Command parsing** — `sscanf`, `strtok`
- **Memory management** — `malloc`, `realloc`, `free`

## The Data Structure

```c
typedef struct {
    char **lines;    /* array of strings */
    int count;       /* how many used */
    int capacity;    /* how many allocated */
} Editor;
