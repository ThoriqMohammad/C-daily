# c-daily

A daily collection of small C programs across five themes:
📐 **Math** · 🎮 **Game** · 🔐 **Cybersecurity** · 🐧 **Linux Kernel** · ⚙️ **Pure C**

One project per day—building fluency, proof of work, and a foundation
for systems and security roles.

## Purpose

- Practice writing real, working C code every day
- Build a public portfolio of proof
- Rotate through five complementary themes
- Strengthen fundamentals for systems programming and cybersecurity

## Themes

| Theme | Icon | Purpose |
|-------|------|---------|
| **Math** | 📐 | Number theory, combinatorics, cryptography, modular arithmetic |
| **Game** | 🎮 | Interactive programs, state, logic, randomness |
| **Cybersecurity** | 🔐 | Ciphers, encoding, security concepts, hashing |
| **Linux Kernel** | 🐧 | Syscalls, processes, signals, file descriptors |
| **Pure C** | ⚙️ | Data structures, algorithms, file I/O, systems programming |

## Projects

| Day | Project | Theme | Focus |
|-----|---------|-------|-------|
| 01 | `word_count` | ⚙️ Pure C | File I/O, `argc`/`argv` |
| 02 | `gcd_lcm` | 📐 Math | Recursion, number theory |
| 03 | `guess_the_number` | 🎮 Game | Random numbers, game loop |
| 04 | `rock_paper_scissors` | 🎮 Game | Game logic, score tracking |
| 05 | `prime_sieve` | 📐 Math | Arrays, Sieve of Eratosthenes, `malloc` |
| 06 | `tic_tac_toe` | 🎮 Game | 2D arrays, win detection, simple AI |
| 07 | `caesar_cipher` | 🔐 Cybersecurity | Modular arithmetic, classical cryptography |
| 08 | `number_base_converter` | 📐 Math | Binary, octal, decimal, hex |
| 09 | `linked_list` | ⚙️ Pure C | `struct`, `malloc`, pointers |
| 10 | `hangman` | 🎮 Game | Strings, game state, ASCII art |
| 11 | `xor_cipher` | 🔐 Cybersecurity | Bitwise XOR, stream ciphers |
| 12 | `mini_cat` | 🐧 Linux Kernel | `open`, `read`, `write`, `close` |
| 13 | `binary_search` | ⚙️ Pure C | Algorithms, recursion |
| 14 | `fibonacci_memo` | 📐 Math | Dynamic programming |
| 15 | `mini_shell` | 🐧 Linux Kernel | `fork`, `exec`, `wait` |
| 16 | `snakes_and_ladders` | 🎮 Game | Simulation, probability |
| 17 | `password_strength` | 🔐 Cybersecurity | Entropy, password security |
| 18 | `statistics_calculator` | 📐 Math | Mean, variance, stddev |
| 19 | `dice_simulator` | 🎮 Game | Randomness, distributions |
| 20 | `hash_function` | 🔐 Cybersecurity | Hashing, avalanche effect |
| 21 | `binary_tree` | ⚙️ Pure C | BST insert, search, traversal |
| 22 | `matrix_operations` | 📐 Math | 2D arrays, linear algebra |
| 23 | `wordle_clone` | 🎮 Game | String matching, feedback |
| 24 | `dynamic_array` | ⚙️ Pure C | `realloc`, amortized growth |
| 25 | `base64_encoder` | 🔐 Cybersecurity | Encoding, bit manipulation |
| 26 | `linear_algebra` | 📐 Math | Vectors, dot product |
| 27 | `port_scanner` | 🔐 Cybersecurity | Sockets, TCP connect |
| 28 | `hash_table` | ⚙️ Pure C | Chaining, collision handling |
| 29 | `complex_numbers` | 📐 Math | Structs, operator functions |
| 30 | `minesweeper` | 🎮 Game | Grids, recursion, flood fill |
| 31 | `text_editor` | ⚙️ Pure C | Buffers, line editing |
| 32 | `password_manager` | 🔐 Cybersecurity | Encryption, file storage |
| 33 | `file_stats` | 🐧 Linux Kernel | `stat()`, `st_mode`, bitmasking |
| 34 | `mini_ls` | 🐧 Linux Kernel | `opendir`, `readdir`, `struct dirent` |
| 35 | `signal_demo` | 🐧 Linux Kernel | Signals, `sigaction`, signal masks |
| 36 | `dup2_demo` | 🐧 Linux Kernel | `dup`, `dup2`, file descriptor redirection |
| 37 | `ring_buffer` | ⚙️ Pure C | Circular queue, modular indexing |
| 38 | `polynomial` | 📐 Math | Horner's method, convolution, degree management |
| 39 | `2048` | 🎮 Game | Grid state, compress-merge-compress, spawn rules |
| 40 | `vigenere_cipher` | 🔐 Cybersecurity | Polyalphabetic cipher, key repetition, Kasiski attack |
| 41 | `mmap_demo` | 🐧 Linux Kernel | `mmap`, `msync`, `munmap`, file-backed memory |
| 42 | `bloom_filter` | ⚙️ Pure C | Bit arrays, hashing, probabilistic membership |
| 43 | `big_integer` | 📐 Math | Arbitrary-precision arithmetic, carry propagation |
| 44 | `connect_four` | 🎮 Game | Minimax, alpha-beta pruning, heuristic evaluation |
| 45 | `modular_exponentiation` | 🔐 Cybersecurity | Square-and-multiply, toy RSA, Fermat check |
| 46 | `pipe_demo` | 🐧 Linux Kernel | `pipe`, `fork`, `dup2`, producer/consumer |
| 47 | `huffman_coding` | ⚙️ Pure C | Huffman trees, bit packing, compression |
| 48 | `prime_factorization` | 📐 Math | Trial division, Pollard's rho, Miller-Rabin |
| 49 | `snake` | 🎮 Game | Raw terminal mode, non-blocking input, real-time loop |
| 50 | `rsa` | 🔐 Cybersecurity | Public-key crypto, key generation, capstone |

## Structure

Each project has its own folder:

    day-12-mini-cat/
    ├── mini_cat.c
    └── README.md

## Build

Each project has its own README with build instructions.

Typically:

    gcc -o program program.c
    ./program

Some projects require additional flags (e.g., `-lm` for math functions).

## Why This Exists

Reading about C is not the same as writing C.
This repo is the proof of the writing.

## Author

Mohammad Thoriq
https://www.linkedin.com/in/mohammad-thoriq-344a0031a/
