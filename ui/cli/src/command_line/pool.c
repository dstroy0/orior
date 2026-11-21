// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pool.c: the CLI's memory: MMgr's prison site, one cellblock over the pool declared here at CLI_POOL_BYTES, emptied
// for a program's request while no cell is allocated; and the program's owner table, positioned here from owners.h as
// data, each module's owner a const pointer into it. Every take and release goes through the cellblock's guard and
// carries the owner. MMgr's report of a release carrying the wrong owner is logged. The reports read the cellblock's
// peaks and the owner table off their atomic words, and are written at a command's end
#include "pool.h"
#include "rings.h"

#include "cli_internal.h"

// The pool: CLI_POOL_BYTES the system gives once, as the pool is first made, outside the program's image so that the
// pages no cell holds can go back to the system; the cellblock's record settled by the linker and placed over those
// bytes; emptied for a program's request while no cell is allocated. Minimum security: a cell is left as it is when
// released and passed out unzeroed, so that a release costs no bytes it did not use and no page it did not touch; a
// capacity writes what it reads
ParsMemoriaeHospes(cli_pool);
LocusCarcerum(cli_pool_site, MMGR_MINIMUM_SECURITY_HOSPES(cli_pool));

// the program's owners, the table and each owner's pointer into it
MMGR_OWNERS(CLI_OWNERS)

const MinimumSecurityGuard *const g_cli_guard = &cli_pool_site.cli_pool;

// One owner as the table holds it, its counts read off their atomic words and written after, since a line written to
// may take a cell of the same cellblock
typedef struct
{
    const char *name;
    unsigned long long in_use;
    unsigned long long hw;
} PoolOwnerRead;

typedef struct
{
    const char *type;
    const VerbaLinea *line;
    PoolOwnerRead *owners;
    VerbaLinea *out;
    unsigned long long persistent;
    unsigned long long temporary;
} PoolCtx;

// A line of the pool's log: written to the run this process is a member of as the record `cells/<type>`, and to the
// process's error stream, where the harness saves the window's
EMBED_INLINE void pool_log(const PoolCtx *args)
{
    cli_pool_reported(args->type, args->line);
    if (mmgr_line_ok(args->line))
    {
        char text[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea line = {text, sizeof(text), 0ULL};
        mmgr_line_put(&line, "cells ");
        mmgr_line_put(&line, args->type);
        mmgr_line_ch(&line, ' ');
        mmgr_line_put_n(&line, args->line->out, args->line->at);
        mmgr_line_ch(&line, '\n');
        EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = text,
                   .length = mmgr_line_finish(&line));
    }
}

embed_u64 embed_clock_ns(void)
{
    // the one clock: the microsecond clock in nanoseconds, which stamps a turnstile's badge and a cell's take alike
    return (embed_u64)cli_clock_microseconds() * 1000u;
}

embed_u64 mmgr_carcer_hw_clock_ns(void)
{
    return embed_clock_ns();
}

// the places threads hold: a thread takes one on its first call and gives it back as it ends. The places in use stay
// within the live threads, and a region of the temporary tier is one thread's
#define POOL_PLACES_MAX 256u
// the places given so far: a thread with no free place to take gets the count as its place
static embed_atomic_u32 s_pool_places_given = EMBED_ATOMIC_INIT(0u);
// the free places as a stack of indices: each entry the next free place plus one, 0 at the bottom. The head holds
// the top place plus one in its low half and a count of pushes in its high half, so a place taken and put back
// between a read and a swap does not pass as unchanged
static embed_atomic_u32 s_pool_place_next[POOL_PLACES_MAX];
static embed_atomic_u64 s_pool_place_free = EMBED_ATOMIC_INIT(0u);
// the place of this thread plus one, and 0 before its first call or after its release
static _Thread_local embed_u32 t_pool_place_plus_one;

// a free place taken off the stack: its place plus one, or 0 where the stack is empty
static embed_u32 pool_place_popped(void)
{
    embed_u64 head = EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg, .word = &s_pool_place_free);
    while ((head & 0xffffffffULL) != 0ULL)
    {
        const embed_u32 top = (embed_u32)(head & 0xffffffffULL);
        const embed_u32 next =
            EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_pool_place_next[top - 1u]);
        const embed_u64 after = (((head >> 32u) + 1ULL) << 32u) | (embed_u64)next;
        if (EMBED_CALL(embed_atomic_u64_table.swap_if, EmbedAtomicU64SwapIfCfg, .word = &s_pool_place_free,
                       .expected = &head, .value = after))
        {
            return top;
        }
    }
    return 0u;
}

