// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_request.c: what the server of a file answers about a place in it: a hover, the definitions, the completions,
// orior's own reading's of a build file or JSON first, every usage of a symbol with its line's text read from its file
// once, whether the symbol can be renamed and the edits that rename it, and the signature of the call the cursor is
// in. An answer requested while no server has the file open is none, but a usage, a rename and what it would change,
// which report there is no server to query. A file given its server as slices is asked of the tree alone: its
// definition, usages and renames are the tree's (servers_usages.c), its completions the names the tree has in effect
// at the place (symbols_complete.c), and what the tree does not answer is none
#include "../builds/builds.h"
#include "../code/schema.h"
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "servers_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// One usage: its path in the paths' pool, its span, and its place among the usages
typedef struct
{
    unsigned int path;
    CliTextPlace from;
    CliTextPlace to;
    unsigned int order;
} CliServersUsage;

// what a server answers quickly and what a search of the tree answers, each read under its own lock
static CliServersReading s_quick = {0};
static CliServersReading s_search = {0};
// the completions, under the quick reading's lock; and the usages and their paths, each in a cell of the answer's
// size, under the search's
static CliCompletions s_items;
static CliServersUsage *s_usage;
static char *s_usage_paths;

typedef struct
{
    const char *path;
    VerbaLinea *err_msg;
    CliLsp *server;
    CliServersReading *reading;
    const char *method;
    unsigned int line;
    unsigned int col;
    const char *added;
    unsigned long long wait;
    int *requested;
    VerbaLinea *out;
    unsigned int one;
    char *path_out;
    CliTextPlace *from;
    CliTextPlace *to;
    unsigned int all;
    const char *file;
    unsigned long long size;
    unsigned int *at;
    unsigned long long *start;
} ServersRequestCtx;
// the server that has `path` open, referenced for the caller, or NULL with why there is none given
EMBED_INLINE CliLsp *request_server(const ServersRequestCtx *args)
{
    const char *path = args->path;
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = path);
    if (server == NULL)
    {
        mmgr_line_put(args->err_msg, "no language server has this file open");
    }
    return server;
}

// `method` sent to `server` about `line`, `col` of `path`, `added` after the place, the answer read into `reading`:
// its root, or CLI_JSON_NONE with why given, where `requested` is 0
EMBED_INLINE unsigned int request_read(const ServersRequestCtx *args)
{
    CliLsp *server = args->server;
    CliServersReading *reading = args->reading;
    const char *method = args->method;
    const char *path = args->path;
    unsigned int line = args->line;
    unsigned int col = args->col;
    const char *added = args->added;
    unsigned long long wait = args->wait;
    int *requested = args->requested;
    VerbaLinea *err_msg = args->err_msg;
    CliLspAnswer answer;
    *requested = EMBED_CALL(cli_servers_path_internal.request_at, CliServersPathCtx, .server = server, .method = method,
                            .path = path, .line = line, .col = col, .added = added, .wait = wait, .answer = &answer,
                            .err_msg = err_msg);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    return *requested ? EMBED_CALL(cli_servers_reading_answer_internal.answer_read, CliServersReadingAnswerCtx,
                                   .reading = reading, .answer = &answer)
                      : CLI_JSON_NONE;
}

int cli_servers_hover(const CliServersPathOutCfg *args)
{
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = args->path);
    if (server == NULL)
    {
        mmgr_line_put(args->out, "null");
        return 1;
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_quick);
    int requested = 0;
    const unsigned int root = EMBED_CALL(request_read, ServersRequestCtx, .server = server, .reading = &s_quick,
                                         .method = "textDocument/hover", .path = args->path, .line = args->line,
                                         .col = args->col, .added = NULL, .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS,
                                         .requested = &requested, .err_msg = args->err_msg);
    if (requested)
    {
        const unsigned long long room =
            EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = &s_quick.json);
        char *const shown = g_cli_cells->temporary_buf_alloc(room, &g_cli_servers_cells);
        VerbaLinea markdown = mmgr_line_over(shown, room);
        EMBED_CALL(cli_servers_json_out_internal.markdown_get, CliServersJsonOutCtx, .json = &s_quick.json,
                   .contents = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &s_quick.json, .object = root, .name = "contents"),
                   .out = &markdown);
        const unsigned long long size = mmgr_line_ok(&markdown) ? markdown.at : 0ull;
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = (size > 0ull) ? shown : "",
                   .size = size, .from_u64s = &from, .to_u64s = &to);
        if (to > from)
        {
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = shown,
                       .length = size);
        }
        else
        {
            mmgr_line_put(args->out, "null");
        }
        g_cli_cells->persistent_buf_release(shown, &g_cli_servers_cells);
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_quick);
    return requested;
}

