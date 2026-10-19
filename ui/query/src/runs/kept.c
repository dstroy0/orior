// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kept.c: the runs of the tree kept while the host is idle, in a run of their own. Every folder of the tree that holds
// a run is taken in turn, but one whose newest run's writer runs, which a loop is still using: each run still a .qry
// has its writer's account read, its records, its bytes and its time, and the folder's median rate said beside every
// run that wrote at under half of it; where the part is free, each run but the newest is kept as its crystal; only the
// newest QRY_KEPT runs of the folder are kept. kept_part.c holds whether the part is free and the crystal's program
#include "../../../cli/src/text/text.h"
#include "kept_internal.h"

int query_account_read(const unsigned char *blob, unsigned long long size, QueryAccount *account)
{
    if (size != 48ull)
    {
        return 0;
    }
    unsigned long long word[6];
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        word[at] = 0ull;
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            word[at] |= (unsigned long long)blob[(at * 8u) + byte] << (8u * byte);
        }
    }
    account->records = word[0];
    account->bytes = word[1];
    account->corrupt = word[2];
    account->unplaced = word[3];
    account->panics = word[4];
    // the writer counts nanoseconds, and a run's are far below where a double loses a millisecond
    account->ms = (double)word[5] / 1000000.0;
    return 1;
}

double query_account_rate(const QueryAccount *account)
{
    return (account->ms > 0.0) ? (((double)account->bytes / 1048576.0) / (account->ms / 1000.0)) : 0.0;
}

// `line` said through the context and handed to the run as the keepers' console
static void query_kept_said(QueryContext *context, const CliLine *line)
{
    if (cli_line_fits(line))
    {
        query_said(context, line->out);
    }
}

// The folder's median rate over the runs' rates, `count` of them, and every run under half of it, as lines
void query_median_said(QueryContext *context, const char *folder, char (*names)[QUERY_PATH_LONGEST],
                       const double *rates, unsigned int count)
{
    if (count == 0u)
    {
        return;
    }
    // the rates sorted in a cell of their count
    double *const sorted =
        cli_cell_taken_by(&g_query_cells, (unsigned long long)count * sizeof(double), "the runs' rates sorted");
    if (sorted == NULL)
    {
        return;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) && (sorted[place - 1u] > rates[at]))
        {
            sorted[place] = sorted[place - 1u];
            place -= 1u;
        }
        sorted[place] = rates[at];
    }
    const double median = sorted[((count + 1u) / 2u) - 1u];
    cli_cell_given_by(&g_query_cells, sorted);
    char text[QUERY_PATH_LONGEST + 128u];
    CliLine line = {text, sizeof(text), 0ull};
    cli_line_text(&line, "  ");
    cli_line_text(&line, folder);
    cli_line_text(&line, ": ");
    cli_line_decimal(&line, count, 0u);
    cli_line_text(&line, " runs read, the median ");
    // a rate rounded to the whole MiB a second, far below what an unsigned long long holds
    cli_line_decimal(&line, (unsigned long long)(median + 0.5), 0u);
    cli_line_text(&line, " MiB/s");
    cli_line_closed(&line);
    query_kept_said(context, &line);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (rates[at] < (median / 2.0))
        {
            line.at = 0ull;
            cli_line_text(&line, "    ");
            cli_line_text(&line, names[at]);
            cli_line_text(&line, " wrote at under half the median");
            cli_line_closed(&line);
            query_kept_said(context, &line);
        }
    }
}

// The folders found holding runs: where they are kept, sorted, NULL for a walk that counts them, their count and room
typedef struct
{
    char (*found)[QUERY_PATH_LONGEST];
    unsigned int founds;
    unsigned int room;
} QueryFoldersFound;

// One folder walked: the folders found, the folder, and whether it holds a run
typedef struct
{
    QueryFoldersFound *all;
    const char *folder;
    int holds;
} QueryFoldersLevel;

static void query_folder_level(QueryFoldersFound *all, const char *folder);

// an entry of a folder walked: a folder under it walked in its turn, .git and every `restored` pruned, and a run's
// .qry marking the folder as one that holds runs
static void query_folder_walked(void *context, const char *name, int is_folder)
{
    QueryFoldersLevel *const level = (QueryFoldersLevel *)context;
    if (is_folder)
    {
        if ((!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_CELLS_POOL})) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "restored", .cap = CLI_CELLS_POOL})))
        {
            char path[QUERY_PATH_LONGEST];
            CliLine line = {path, sizeof(path), 0ull};
            cli_line_text(&line, level->folder);
            cli_line_character(&line, '/');
            cli_line_text(&line, name);
            if (cli_line_closed(&line) != 0ull)
            {
                query_folder_level(level->all, path);
            }
        }
        return;
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    level->holds =
        level->holds ||
        ((cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "query", .cap = CLI_CELLS_POOL})) && (length > 9u) &&
         (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})) &&
         (name[5] >= '0') && (name[5] <= '9') &&
         (cellul.chr(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL, .byte = '-'}) != NULL));
}

