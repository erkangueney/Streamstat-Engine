/* StreamStat Engine — Copyright (c) 2026 Erkan <erkantahaguney@gmail.com>. See LICENSE. */
#include "stats.h"

/* Hand-rolled sqrt (Newton-Raphson) so the engine doesn't need to link
 * against libm just for one function. Converges quadratically; a dozen
 * iterations is massive overkill for double precision but the cost is
 * paid once per summary print, never in the hot per-row loop. */
static double my_sqrt(double x) {
    if (x <= 0.0) return 0.0;
    double guess = x;
    /* A decent starting point matters for convergence speed, not
     * correctness — bit-twiddling tricks are unnecessary here since this
     * is called a handful of times total. */
    if (guess > 1.0) {
        guess = x / 2.0;
        if (guess < 1.0) guess = 1.0;
    }
    for (int i = 0; i < 60; i++) {
        double next = 0.5 * (guess + x / guess);
        if (next == guess) break;
        guess = next;
    }
    return guess;
}

void running_stats_init(running_stats_t *s) {
    s->count = 0;
    s->mean  = 0.0;
    s->m2    = 0.0;
}

void running_stats_update(running_stats_t *s, double x) {
    s->count++;
    double delta = x - s->mean;
    s->mean += delta / (double)s->count;
    double delta2 = x - s->mean;
    s->m2 += delta * delta2;
}

double running_stats_mean(const running_stats_t *s) {
    return s->mean;
}

double running_stats_population_variance(const running_stats_t *s) {
    if (s->count == 0) return 0.0;
    return s->m2 / (double)s->count;
}

double running_stats_sample_variance(const running_stats_t *s) {
    if (s->count < 2) return 0.0;
    return s->m2 / (double)(s->count - 1);
}

double running_stats_stddev(const running_stats_t *s) {
    return my_sqrt(running_stats_population_variance(s));
}

void sliding_window_init(sliding_window_t *w, double *buf, size_t capacity) {
    w->buf      = buf;
    w->capacity = capacity;
    w->count    = 0;
    w->head     = 0;
    w->sum      = 0.0;
    w->sum_sq   = 0.0;
}

void sliding_window_push(sliding_window_t *w, double x) {
    if (w->count == w->capacity) {
        double evicted = w->buf[w->head];
        w->sum    -= evicted;
        w->sum_sq -= evicted * evicted;
    } else {
        w->count++;
    }

    w->buf[w->head] = x;
    w->sum    += x;
    w->sum_sq += x * x;

    w->head++;
    if (w->head == w->capacity) w->head = 0;
}

double sliding_window_mean(const sliding_window_t *w) {
    if (w->count == 0) return 0.0;
    return w->sum / (double)w->count;
}

double sliding_window_stddev(const sliding_window_t *w) {
    if (w->count == 0) return 0.0;
    double mean = sliding_window_mean(w);
    double variance = (w->sum_sq / (double)w->count) - (mean * mean);
    if (variance < 0.0) variance = 0.0; /* guard against fp cancellation */
    return my_sqrt(variance);
}
