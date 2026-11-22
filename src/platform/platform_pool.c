// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_pool.c: the memory of a program as the prison site of MMgr. One cellblock lies over a pool of pages that the
// system gives at the process stage. The owner table of the program comes from owners.h as data, and the movable option
// has a mover thread of its own. The block of fixed parts lies beside the pool, and the line of the output stream is
// one of its parts. No part of the block comes from the cells, and so no part makes a hole in the persistent tier
#include "platform_internal.h"

#include "memoria_operor/memoria_linea.h"
#include "memoria_operor/memoria_operor.h"

#include <signal.h>
#include <stdlib.h>

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

// 1 where the output stream is a terminal, which platform_open asks the system once
static int s_line_out_terminal;

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
        t_place_plus_one = EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg,
                                      .word = &s_places_given, .value = 1u) +
                           1u;
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
    (void)EMBED_CALL(embed_atomic_word_table.fetch_add, EmbedAtomicWordFetchAddCfg, .word = &s_mover_wake, .value = 1u);
    EMBED_CALL(embed_word_wake, EmbedWordWakeCfg, .word = &s_mover_wake);
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
    const embed_atomic_word_value seen =
        EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_mover_wake);
    (void)EMBED_CALL(embed_word_wait, EmbedWordWaitCfg, .word = &s_mover_wake, .old_value = seen,
                     .nanoseconds = (embed_word)milliseconds * (embed_word)1000000u);
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
    // the persistent tier gets the bytes asked for, and the regions of the temporary tier come off the top beside them
    const unsigned long long asked = ((bytes < PLATFORM_POOL_BYTES_LEAST) ? PLATFORM_POOL_BYTES_LEAST : bytes) +
                                     (unsigned long long)MMGR_CARCER_REGION_AREA;
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
    s_line_out_terminal = platform_terminal_is(0);
    // a program that calls exit before program_main returns skips platform_close, and its line goes out at exit
    (void)atexit(platform_line_flushed);
    return 1;
}

// the hooks that platform_close runs, in the order of their calls, and their count
static void (*s_close_hooks[PLATFORM_CLOSE_HOOKS])(void);
static unsigned int s_close_hook_count;

int platform_at_close(void (*hook)(void))
{
    if ((hook == NULL) || (s_close_hook_count >= PLATFORM_CLOSE_HOOKS))
    {
        return 0;
    }
    s_close_hooks[s_close_hook_count] = hook;
    s_close_hook_count += 1u;
    return 1;
}

void platform_close(void)
{
    if (s_pool_bytes == 0ULL)
    {
        return;
    }
    // the hooks run while the pool and the line are still there, the last one first. A hook that ends the process in
    // its turn finds no hook left to run twice
    while (s_close_hook_count > 0u)
    {
        s_close_hook_count -= 1u;
        s_close_hooks[s_close_hook_count]();
    }
    platform_line_flushed();
    platform_mover_ended();
    platform_pages_released(platform_site_platform_pool_ctx.base, s_pool_bytes);
    s_pool_bytes = 0ULL;
}

int platform_pool_placed(void)
{
    return s_pool_bytes != 0ULL;
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

void platform_line_out_ended(void)
{
    if (s_line_out_terminal != 0)
    {
        platform_line_flushed();
    }
}

void platform_line_out_every_write(void)
{
    s_line_out_terminal = 1;
}

unsigned long long platform_line_out_put(const char *bytes, unsigned long long length)
{
    // Explicit cast takes a length of bytes in memory, which a size_t counts
    const size_t taken = mmgr_line_put_n(&s_line_out, bytes, (size_t)length);
    platform_line_out_ended();
    return (unsigned long long)taken;
}

// a note on the error stream that the pool did not meet a cell. It names the bytes of the cell, its tier, and the bytes
// of the pool. A program that needs more defines g_program_pool_bytes
// MMgr tells the port of each give or growth that names an owner other than the cell's. The cell stays taken. The note
// names both owners, and a reader then finds the give in the wrong place
void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    EMBED_ALIGN(MMGR_ALIGN_BYTES) char text[192];
    VerbaLinea line = mmgr_line_over(text, sizeof(text));
    mmgr_line_put(&line, "  platform: a cell of ");
    mmgr_line_decimal(&line, bytes, 1u);
    mmgr_line_put(&line, " bytes went back as a cell of ");
    mmgr_line_put(&line, (named != NULL) ? named->name : "no owner");
    mmgr_line_put(&line, ", and it counts to ");
    mmgr_line_put(&line, (cell_owner != NULL) ? cell_owner->name : "no owner");
    mmgr_line_ch(&line, '\n');
    const size_t length = mmgr_line_finish(&line);
    platform_terminal_write(1, text, (unsigned long long)length);
}

