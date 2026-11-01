// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_folder.c: a folder's runs: each numbered newest first, the next one's name stamped by the clock, a path named
// under the top of the tree, a run read back out of its crystal, and only the newest QRY_RETAINED retained
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

unsigned int query_runs_each(const char *folder, unsigned long long *number, char (*path)[QUERY_PATH_MAX_LEN],
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
    runs->number = g_cli_cells->persistent_buf_alloc((unsigned long long)counting.count * sizeof(unsigned long long),
                                                     &g_query_cells);
    runs->path =
        (runs->number != NULL)
            ? g_cli_cells->persistent_buf_alloc((unsigned long long)counting.count * QUERY_PATH_MAX_LEN, &g_query_cells)
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
    g_cli_cells->persistent_buf_release(runs->number, &g_query_cells);
    g_cli_cells->persistent_buf_release(runs->path, &g_query_cells);
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
    VerbaLinea line = {name, room, 0ull};
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

void query_named(const char *top, const char *path, char *name, size_t room)
{
    char full_top[QUERY_PATH_MAX_LEN];
    char full_path[QUERY_PATH_MAX_LEN];
    const int both =
        query_path_full(top, full_top, sizeof(full_top)) && query_path_full(path, full_path, sizeof(full_path));
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = full_top, .cap = CLI_CELLS_POOL});
    if (both && cli_text_same(full_path, full_top, length) &&
        ((full_path[length] == '/') || (full_path[length] == '\\')))
    {
        VerbaLinea line = {name, room, 0ull};
        (void)cli_text_put_swapped(&line, full_path + length + 1u, '\\', '/');
        mmgr_line_finish(&line);
        return;
    }
    const char *base = path;
    const unsigned long long base_past =
        cli_text_back_any(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), "/\\");
    base = (base_past != 0ull) ? (path + base_past) : base;
    memor.cpy(&(MemoriaCfg){
        .dst = name, .src = base, .bytes = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}) + 1u});
}

int query_opened(const char *qry_path, const char *crystal, char *named, size_t room)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = qry_path, .cap = CLI_CELLS_POOL});
    if ((length > 4u) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = qry_path + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})))
    {
        memor.cpy(&(MemoriaCfg){.dst = named,
                                .src = qry_path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = qry_path, .cap = CLI_CELLS_POOL}) + 1u});
        return named[0] != '\0';
    }
    if (crystal == NULL)
    {
        return 0;
    }
    // the crystal read back into `restored/` beside it, as `<name>.qry`
    const char *base = qry_path;
    const unsigned long long base_past =
        cli_text_back_any(qry_path, cellul.len(&(CatenaFinitaCfg){.src = qry_path, .cap = CLI_CELLS_POOL}), "/\\");
    base = (base_past != 0ull) ? (qry_path + base_past) : base;
    char restored[QUERY_PATH_MAX_LEN];
    VerbaLinea restored_line = {restored, sizeof(restored), 0ull};
    mmgr_line_put_n(&restored_line, qry_path, (unsigned long long)(base - qry_path));
    mmgr_line_put(&restored_line, "restored");
    VerbaLinea named_line = {named, room, 0ull};
    mmgr_line_put(&named_line, restored);
    mmgr_line_ch(&named_line, '/');
    mmgr_line_put(&named_line, base);
    mmgr_line_put(&named_line, ".qry");
    if ((mmgr_line_finish(&restored_line) == 0ull) || (mmgr_line_finish(&named_line) == 0ull) ||
        !query_folder_made(restored))
    {
        return 0;
    }
    if (query_is_file(named))
    {
        return 1;
    }
    const char *const words[5] = {crystal, "read", qry_path, named, NULL};
    QueryProcess read;
    return query_process_start(&read, words, NULL, QUERY_OUTPUT_NONE) && (query_process_wait(&read) == 0ll);
}

int query_crystal(const char *root, char *program, size_t room)
{
    char base[64];
    query_binary_name("qry_crystal", base, sizeof(base));
    VerbaLinea line = {program, room, 0ull};
    mmgr_line_put(&line, root);
    mmgr_line_put(&line, "/build/qry_crystal/engine/");
    mmgr_line_put(&line, base);
    return (mmgr_line_finish(&line) != 0ull) && query_is_file(program);
}

unsigned int query_retain_max(void)
{
    char retain_text[32];
    if (!query_environment_read("QRY_RETAINED", retain_text, sizeof(retain_text)))
    {
        return QUERY_RETAIN_MAX;
    }
    char *end = NULL;
    const unsigned long most = strtoul(retain_text, &end, 10);
    // a count recorded is far below what an unsigned int represents
    return ((end != retain_text) && (*end == '\0')) ? (unsigned int)most : QUERY_RETAIN_MAX;
}

void query_runs_pruned(const char *folder, unsigned int most)
{
    char restored[QUERY_PATH_MAX_LEN];
    VerbaLinea line = {restored, sizeof(restored), 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_put(&line, "/restored");
    if (mmgr_line_finish(&line) != 0ull)
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
    char own_full[QUERY_PATH_MAX_LEN] = "";
    if (own != NULL)
    {
        query_path_full(own, own_full, sizeof(own_full));
    }
    QueryRuns runs;
    (void)query_runs_taken(folder, &runs);
    int latest = 0;
    for (unsigned int at = 0u; !latest && (at < runs.count); at += 1u)
    {
        char named[QUERY_PATH_MAX_LEN];
        char named_full[QUERY_PATH_MAX_LEN];
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
