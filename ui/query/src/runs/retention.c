// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// retention.c: the runs of the tree retained while the host is idle, in a run of their own. Every folder of the tree
// that contains a run is taken in turn, but one whose newest run's writer runs, which a loop is still using: each run
// still a .qry has its writer's account read, its records, its bytes and its time, and the folder's median rate
// reported beside every run that wrote at under half of it; where the part is free, each run but the newest is stored
// as its crystal; only the newest QRY_RETAINED runs of the folder are retained. retention_part.c determines whether the
// part is free and the crystal's program
#include "../../../cli/src/command_line/callback_cfg.h"
#include "../../../cli/src/text/text.h"
#include "retention_internal.h"

// The folders found containing runs: where they are stored, sorted, NULL for a walk that counts them, their count and
// room
typedef struct
{
    char (*found)[QUERY_PATH_MAX_LEN];
    unsigned int founds;
    unsigned int room;
} QueryFoldersFound;
int query_account_read(const QueryRetentionBlobCtx *args)
{
    if (args->size != 48ull)
    {
        return 0;
    }
    unsigned long long word[6];
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        word[at] = 0ull;
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            word[at] |= (unsigned long long)args->blob[(at * 8u) + byte] << (8u * byte);
        }
    }
    args->account->records = word[0];
    args->account->bytes = word[1];
    args->account->corrupt = word[2];
    args->account->unplaced = word[3];
    args->account->panics = word[4];
    // the writer counts nanoseconds, and a run's are far below where a double loses a millisecond
    args->account->ms = (double)word[5] / 1000000.0;
    return 1;
}

double query_account_rate(const QueryRetentionCtx *args)
{
    return (args->account_read->ms > 0.0)
               ? (((double)args->account_read->bytes / 1048576.0) / (args->account_read->ms / 1000.0))
               : 0.0;
}

typedef struct
{
    QueryContext *context;
    const VerbaLinea *line;
    QueryFoldersFound *all;
    const char *folder;
    const QueryWriter *own;
    const char *shown;
} RetentionCtx;
// `line` printed through the context and handed to the run as the stores' console
EMBED_INLINE void query_retention_print(const RetentionCtx *args)
{
    QueryContext *context = args->context;
    if (mmgr_line_ok(args->line))
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context, .text = args->line->out);
    }
}

// The folder's median rate over the runs' rates, `count` of them, and every run under half of it, as lines
void query_median_print(const QueryRetentionContextFolderCtx *args)
{
    QueryContext *context = args->context;
    if (args->count == 0u)
    {
        return;
    }
    // the rates sorted in a cell of their count
    double *const sorted =
        g_cli_cells->persistent_buf_alloc((unsigned long long)args->count * sizeof(double), &g_query_cells);
    if (sorted == NULL)
    {
        return;
    }
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) && (sorted[place - 1u] > args->rates[at]))
        {
            sorted[place] = sorted[place - 1u];
            place -= 1u;
        }
        sorted[place] = args->rates[at];
    }
    const double median = sorted[((args->count + 1u) / 2u) - 1u];
    g_cli_cells->persistent_buf_release(sorted, &g_query_cells);
    char text[QUERY_PATH_MAX_LEN + 128u];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, "  ");
    mmgr_line_put(&line, args->folder);
    mmgr_line_put(&line, ": ");
    mmgr_line_decimal(&line, args->count, 0u);
    mmgr_line_put(&line, " runs read, the median ");
    // a rate rounded to the whole MiB a second, far below what an unsigned long long represents
    mmgr_line_decimal(&line, (unsigned long long)(median + 0.5), 0u);
    mmgr_line_put(&line, " MiB/s");
    mmgr_line_finish(&line);
    EMBED_CALL(query_retention_print, RetentionCtx, .context = context, .line = &line);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (args->rates[at] < (median / 2.0))
        {
            line.at = 0ull;
            mmgr_line_put(&line, "    ");
            mmgr_line_put(&line, args->names[at]);
            mmgr_line_put(&line, " wrote at under half the median");
            mmgr_line_finish(&line);
            EMBED_CALL(query_retention_print, RetentionCtx, .context = context, .line = &line);
        }
    }
}

// One folder walked: the folders found, the folder, and whether it contains a run
typedef struct
{
    QueryFoldersFound *all;
    const char *folder;
    int has_run;
} QueryFoldersLevel;

EMBED_INLINE void query_folder_level(const RetentionCtx *args);