int cli_servers_definition(const CliServersPathOutCfg *args)
{
    if (EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = args->path))
    {
        return EMBED_CALL(cli_servers_path_err_msg_internal.definition_write, CliServersPathErrMsgCtx,
                          .tree = args->tree, .path = args->path, .line = args->line, .col = args->col,
                          .out = args->out, .err_msg = args->err_msg);
    }
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = args->path);
    if (server == NULL)
    {
        mmgr_line_put(args->out, "[]");
        return 1;
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_quick);
    int requested = 0;
    const unsigned int root = EMBED_CALL(request_read, ServersRequestCtx, .server = server, .reading = &s_quick,
                                         .method = "textDocument/definition", .path = args->path, .line = args->line,
                                         .col = args->col, .added = NULL, .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS,
                                         .requested = &requested, .err_msg = args->err_msg);
    if (requested)
    {
        const CliJson *const json = &s_quick.json;
        mmgr_line_ch(args->out, '[');
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = root,
                       .kind = CFG_JSON_ARRAY))
        {
            int written = 0;
            for (unsigned int one = json->node[root].first; one != CLI_JSON_NONE; one = json->node[one].next)
            {
                const unsigned long long before = args->out->at;
                mmgr_line_put(args->out, written ? "," : "");
                if (!EMBED_CALL(cli_servers_json_tree_internal.found_get, CliServersJsonTreeCtx, .json = json,
                                .location = one, .tree = args->tree, .out = args->out))
                {
                    args->out->at = before;
                    continue;
                }
                written = 1;
            }
        }
        else if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = root,
                             .kind = CFG_JSON_NULL))
        {
            (void)EMBED_CALL(cli_servers_json_tree_internal.found_get, CliServersJsonTreeCtx, .json = json,
                             .location = root, .tree = args->tree, .out = args->out);
        }
        mmgr_line_ch(args->out, ']');
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_quick);
    return requested;
}

// the completions written as an array
EMBED_INLINE void request_items_write(const ServersRequestCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < s_items.count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_edits_out_items.completion_write, CliEditsOutItemsCfg, .items_read = &s_items, .at = at,
                   .out = out);
    }
    mmgr_line_ch(out, ']');
}

// a name the tree completes a word with put among the completions as what it is, a kind the tree names saved by the
// page's own word for it, `word` for one it does not name, as an item saves its kind by pointer
static void request_tree_item(const CliDeclaredCfg *args)
{
    const char *kind = args->kind;
    static const char *const s_kinds[8] = {"function", "constant", "class",     "method",
                                           "module",   "variable", "parameter", "local"};
    const char *named = "word";
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        named = cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = s_kinds[at], .cap = CLI_CELLS_POOL}) ? s_kinds[at]
                                                                                                        : named;
    }
    (void)EMBED_CALL(cli_edits_items.completion_put, CliEditsItemsCfg, .items = &s_items, .label = args->name,
                     .label_size = args->length, .kind = named, .detail = "", .detail_size = 0ull, .insert = args->name,
                     .insert_size = args->length, .snippet = 0);
}

// the names the tree completes the word at `line`, `col` of the file at `path` with, the editor's text of it read,
// put among the completions
EMBED_INLINE void request_tree_complete(const ServersRequestCtx *args)
{
    const char *path = args->path;
    unsigned int line = args->line;
    unsigned int col = args->col;
    char tree[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = tree, .src = g_cli_servers.checked_root, .bytes = sizeof(tree)});
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = tree, .path = path,
               .out = &own_line);
    unsigned long long size = 0ull;
    char *const text = (mmgr_line_finish(&own_line) != 0ull)
                           ? EMBED_CALL(cli_servers_path_size_kind.text_take, CliServersPathSizeKindCfg, .path = path,
                                        .size_pointer = &size)
                           : NULL;
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    if (text != NULL)
    {
        (void)EMBED_CALL(cli_symbols_path_text_size.complete, CliSymbolsPathTextSizeCfg, .path = own, .text = text,
                         .size = size, .line = line, .col = col, .each_declared = request_tree_item, .context = NULL,
                         .err_msg = &err_msg);
    }
    g_cli_cells->persistent_buf_release(text, &g_cli_servers_cells);
}

