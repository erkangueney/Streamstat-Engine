/* StreamStat Engine — Copyright (c) 2026 Erkan <erkantahaguney@gmail.com>. See LICENSE. */
#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

/*
 * arena.h — bump-pointer memory pool.
 *
 * Rationale: the assignment explicitly forbids fragmenting the heap with
 * repeated malloc()/free() calls while streaming a multi-gigabyte file.
 * Instead we grab a small number of large blocks up front (via mmap when
 * available, falling back to malloc) and hand out memory with a simple
 * bump pointer. Nothing inside the hot parsing loop ever calls malloc.
 *
 * A block is only released when the whole arena is destroyed, so there is
 * no per-allocation bookkeeping, no free-list, no fragmentation, and the
 * allocation itself is a handful of instructions (compare + add).
 */

typedef struct arena_block {
    struct arena_block *next;
    uint8_t            *base;      /* start of usable memory in this block   */
    size_t              size;      /* total usable bytes in this block       */
    size_t              used;      /* bump offset                            */
    int                 is_mmap;   /* 1 if base was obtained via mmap        */
} arena_block_t;

typedef struct {
    arena_block_t *head;       /* most recently allocated block (bump target) */
    size_t         default_block_size;
    size_t         total_allocated;    /* bytes handed out to callers         */
    size_t         total_reserved;     /* bytes actually reserved from OS     */
} arena_t;

/* Create an arena. default_block_size is the size of each backing block
 * (e.g. 8 * 1024 * 1024 for 8 MiB chunks). Returns 1 on success, 0 on
 * failure. */
int  arena_init(arena_t *arena, size_t default_block_size);

/* Bump-allocate `size` bytes, 16-byte aligned. Grows the arena with a new
 * block (sized to fit `size` if it exceeds default_block_size) on demand.
 * Returns NULL only if the OS refuses to give us more memory at all. */
void *arena_alloc(arena_t *arena, size_t size);

/* Release every block back to the OS. */
void arena_destroy(arena_t *arena);

#endif /* ARENA_H */