embed_u32 embed_thread_place(void)
{
    // the place of the thread that calls: a free place where one waits, else a new count. MMgr selects the region of
    // the temporary tier a take goes to by it
    if (t_pool_place_plus_one == 0u)
    {
        const embed_u32 popped = pool_place_popped();
        t_pool_place_plus_one = (popped != 0u)
                                    ? popped
                                    : (EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg,
                                                  .word = &s_pool_places_given, .value = 1u) +
                                       1u);
    }
    return t_pool_place_plus_one - 1u;
}

void cli_thread_place_released(void)
{
    const embed_u32 plus_one = t_pool_place_plus_one;
    // a thread with no place, or one past the stack's room, leaves nothing
    if ((plus_one == 0u) || (plus_one > POOL_PLACES_MAX))
    {
        return;
    }
    t_pool_place_plus_one = 0u;
    embed_u64 head = EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg, .word = &s_pool_place_free);
    for (;;)
    {
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_pool_place_next[plus_one - 1u],
                   .value = (embed_u32)(head & 0xffffffffULL));
        const embed_u64 after = (((head >> 32u) + 1ULL) << 32u) | (embed_u64)plus_one;
        if (EMBED_CALL(embed_atomic_u64_table.swap_if, EmbedAtomicU64SwapIfCfg, .word = &s_pool_place_free,
                       .expected = &head, .value = after))
        {
            return;
        }
    }
}

void mmgr_carcer_hw_misnamed(const CellOwner *named, const CellOwner *cell_owner, size_t bytes)
{
    char text[200];
    VerbaLinea line = {text, sizeof(text), 0u, NULL, NULL, 0u};
    mmgr_line_put(&line, named->name);
    mmgr_line_put(&line, " named for a cell of ");
    mmgr_line_put(&line, (cell_owner != NULL) ? cell_owner->name : "no owner of the table");
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, (unsigned long long)bytes, 1u);
    mmgr_line_put(&line, " bytes: refused");
    EMBED_CALL(pool_log, PoolCtx, .type = "misnamed", .line = &line);
}

// the turnstiles the option's storage has capacity for, which the machine's processors are bounded to, and the rows of
// touches per thread it saves
#define POOL_WIDTH_MAX 256u
#define POOL_TOUCH_ROWS 64u

// What MMgr's movable option is given: storage whose address the linker settles, and the thread its mover runs on,
// saved at idle between moves and raised across each, since a reader waits on the copy
ParsMemoriaeInternae(cli_pool_movable, MMGR_MOVABLE_STORAGE_BYTES(CLI_POOL_MOVABLE, POOL_WIDTH_MAX, POOL_TOUCH_ROWS));
static CliThread s_mover;
static CliWake s_mover_wake = CLI_WAKE_INIT;

// The mover as MMgr passes it to the thread: what it runs and on what, and whether the pool asks it to end, which its
// next wait answers, and whether it has ended, which the pool waits on before it empties the cellblock again
static void (*s_mover_run)(void *context);
static void *s_mover_context;
static embed_atomic_u32 s_mover_ending;
static embed_atomic_u32 s_mover_ended;

static void pool_mover_run(void *context)
{
    (void)context;
    s_mover_run(s_mover_context);
    EMBED_ATOMIC_STORE(&s_mover_ended, 1u);
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_mover_wake);
}

// the thread's id in the width MMgr badges a pass with
static size_t pool_thread_id(void)
{
    return (size_t)cli_thread_id();
}

static embed_bool pool_mover_started(void (*run)(void *context), void *context)
{
    s_mover_run = run;
    s_mover_context = context;
    EMBED_ATOMIC_STORE(&s_mover_ending, 0u);
    EMBED_ATOMIC_STORE(&s_mover_ended, 0u);
    s_mover.run = pool_mover_run;
    s_mover.context = NULL;
    s_mover.priority = CLI_PRIORITY_IDLE;
    const int started = EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_mover) != 0;
    // a thread that did not start leaves no mover to end
    s_mover_run = started ? run : NULL;
    return (embed_bool)started;
}

static embed_bool pool_mover_waited(embed_u32 milliseconds)
{
    if (EMBED_ATOMIC_LOAD(&s_mover_ending) != 0u)
    {
        return EMBED_FALSE;
    }
    (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_mover_wake,
                     .milliseconds_u64 = milliseconds);
    return (embed_bool)(EMBED_ATOMIC_LOAD(&s_mover_ending) == 0u);
}