// an entry of a folder walked: a folder under it walked in its turn, .git and every `restored` pruned, and a run's
// .qry marking the folder as one that contains runs
static void query_folder_walk(const CliEachNameIsFolderCfg *args)
{
    QueryFoldersLevel *const level = (QueryFoldersLevel *)args->context;
    if (args->is_folder)
    {
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = ".git", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "restored", .cap = CLI_CELLS_POOL})))
        {
            char path[QUERY_PATH_MAX_LEN];
            VerbaLinea line = {path, sizeof(path), 0ull};
            mmgr_line_put(&line, level->folder);
            mmgr_line_ch(&line, '/');
            mmgr_line_put(&line, args->name);
            if (mmgr_line_finish(&line) != 0ull)
            {
                EMBED_CALL(query_folder_level, RetentionCtx, .all = level->all, .folder = path);
            }
        }
        return;
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    level->has_run =
        level->has_run ||
        ((cellul.starts(&(CatenaFinitaCfg){.src = args->name, .other = "query", .cap = CLI_CELLS_POOL})) &&
         (length > 9u) &&
         (cellul.eq(&(CatenaFinitaCfg){.src = args->name + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})) &&
         (args->name[5] >= '0') && (args->name[5] <= '9') &&
         (cellul.chr(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL, .byte = '-'}) != NULL));
}

// the folder `folder` and every folder under it walked, each that contains a run counted or stored in its sorted place
EMBED_INLINE void query_folder_level(const RetentionCtx *args)
{
    QueryFoldersLevel level = {args->all, args->folder, 0};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
               .each = query_folder_walk, .context_bytess = &level);
    if (!level.has_run)
    {
        return;
    }
    if (args->all->found == NULL)
    {
        args->all->founds += 1u;
        return;
    }
    if (args->all->founds >= args->all->room)
    {
        return;
    }
    unsigned int at = args->all->founds;
    while ((at > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->all->found[at - 1u],
                                    .other_pointer = args->folder) > 0))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->all->found[at], .src = args->all->found[at - 1u], .bytes = QUERY_PATH_MAX_LEN});
        at -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->all->found[at],
                            .src = args->folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL}) + 1u});
    args->all->founds += 1u;
}

unsigned int query_folders_with_runs(const QueryRetentionRootFoundCtx *args)
{
    QueryFoldersFound all = {args->found, 0u, args->room};
    EMBED_CALL(query_folder_level, RetentionCtx, .all = &all, .folder = args->root);
    return all.founds;
}

void query_writer_get(const QueryRetentionOwnCtx *args)
{
    QueryWriter *writer = args->writer;
    *writer = *args->own;
    writer->member = 0;
    // a run's shared memory is named by the full path its run was started under, so the run is named the same way
    if (!EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = args->qry, .full = writer->qry,
                    .room = sizeof(writer->qry)))
    {
        memor.cpy(&(MemoriaCfg){.dst = writer->qry,
                                .src = args->qry,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->qry, .cap = CLI_CELLS_POOL}) + 1u});
    }
}

