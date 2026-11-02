// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sweep.c: the host swept, in a run of its own: a list walk of the sizes the run's writer and the editor's file window
// take, each entry timed on the run's one clock, the nanoseconds it took a MiB a word each in the run's blob `sweep`,
// an entry's QUERY_SWEEP_ROUNDS words together. Each round starts one entry further along than the round before. Then
// each entry's median over every sweep recorded, and qry_host.h written for the writer and files_host.js for the window
// from the fastest, once QUERY_SWEEP_LEAST sweeps report
#include "../../../../src/cu/includes/file_defs/qry/writer/qry_writer_internal.h"
#include "../../../cli/src/command_line/callback_cfg.h"
#include "../../../cli/src/text/text.h"
#include "retention_internal.h"

// An entry of the sweep: what it times, its size, and what is done at that size, the bytes done or 0
typedef struct
{
    const char *what;
    unsigned long long size;
    unsigned long long (*act)(const QuerySizeCfg *args);
} QuerySweepEntry;
// the sweeps' run folder, the files written from them, the rounds an entry is timed, the sweeps the files are written
// from at least, how far past the fastest a smaller slice is still taken in parts a thousand, and the bytes timed over
#define QUERY_SWEEPS "build/qry_sweep/qry"
#define QUERY_SWEEP_HOST_HEADER "src/cu/includes/file_defs/qry/writer/qry_host.h"
#define QUERY_SWEEP_WINDOW_MODULE "ui/gui/src-js/files_host.js"
#define QUERY_SWEEP_ROUNDS 3u
#define QUERY_SWEEP_LEAST 3u
#define QUERY_SWEEP_SLICE_NEAR 50ull
#define QUERY_SWEEP_WRITE_BYTES (256ull << 20u)
#define QUERY_SWEEP_READ_BYTES (64ull << 20u)

// the scratch every entry works in: a file's path, and the bytes written and read
static char s_scratch[QUERY_PATH_MAX_LEN];
static unsigned char *s_bytes;
static volatile unsigned long long s_read_sum;

// a run's region made whole for a request of `size` bytes, as the writer makes it, and every page of it touched
static unsigned long long sweep_region(const QuerySizeCfg *args)
{
    unsigned long long members = 0ull;
    unsigned long long places = 0ull;
    unsigned long long run_bytes = 0ull;
    const unsigned long long whole = qry_shared_laid(args->size, &members, &places, &run_bytes);
    QryShared *const shared = qry_writer_shared_made(s_scratch, whole);
    if (shared == NULL)
    {
        return 0ull;
    }
    unsigned char *const bytes = (unsigned char *)shared;
    for (unsigned long long page = 4096ull; page < whole; page += 4096ull)
    {
        bytes[page] = 1u;
    }
    qry_writer_shared_released(shared);
    return whole;
}

// the writer's write call: a file written whole `size` bytes a call, flushed to the disk and taken away
static unsigned long long sweep_write(const QuerySizeCfg *args)
{
    const long long file = qry_writer_file_made(s_scratch);
    int whole = (file >= 0ll);
    for (unsigned long long at = 0ull; whole && (at < QUERY_SWEEP_WRITE_BYTES); at += args->size)
    {
        whole = qry_writer_file_put(file, s_bytes + (at % QUERY_SWEEP_READ_BYTES), args->size);
    }
    whole = (file >= 0ll) && qry_writer_file_closed(file) && whole;
    (void)qry_writer_file_removed(s_scratch);
    return whole ? QUERY_SWEEP_WRITE_BYTES : 0ull;
}

// the window's slice: a file mapped, `size` bytes of it read and let go, as the editor reads a slice, over the file
static unsigned long long sweep_slice(const QuerySizeCfg *args)
{
    unsigned long long size = args->size;
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = s_scratch,
                    .bytes = s_bytes, .size = QUERY_SWEEP_READ_BYTES))
    {
        return 0ull;
    }
    unsigned long long sum = 0ull;
    for (unsigned long long at = 0ull; at < QUERY_SWEEP_READ_BYTES; at += size)
    {
        unsigned long long mapped = 0ull;
        const unsigned char *const bytes = qry_member_file_mapped(s_scratch, &mapped);
        for (unsigned long long byte = at; (bytes != NULL) && (byte < (at + size)) && (byte < mapped); byte += 1ull)
        {
            sum += bytes[byte];
        }
        if (bytes != NULL)
        {
            qry_member_file_unmapped(bytes, mapped);
        }
    }
    s_read_sum = sum;
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = s_scratch);
    return QUERY_SWEEP_READ_BYTES;
}