// the mover ended where it runs: asked to end, woken, and waited for
static void pool_mover_ended(void)
{
    if (s_mover_run == NULL)
    {
        return;
    }
    EMBED_ATOMIC_STORE(&s_mover_ending, 1u);
    while (EMBED_ATOMIC_LOAD(&s_mover_ended) == 0u)
    {
        EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_mover_wake);
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_mover_wake, .milliseconds_u64 = 1ULL);
    }
    s_mover_run = NULL;
}

// the sorts the mover has started, read by the telemetry
static embed_atomic_u32 s_mover_sorts;

static void pool_mover_moving(embed_bool moving)
{
    if (moving)
    {
        (void)EMBED_ATOMIC_ADD_32(&s_mover_sorts, 1u);
    }
    EMBED_CALL(cli_words_err_msg_internal.priority_own, CliWordsErrMsgCtx,
               .priority = moving ? CLI_PRIORITY_WORK : CLI_PRIORITY_IDLE);
}

void cli_pool_movable_ready(void)
{
    const unsigned int processors = cli_processors();
    // a refusal is the option assigned already, or the thread not started, which leaves the pool working and still
    (void)g_cli_guard->movable_init(
        &(CarcerMovableInitCfg){.storage = mmgr_pars_storage_cli_pool_movable,
                                .storage_bytes = cli_pool_movable_bytes,
                                .cells = CLI_POOL_MOVABLE,
                                .width = (processors < POOL_WIDTH_MAX) ? processors : POOL_WIDTH_MAX,
                                .rows = POOL_TOUCH_ROWS,
                                .thread_started = pool_mover_started,
                                .thread_id = pool_thread_id,
                                .thread_wait = pool_mover_waited,
                                .thread_yield = cli_thread_yield,
                                .thread_moving = pool_mover_moving});
}

const SeriesCellularum *cli_pool_series_cellularum(void)
{
    return &cli_pool_site_cli_pool_ctx;
}

// the pool's bytes
static unsigned long long pool_bytes(void)
{
    return (unsigned long long)cli_pool_site_cli_pool_ctx.size;
}

// the owners of the table copied into `owners`, sized for MMGR_CARCER_OWNERS: their count
EMBED_INLINE unsigned int pool_owners_read(const PoolCtx *args)
{
    unsigned int count = 0u;
    for (embed_word at = 0u; at < mmgr_owner_table_count; at += 1u)
    {
        const CellOwner *const owner = &mmgr_owner_table[at];
        args->owners[count] = (PoolOwnerRead){owner->name, (unsigned long long)mmgr_owner_in_use(owner),
                                              (unsigned long long)mmgr_owner_hw(owner)};
        count += 1u;
    }
    return count;
}

// a report, `<pool's bytes> <persistent> <temporary>`, written into `out`
EMBED_INLINE void pool_write(const PoolCtx *args)
{
    mmgr_line_decimal(args->out, pool_bytes(), 1u);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_decimal(args->out, args->persistent, 1u);
    mmgr_line_ch(args->out, ' ');
    mmgr_line_decimal(args->out, args->temporary, 1u);
}

void cli_pool_reported(const char *name, const VerbaLinea *report)
{
    if ((qry_member_run() == NULL) || !mmgr_line_ok(report))
    {
        return;
    }
    char named[QRY_NAME_MAX_LEN + 1u];
    VerbaLinea name_line = {named, sizeof(named), 0u, NULL, NULL, 0u};
    mmgr_line_put(&name_line, "cells/");
    mmgr_line_put(&name_line, name);
    if (mmgr_line_finish(&name_line) != 0u)
    {
        (void)qry_member_append(named, report->out, report->at, QRY_ORDINARY);
    }
}

// the mover's run on the option assigned already: what the pool starts again after a refusal ended the mover
static void pool_mover_entered(void *context)
{
    mmgr_movable_run(context);
}

