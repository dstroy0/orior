// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_folder.c: a folder's runs: each numbered newest first, the next one's name stamped by the clock, a path named
// under the top of the tree, a run read back out of its crystal, and only the newest QRY_RETAINED retained
#include "../../../cli/src/command_line/callback_cfg.h"
#include "../query_loops.h"

#include "../../../cli/src/text/text.h"
#include <stdlib.h>

// What the walk over a folder's runs records: the folder, the numbers and paths found, their count and room, and the
// highest number found
typedef struct
{
    const char *folder;
    unsigned long long *number;
    char (*path)[QUERY_PATH_MAX_LEN];
    unsigned int count;
    unsigned int room;
    unsigned long long highest;
} QueryRunsWalk;

typedef struct
{
    const char *name;
    int is_folder;
    unsigned long long days;
    unsigned long long *year;
    unsigned long long *month;
    unsigned long long *day;
} RunFolderCtx;
// a run's entry `query<n>-<datetime>.qry`, or the folder of its crystal: its number, or 0 where it is no run
EMBED_INLINE unsigned long long query_run_number(const RunFolderCtx *args)
{
    if (!cellul.starts(&(CatenaFinitaCfg){.src = args->name, .other = "query", .cap = CLI_CELLS_POOL}))
    {
        return 0ull;
    }
    const char *const rest = args->name + 5;
    const size_t digits = EMBED_CALL(cli_text_set_table.over, CliTextSetCfg, .text = rest, .set = "0123456789");
    if ((digits == 0u) || (rest[digits] != '-'))
    {
        return 0ull;
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    const int qry =
        (length > 4u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL}));
    if (qry == args->is_folder)
    {
        return 0ull;
    }
    unsigned long long number = 0ull;
    for (size_t at = 0u; at < digits; at += 1u)
    {
        number = (number * 10ull) + (unsigned long long)(rest[at] - '0');
    }
    return number;
}

static void query_run_walk(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    int is_folder = args->is_folder;
    QueryRunsWalk *const walk = (QueryRunsWalk *)args->context;
    const unsigned long long number = EMBED_CALL(query_run_number, RunFolderCtx, .name = name, .is_folder = is_folder);
    // a walk handed no storage counts the runs and finds the highest number
    if ((number != 0ull) && (walk->path == NULL))
    {
        walk->count += 1u;
        walk->highest = (number > walk->highest) ? number : walk->highest;
        return;
    }
    if ((number == 0ull) || (walk->count >= walk->room))
    {
        return;
    }
    VerbaLinea line = {walk->path[walk->count], QUERY_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, walk->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    if (mmgr_line_finish(&line) == 0ull)
    {
        return;
    }
    // sorted newest first: the highest number at the front
    char saved_path[QUERY_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = saved_path, .src = walk->path[walk->count], .bytes = QUERY_PATH_MAX_LEN});
    unsigned int at = walk->count;
    while ((at > 0u) && (walk->number[at - 1u] < number))
    {
        walk->number[at] = walk->number[at - 1u];
        memor.cpy(&(MemoriaCfg){.dst = walk->path[at], .src = walk->path[at - 1u], .bytes = QUERY_PATH_MAX_LEN});
        at -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = walk->path[at], .src = saved_path, .bytes = QUERY_PATH_MAX_LEN});
    walk->number[at] = number;
    walk->count += 1u;
}

unsigned int query_runs_each(const QueryFolderNumberCtx *args)
{
    QueryRunsWalk walk = {args->folder, args->number, args->path_callback, 0u, args->room_u32, 0ull};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = query_run_walk, .context_bytess = &walk);
    return walk.count;
}

unsigned int query_runs_count(const QueryFolderMostCtx *args)
{
    QueryRunsWalk counting = {args->folder, NULL, NULL, 0u, 0u, 0ull};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = query_run_walk, .context_bytess = &counting);
    return counting.count;
}

int query_runs_take(const QueryFolderRunsCtx *args)
{
    QueryRuns *runs = args->runs;
    *runs = (QueryRuns){NULL, NULL, 0u};
    QueryRunsWalk counting = {args->folder, NULL, NULL, 0u, 0u, 0ull};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = query_run_walk, .context_bytess = &counting);
    if (counting.count == 0u)
    {
        return 1;
    }
    runs->number = g_cli_cells->persistent_buf_alloc((unsigned long long)counting.count * sizeof(unsigned long long),
                                                     &g_query_cells);
    runs->path =
        (runs->number != NULL)
            ? g_cli_cells->persistent_buf_alloc((unsigned long long)counting.count * QUERY_PATH_MAX_LEN, &g_query_cells)
            : NULL;
    if (runs->path == NULL)
    {
        EMBED_CALL(query_folder_runs_internal.runs_free, QueryFolderRunsCtx, .runs = runs);
        return 0;
    }
    runs->count = EMBED_CALL(query_folder_number_internal.runs_each, QueryFolderNumberCtx, .folder = args->folder,
                             .number = runs->number, .path_callback = runs->path, .room_u32 = counting.count);
    return 1;
}