int cli_servers_complete(const CliServersPathOutCfg *args)
{
    const char *path = args->path;
    unsigned int line = args->line;
    unsigned int col = args->col;
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_quick);
    EMBED_CALL(cli_edits_items.completions_begin, CliEditsItemsCfg, .items = &s_items);
    // a file given its server as a slice is completed from the tree
    if (EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = path))
    {
        EMBED_CALL(request_tree_complete, ServersRequestCtx, .path = path, .line = line, .col = col);
    }
    if (EMBED_CALL(cli_servers_path_size_root_sink_internal.own_reads, CliServersPathSizeRootSinkCtx, .path = path))
    {
        char root[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_servers.checked_root, .bytes = sizeof(root)});
        unsigned long long size = 0ull;
        char *const text = (root[0] != '\0')
                               ? EMBED_CALL(cli_servers_path_size_kind.text_take, CliServersPathSizeKindCfg,
                                            .path = path, .size_pointer = &size)
                               : NULL;
        if ((text != NULL) && (EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = path) != 0u))
        {
            EMBED_CALL(cli_builds_path.build_complete, CliBuildsPathCfg, .root = root, .path = path, .text = text,
                       .size = size, .line = line, .col = col, .items = &s_items);
        }
        else if (text != NULL)
        {
            EMBED_CALL(cli_schema_line.complete, CliSchemaLineCfg, .path = path, .text = text, .size = size,
                       .line = line, .col = col, .items = &s_items);
        }
        g_cli_cells->persistent_buf_release(text, &g_cli_servers_cells);
    }
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = path);
    int requested = 1;
    if (server != NULL)
    {
        const unsigned long long before = args->err_msg->at;
        const unsigned int root = EMBED_CALL(
            request_read, ServersRequestCtx, .server = server, .reading = &s_quick, .method = "textDocument/completion",
            .path = path, .line = line, .col = col, .added = "\"context\":{\"triggerKind\":1}",
            .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS, .requested = &requested, .err_msg = args->err_msg);
        if (!requested && (s_items.count > 0u))
        {
            args->err_msg->at = before;
            requested = 1;
        }
        const CliJson *const json = &s_quick.json;
        const unsigned int items = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = json, .node = root, .kind = CFG_JSON_ARRAY)
                                       ? root
                                       : EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = json, .object = root, .name = "items");
        for (unsigned int one = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                           .node = items, .kind = CFG_JSON_ARRAY)
                                    ? json->node[items].first
                                    : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            EMBED_CALL(cli_servers_json_one_internal.item_get, CliServersJsonOneCtx, .json = json, .one = one,
                       .items = &s_items);
        }
    }
    if (requested)
    {
        EMBED_CALL(request_items_write, ServersRequestCtx, .out = args->out);
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_quick);
    return requested;
}

// two usages by their path, then where they start, then the order they were given in
static int request_usage_order(const void *one, const void *other)
{
    const CliServersUsage *const left = one;
    const CliServersUsage *const right = other;
    const int path = EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = s_usage_paths + left->path,
                                .other_pointer = s_usage_paths + right->path);
    if (path != 0)
    {
        return path;
    }
    if (left->from.line != right->from.line)
    {
        return (left->from.line < right->from.line) ? -1 : 1;
    }
    if (left->from.col != right->from.col)
    {
        return (left->from.col < right->from.col) ? -1 : 1;
    }
    return (left->order < right->order) ? -1 : 1;
}

// the path of the usage `one` of the answer read into `path` and its span into `from` and `to`: the bytes the path
// takes with the 0 after it, or 0 where it names none
EMBED_INLINE unsigned long long request_usage_path(const ServersRequestCtx *args)
{
    char *path = args->path_out;
    CliTextPlace *from = args->from;
    CliTextPlace *to = args->to;
    const CliJson *const json = &s_search.json;
    unsigned long long length = 0ull;
    const char *const uri =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->one, .name = "uri"),
                   .length_pointer = &length);
    const unsigned int range = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->one, .name = "range");
    *from = EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = range, .name = "start"));
    *to = EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = json,
                     .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                        .object = range, .name = "end"));
    const int found = (uri != NULL) && EMBED_CALL(cli_lsp_length_uri.path_get, CliLspLengthUriCfg, .uri = uri,
                                                  .length = length, .path_out = path);
    return found ? cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull : 0ull;
}