int cli_pool_made(unsigned long long bytes)
{
    // A program empties its pool before any thread takes a cell, and empties it again only while no cell is out.
    // The pool ends the mover first: the mover then reads no cellblock as the pool empties, and holds no closed
    // bit that leaves a trim owed. The gap reader runs the trims owed, and the idle test comes after it. Nothing
    // takes between the test and the emptying. The pool refuses a request past the pool declared, and after a
    // refusal starts the mover again on the option assigned
    SeriesCellularum *const record = &cli_pool_site_cli_pool_ctx;
    const int ran = s_mover_run != NULL;
    pool_mover_ended();
    (void)g_cli_guard->buf_available();
    const int idle = mmgr_series_cellularum_idle(record);
    const int fits = bytes <= CLI_POOL_BYTES;
    // the pool's bytes come from the system the first time the pool is made, and the cellblock is placed over them
    if (idle && fits && (record->base == NULL))
    {
        // Explicit cast takes the pages the system gave as the bytes the cellblock is placed on
        uint8_t *const storage = (uint8_t *)cli_pages_mapped(CLI_POOL_BYTES);
        if (!mmgr_series_cellularum_placed(record, storage, CLI_POOL_BYTES))
        {
            return 0;
        }
    }
    if (idle && fits)
    {
        mmgr_series_cellularum_emptied(record);
        // the mover's thread starts with the pool, so the cellblock is sorted in every quiet past the fragmentation
        // threshold, whether or not a module stores movable cells
        cli_pool_movable_ready();
        // the rings' records positioned over their parts of the rings block, with the pool, before any ticket is taken
        cli_rings.positioned();
    }
    else if (ran)
    {
        (void)pool_mover_started(pool_mover_entered, record);
    }
    return idle && fits;
}

// The line's flush as its cell fills: a cell of twice the capacity taken, the text so far copied into it, the cell
// given back, and the line's `out` set past the copied text. 1, or 0 where the cells cannot meet the next cell or it
// would pass `max`, which fills the line
static int grown_flush(void *context, const char *bytes, size_t size)
{
    CliGrownLine *const grown = context;
    (void)bytes;
    const unsigned long long held = grown->line.flushed + size;
    const unsigned long long doubled = grown->capacity * 2ULL;
    const unsigned long long capacity = (doubled < grown->max) ? doubled : grown->max;
    if (capacity <= grown->capacity)
    {
        return 0;
    }
    char *const cell = g_cli_guard->temporary_buf_alloc(capacity, grown->owner);
    if (cell == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = cell, .src = grown->cell, .bytes = held});
    g_cli_guard->persistent_buf_release(grown->cell, grown->owner);
    grown->cell = cell;
    grown->capacity = capacity;
    grown->line.out = cell + held;
    grown->line.capacity = capacity - held;
    return 1;
}

// the bytes a grown line begins with: a cell of this size stays on its thread's stack, and the flush doubles it toward
// its max as the text asks
#define POOL_GROWN_FIRST (64ULL << 10u)

void cli_grown_begin(CliGrownLine *grown, unsigned long long max, const CellOwner *owner)
{
    // the line's cell begins small and grows to its max as the writes come
    const unsigned long long first = (max < POOL_GROWN_FIRST) ? max : POOL_GROWN_FIRST;
    grown->cell = g_cli_guard->temporary_buf_alloc(first, owner);
    grown->capacity = (grown->cell != NULL) ? first : 0ULL;
    grown->max = max;
    grown->owner = owner;
    grown->line = mmgr_line_over(grown->cell, first);
    grown->line.flush = grown_flush;
    grown->line.context = grown;
}

unsigned long long cli_grown_size(const CliGrownLine *grown)
{
    // a full line holds its capacity less its terminator
    const unsigned long long at =
        (grown->line.at < grown->line.capacity) ? grown->line.at : (grown->line.capacity - 1ULL);
    return (grown->line.capacity != 0ULL) ? (grown->line.flushed + at) : 0ULL;
}

void cli_grown_reset(CliGrownLine *grown)
{
    grown->line = mmgr_line_over(grown->cell, grown->capacity);
    grown->line.flush = grown_flush;
    grown->line.context = grown;
}

void cli_grown_end(CliGrownLine *grown)
{
    g_cli_guard->persistent_buf_release(grown->cell, grown->owner);
    grown->cell = NULL;
    grown->capacity = 0ULL;
    grown->line = mmgr_line_over(NULL, 0u);
}

// the bytes of a page, and the least an idle span holds in whole pages before its pages go back
#define POOL_PAGE_BYTES 4096ULL
#define POOL_IDLE_RELEASED (64ULL << 10u)

// The idle walk's record: whether the walk passed the gap between the tiers yet. The gap comes first
typedef struct
{
    int gap_passed;
} PoolIdle;

