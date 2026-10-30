// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_usages.c: the definition, the usages, whether a name can be renamed and the edits that rename it, for a file
// its server is given as the tree's slice, answered from the tree (symbols_usages.c), the usages in the order of their
// files, lines and columns, each file's text the editor's where it holds one. A name the tree knows no definition of
// has none: the server is never asked
#include "../code/symbols.h"
#include "../command_line/file_writing.h"
#include "servers_internal.h"

#include <stdlib.h>

// the bytes the usages, their paths and their lines' texts are each begun in, doubling as they fill
#define CLI_SERVERS_USAGES_FIRST 4096u

// One usage read: its path among the paths, its line, its span in UTF-16 units, and its line's text among the texts
// and how many bytes
typedef struct
{
    unsigned long long path;
    unsigned int line;
    unsigned int from;
    unsigned int to;
    unsigned long long text;
    unsigned long long length;
} CliServersUsageRead;

// The usages read: the tree's top, the usages, their paths and their lines' texts, each a growing line, and how many
typedef struct
{
    const char *tree;
    VerbaLinea usage;
    VerbaLinea paths;
    VerbaLinea texts;
    unsigned int count;
} CliServersUsagesRead;

// the usages read, sorted under their own lock
static const CliServersUsagesRead *s_sorting;
static CliLock s_sorting_lock = CLI_LOCK_INIT;

// the text the editor holds of the tree's file at `path`, in a cell of its size: NULL where it holds none
static char *usages_held(void *context, const char *path, unsigned long long *size)
{
    const CliServersUsagesRead *const read = (const CliServersUsagesRead *)context;
    char full[CLI_PATH_MAX_LEN];
    cli_full(read->tree, path, full);
    return cli_servers_text_taken(full, size);
}

// a usage the tree found put after those read
static void usages_put(void *context, const char *path, unsigned int line, unsigned int from, unsigned int to,
                       const char *text, unsigned long long length)
{
    CliServersUsagesRead *const read = (CliServersUsagesRead *)context;
    const CliServersUsageRead one = {read->paths.at, line, from, to, read->texts.at, length};
    mmgr_line_put(&read->paths, path);
    mmgr_line_ch(&read->paths, '\0');
    // a line's text is below 2^32 bytes
    mmgr_line_put_n(&read->texts, text, (size_t)length);
    mmgr_line_put_n(&read->usage, (const char *)&one, sizeof(one));
    read->count += 1u;
}

// the usage at `at` of those read
static CliServersUsageRead usages_at(const CliServersUsagesRead *read, unsigned int at)
{
    CliServersUsageRead one;
    memor.cpy(&(MemoriaCfg){.dst = &one, .src = read->usage.out + ((size_t)at * sizeof(one)), .bytes = sizeof(one)});
    return one;
}

// two usages by their paths, then their lines, then their columns
static int usages_order(const void *one, const void *other)
{
    const CliServersUsageRead left = usages_at(s_sorting, *(const unsigned int *)one);
    const CliServersUsageRead right = usages_at(s_sorting, *(const unsigned int *)other);
    const int path = cli_text_order(s_sorting->paths.out + left.path, s_sorting->paths.out + right.path);
    if (path != 0)
    {
        return path;
    }
    if (left.line != right.line)
    {
        return (left.line < right.line) ? -1 : 1;
    }
    return (left.from < right.from) ? -1 : ((left.from > right.from) ? 1 : 0);
}

