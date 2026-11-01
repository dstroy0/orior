// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_listed.c: the runs of a tree, from its folders of runs: those one walk of the tree found, and each folder the
// tree's watch since saw a run written in, the tree walked again only where the watch lost changes; each folder read
// again on every listing so a run that appears in it shows at once; and the lessons of each folder's `overseer.txt`
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "qry.h"

// the most trees whose walks are cached, the most folders of runs one contains, the most folders a walk has yet to
// read, the most runs listed, and the places a list is first taken with
#define CLI_WALKS_MOST 16u

#define CLI_RUN_FOLDERS 256u

#define CLI_WALK_PENDING 4096u

#define CLI_LISTED_MOST 4096u

// A tree's walk: its root, when it was walked on the clock that only goes forward, whether the tree's watch lost
// changes since, and its folders of runs in order, in a cell of `room` at their most
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    int used;
    unsigned long long walked;
    int lost;
    char (*folders)[CLI_PATH_MAX_LEN];
    unsigned int count;
    unsigned int room;
} CliWalk;

// A run as the window lists it
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size;
    unsigned long long changed;
    int live;
    unsigned long long panicked;
    unsigned int folder;
} CliListed;

// the walks cached, under the lock
static CliWalk s_walks[CLI_WALKS_MOST];
static CliLock s_lock = CLI_LOCK_INIT;

// The folders a walk has yet to read, in a cell of `room`
typedef struct
{
    char (*folder)[CLI_PATH_MAX_LEN];
    unsigned int count;
    unsigned int room;
} CliPending;

// The runs listed, in a cell of `room`
typedef struct
{
    CliListed *run;
    unsigned int count;
    unsigned int room;
} CliRuns;

// What one folder read of a walk hands on: the walk, the folder read, and the folders that wait
typedef struct
{
    CliWalk *walk;
    const char *folder;
    CliPending *pending;
} CliWalking;

// `folder`, a path from the top of the tree, put in its place among the walk's folders of runs where it is not one
// already; under the lock
static void cli_walk_folder_put(CliWalk *walk, const char *folder)
{
    for (unsigned int at = 0u; at < walk->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = walk->folders[at], .other = folder, .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    if (walk->count == walk->room)
    {
        return;
    }
    unsigned int place = walk->count;
    while ((place > 0u) && (cli_text_order(walk->folders[place - 1u], folder) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = walk->folders[place],
            .src = walk->folders[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = walk->folders[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = walk->folders[place],
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
    walk->count += 1u;
}

// each entry of a folder walked: a folder to read after, other than version control's and built crates', or a run
// whose folder is put among the folders of runs
static void cli_walk_each(void *context, const char *name, int is_folder)
{
    CliWalking *const walking = (CliWalking *)context;
    CliPending *const pending = walking->pending;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder)
    {
        if ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "node_modules", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "target", .cap = CLI_CELLS_POOL})))
        {
            return;
        }
        if (pending->count == pending->room)
        {
            return;
        }
        VerbaLinea line = {pending->folder[pending->count], CLI_PATH_MAX_LEN, 0ull};
        mmgr_line_put(&line, walking->folder);
        mmgr_line_put(&line, (walking->folder[0] != '\0') ? "/" : "");
        mmgr_line_put(&line, name);
        pending->count += mmgr_line_ok(&line) ? 1u : 0u;
        mmgr_line_finish(&line);
        return;
    }
    if ((length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})))
    {
        return;
    }
    cli_walk_folder_put(walking->walk, walking->folder);
}