// The span from `from` to `to` cut to the whole pages inside it. Those pages go back where they hold
// POOL_IDLE_RELEASED bytes or more
static void pool_idle_pages_released(unsigned long long from, unsigned long long to)
{
    const unsigned long long first = (from + POOL_PAGE_BYTES - 1ULL) & ~(POOL_PAGE_BYTES - 1ULL);
    const unsigned long long end = to & ~(POOL_PAGE_BYTES - 1ULL);
    if ((end > first) && ((end - first) >= POOL_IDLE_RELEASED))
    {
        // Explicit cast takes the first page back as its address
        cli_pages_released((void *)(uintptr_t)first, end - first);
    }
}

// One idle span of the cellblock, its touched pages given back. A take touches the gap between the tiers only up to
// the furthest persistent end and down to the furthest temporary top. The rest of the gap holds pages that no take
// touched, which no working set holds, so the walk gives the gap's two touched ends back alone. Each span after the
// gap goes back whole
static void pool_idle_each(void *context, void *at, size_t bytes)
{
    PoolIdle *const idle = (PoolIdle *)context;
    const SeriesCellularum *const record = &cli_pool_site_cli_pool_ctx;
    // Explicit casts read the span's bounds and the cellblock's start as integers for the page arithmetic
    const unsigned long long from = (unsigned long long)(uintptr_t)at;
    const unsigned long long to = from + (unsigned long long)bytes;
    if (idle->gap_passed)
    {
        pool_idle_pages_released(from, to);
        return;
    }
    idle->gap_passed = 1;
    const unsigned long long base = (unsigned long long)(uintptr_t)record->base;
    const unsigned long long persistent_reach = base + (unsigned long long)mmgr_series_cellularum_persistent_hw(record);
    const unsigned long long temporary_reach =
        base + pool_bytes() - (unsigned long long)mmgr_series_cellularum_temporary_hw(record);
    const unsigned long long low_end = (persistent_reach < to) ? persistent_reach : to;
    const unsigned long long high_from = (temporary_reach > from) ? temporary_reach : from;
    // a build that tracks no marks reads them as 0, and its gap goes back whole
    if (!MMGR_ENABLE_HW_MEM_CAPACITY_CB || (high_from <= low_end))
    {
        pool_idle_pages_released(from, to);
        return;
    }
    pool_idle_pages_released(from, low_end);
    pool_idle_pages_released(high_from, to);
}

void cli_pool_idle_released(void)
{
    PoolIdle idle = {0};
    mmgr_series_cellularum_idle_each(&cli_pool_site_cli_pool_ctx, pool_idle_each, &idle);
}

void cli_pool_report(unsigned long long *pool, unsigned long long *persistent, unsigned long long *temporary)
{
    const SeriesCellularum *const record = &cli_pool_site_cli_pool_ctx;
    *pool = pool_bytes();
    *persistent = (unsigned long long)mmgr_series_cellularum_persistent_hw(record);
    *temporary = (unsigned long long)mmgr_series_cellularum_temporary_hw(record);
}

void cli_pool_usage_write(VerbaLinea *out)
{
    unsigned long long pool = 0ULL;
    unsigned long long persistent = 0ULL;
    unsigned long long temporary = 0ULL;
    cli_pool_report(&pool, &persistent, &temporary);
    EMBED_CALL(pool_write, PoolCtx, .out = out, .persistent = persistent, .temporary = temporary);
}

void cli_pool_owner_usage_write(const CellOwner *owner, VerbaLinea *out)
{
    EMBED_CALL(pool_write, PoolCtx, .out = out, .persistent = (unsigned long long)mmgr_owner_hw(owner),
               .temporary = 0ULL);
}

void cli_pool_owners_write(VerbaLinea *out)
{
    PoolOwnerRead owners[MMGR_CARCER_OWNERS];
    const unsigned int count = EMBED_CALL(pool_owners_read, PoolCtx, .owners = owners);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, owners[at].name);
        mmgr_line_ch(out, '\t');
        mmgr_line_decimal(out, owners[at].in_use, 1u);
        mmgr_line_ch(out, '\t');
        mmgr_line_decimal(out, owners[at].hw, 1u);
        mmgr_line_ch(out, '\n');
    }
}

// the in-use count of each owner at the last telemetry line, which only the window's main thread reads and writes
static unsigned long long s_pool_telemetry_in_use[MMGR_CARCER_OWNERS];

