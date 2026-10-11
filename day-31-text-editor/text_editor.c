/* text_editor.c
 *
 * A minimal line-based text editor in C.
 *
 * Stores lines in a dynamic array. Supports:
 *   - Append a line
 *   - Insert a line at a position
 *   - Delete a line
 *   - Replace a line
 *   - Print all lines
 *   - Print a range
 *   - Save to a file
 *   - Load from a file
 *
 * Commands (line-based):
 *   a <text>       append a line
 *   i <n> <text>   insert at line n (0-based)
 *   d <n>          delete line n
 *   r <n> <text>   replace line n
 *   p              print all lines
 *   p <start> <end> print lines start..end
 *   s <filename>   save to file
 *   l <filename>   load from file
 *   q              quit
 *
 * Usage:
 *   ./text_editor
 *
 * Skills practiced:
 *   - Dynamic arrays
 *   - Strings
 *   - File I/O
 *   - Command parsing
 *   - Memory management
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_LINE 1024
#define INITIAL_CAPACITY 16

/* The editor state */
typedef struct {
    char **lines;       /* dynamic array of strings */
    int count;          /* number of lines */
    int capacity;       /* allocated slots */
} Editor;

/* --- Lifecycle --- */

Editor *editor_create(void)
{
    Editor *e = malloc(sizeof(Editor));
    if (e == NULL) return NULL;

    e->lines = malloc(INITIAL_CAPACITY * sizeof(char *));
    if (e->lines == NULL)
    {
        free(e);
        return NULL;
    }

    e->count = 0;
    e->capacity = INITIAL_CAPACITY;
    return e;
}

void editor_free(Editor *e)
{
    if (e == NULL) return;

    for (int i = 0; i < e->count; i++)
        free(e->lines[i]);

    free(e->lines);
    free(e);
}

/* Grow the array if needed */
static bool editor_ensure_capacity(Editor *e)
{
    if (e->count < e->capacity)
        return true;

    int new_capacity = e->capacity * 2;
    char **new_lines = realloc(e->lines, new_capacity * sizeof(char *));
    if (new_lines == NULL)
        return false;

    e->lines = new_lines;
    e->capacity = new_capacity;
    return true;
}

/* --- Operations --- */

bool editor_append(Editor *e, const char *text)
{
    if (!editor_ensure_capacity(e))
        return false;

    e->lines[e->count] = strdup(text);
    if (e->lines[e->count] == NULL)
        return false;

    e->count++;
    return true;
}

bool editor_insert(Editor *e, int index, const char *text)
{
    if (index < 0 || index > e->count)
        return false;

    if (!editor_ensure_capacity(e))
        return false;

    /* Shift lines right */
    for (int i = e->count; i > index; i--)
        e->lines[i] = e->lines[i - 1];

    e->lines[index] = strdup(text);
    if (e->lines[index] == NULL)
        return false;

    e->count++;
    return true;
}

bool editor_delete(Editor *e, int index)
{
    if (index < 0 || index >= e->count)
        return false;

    free(e->lines[index]);

    /* Shift lines left */
    for (int i = index; i < e->count - 1; i++)
        e->lines[i] = e->lines[i + 1];

    e->count--;
    return true;
}

bool editor_replace(Editor *e, int index, const char *text)
{
    if (index < 0 || index >= e->count)
        return false;

    char *new_line = strdup(text);
    if (new_line == NULL)
        return false;

    free(e->lines[index]);
    e->lines[index] = new_line;
    return true;
}

/* --- Printing --- */

void editor_print(const Editor *e)
{
    for (int i = 0; i < e->count; i++)
        printf("%3d | %s\n", i, e->lines[i]);
}

void editor_print_range(const Editor *e, int start, int end)
{
    if (start < 0) start = 0;
    if (end >= e->count) end = e->count - 1;

    for (int i = start; i <= end; i++)
        printf("%3d | %s\n", i, e->lines[i]);
}

/* --- File I/O --- */

bool editor_save(const Editor *e, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
        return false;

    for (int i = 0; i < e->count; i++)
        fprintf(fp, "%s\n", e->lines[i]);

    fclose(fp);
    return true;
}