// the tree at `root` walked for its folders of runs, each a path from the top of the tree, the top itself empty
static void cli_walked(const char *root, CliWalk *walk)
{
    walk->count = 0u;
    if (walk->folders == NULL)
    {
        walk->folders = g_cli_cells->persistent_buf_alloc((unsigned long long)CLI_RUN_FOLDERS * CLI_PATH_MAX_LEN,
                                                          &g_cli_qry_listed_cells);
        walk->room = (walk->folders != NULL) ? CLI_RUN_FOLDERS : 0u;
    }
    CliPending pending = {NULL, 0u, 0u};
    pending.folder = g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_WALK_PENDING * CLI_PATH_MAX_LEN,
                                                      &g_cli_qry_listed_cells);
    if (pending.folder != NULL)
    {
        pending.room = CLI_WALK_PENDING;
        pending.count = 1u;
        pending.folder[0][0] = '\0';
    }
    while (pending.count > 0u)
    {
        pending.count -= 1u;
        char folder[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){
            .dst = folder,
            .src = pending.folder[pending.count],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = pending.folder[pending.count], .cap = CLI_CELLS_POOL}) + 1u});
        char full[CLI_PATH_MAX_LEN];
        VerbaLinea line = {full, sizeof(full), 0ull};
        mmgr_line_put(&line, root);
        if (folder[0] != '\0')
        {
            mmgr_line_ch(&line, '/');
            mmgr_line_put(&line, folder);
        }
        if (mmgr_line_finish(&line) == 0ull)
        {
            continue;
        }
        CliWalking walking = {walk, folder, &pending};
        query_folder_each(full, cli_walk_each, &walking);
    }
    g_cli_cells->persistent_buf_release(pending.folder, &g_cli_qry_listed_cells);
}

// the walk of the tree at `root`, or CLI_WALKS_MOST where none is cached; under the lock
static unsigned int cli_walk_found(const char *root)
{
    for (unsigned int at = 0u; at < CLI_WALKS_MOST; at += 1u)
    {
        if (s_walks[at].used &&
            cellul.eq(&(CatenaFinitaCfg){.src = s_walks[at].root, .other = root, .cap = CLI_CELLS_POOL}))
        {
            return at;
        }
    }
    return CLI_WALKS_MOST;
}

// the walk of the tree at `root`, walked where none is cached or its watch lost changes, in place of the walk used
// longest ago where every place holds one; under the lock
static CliWalk *cli_walk_of(const char *root)
{
    const unsigned int found = cli_walk_found(root);
    unsigned int oldest = 0u;
    for (unsigned int at = 0u; at < CLI_WALKS_MOST; at += 1u)
    {
        oldest = (!s_walks[at].used || (s_walks[at].walked < s_walks[oldest].walked)) ? at : oldest;
    }
    CliWalk *const walk = &s_walks[(found != CLI_WALKS_MOST) ? found : oldest];
    if ((found == CLI_WALKS_MOST) || walk->lost)
    {
        memor.cpy(&(MemoriaCfg){.dst = walk->root,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        walk->used = 1;
        walk->lost = 0;
        walk->walked = cli_clock_milliseconds();
        cli_walked(root, walk);
    }
    return walk;
}

void cli_qry_run_written(const char *root, const char *path, unsigned long long length)
{
    if ((length <= 4u) || (length >= CLI_PATH_MAX_LEN) ||
        (memor.cmp(&(MemoriaCfg){.src = path + length - 4u, .other = ".qry", .bytes = 4u}) != 0))
    {
        return;
    }
    // the folder is the path before its last slash, the top of the tree where it has none
    unsigned long long cut = length;
    cut = cli_text_back_any(path, cut, "/");
    char folder[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = folder, .src = path, .bytes = (size_t)((cut > 0u) ? cut - 1u : 0u)});
    folder[(cut > 0u) ? cut - 1u : 0u] = '\0';
    cli_lock_take(&s_lock);
    const unsigned int found = cli_walk_found(root);
    if (found != CLI_WALKS_MOST)
    {
        cli_walk_folder_put(&s_walks[found], folder);
    }
    cli_lock_give(&s_lock);
}

void cli_qry_runs_lost(const char *root)
{
    cli_lock_take(&s_lock);
    const unsigned int found = cli_walk_found(root);
    if (found != CLI_WALKS_MOST)
    {
        s_walks[found].lost = 1;
    }
    cli_lock_give(&s_lock);
}

// What one folder of runs read hands on: its full path, its place among the folders, and the runs listed
typedef struct
{
    const char *full;
    const char *folder;
    unsigned int index;
    CliRuns *runs;
} CliFolderRead;

static void cli_run_each(void *context, const char *name, int is_folder)
{
    CliFolderRead *const read = (CliFolderRead *)context;
    CliRuns *const runs = read->runs;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})))
    {
        return;
    }
    if (runs->count == runs->room)
    {
        return;
    }
    CliListed *const listed = &runs->run[runs->count];
    char full[CLI_PATH_MAX_LEN];
    VerbaLinea path_line = {full, sizeof(full), 0ull};
    mmgr_line_put(&path_line, read->full);
    mmgr_line_ch(&path_line, '/');
    mmgr_line_put(&path_line, name);
    VerbaLinea listed_line = {listed->path, sizeof(listed->path), 0ull};
    mmgr_line_put(&listed_line, read->folder);
    mmgr_line_put(&listed_line, (read->folder[0] != '\0') ? "/" : "");
    mmgr_line_put(&listed_line, name);
    if ((mmgr_line_finish(&path_line) == 0ull) || (mmgr_line_finish(&listed_line) == 0ull))
    {
        return;
    }
    if (!cli_file_stamp(full, &listed->size, &listed->changed))
    {
        listed->size = 0ull;
        listed->changed = 0ull;
    }
    listed->live = cli_qry_standing(full, &listed->panicked);
    listed->folder = read->index;
    runs->count += 1u;
}