void query_runs_free(const QueryFolderRunsCtx *args)
{
    QueryRuns *runs = args->runs;
    g_cli_cells->persistent_buf_release(runs->number, &g_query_cells);
    g_cli_cells->persistent_buf_release(runs->path, &g_query_cells);
    *runs = (QueryRuns){NULL, NULL, 0u};
}

// the civil date of a count of days from 1970-01-01
EMBED_INLINE void query_civil(const RunFolderCtx *args)
{
    unsigned long long *year = args->year;
    unsigned long long *month = args->month;
    unsigned long long *day = args->day;
    const unsigned long long z = args->days + 719468ull;
    const unsigned long long era = z / 146097ull;
    const unsigned long long doe = z - (era * 146097ull);
    const unsigned long long yoe = (doe - (doe / 1460ull) + (doe / 36524ull) - (doe / 146096ull)) / 365ull;
    const unsigned long long doy = doe - ((365ull * yoe) + (yoe / 4ull) - (yoe / 100ull));
    const unsigned long long mp = ((5ull * doy) + 2ull) / 153ull;
    *day = doy - (((153ull * mp) + 2ull) / 5ull) + 1ull;
    *month = (mp < 10ull) ? (mp + 3ull) : (mp - 9ull);
    *year = yoe + (era * 400ull) + ((*month <= 2ull) ? 1ull : 0ull);
}

void query_next_name(const QueryFolderRoomCtx *args)
{
    // the highest number of the folder's runs, found by a walk that counts
    QueryRunsWalk counting = {args->folder, NULL, NULL, 0u, 0u, 0ull};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = query_run_walk, .context_bytess = &counting);
    const unsigned long long highest = counting.highest;
    const unsigned long long seconds = query_now();
    unsigned long long year = 0ull;
    unsigned long long month = 0ull;
    unsigned long long day = 0ull;
    EMBED_CALL(query_civil, RunFolderCtx, .days = seconds / 86400ull, .year = &year, .month = &month, .day = &day);
    const unsigned long long time = seconds % 86400ull;
    VerbaLinea line = {args->name_out, args->room, 0ull};
    mmgr_line_put(&line, "query");
    mmgr_line_decimal(&line, highest + 1ull, 0u);
    mmgr_line_ch(&line, '-');
    mmgr_line_decimal(&line, year, 4u);
    mmgr_line_decimal(&line, month, 2u);
    mmgr_line_decimal(&line, day, 2u);
    mmgr_line_ch(&line, 'T');
    mmgr_line_decimal(&line, time / 3600ull, 2u);
    mmgr_line_decimal(&line, (time % 3600ull) / 60ull, 2u);
    mmgr_line_decimal(&line, time % 60ull, 2u);
    mmgr_line_put(&line, ".qry");
    mmgr_line_finish(&line);
}

void query_by_name(const QueryRoomPathNameCtx *args)
{
    char full_top[QUERY_PATH_MAX_LEN];
    char full_path[QUERY_PATH_MAX_LEN];
    const int both = EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = args->top,
                                .full = full_top, .room = sizeof(full_top)) &&
                     EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = args->path,
                                .full = full_path, .room = sizeof(full_path));
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = full_top, .cap = CLI_CELLS_POOL});
    if (both &&
        EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = full_path, .other_pointer = full_top,
                   .size = length) &&
        ((full_path[length] == '/') || (full_path[length] == '\\')))
    {
        VerbaLinea line = {args->name_out, args->room, 0ull};
        (void)EMBED_CALL(cli_text_from.put_swap, CliTextFromCfg, .out = &line, .text = full_path + length + 1u,
                         .from = '\\', .to = '/');
        mmgr_line_finish(&line);
        return;
    }
    const char *base = args->path;
    const unsigned long long base_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    base = (base_past != 0ull) ? (args->path + base_past) : base;
    memor.cpy(&(MemoriaCfg){.dst = args->name_out,
                            .src = base,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) + 1u});
}