// the usages of the answer read, and their paths, each in a cell of their size: their count, 0 where none are or the
// cells cannot meet them; the cells given back with the search
EMBED_INLINE unsigned int request_usages_get(const ServersRequestCtx *args)
{
    const CliJson *const json = &s_search.json;
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = args->all, .kind = CFG_JSON_ARRAY)
                                   ? json->node[args->all].first
                                   : CLI_JSON_NONE;
    char path[CLI_PATH_MAX_LEN];
    CliTextPlace from = {0u, 0u};
    CliTextPlace to = {0u, 0u};
    unsigned long long paths = 0ull;
    for (unsigned int one = first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        paths +=
            EMBED_CALL(request_usage_path, ServersRequestCtx, .one = one, .path_out = path, .from = &from, .to = &to);
    }
    s_usage = (paths > 0ull)
                  ? g_cli_cells->persistent_buf_alloc(
                        (unsigned long long)json->node[args->all].count * sizeof(CliServersUsage), &g_cli_servers_cells)
                  : NULL;
    s_usage_paths = (s_usage != NULL) ? g_cli_cells->persistent_buf_alloc(paths, &g_cli_servers_cells) : NULL;
    unsigned int count = 0u;
    unsigned int used = 0u;
    for (unsigned int one = (s_usage_paths != NULL) ? first : CLI_JSON_NONE; one != CLI_JSON_NONE;
         one = json->node[one].next)
    {
        const unsigned long long size =
            EMBED_CALL(request_usage_path, ServersRequestCtx, .one = one, .path_out = path, .from = &from, .to = &to);
        if (size == 0ull)
        {
            continue;
        }
        // a path's bytes are under CLI_PATH_MAX_LEN
        memor.cpy(&(MemoriaCfg){.dst = s_usage_paths + used, .src = path, .bytes = (size_t)size});
        s_usage[count] = (CliServersUsage){used, from, to, count};
        used += (unsigned int)size;
        count += 1u;
    }
    if (count > 0u)
    {
        qsort(s_usage, count, sizeof(s_usage[0]), request_usage_order);
    }
    return count;
}

// the usages and their paths given back
static void request_usages_free(void)
{
    g_cli_cells->persistent_buf_release(s_usage, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(s_usage_paths, &g_cli_servers_cells);
    s_usage = NULL;
    s_usage_paths = NULL;
}

// the text of the line `line` of the `size` bytes of `file`, its "\r" before its end left off, written as a JSON
// string, the line `*at` starts at byte `*start` walked forward from there
EMBED_INLINE void request_line_write(const ServersRequestCtx *args)
{
    unsigned long long size = args->size;
    unsigned int *at = args->at;
    unsigned long long *start = args->start;
    VerbaLinea *out = args->out;
    while ((*at < args->line) && (*start < size))
    {
        *start = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->file, .at = *start,
                            .size = size, .set = "\n");
        *start += (*start < size) ? 1ull : 0ull;
        *at += 1u;
    }
    unsigned long long end = *start;
    end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->file, .at = end,
                     .size = size, .set = "\n");
    const unsigned long long cut = ((end > *start) && (args->file[end - 1ull] == '\r')) ? end - 1ull : end;
    const int there = (*at == args->line) && (*start < size);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = (size > 0ull) ? args->file + *start : "", .length = there ? cut - *start : 0ull);
}