// whether a run lists before another: the live first, then the newest, then by path
static int cli_before(const CliListed *one, const CliListed *other)
{
    if (one->live != other->live)
    {
        return one->live > other->live;
    }
    if (one->changed != other->changed)
    {
        return one->changed > other->changed;
    }
    return cli_text_order(one->path, other->path) < 0;
}

void cli_qry_lessons_written(const char *text, unsigned long long size, VerbaLinea *out)
{
    const char *at = text;
    const char *const end = text + size;
    int first = 1;
    while (at < end)
    {
        const char *stop = at;
        stop += cli_text_to_any(stop, 0ull, (unsigned long long)(end - stop), "\n");
        const char *words[4] = {NULL, NULL, NULL, NULL};
        unsigned long long lengths[4] = {0ull, 0ull, 0ull, 0ull};
        const char *rest = at;
        for (unsigned int each = 0u; each < 3u; each += 1u)
        {
            rest += cli_text_past_any(rest, 0ull, (unsigned long long)(stop - rest), CLI_TEXT_WHITE);
            words[each] = rest;
            rest += cli_text_to_any(rest, 0ull, (unsigned long long)(stop - rest), CLI_TEXT_WHITE);
            lengths[each] = (unsigned long long)(rest - words[each]);
        }
        // the stand-in: the words after, one space between each
        char stand_in[CLI_PATH_MAX_LEN];
        VerbaLinea joined = {stand_in, sizeof(stand_in), 0ull};
        while (rest < stop)
        {
            rest += cli_text_past_any(rest, 0ull, (unsigned long long)(stop - rest), CLI_TEXT_WHITE);
            const char *const word = rest;
            rest += cli_text_to_any(rest, 0ull, (unsigned long long)(stop - rest), CLI_TEXT_WHITE);
            if (rest > word)
            {
                mmgr_line_put(&joined, (joined.at != 0ull) ? " " : "");
                mmgr_line_put_n(&joined, word, (unsigned long long)(rest - word));
            }
        }
        const unsigned long long stand_length = mmgr_line_ok(&joined) ? joined.at : 0ull;
        if ((lengths[0] == 6u) && (memor.cmp(&(MemoriaCfg){.src = words[0], .other = "lesson", .bytes = 6u}) == 0) &&
            (lengths[1] != 0ull) && (lengths[2] != 0ull) && (stand_length != 0ull))
        {
            mmgr_line_put(out, first ? "{\"cause\":" : ",{\"cause\":");
            cli_json_quoted(out, words[1], lengths[1]);
            mmgr_line_put(out, ",\"carrier\":");
            cli_json_quoted(out, words[2], lengths[2]);
            mmgr_line_put(out, ",\"stand_in\":");
            cli_json_quoted(out, stand_in, stand_length);
            mmgr_line_ch(out, '}');
            first = 0;
        }
        at = (stop < end) ? (stop + 1) : stop;
    }
}