// the folder `folder` and every folder under it walked, each that holds a run counted or kept in its sorted place
static void query_folder_level(QueryFoldersFound *all, const char *folder)
{
    QueryFoldersLevel level = {all, folder, 0};
    query_folder_each(folder, query_folder_walked, &level);
    if (!level.holds)
    {
        return;
    }
    if (all->found == NULL)
    {
        all->founds += 1u;
        return;
    }
    if (all->founds >= all->room)
    {
        return;
    }
    unsigned int at = all->founds;
    while ((at > 0u) && (cli_text_order(all->found[at - 1u], folder) > 0))
    {
        memor.cpy(&(MemoriaCfg){.dst = all->found[at], .src = all->found[at - 1u], .bytes = QUERY_PATH_LONGEST});
        at -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = all->found[at],
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
    all->founds += 1u;
}

unsigned int query_folders_with_runs(const char *root, char (*found)[QUERY_PATH_LONGEST], unsigned int room)
{
    QueryFoldersFound all = {found, 0u, room};
    query_folder_level(&all, root);
    return all.founds;
}

void query_writer_of(const QueryWriter *own, const char *qry, QueryWriter *writer)
{
    *writer = *own;
    writer->member = 0;
    // a run's shared memory is named by the full path its run was started under, so the run is named the same way
    if (!query_path_full(qry, writer->qry, sizeof(writer->qry)))
    {
        memor.cpy(&(MemoriaCfg){.dst = writer->qry,
                                .src = qry,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = qry, .cap = CLI_CELLS_POOL}) + 1u});
    }
}

// Each run of `folder` still a .qry with its writer's account read: a line each, then the folder's median and the runs
// under half of it
static void query_writers_read(QueryContext *context, const QueryWriter *own, const char *folder, const char *shown)
{
    // the runs listed, and the names and rates of those read, each in a cell of the runs' count
    QueryRuns listed;
    (void)query_runs_taken(folder, &listed);
    const unsigned int runs = listed.count;
    char (*const names)[QUERY_PATH_LONGEST] =
        (runs > 0u) ? cli_cell_taken_by(&g_query_cells, (unsigned long long)runs * QUERY_PATH_LONGEST,
                                        "the names of the runs read")
                    : NULL;
    double *const rates_of =
        (names != NULL)
            ? cli_cell_taken_by(&g_query_cells, (unsigned long long)runs * sizeof(double), "the rates of the runs read")
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
        query_writer_of(own, path, &writer);
        const unsigned char *blob = NULL;
        unsigned long long size = 0ull;
        QueryAccount account;
        if (!query_writer_latest(&writer, "writer", &blob, &size) || !query_account_read(blob, size, &account))
        {
            continue;
        }
        const char *const name =
            cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/')
                ? (cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/') + 1)
                : path;
        char text[QUERY_PATH_LONGEST + 256u];
        CliLine line = {text, sizeof(text), 0ull};
        cli_line_text(&line, "    ");
        cli_line_text(&line, name);
        cli_line_text(&line, ": ");
        cli_line_decimal(&line, account.bytes / 1048576ull, 0u);
        cli_line_text(&line, " MiB in ");
        cli_line_decimal(&line, account.records, 0u);
        cli_line_text(&line, " records, ");
        // a run's milliseconds are far below what an unsigned long long holds
        cli_line_decimal(&line, (unsigned long long)account.ms, 0u);
        cli_line_text(&line, " ms; ");
        cli_line_decimal(&line, account.corrupt, 0u);
        cli_line_text(&line, " corrupt, ");
        cli_line_decimal(&line, account.unplaced, 0u);
        cli_line_text(&line, " without room, ");
        cli_line_decimal(&line, account.panics, 0u);
        cli_line_text(&line, " panics");
        cli_line_closed(&line);
        query_kept_said(context, &line);
        memor.cpy(&(MemoriaCfg){.dst = names[rates],
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        rates_of[rates] = query_account_rate(&account);
        rates += 1u;
    }
    query_median_said(context, shown, names, rates_of, rates);
    cli_cell_given_by(&g_query_cells, rates_of);
    cli_cell_given_by(&g_query_cells, names);
    query_runs_given(&listed);
}

// One folder of runs kept: left as it is where its newest run's writer runs; else its writers' accounts read, its runs
// kept as crystals where `crystal` is given, and only the newest `most` kept
void query_kept_folder(QueryContext *context, const char *folder, const char *crystal, unsigned int most,
                       const QueryWriter *own)
{
    char shown[QUERY_PATH_LONGEST];
    query_named(context->root, folder, shown, sizeof(shown));
    // the newest run of the folder, the first a walk keeps newest first
    QueryRuns listed;
    (void)query_runs_taken(folder, &listed);
    char newest_path[QUERY_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = newest_path,
                            .src = (listed.count != 0u) ? listed.path[0] : "",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = (listed.count != 0u) ? listed.path[0] : "",
                                                                   .cap = CLI_CELLS_POOL}) +
                                     1u});
    query_runs_given(&listed);
    char text[QUERY_PATH_LONGEST + 128u];
    CliLine line = {text, sizeof(text), 0ull};
    if (newest_path[0] != '\0')
    {
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = newest_path, .cap = CLI_CELLS_POOL});
        QueryWriter newest;
        query_writer_of(own, newest_path, &newest);
        if ((length > 4u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = newest_path + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})) &&
            query_writer_runs(&newest))
        {
            cli_line_text(&line, "  ");
            cli_line_text(&line, shown);
            cli_line_text(&line, ": left as it is, its newest run's writer runs");
            cli_line_closed(&line);
            query_kept_said(context, &line);
            return;
        }
    }
    query_writers_read(context, own, folder, shown);
    if (crystal != NULL)
    {
        query_crystals(context, own, folder, crystal);
    }
    const unsigned int before = query_runs_counted(folder);
    query_kept(folder, most);
    const unsigned int runs = query_runs_counted(folder);
    line.at = 0ull;
    cli_line_text(&line, "  ");
    cli_line_text(&line, shown);
    cli_line_text(&line, ": ");
    cli_line_decimal(&line, runs, 0u);
    cli_line_text(&line, " of ");
    cli_line_decimal(&line, before, 0u);
    cli_line_text(&line, " runs kept");
    cli_line_closed(&line);
    query_kept_said(context, &line);
}

