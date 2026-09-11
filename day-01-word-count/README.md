# Day 01 — word_count

A simple clone of the Unix `wc` command.

## What It Does

Counts the number of:

- **Lines** (newline characters)
- **Words** (sequences of non-whitespace characters)
- **Characters** (including whitespace)

## Usage

    ./word_count [file]

- If `file` is provided, reads from that file.
- If no file is given, reads from **stdin** (standard input).

## Output Format

    lines words characters

## Examples

**Reading from a file:**

    $ ./word_count test.txt
    3 12 67

**Reading from stdin:**

    $ echo "Hello world" | ./word_count
    1 2 12

**Build:**

    gcc -o word_count word_count.c

## Skills Practiced

- `argc` / `argv`
- File I/O (`fopen`, `fgetc`, `fclose`)
- `stdin` vs. file input
- Character classification (`isspace`)
- State tracking (`in_word`)
- Error handling (`fprintf`, `stderr`)

## Notes

- Uses `isspace()` from `<ctype.h>` to detect whitespace.
- Treats consecutive whitespace as a single word boundary.
- Prints to stdout in the same format as `wc`.