// A note that the pool refused a cell of `bytes` on `tier` to `owner`. It gives the bytes between the two tiers, which
// tell a full pool from a sunk stack. It also gives the return addresses of the calls that asked
static void platform_cell_refused(unsigned long long bytes, int tier, const CellOwner *owner)
{
    static const char *const s_tier_names[3] = {" bytes on the persistent tier", " bytes on the temporary tier",
                                                " bytes as a movable cell"};
    EMBED_ALIGN(MMGR_ALIGN_BYTES) char text[512];
    VerbaLinea line = mmgr_line_over(text, sizeof(text));
    mmgr_line_put(&line, "  platform: the pool of ");
    mmgr_line_decimal(&line, s_pool_bytes, 1u);
    mmgr_line_put(&line, " bytes cannot meet a cell of ");
    mmgr_line_decimal(&line, bytes, 1u);
    mmgr_line_put(&line, s_tier_names[((tier >= 0) && (tier <= PLATFORM_TIER_MOVABLE)) ? tier : 0]);
    mmgr_line_put(&line, " for ");
    mmgr_line_put(&line, owner->name);
    mmgr_line_put(&line, ", with ");
    mmgr_line_decimal(&line, g_platform_guard->buf_available(), 1u);
    mmgr_line_put(&line, " bytes between the tiers; a program that needs more defines g_program_pool_bytes\n");
    mmgr_line_put(&line, "    asked from");
    platform_stack_put(&line);
    mmgr_line_ch(&line, '\n');
    const size_t length = mmgr_line_finish(&line);
    platform_terminal_write(1, text, (unsigned long long)length);
    // each owner that holds cells now, with the payload bytes of them
    EMBED_ALIGN(MMGR_ALIGN_BYTES) char held[1024];
    VerbaLinea held_line = mmgr_line_over(held, sizeof(held));
    mmgr_line_put(&held_line, "    held by");
    for (unsigned int place = 0u; place < (unsigned int)platform_place_count; place += 1u)
    {
        const size_t in_use =
            EMBED_CALL(embed_atomic_usize_table.load, EmbedAtomicUsizeLoadCfg, .word = &mmgr_owner_table[place].in_use);
        if (in_use != 0u)
        {
            mmgr_line_ch(&held_line, ' ');
            mmgr_line_put(&held_line, mmgr_owner_table[place].name);
            mmgr_line_ch(&held_line, '=');
            mmgr_line_decimal(&held_line, in_use, 1u);
        }
    }
    mmgr_line_ch(&held_line, '\n');
    const size_t held_length = mmgr_line_finish(&held_line);
    platform_terminal_write(1, held, (unsigned long long)held_length);
}

void *platform_cell_taken(unsigned long long bytes, unsigned int place, int ephemeral)
{
    if (place >= (unsigned int)platform_place_count)
    {
        return NULL;
    }
    const CellOwner *const owner = &mmgr_owner_table[place];
    void *const cell = ephemeral ? g_platform_guard->temporary_buf_alloc((size_t)bytes, owner)
                                 : g_platform_guard->persistent_buf_alloc((size_t)bytes, owner);
    if ((cell == NULL) && (bytes != 0ULL))
    {
        platform_cell_refused(bytes, (ephemeral != 0) ? PLATFORM_TIER_TEMPORARY : PLATFORM_TIER_PERSISTENT, owner);
    }
    return cell;
}

void *platform_cell_cleared(unsigned long long count, unsigned long long size, unsigned int place, int ephemeral)
{
    if ((size != 0ULL) && (count > (~0ULL / size)))
    {
        return NULL;
    }
    void *const cell = platform_cell_taken(count * size, place, ephemeral);
    if (cell != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = cell, .bytes = (size_t)(count * size), .val = 0u});
    }
    return cell;
}

