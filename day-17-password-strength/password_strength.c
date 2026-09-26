/* password_strength.c
 *
 * Estimates the strength of a password.
 *
 * It scores a password based on:
 *   - Length
 *   - Character variety (lowercase, uppercase, digits, symbols)
 *   - Presence of repeated patterns
 *   - Presence of common words
 *
 * It also estimates entropy (in bits) and reports how long
 * a brute-force attack would take (roughly).
 *
 * Usage:
 *   ./password_strength
 *
 * Skills practiced:
 *   - String handling
 *   - Character classification (ctype.h)
 *   - Bit manipulation (entropy)
 *   - Math (log2)
 *   - Security concepts
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_LEN 256

/* A small list of common passwords to penalize */
static const char *COMMON[] = {
    "password", "123456", "12345678", "qwerty", "abc123",
    "letmein", "welcome", "admin", "login", "monkey"
};
#define NUM_COMMON (sizeof(COMMON) / sizeof(COMMON[0]))

/* Character-set size for entropy calculation */
int charset_size(const char *pw)
{
    int has_lower = 0, has_upper = 0, has_digit = 0, has_symbol = 0;

    for (int i = 0; pw[i]; i++)
    {
        unsigned char c = (unsigned char) pw[i];
        if (islower(c))      has_lower = 1;
        else if (isupper(c)) has_upper = 1;
        else if (isdigit(c)) has_digit = 1;
        else                 has_symbol = 1;
    }

    int size = 0;
    if (has_lower)  size += 26;
    if (has_upper)  size += 26;
    if (has_digit)  size += 10;
    if (has_symbol) size += 33;   /* printable symbols */

    return size;
}

/* Calculate entropy in bits */
double entropy_bits(const char *pw)
{
    int size = charset_size(pw);
    int len = (int) strlen(pw);

    if (size == 0 || len == 0) return 0.0;

    return len * (log((double) size) / log(2.0));
}

/* Check if the password contains a common password */
int contains_common(const char *pw)
{
    char lower[MAX_LEN];
    int i;

    for (i = 0; pw[i] && i < MAX_LEN - 1; i++)
        lower[i] = (char) tolower((unsigned char) pw[i]);
    lower[i] = '\0';

    for (size_t k = 0; k < NUM_COMMON; k++)
    {
        if (strstr(lower, COMMON[k]) != NULL)
            return 1;
    }
    return 0;
}

/* Count character-class variety (0-4) */
int variety(const char *pw)
{
    int has_lower = 0, has_upper = 0, has_digit = 0, has_symbol = 0;

    for (int i = 0; pw[i]; i++)
    {
        unsigned char c = (unsigned char) pw[i];
        if (islower(c))      has_lower = 1;
        else if (isupper(c)) has_upper = 1;
        else if (isdigit(c)) has_digit = 1;
        else                 has_symbol = 1;
    }

    return has_lower + has_upper + has_digit + has_symbol;
}

/* Score 0-100 */
int score_password(const char *pw)
{
    int len = (int) strlen(pw);
    int var = variety(pw);
    double ent = entropy_bits(pw);
    int score = 0;

    /* Length: up to 40 points */
    if (len >= 8)  score += 10;
    if (len >= 12) score += 10;
    if (len >= 16) score += 10;
    if (len >= 20) score += 10;

    /* Variety: up to 30 points */
    score += var * 7;   /* 1 -> 7, 2 -> 14, 3 -> 21, 4 -> 28 */

    /* Entropy: up to 30 points */
    if (ent >= 30)  score += 5;
    if (ent >= 50)  score += 5;
    if (ent >= 70)  score += 10;
    if (ent >= 100) score += 10;

    /* Penalties */
    if (contains_common(pw))  score -= 40;
    if (len < 8)              score -= 20;

    /* Clamp to 0-100 */
    if (score < 0)   score = 0;
    if (score > 100) score = 100;

    return score;
}

/* Human-readable label */
const char *strength_label(int score)
{
    if (score < 20)  return "Very Weak";
    if (score < 40)  return "Weak";
    if (score < 60)  return "Moderate";
    if (score < 80)  return "Strong";
    return "Very Strong";
}

/* Rough guess of brute-force time (assume 1e10 guesses/sec) */
void brute_force_time(double bits)
{
    double combinations = pow(2.0, bits);
    double seconds = combinations / 1e10;

    if (seconds < 1)             printf("Instantly\n");
    else if (seconds < 60)       printf("%.1f seconds\n", seconds);
    else if (seconds < 3600)     printf("%.1f minutes\n", seconds / 60);
    else if (seconds < 86400)    printf("%.1f hours\n", seconds / 3600);
    else if (seconds < 31536000) printf("%.1f days\n", seconds / 86400);
    else                         printf("%.0f years\n", seconds / 31536000);
}

int main(void)
{
    char password[MAX_LEN];

    printf("====================================\n");
    printf("       PASSWORD STRENGTH            \n");
    printf("====================================\n\n");

    printf("Enter a password to analyze: ");
    if (scanf("%255s", password) != 1)
    {
        fprintf(stderr, "Error reading password.\n");
        return 1;
    }

    int score = score_password(password);
    double ent = entropy_bits(password);
    int var = variety(password);

    printf("\n--- Analysis ---\n");
    printf("Length:           %zu\n", strlen(password));
    printf("Character classes: %d (lower/upper/digit/symbol)\n", var);
    printf("Charset size:      %d\n", charset_size(password));
    printf("Entropy:           %.1f bits\n", ent);
    printf("Score:             %d/100\n", score);
    printf("Rating:            %s\n", strength_label(score));

    printf("Brute-force time:  ");
    brute_force_time(ent);
    printf("\n");

    if (contains_common(password))
        printf("⚠️  This password contains a common pattern!\n");

    return 0;
}
