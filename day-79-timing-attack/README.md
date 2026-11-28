# Day 79 — timing_attack

A demonstration of the **timing side-channel attack**. A
comparison function that returns early on mismatch leaks
information through execution time. An attacker who can
measure that time can recover a secret byte by byte, without
any cryptographic weakness.

## What the Attack Is

Consider a naive password check:

```c
int check_password(const char *user_input, const char *secret) {
    for (int i = 0; i < strlen(secret); i++) {
        if (user_input[i] != secret[i]) {
            return 0;   // early exit
        }
    }
    return 1;
}