int cli_servers_references(const CliServersPathOutCfg *args)
{
    // a file given its server as a slice is answered from the tree alone, a name it knows no definition of with none
    if (EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = args->path))
    {
        const int tree_found = EMBED_CALL(cli_servers_path_err_msg_internal.usages_write, CliServersPathErrMsgCtx,
                                          .tree = args->tree, .path = args->path, .line = args->line, .col = args->col,
                                          .out = args->out, .err_msg = args->err_msg);
        mmgr_line_put(args->out, (tree_found == 0) ? "[]" : "");
        return tree_found >= 0;
    }
    CliLsp *const server = EMBED_CALL(request_server, ServersRequestCtx, .path = args->path, .err_msg = args->err_msg);
    if (server == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_search);
    int requested = 0;
    const unsigned int all = EMBED_CALL(
        request_read, ServersRequestCtx, .server = server, .reading = &s_search, .method = "textDocument/references",
        .path = args->path, .line = args->line, .col = args->col, .added = "\"context\":{\"includeDeclaration\":true}",
        .wait = CLI_SERVERS_SEARCHING, .requested = &requested, .err_msg = args->err_msg);
    if (requested)
    {
        const unsigned int count = EMBED_CALL(request_usages_get, ServersRequestCtx, .all = all);
        // the file of the usages written, mapped where it lies while they are, NULL where it is not read whole
        const unsigned char *mapped = NULL;
        unsigned long long mapped_size = 0ull;
        unsigned long long size = 0ull;
        unsigned int at_line = 0u;
        unsigned long long start = 0ull;
        mmgr_line_ch(args->out, '[');
        for (unsigned int one = 0u; one < count; one += 1u)
        {
            const CliServersUsage *const usage = &s_usage[one];
            const char *const file = s_usage_paths + usage->path;
            const int same_file =
                (one > 0u) &&
                (cellul.eq(&(CatenaFinitaCfg){
                    .src = file, .other = s_usage_paths + s_usage[one - 1u].path, .cap = CLI_CELLS_POOL}));
            if (same_file && (usage->from.line == s_usage[one - 1u].from.line) &&
                (usage->from.col == s_usage[one - 1u].from.col))
            {
                continue;
            }
            if (!same_file)
            {
                EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
                           .owner = &g_cli_servers_cells);
                mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = file, .size_pointer = &mapped_size,
                                    .owner = &g_cli_servers_cells);
                size = ((mapped != NULL) && EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg,
                                                       .bytes = (const char *)mapped, .size = mapped_size))
                           ? mapped_size
                           : 0ull;
                at_line = 0u;
                start = 0ull;
            }
            mmgr_line_put(args->out, (one > 0u) ? ",{\"path\":" : "{\"path\":");
            char shown[CLI_PATH_MAX_LEN];
            VerbaLinea where = {shown, sizeof(shown), 0ull};
            EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = args->tree,
                       .path = file, .out = &where);
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                       .text_bytes = (const unsigned char *)shown, .length = mmgr_line_ok(&where) ? where.at : 0ull);
            mmgr_line_put(args->out, ",\"from\":");
            EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = usage->from, .out = args->out);
            mmgr_line_put(args->out, ",\"to\":");
            EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = usage->to, .out = args->out);
            mmgr_line_put(args->out, ",\"text\":");
            EMBED_CALL(request_line_write, ServersRequestCtx, .file = (const char *)mapped, .size = size,
                       .line = usage->from.line, .at = &at_line, .start = &start, .out = args->out);
            mmgr_line_ch(args->out, '}');
        }
        mmgr_line_ch(args->out, ']');
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
                   .owner = &g_cli_servers_cells);
        request_usages_free();
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_search);
    return requested;
}

int cli_servers_renamable(const CliServersPathOutCfg *args)
{
    // a file given its server as a slice is answered from the tree alone, a name it knows no definition of with none
    char tree[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = tree, .src = g_cli_servers.checked_root, .bytes = sizeof(tree)});
    if (EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = args->path))
    {
        const int tree_found = EMBED_CALL(cli_servers_path_err_msg_internal.renamable_write, CliServersPathErrMsgCtx,
                                          .tree = tree, .path = args->path, .line = args->line, .col = args->col,
                                          .out = args->out, .err_msg = args->err_msg);
        mmgr_line_put(args->out, (tree_found == 0) ? "null" : "");
        return tree_found >= 0;
    }
    CliLsp *const server = EMBED_CALL(request_server, ServersRequestCtx, .path = args->path, .err_msg = args->err_msg);
    if (server == NULL)
    {
        return 0;
    }
    if (!EMBED_CALL(cli_servers_server_internal.capable_true, CliServersServerCtx, .server = server,
                    .name = "renameProvider", .inner = "prepareProvider"))
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
        mmgr_line_put(args->out, "null");
        return 1;
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_quick);
    int requested = 0;
    const unsigned int root = EMBED_CALL(request_read, ServersRequestCtx, .server = server, .reading = &s_quick,
                                         .method = "textDocument/prepareRename", .path = args->path, .line = args->line,
                                         .col = args->col, .added = NULL, .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS,
                                         .requested = &requested, .err_msg = args->err_msg);
    const CliJson *const json = &s_quick.json;
    const unsigned int named = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = root, .name = "range");
    const unsigned int range = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = named, .kind = CFG_JSON_OBJECT)
                                   ? named
                                   : root;
    const unsigned int start = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = range, .name = "start");
    const CliTextPlace from = EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx,
                                         .json = json, .node = start);
    const CliTextPlace to =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.place, CliServersJsonNodeOtherwiseCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = range, .name = "end"));
    if (requested && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                 .node = start, .kind = CFG_JSON_OBJECT))
    {
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = root,
                       .kind = CFG_JSON_NULL))
        {
            mmgr_line_put(args->out, "null");
        }
        else
        {
            mmgr_line_put(args->err_msg, "nothing here can be renamed");
            requested = 0;
        }
    }
    else if (requested)
    {
        mmgr_line_put(args->out, "{\"from\":");
        EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = from, .out = args->out);
        mmgr_line_put(args->out, ",\"to\":");
        EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = to, .out = args->out);
        unsigned long long size = 0ull;
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = root, .name = "placeholder"),
                       .length_pointer = &size);
        mmgr_line_put(args->out, ",\"name\":");
        if (name != NULL)
        {
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = name,
                       .length = size);
        }
        else
        {
            mmgr_line_put(args->out, "null");
        }
        mmgr_line_ch(args->out, '}');
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_quick);
    return requested;
}

