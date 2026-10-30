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
static CliServersReading s_quick = {CLI_LOCK_INIT};
static CliServersReading s_search = {CLI_LOCK_INIT};
// the completions, under the quick reading's lock; and the usages and their paths, each in a cell of the answer's
// size, under the search's
static CliCompletions s_items;
static CliServersUsage *s_usage;
static char *s_usage_paths;

// the server that has `path` open, referenced for the caller, or NULL with why there is none given
static CliLsp *request_server(const char *path, VerbaLinea *err_msg)
{
    CliLsp *const server = cli_servers_owner_answering(path);
    if (server == NULL)
    {
        mmgr_line_put(err_msg, "no language server has this file open");
    }
    return server;
}

// `method` sent to `server` about `line`, `col` of `path`, `added` after the place, the answer read into `reading`:
// its root, or CLI_JSON_NONE with why given, where `requested` is 0
static unsigned int request_read(CliLsp *server, CliServersReading *reading, const char *method, const char *path,
                                 unsigned int line, unsigned int col, const char *added, unsigned long long wait,
                                 int *requested, VerbaLinea *err_msg)
{
    VerbaLinea result;
    cli_servers_answer_begun(reading, &result);
    *requested = cli_servers_request_at(server, method, path, line, col, added, wait, &result, err_msg);
    cli_lsp_unref(server);
    return *requested ? cli_servers_answer_read(reading, &result) : CLI_JSON_NONE;
}

int cli_servers_hover(const char *path, unsigned int line, unsigned int col, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliLsp *const server = cli_servers_owner_answering(path);
    if (server == NULL)
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    cli_servers_reading_locked(&s_quick);
    int requested = 0;
    const unsigned int root = request_read(server, &s_quick, "textDocument/hover", path, line, col, NULL,
                                           CLI_SERVERS_REQUEST_TIMEOUT_MS, &requested, err_msg);
    if (requested)
    {
        char piece[CLI_SERVERS_PIECE];
        CliPieces pieces;
        VerbaLinea markdown;
        cli_pieces_begun(&pieces, &markdown, piece, sizeof(piece), &g_cli_servers_cells);
        cli_servers_markdown_of(&s_quick.json, cli_json_member(&s_quick.json, root, "contents"), &markdown);
        unsigned long long size = 0ull;
        char *const shown = cli_pieces_joined(&pieces, &markdown, "a hover's text", &size);
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        cli_text_trimmed((shown != NULL) ? shown : "", (shown != NULL) ? size : 0ull, &from, &to);
        if (to > from)
        {
            cli_json_quoted(out, shown, size);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        cli_cell_release(shown, &g_cli_servers_cells);
    }
    cli_servers_reading_released(&s_quick);
    return requested;
}

int cli_servers_definition(const char *tree, const char *path, unsigned int line, unsigned int col, VerbaLinea *out,
                           VerbaLinea *err_msg)
{
    if (cli_servers_sliced(path))
    {
        return cli_servers_definition_written(tree, path, line, col, out, err_msg);
    }
    CliLsp *const server = cli_servers_owner_answering(path);
    if (server == NULL)
    {
        mmgr_line_put(out, "[]");
        return 1;
    }
    cli_servers_reading_locked(&s_quick);
    int requested = 0;
    const unsigned int root = request_read(server, &s_quick, "textDocument/definition", path, line, col, NULL,
                                           CLI_SERVERS_REQUEST_TIMEOUT_MS, &requested, err_msg);
    if (requested)
    {
        const CliJson *const json = &s_quick.json;
        mmgr_line_ch(out, '[');
        if (cli_json_is(json, root, CFG_JSON_ARRAY))
        {
            int written = 0;
            for (unsigned int one = json->node[root].first; one != CLI_JSON_NONE; one = json->node[one].next)
            {
                const unsigned long long before = out->at;
                mmgr_line_put(out, written ? "," : "");
                if (!cli_servers_found_of(json, one, tree, out))
                {
                    out->at = before;
                    continue;
                }
                written = 1;
            }
        }
        else if (!cli_json_is(json, root, CFG_JSON_NULL))
        {
            (void)cli_servers_found_of(json, root, tree, out);
        }
        mmgr_line_ch(out, ']');
    }
    cli_servers_reading_released(&s_quick);
    return requested;
}

// the completions written as an array
static void request_items_written(VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < s_items.count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        cli_completion_written(&s_items, at, out);
    }
    mmgr_line_ch(out, ']');
}