void platform_cell_released(void *cell, unsigned int place)
{
    // platform_close gave the pages back, and every cell went with them. A destructor of a static that runs after
    // main returns gives back nothing
    if ((cell != NULL) && (place < (unsigned int)platform_place_count) && (s_pool_bytes != 0ULL))
    {
        g_platform_guard->persistent_buf_release(cell, &mmgr_owner_table[place]);
    }
}

// A cell grown in place where it is the newest of its tier, which leaves no hole behind it. Else a new cell takes the
// bytes, and the old cell goes back. A temporary cell grows down, and the bytes it contains then move down with it
void *platform_cell_grown(void *cell, unsigned long long held, unsigned long long bytes, unsigned int place,
                          int ephemeral)
{
    if ((cell != NULL) && (place < (unsigned int)platform_place_count))
    {
        void *const in_place = g_platform_guard->buf_grow(cell, (size_t)bytes, &mmgr_owner_table[place]);
        if ((in_place != NULL) && (in_place != cell) && (held != 0ULL))
        {
            mmgr_bytes_move(in_place, cell, (size_t)((held < bytes) ? held : bytes));
        }
        if (in_place != NULL)
        {
            return in_place;
        }
    }
    void *const grown = platform_cell_taken(bytes, place, ephemeral);
    if (grown == NULL)
    {
        return NULL;
    }
    if ((cell != NULL) && (held != 0ULL))
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = cell, .bytes = (size_t)((held < bytes) ? held : bytes)});
    }
    platform_cell_released(cell, place);
    return grown;
}

// the bytes that a text begins with. The flush doubles the cell toward its max as the writes come
#define PLATFORM_TEXT_FIRST (4ULL << 10u)

// the movable cell of a movable text, as the guard names it
static const CarcerMovableEntry *platform_text_movable(const PlatformText *text)
{
    // Explicit cast reads the text's handle as the entry of the pool that it is
    return (const CarcerMovableEntry *)text->movable;
}

// The movable cell of a text grown to hold `needed` bytes, doubled toward its max. A new movable cell takes the bytes
// so far, copied over a stage at a time, and the old cell goes back. 1, or 0 where the pool cannot meet the cell or it
// passes the max
static int platform_text_moved(PlatformText *text, unsigned long long needed)
{
    if (needed <= text->capacity)
    {
        return 1;
    }
    unsigned long long capacity = (text->capacity != 0ULL) ? text->capacity : PLATFORM_TEXT_FIRST;
    while (capacity < needed)
    {
        capacity *= 2ULL;
    }
    capacity = (capacity < text->max) ? capacity : text->max;
    if (capacity < needed)
    {
        return 0;
    }
    const CellOwner *const owner = &mmgr_owner_table[text->place];
    const CarcerMovableEntry *const cell = g_platform_guard->movable_buf_alloc((size_t)capacity, owner);
    if (cell == NULL)
    {
        platform_cell_refused(capacity, PLATFORM_TIER_MOVABLE, owner);
        return 0;
    }
    const unsigned long long held = (unsigned long long)text->line.flushed;
    int moved = 1;
    for (unsigned long long at = 0ULL; (moved != 0) && (at < held); at += PLATFORM_TEXT_STAGE_BYTES)
    {
        char page[PLATFORM_TEXT_STAGE_BYTES];
        const unsigned long long piece =
            ((held - at) < PLATFORM_TEXT_STAGE_BYTES) ? (held - at) : PLATFORM_TEXT_STAGE_BYTES;
        moved = g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
                    .cell = platform_text_movable(text), .offset = (size_t)at, .dst = page, .bytes = (size_t)piece}) &&
                g_platform_guard->movable_buf_write(
                    &(CarcerMovableCfg){.cell = cell, .offset = (size_t)at, .src = page, .bytes = (size_t)piece});
    }
    if (moved == 0)
    {
        (void)g_platform_guard->movable_buf_release(cell, owner);
        return 0;
    }
    if (text->movable != NULL)
    {
        (void)g_platform_guard->movable_buf_release(platform_text_movable(text), owner);
    }
    text->movable = cell;
    text->capacity = capacity;
    return 1;
}