// the usages of the name at `line`, `col` of the file at `path` read from the tree whose top is `tree`, across it where
// `across` is 1, into `read`, and their order into `order`, a cell of their count the caller gives back: 1, 0 where the
// tree knows no definition of the name, or -1 with why given
static int usages_read(const char *tree, const char *path, unsigned int line, unsigned int col, int across,
                       CliServersUsagesRead *read, unsigned int **order, VerbaLinea *err_msg)
{
    *order = NULL;
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    cli_servers_tree_path(tree, path, &own_line);
    *read = (CliServersUsagesRead){tree,
                                   {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                   {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                   {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                   0u};
    if ((mmgr_line_finish(&own_line) == 0ull) ||
        !cli_growing_line_made(&read->usage, CLI_SERVERS_USAGES_FIRST, &g_cli_servers_cells) ||
        !cli_growing_line_made(&read->paths, CLI_SERVERS_USAGES_FIRST, &g_cli_servers_cells) ||
        !cli_growing_line_made(&read->texts, CLI_SERVERS_USAGES_FIRST, &g_cli_servers_cells))
    {
        mmgr_line_put(err_msg, "the usages could not be read: the cells cannot meet them");
        return -1;
    }
    const int found = cli_symbols_usages(own, line, col, across, usages_held, read, usages_put, read, err_msg);
    if ((found == 1) && (!mmgr_line_ok(&read->usage) || !mmgr_line_ok(&read->paths) || !mmgr_line_ok(&read->texts)))
    {
        mmgr_line_put(err_msg, "the usages could not be read: the cells cannot meet them");
        return -1;
    }
    *order = ((found == 1) && (read->count > 0u))
                 ? g_cli_cells->persistent_buf_alloc((unsigned long long)read->count * sizeof(unsigned int),
                                                     &g_cli_servers_cells)
                 : NULL;
    if ((found == 1) && (read->count > 0u) && (*order == NULL))
    {
        mmgr_line_put(err_msg, "the usages could not be sorted: the cells cannot meet them");
        return -1;
    }
    for (unsigned int at = 0u; (*order != NULL) && (at < read->count); at += 1u)
    {
        (*order)[at] = at;
    }
    if (*order != NULL)
    {
        cli_lock_take(&s_sorting_lock);
        s_sorting = read;
        qsort(*order, read->count, sizeof(unsigned int), usages_order);
        cli_lock_give(&s_sorting_lock);
    }
    return found;
}

// the usages read and their order given back
static void usages_given(CliServersUsagesRead *read, unsigned int *order)
{
    cli_cell_release(order, &g_cli_servers_cells);
    cli_cell_release(read->usage.out, &g_cli_servers_cells);
    cli_cell_release(read->paths.out, &g_cli_servers_cells);
    cli_cell_release(read->texts.out, &g_cli_servers_cells);
}

int cli_servers_usages_written(const char *tree, const char *path, unsigned int line, unsigned int col, VerbaLinea *out,
                               VerbaLinea *err_msg)
{
    CliServersUsagesRead read;
    unsigned int *order = NULL;
    const int found = usages_read(tree, path, line, col, 1, &read, &order, err_msg);
    if (found == 1)
    {
        mmgr_line_ch(out, '[');
        for (unsigned int at = 0u; at < read.count; at += 1u)
        {
            const CliServersUsageRead one = usages_at(&read, order[at]);
            const char *const where = read.paths.out + one.path;
            mmgr_line_put(out, (at > 0u) ? ",{\"path\":" : "{\"path\":");
            cli_json_quoted(out, where, cellul.len(&(CatenaFinitaCfg){.src = where, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(out, ",\"from\":");
            cli_text_place_written((CliTextPlace){one.line, one.from}, out);
            mmgr_line_put(out, ",\"to\":");
            cli_text_place_written((CliTextPlace){one.line, one.to}, out);
            mmgr_line_put(out, ",\"text\":");
            cli_json_quoted_lossy(out, (const unsigned char *)(read.texts.out + one.text), one.length);
            mmgr_line_ch(out, '}');
        }
        mmgr_line_ch(out, ']');
    }
    usages_given(&read, order);
    return found;
}

// the definition the tree found put after those read
static void usages_defined_put(void *context, const char *path, unsigned int line, unsigned int from, unsigned int to,
                               const char *text, unsigned long long length)
{
    (void)to;
    (void)text;
    (void)length;
    VerbaLinea *const out = (VerbaLinea *)context;
    mmgr_line_put(out, "{\"path\":");
    cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"line\":");
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_put(out, ",\"col\":");
    mmgr_line_decimal(out, from, 1u);
    mmgr_line_ch(out, '}');
}

int cli_servers_definition_written(const char *tree, const char *path, unsigned int line, unsigned int col,
                                   VerbaLinea *out, VerbaLinea *err_msg)
{
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    cli_servers_tree_path(tree, path, &own_line);
    if (mmgr_line_finish(&own_line) == 0ull)
    {
        mmgr_line_put(err_msg, "the file is no file of the tree");
        return 0;
    }
    CliServersUsagesRead read = {tree,
                                 {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                 {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                 {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                 0u};
    mmgr_line_ch(out, '[');
    const int found = cli_symbols_definition(own, line, col, usages_held, &read, usages_defined_put, out, err_msg);
    mmgr_line_ch(out, ']');
    return found >= 0;
}

int cli_servers_renamable_written(const char *tree, const char *path, unsigned int line, unsigned int col,
                                  VerbaLinea *out, VerbaLinea *err_msg)
{
    CliServersUsagesRead read;
    unsigned int *order = NULL;
    int found = usages_read(tree, path, line, col, 0, &read, &order, err_msg);
    unsigned int at = 0u;
    CliServersUsageRead one = {0ull, 0u, 0u, 0u, 0ull, 0ull};
    while ((found == 1) && (at < read.count))
    {
        one = usages_at(&read, at);
        if ((one.line == line) && (one.from <= col) && (col <= one.to))
        {
            break;
        }
        at += 1u;
    }
    found = ((found == 1) && (at == read.count)) ? 0 : found;
    if (found == 1)
    {
        // the name is the span of its line's text from `from` to `to`, columns counted in UTF-16 units
        const char *const text = read.texts.out + one.text;
        const unsigned long long start = cli_text_byte_of(text, one.length, one.from);
        const unsigned long long end = cli_text_byte_of(text, one.length, one.to);
        mmgr_line_put(out, "{\"from\":");
        cli_text_place_written((CliTextPlace){one.line, one.from}, out);
        mmgr_line_put(out, ",\"to\":");
        cli_text_place_written((CliTextPlace){one.line, one.to}, out);
        mmgr_line_put(out, ",\"name\":");
        cli_json_quoted(out, text + start, end - start);
        mmgr_line_ch(out, '}');
    }
    usages_given(&read, order);
    return found;
}

int cli_servers_rename_written(const char *tree, const char *path, unsigned int line, unsigned int col,
                               const char *name, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliServersUsagesRead read;
    unsigned int *order = NULL;
    const int found = usages_read(tree, path, line, col, 1, &read, &order, err_msg);
    if (found == 1)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        mmgr_line_ch(out, '[');
        for (unsigned int at = 0u; at < read.count; at += 1u)
        {
            const CliServersUsageRead one = usages_at(&read, order[at]);
            const char *const where = read.paths.out + one.path;
            const int first =
                (at == 0u) ||
                !cellul.eq(&(CatenaFinitaCfg){.src = where,
                                              .other = read.paths.out + usages_at(&read, order[at - 1u]).path,
                                              .cap = CLI_CELLS_POOL});
            if (first)
            {
                mmgr_line_put(out, (at > 0u) ? "]},{\"path\":" : "{\"path\":");
                cli_json_quoted(out, where, cellul.len(&(CatenaFinitaCfg){.src = where, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(out, ",\"edits\":[");
            }
            const CliTextEdit edit = {{one.line, one.from}, {one.line, one.to}, name, size};
            mmgr_line_put(out, first ? "" : ",");
            cli_text_edit_written(&edit, out);
        }
        mmgr_line_put(out, (read.count > 0u) ? "]}]" : "]");
    }
    usages_given(&read, order);
    return found;
}
