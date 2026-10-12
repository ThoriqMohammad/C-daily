# Day 32 — password_manager

A simple password manager in C.

## ⚠️ Warning: Educational Only

**This program uses a XOR cipher—it is NOT secure.**

For real password management, use established tools:

| Tool | Why |
|------|-----|
| **KeePassXC** | Open source, offline |
| **Bitwarden** | Open source, cloud sync |
| **1Password** | Commercial, polished |
| **GnuPG + pass** | Unix philosophy |

**Never use this program for real passwords.**

## Features

| Command | Action |
|---------|--------|
| `add <site> <username> <password>` | Add an account |
| `get <site>` | Retrieve an account |
| `list` | List all sites |
| `delete <site>` | Delete an account |
| `save <file>` | Save to file (encrypted) |
| `load <file>` | Load from file (decrypted) |
| `q` | Quit |

## Usage

    ./password_manager

## Example Session

    ====================================
            PASSWORD MANAGER
    ====================================

    ⚠️  Educational only—NOT for real passwords.

    > add github alice mypassword
    Added github.
    > add gitlab bob anotherpass
    Added gitlab.
    > list
    Site                 Username             Password
    ----                 --------             --------
    github               alice                mypassword
    gitlab               bob                  anotherpass
    > get github
    Site:     github
    Username: alice
    Password: mypassword
    > save vault.dat
    Saved to vault.dat.
    > q
    Goodbye.

## Build

    gcc -Wall -Wextra -std=c99 -o password_manager password_manager.c

## Skills Practiced

- **File I/O** — `fwrite`, `fread` (binary mode)
- **Strings** — `snprintf`, `strcmp`, `strcspn`
- **Structs** — `Account`, `Manager`
- **Arrays** — fixed-size storage
- **XOR cipher** — educational encryption
- **Command parsing** — `sscanf`

## The XOR Cipher

```c
void xor_encrypt(char *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        data[i] ^= XOR_KEY;
}
