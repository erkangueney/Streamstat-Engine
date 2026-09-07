/*
 * main.c — CLI driver for the pure-C CSV parsing & statistics engine.
 *
 * StreamStat Engine
 * Copyright (c) 2026 Erkan <erkantahaguney@gmail.com>. All rights reserved.
 * See LICENSE and PROVENANCE.md for authorship/provenance terms.
 *
 * Usage:
 *   ./streamstat <file.csv> <column_1based> [window_size] [delimiter] [--header]
 *   ./streamstat --about
 *
 * Example:
 *   ./streamstat data/ticks.csv 3 500 , --header
 *
 * Reads `file.csv` in fixed-size chunks straight from the OS (read(2)),
 * with zero per-line heap allocation: every buffer used by the pipeline
 * comes from a single bump-pointer arena created once at startup. For the
 * chosen 1-based column it computes, in one streaming pass:
 *
 *   - the overall mean and standard deviation (Welford's algorithm), and
 *   - a moving average and moving standard deviation over the last
 *     `window_size` values (O(1) sliding window).
 *
 * No third-party library is used anywhere: no Pandas/NumPy-equivalent, no
 * external database, no pre-built analytics service. I/O is POSIX
 * read()/open()/close(); everything else (buffering, line splitting,
 * string-to-double conversion, memmove, sqrt, statistics) is implemented
 * from scratch in this repository.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arena.h"
#include "csv_parser.h"
#include "provenance.h"
#include "stats.h"

#define DEFAULT_CHUNK_SIZE   (8u * 1024u * 1024u)   /* 8 MiB I/O blocks   */
#define DEFAULT_WINDOW_SIZE  1000

static double wall_seconds(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

/* Prints the authorship fingerprint baked into this binary. This string
 * data survives in the compiled executable (visible even via a plain
 * `strings` on the binary, not just via --about) as evidence of original
 * authorship — see PROVENANCE.md. */
static void print_about(void) {
    printf("StreamStat Engine — pure C streaming CSV/statistics engine\n");
    printf("Author        : %s <%s>\n", PROVENANCE_AUTHOR_NAME, PROVENANCE_AUTHOR_EMAIL);
    printf("Project UUID  : %s\n", PROVENANCE_PROJECT_UUID);
    printf("Fingerprint   : STREAMSTAT-FINGERPRINT-v%d-%s\n",
           PROVENANCE_SCHEME_VERSION, PROVENANCE_PROJECT_UUID);
    printf("Built         : %s %s\n", __DATE__, __TIME__);
    printf("See PROVENANCE.md in the source distribution for the hashed,\n");
    printf("dated authorship record this fingerprint corresponds to.\n");
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--about") == 0) {
        print_about();
        return 0;
    }

    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <file.csv> <column_1based> [window_size] "
                "[delimiter] [--header]\n",
                argv[0]);
        return 1;
    }

    const char *path       = argv[1];
    int         column     = atoi(argv[2]);
    size_t      window     = (argc >= 4) ? (size_t)atol(argv[3]) : DEFAULT_WINDOW_SIZE;
    char        delimiter  = (argc >= 5 && argv[4][0] != '\0') ? argv[4][0] : ',';
    int         has_header = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--header") == 0) has_header = 1;
    }

    if (column < 1) {
        fprintf(stderr, "column_1based must be >= 1\n");
        return 1;
    }
    if (window < 1) window = 1;

    arena_t arena;
    if (!arena_init(&arena, DEFAULT_CHUNK_SIZE)) {
        fprintf(stderr, "fatal: could not reserve memory pool\n");
        return 1;
    }

    csv_reader_t reader;
    if (!csv_reader_open(&reader, path, &arena, DEFAULT_CHUNK_SIZE)) {
        fprintf(stderr, "fatal: could not open '%s'\n", path);
        arena_destroy(&arena);
        return 1;
    }

    double *window_buf = (double *)arena_alloc(&arena, window * sizeof(double));
    if (!window_buf) {
        fprintf(stderr, "fatal: could not allocate sliding window buffer\n");
        csv_reader_close(&reader);
        arena_destroy(&arena);
        return 1;
    }

    running_stats_t  overall;
    sliding_window_t win;
    running_stats_init(&overall);
    sliding_window_init(&win, window_buf, window);

    uint64_t rows_parsed = 0;
    uint64_t rows_skipped_unparsable = 0;
    uint64_t rows_seen = 0;

    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    csv_span_t line;
    while (csv_reader_next_line(&reader, &line)) {
        rows_seen++;
        if (has_header && rows_seen == 1) continue;
        if (line.len == 0) continue;

        csv_span_t field;
        if (!csv_field_at(line, delimiter, column, &field)) {
            rows_skipped_unparsable++;
            continue;
        }

        double value;
        if (!csv_parse_double(field, &value)) {
            rows_skipped_unparsable++;
            continue;
        }

        running_stats_update(&overall, value);
        sliding_window_push(&win, value);
        rows_parsed++;
    }

    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed = wall_seconds(t_start, t_end);
    double mb_read = (double)reader.bytes_read_total / (1024.0 * 1024.0);

    printf("==== StreamStat Engine — pure C, zero dependencies ====\n");
    printf("file                 : %s\n", path);
    printf("column (1-based)     : %d\n", column);
    printf("delimiter            : '%c'\n", delimiter);
    printf("window size          : %zu\n", window);
    printf("---------------------------------------------------------\n");
    printf("bytes read           : %.2f MB\n", mb_read);
    printf("lines read           : %llu\n", (unsigned long long)reader.lines_read_total);
    printf("rows parsed          : %llu\n", (unsigned long long)rows_parsed);
    printf("rows skipped (bad)   : %llu\n", (unsigned long long)rows_skipped_unparsable);
    printf("elapsed time         : %.4f s\n", elapsed);
    if (elapsed > 0.0) {
        printf("throughput           : %.2f MB/s\n", mb_read / elapsed);
        printf("throughput           : %.0f rows/s\n", (double)rows_parsed / elapsed);
    }
    printf("---------------------------------------------------------\n");
    printf("overall mean         : %.10g\n", running_stats_mean(&overall));
    printf("overall stddev (pop) : %.10g\n", running_stats_stddev(&overall));
    printf("overall variance     : %.10g\n", running_stats_population_variance(&overall));
    printf("---------------------------------------------------------\n");
    printf("moving average (last %zu values, %zu collected)\n", window, win.count);
    printf("moving average       : %.10g\n", sliding_window_mean(&win));
    printf("moving stddev (pop)  : %.10g\n", sliding_window_stddev(&win));
    printf("---------------------------------------------------------\n");
    printf("memory pool          : %zu bytes reserved from OS, %zu bytes handed out\n",
           arena.total_reserved, arena.total_allocated);
    printf("=========================================================\n");

    csv_reader_close(&reader);
    arena_destroy(&arena);

    return 0;
}