// a name the tree completes a word with put among the completions as what it is, a kind the tree names kept by the
// page's own word for it, `word` for one it does not name, as an item keeps its kind by pointer
static void request_tree_item(void *context, const char *name, unsigned long long length, const char *kind,
                              unsigned int line)
{
    (void)context;
    (void)line;
    static const char *const s_kinds[8] = {"function", "constant", "class",     "method",
                                           "module",   "variable", "parameter", "local"};
    const char *named = "word";
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        named = cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = s_kinds[at], .cap = CLI_CELLS_POOL}) ? s_kinds[at]
                                                                                                        : named;
    }
    (void)cli_completion_put(&s_items, name, length, named, "", 0ull, name, length, 0);
}

// the names the tree completes the word at `line`, `col` of the file at `path` with, the editor's text of it read,
// put among the completions
static void request_tree_completed(const char *path, unsigned int line, unsigned int col)
{
    char tree[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_servers.lock);
    memor.cpy(&(MemoriaCfg){.dst = tree, .src = g_cli_servers.checked_root, .bytes = sizeof(tree)});
    cli_lock_give(&g_cli_servers.lock);
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    cli_servers_tree_path(tree, path, &own_line);
    unsigned long long size = 0ull;
    char *const text = (mmgr_line_finish(&own_line) != 0ull) ? cli_servers_text_taken(path, &size) : NULL;
    char quiet[256];
    VerbaLinea err_msg = {quiet, sizeof(quiet), 0ull};
    if (text != NULL)
    {
        (void)cli_symbols_complete(own, text, size, line, col, request_tree_item, NULL, &err_msg);
    }
    cli_cell_release(text, &g_cli_servers_cells);
}

int cli_servers_complete(const char *path, unsigned int line, unsigned int col, VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_servers_reading_locked(&s_quick);
    cli_completions_begun(&s_items);
    // a file given its server as a slice is completed from the tree
    if (cli_servers_sliced(path))
    {
        request_tree_completed(path, line, col);
    }
    if (cli_servers_own_reads(path))
    {
        char root[CLI_PATH_MAX_LEN];
        cli_lock_take(&g_cli_servers.lock);
        memor.cpy(&(MemoriaCfg){.dst = root, .src = g_cli_servers.checked_root, .bytes = sizeof(root)});
        cli_lock_give(&g_cli_servers.lock);
        unsigned long long size = 0ull;
        char *const text = (root[0] != '\0') ? cli_servers_text_taken(path, &size) : NULL;
        if ((text != NULL) && (cli_build_kind_of(path) != 0u))
        {
            cli_build_complete(root, path, text, size, line, col, &s_items);
        }
        else if (text != NULL)
        {
            cli_schema_complete(path, text, size, line, col, &s_items);
        }
        cli_cell_release(text, &g_cli_servers_cells);
    }
    CliLsp *const server = cli_servers_owner_answering(path);
    int requested = 1;
    if (server != NULL)
    {
        const unsigned long long before = err_msg->at;
        const unsigned int root =
            request_read(server, &s_quick, "textDocument/completion", path, line, col,
                         "\"context\":{\"triggerKind\":1}", CLI_SERVERS_REQUEST_TIMEOUT_MS, &requested, err_msg);
        if (!requested && (s_items.count > 0u))
        {
            err_msg->at = before;
            requested = 1;
        }
        const CliJson *const json = &s_quick.json;
        const unsigned int items =
            cli_json_is(json, root, CFG_JSON_ARRAY) ? root : cli_json_member(json, root, "items");
        for (unsigned int one = cli_json_is(json, items, CFG_JSON_ARRAY) ? json->node[items].first : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = json->node[one].next)
        {
            cli_servers_item_of(json, one, &s_items);
        }
    }
    if (requested)
    {
        request_items_written(out);
    }
    cli_servers_reading_released(&s_quick);
    return requested;
}