// the full path of a folder of runs under `root`
static int cli_folder_full(const char *root, const char *folder, char *full)
{
    VerbaLinea line = {full, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, root);
    if (folder[0] != '\0')
    {
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, folder);
    }
    return mmgr_line_finish(&line) != 0ull;
}

void cli_qry_listed(const char *root, VerbaLinea *out)
{
    cli_lock_take(&s_lock);
    const CliWalk *const walk = cli_walk_of(root);
    CliListed *const listed_cell = g_cli_cells->temporary_buf_alloc(
        (unsigned long long)CLI_LISTED_MOST * sizeof(CliListed), &g_cli_qry_listed_cells);
    CliRuns runs = {listed_cell, 0u, (listed_cell != NULL) ? CLI_LISTED_MOST : 0u};
    for (unsigned int index = 0u; index < walk->count; index += 1u)
    {
        char full[CLI_PATH_MAX_LEN];
        if (!cli_folder_full(root, walk->folders[index], full))
        {
            continue;
        }
        CliFolderRead read = {full, walk->folders[index], index, &runs};
        query_folder_each(full, cli_run_each, &read);
    }
    const unsigned int count = runs.count;
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        const CliListed pivot = runs.run[at];
        unsigned int place = at;
        while ((place > 0u) && cli_before(&pivot, &runs.run[place - 1u]))
        {
            runs.run[place] = runs.run[place - 1u];
            place -= 1u;
        }
        runs.run[place] = pivot;
    }
    mmgr_line_put(out, "{\"runs\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliListed *const listed = &runs.run[at];
        const char *const folder = walk->folders[listed->folder];
        mmgr_line_put(out, (at != 0u) ? ",{\"path\":" : "{\"path\":");
        cli_json_quoted(out, listed->path, cellul.len(&(CatenaFinitaCfg){.src = listed->path, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"folder\":");
        cli_json_quoted(out, folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"size\":");
        mmgr_line_decimal(out, listed->size, 1u);
        mmgr_line_put(out, ",\"changed\":");
        mmgr_line_decimal(out, listed->changed, 1u);
        mmgr_line_put(out, listed->live ? ",\"live\":true,\"panicked\":" : ",\"live\":false,\"panicked\":");
        mmgr_line_decimal(out, listed->panicked, 1u);
        mmgr_line_ch(out, '}');
    }
    g_cli_cells->persistent_buf_release(runs.run, &g_cli_qry_listed_cells);
    mmgr_line_put(out, "],\"folders\":[");
    for (unsigned int index = 0u; index < walk->count; index += 1u)
    {
        char full[CLI_PATH_MAX_LEN];
        char overseer[CLI_PATH_MAX_LEN];
        mmgr_line_put(out, (index != 0u) ? ",{\"folder\":" : "{\"folder\":");
        cli_json_quoted(out, walk->folders[index],
                        cellul.len(&(CatenaFinitaCfg){.src = walk->folders[index], .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"lessons\":[");
        VerbaLinea line = {overseer, sizeof(overseer), 0ull};
        mmgr_line_put(&line, cli_folder_full(root, walk->folders[index], full) ? full : "");
        mmgr_line_put(&line, "/overseer.txt");
        unsigned long long size = 0ull;
        const unsigned char *const bytes =
            (mmgr_line_finish(&line) != 0ull) ? cli_file_taken(overseer, &size, &g_cli_qry_listed_cells) : NULL;
        if (bytes != NULL)
        {
            // the overseer's bytes are its text
            cli_qry_lessons_written((const char *)bytes, size, out);
            cli_file_given(bytes, &g_cli_qry_listed_cells);
        }
        mmgr_line_put(out, "]}");
    }
    mmgr_line_put(out, "]}");
    cli_lock_give(&s_lock);
}