int cli_servers_rename(const CliServersPathOutCfg *args)
{
    // a file given its server as a slice is renamed across the tree alone, a name it knows no definition of not at all
    if (EMBED_CALL(cli_servers_path_lsp_internal.slice, CliServersPathLspCtx, .path = args->path))
    {
        const int tree_found = EMBED_CALL(cli_servers_path_err_msg_internal.rename_write, CliServersPathErrMsgCtx,
                                          .tree = args->tree, .path = args->path, .line = args->line, .col = args->col,
                                          .name = args->name, .out = args->out, .err_msg = args->err_msg);
        if (tree_found == 0)
        {
            mmgr_line_put(args->err_msg, "the tree knows no definition of the name to rename");
        }
        return tree_found == 1;
    }
    CliLsp *const server = EMBED_CALL(request_server, ServersRequestCtx, .path = args->path, .err_msg = args->err_msg);
    if (server == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_search);
    char named[CLI_SERVERS_PARAMS];
    VerbaLinea added = {named, sizeof(named), 0ull};
    mmgr_line_put(&added, "\"newName\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &added, .text = args->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}));
    int requested = 0;
    unsigned int root = CLI_JSON_NONE;
    if (mmgr_line_ok(&added))
    {
        root = EMBED_CALL(request_read, ServersRequestCtx, .server = server, .reading = &s_search,
                          .method = "textDocument/rename", .path = args->path, .line = args->line, .col = args->col,
                          .added = named, .wait = CLI_SERVERS_SEARCHING, .requested = &requested,
                          .err_msg = args->err_msg);
    }
    else
    {
        EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
        mmgr_line_put(args->err_msg, "the rename could not be written: its new name does not fit");
    }
    if (requested && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_search.json,
                                .node = root, .kind = CFG_JSON_NULL))
    {
        mmgr_line_put(args->err_msg, "the server found nothing to rename");
        requested = 0;
    }
    if (requested)
    {
        EMBED_CALL(cli_servers_json_tree_out_internal.workspace_edit_get, CliServersJsonTreeOutCtx,
                   .json = &s_search.json, .edit = root, .tree = args->tree, .out = args->out);
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_search);
    return requested;
}

int cli_servers_signature(const CliServersPathOutCfg *args)
{
    CliLsp *const server =
        EMBED_CALL(cli_servers_path_lsp_internal.owner_answering, CliServersPathLspCtx, .path = args->path);
    if (server == NULL)
    {
        mmgr_line_put(args->out, "null");
        return 1;
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_quick);
    int requested = 0;
    const unsigned int root = EMBED_CALL(request_read, ServersRequestCtx, .server = server, .reading = &s_quick,
                                         .method = "textDocument/signatureHelp", .path = args->path, .line = args->line,
                                         .col = args->col, .added = NULL, .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS,
                                         .requested = &requested, .err_msg = args->err_msg);
    if (requested)
    {
        EMBED_CALL(cli_servers_json_out_help_internal.signature_get, CliServersJsonOutHelpCtx, .json = &s_quick.json,
                   .help = root, .out = args->out);
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_quick);
    return requested;
}
