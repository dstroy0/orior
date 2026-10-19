// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_folder.c: a folder's runs: each numbered newest first, the next one's name stamped by the clock, a path named
// under the top of the tree, a run read back out of its crystal, and only the newest QRY_KEPT kept
#include "../query_loops.h"

#include "../../../cli/src/text/text.h"
#include <stdlib.h>

// What the walk over a folder's runs keeps: the folder, the numbers and paths found, their count and room, and the
// highest number found
typedef struct
{
    const char *folder;
    unsigned long long *number;
    char (*path)[QUERY_PATH_LONGEST];
    unsigned int count;
    unsigned int room;
    unsigned long long highest;
} QueryRunsWalk;

// a run's entry `query<n>-<datetime>.qry`, or the folder of its crystal: its number, or 0 where it is no run
static unsigned long long query_run_number(const char *name, int is_folder)
{
    if (!cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "query", .cap = CLI_CELLS_POOL}))
    {
        return 0ull;
    }
    const char *const rest = name + 5;
    const size_t digits = cli_text_over(rest, "0123456789");
    if ((digits == 0u) || (rest[digits] != '-'))
    {
        return 0ull;
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    const int qry = (length > 4u) &&
                    (cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL}));
    if (qry == is_folder)
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

static void query_run_walked(void *context, const char *name, int is_folder)
{
    QueryRunsWalk *const walk = (QueryRunsWalk *)context;
    const unsigned long long number = query_run_number(name, is_folder);
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
    CliLine line = {walk->path[walk->count], QUERY_PATH_LONGEST, 0ull};
    cli_line_text(&line, walk->folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    if (cli_line_closed(&line) == 0ull)
    {
        return;
    }
    // kept newest first: the highest number at the front
    char held[QUERY_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = held, .src = walk->path[walk->count], .bytes = QUERY_PATH_LONGEST});
    unsigned int at = walk->count;
    while ((at > 0u) && (walk->number[at - 1u] < number))
    {
        walk->number[at] = walk->number[at - 1u];
        memor.cpy(&(MemoriaCfg){.dst = walk->path[at], .src = walk->path[at - 1u], .bytes = QUERY_PATH_LONGEST});
        at -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = walk->path[at], .src = held, .bytes = QUERY_PATH_LONGEST});
    walk->number[at] = number;
    walk->count += 1u;
}

unsigned int query_runs_each(const char *folder, unsigned long long *number, char (*path)[QUERY_PATH_LONGEST],
                             unsigned int room)
{
    QueryRunsWalk walk = {folder, number, path, 0u, room, 0ull};
    query_folder_each(folder, query_run_walked, &walk);
    return walk.count;
}

unsigned int query_runs_counted(const char *folder)
{
    QueryRunsWalk counting = {folder, NULL, NULL, 0u, 0u, 0ull};
    query_folder_each(folder, query_run_walked, &counting);
    return counting.count;
}

int query_runs_taken(const char *folder, QueryRuns *runs)
{
    *runs = (QueryRuns){NULL, NULL, 0u};
    QueryRunsWalk counting = {folder, NULL, NULL, 0u, 0u, 0ull};
    query_folder_each(folder, query_run_walked, &counting);
    if (counting.count == 0u)
    {
        return 1;
    }
    runs->number = cli_cell_taken_by(&g_query_cells, (unsigned long long)counting.count * sizeof(unsigned long long),
                                     "a folder's runs");
    runs->path = (runs->number != NULL)
                     ? cli_cell_taken_by(&g_query_cells, (unsigned long long)counting.count * QUERY_PATH_LONGEST,
                                         "the paths of a folder's runs")
                     : NULL;
    if (runs->path == NULL)
    {
        query_runs_given(runs);
        return 0;
    }
    runs->count = query_runs_each(folder, runs->number, runs->path, counting.count);
    return 1;
}

void query_runs_given(QueryRuns *runs)
{
    cli_cell_given_by(&g_query_cells, runs->number);
    cli_cell_given_by(&g_query_cells, runs->path);
    *runs = (QueryRuns){NULL, NULL, 0u};
}

// the civil date of a count of days from 1970-01-01
static void query_civil(unsigned long long days, unsigned long long *year, unsigned long long *month,
                        unsigned long long *day)
{
    const unsigned long long z = days + 719468ull;
    const unsigned long long era = z / 146097ull;
    const unsigned long long doe = z - (era * 146097ull);
    const unsigned long long yoe = (doe - (doe / 1460ull) + (doe / 36524ull) - (doe / 146096ull)) / 365ull;
    const unsigned long long doy = doe - ((365ull * yoe) + (yoe / 4ull) - (yoe / 100ull));
    const unsigned long long mp = ((5ull * doy) + 2ull) / 153ull;
    *day = doy - (((153ull * mp) + 2ull) / 5ull) + 1ull;
    *month = (mp < 10ull) ? (mp + 3ull) : (mp - 9ull);
    *year = yoe + (era * 400ull) + ((*month <= 2ull) ? 1ull : 0ull);
}

void query_next_name(const char *folder, char *name, size_t room)
{
    // the highest number of the folder's runs, found by a walk that counts
    QueryRunsWalk counting = {folder, NULL, NULL, 0u, 0u, 0ull};
    query_folder_each(folder, query_run_walked, &counting);
    const unsigned long long highest = counting.highest;
    const unsigned long long seconds = query_now();
    unsigned long long year = 0ull;
    unsigned long long month = 0ull;
    unsigned long long day = 0ull;
    query_civil(seconds / 86400ull, &year, &month, &day);
    const unsigned long long time = seconds % 86400ull;
    CliLine line = {name, room, 0ull};
    cli_line_text(&line, "query");
    cli_line_decimal(&line, highest + 1ull, 0u);
    cli_line_character(&line, '-');
    cli_line_decimal(&line, year, 4u);
    cli_line_decimal(&line, month, 2u);
    cli_line_decimal(&line, day, 2u);
    cli_line_character(&line, 'T');
    cli_line_decimal(&line, time / 3600ull, 2u);
    cli_line_decimal(&line, (time % 3600ull) / 60ull, 2u);
    cli_line_decimal(&line, time % 60ull, 2u);
    cli_line_text(&line, ".qry");
    cli_line_closed(&line);
}