void cli_pool_telemetry_log(void)
{
    // The reads come first and the writes after them, because a line can take a cell of the same cellblock
    PoolOwnerRead owners[MMGR_CARCER_OWNERS];
    const unsigned int count = EMBED_CALL(pool_owners_read, PoolCtx, .owners = owners);
    unsigned long long pool = 0ULL;
    unsigned long long persistent = 0ULL;
    unsigned long long temporary = 0ULL;
    cli_pool_report(&pool, &persistent, &temporary);
    unsigned long long in_use = 0ULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        in_use += owners[at].in_use;
    }
    const SeriesCellularum *const record = &cli_pool_site_cli_pool_ctx;
    unsigned long long touches = 0ULL;
    for (embed_word row = 0u; row <= POOL_TOUCH_ROWS; row += 1u)
    {
        size_t id = 0u;
        size_t passes = 0u;
        if (mmgr_movable_touches(record, row, &id, &passes))
        {
            touches += (unsigned long long)passes;
        }
    }
    char text[200];
    VerbaLinea line = {text, sizeof(text), 0u, NULL, NULL, 0u};
    mmgr_line_decimal(&line, pool, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, in_use, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, persistent, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, temporary, 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, (unsigned long long)mmgr_movable_queue_depth(record), 1u);
    mmgr_line_ch(&line, ' ');
    mmgr_line_decimal(&line, touches, 1u);
    if (mmgr_line_ok(&line))
    {
        EMBED_CALL(pool_log, PoolCtx, .type = "telemetry", .line = &line);
    }
    // the sorts the mover has run, with the persistent end and the fragmentation as they are
    line.at = 0ULL;
    mmgr_line_decimal(&line, EMBED_ATOMIC_LOAD(&s_mover_sorts), 1u);
    mmgr_line_put(&line, " end ");
    mmgr_line_decimal(&line, (unsigned long long)mmgr_series_cellularum_persistent_end(record), 1u);
    mmgr_line_put(&line, " fragmentation ");
    mmgr_line_decimal(&line, (unsigned long long)mmgr_series_cellularum_fragmentation(record), 1u);
    EMBED_CALL(pool_log, PoolCtx, .type = "sorts", .line = &line);
    // an owner gets a line only where its in-use count moved since the last telemetry line
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (owners[at].in_use != s_pool_telemetry_in_use[at])
        {
            s_pool_telemetry_in_use[at] = owners[at].in_use;
            line.at = 0ULL;
            mmgr_line_put(&line, owners[at].name);
            mmgr_line_ch(&line, ' ');
            mmgr_line_decimal(&line, owners[at].in_use, 1u);
            mmgr_line_ch(&line, ' ');
            mmgr_line_decimal(&line, owners[at].hw, 1u);
            EMBED_CALL(pool_log, PoolCtx, .type = "owner", .line = &line);
        }
    }
    // A lane's passes are an owner's takes and releases, its id the owner's place from 1. A row gets a line only where
    // its passes moved since the last telemetry line; the last row counts every lane past the rest
    static unsigned long long s_pool_telemetry_passes[POOL_TOUCH_ROWS + 1u];
    for (embed_word row = 0u; row <= POOL_TOUCH_ROWS; row += 1u)
    {
        size_t id = 0u;
        size_t passes = 0u;
        if (mmgr_movable_touches(record, row, &id, &passes) &&
            ((unsigned long long)passes != s_pool_telemetry_passes[row]))
        {
            s_pool_telemetry_passes[row] = (unsigned long long)passes;
            const int named = (id >= 1u) && (id <= (size_t)count);
            line.at = 0ULL;
            mmgr_line_put(&line, named ? owners[id - 1u].name : "other");
            mmgr_line_ch(&line, ' ');
            mmgr_line_decimal(&line, (unsigned long long)passes, 1u);
            EMBED_CALL(pool_log, PoolCtx, .type = "touches", .line = &line);
        }
    }
}

void cli_pool_peaks_report(void)
{
    if (qry_member_run() == NULL)
    {
        return;
    }
    char report[96];
    VerbaLinea line = {report, sizeof(report), 0u, NULL, NULL, 0u};
    cli_pool_usage_write(&line);
    cli_pool_reported("cli_cells", &line);
    PoolOwnerRead owners[MMGR_CARCER_OWNERS];
    const unsigned int count = EMBED_CALL(pool_owners_read, PoolCtx, .owners = owners);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        line.at = 0u;
        EMBED_CALL(pool_write, PoolCtx, .out = &line, .persistent = owners[at].hw, .temporary = 0ULL);
        cli_pool_reported(owners[at].name, &line);
    }
}
