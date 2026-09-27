# Day 18 — statistics_calculator

Computes basic statistics on a list of numbers.

## Statistics Computed

| Statistic | Formula |
|-----------|---------|
| **Count** | n |
| **Sum** | Σx |
| **Mean** | Σx / n |
| **Median** | Middle value (sorted) |
| **Minimum** | Smallest value |
| **Maximum** | Largest value |
| **Range** | max − min |
| **Variance** | Σ(x − μ)² / n |
| **Standard deviation** | √variance |

## Usage

    ./statistics_calculator <n> <value1> <value2> ...

## Example

    $ ./statistics_calculator 5 10 20 30 40 50
    ====================================
           STATISTICS CALCULATOR
    ====================================

    Count:              5
    Sum:                150.0000
    Mean:               30.0000
    Median:             30.0000
    Minimum:            10.0000
    Maximum:            50.0000
    Range:              40.0000
    Variance:           200.0000
    Standard deviation: 14.1421

## Build

    gcc -Wall -Wextra -std=c99 -o statistics_calculator statistics_calculator.c -lm

**Note:** `-lm` is needed for `sqrt`.

## Skills Practiced

- **Arrays** — storing the values
- **Sorting** — `qsort` for median
- **Math** — `sqrt`, `pow`
- **`argc` / `argv`** — parsing input
- **Statistical formulas** — mean, variance, SD
- **Dynamic memory** — `malloc` / `free`

## Why This Matters

Statistics is the foundation of:

| Domain | Use |
|--------|-----|
| **Security** | Anomaly detection |
| **Data science** | Exploratory analysis |
| **Finance** | Risk measurement |
| **Science** | Experimental analysis |
| **Machine learning** | Feature engineering |

## Notes

- Uses **population variance** (divides by n).
- For **sample variance**, divide by (n − 1).
- Median requires a sorted array.
- For even n, median is the average of the two middle values.