bool editor_load(Editor *e, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
        return false;

    char buffer[MAX_LINE];

    /* Clear existing lines */
    for (int i = 0; i < e->count; i++)
        free(e->lines[i]);
    e->count = 0;

    while (fgets(buffer, MAX_LINE, fp) != NULL)
    {
        /* Strip trailing newline */
        buffer[strcspn(buffer, "\n")] = '\0';
        editor_append(e, buffer);
    }

    fclose(fp);
    return true;
}

/* --- Command parsing --- */

/* Read a line safely from stdin */
static bool read_line(char *buffer, int size)
{
    if (fgets(buffer, size, stdin) == NULL)
        return false;

    buffer[strcspn(buffer, "\n")] = '\0';
    return true;
}

int main(void)
{
    Editor *editor = editor_create();
    if (editor == NULL)
    {
        fprintf(stderr, "Error: could not create editor.\n");
        return 1;
    }

    char line[MAX_LINE];

    printf("====================================\n");
    printf("         TEXT EDITOR                \n");
    printf("====================================\n\n");
    printf("Commands:\n");
    printf("  a <text>        append\n");
    printf("  i <n> <text>    insert at line n\n");
    printf("  d <n>           delete line n\n");
    printf("  r <n> <text>    replace line n\n");
    printf("  p               print all\n");
    printf("  p <s> <e>       print range\n");
    printf("  s <file>        save\n");
    printf("  l <file>        load\n");
    printf("  q               quit\n\n");

    for (;;)
    {
        printf("> ");
        if (!read_line(line, MAX_LINE))
            break;

        if (line[0] == '\0')
            continue;

        char cmd = line[0];
        const char *rest = line + 1;
        while (*rest == ' ') rest++;

        if (cmd == 'q' || cmd == 'Q')
        {
            break;
        }
        else if (cmd == 'a' || cmd == 'A')
        {
            if (editor_append(editor, rest))
                printf("Appended.\n");
            else
                printf("Error: append failed.\n");
        }
        else if (cmd == 'i' || cmd == 'I')
        {
            int index;
            char text[MAX_LINE];
            if (sscanf(rest, "%d %[^\n]", &index, text) == 2)
            {
                if (editor_insert(editor, index, text))
                    printf("Inserted at %d.\n", index);
                else
                    printf("Error: invalid index.\n");
            }
            else
            {
                printf("Usage: i <n> <text>\n");
            }
        }
        else if (cmd == 'd' || cmd == 'D')
        {
            int index;
            if (sscanf(rest, "%d", &index) == 1)
            {
                if (editor_delete(editor, index))
                    printf("Deleted line %d.\n", index);
                else
                    printf("Error: invalid index.\n");
            }
            else
            {
                printf("Usage: d <n>\n");
            }
        }
        else if (cmd == 'r' || cmd == 'R')
        {
            int index;
            char text[MAX_LINE];
            if (sscanf(rest, "%d %[^\n]", &index, text) == 2)
            {
                if (editor_replace(editor, index, text))
                    printf("Replaced line %d.\n", index);
                else
                    printf("Error: invalid index.\n");
            }
            else
            {
                printf("Usage: r <n> <text>\n");
            }
        }
        else if (cmd == 'p' || cmd == 'P')
        {
            int start, end;
            if (sscanf(rest, "%d %d", &start, &end) == 2)
                editor_print_range(editor, start, end);
            else
                editor_print(editor);
        }
        else if (cmd == 's' || cmd == 'S')
        {
            if (*rest == '\0')
                printf("Usage: s <file>\n");
            else if (editor_save(editor, rest))
                printf("Saved to %s.\n", rest);
            else
                printf("Error: could not save.\n");
        }
        else if (cmd == 'l' || cmd == 'L')
        {
            if (*rest == '\0')
                printf("Usage: l <file>\n");
            else if (editor_load(editor, rest))
                printf("Loaded from %s.\n", rest);
            else
                printf("Error: could not load.\n");
        }
        else
        {
            printf("Unknown command: %c\n", cmd);
        }
    }

    editor_free(editor);
    printf("Goodbye.\n");
    return 0;
}
