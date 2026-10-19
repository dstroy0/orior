// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_listed.c: the runs of a tree, from its folders of runs: those a walk of the tree found, walked again at most
// every ten seconds, each folder read again on every listing so a run that appears in it shows at once; and the lessons
// of each folder's `overseer.txt`
#include "../text/json_tree.h"
#include "../text/text.h"
#include "qry.h"

// how often the tree is walked for folders of runs, in milliseconds; the most trees whose walks are kept, the most
// folders of runs one holds, the most folders a walk has yet to read, and the most runs listed
#define CLI_WALK_EVERY 10000ull

#define CLI_WALKS_MOST 16u

#define CLI_RUN_FOLDERS 256u

#define CLI_WALK_PENDING 4096u

#define CLI_LISTED_MOST 4096u

#define CLI_OVERSEER_BYTES (1u << 20u)

// A tree's walk: its root, when it was walked on the clock that only goes forward, and its folders of runs in order
typedef struct
{
    char root[CLI_PATH_LONGEST];
    int used;
    unsigned long long walked;
    char folders[CLI_RUN_FOLDERS][CLI_PATH_LONGEST];
    unsigned int count;
} CliWalk;

// A run as the window lists it
typedef struct
{
    char path[CLI_PATH_LONGEST];
    unsigned long long size;
    unsigned long long changed;
    int live;
    unsigned long long panicked;
    unsigned int folder;
} CliListed;

// the walks kept, the folders a walk has yet to read, the runs listed and an overseer's text, under the lock
static CliWalk s_walks[CLI_WALKS_MOST];
static char s_pending[CLI_WALK_PENDING][CLI_PATH_LONGEST];
static CliListed s_listed[CLI_LISTED_MOST];
static char s_overseer[CLI_OVERSEER_BYTES];
static CliLock s_lock = CLI_LOCK_INIT;

// What one folder read of a walk hands on: the walk, the folder read, and how many folders wait
typedef struct
{
    CliWalk *walk;
    const char *folder;
    unsigned int *pending;
} CliWalking;

// each entry of a folder walked: a folder to read after, other than version control's and built crates', or a run
// whose folder is put among the folders of runs
static void cli_walk_each(void *context, const char *name, int is_folder)
{
    CliWalking *const walking = (CliWalking *)context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder)
    {
        if ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "node_modules", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "target", .cap = CLI_CELLS_POOL})) ||
            (*walking->pending >= CLI_WALK_PENDING))
        {
            return;
        }
        CliLine line = {s_pending[*walking->pending], CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, walking->folder);
        cli_line_text(&line, (walking->folder[0] != '\0') ? "/" : "");
        cli_line_text(&line, name);
        *walking->pending += cli_line_fits(&line) ? 1u : 0u;
        cli_line_closed(&line);
        return;
    }
    if ((length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})))
    {
        return;
    }
    CliWalk *const walk = walking->walk;
    for (unsigned int at = 0u; at < walk->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = walk->folders[at], .other = walking->folder, .cap = CLI_CELLS_POOL}))
        {
            return;
        }
    }
    if (walk->count < CLI_RUN_FOLDERS)
    {
        // put in its place among the folders, in order
        unsigned int place = walk->count;
        while ((place > 0u) && (cli_text_order(walk->folders[place - 1u], walking->folder) > 0))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = walk->folders[place],
                .src = walk->folders[place - 1u],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = walk->folders[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
            place -= 1u;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = walk->folders[place],
                          .src = walking->folder,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = walking->folder, .cap = CLI_CELLS_POOL}) + 1u});
        walk->count += 1u;
    }
}

// the tree at `root` walked for its folders of runs, each a path from the top of the tree, the top itself empty
static void cli_walked(const char *root, CliWalk *walk)
{
    walk->count = 0u;
    unsigned int pending = 1u;
    s_pending[0][0] = '\0';
    while (pending > 0u)
    {
        pending -= 1u;
        char folder[CLI_PATH_LONGEST];
        memor.cpy(&(MemoriaCfg){
            .dst = folder,
            .src = s_pending[pending],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_pending[pending], .cap = CLI_CELLS_POOL}) + 1u});
        char full[CLI_PATH_LONGEST];
        CliLine line = {full, sizeof(full), 0ull};
        cli_line_text(&line, root);
        if (folder[0] != '\0')
        {
            cli_line_character(&line, '/');
            cli_line_text(&line, folder);
        }
        if (cli_line_closed(&line) == 0ull)
        {
            continue;
        }
        CliWalking walking = {walk, folder, &pending};
        query_folder_each(full, cli_walk_each, &walking);
    }
}