// The flush of a movable text: the stage put into the movable cell after the bytes before it. The cell grows first
// where it is full. 1, or 0 where the pool cannot meet the cell or it passes the max, which fills the line
static int platform_text_staged(void *context, const char *bytes, size_t size)
{
    PlatformText *const text = (PlatformText *)context;
    const unsigned long long at = (unsigned long long)text->line.flushed;
    return platform_text_moved(text, at + (unsigned long long)size) &&
           g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
               .cell = platform_text_movable(text), .offset = (size_t)at, .src = bytes, .bytes = size});
}

// The flush of a text as its cell fills. A cell of twice the capacity takes the text so far, and the old cell goes
// back. The line then goes on past the copied text. 1, or 0 where the pool cannot meet the next cell or the
// next cell passes the max, which fills the line
static int platform_text_flushed(void *context, const char *bytes, size_t size)
{
    PlatformText *const text = (PlatformText *)context;
    (void)bytes;
    const unsigned long long held = (unsigned long long)text->line.flushed + (unsigned long long)size;
    const unsigned long long doubled = text->capacity * 2ULL;
    const unsigned long long capacity = (doubled < text->max) ? doubled : text->max;
    if (capacity <= text->capacity)
    {
        return 0;
    }
    char *const cell = (char *)platform_cell_grown(text->cell, held, capacity, text->place, text->tier);
    if (cell == NULL)
    {
        return 0;
    }
    text->cell = cell;
    text->capacity = capacity;
    text->line.out = cell + held;
    text->line.capacity = (size_t)(capacity - held);
    return 1;
}

// the line of a text over its cell, or over its stage on the movable tier, empty
static void platform_text_lined(PlatformText *text)
{
    if (text->tier == PLATFORM_TIER_MOVABLE)
    {
        text->line = mmgr_line_over(text->stage, sizeof(text->stage));
        text->line.flush = platform_text_staged;
    }
    else
    {
        text->line = mmgr_line_over(text->cell, (size_t)text->capacity);
        text->line.flush = platform_text_flushed;
    }
    text->line.context = text;
}

// the copy that the last read of a movable text gave, given back
static void platform_text_unviewed(PlatformText *text)
{
    platform_cell_released(text->view, text->place);
    text->view = NULL;
}

void platform_text_begun(PlatformText *text, unsigned long long max, unsigned int place, int tier)
{
    const unsigned long long first = (max < PLATFORM_TEXT_FIRST) ? max : PLATFORM_TEXT_FIRST;
    text->place = place;
    text->tier = tier;
    text->max = max;
    text->movable = NULL;
    text->view = NULL;
    text->cell = (tier != PLATFORM_TIER_MOVABLE) ? (char *)platform_cell_taken(first, place, tier) : NULL;
    text->capacity = (text->cell != NULL) ? first : 0ULL;
    platform_text_lined(text);
}

unsigned long long platform_text_size(const PlatformText *text)
{
    // a full line holds its capacity less its terminator
    if (text->line.capacity == 0u)
    {
        return 0ULL;
    }
    const unsigned long long at = (text->line.at < text->line.capacity) ? text->line.at : (text->line.capacity - 1u);
    return (unsigned long long)text->line.flushed + at;
}

const char *platform_text_bytes(PlatformText *text)
{
    (void)mmgr_line_finish(&text->line);
    if (text->tier != PLATFORM_TIER_MOVABLE)
    {
        return (text->cell != NULL) ? text->cell : "";
    }
    platform_text_unviewed(text);
    const unsigned long long size = platform_text_size(text);
    const unsigned long long flushed = (unsigned long long)text->line.flushed;
    char *const view = (char *)platform_cell_taken(size + 1ULL, text->place, PLATFORM_TIER_TEMPORARY);
    const int read = (view != NULL) &&
                     ((flushed == 0ULL) ||
                      g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
                          .cell = platform_text_movable(text), .offset = 0u, .dst = view, .bytes = (size_t)flushed}));
    if (read == 0)
    {
        platform_cell_released(view, text->place);
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = view + flushed, .src = text->stage, .bytes = (size_t)(size - flushed)});
    view[size] = '\0';
    text->view = view;
    return view;
}