// two usages by their path, then where they start, then the order they were given in
static int request_usage_order(const void *one, const void *other)
{
    const CliServersUsage *const left = one;
    const CliServersUsage *const right = other;
    const int path = cli_text_order(s_usage_paths + left->path, s_usage_paths + right->path);
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
static unsigned long long request_usage_path(unsigned int one, char *path, CliTextPlace *from, CliTextPlace *to)
{
    const CliJson *const json = &s_search.json;
    unsigned long long length = 0ull;
    const char *const uri = cli_json_string(json, cli_json_member(json, one, "uri"), &length);
    const unsigned int range = cli_json_member(json, one, "range");
    *from = cli_servers_place(json, cli_json_member(json, range, "start"));
    *to = cli_servers_place(json, cli_json_member(json, range, "end"));
    const int found = (uri != NULL) && cli_lsp_path_of(uri, length, path);
    return found ? cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull : 0ull;
}

// the usages of the answer read, and their paths, each in a cell of their size: their count, 0 where none are or the
// cells cannot meet them; the cells given back with the search
static unsigned int request_usages_of(unsigned int all)
{
    const CliJson *const json = &s_search.json;
    const unsigned int first = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].first : CLI_JSON_NONE;
    char path[CLI_PATH_MAX_LEN];
    CliTextPlace from = {0u, 0u};
    CliTextPlace to = {0u, 0u};
    unsigned long long paths = 0ull;
    for (unsigned int one = first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        paths += request_usage_path(one, path, &from, &to);
    }
    s_usage = (paths > 0ull)
                  ? g_cli_cells->persistent_buf_alloc(
                        (unsigned long long)json->node[all].count * sizeof(CliServersUsage), &g_cli_servers_cells)
                  : NULL;
    s_usage_paths = (s_usage != NULL) ? g_cli_cells->persistent_buf_alloc(paths, &g_cli_servers_cells) : NULL;
    unsigned int count = 0u;
    unsigned int used = 0u;
    for (unsigned int one = (s_usage_paths != NULL) ? first : CLI_JSON_NONE; one != CLI_JSON_NONE;
         one = json->node[one].next)
    {
        const unsigned long long size = request_usage_path(one, path, &from, &to);
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
static void request_usages_given(void)
{
    cli_cell_release(s_usage, &g_cli_servers_cells);
    cli_cell_release(s_usage_paths, &g_cli_servers_cells);
    s_usage = NULL;
    s_usage_paths = NULL;
}

// the text of the line `line` of the `size` bytes of `file`, its "\r" before its end left off, written as a JSON
// string, the line `*at` starts at byte `*start` walked forward from there
static void request_line_written(const char *file, unsigned long long size, unsigned int line, unsigned int *at,
                                 unsigned long long *start, VerbaLinea *out)
{
    while ((*at < line) && (*start < size))
    {
        while ((*start < size) && (file[*start] != '\n'))
        {
            *start += 1ull;
        }
        *start += (*start < size) ? 1ull : 0ull;
        *at += 1u;
    }
    unsigned long long end = *start;
    while ((end < size) && (file[end] != '\n'))
    {
        end += 1ull;
    }
    const unsigned long long cut = ((end > *start) && (file[end - 1ull] == '\r')) ? end - 1ull : end;
    const int there = (*at == line) && (*start < size);
    cli_json_quoted(out, (size > 0ull) ? file + *start : "", there ? cut - *start : 0ull);
}

int cli_servers_references(const char *tree, const char *path, unsigned int line, unsigned int col, VerbaLinea *out,
                           VerbaLinea *err_msg)
{
    // a file given its server as a slice is answered from the tree alone, a name it knows no definition of with none
    if (cli_servers_sliced(path))
    {
        const int tree_found = cli_servers_usages_written(tree, path, line, col, out, err_msg);
        mmgr_line_put(out, (tree_found == 0) ? "[]" : "");
        return tree_found >= 0;
    }
    CliLsp *const server = request_server(path, err_msg);
    if (server == NULL)
    {
        return 0;
    }
    cli_servers_reading_locked(&s_search);
    int requested = 0;
    const unsigned int all =
        request_read(server, &s_search, "textDocument/references", path, line, col,
                     "\"context\":{\"includeDeclaration\":true}", CLI_SERVERS_SEARCHING, &requested, err_msg);
    if (requested)
    {
        const unsigned int count = request_usages_of(all);
        // the file of the usages written, mapped where it lies while they are, NULL where it is not read whole
        const unsigned char *mapped = NULL;
        unsigned long long mapped_size = 0ull;
        unsigned long long size = 0ull;
        unsigned int at_line = 0u;
        unsigned long long start = 0ull;
        mmgr_line_ch(out, '[');
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
                if (mapped != NULL)
                {
                    cli_file_let_go(mapped, mapped_size);
                }
                mapped = cli_file_read(file, &mapped_size);
                size = ((mapped != NULL) && cli_utf8_whole((const char *)mapped, mapped_size)) ? mapped_size : 0ull;
                at_line = 0u;
                start = 0ull;
            }
            mmgr_line_put(out, (one > 0u) ? ",{\"path\":" : "{\"path\":");
            char shown[CLI_PATH_MAX_LEN];
            VerbaLinea where = {shown, sizeof(shown), 0ull};
            cli_servers_tree_path(tree, file, &where);
            cli_json_quoted_lossy(out, (const unsigned char *)shown, mmgr_line_ok(&where) ? where.at : 0ull);
            mmgr_line_put(out, ",\"from\":");
            cli_text_place_written(usage->from, out);
            mmgr_line_put(out, ",\"to\":");
            cli_text_place_written(usage->to, out);
            mmgr_line_put(out, ",\"text\":");
            request_line_written((const char *)mapped, size, usage->from.line, &at_line, &start, out);
            mmgr_line_ch(out, '}');
        }
        mmgr_line_ch(out, ']');
        if (mapped != NULL)
        {
            cli_file_let_go(mapped, mapped_size);
        }
        request_usages_given();
    }
    cli_servers_reading_released(&s_search);
    return requested;
}

