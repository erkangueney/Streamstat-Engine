/*
 * gen_test_csv.c — synthetic time-series CSV generator, used only to
 * produce test/benchmark input for the engine. This is a *tooling*
 * program (not part of the engine's hot data path), so it freely uses
 * <stdio.h> buffered I/O for simplicity.
 *
 * Usage:
 *   ./gen_test_csv <output.csv> <num_rows> [seed]
 *
 * Produces a header line: timestamp,value,sensor_b,sensor_c
 * `value` is a random walk plus a slow sine-like oscillation so that the
 * moving average / stddev computed by the engine are visibly non-trivial
 * and can be sanity-checked against a Python/awk reference on small runs.
 *
 * Rough size guide: ~40 bytes/row, so ~25M rows ≈ 1 GB, ~250M rows ≈ 10 GB.
 */

#include <stdio.h>
#include <stdlib.h>

typedef struct { unsigned long long state; } rng_t;

static unsigned long long xorshift64(rng_t *r) {
    unsigned long long x = r->state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    r->state = x;
    return x;
}

static double rand_unit(rng_t *r) {
    /* [0, 1) */
    return (double)(xorshift64(r) >> 11) / (double)(1ULL << 53);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <output.csv> <num_rows> [seed]\n", argv[0]);
        return 1;
    }

    const char *out_path = argv[1];
    long long   num_rows = atoll(argv[2]);
    unsigned long long seed = (argc >= 4) ? (unsigned long long)atoll(argv[3]) : 88172645463325252ULL;

    FILE *f = fopen(out_path, "w");
    if (!f) {
        perror("fopen");
        return 1;
    }

    rng_t rng = { seed ? seed : 1 };
    double walk = 100.0;

    fputs("timestamp,value,sensor_b,sensor_c\n", f);

    /* Manual buffered writes via a local char buffer + fwrite would be
     * marginally faster than per-line fprintf, but this tool's own speed
     * is not the point of the assignment — only the engine's is. */
    for (long long i = 0; i < num_rows; i++) {
        double noise = (rand_unit(&rng) - 0.5) * 2.0;
        walk += noise * 0.05;
        double oscillation = 5.0 * ((double)((i / 500) % 200) - 100.0) / 100.0;
        double value = walk + oscillation;

        double sensor_b = rand_unit(&rng) * 1000.0;
        double sensor_c = rand_unit(&rng) * 50.0 - 25.0;

        fprintf(f, "%lld,%.6f,%.6f,%.6f\n", 1700000000LL + i, value, sensor_b, sensor_c);
    }

    fclose(f);
    fprintf(stderr, "wrote %lld rows to %s\n", num_rows, out_path);
    return 0;
}
