// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_cycles.c: each import that leads back round to the file that makes it: every file's imports and the
// files each runs, and for each, the shortest way from what it runs back to its own file, named in the finding with
// each file it passes through. An import is reported once, by the first file it runs that leads back. The imports are
// counted by one walk of the files and followed by a second, every list of the walk one cell sized to the tree's files,
// imports and edges; a finding's message a cell sized to the paths of its way
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "inspect_tree_internal.h"

// The walk: each file's first edge and its count; each edge's import and the file it runs, counted where
// `edge_import` is NULL; the file each was come to from, the round each was last seen in, the waiting files and the
// way back; each import's round, by which an import is reported once; and the import an edge is being gathered for
typedef struct
{
    unsigned int *edge_first;
    unsigned int *edges;
    unsigned int *edge_import;
    unsigned int *edge_target;
    unsigned int edges_used;
    unsigned int *came;
    unsigned int *seen;
    unsigned int *waiting;
    unsigned int *way;
    unsigned int round;
    unsigned int *visited;
    unsigned int visit_round;
    unsigned int import;
} CliTreeCycles;

static CliTreeCycles s_walk;

// an edge put for the file being gathered, or counted
static void cycles_edge(const CliEachTargetCfg *args)
{
    const unsigned int *const file = args->context;
    if (s_walk.edge_import != NULL)
    {
        s_walk.edge_import[s_walk.edges_used] = s_walk.import;
        s_walk.edge_target[s_walk.edges_used] = args->target;
        s_walk.edges[*file] += 1u;
    }
    s_walk.edges_used += 1u;
}

// each file's imports' edges gathered, or counted where the walk has no lists
static void cycles_gather(void)
{
    s_walk.edges_used = 0u;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        if (s_walk.edge_import != NULL)
        {
            s_walk.edge_first[file] = s_walk.edges_used;
            s_walk.edges[file] = 0u;
        }
        const CliTreeFile *const tree_file = &CLI_TREE_FILE(file);
        for (unsigned int import = 0u; tree_file->used && (import < tree_file->facts.imports); import += 1u)
        {
            s_walk.import = import;
            EMBED_CALL(cli_inspect_tree_file_internal.tree_targets, CliInspectTreeFileCtx, .file = file,
                       .import = import, .each_callback = cycles_edge, .context = &file);
        }
    }
}

typedef struct
{
    unsigned int from;
    unsigned int to;
    VerbaLinea *message;
    unsigned int number;
    unsigned int file;
    unsigned int import;
    unsigned int count;
    CliTreeFindingFn each;
    void *context;
} InspectTreeCyclesCtx;
// the shortest way from `from` back to `to` over the edges, `from` first and `to` last, into the way: its count, 0
// where none leads back
EMBED_INLINE unsigned int cycles_path_back(const InspectTreeCyclesCtx *args)
{
    s_walk.round += 1u;
    unsigned int head = 0u;
    unsigned int tail = 1u;
    s_walk.waiting[0] = args->from;
    s_walk.seen[args->from] = s_walk.round;
    s_walk.came[args->from] = CLI_TREE_NONE;
    while (head < tail)
    {
        const unsigned int one = s_walk.waiting[head];
        head += 1u;
        if (one == args->to)
        {
            unsigned int count = 0u;
            for (unsigned int at = one; at != CLI_TREE_NONE; at = s_walk.came[at])
            {
                s_walk.way[count] = at;
                count += 1u;
            }
            for (unsigned int low = 0u; low < count / 2u; low += 1u)
            {
                const unsigned int pivot = s_walk.way[low];
                s_walk.way[low] = s_walk.way[count - 1u - low];
                s_walk.way[count - 1u - low] = pivot;
            }
            return count;
        }
        for (unsigned int at = 0u; at < s_walk.edges[one]; at += 1u)
        {
            const unsigned int next = s_walk.edge_target[s_walk.edge_first[one] + at];
            if (s_walk.seen[next] != s_walk.round)
            {
                s_walk.seen[next] = s_walk.round;
                s_walk.came[next] = one;
                s_walk.waiting[tail] = next;
                tail += 1u;
            }
        }
    }
    return 0u;
}

// a name of the way, numbered from 1, put on the message
EMBED_INLINE void cycles_by_name(const InspectTreeCyclesCtx *args)
{
    mmgr_line_put(args->message, (args->number > 1u) ? "\n" : "");
    mmgr_line_decimal(args->message, args->number, 1u);
    mmgr_line_put(args->message, ". `");
    mmgr_line_put_n(args->message, CLI_TREE_FILE(args->file).path, CLI_TREE_FILE(args->file).path_size);
    mmgr_line_ch(args->message, '`');
}

