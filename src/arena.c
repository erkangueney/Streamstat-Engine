/* StreamStat Engine — Copyright (c) 2026 Erkan <erkantahaguney@gmail.com>. See LICENSE. */
#include "arena.h"

#include <stdlib.h>

#if defined(__unix__) || defined(__APPLE__)
#  include <sys/mman.h>
#  define ARENA_HAVE_MMAP 1
#else
#  define ARENA_HAVE_MMAP 0
#endif

static size_t align_up(size_t n, size_t align) {
    return (n + (align - 1)) & ~(align - 1);
}

static arena_block_t *arena_new_block(size_t size) {
    /* The block header itself is allocated with a single malloc — this is
     * a one-time, O(number_of_blocks) cost, not a per-record cost, so it
     * does not violate the "no fragmentation from the hot path" rule. */
    arena_block_t *blk = (arena_block_t *)malloc(sizeof(arena_block_t));
    if (!blk) return NULL;

    void *mem = NULL;
    int used_mmap = 0;

#if ARENA_HAVE_MMAP
    mem = mmap(NULL, size, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        mem = NULL;
    } else {
        used_mmap = 1;
    }
#endif

    if (!mem) {
        mem = malloc(size);
        used_mmap = 0;
    }

    if (!mem) {
        free(blk);
        return NULL;
    }

    blk->base    = (uint8_t *)mem;
    blk->size    = size;
    blk->used    = 0;
    blk->is_mmap = used_mmap;
    blk->next    = NULL;
    return blk;
}

int arena_init(arena_t *arena, size_t default_block_size) {
    arena->default_block_size = default_block_size;
    arena->total_allocated    = 0;
    arena->total_reserved     = 0;

    arena_block_t *blk = arena_new_block(default_block_size);
    if (!blk) return 0;

    arena->head = blk;
    arena->total_reserved += blk->size;
    return 1;
}

void *arena_alloc(arena_t *arena, size_t size) {
    size_t aligned = align_up(size, 16);

    arena_block_t *blk = arena->head;
    if (blk->used + aligned > blk->size) {
        /* Current block is full — grow the pool with a fresh block big
         * enough for this request (rounded up to at least the default
         * block size so tiny allocations don't cause a storm of tiny
         * mmap calls). */
        size_t new_size = arena->default_block_size;
        if (aligned > new_size) new_size = aligned;

        arena_block_t *nb = arena_new_block(new_size);
        if (!nb) return NULL;

        nb->next    = arena->head;
        arena->head = nb;
        arena->total_reserved += nb->size;
        blk = nb;
    }

    void *ptr = blk->base + blk->used;
    blk->used += aligned;
    arena->total_allocated += aligned;
    return ptr;
}

void arena_destroy(arena_t *arena) {
    arena_block_t *blk = arena->head;
    while (blk) {
        arena_block_t *next = blk->next;
#if ARENA_HAVE_MMAP
        if (blk->is_mmap) {
            munmap(blk->base, blk->size);
        } else {
            free(blk->base);
        }
#else
        free(blk->base);
#endif
        free(blk);
        blk = next;
    }
    arena->head = NULL;
}
