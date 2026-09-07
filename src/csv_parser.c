/* StreamStat Engine — Copyright (c) 2026 Erkan <erkantahaguney@gmail.com>. See LICENSE. */
#include "csv_parser.h"

#include <fcntl.h>
#include <unistd.h>

/* ---- hand-rolled replacements for the string.h helpers we'd otherwise
 * reach for; kept tiny and branch-predictable since they run on every
 * byte of a multi-gigabyte file. ---- */

static void raw_memmove(uint8_t *dst, const uint8_t *src, size_t n) {
    if (dst == src || n == 0) return;
    if (dst < src) {
        for (size_t i = 0; i < n; i++) dst[i] = src[i];
    } else {
        for (size_t i = n; i > 0; i--) dst[i - 1] = src[i - 1];
    }
}

int csv_reader_open(csv_reader_t *r, const char *path, arena_t *arena,
                     size_t chunk_size) {
    r->fd = open(path, O_RDONLY);
    if (r->fd < 0) return 0;

    r->buf = (uint8_t *)arena_alloc(arena, chunk_size);
    if (!r->buf) {
        close(r->fd);
        return 0;
    }

    r->buf_capacity     = chunk_size;
    r->buf_len          = 0;
    r->pos              = 0;
    r->eof_reached      = 0;
    r->bytes_read_total = 0;
    r->lines_read_total = 0;
    return 1;
}

/* Refill the buffer: slide any unconsumed tail to the front, then read()
 * as much as fits after it. Returns the number of *new* bytes appended
 * (0 means the underlying file is exhausted). */
static size_t csv_reader_refill(csv_reader_t *r) {
    size_t leftover = r->buf_len - r->pos;
    if (leftover > 0) {
        raw_memmove(r->buf, r->buf + r->pos, leftover);
    }
    r->pos     = 0;
    r->buf_len = leftover;

    if (r->eof_reached) return 0;

    size_t space = r->buf_capacity - r->buf_len;
    if (space == 0) {
        /* A single "line" is larger than the whole chunk buffer. This
         * engine is built for numeric time-series rows, not pathological
         * inputs, so we treat this as EOF-of-usable-data rather than
         * silently growing without bound. */
        r->eof_reached = 1;
        return 0;
    }

    ssize_t n = read(r->fd, r->buf + r->buf_len, space);
    if (n <= 0) {
        r->eof_reached = 1;
        return 0;
    }

    r->buf_len          += (size_t)n;
    r->bytes_read_total += (uint64_t)n;
    return (size_t)n;
}

int csv_reader_next_line(csv_reader_t *r, csv_span_t *out) {
    for (;;) {
        /* Scan forward from pos for '\n' within the currently buffered
         * bytes. Manual byte loop rather than memchr — this keeps the
         * whole reader free of libc's string routines, per the
         * assignment's "own the byte path" requirement. */
        size_t i = r->pos;
        while (i < r->buf_len && r->buf[i] != '\n') i++;

        if (i < r->buf_len) {
            /* Found a full line in [pos, i). */
            size_t start = r->pos;
            size_t end   = i; /* exclusive, points at '\n' */
            if (end > start && r->buf[end - 1] == '\r') end--; /* CRLF */

            out->ptr = (const char *)(r->buf + start);
            out->len = end - start;
            r->pos   = i + 1; /* skip past '\n' */
            r->lines_read_total++;
            return 1;
        }

        /* No newline in what's buffered — need more data. */
        size_t appended = csv_reader_refill(r);
        if (appended == 0) {
            /* True EOF: flush a final unterminated line, if any. */
            if (r->pos < r->buf_len) {
                size_t start = r->pos;
                size_t end   = r->buf_len;
                if (end > start && r->buf[end - 1] == '\r') end--;
                out->ptr = (const char *)(r->buf + start);
                out->len = end - start;
                r->pos   = r->buf_len;
                r->lines_read_total++;
                return 1;
            }
            return 0;
        }
        /* Loop again now that the buffer has more bytes. */
    }
}

void csv_reader_close(csv_reader_t *r) {
    if (r->fd >= 0) close(r->fd);
    r->fd = -1;
}

int csv_field_at(csv_span_t line, char delimiter, int column_1based,
                  csv_span_t *out) {
    if (column_1based < 1) return 0;

    int field_index = 1;
    size_t field_start = 0;

    for (size_t i = 0; i <= line.len; i++) {
        int at_delim = (i == line.len) || (line.ptr[i] == delimiter);
        if (!at_delim) continue;

        if (field_index == column_1based) {
            out->ptr = line.ptr + field_start;
            out->len = i - field_start;
            return 1;
        }
        field_index++;
        field_start = i + 1;
    }
    return 0;
}

int csv_parse_double(csv_span_t s, double *out) {
    const char *p   = s.ptr;
    const char *end = s.ptr + s.len;
    *out = 0.0;

    while (p < end && (*p == ' ' || *p == '\t')) p++;
    if (p >= end) return 0;

    int sign = 1;
    if (*p == '+' || *p == '-') {
        if (*p == '-') sign = -1;
        p++;
    }

    int      saw_digit    = 0;
    uint64_t int_part     = 0;  /* accumulate as integer to avoid repeated
                                    fp multiply/add rounding on the
                                    integer digits */
    while (p < end && *p >= '0' && *p <= '9') {
        int_part = int_part * 10u + (uint64_t)(*p - '0');
        saw_digit = 1;
        p++;
    }

    double value = (double)int_part;

    if (p < end && *p == '.') {
        p++;
        double frac_scale = 0.1;
        while (p < end && *p >= '0' && *p <= '9') {
            value += (double)(*p - '0') * frac_scale;
            frac_scale *= 0.1;
            saw_digit = 1;
            p++;
        }
    }

    if (!saw_digit) return 0;

    if (p < end && (*p == 'e' || *p == 'E')) {
        const char *save = p;
        p++;
        int exp_sign = 1;
        if (p < end && (*p == '+' || *p == '-')) {
            if (*p == '-') exp_sign = -1;
            p++;
        }
        int exp_val = 0;
        int saw_exp_digit = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            exp_val = exp_val * 10 + (*p - '0');
            saw_exp_digit = 1;
            p++;
        }
        if (saw_exp_digit) {
            double mult = 1.0;
            double base = 10.0;
            int e = exp_val;
            /* fast exponentiation by squaring, no math.h pow() */
            while (e > 0) {
                if (e & 1) mult *= base;
                base *= base;
                e >>= 1;
            }
            value = (exp_sign > 0) ? value * mult : value / mult;
        } else {
            /* 'e' wasn't actually followed by an exponent; ignore it and
             * rewind (leaves trailing garbage, which callers ignore since
             * we've already got the numeric prefix). */
            p = save;
        }
    }

    *out = sign < 0 ? -value : value;
    return 1;
}