// Each run of `folder` still a .qry with its writer's account read: a line each, then the folder's median and the runs
// under half of it
EMBED_INLINE void query_writers_read(const RetentionCtx *args)
{
    const QueryWriter *own = args->own;
    const char *folder = args->folder;
    // the runs listed, and the names and rates of those read, each in a cell of the runs' count
    QueryRuns listed;
    (void)EMBED_CALL(query_folder_runs_internal.runs_take, QueryFolderRunsCtx, .folder = folder, .runs = &listed);
    const unsigned int runs = listed.count;
    char (*const names)[QUERY_PATH_MAX_LEN] =
        (runs > 0u) ? g_cli_cells->persistent_buf_alloc((unsigned long long)runs * QUERY_PATH_MAX_LEN, &g_query_cells)
                    : NULL;
    double *const rates_of =
        (names != NULL) ? g_cli_cells->persistent_buf_alloc((unsigned long long)runs * sizeof(double), &g_query_cells)
                        : NULL;
    unsigned int rates = 0u;
    for (unsigned int at = 0u; (rates_of != NULL) && (at < runs); at += 1u)
    {
        const char *const path = listed.path[at];
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        if ((length < 4u) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = path + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        QueryWriter writer;
        EMBED_CALL(query_retention_own_internal.writer_get, QueryRetentionOwnCtx, .own = own, .qry = path,
                   .writer = &writer);
        const unsigned char *blob = NULL;
        unsigned long long size = 0ull;
        QueryAccount account;
        if (!EMBED_CALL(query_count_name_internal.writer_latest, QueryCountNameCtx, .writer = &writer, .name = "writer",
                        .bytes_u8s = &blob, .size_pointer = &size) ||
            !EMBED_CALL(query_retention_blob_internal.account_read, QueryRetentionBlobCtx, .blob = blob, .size = size,
                        .account = &account))
        {
            continue;
        }
        const char *const name =
            EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                       .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .byte = '/')
                ? (EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                              .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}),
                              .byte = '/') +
                   1)
                : path;
        char text[QUERY_PATH_MAX_LEN + 256u];
        VerbaLinea line = {text, sizeof(text), 0ull};
        mmgr_line_put(&line, "    ");
        mmgr_line_put(&line, name);
        mmgr_line_put(&line, ": ");
        mmgr_line_decimal(&line, account.bytes / 1048576ull, 0u);
        mmgr_line_put(&line, " MiB in ");
        mmgr_line_decimal(&line, account.records, 0u);
        mmgr_line_put(&line, " records, ");
        // a run's milliseconds are far below what an unsigned long long represents
        mmgr_line_decimal(&line, (unsigned long long)account.ms, 0u);
        mmgr_line_put(&line, " ms; ");
        mmgr_line_decimal(&line, account.corrupt, 0u);
        mmgr_line_put(&line, " corrupt, ");
        mmgr_line_decimal(&line, account.unplaced, 0u);
        mmgr_line_put(&line, " without room, ");
        mmgr_line_decimal(&line, account.panics, 0u);
        mmgr_line_put(&line, " panics");
        mmgr_line_finish(&line);
        EMBED_CALL(query_retention_print, RetentionCtx, .context = args->context, .line = &line);
        memor.cpy(&(MemoriaCfg){.dst = names[rates],
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        rates_of[rates] =
            EMBED_CALL(query_retention_internal.account_rate, QueryRetentionCtx, .account_read = &account);
        rates += 1u;
    }
    EMBED_CALL(query_retention_context_folder_internal.median_print, QueryRetentionContextFolderCtx,
               .context = args->context, .folder = args->shown, .names = names, .rates = rates_of, .count = rates);
    g_cli_cells->persistent_buf_release(rates_of, &g_query_cells);
    g_cli_cells->persistent_buf_release(names, &g_query_cells);
    EMBED_CALL(query_folder_runs_internal.runs_free, QueryFolderRunsCtx, .runs = &listed);
}

// One folder of runs retained: left as it is where its newest run's writer runs; else its writers' accounts read, its
// runs stored as crystals where `crystal` is given, and only the newest `most` retained
void query_retention_folder(const QueryRetentionContextOwnCtx *args)
{
    const char *folder = args->folder;
    unsigned int most = args->most;
    char shown[QUERY_PATH_MAX_LEN];
    EMBED_CALL(query_room_path_name_internal.by_name, QueryRoomPathNameCtx, .top = args->context->root, .path = folder,
               .name_out = shown, .room = sizeof(shown));
    // the newest run of the folder, the first a walk visits newest first
    QueryRuns listed;
    (void)EMBED_CALL(query_folder_runs_internal.runs_take, QueryFolderRunsCtx, .folder = folder, .runs = &listed);
    char newest_path[QUERY_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = newest_path,
                            .src = (listed.count != 0u) ? listed.path[0] : "",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = (listed.count != 0u) ? listed.path[0] : "",
                                                                   .cap = CLI_CELLS_POOL}) +
                                     1u});
    EMBED_CALL(query_folder_runs_internal.runs_free, QueryFolderRunsCtx, .runs = &listed);
    char text[QUERY_PATH_MAX_LEN + 128u];
    VerbaLinea line = {text, sizeof(text), 0ull};
    if (newest_path[0] != '\0')
    {
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = newest_path, .cap = CLI_CELLS_POOL});
        QueryWriter newest;
        EMBED_CALL(query_retention_own_internal.writer_get, QueryRetentionOwnCtx, .own = args->own, .qry = newest_path,
                   .writer = &newest);
        if ((length > 4u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = newest_path + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})) &&
            EMBED_CALL(query_writer_internal.writer_runs, QueryWriterCtx, .writer = &newest))
        {
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, shown);
            mmgr_line_put(&line, ": left as it is, its newest run's writer runs");
            mmgr_line_finish(&line);
            EMBED_CALL(query_retention_print, RetentionCtx, .context = args->context, .line = &line);
            return;
        }
    }
    EMBED_CALL(query_writers_read, RetentionCtx, .context = args->context, .own = args->own, .folder = folder,
               .shown = shown);
    if (args->crystal != NULL)
    {
        EMBED_CALL(query_retention_context_own_internal.crystals, QueryRetentionContextOwnCtx, .context = args->context,
                   .own = args->own, .folder = folder, .crystal = args->crystal);
    }
    const unsigned int before = EMBED_CALL(query_folder_most_internal.runs_count, QueryFolderMostCtx, .folder = folder);
    EMBED_CALL(query_folder_most_internal.runs_prune, QueryFolderMostCtx, .folder = folder, .most = most);
    const unsigned int runs = EMBED_CALL(query_folder_most_internal.runs_count, QueryFolderMostCtx, .folder = folder);
    line.at = 0ull;
    mmgr_line_put(&line, "  ");
    mmgr_line_put(&line, shown);
    mmgr_line_put(&line, ": ");
    mmgr_line_decimal(&line, runs, 0u);
    mmgr_line_put(&line, " of ");
    mmgr_line_decimal(&line, before, 0u);
    mmgr_line_put(&line, " runs retained");
    mmgr_line_finish(&line);
    EMBED_CALL(query_retention_print, RetentionCtx, .context = args->context, .line = &line);
}

