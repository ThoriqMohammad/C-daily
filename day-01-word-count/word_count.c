/* word_count.c
 *
 * A simple clone of the Unix `wc` command.
 * Counts lines, words, and characters from a file or stdin.
 *
 * Usage:
 *   ./word_count [file]
 *
 * If no file is given, reads from stdin.
 * Output format:
 *   lines words characters
 */

#include <stdio.h>
#include <ctype.h>

int main(int argc, char *argv[])
{
    FILE *fp;
    int ch;
    int lines = 0, words = 0, chars = 0;
    int in_word = 0;

    /* Determine input source */
    if (argc > 1)
    {
        fp = fopen(argv[1], "r");
        if (fp == NULL)
        {
            fprintf(stderr, "Error: cannot open file '%s'\n", argv[1]);
            return 1;
        }
    }
    else
    {
        fp = stdin;
    }

    /* Read character by character */
    while ((ch = fgetc(fp)) != EOF)
    {
        chars++;

        if (ch == '\n')
            lines++;

        if (isspace(ch))
        {
            in_word = 0;
        }
        else if (!in_word)
        {
            in_word = 1;
            words++;
        }
    }

    /* Close file if we opened one */
    if (fp != stdin)
        fclose(fp);

    printf("%d %d %d\n", lines, words, chars);
    return 0;
}