// the opening of a finding's message, before the way
#define CLI_CYCLES_OPENING "This import leads back round to the file that makes it, by way of:\n\n"

// the finding that the import at `import` of the file at `file` leads back round by the way of `count` files, in a
// message sized to their paths, handed to `each`
EMBED_INLINE void cycles_report(const InspectTreeCyclesCtx *args)
{
    // each name of the way its path, its number, a newline, a dot, a space and two quotes
    unsigned long long size = sizeof(CLI_CYCLES_OPENING) + CLI_TREE_FILE(args->file).path_size + 32ull;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        size += CLI_TREE_FILE(s_walk.way[at]).path_size + 32ull;
    }
    char *const text = g_cli_cells->temporary_buf_alloc(size, &g_cli_inspect_cells);
    if (text == NULL)
    {
        return;
    }
    VerbaLinea message = {text, size, 0ull};
    mmgr_line_put(&message, CLI_CYCLES_OPENING);
    EMBED_CALL(cycles_by_name, InspectTreeCyclesCtx, .message = &message, .number = 1u, .file = args->file);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        EMBED_CALL(cycles_by_name, InspectTreeCyclesCtx, .message = &message, .number = at + 2u,
                   .file = s_walk.way[at]);
    }
    const CliFacts *const facts = &CLI_TREE_FILE(args->file).facts;
    if (mmgr_line_ok(&message))
    {
        EMBED_CALL(args->each, CliTreeFindingCfg, .context = args->context, .file = args->file,
                   .from = facts->import[args->import].from, .to = facts->import[args->import].to,
                   .code = "import-cycle", .message = &message);
    }
    g_cli_cells->persistent_buf_release(text, &g_cli_inspect_cells);
}

// the imports of the file at `file` that lead back round to it, each handed to `each`
EMBED_INLINE void cycles_get(const InspectTreeCyclesCtx *args)
{
    s_walk.visit_round += 1u;
    for (unsigned int edge = 0u; edge < s_walk.edges[args->file]; edge += 1u)
    {
        const unsigned int import = s_walk.edge_import[s_walk.edge_first[args->file] + edge];
        if (s_walk.visited[import] == s_walk.visit_round)
        {
            continue;
        }
        const unsigned int count =
            EMBED_CALL(cycles_path_back, InspectTreeCyclesCtx,
                       .from = s_walk.edge_target[s_walk.edge_first[args->file] + edge], .to = args->file);
        if (count == 0u)
        {
            continue;
        }
        s_walk.visited[import] = s_walk.visit_round;
        EMBED_CALL(cycles_report, InspectTreeCyclesCtx, .file = args->file, .import = import, .count = count,
                   .each = args->each, .context = args->context);
    }
}

void cli_tree_cycles(const CliInspectTreeCtx *args)
{
    // the edges counted, and the most imports one file has
    memor.set(&(MemoriaCfg){.dst = &s_walk, .val = 0, .bytes = sizeof(s_walk)});
    cycles_gather();
    const unsigned long long edges = s_walk.edges_used;
    unsigned long long imports = 1ull;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        const CliTreeFile *const tree_file = &CLI_TREE_FILE(file);
        imports = (tree_file->used && (tree_file->facts.imports > imports)) ? tree_file->facts.imports : imports;
    }
    const unsigned long long files = g_cli_tree_files + 1ull;
    unsigned int *const cell = g_cli_cells->persistent_buf_alloc(
        ((6ull * files) + (2ull * edges) + imports) * sizeof(unsigned int), &g_cli_inspect_cells);
    if (cell == NULL)
    {
        return;
    }
    memor.set(
        &(MemoriaCfg){.dst = cell, .val = 0, .bytes = (size_t)(((6ull * files) + imports) * sizeof(unsigned int))});
    s_walk.edge_first = cell;
    s_walk.edges = s_walk.edge_first + files;
    s_walk.came = s_walk.edges + files;
    s_walk.seen = s_walk.came + files;
    s_walk.waiting = s_walk.seen + files;
    s_walk.way = s_walk.waiting + files;
    s_walk.visited = s_walk.way + files;
    s_walk.edge_import = s_walk.visited + imports;
    s_walk.edge_target = s_walk.edge_import + edges;
    cycles_gather();
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        if (CLI_TREE_FILE(file).used)
        {
            EMBED_CALL(cycles_get, InspectTreeCyclesCtx, .file = file, .each = args->each, .context = args->context);
        }
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_inspect_cells);
    memor.set(&(MemoriaCfg){.dst = &s_walk, .val = 0, .bytes = sizeof(s_walk)});
}
