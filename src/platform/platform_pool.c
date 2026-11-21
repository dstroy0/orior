// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_pool.c: the memory of a program as the prison site of MMgr. One cellblock lies over a pool of pages that the
// system gives at the process stage. The owner table of the program comes from owners.h as data, and the movable option
// has a mover thread of its own. The block of fixed parts lies beside the pool, and the line of the output stream is
// one of its parts. No part of the block comes from the cells, and so no part makes a hole in the persistent tier
#include "platform_internal.h"

// The pool. Its bytes come from the system at the process stage, outside the image of the program, and the record of
// the cellblock lies over them. Minimum security: a release leaves the bytes of a cell as they are
ParsMemoriaeHospes(platform_pool);
LocusCarcerum(platform_site, MMGR_MINIMUM_SECURITY_HOSPES(platform_pool));

// the owners of the program, the table and the pointer of each owner into it
MMGR_OWNERS(PLATFORM_OWNERS)

const MinimumSecurityGuard *const g_platform_guard = &platform_site.platform_pool;

// the guard that exact_types takes its cells through, which that library leaves to the program
const MinimumSecurityGuard *const g_exact_guard = &platform_site.platform_pool;

// the lanes that the storage of the movable option holds turnstiles for, and the rows of touches that it saves
#define PLATFORM_WIDTH_MAX 256u
#define PLATFORM_TOUCH_ROWS 64u

// the storage of the movable option, at an address that the linker settles
ParsMemoriaeInternae(platform_movable,
                     MMGR_MOVABLE_STORAGE_BYTES(PLATFORM_POOL_MOVABLE, PLATFORM_WIDTH_MAX, PLATFORM_TOUCH_ROWS));

// the bytes of the pool, 0 before the process stage
static unsigned long long s_pool_bytes;

// The fixed parts of the block, one after the next, each part on a boundary of PLATFORM_PARTS_ALIGN bytes. The line of
// the output stream is the single part
#define PLATFORM_PARTS_ALIGN 64ULL
#define PLATFORM_PARTS_ALIGNED(bytes_)                                                                                 \
    ((((unsigned long long)(bytes_)) + PLATFORM_PARTS_ALIGN - 1ULL) & ~(PLATFORM_PARTS_ALIGN - 1ULL))
#define PLATFORM_LINE_BYTES (64ULL << 10u)
#define PLATFORM_BLOCK_BYTES (PLATFORM_PARTS_ALIGNED(PLATFORM_LINE_BYTES))
EMBED_ALIGN(PLATFORM_PARTS_ALIGN) static char s_block[PLATFORM_BLOCK_BYTES];

// the line of the output stream, over its part of the block
static VerbaLinea s_line_out;

// the clock that badges a take of a cell, the clock of the host
embed_u64 mmgr_carcer_hw_clock_ns(void)
{
    return (embed_u64)embed_clock_ns();
}

// the places of the threads: each thread takes the next count on its first call, and keeps it for its life. MMgr reads
// the place modulo its regions, and so no count passes what it reads
static embed_atomic_u32 s_places_given = EMBED_ATOMIC_INIT(0u);
static _Thread_local embed_u32 t_place_plus_one;

embed_u32 embed_thread_place(void)
{
    if (t_place_plus_one == 0u)
    {
        t_place_plus_one = embed_atomic_u32_fetch_add(&s_places_given, 1u) + 1u;
    }
    return t_place_plus_one - 1u;
}

// The mover as MMgr gives it to its thread: what it runs and on what. The word `s_mover_wake` moves on each wake, and
// the mover sleeps on it between its readings. `s_mover_ending` asks the mover to end, and `s_mover_ended` says that it
// ended
static void (*s_mover_run)(void *context);
static void *s_mover_context;
static embed_atomic_u32 s_mover_ending = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_mover_ended = EMBED_ATOMIC_INIT(0u);
static embed_atomic_word s_mover_wake = EMBED_ATOMIC_INIT(0u);

static void platform_mover_woken(void)
{
    (void)embed_atomic_word_fetch_add(&s_mover_wake, 1u);
    embed_word_wake(&s_mover_wake);
}