void platform_text_reset(PlatformText *text)
{
    platform_text_unviewed(text);
    platform_text_lined(text);
}

void platform_text_cut(PlatformText *text, unsigned long long size)
{
    const unsigned long long held = platform_text_size(text);
    const unsigned long long kept = (size < held) ? size : held;
    const unsigned long long flushed = (unsigned long long)text->line.flushed;
    if ((text->tier == PLATFORM_TIER_MOVABLE) && (kept >= flushed))
    {
        // the cut is inside the stage, and the bytes before it stay where they are
        platform_text_unviewed(text);
        text->line.at = (size_t)(kept - flushed);
        return;
    }
    platform_text_reset(text);
    if (text->tier != PLATFORM_TIER_MOVABLE)
    {
        // the cell holds every byte of the text, and the cut is at or below its size
        text->line.at = (size_t)kept;
        return;
    }
    // the last part of a stage before the cut goes back into the stage, and the cell keeps the bytes before it
    const unsigned long long base = kept - (kept % PLATFORM_TEXT_STAGE_BYTES);
    const int read =
        (kept == base) || g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = platform_text_movable(text),
                                                                                 .offset = (size_t)base,
                                                                                 .dst = text->stage,
                                                                                 .bytes = (size_t)(kept - base)});
    text->line.flushed = (size_t)((read != 0) ? base : 0ULL);
    text->line.at = (size_t)((read != 0) ? (kept - base) : 0ULL);
}

void platform_text_ended(PlatformText *text)
{
    platform_text_unviewed(text);
    if ((text->movable != NULL) && (s_pool_bytes != 0ULL))
    {
        (void)g_platform_guard->movable_buf_release(platform_text_movable(text), &mmgr_owner_table[text->place]);
    }
    platform_cell_released(text->cell, text->place);
    text->cell = NULL;
    text->movable = NULL;
    text->capacity = 0ULL;
    text->line = mmgr_line_over(NULL, 0u);
}

void platform_array_begun(PlatformArray *array, unsigned long long element, unsigned int place, int ephemeral)
{
    array->cell = NULL;
    array->count = 0ULL;
    array->room = 0ULL;
    array->element = element;
    array->place = place;
    array->ephemeral = ephemeral;
}

int platform_array_counted(PlatformArray *array, unsigned long long count)
{
    if (count > array->room)
    {
        unsigned long long room = (array->room == 0ULL) ? 8ULL : array->room;
        while (room < count)
        {
            room *= 2ULL;
        }
        void *const grown = platform_cell_grown(array->cell, array->count * array->element, room * array->element,
                                                array->place, array->ephemeral);
        if (grown == NULL)
        {
            return 0;
        }
        array->cell = grown;
        array->room = room;
    }
    if (count > array->count)
    {
        // Explicit cast reads the cell as its bytes, from the first new element on
        memor.set(&(MemoriaCfg){.dst = (char *)array->cell + (array->count * array->element),
                                .bytes = (size_t)((count - array->count) * array->element),
                                .val = 0u});
    }
    array->count = count;
    return 1;
}

int platform_array_appended(PlatformArray *array, const void *value)
{
    const unsigned long long at = array->count;
    if (!platform_array_counted(array, at + 1ULL))
    {
        return 0;
    }
    // Explicit cast reads the cell as its bytes, at the new element
    memor.cpy(&(MemoriaCfg){
        .dst = (char *)array->cell + (at * array->element), .src = value, .bytes = (size_t)array->element});
    return 1;
}

void *platform_array_at(const PlatformArray *array, unsigned long long at)
{
    // Explicit cast reads the cell as its bytes, at the element
    return (at < array->count) ? (void *)((char *)array->cell + (at * array->element)) : NULL;
}

void platform_array_ended(PlatformArray *array)
{
    platform_cell_released(array->cell, array->place);
    array->cell = NULL;
    array->count = 0ULL;
    array->room = 0ULL;
}

PlatformSignalHandler platform_signal_handled(int number, PlatformSignalHandler handler)
{
    return signal(number, handler);
}

unsigned long long platform_clock_ns(void)
{
    return (unsigned long long)embed_clock_ns();
}
