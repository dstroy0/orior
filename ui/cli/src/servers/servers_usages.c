// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_usages.c: the definition, the usages, whether a name can be renamed and the edits that rename it, for a file
// its server is given as the tree's slice, answered from the tree (symbols_usages.c), the usages in the order of their
// files, lines and columns, each file's text the editor's where it holds one. A name the tree knows no definition of
// has none: the server is never asked
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "servers_internal.h"

#include <stdlib.h>

// The usages read: the tree's top, the usages, their paths and their lines' texts, each a line, and how many
typedef struct
{
    const char *tree;
    VerbaLinea usage;
    VerbaLinea paths;
    VerbaLinea texts;
    unsigned int count;
} CliServersUsagesRead;
// the bytes the usages, their paths and their lines' texts are each written in
#define CLI_SERVERS_USAGES_ROOM (8ull << 20u)

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

// the usages read, sorted under their own lock
static const CliServersUsagesRead *s_sorting;
static CliLock s_sorting_lock = CLI_LOCK_INIT;

// the text the editor holds of the tree's file at `path`, in a cell of its size: NULL where it holds none
static char *usages_open_text(const CliOpenTextCfg *args)
{
    const char *path = args->path;
    unsigned long long *size = args->size;
    const CliServersUsagesRead *const read = (const CliServersUsagesRead *)args->context;
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = read->tree, .relative = path, .joined = full);
    return EMBED_CALL(cli_servers_path_size_kind.text_take, CliServersPathSizeKindCfg, .path = full,
                      .size_pointer = size);
}

// a usage the tree found put after those read
static void usages_put(const CliUsageCfg *args)
{
    CliServersUsagesRead *const read = (CliServersUsagesRead *)args->context;
    const CliServersUsageRead one = {read->paths.at, args->line, args->from, args->to, read->texts.at, args->length};
    mmgr_line_put(&read->paths, args->path);
    mmgr_line_ch(&read->paths, '\0');
    // a line's text is below 2^32 bytes
    mmgr_line_put_n(&read->texts, args->text, (size_t)args->length);
    mmgr_line_put_n(&read->usage, (const char *)&one, sizeof(one));
    read->count += 1u;
}

typedef struct
{
    const CliServersUsagesRead *read_read;
    unsigned int at;
    const char *tree;
    const char *path;
    unsigned int line;
    unsigned int col;
    int across;
    CliServersUsagesRead *read;
    unsigned int **order;
    VerbaLinea *err_msg;
    unsigned int *order_value;
} ServersUsagesCtx;
// the usage at `at` of those read
EMBED_INLINE CliServersUsageRead usages_at(const ServersUsagesCtx *args)
{
    CliServersUsageRead one;
    memor.cpy(&(MemoriaCfg){
        .dst = &one, .src = args->read_read->usage.out + ((size_t)args->at * sizeof(one)), .bytes = sizeof(one)});
    return one;
}

