# StreamStat Engine — Pure C CSV Parser & Statistics Engine

A dependency-free (no libc-external, no third-party) C program that parses
multi-gigabyte numeric time-series CSV files and computes, in a single
streaming pass, the overall mean/standard deviation of a chosen column plus
a windowed moving average and moving standard deviation — written to satisfy
a take-home technical assessment (see `docs/gorev.md` for the original brief; company-identifying details are omitted from this README by request).

## What it does

```
./streamstat <file.csv> <column_1based> [window_size] [delimiter] [--header]
```

Example:

```
./streamstat data/sensor_log.csv 3 500 , --header
```

reads column 3 of a comma-separated file (skipping the header row), and
reports the overall mean/stddev of that column together with the moving
average/stddev over the trailing 500 values.

## Design decisions (mapped to the assignment's requirements)

**"Hiçbir dış kütüphane kullanma" (no third-party library).**
The only calls into the outside world are the POSIX syscalls `open`,
`read`, `close` (unavoidable — something has to ask the kernel for file
bytes) and `mmap`/`malloc` once at startup to reserve the memory pool. No
`strtod`/`atof`, no `memcpy`/`memmove` from `<string.h>`, no `sqrt` from
`<math.h>`, no database, no cloud SDK, no Pandas/NumPy-equivalent. Every
one of those was reimplemented by hand (see below) so the whole data path,
from disk to statistic, is code in this repository.

**"CPU cache'i nasıl yöneteceğini bilmek" (manage CPU cache behavior).**
The engine never materializes the file, a line, or a field as a heap
object. Lines are handed to the caller as `(pointer, length)` spans that
point directly into an 8 MiB I/O buffer; the only "extra" memory that's
ever touched per row is the fixed-size sliding-window ring buffer and a
handful of accumulator doubles. That working set (a few dozen bytes) is
tiny enough to live in L1 cache regardless of whether the input file is
10 MB or 10 GB, so cache-miss rate — not allocation overhead — becomes the
only thing that can slow the loop down, and sequential access keeps the
hardware prefetcher fully effective.

**"Kendi string-to-float dönüşüm mantığı" (own string→float conversion).**
`csv_parse_double()` in `src/csv_parser.c` hand-parses sign, integer part,
fractional part, and optional exponent, accumulating the integer digits in
a 64-bit integer before converting to `double` (avoids the repeated
rounding you'd get from naive digit-by-digit float accumulation) and using
exponentiation-by-squaring for the exponent instead of `pow()`.

**"Bellek dostu okuma / kendi bellek havuzu" (memory-pool, chunked I/O).**
`include/arena.h` / `src/arena.c` implement a bump-pointer arena: a small
number of large blocks (`mmap`, falling back to `malloc`) are reserved up
front, and every allocation for the rest of the program's life is just
`base + used_offset` — no per-record `malloc`/`free`, hence no
fragmentation. `src/csv_parser.c`'s `csv_reader_t` reads the file in fixed
8 MiB blocks straight from that pool via `read(2)`; a line that straddles
two blocks has its unfinished tail carried to the front of the buffer
before the next read (`raw_memmove`, hand-written, not `<string.h>`'s
`memmove`).

**"Hareketli ortalama ve standart sapma" (moving average + stddev).**
Two independent, O(1)-per-row accumulators run in the same pass
(`src/stats.c`):
- `running_stats_t` — Welford's online algorithm for the *entire* column's
  mean/variance. Single pass, numerically stable even over billions of
  rows (no catastrophic cancellation from a naive `Σx²` accumulator).
- `sliding_window_t` — a fixed-capacity ring buffer keeping a running
  `sum` and `sum_sq` over the last N values, giving O(1) moving average
  and moving standard deviation per row. This is the classic sliding-window
  sum/sum-of-squares technique; it trades a small amount of numerical
  precision (vs. a windowed Welford) for simplicity, which is a reasonable
  choice for a bounded window and is called out here explicitly rather than
  left as a silent trade-off.
- `sqrt()` itself is hand-rolled with Newton–Raphson (`my_sqrt` in
  `src/stats.c`) so the binary doesn't need to link `-lm` for one function.

## Layout

```
include/        public headers (arena, csv_parser, stats)
src/            implementation + CLI driver (main.c)
tools/          gen_test_csv.c — synthetic CSV generator for benchmarking
tests/          tests/sample.csv — tiny, hand-verified correctness fixture
Makefile        build / test / gen_test_data targets
```

## Building

```
make            # builds ./streamstat  (cc -std=c11 -O3, no external libs)
make test       # runs the engine against tests/sample.csv
make gen_test_data
./tools/gen_test_csv data/bench.csv 250000000   # ≈10 GB synthetic file
./streamstat data/bench.csv 2 1000 , --header
```

## Correctness

`tests/sample.csv` has 10 hand-picked rows. The expected overall
mean/stddev and window(=3) moving average/stddev were computed by hand and
cross-checked with a plain Python reference; `make test` reproduces them
exactly:

```
overall mean         : 14.5
overall stddev (pop) : 2.872281323
moving average (w=3) : 18
moving stddev (w=3)  : 0.8164965809
```

At a larger scale, a 5,000,000-row (≈205 MB) synthetic file was
cross-checked against an independent Python (`csv` + pure-Python
mean/variance) computation of the same column; every reported figure
(mean, population stddev, trailing-1000-row moving average/stddev) matched
to full displayed precision.

## Performance (this sandbox's shared VM — expect a real machine to do
## better, especially with a real disk vs. this container's filesystem)

On a 205 MB / 5,000,000-row file, single run:

```
throughput : ~535 MB/s
throughput : ~13.0M rows/s
```

Because the working set per row is a few accumulator doubles plus a bump
pointer, throughput here is essentially bounded by the speed at which the
OS can hand over pages via `read(2)`, not by the parsing/statistics code —
which is the point of avoiding per-row allocation and per-row string
processing overhead in the first place.

## Authorship / anti-plagiarism measures

This project embeds a provenance fingerprint so authorship can be proven
even if the code or binary is copied and resubmitted by someone else:

- `include/provenance.h` defines the author's name/email and a random
  per-project UUID (`4c128310-5fab-438f-8f65-4137092a8976`), compiled
  directly into the binary.
- `./streamstat --about` prints that fingerprint; it is also recoverable
  from a plain `strings ./streamstat` even without the flag, so it
  survives a binary-only copy, not just a source copy.
- `PROVENANCE.md` records a SHA-256 hash of the full source tree at the
  time of first submission plus the initial git commit hash/timestamp —
  a way to show *when* this exact code existed, independent of any later
  copy's claimed date.
- `LICENSE` states in plain terms that copying this code (or its ideas'
  packaging) under another name is not permitted.

None of this prevents someone from reading or evaluating the code — the
goal is a paper trail, not obfuscation.

## Honest limitations / what a production version would add next

- Only plain (non-quoted, no embedded delimiters/newlines) CSV fields are
  supported; RFC 4180 quoting is not implemented. Given the assignment's
  framing (raw numeric time-series logs), this was a deliberate scope cut,
  not an oversight — adding a quote-aware field scanner is a small,
  additive change to `csv_field_at`.
- Single-threaded. The chunked design maps cleanly onto multiple worker
  threads each owning a byte-range of the file (with a small amount of
  care at chunk boundaries for line alignment), which would be the next
  step to push past single-core I/O/parsing throughput.
- `sliding_window_stddev` guards against negative variance from floating
  point cancellation but does not periodically "re-sum" the window to
  correct long-run drift; for extremely long runs with a small window and
  extreme value magnitudes, an occasional resync pass would be the more
  rigorous fix.
