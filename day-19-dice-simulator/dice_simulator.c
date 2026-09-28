/* dice_simulator.c
 *
 * Simulates rolling dice and shows the distribution of results.
 *
 * This is a classic probability experiment:
 *   - Roll N dice M times
 *   - Record the sum of each roll
 *   - Display a histogram of the results
 *
 * It demonstrates:
 *   - Randomness
 *   - The Central Limit Theorem (sums cluster around the mean)
 *   - Arrays as counters
 *
 * Usage:
 *   ./dice_simulator <num_dice> <num_rolls>
 *
 * Example:
 *   ./dice_simulator 2 10000
 *
 * Skills practiced:
 *   - Randomness (rand, srand)
 *   - Arrays as frequency counters
 *   - Histograms
 *   - Math (probability, mean)
 *   - argc / argv
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_DICE 10
#define MIN_ROLLS 1
#define MAX_ROLLS 10000000

/* Roll a single six-sided die */
int roll_die(void)
{
    return rand() % 6 + 1;
}

/* Print a histogram bar */
void print_bar(int count, int max_count, int width)
{
    int bar_length = (int)((double) count / max_count * width);

    for (int i = 0; i < bar_length; i++)
        putchar('#');
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <num_dice> <num_rolls>\n", argv[0]);
        return 1;
    }

    int num_dice = atoi(argv[1]);
    int num_rolls = atoi(argv[2]);

    if (num_dice < 1 || num_dice > MAX_DICE)
    {
        fprintf(stderr, "Error: num_dice must be between 1 and %d.\n", MAX_DICE);
        return 1;
    }

    if (num_rolls < MIN_ROLLS || num_rolls > MAX_ROLLS)
    {
        fprintf(stderr, "Error: num_rolls must be between %d and %d.\n",
                MIN_ROLLS, MAX_ROLLS);
        return 1;
    }

    /* Possible sums: from num_dice to num_dice*6 */
    int min_sum = num_dice;
    int max_sum = num_dice * 6;
    int num_sums = max_sum - min_sum + 1;

    /* Allocate frequency array */
    int *freq = calloc(num_sums, sizeof(int));
    if (freq == NULL)
    {
        fprintf(stderr, "Error: malloc failed.\n");
        return 1;
    }

    srand((unsigned) time(NULL));

    /* Roll the dice */
    for (int i = 0; i < num_rolls; i++)
    {
        int sum = 0;
        for (int d = 0; d < num_dice; d++)
            sum += roll_die();

        freq[sum - min_sum]++;
    }

    /* Find the maximum frequency (for scaling the histogram) */
    int max_freq = 0;
    for (int i = 0; i < num_sums; i++)
        if (freq[i] > max_freq)
            max_freq = freq[i];

    /* Print the histogram */
    printf("====================================\n");
    printf("       DICE SIMULATOR               \n");
    printf("====================================\n\n");

    printf("Dice:  %d\n", num_dice);
    printf("Rolls: %d\n\n", num_rolls);

    printf("Sum   Count    Frequency\n");
    printf("----  -------  ---------------------------------\n");

    for (int i = 0; i < num_sums; i++)
    {
        int sum = min_sum + i;
        double percent = (double) freq[i] / num_rolls * 100.0;

        printf("%3d  %7d  %5.2f%%  ", sum, freq[i], percent);
        print_bar(freq[i], max_freq, 30);
        printf("\n");
    }

    /* Statistics */
    double total = 0.0;
    for (int i = 0; i < num_sums; i++)
        total += (double) freq[i] * (min_sum + i);

    double mean = total / num_rolls;
    double expected = num_dice * 3.5;   /* expected value per die = 3.5 */

    printf("\nMean (observed): %.2f\n", mean);
    printf("Mean (expected): %.2f\n", expected);

    free(freq);
    return 0;
}