// two usages by their paths, then their lines, then their columns
static int usages_order(const void *one, const void *other)
{
    const CliServersUsageRead left =
        EMBED_CALL(usages_at, ServersUsagesCtx, .read_read = s_sorting, .at = *(const unsigned int *)one);
    const CliServersUsageRead right =
        EMBED_CALL(usages_at, ServersUsagesCtx, .read_read = s_sorting, .at = *(const unsigned int *)other);
    const int path = EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = s_sorting->paths.out + left.path,
                                .other_pointer = s_sorting->paths.out + right.path);
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
static int usages_read(const ServersUsagesCtx *args)
{
    const char *tree = args->tree;
    const char *path = args->path;
    unsigned int line = args->line;
    unsigned int col = args->col;
    int across = args->across;
    CliServersUsagesRead *read = args->read;
    unsigned int **order = args->order;
    VerbaLinea *err_msg = args->err_msg;
    *order = NULL;
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = tree, .path = path,
               .out = &own_line);
    char *const usage_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_USAGES_ROOM, &g_cli_servers_cells);
    char *const paths_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_USAGES_ROOM, &g_cli_servers_cells);
    char *const texts_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_USAGES_ROOM, &g_cli_servers_cells);
    *read = (CliServersUsagesRead){tree, mmgr_line_over(usage_cell, CLI_SERVERS_USAGES_ROOM),
                                   mmgr_line_over(paths_cell, CLI_SERVERS_USAGES_ROOM),
                                   mmgr_line_over(texts_cell, CLI_SERVERS_USAGES_ROOM), 0u};
    if ((mmgr_line_finish(&own_line) == 0ull) || (usage_cell == NULL) || (paths_cell == NULL) || (texts_cell == NULL))
    {
        mmgr_line_put(err_msg, "the usages could not be read: the cells cannot meet them");
        return -1;
    }
    const int found = EMBED_CALL(cli_symbols_path_context.usages, CliSymbolsPathContextCfg, .path = own, .line = line,
                                 .col = col, .across = across, .open_text = usages_open_text, .open_text_context = read,
                                 .each_usage = usages_put, .context = read, .err_msg = err_msg);
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
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_sorting_lock);
        s_sorting = read;
        qsort(*order, read->count, sizeof(unsigned int), usages_order);
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_sorting_lock);
    }
    return found;
}

// the usages read and their order given back
EMBED_INLINE void usages_free(const ServersUsagesCtx *args)
{
    g_cli_cells->persistent_buf_release(args->order_value, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(args->read->usage.out, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(args->read->paths.out, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(args->read->texts.out, &g_cli_servers_cells);
}

int cli_servers_usages_write(const CliServersPathErrMsgCtx *args)
{
    CliServersUsagesRead read;
    unsigned int *order = NULL;
    const int found =
        EMBED_CALL(usages_read, ServersUsagesCtx, .tree = args->tree, .path = args->path, .line = args->line,
                   .col = args->col, .across = 1, .read = &read, .order = &order, .err_msg = args->err_msg);
    if (found == 1)
    {
        mmgr_line_ch(args->out, '[');
        for (unsigned int at = 0u; at < read.count; at += 1u)
        {
            const CliServersUsageRead one =
                EMBED_CALL(usages_at, ServersUsagesCtx, .read_read = &read, .at = order[at]);
            const char *const where = read.paths.out + one.path;
            mmgr_line_put(args->out, (at > 0u) ? ",{\"path\":" : "{\"path\":");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = where,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = where, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(args->out, ",\"from\":");
            EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = (CliTextPlace){one.line, one.from},
                       .out = args->out);
            mmgr_line_put(args->out, ",\"to\":");
            EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = (CliTextPlace){one.line, one.to},
                       .out = args->out);
            mmgr_line_put(args->out, ",\"text\":");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = (const unsigned char *)(read.texts.out + one.text), .length = one.length);
            mmgr_line_ch(args->out, '}');
        }
        mmgr_line_ch(args->out, ']');
    }
    EMBED_CALL(usages_free, ServersUsagesCtx, .read = &read, .order_value = order);
    return found;
}

// the definition the tree found put after those read
static void usages_defined_put(const CliUsageCfg *args)
{
    const char *text = args->text;
    unsigned long long length = args->length;
    (void)text;
    (void)length;
    VerbaLinea *const out = (VerbaLinea *)args->context;
    mmgr_line_put(out, "{\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"line\":");
    mmgr_line_decimal(out, args->line, 1u);
    mmgr_line_put(out, ",\"col\":");
    mmgr_line_decimal(out, args->from, 1u);
    mmgr_line_ch(out, '}');
}