static const QuerySweepEntry s_entries[] = {
    {"region", 16ull << 20u, sweep_region},  {"region", 64ull << 20u, sweep_region},
    {"region", 256ull << 20u, sweep_region}, {"region", 1ull << 30u, sweep_region},
    {"write", 64ull << 10u, sweep_write},    {"write", 256ull << 10u, sweep_write},
    {"write", 1ull << 20u, sweep_write},     {"write", 4ull << 20u, sweep_write},
    {"write", 16ull << 20u, sweep_write},    {"write", 64ull << 20u, sweep_write},
    {"slice", 64ull << 10u, sweep_slice},    {"slice", 128ull << 10u, sweep_slice},
    {"slice", 256ull << 10u, sweep_slice},   {"slice", 384ull << 10u, sweep_slice},
    {"slice", 512ull << 10u, sweep_slice},   {"slice", 1ull << 20u, sweep_slice},
    {"slice", 2ull << 20u, sweep_slice}};

#define QUERY_SWEEP_ENTRIES ((unsigned int)(sizeof(s_entries) / sizeof(s_entries[0])))

typedef struct
{
    const QuerySweepEntry *entry;
    const QueryContext *context_read;
    const char *relative;
    char *path;
    unsigned long long *timed;
    unsigned int count;
    const char *what;
    unsigned long long near;
    const unsigned long long *median;
    QueryContext *context;
    const VerbaLinea *text;
    const QueryWriter *own;
} SweepCtx;
// the entry timed on the run's clock: the nanoseconds it took a MiB, 0 where it did nothing
EMBED_INLINE unsigned long long sweep_time(const SweepCtx *args)
{
    const unsigned long long began = qry_member_clock();
    const unsigned long long bytes = EMBED_CALL(args->entry->act, QuerySizeCfg, .size = args->entry->size);
    return (bytes != 0ull) ? (((qry_member_clock() - began) << 20u) / bytes) : 0ull;
}

// `relative` under the tree's top joined into `path`, sized for QUERY_PATH_MAX_LEN
EMBED_INLINE void sweep_path(const SweepCtx *args)
{
    VerbaLinea line = {args->path, QUERY_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->context_read->root);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->relative);
    (void)mmgr_line_finish(&line);
}

// the median of the `count` timings at `timed`, sorted in place, 0 where there are none
EMBED_INLINE unsigned long long sweep_median(const SweepCtx *args)
{
    for (unsigned int at = 1u; at < args->count; at += 1u)
    {
        const unsigned long long pivot = args->timed[at];
        unsigned int place = at;
        while ((place > 0u) && (args->timed[place - 1u] > pivot))
        {
            args->timed[place] = args->timed[place - 1u];
            place -= 1u;
        }
        args->timed[place] = pivot;
    }
    return (args->count != 0u) ? args->timed[args->count / 2u] : 0ull;
}

// the size of `what` whose median is least, or the smallest within `near` parts a thousand of that least
EMBED_INLINE unsigned long long sweep_take(const SweepCtx *args)
{
    unsigned long long least = ~0ull;
    unsigned long long taken = 0ull;
    for (unsigned int pass = 0u; pass < 2u; pass += 1u)
    {
        for (unsigned int entry = 0u; entry < QUERY_SWEEP_ENTRIES; entry += 1u)
        {
            const int ours =
                (args->median[entry] != 0ull) &&
                cellul.eq(&(CatenaFinitaCfg){.src = s_entries[entry].what, .other = args->what, .cap = 16u});
            least = (ours && (pass == 0u) && (args->median[entry] < least)) ? args->median[entry] : least;
            taken = (ours && (pass == 1u) && ((taken == 0ull) || (s_entries[entry].size < taken)) &&
                     ((args->median[entry] * 1000ull) <= (least * (1000ull + args->near))))
                        ? s_entries[entry].size
                        : taken;
        }
    }
    return taken;
}

// the file at the tree's `relative` written with `text` where it contains anything else, and reported
EMBED_INLINE void sweep_on_write(const SweepCtx *args)
{
    QueryContext *context = args->context;
    const VerbaLinea *text = args->text;
    char path[QUERY_PATH_MAX_LEN];
    EMBED_CALL(sweep_path, SweepCtx, .context_read = context, .relative = args->relative, .path = path);
    static unsigned char s_was[4096];
    const unsigned long long was = qry_member_file_read(path, s_was, sizeof(s_was));
    const int same =
        (was == text->at) && (memor.cmp(&(MemoriaCfg){.src = s_was, .other = text->out, .bytes = was}) == 0);
    const int written = same || EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                           .path = path, .bytes = text->out, .size = text->at);
    char print_buf[QUERY_PATH_MAX_LEN + 64u];
    VerbaLinea line = {print_buf, sizeof(print_buf), 0ull};
    mmgr_line_put(&line, "  ");
    mmgr_line_put(&line, args->relative);
    mmgr_line_put(&line, same ? " stays as it was" : (written ? " written" : " could not be written"));
    (void)mmgr_line_finish(&line);
    EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = print_buf);
}