int query_open(const QueryRoomQryPathCtx *args)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->qry_path, .cap = CLI_CELLS_POOL});
    if ((length > 4u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->qry_path + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->named,
                          .src = args->qry_path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->qry_path, .cap = CLI_CELLS_POOL}) + 1u});
        return args->named[0] != '\0';
    }
    if (args->crystal == NULL)
    {
        return 0;
    }
    // the crystal read back into `restored/` beside it, as `<name>.qry`
    const char *base = args->qry_path;
    const unsigned long long base_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->qry_path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->qry_path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    base = (base_past != 0ull) ? (args->qry_path + base_past) : base;
    char restored[QUERY_PATH_MAX_LEN];
    VerbaLinea restored_line = {restored, sizeof(restored), 0ull};
    mmgr_line_put_n(&restored_line, args->qry_path, (unsigned long long)(base - args->qry_path));
    mmgr_line_put(&restored_line, "restored");
    VerbaLinea named_line = {args->named, args->room, 0ull};
    mmgr_line_put(&named_line, restored);
    mmgr_line_ch(&named_line, '/');
    mmgr_line_put(&named_line, base);
    mmgr_line_put(&named_line, ".qry");
    if ((mmgr_line_finish(&restored_line) == 0ull) || (mmgr_line_finish(&named_line) == 0ull) ||
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = restored))
    {
        return 0;
    }
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->named))
    {
        return 1;
    }
    const char *const words[5] = {args->crystal, "read", args->qry_path, args->named, NULL};
    QueryProcess read;
    return EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &read, .words = words,
                      .folder = NULL, .output = QUERY_OUTPUT_NONE) &&
           (EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &read) == 0ll);
}

int query_crystal(const QueryRoomRootCtx *args)
{
    char base[64];
    EMBED_CALL(query_name_room_internal.binary_name, QueryNameRoomCtx, .name = "qry_crystal", .binary = base,
               .room = sizeof(base));
    VerbaLinea line = {args->program, args->room, 0ull};
    mmgr_line_put(&line, args->root);
    mmgr_line_put(&line, "/build/qry_crystal/engine/");
    mmgr_line_put(&line, base);
    return (mmgr_line_finish(&line) != 0ull) &&
           EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->program);
}

unsigned int query_retain_max(void)
{
    char retain_text[32];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "QRY_RETAINED",
                    .value_out = retain_text, .room = sizeof(retain_text)))
    {
        return QUERY_RETAIN_MAX;
    }
    char *end = NULL;
    const unsigned long most = strtoul(retain_text, &end, 10);
    // a count recorded is far below what an unsigned int represents
    return ((end != retain_text) && (*end == '\0')) ? (unsigned int)most : QUERY_RETAIN_MAX;
}

void query_runs_prune(const QueryFolderMostCtx *args)
{
    char restored[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {restored, sizeof(restored), 0ull};
    mmgr_line_put(&line, args->folder);
    mmgr_line_put(&line, "/restored");
    if (mmgr_line_finish(&line) != 0ull)
    {
        EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = restored);
    }
    QueryRuns runs;
    (void)EMBED_CALL(query_folder_runs_internal.runs_take, QueryFolderRunsCtx, .folder = args->folder, .runs = &runs);
    for (unsigned int at = args->most; at < runs.count; at += 1u)
    {
        EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = runs.path[at]);
    }
    EMBED_CALL(query_folder_runs_internal.runs_free, QueryFolderRunsCtx, .runs = &runs);
}

int query_latest_run(const QueryLoopsFolderCfg *args)
{
    const char *folder = args->folder;
    const char *name = args->name;
    const QueryWriter *writer = args->writer;
    const char *crystal = args->crystal;
    size_t room = args->room;
    char own_full[QUERY_PATH_MAX_LEN] = "";
    if (args->own != NULL)
    {
        EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = args->own, .full = own_full,
                   .room = sizeof(own_full));
    }
    QueryRuns runs;
    (void)EMBED_CALL(query_folder_runs_internal.runs_take, QueryFolderRunsCtx, .folder = folder, .runs = &runs);
    int latest = 0;
    for (unsigned int at = 0u; !latest && (at < runs.count); at += 1u)
    {
        char named[QUERY_PATH_MAX_LEN];
        char named_full[QUERY_PATH_MAX_LEN];
        if (!EMBED_CALL(query_room_qry_path_internal.open, QueryRoomQryPathCtx, .qry_path = runs.path[at],
                        .crystal = crystal, .named = named, .room = sizeof(named)) ||
            ((args->own != NULL) &&
             EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = named, .full = named_full,
                        .room = sizeof(named_full)) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = named_full, .other = own_full, .cap = CLI_CELLS_POOL}))))
        {
            continue;
        }
        static QueryWriter s_other;
        s_other = *writer;
        s_other.member = 0;
        memor.cpy(&(MemoriaCfg){.dst = s_other.qry,
                                .src = named,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
        const unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        if (EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &s_other, .name = name,
                       .bytes_u8s = &bytes, .size_pointer = &size))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->found,
                                    .src = named,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
            latest = (args->found[0] != '\0') ? 1 : -1;
        }
    }
    EMBED_CALL(query_folder_runs_internal.runs_free, QueryFolderRunsCtx, .runs = &runs);
    return latest == 1;
}