int cli_servers_definition_write(const CliServersPathErrMsgCtx *args)
{
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = args->tree,
               .path = args->path, .out = &own_line);
    if (mmgr_line_finish(&own_line) == 0ull)
    {
        mmgr_line_put(args->err_msg, "the file is no file of the tree");
        return 0;
    }
    CliServersUsagesRead read = {args->tree,
                                 {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                 {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                 {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                                 0u};
    mmgr_line_ch(args->out, '[');
    const int found =
        EMBED_CALL(cli_symbols_path_context.definition, CliSymbolsPathContextCfg, .path = own, .line = args->line,
                   .col = args->col, .open_text = usages_open_text, .open_text_context = &read,
                   .each_usage = usages_defined_put, .context = args->out, .err_msg = args->err_msg);
    mmgr_line_ch(args->out, ']');
    return found >= 0;
}

int cli_servers_renamable_write(const CliServersPathErrMsgCtx *args)
{
    CliServersUsagesRead read;
    unsigned int *order = NULL;
    int found = EMBED_CALL(usages_read, ServersUsagesCtx, .tree = args->tree, .path = args->path, .line = args->line,
                           .col = args->col, .across = 0, .read = &read, .order = &order, .err_msg = args->err_msg);
    unsigned int at = 0u;
    CliServersUsageRead one = {0ull, 0u, 0u, 0u, 0ull, 0ull};
    while ((found == 1) && (at < read.count))
    {
        one = EMBED_CALL(usages_at, ServersUsagesCtx, .read_read = &read, .at = at);
        if ((one.line == args->line) && (one.from <= args->col) && (args->col <= one.to))
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
        const unsigned long long start = EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg, .line = text,
                                                    .size = one.length, .col = one.from);
        const unsigned long long end =
            EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg, .line = text, .size = one.length, .col = one.to);
        mmgr_line_put(args->out, "{\"from\":");
        EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = (CliTextPlace){one.line, one.from},
                   .out = args->out);
        mmgr_line_put(args->out, ",\"to\":");
        EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = (CliTextPlace){one.line, one.to},
                   .out = args->out);
        mmgr_line_put(args->out, ",\"name\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = text + start, .length = end - start);
        mmgr_line_ch(args->out, '}');
    }
    EMBED_CALL(usages_free, ServersUsagesCtx, .read = &read, .order_value = order);
    return found;
}

int cli_servers_rename_write(const CliServersPathErrMsgCtx *args)
{
    CliServersUsagesRead read;
    unsigned int *order = NULL;
    const int found =
        EMBED_CALL(usages_read, ServersUsagesCtx, .tree = args->tree, .path = args->path, .line = args->line,
                   .col = args->col, .across = 1, .read = &read, .order = &order, .err_msg = args->err_msg);
    if (found == 1)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
        mmgr_line_ch(args->out, '[');
        for (unsigned int at = 0u; at < read.count; at += 1u)
        {
            const CliServersUsageRead one =
                EMBED_CALL(usages_at, ServersUsagesCtx, .read_read = &read, .at = order[at]);
            const char *const where = read.paths.out + one.path;
            const int first =
                (at == 0u) ||
                !cellul.eq(&(CatenaFinitaCfg){
                    .src = where,
                    .other = read.paths.out +
                             EMBED_CALL(usages_at, ServersUsagesCtx, .read_read = &read, .at = order[at - 1u]).path,
                    .cap = CLI_CELLS_POOL});
            if (first)
            {
                mmgr_line_put(args->out, (at > 0u) ? "]},{\"path\":" : "{\"path\":");
                EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                           .text = where,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = where, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(args->out, ",\"edits\":[");
            }
            const CliTextEdit edit = {{one.line, one.from}, {one.line, one.to}, args->name, size};
            mmgr_line_put(args->out, first ? "" : ",");
            EMBED_CALL(cli_edits.text_edit_write, CliEditsCfg, .edit = &edit, .out = args->out);
        }
        mmgr_line_put(args->out, (read.count > 0u) ? "]}]" : "]");
    }
    EMBED_CALL(usages_free, ServersUsagesCtx, .read = &read, .order_value = order);
    return found;
}