// every sweep recorded walked, each entry's timings gathered, and qry_host.h and files_host.js written from their
// medians: 0, or 1 where fewer than QUERY_SWEEP_LEAST sweeps report
EMBED_INLINE int sweep_pick(const SweepCtx *args)
{
    QueryContext *context = args->context;
    const QueryWriter *own = args->own;
    char folder[QUERY_PATH_MAX_LEN];
    EMBED_CALL(sweep_path, SweepCtx, .context_read = context, .relative = QUERY_SWEEPS, .path = folder);
    QueryRuns runs;
    (void)EMBED_CALL(query_folder_runs_internal.runs_take, QueryFolderRunsCtx, .folder = folder, .runs = &runs);
    const unsigned int room = runs.count * QUERY_SWEEP_ROUNDS;
    unsigned long long *const rates =
        (room != 0u)
            ? g_cli_cells->persistent_buf_alloc((unsigned long long)QUERY_SWEEP_ENTRIES * room * 8ull, &g_query_cells)
            : NULL;
    unsigned int sweeps = 0u;
    for (unsigned int at = 0u; (rates != NULL) && (at < runs.count); at += 1u)
    {
        QueryWriter writer;
        EMBED_CALL(query_retention_own_internal.writer_get, QueryRetentionOwnCtx, .own = own, .qry = runs.path[at],
                   .writer = &writer);
        const unsigned char *blob = NULL;
        unsigned long long size = 0ull;
        if (EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &writer, .name = "sweep",
                       .bytes_u8s = &blob, .size_pointer = &size) &&
            (size == (QUERY_SWEEP_ENTRIES * QUERY_SWEEP_ROUNDS * 8ull)))
        {
            for (unsigned int entry = 0u; entry < QUERY_SWEEP_ENTRIES; entry += 1u)
            {
                memor.cpy(&(MemoriaCfg){.dst = rates + (entry * room) + (sweeps * QUERY_SWEEP_ROUNDS),
                                        .src = blob + (entry * QUERY_SWEEP_ROUNDS * 8u),
                                        .bytes = QUERY_SWEEP_ROUNDS * 8u});
            }
            sweeps += 1u;
        }
    }
    EMBED_CALL(query_folder_runs_internal.runs_free, QueryFolderRunsCtx, .runs = &runs);
    unsigned long long median[QUERY_SWEEP_ENTRIES] = {0ull};
    for (unsigned int entry = 0u; (sweeps >= QUERY_SWEEP_LEAST) && (entry < QUERY_SWEEP_ENTRIES); entry += 1u)
    {
        // a timing of 0 is an entry that did nothing, left out of its median
        unsigned int count = 0u;
        unsigned long long *const timed = rates + (entry * room);
        for (unsigned int at = 0u; at < (sweeps * QUERY_SWEEP_ROUNDS); at += 1u)
        {
            timed[count] = timed[at];
            count += (timed[at] != 0ull) ? 1u : 0u;
        }
        median[entry] = EMBED_CALL(sweep_median, SweepCtx, .timed = timed, .count = count);
    }
    g_cli_cells->persistent_buf_release(rates, &g_query_cells);
    const unsigned long long region =
        EMBED_CALL(sweep_take, SweepCtx, .what = "region", .near = 0ull, .median = median);
    const unsigned long long write = EMBED_CALL(sweep_take, SweepCtx, .what = "write", .near = 0ull, .median = median);
    const unsigned long long slice =
        EMBED_CALL(sweep_take, SweepCtx, .what = "slice", .near = QUERY_SWEEP_SLICE_NEAR, .median = median);
    if ((region == 0ull) || (write == 0ull) || (slice == 0ull))
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context,
                   .text = "  fewer sweeps report than the host's sizes are written from");
        return 1;
    }
    char text[1024];
    VerbaLinea header = {text, sizeof(text), 0ull};
    mmgr_line_put(&header,
                  "// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR "
                  "LicenseRef-Educational\n// qry_host.h: this host's steps for the run's writer, the fastest "
                  "of each size's median over the host's\n// sweeps, written by `orior_cli protocol sweep` and "
                  "read by qry_writer_internal.h\n#ifndef QRY_HOST_H\n#define QRY_HOST_H\n\n#define "
                  "QRY_WRITER_PIECE_BYTES ");
    mmgr_line_decimal(&header, write, 0u);
    mmgr_line_put(&header, "ull\n\n#endif\n");
    EMBED_CALL(sweep_on_write, SweepCtx, .context = context, .relative = QUERY_SWEEP_HOST_HEADER, .text = &header);
    VerbaLinea module = {text, sizeof(text), 0ull};
    mmgr_line_put(&module, "// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR "
                           "LicenseRef-Educational\n\n// The file window's sizes on this host, the slice the fastest "
                           "of each size's median over the\n// host's sweeps, or within a twentieth of it and smaller, "
                           "written by `orior_cli protocol sweep`:\n// a window HALF bytes either side of a line, read "
                           "outward a SLICE at a time.\nexport const HALF = ");
    mmgr_line_decimal(&module, slice / 2ull, 0u);
    mmgr_line_put(&module, ";\nexport const SLICE = ");
    mmgr_line_decimal(&module, slice, 0u);
    mmgr_line_put(&module, ";\n");
    EMBED_CALL(sweep_on_write, SweepCtx, .context = context, .relative = QUERY_SWEEP_WINDOW_MODULE, .text = &module);
    return 0;
}

