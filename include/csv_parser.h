/* StreamStat Engine — Copyright (c) 2026 Erkan <erkantahaguney@gmail.com>. See LICENSE. */
#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <stddef.h>
#include <stdint.h>

#include "arena.h"

/*
 * csv_parser.h — zero-copy, chunked CSV reader.
 *
 * Design goals dictated by the assignment:
 *   1. No fread()/fgets() buffering games and no libc string helpers in
 *      the hot path — we read raw bytes with the POSIX read(2) syscall
 *      directly into an arena-backed buffer.
 *   2. The file is consumed in large sequential blocks (default 8 MiB),
 *      which keeps the number of syscalls tiny (a 10 GB file needs ~1250
 *      read() calls at 8 MiB/chunk) and lets the OS read-ahead/prefetcher
 *      do its job — this is what actually saturates disk bandwidth.
 *   3. Lines are handed back as (pointer, length) spans *inside* the
 *      buffer — no per-line allocation, no copying, so the whole pipeline
 *      after the syscall is just pointer arithmetic that fits in L1/L2
 *      cache regardless of file size.
 *   4. A line that straddles the boundary between two reads is handled by
 *      carrying the unfinished tail to the front of the buffer before the
 *      next read() — the classic "carry-over" technique for chunked
 *      parsing.
 */

typedef struct {
    const char *ptr;
    size_t      len;
} csv_span_t;

typedef struct {
    int      fd;
    uint8_t *buf;         /* arena-allocated I/O buffer                    */
    size_t   buf_capacity;
    size_t   buf_len;     /* valid bytes currently in buf                  */
    size_t   pos;         /* current read cursor within buf                */
    int      eof_reached; /* underlying file exhausted                     */

    /* stats, useful for reporting throughput */
    uint64_t bytes_read_total;
    uint64_t lines_read_total;
} csv_reader_t;

/* Open `path` and prepare a chunked reader whose I/O buffer (of
 * `chunk_size` bytes) is allocated from `arena`. Returns 1 on success. */
int csv_reader_open(csv_reader_t *r, const char *path, arena_t *arena,
                     size_t chunk_size);

/* Fetch the next line as a zero-copy span into the internal buffer.
 * Returns 1 and fills *out on success, 0 at EOF. The trailing '\n' (and
 * '\r' if present) is stripped from the span. The pointer in *out is only
 * valid until the next call to csv_reader_next_line(). */
int csv_reader_next_line(csv_reader_t *r, csv_span_t *out);

void csv_reader_close(csv_reader_t *r);

/* Locate the Nth (1-based) delimiter-separated field within `line` and
 * return it as a span, without splitting/allocating the rest of the line.
 * Returns 1 if the column exists, 0 otherwise. */
int csv_field_at(csv_span_t line, char delimiter, int column_1based,
                  csv_span_t *out);

/* Hand-rolled string -> double conversion (no strtod/atof). Supports an
 * optional leading sign, integer part, fractional part, and optional
 * exponent (e/E with optional sign). Returns 1 on success and writes the
 * parsed value to *out; returns 0 if the span contains no parseable
 * number (out is set to 0.0 in that case). */
int csv_parse_double(csv_span_t s, double *out);

#endif /* CSV_PARSER_H */