void query_crystals_reject(const QueryRetentionContextFreePartCtx *args)
{
    QueryContext *context = args->context;
    if (!args->free_part && (args->why[0] != '\0'))
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context,
                   .text = "  no run is stored as its crystal this time:");
        char text[QUERY_LINE_MAX_LEN + 4u];
        VerbaLinea line = {text, sizeof(text), 0ull};
        mmgr_line_put(&line, "  ");
        mmgr_line_put(&line, args->why);
        mmgr_line_finish(&line);
        EMBED_CALL(query_retention_print, RetentionCtx, .context = context, .line = &line);
    }
    else if (args->free_part && !args->built)
    {
        EMBED_CALL(query_text_internal.print, QueryTextCtx, .context = context,
                   .text = "  no run is stored as its crystal this time: the crystal's program is not built");
    }
}

int query_retention_run(const QueryLoopCfg *args)
{
    QueryContext *context = args->context;
    VerbaLinea *err_msg = args->err_msg;
    if (args->count != 0u)
    {
        mmgr_line_put(err_msg, "retention takes no words");
        return -1;
    }
    EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "QRY", .value = "");
    static QueryRun s_own;
    if (!EMBED_CALL(query_loops_context_run.context_start, QueryLoopsContextRunCfg, .context = context,
                    .run_out = &s_own, .folder = "build/qry_retention/qry", .err_msg = err_msg))
    {
        return -1;
    }
    char why[QUERY_LINE_MAX_LEN];
    VerbaLinea why_line = {why, sizeof(why), 0ull};
    const int free_part = EMBED_CALL(query_retention_context_err_msg_internal.part_free, QueryRetentionContextErrMsgCtx,
                                     .context = context, .err_msg = &why_line);
    static char s_crystal[QUERY_PATH_MAX_LEN];
    const int built = free_part && EMBED_CALL(query_retention_context_internal.crystal_build, QueryRetentionContextCtx,
                                              .context = context, .binary = s_crystal);
    EMBED_CALL(query_retention_context_free_part_internal.crystals_reject, QueryRetentionContextFreePartCtx,
               .context = context, .free_part = free_part, .why = (mmgr_line_finish(&why_line) != 0ull) ? why : "",
               .built = built);
    // the folders containing runs counted, then found into a cell of their count
    const unsigned int counted =
        EMBED_CALL(query_retention_root_found_internal.folders_with_runs, QueryRetentionRootFoundCtx,
                   .root = context->root, .found = NULL, .room = 0u);
    char (*const folders_found)[QUERY_PATH_MAX_LEN] =
        (counted > 0u)
            ? g_cli_cells->persistent_buf_alloc((unsigned long long)counted * QUERY_PATH_MAX_LEN, &g_query_cells)
            : NULL;
    const unsigned int folders =
        (folders_found != NULL)
            ? EMBED_CALL(query_retention_root_found_internal.folders_with_runs, QueryRetentionRootFoundCtx,
                         .root = context->root, .found = folders_found, .room = counted)
            : 0u;
    for (unsigned int at = 0u; at < folders; at += 1u)
    {
        EMBED_CALL(query_retention_context_own_internal.folder, QueryRetentionContextOwnCtx, .context = context,
                   .folder = folders_found[at], .crystal = built ? s_crystal : NULL, .most = query_retain_max(),
                   .own = &s_own.writer);
    }
    g_cli_cells->persistent_buf_release(folders_found, &g_query_cells);
    if (!EMBED_CALL(query_loops_context_run.context_end, QueryLoopsContextRunCfg, .context = context, .run_out = &s_own,
                    .err_msg = err_msg))
    {
        return -1;
    }
    EMBED_CALL(query_retention_context_internal.swept_again, QueryRetentionContextCtx, .context = context);
    return 0;
}
