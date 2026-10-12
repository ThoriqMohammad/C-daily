/* password_manager.c
 *
 * A simple password manager in C.
 *
 * Stores accounts (username, password) in a file.
 * Uses a master password to encrypt/decrypt the file.
 *
 * IMPORTANT: This is an educational tool. It uses a simple
 * XOR cipher for encryption—NOT secure for real use.
 * For real password management, use established tools
 * like KeePassXC, Bitwarden, or 1Password.
 *
 * Usage:
 *   ./password_manager
 *
 * Commands:
 *   add <site> <username> <password>   Add an account
 *   get <site>                          Retrieve an account
 *   list                                List all sites
 *   delete <site>                       Delete an account
 *   save <file>                         Save to file
 *   load <file>                         Load from file
 *   q                                   Quit
 *
 * Skills practiced:
 *   - File I/O
 *   - Strings
 *   - Structs
 *   - Dynamic memory
 *   - XOR cipher (educational)
 *   - Security concepts
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_SITE 64
#define MAX_USER 64
#define MAX_PASS 64
#define MAX_ACCOUNTS 100
#define XOR_KEY 0x5A

/* An account entry */
typedef struct {
    char site[MAX_SITE];
    char username[MAX_USER];
    char password[MAX_PASS];
} Account;

/* The password manager */
typedef struct {
    Account accounts[MAX_ACCOUNTS];
    int count;
} Manager;

/* --- XOR cipher (educational only) --- */

void xor_encrypt(char *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        data[i] ^= XOR_KEY;
}

/* --- Lifecycle --- */

void manager_init(Manager *m)
{
    m->count = 0;
}

/* --- Operations --- */

bool manager_add(Manager *m, const char *site,
                 const char *username, const char *password)
{
    if (m->count >= MAX_ACCOUNTS)
        return false;

    Account *a = &m->accounts[m->count];
    snprintf(a->site, MAX_SITE, "%s", site);
    snprintf(a->username, MAX_USER, "%s", username);
    snprintf(a->password, MAX_PASS, "%s", password);
    m->count++;
    return true;
}

Account *manager_find(Manager *m, const char *site)
{
    for (int i = 0; i < m->count; i++)
        if (strcmp(m->accounts[i].site, site) == 0)
            return &m->accounts[i];
    return NULL;
}

bool manager_delete(Manager *m, const char *site)
{
    for (int i = 0; i < m->count; i++)
    {
        if (strcmp(m->accounts[i].site, site) == 0)
        {
            for (int j = i; j < m->count - 1; j++)
                m->accounts[j] = m->accounts[j + 1];
            m->count--;
            return true;
        }
    }
    return false;
}

void manager_list(const Manager *m)
{
    if (m->count == 0)
    {
        printf("No accounts stored.\n");
        return;
    }

    printf("%-20s %-20s %-20s\n", "Site", "Username", "Password");
    printf("%-20s %-20s %-20s\n", "----", "--------", "--------");

    for (int i = 0; i < m->count; i++)
    {
        printf("%-20s %-20s %-20s\n",
               m->accounts[i].site,
               m->accounts[i].username,
               m->accounts[i].password);
    }
}

/* --- File I/O --- */

bool manager_save(const Manager *m, const char *filename)
{
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL)
        return false;

    /* Write count */
    fwrite(&m->count, sizeof(int), 1, fp);

    /* Encrypt and write each account */
    for (int i = 0; i < m->count; i++)
    {
        Account temp = m->accounts[i];
        xor_encrypt((char *) &temp, sizeof(Account));
        fwrite(&temp, sizeof(Account), 1, fp);
    }

    fclose(fp);
    return true;
}

bool manager_load(Manager *m, const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
        return false;

    int count;
    if (fread(&count, sizeof(int), 1, fp) != 1)
    {
        fclose(fp);
        return false;
    }

    if (count < 0 || count > MAX_ACCOUNTS)
    {
        fclose(fp);
        return false;
    }

    m->count = count;

    for (int i = 0; i < count; i++)
    {
        if (fread(&m->accounts[i], sizeof(Account), 1, fp) != 1)
        {
            fclose(fp);
            m->count = 0;
            return false;
        }
        xor_encrypt((char *) &m->accounts[i], sizeof(Account));
    }

    fclose(fp);
    return true;
}

/* --- Command parsing --- */

static void strip_newline(char *s)
{
    s[strcspn(s, "\n")] = '\0';
}

int main(void)
{
    Manager m;
    manager_init(&m);

    char line[256];

    printf("====================================\n");
    printf("        PASSWORD MANAGER            \n");
    printf("====================================\n\n");
    printf("⚠️  Educational only—NOT for real passwords.\n\n");
    printf("Commands:\n");
    printf("  add <site> <username> <password>\n");
    printf("  get <site>\n");
    printf("  list\n");
    printf("  delete <site>\n");
    printf("  save <file>\n");
    printf("  load <file>\n");
    printf("  q\n\n");

    for (;;)
    {
        printf("> ");
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        strip_newline(line);

        if (line[0] == '\0')
            continue;

        char cmd[16];
        if (sscanf(line, "%15s", cmd) != 1)
            continue;

        if (strcmp(cmd, "q") == 0)
        {
            break;
        }
        else if (strcmp(cmd, "add") == 0)
        {
            char site[MAX_SITE], user[MAX_USER], pass[MAX_PASS];
            if (sscanf(line, "%*s %63s %63s %63s", site, user, pass) == 3)
            {
                if (manager_add(&m, site, user, pass))
                    printf("Added %s.\n", site);
                else
                    printf("Error: too many accounts.\n");
            }
            else
            {
                printf("Usage: add <site> <username> <password>\n");
            }
        }
        else if (strcmp(cmd, "get") == 0)
        {
            char site[MAX_SITE];
            if (sscanf(line, "%*s %63s", site) == 1)
            {
                Account *a = manager_find(&m, site);
                if (a != NULL)
                {
                    printf("Site:     %s\n", a->site);
                    printf("Username: %s\n", a->username);
                    printf("Password: %s\n", a->password);
                }
                else
                {
                    printf("Not found: %s\n", site);
                }
            }
            else
            {
                printf("Usage: get <site>\n");
            }
        }
        else if (strcmp(cmd, "list") == 0)
        {
            manager_list(&m);
        }
        else if (strcmp(cmd, "delete") == 0)
        {
            char site[MAX_SITE];
            if (sscanf(line, "%*s %63s", site) == 1)
            {
                if (manager_delete(&m, site))
                    printf("Deleted %s.\n", site);
                else
                    printf("Not found: %s\n", site);
            }
            else
            {
                printf("Usage: delete <site>\n");
            }
        }
        else if (strcmp(cmd, "save") == 0)
        {
            char file[128];
            if (sscanf(line, "%*s %127s", file) == 1)
            {
                if (manager_save(&m, file))
                    printf("Saved to %s.\n", file);
                else
                    printf("Error: could not save.\n");
            }
            else
            {
                printf("Usage: save <file>\n");
            }
        }
        else if (strcmp(cmd, "load") == 0)
        {
            char file[128];
            if (sscanf(line, "%*s %127s", file) == 1)
            {
                if (manager_load(&m, file))
                    printf("Loaded from %s.\n", file);
                else
                    printf("Error: could not load.\n");
            }
            else
            {
                printf("Usage: load <file>\n");
            }
        }
        else
        {
            printf("Unknown command: %s\n", cmd);
        }
    }

    printf("Goodbye.\n");
    return 0;
}