int query_sweep_run(const QueryLoopCfg *args)
{
    QueryContext *context = args->context;
    VerbaLinea *err_msg = args->err_msg;
    if (args->count != 0u)
    {
        mmgr_line_put(err_msg, "sweep takes no words");
        return -1;
    }
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "QRY", .value = "");
    static QueryRun s_run;
    if (!EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_run, .folder = QUERY_SWEEPS, .err_msg = err_msg))
    {
        return -1;
    }
    EMBED_CALL(sweep_path, SweepCtx, .context_read = context, .relative = "build/qry_sweep/scratch.bin",
               .path = s_scratch);
    s_bytes = g_cli_cells->persistent_buf_alloc(QUERY_SWEEP_READ_BYTES, &g_query_cells);
    static unsigned long long s_rates[QUERY_SWEEP_ENTRIES][QUERY_SWEEP_ROUNDS];
    const int timed = s_bytes != NULL;
    if (timed)
    {
        for (unsigned long long at = 0ull; at < QUERY_SWEEP_READ_BYTES; at += 1ull)
        {
            // the low byte of a scattered count
            s_bytes[at] = (unsigned char)((at * 2654435761ull) >> 13u);
        }
        // the first entry done once before any timing is recorded, to warm the pages and the caches
        (void)EMBED_CALL(sweep_time, SweepCtx, .entry = &s_entries[0]);
        for (unsigned int round = 0u; round < QUERY_SWEEP_ROUNDS; round += 1u)
        {
            for (unsigned int step = 0u; step < QUERY_SWEEP_ENTRIES; step += 1u)
            {
                const unsigned int entry = (round + step) % QUERY_SWEEP_ENTRIES;
                s_rates[entry][round] = EMBED_CALL(sweep_time, SweepCtx, .entry = &s_entries[entry]);
            }
        }
        (void)qry_member_append("sweep", s_rates, sizeof(s_rates), QRY_CRITICAL);
    }
    g_cli_cells->persistent_buf_release(s_bytes, &g_query_cells);
    s_bytes = NULL;
    const QueryWriter own = s_run.writer;
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_run,
                    .err_msg = err_msg))
    {
        return -1;
    }
    if (!timed)
    {
        mmgr_line_put(err_msg, "the sweep's bytes could not be taken");
        return -1;
    }
    return EMBED_CALL(sweep_pick, SweepCtx, .context = context, .own = &own);
}

void query_swept_again(const QueryRetentionContextCtx *args)
{
    QueryContext *context = args->context;
    char text[32];
    unsigned long long hours = 0ull;
    const int given = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx,
                                 .name = "QRY_SWEEP_HOURS", .value_out = text, .room = sizeof(text));
    hours = given ? EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = text, .at = 0ull,
                               .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), .end = NULL)
                  : 0ull;
    hours = (hours != 0ull) ? hours : 12ull;
    char folder[QUERY_PATH_MAX_LEN];
    EMBED_CALL(sweep_path, SweepCtx, .context_read = context, .relative = QUERY_SWEEPS, .path = folder);
    QueryRuns runs;
    (void)EMBED_CALL(query_folder_runs_internal.runs_take, QueryFolderRunsCtx, .folder = folder, .runs = &runs);
    int within = 0;
    for (unsigned int at = 0u; !within && (at < runs.count); at += 1u)
    {
        unsigned long long seconds = 0ull;
        within = EMBED_CALL(query_path_seconds_internal.modify, QueryPathSecondsCtx, .path = runs.path[at],
                            .seconds = &seconds) &&
                 ((seconds + (hours * 3600ull)) > query_now());
    }
    EMBED_CALL(query_folder_runs_internal.runs_free, QueryFolderRunsCtx, .runs = &runs);
    char why[QUERY_LINE_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (within)
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context,
                   .text = "  a sweep of the host is newer than QRY_SWEEP_HOURS: none is run");
    }
    else if ((EMBED_CALL(query_loop.sweep_run, QueryLoopCfg, .context = context, .words = NULL, .count = 0u,
                         .err_msg = &err_msg) < 0) &&
             (mmgr_line_finish(&err_msg) != 0ull))
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = why);
    }
}
