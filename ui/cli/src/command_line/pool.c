// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pool.c: the CLI's memory on the one pool of the platform (platform_pool.c): the pool opened and emptied for a
// program's request while no cell is allocated, the lines that grow on the temporary tier, and the reports. The reports
// read the cellblock's peaks and the owner table off their atomic words, and are written at a command's end
#include "pool.h"
#include "rings.h"

#include "cli_internal.h"

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

// the turnstiles' rows of touches that the pool's movable option saves (platform_pool.c)
#define POOL_TOUCH_ROWS 64u

// the pool's bytes
static unsigned long long pool_bytes(void)
{
    return (unsigned long long)platform_pool_series()->size;
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

int cli_pool_made(unsigned long long bytes)
{
    // The platform opens the pool at the first request and empties it for each request while no cell is out. The pool
    // is CLI_POOL_BYTES whole, the regions of the temporary tier among them, and the platform places the regions past
    // the bytes it is asked for. A request past the pool declared is a refusal. The rings' records take their places
    // over their parts of the rings block with each emptied pool, before any ticket is taken
    const int emptied = (bytes <= CLI_POOL_BYTES) &&
                        platform_open(CLI_POOL_BYTES - (unsigned long long)MMGR_CARCER_REGION_AREA) &&
                        platform_pool_emptied(bytes);
    if (emptied)
    {
        cli_rings.positioned();
    }
    return emptied;
}

void cli_pool_idle_released(void)
{
    platform_pool_idle_released();
}

void cli_pool_report(unsigned long long *pool, unsigned long long *persistent, unsigned long long *temporary)
{
    const SeriesCellularum *const record = platform_pool_series();
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
    const SeriesCellularum *const record = platform_pool_series();
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
    mmgr_line_decimal(&line, platform_pool_sorts(), 1u);
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
