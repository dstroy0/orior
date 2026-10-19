// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_tree_cycles.c: each import that leads back round to the file that makes it: every file's imports and the
// files each runs, and for each, the shortest way from what it runs back to its own file, named in the finding with
// each file it passes through. An import is reported once, by the first file it runs that leads back. The imports are
// counted by one walk of the files and followed by a second, every list of the walk one cell sized to the tree's files,
// imports and edges; a finding's message a cell sized to the paths of its way
#include "../command_line/cells.h"
#include "inspect_tree_internal.h"

// The walk: each file's first edge and its count; each edge's import and the file it runs, counted where
// `edge_import` is NULL; the file each was come to from, the round each was last seen in, the waiting files and the
// way back; each import's round, so an import is reported once; and the import an edge is being gathered for
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
    unsigned int *said;
    unsigned int said_round;
    unsigned int import;
} CliTreeCycles;

static CliTreeCycles s_walk;

// an edge put for the file being gathered, or counted
static void cycles_edge(void *context, unsigned int target)
{
    const unsigned int *const file = context;
    if (s_walk.edge_import != NULL)
    {
        s_walk.edge_import[s_walk.edges_used] = s_walk.import;
        s_walk.edge_target[s_walk.edges_used] = target;
        s_walk.edges[*file] += 1u;
    }
    s_walk.edges_used += 1u;
}

// each file's imports' edges gathered, or counted where the walk has no lists
static void cycles_gathered(void)
{
    s_walk.edges_used = 0u;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        if (s_walk.edge_import != NULL)
        {
            s_walk.edge_first[file] = s_walk.edges_used;
            s_walk.edges[file] = 0u;
        }
        const CliTreeFile *const held = &CLI_TREE_FILE(file);
        for (unsigned int import = 0u; held->used && (import < held->facts.imports); import += 1u)
        {
            s_walk.import = import;
            cli_tree_targets(file, import, cycles_edge, &file);
        }
    }
}

// the shortest way from `from` back to `to` over the edges, `from` first and `to` last, into the way: its count, 0
// where none leads back
static unsigned int cycles_path_back(unsigned int from, unsigned int to)
{
    s_walk.round += 1u;
    unsigned int head = 0u;
    unsigned int tail = 1u;
    s_walk.waiting[0] = from;
    s_walk.seen[from] = s_walk.round;
    s_walk.came[from] = CLI_TREE_NONE;
    while (head < tail)
    {
        const unsigned int one = s_walk.waiting[head];
        head += 1u;
        if (one == to)
        {
            unsigned int count = 0u;
            for (unsigned int at = one; at != CLI_TREE_NONE; at = s_walk.came[at])
            {
                s_walk.way[count] = at;
                count += 1u;
            }
            for (unsigned int low = 0u; low < count / 2u; low += 1u)
            {
                const unsigned int kept = s_walk.way[low];
                s_walk.way[low] = s_walk.way[count - 1u - low];
                s_walk.way[count - 1u - low] = kept;
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
static void cycles_named(CliLine *message, unsigned int number, unsigned int file)
{
    cli_line_text(message, (number > 1u) ? "\n" : "");
    cli_line_decimal(message, number, 1u);
    cli_line_text(message, ". `");
    cli_line_counted(message, CLI_TREE_FILE(file).path, CLI_TREE_FILE(file).path_size);
    cli_line_character(message, '`');
}

// the opening of a finding's message, before the way
#define CLI_CYCLES_OPENING "This import leads back round to the file that makes it, by way of:\n\n"

// the finding that the import at `import` of the file at `file` leads back round by the way of `count` files, in a
// message sized to their paths, handed to `each`
static void cycles_said(unsigned int file, unsigned int import, unsigned int count, CliTreeSaid each, void *context)
{
    // each name of the way its path, its number, a newline, a dot, a space and two quotes
    unsigned long long size = sizeof(CLI_CYCLES_OPENING) + CLI_TREE_FILE(file).path_size + 32ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        size += CLI_TREE_FILE(s_walk.way[at]).path_size + 32ull;
    }
    char *const text = cli_cell_taken(size, "an import cycle's message");
    if (text == NULL)
    {
        return;
    }
    CliLine message = {text, size, 0ull};
    cli_line_text(&message, CLI_CYCLES_OPENING);
    cycles_named(&message, 1u, file);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cycles_named(&message, at + 2u, s_walk.way[at]);
    }
    const CliFacts *const facts = &CLI_TREE_FILE(file).facts;
    if (cli_line_fits(&message))
    {
        each(context, file, facts->import[import].from, facts->import[import].to, "import-cycle", &message);
    }
    cli_cell_given(text);
}

// the imports of the file at `file` that lead back round to it, each handed to `each`
static void cycles_of(unsigned int file, CliTreeSaid each, void *context)
{
    s_walk.said_round += 1u;
    for (unsigned int edge = 0u; edge < s_walk.edges[file]; edge += 1u)
    {
        const unsigned int import = s_walk.edge_import[s_walk.edge_first[file] + edge];
        if (s_walk.said[import] == s_walk.said_round)
        {
            continue;
        }
        const unsigned int count = cycles_path_back(s_walk.edge_target[s_walk.edge_first[file] + edge], file);
        if (count == 0u)
        {
            continue;
        }
        s_walk.said[import] = s_walk.said_round;
        cycles_said(file, import, count, each, context);
    }
}

void cli_tree_cycles(CliTreeSaid each, void *context)
{
    // the edges counted, and the most imports one file has
    memor.set(&(MemoriaCfg){.dst = &s_walk, .val = 0, .bytes = sizeof(s_walk)});
    cycles_gathered();
    const unsigned long long edges = s_walk.edges_used;
    unsigned long long imports = 1ull;
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        const CliTreeFile *const held = &CLI_TREE_FILE(file);
        imports = (held->used && (held->facts.imports > imports)) ? held->facts.imports : imports;
    }
    const unsigned long long files = g_cli_tree_files + 1ull;
    unsigned int *const cell =
        cli_cell_taken(((6ull * files) + (2ull * edges) + imports) * sizeof(unsigned int), "the tree's imports walked");
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
    s_walk.said = s_walk.way + files;
    s_walk.edge_import = s_walk.said + imports;
    s_walk.edge_target = s_walk.edge_import + edges;
    cycles_gathered();
    for (unsigned int file = 0u; file < g_cli_tree_files; file += 1u)
    {
        if (CLI_TREE_FILE(file).used)
        {
            cycles_of(file, each, context);
        }
    }
    cli_cell_given(cell);
    memor.set(&(MemoriaCfg){.dst = &s_walk, .val = 0, .bytes = sizeof(s_walk)});
}
