# Day 08 — number_base_converter

Converts numbers between bases: **binary**, **octal**, **decimal**, and **hexadecimal**.

## Usage

    ./number_base_converter <value> <from_base> <to_base>

Supported bases: `2`, `8`, `10`, `16`.

## Examples

**Decimal → Binary:**

    $ ./number_base_converter 255 10 2
    11111111

**Binary → Decimal:**

    $ ./number_base_converter 11111111 2 10
    255

**Decimal → Hex:**

    $ ./number_base_converter 255 10 16
    FF

**Hex → Decimal:**

    $ ./number_base_converter FF 16 10
    255

**Octal → Decimal:**

    $ ./number_base_converter 377 8 10
    255

**Negative numbers:**

    $ ./number_base_converter -10 10 2
    -1010

## Build

    gcc -o number_base_converter number_base_converter.c

## Skills Practiced

- Number systems (binary, octal, decimal, hexadecimal)
- String parsing (manual conversion)
- Modular arithmetic (`value % base`, `value / base`)
- Character arithmetic (`c - '0'`, `c - 'A' + 10`)
- `argc` / `argv`
- Input validation
- Error handling

## Math Behind the Conversion

**Parsing** (base `b` to decimal):

    result = 0
    for each digit d:
        result = result * b + value_of(d)

**Converting** (decimal to base `b`):

    while value > 0:
        digit = value % b
        value = value / b
        prepend digit

## Notes

- Only bases 2, 8, 10, and 16 are supported.
- Negative numbers are handled.
- Invalid digits are reported with a clear error message.
- The output is always in uppercase for hex (e.g., `FF`, not `ff`).
- Uses `long` to handle larger values.

## Why This Matters

- Number bases are fundamental to computing.
- Hex is everywhere: memory addresses, colors, MAC addresses.
- Binary is the machine's native language.
- This project teaches the **mechanics** behind `printf("%x")` and `strtol()`.