// the walk of the tree at `root`, walked again where it is older than CLI_WALK_EVERY
static CliWalk *cli_walk_of(const char *root)
{
    const unsigned long long now = cli_clock_milliseconds();
    unsigned int found = CLI_WALKS_MOST;
    unsigned int oldest = 0u;
    for (unsigned int at = 0u; at < CLI_WALKS_MOST; at += 1u)
    {
        found = (s_walks[at].used &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_walks[at].root, .other = root, .cap = CLI_CELLS_POOL})))
                    ? at
                    : found;
        oldest = (!s_walks[at].used || (s_walks[at].walked < s_walks[oldest].walked)) ? at : oldest;
    }
    CliWalk *const walk = &s_walks[(found != CLI_WALKS_MOST) ? found : oldest];
    if ((found == CLI_WALKS_MOST) || ((now - walk->walked) >= CLI_WALK_EVERY))
    {
        memor.cpy(&(MemoriaCfg){.dst = walk->root,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
        walk->used = 1;
        walk->walked = now;
        cli_walked(root, walk);
    }
    return walk;
}

// What one folder of runs read hands on: its full path, its place among the folders, and the count listed
typedef struct
{
    const char *full;
    const char *folder;
    unsigned int index;
    unsigned int *count;
} CliFolderRead;

static void cli_run_each(void *context, const char *name, int is_folder)
{
    CliFolderRead *const read = (CliFolderRead *)context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (is_folder || (length <= 4u) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = name + length - 4u, .other = ".qry", .cap = CLI_CELLS_POOL})) ||
        (*read->count >= CLI_LISTED_MOST))
    {
        return;
    }
    CliListed *const listed = &s_listed[*read->count];
    char full[CLI_PATH_LONGEST];
    CliLine path_line = {full, sizeof(full), 0ull};
    cli_line_text(&path_line, read->full);
    cli_line_character(&path_line, '/');
    cli_line_text(&path_line, name);
    CliLine listed_line = {listed->path, sizeof(listed->path), 0ull};
    cli_line_text(&listed_line, read->folder);
    cli_line_text(&listed_line, (read->folder[0] != '\0') ? "/" : "");
    cli_line_text(&listed_line, name);
    if ((cli_line_closed(&path_line) == 0ull) || (cli_line_closed(&listed_line) == 0ull))
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
    *read->count += 1u;
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

void cli_qry_lessons_written(const char *text, unsigned long long size, CliLine *out)
{
    const char *at = text;
    const char *const end = text + size;
    int first = 1;
    while (at < end)
    {
        const char *stop = at;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *words[4] = {NULL, NULL, NULL, NULL};
        unsigned long long lengths[4] = {0ull, 0ull, 0ull, 0ull};
        const char *rest = at;
        for (unsigned int each = 0u; each < 3u; each += 1u)
        {
            while ((rest < stop) && ((rest[0] == ' ') || ((rest[0] >= '\t') && (rest[0] <= '\r'))))
            {
                rest += 1;
            }
            words[each] = rest;
            while ((rest < stop) && !((rest[0] == ' ') || ((rest[0] >= '\t') && (rest[0] <= '\r'))))
            {
                rest += 1;
            }
            lengths[each] = (unsigned long long)(rest - words[each]);
        }
        // the stand-in: the words after, one space between each
        char stand_in[CLI_PATH_LONGEST];
        CliLine joined = {stand_in, sizeof(stand_in), 0ull};
        while (rest < stop)
        {
            while ((rest < stop) && ((rest[0] == ' ') || ((rest[0] >= '\t') && (rest[0] <= '\r'))))
            {
                rest += 1;
            }
            const char *const word = rest;
            while ((rest < stop) && !((rest[0] == ' ') || ((rest[0] >= '\t') && (rest[0] <= '\r'))))
            {
                rest += 1;
            }
            if (rest > word)
            {
                cli_line_text(&joined, (joined.at != 0ull) ? " " : "");
                cli_line_counted(&joined, word, (unsigned long long)(rest - word));
            }
        }
        const unsigned long long stand_length = cli_line_fits(&joined) ? joined.at : 0ull;
        if ((lengths[0] == 6u) && (memor.cmp(&(MemoriaCfg){.src = words[0], .other = "lesson", .bytes = 6u}) == 0) &&
            (lengths[1] != 0ull) && (lengths[2] != 0ull) && (stand_length != 0ull))
        {
            cli_line_text(out, first ? "{\"cause\":" : ",{\"cause\":");
            cli_json_quoted(out, words[1], lengths[1]);
            cli_line_text(out, ",\"carrier\":");
            cli_json_quoted(out, words[2], lengths[2]);
            cli_line_text(out, ",\"stand_in\":");
            cli_json_quoted(out, stand_in, stand_length);
            cli_line_character(out, '}');
            first = 0;
        }
        at = (stop < end) ? (stop + 1) : stop;
    }
}