void query_named(const char *top, const char *path, char *name, size_t room)
{
    char full_top[QUERY_PATH_LONGEST];
    char full_path[QUERY_PATH_LONGEST];
    const int both =
        query_path_full(top, full_top, sizeof(full_top)) && query_path_full(path, full_path, sizeof(full_path));
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = full_top, .cap = CLI_CELLS_POOL});
    if (both && cli_text_same(full_path, full_top, length) &&
        ((full_path[length] == '/') || (full_path[length] == '\\')))
    {
        CliLine line = {name, room, 0ull};
        for (const char *at = full_path + length + 1u; *at != '\0'; at += 1)
        {
            cli_line_character(&line, (*at == '\\') ? '/' : *at);
        }
        cli_line_closed(&line);
        return;
    }
    const char *base = path;
    for (const char *at = path; *at != '\0'; at += 1)
    {
        base = ((*at == '/') || (*at == '\\')) ? (at + 1) : base;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = name, .src = base, .bytes = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) + 1u});
}

int query_opened(const char *held, const char *crystal, char *named, size_t room)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL});
    if ((length > 4u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = held + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})))
    {
        memor.cpy(&(MemoriaCfg){.dst = named,
                                .src = held,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}) + 1u});
        return named[0] != '\0';
    }
    if (crystal == NULL)
    {
        return 0;
    }
    // the crystal read back into `restored/` beside it, as `<name>.qry`
    const char *base = held;
    for (const char *at = held; *at != '\0'; at += 1)
    {
        base = ((*at == '/') || (*at == '\\')) ? (at + 1) : base;
    }
    char restored[QUERY_PATH_LONGEST];
    CliLine restored_line = {restored, sizeof(restored), 0ull};
    cli_line_counted(&restored_line, held, (unsigned long long)(base - held));
    cli_line_text(&restored_line, "restored");
    CliLine named_line = {named, room, 0ull};
    cli_line_text(&named_line, restored);
    cli_line_character(&named_line, '/');
    cli_line_text(&named_line, base);
    cli_line_text(&named_line, ".qry");
    if ((cli_line_closed(&restored_line) == 0ull) || (cli_line_closed(&named_line) == 0ull) ||
        !query_folder_made(restored))
    {
        return 0;
    }
    if (query_is_file(named))
    {
        return 1;
    }
    const char *const words[5] = {crystal, "read", held, named, NULL};
    QueryProcess read;
    return query_process_start(&read, words, NULL, QUERY_OUTPUT_NONE) && (query_process_wait(&read) == 0ll);
}

int query_crystal(const char *root, char *program, size_t room)
{
    char base[64];
    query_binary_name("qry_crystal", base, sizeof(base));
    CliLine line = {program, room, 0ull};
    cli_line_text(&line, root);
    cli_line_text(&line, "/build/qry_crystal/engine/");
    cli_line_text(&line, base);
    return (cli_line_closed(&line) != 0ull) && query_is_file(program);
}

unsigned int query_kept_most(void)
{
    char kept[32];
    if (!query_environment_read("QRY_KEPT", kept, sizeof(kept)))
    {
        return QUERY_KEPT_MOST;
    }
    char *end = NULL;
    const unsigned long most = strtoul(kept, &end, 10);
    // a count kept is far below what an unsigned int holds
    return ((end != kept) && (*end == '\0')) ? (unsigned int)most : QUERY_KEPT_MOST;
}

void query_kept(const char *folder, unsigned int most)
{
    char restored[QUERY_PATH_LONGEST];
    CliLine line = {restored, sizeof(restored), 0ull};
    cli_line_text(&line, folder);
    cli_line_text(&line, "/restored");
    if (cli_line_closed(&line) != 0ull)
    {
        query_removed(restored);
    }
    QueryRuns runs;
    (void)query_runs_taken(folder, &runs);
    for (unsigned int at = most; at < runs.count; at += 1u)
    {
        query_removed(runs.path[at]);
    }
    query_runs_given(&runs);
}

int query_latest_run(const char *folder, const char *name, const char *own, const QueryWriter *writer,
                     const char *crystal, char *found, size_t room)
{
    char own_full[QUERY_PATH_LONGEST] = "";
    if (own != NULL)
    {
        query_path_full(own, own_full, sizeof(own_full));
    }
    QueryRuns runs;
    (void)query_runs_taken(folder, &runs);
    int latest = 0;
    for (unsigned int at = 0u; !latest && (at < runs.count); at += 1u)
    {
        char named[QUERY_PATH_LONGEST];
        char named_full[QUERY_PATH_LONGEST];
        if (!query_opened(runs.path[at], crystal, named, sizeof(named)) ||
            ((own != NULL) && query_path_full(named, named_full, sizeof(named_full)) &&
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
        if (query_writer_latest(&s_other, name, &bytes, &size))
        {
            memor.cpy(&(MemoriaCfg){.dst = found,
                                    .src = named,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
            latest = (found[0] != '\0') ? 1 : -1;
        }
    }
    query_runs_given(&runs);
    return latest == 1;
}
