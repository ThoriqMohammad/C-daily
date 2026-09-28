# Day 19 — dice_simulator

Simulates rolling dice and shows the distribution of results.

## What It Does

- Rolls N dice M times.
- Records the sum of each roll.
- Displays a histogram of the results.
- Compares the observed mean to the expected mean.

## Usage

    ./dice_simulator <num_dice> <num_rolls>

- `num_dice`: 1–10
- `num_rolls`: 1–10,000,000

## Example

    $ ./dice_simulator 2 10000
    ====================================
           DICE SIMULATOR
    ====================================

    Dice:  2
    Rolls: 10000

    Sum   Count    Frequency
    ----  -------  ---------------------------------
      2      281   2.81%  ##
      3      555   5.55%  ####
      4      838   8.38%  #######
      5     1114  11.14%  #########
      6     1385  13.85%  ###########
      7     1667  16.67%  #############
      8     1389  13.89%  ###########
      9     1111  11.11%  #########
     10      833   8.33%  #######
     11      556   5.56%  ####
     12      271   2.71%  ##

    Mean (observed): 7.00
    Mean (expected): 7.00

## Build

    gcc -Wall -Wextra -std=c99 -o dice_simulator dice_simulator.c

## Skills Practiced

- **Randomness** — `rand`, `srand`
- **Arrays as counters** — frequency array
- **Histograms** — visual representation of data
- **Math** — probability, expected value
- **Dynamic memory** — `calloc` / `free`
- **`argc` / `argv`** — parsing input

## The Math

For a single six-sided die:

    Expected value = (1 + 2 + 3 + 4 + 5 + 6) / 6 = 3.5

For N dice:

    Expected sum = N × 3.5

For 2 dice: 2 × 3.5 = 7.0

The histogram should cluster around 7.

## The Central Limit Theorem

As the number of dice increases, the distribution of sums
approaches a normal (bell-shaped) curve.

| Dice | Distribution Shape |
|------|--------------------|
| 1 | Uniform (flat) |
| 2 | Triangular |
| 3 | Bell-shaped |
| 10 | Almost normal |

## Real-World Uses

| Domain | Use |
|--------|-----|
| Gaming | Dice mechanics |
| Statistics | Probability experiments |
| Simulation | Monte Carlo methods |
| Security | Random number analysis |
| Finance | Risk modeling |

## Notes

- Uses `calloc` to zero-initialize the frequency array.
- The histogram is scaled to fit 30 characters wide.
- The expected mean is `num_dice * 3.5`.