// the full path of a folder of runs under `root`
static int cli_folder_full(const char *root, const char *folder, char *full)
{
    CliLine line = {full, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, root);
    if (folder[0] != '\0')
    {
        cli_line_character(&line, '/');
        cli_line_text(&line, folder);
    }
    return cli_line_closed(&line) != 0ull;
}

void cli_qry_listed(const char *root, CliLine *out)
{
    cli_lock_take(&s_lock);
    const CliWalk *const walk = cli_walk_of(root);
    unsigned int count = 0u;
    for (unsigned int index = 0u; index < walk->count; index += 1u)
    {
        char full[CLI_PATH_LONGEST];
        if (!cli_folder_full(root, walk->folders[index], full))
        {
            continue;
        }
        CliFolderRead read = {full, walk->folders[index], index, &count};
        query_folder_each(full, cli_run_each, &read);
    }
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        static CliListed s_held;
        s_held = s_listed[at];
        unsigned int place = at;
        while ((place > 0u) && cli_before(&s_held, &s_listed[place - 1u]))
        {
            s_listed[place] = s_listed[place - 1u];
            place -= 1u;
        }
        s_listed[place] = s_held;
    }
    cli_line_text(out, "{\"runs\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliListed *const listed = &s_listed[at];
        const char *const folder = walk->folders[listed->folder];
        cli_line_text(out, (at != 0u) ? ",{\"path\":" : "{\"path\":");
        cli_json_quoted(out, listed->path, cellul.len(&(CatenaFinitaCfg){.src = listed->path, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"folder\":");
        cli_json_quoted(out, folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"size\":");
        cli_line_decimal(out, listed->size, 1u);
        cli_line_text(out, ",\"changed\":");
        cli_line_decimal(out, listed->changed, 1u);
        cli_line_text(out, listed->live ? ",\"live\":true,\"panicked\":" : ",\"live\":false,\"panicked\":");
        cli_line_decimal(out, listed->panicked, 1u);
        cli_line_character(out, '}');
    }
    cli_line_text(out, "],\"folders\":[");
    for (unsigned int index = 0u; index < walk->count; index += 1u)
    {
        char full[CLI_PATH_LONGEST];
        char overseer[CLI_PATH_LONGEST];
        cli_line_text(out, (index != 0u) ? ",{\"folder\":" : "{\"folder\":");
        cli_json_quoted(out, walk->folders[index],
                        cellul.len(&(CatenaFinitaCfg){.src = walk->folders[index], .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"lessons\":[");
        CliLine line = {overseer, sizeof(overseer), 0ull};
        cli_line_text(&line, cli_folder_full(root, walk->folders[index], full) ? full : "");
        cli_line_text(&line, "/overseer.txt");
        const unsigned long long size =
            (cli_line_closed(&line) != 0ull)
                ? qry_member_file_read(overseer, (unsigned char *)s_overseer, sizeof(s_overseer))
                : QRY_MEMBER_UNREAD;
        if (size != QRY_MEMBER_UNREAD)
        {
            cli_qry_lessons_written(s_overseer, size, out);
        }
        cli_line_text(out, "]}");
    }
    cli_line_text(out, "]}");
    cli_lock_give(&s_lock);
}