int cli_servers_renamable(const char *path, unsigned int line, unsigned int col, VerbaLinea *out, VerbaLinea *err_msg)
{
    // a file given its server as a slice is answered from the tree alone, a name it knows no definition of with none
    char tree[CLI_PATH_MAX_LEN];
    cli_lock_take(&g_cli_servers.lock);
    memor.cpy(&(MemoriaCfg){.dst = tree, .src = g_cli_servers.checked_root, .bytes = sizeof(tree)});
    cli_lock_give(&g_cli_servers.lock);
    if (cli_servers_sliced(path))
    {
        const int tree_found = cli_servers_renamable_written(tree, path, line, col, out, err_msg);
        mmgr_line_put(out, (tree_found == 0) ? "null" : "");
        return tree_found >= 0;
    }
    CliLsp *const server = request_server(path, err_msg);
    if (server == NULL)
    {
        return 0;
    }
    if (!cli_servers_capable_true(server, "renameProvider", "prepareProvider"))
    {
        cli_lsp_unref(server);
        mmgr_line_put(out, "null");
        return 1;
    }
    cli_servers_reading_locked(&s_quick);
    int requested = 0;
    const unsigned int root = request_read(server, &s_quick, "textDocument/prepareRename", path, line, col, NULL,
                                           CLI_SERVERS_REQUEST_TIMEOUT_MS, &requested, err_msg);
    const CliJson *const json = &s_quick.json;
    const unsigned int named = cli_json_member(json, root, "range");
    const unsigned int range = cli_json_is(json, named, CFG_JSON_OBJECT) ? named : root;
    const unsigned int start = cli_json_member(json, range, "start");
    const CliTextPlace from = cli_servers_place(json, start);
    const CliTextPlace to = cli_servers_place(json, cli_json_member(json, range, "end"));
    if (requested && !cli_json_is(json, start, CFG_JSON_OBJECT))
    {
        if (cli_json_is(json, root, CFG_JSON_NULL))
        {
            mmgr_line_put(out, "null");
        }
        else
        {
            mmgr_line_put(err_msg, "nothing here can be renamed");
            requested = 0;
        }
    }
    else if (requested)
    {
        mmgr_line_put(out, "{\"from\":");
        cli_text_place_written(from, out);
        mmgr_line_put(out, ",\"to\":");
        cli_text_place_written(to, out);
        unsigned long long size = 0ull;
        const char *const name = cli_json_string(json, cli_json_member(json, root, "placeholder"), &size);
        mmgr_line_put(out, ",\"name\":");
        if (name != NULL)
        {
            cli_json_quoted(out, name, size);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
        mmgr_line_ch(out, '}');
    }
    cli_servers_reading_released(&s_quick);
    return requested;
}

int cli_servers_rename(const char *tree, const char *path, unsigned int line, unsigned int col, const char *name,
                       VerbaLinea *out, VerbaLinea *err_msg)
{
    // a file given its server as a slice is renamed across the tree alone, a name it knows no definition of not at all
    if (cli_servers_sliced(path))
    {
        const int tree_found = cli_servers_rename_written(tree, path, line, col, name, out, err_msg);
        if (tree_found == 0)
        {
            mmgr_line_put(err_msg, "the tree knows no definition of the name to rename");
        }
        return tree_found == 1;
    }
    CliLsp *const server = request_server(path, err_msg);
    if (server == NULL)
    {
        return 0;
    }
    cli_servers_reading_locked(&s_search);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    VerbaLinea added;
    cli_pieces_begun(&pieces, &added, piece, sizeof(piece), &g_cli_servers_cells);
    mmgr_line_put(&added, "\"newName\":");
    cli_json_quoted(&added, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    unsigned long long added_size = 0ull;
    char *const named = cli_pieces_joined(&pieces, &added, "a rename's new name", &added_size);
    int requested = 0;
    unsigned int root = CLI_JSON_NONE;
    if (named != NULL)
    {
        root = request_read(server, &s_search, "textDocument/rename", path, line, col, named, CLI_SERVERS_SEARCHING,
                            &requested, err_msg);
    }
    else
    {
        cli_lsp_unref(server);
        mmgr_line_put(err_msg, "the rename could not be written: the cells cannot meet it");
    }
    cli_cell_release(named, &g_cli_servers_cells);
    if (requested && cli_json_is(&s_search.json, root, CFG_JSON_NULL))
    {
        mmgr_line_put(err_msg, "the server found nothing to rename");
        requested = 0;
    }
    if (requested)
    {
        cli_servers_workspace_edit_of(&s_search.json, root, tree, out);
    }
    cli_servers_reading_released(&s_search);
    return requested;
}

int cli_servers_signature(const char *path, unsigned int line, unsigned int col, VerbaLinea *out, VerbaLinea *err_msg)
{
    CliLsp *const server = cli_servers_owner_answering(path);
    if (server == NULL)
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    cli_servers_reading_locked(&s_quick);
    int requested = 0;
    const unsigned int root = request_read(server, &s_quick, "textDocument/signatureHelp", path, line, col, NULL,
                                           CLI_SERVERS_REQUEST_TIMEOUT_MS, &requested, err_msg);
    if (requested)
    {
        cli_servers_signature_of(&s_quick.json, root, out);
    }
    cli_servers_reading_released(&s_quick);
    return requested;
}