void query_crystals_refused(QueryContext *context, int free_part, const char *why, int built)
{
    if (!free_part && (why[0] != '\0'))
    {
        query_said(context, "  no run is kept as its crystal this time:");
        char text[QUERY_SAID_LONGEST + 4u];
        CliLine line = {text, sizeof(text), 0ull};
        cli_line_text(&line, "  ");
        cli_line_text(&line, why);
        cli_line_closed(&line);
        query_kept_said(context, &line);
    }
    else if (free_part && !built)
    {
        query_said(context, "  no run is kept as its crystal this time: the crystal's program is not built");
    }
}

int query_kept_run(QueryContext *context, const char *const *words, unsigned int count, CliLine *said)
{
    (void)words;
    if (count != 0u)
    {
        cli_line_text(said, "kept takes no words");
        return -1;
    }
    query_environment_set("QRY", "");
    static QueryRun s_own;
    if (!query_context_started(context, &s_own, "build/qry_kept/qry", said))
    {
        return -1;
    }
    char why[QUERY_SAID_LONGEST];
    CliLine why_line = {why, sizeof(why), 0ull};
    const int free_part = query_part_free(context, &why_line);
    static char s_crystal[QUERY_PATH_LONGEST];
    const int built = free_part && query_crystal_built(context, s_crystal);
    query_crystals_refused(context, free_part, (cli_line_closed(&why_line) != 0ull) ? why : "", built);
    // the folders holding runs counted, then found into a cell of their count
    const unsigned int counted = query_folders_with_runs(context->root, NULL, 0u);
    char (*const folders_found)[QUERY_PATH_LONGEST] =
        (counted > 0u) ? cli_cell_taken_by(&g_query_cells, (unsigned long long)counted * QUERY_PATH_LONGEST,
                                           "the folders holding runs")
                       : NULL;
    const unsigned int folders =
        (folders_found != NULL) ? query_folders_with_runs(context->root, folders_found, counted) : 0u;
    for (unsigned int at = 0u; at < folders; at += 1u)
    {
        query_kept_folder(context, folders_found[at], built ? s_crystal : NULL, query_kept_most(), &s_own.writer);
    }
    cli_cell_given_by(&g_query_cells, folders_found);
    if (!query_context_ended(context, &s_own, said))
    {
        return -1;
    }
    query_swept_again(context);
    return 0;
}
