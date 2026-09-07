/* StreamStat Engine — Copyright (c) 2026 Erkan <erkantahaguney@gmail.com>. See LICENSE. */
#ifndef STATS_H
#define STATS_H

#include <stddef.h>

/*
 * stats.h — streaming statistics, O(1) memory regardless of file size.
 *
 * Two independent accumulators are kept:
 *
 *   1. running_stats_t — Welford's online algorithm for the mean and
 *      variance of the *entire* column, single pass, numerically stable
 *      (no giant sum-of-squares cancellation error even over billions of
 *      rows).
 *
 *   2. sliding_window_t — a fixed-size ring buffer that gives an O(1)
 *      update moving average and moving standard deviation over the last
 *      N values. This is the classic sum/sum-of-squares sliding window;
 *      it is O(1) per row and O(window) memory, not O(1) per row and
 *      perfectly cancellation-free like Welford, and that trade-off is
 *      intentional and documented in the README — for a bounded window
 *      the accumulated error is negligible in practice.
 */

typedef struct {
    unsigned long long count;
    double mean;
    double m2;      /* sum of squared differences from the mean */
} running_stats_t;

void   running_stats_init(running_stats_t *s);
void   running_stats_update(running_stats_t *s, double x);
double running_stats_mean(const running_stats_t *s);
double running_stats_population_variance(const running_stats_t *s);
double running_stats_sample_variance(const running_stats_t *s);
double running_stats_stddev(const running_stats_t *s); /* population stddev */

typedef struct {
    double *buf;        /* ring buffer, arena-allocated, length = capacity */
    size_t  capacity;
    size_t  count;       /* number of valid entries so far (<= capacity)   */
    size_t  head;        /* index the *next* value will be written to     */
    double  sum;
    double  sum_sq;
} sliding_window_t;

/* buf must point at `capacity` doubles of caller-owned (arena) memory. */
void   sliding_window_init(sliding_window_t *w, double *buf, size_t capacity);
void   sliding_window_push(sliding_window_t *w, double x);
double sliding_window_mean(const sliding_window_t *w);
double sliding_window_stddev(const sliding_window_t *w); /* population stddev */

#endif /* STATS_H */