static void platform_mover_entered(void *context)
{
    (void)context;
    s_mover_run(s_mover_context);
    EMBED_ATOMIC_STORE(&s_mover_ended, 1u);
    platform_mover_woken();
}

static embed_bool platform_mover_started(void (*run)(void *context), void *context)
{
    s_mover_run = run;
    s_mover_context = context;
    EMBED_ATOMIC_STORE(&s_mover_ending, 0u);
    EMBED_ATOMIC_STORE(&s_mover_ended, 0u);
    const int started = platform_thread_started(platform_mover_entered, NULL);
    // a thread that did not start leaves no mover to end
    s_mover_run = started ? run : NULL;
    return (embed_bool)started;
}

static embed_bool platform_mover_waited(uint32_t milliseconds)
{
    if (EMBED_ATOMIC_LOAD(&s_mover_ending) != 0u)
    {
        return EMBED_FALSE;
    }
    const embed_atomic_word_value seen = embed_atomic_word_load(&s_mover_wake);
    (void)embed_word_wait(&s_mover_wake, seen, (embed_word)milliseconds * (embed_word)1000000u);
    return (embed_bool)(EMBED_ATOMIC_LOAD(&s_mover_ending) == 0u);
}

// the id of the thread that calls, in the width that MMgr badges a pass with
static size_t platform_mover_thread_id(void)
{
    return (size_t)platform_thread_id();
}

// the mover asked to end, woken, and waited for
static void platform_mover_ended(void)
{
    if (s_mover_run == NULL)
    {
        return;
    }
    EMBED_ATOMIC_STORE(&s_mover_ending, 1u);
    while (EMBED_ATOMIC_LOAD(&s_mover_ended) == 0u)
    {
        platform_mover_woken();
        platform_thread_yield();
    }
    s_mover_run = NULL;
}

static int platform_line_written(void *context, const char *bytes, size_t size)
{
    (void)context;
    platform_terminal_write(0, bytes, (unsigned long long)size);
    return 1;
}

int platform_open(unsigned long long bytes)
{
    if (s_pool_bytes != 0ULL)
    {
        return 1;
    }
    const unsigned long long asked = (bytes < PLATFORM_POOL_BYTES_LEAST) ? PLATFORM_POOL_BYTES_LEAST : bytes;
    // Explicit cast takes the pages that the system gave as the bytes that the cellblock lies on
    uint8_t *const storage = (uint8_t *)platform_pages_mapped(asked);
    if ((storage == NULL) || !mmgr_series_cellularum_placed(&platform_site_platform_pool_ctx, storage, (size_t)asked))
    {
        if (storage != NULL)
        {
            platform_pages_released(storage, asked);
        }
        return 0;
    }
    s_pool_bytes = asked;
    const unsigned int processors = platform_processors();
    // a refusal is a mover thread that did not start, and the pool then works with no mover
    (void)g_platform_guard->movable_init(
        &(CarcerMovableInitCfg){.storage = mmgr_pars_storage_platform_movable,
                                .storage_bytes = platform_movable_bytes,
                                .cells = PLATFORM_POOL_MOVABLE,
                                .width = (processors < PLATFORM_WIDTH_MAX) ? processors : PLATFORM_WIDTH_MAX,
                                .rows = PLATFORM_TOUCH_ROWS,
                                .thread_started = platform_mover_started,
                                .thread_id = platform_mover_thread_id,
                                .thread_wait = platform_mover_waited,
                                .thread_yield = platform_thread_yield,
                                .thread_moving = NULL});
    s_line_out = (VerbaLinea){s_block, (size_t)PLATFORM_LINE_BYTES, 0u, platform_line_written, NULL, 0u};
    return 1;
}

void platform_close(void)
{
    if (s_pool_bytes == 0ULL)
    {
        return;
    }
    platform_line_flushed();
    platform_mover_ended();
    platform_pages_released(platform_site_platform_pool_ctx.base, s_pool_bytes);
    s_pool_bytes = 0ULL;
}

VerbaLinea *platform_line_out(void)
{
    return &s_line_out;
}

void platform_line_flushed(void)
{
    if (s_line_out.at != 0u)
    {
        (void)mmgr_line_flush(&s_line_out);
    }
}
