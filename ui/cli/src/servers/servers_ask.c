// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_ask.c: what the server of a file answers about a place in it: a hover, the definitions, the completions,
// orior's own reading's of a build file or JSON first, every usage of a symbol with its line's text read from its file
// once, whether the symbol can be renamed and the edits that rename it, and the signature of the call the cursor is
// in. An answer asked for while no server has the file open is none, but a usage, a rename and what it would change,
// which say there is no server to ask
#include "../builds/builds.h"
#include "../code/schema.h"
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

// the server that has `path` open, held for the caller, or NULL with why there is none said
static CliLsp *ask_asked(const char *path, CliLine *said)
{
    CliLsp *const server = cli_servers_holding(path);
    if (server == NULL)
    {
        cli_line_text(said, "no language server has this file open");
    }
    return server;
}

// `method` asked of `server` about `line`, `col` of `path`, `added` after the place, the answer read into `reading`:
// its root, or CLI_JSON_NONE with why said, where `asked` is 0
static unsigned int ask_read(CliLsp *server, CliServersReading *reading, const char *method, const char *path,
                             unsigned int line, unsigned int col, const char *added, unsigned long long wait,
                             int *asked, CliLine *said)
{
    CliLine result;
    cli_servers_answer_begun(reading, &result);
    *asked = cli_servers_asked_at(server, method, path, line, col, added, wait, &result, said);
    cli_lsp_given(server);
    return *asked ? cli_servers_answer_read(reading, &result) : CLI_JSON_NONE;
}

int cli_servers_hover(const char *path, unsigned int line, unsigned int col, CliLine *out, CliLine *said)
{
    CliLsp *const server = cli_servers_holding(path);
    if (server == NULL)
    {
        cli_line_text(out, "null");
        return 1;
    }
    cli_servers_reading_held(&s_quick);
    int asked = 0;
    const unsigned int root =
        ask_read(server, &s_quick, "textDocument/hover", path, line, col, NULL, CLI_SERVERS_ASKING, &asked, said);
    if (asked)
    {
        char piece[CLI_SERVERS_PIECE];
        CliPieces pieces;
        CliLine markdown;
        cli_pieces_begun(&pieces, &markdown, piece, sizeof(piece));
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
            cli_line_text(out, "null");
        }
        cli_cell_given(shown);
    }
    cli_servers_reading_released(&s_quick);
    return asked;
}

int cli_servers_definition(const char *tree, const char *path, unsigned int line, unsigned int col, CliLine *out,
                           CliLine *said)
{
    CliLsp *const server = cli_servers_holding(path);
    if (server == NULL)
    {
        cli_line_text(out, "[]");
        return 1;
    }
    cli_servers_reading_held(&s_quick);
    int asked = 0;
    const unsigned int root =
        ask_read(server, &s_quick, "textDocument/definition", path, line, col, NULL, CLI_SERVERS_ASKING, &asked, said);
    if (asked)
    {
        const CliJson *const json = &s_quick.json;
        cli_line_character(out, '[');
        if (cli_json_is(json, root, CFG_JSON_ARRAY))
        {
            int written = 0;
            for (unsigned int one = json->node[root].first; one != CLI_JSON_NONE; one = json->node[one].next)
            {
                const unsigned long long before = out->at;
                cli_line_text(out, written ? "," : "");
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
        cli_line_character(out, ']');
    }
    cli_servers_reading_released(&s_quick);
    return asked;
}

// the completions written as an array
static void ask_items_written(CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < s_items.count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_completion_written(&s_items, at, out);
    }
    cli_line_character(out, ']');
}

int cli_servers_complete(const char *path, unsigned int line, unsigned int col, CliLine *out, CliLine *said)
{
    cli_servers_reading_held(&s_quick);
    cli_completions_begun(&s_items);
    if (cli_servers_own_reads(path))
    {
        char root[CLI_PATH_LONGEST];
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
        cli_cell_given(text);
    }
    CliLsp *const server = cli_servers_holding(path);
    int asked = 1;
    if (server != NULL)
    {
        const unsigned long long before = said->at;
        const unsigned int root = ask_read(server, &s_quick, "textDocument/completion", path, line, col,
                                           "\"context\":{\"triggerKind\":1}", CLI_SERVERS_ASKING, &asked, said);
        if (!asked && (s_items.count > 0u))
        {
            said->at = before;
            asked = 1;
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
    if (asked)
    {
        ask_items_written(out);
    }
    cli_servers_reading_released(&s_quick);
    return asked;
}

// two usages by their path, then where they start, then the order they were given in
static int ask_usage_order(const void *one, const void *other)
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

// the path of the usage `one` of the answer read into `path`: the bytes it takes with the 0 after it, or 0 where it
// names none
static unsigned long long ask_usage_path(unsigned int one, char *path)
{
    const CliJson *const json = &s_search.json;
    unsigned long long length = 0ull;
    const char *const uri = cli_json_string(json, cli_json_member(json, one, "uri"), &length);
    return ((uri != NULL) && cli_lsp_path_of(uri, length, path))
               ? cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1ull
               : 0ull;
}

// the usages of the answer read, and their paths, each in a cell of their size: their count, 0 where none are or the
// cells cannot meet them; the cells given back with the search
static unsigned int ask_usages_of(unsigned int all)
{
    const CliJson *const json = &s_search.json;
    const unsigned int first = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].first : CLI_JSON_NONE;
    char path[CLI_PATH_LONGEST];
    unsigned long long paths = 0ull;
    for (unsigned int one = first; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        paths += ask_usage_path(one, path);
    }
    s_usage = (paths > 0ull) ? cli_cell_taken((unsigned long long)json->node[all].count * sizeof(CliServersUsage),
                                              "the usages of a symbol")
                             : NULL;
    s_usage_paths = (s_usage != NULL) ? cli_cell_taken(paths, "the paths of a symbol's usages") : NULL;
    unsigned int count = 0u;
    unsigned int used = 0u;
    for (unsigned int one = (s_usage_paths != NULL) ? first : CLI_JSON_NONE; one != CLI_JSON_NONE;
         one = json->node[one].next)
    {
        const unsigned long long size = ask_usage_path(one, path);
        if (size == 0ull)
        {
            continue;
        }
        // a path's bytes are under CLI_PATH_LONGEST
        memor.cpy(&(MemoriaCfg){.dst = s_usage_paths + used, .src = path, .bytes = (size_t)size});
        const unsigned int range = cli_json_member(json, one, "range");
        s_usage[count] = (CliServersUsage){used, cli_servers_place(json, cli_json_member(json, range, "start")),
                                           cli_servers_place(json, cli_json_member(json, range, "end")), count};
        used += (unsigned int)size;
        count += 1u;
    }
    if (count > 0u)
    {
        qsort(s_usage, count, sizeof(s_usage[0]), ask_usage_order);
    }
    return count;
}

// the usages and their paths given back
static void ask_usages_given(void)
{
    cli_cell_given(s_usage);
    cli_cell_given(s_usage_paths);
    s_usage = NULL;
    s_usage_paths = NULL;
}

// the text of the line `line` of the `size` bytes of `file`, its "\r" before its end left off, written as a JSON
// string, the line `*at` starts at byte `*start` walked forward from there
static void ask_line_written(const char *file, unsigned long long size, unsigned int line, unsigned int *at,
                             unsigned long long *start, CliLine *out)
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

int cli_servers_references(const char *tree, const char *path, unsigned int line, unsigned int col, CliLine *out,
                           CliLine *said)
{
    CliLsp *const server = ask_asked(path, said);
    if (server == NULL)
    {
        return 0;
    }
    cli_servers_reading_held(&s_search);
    int asked = 0;
    const unsigned int all = ask_read(server, &s_search, "textDocument/references", path, line, col,
                                      "\"context\":{\"includeDeclaration\":true}", CLI_SERVERS_SEARCHING, &asked, said);
    if (asked)
    {
        const unsigned int count = ask_usages_of(all);
        // the file of the usages written, mapped where it lies while they are, NULL where it is not read whole
        const unsigned char *mapped = NULL;
        unsigned long long mapped_size = 0ull;
        unsigned long long size = 0ull;
        unsigned int at_line = 0u;
        unsigned long long start = 0ull;
        cli_line_character(out, '[');
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
                    qry_member_file_unmapped(mapped, mapped_size);
                }
                mapped = qry_member_file_mapped(file, &mapped_size);
                size = ((mapped != NULL) && cli_utf8_whole((const char *)mapped, mapped_size)) ? mapped_size : 0ull;
                at_line = 0u;
                start = 0ull;
            }
            cli_line_text(out, (one > 0u) ? ",{\"path\":" : "{\"path\":");
            char shown[CLI_PATH_LONGEST];
            CliLine where = {shown, sizeof(shown), 0ull};
            cli_servers_tree_path(tree, file, &where);
            cli_json_quoted_lossy(out, (const unsigned char *)shown, cli_line_fits(&where) ? where.at : 0ull);
            cli_line_text(out, ",\"from\":");
            cli_text_place_written(usage->from, out);
            cli_line_text(out, ",\"to\":");
            cli_text_place_written(usage->to, out);
            cli_line_text(out, ",\"text\":");
            ask_line_written((const char *)mapped, size, usage->from.line, &at_line, &start, out);
            cli_line_character(out, '}');
        }
        cli_line_character(out, ']');
        if (mapped != NULL)
        {
            qry_member_file_unmapped(mapped, mapped_size);
        }
        ask_usages_given();
    }
    cli_servers_reading_released(&s_search);
    return asked;
}

int cli_servers_renamable(const char *path, unsigned int line, unsigned int col, CliLine *out, CliLine *said)
{
    CliLsp *const server = ask_asked(path, said);
    if (server == NULL)
    {
        return 0;
    }
    if (!cli_servers_capable_true(server, "renameProvider", "prepareProvider"))
    {
        cli_lsp_given(server);
        cli_line_text(out, "null");
        return 1;
    }
    cli_servers_reading_held(&s_quick);
    int asked = 0;
    const unsigned int root = ask_read(server, &s_quick, "textDocument/prepareRename", path, line, col, NULL,
                                       CLI_SERVERS_ASKING, &asked, said);
    const CliJson *const json = &s_quick.json;
    const unsigned int named = cli_json_member(json, root, "range");
    const unsigned int range = cli_json_is(json, named, CFG_JSON_OBJECT) ? named : root;
    const unsigned int start = cli_json_member(json, range, "start");
    if (asked && !cli_json_is(json, start, CFG_JSON_OBJECT))
    {
        if (cli_json_is(json, root, CFG_JSON_NULL))
        {
            cli_line_text(out, "null");
        }
        else
        {
            cli_line_text(said, "nothing here can be renamed");
            asked = 0;
        }
    }
    else if (asked)
    {
        cli_line_text(out, "{\"from\":");
        cli_text_place_written(cli_servers_place(json, start), out);
        cli_line_text(out, ",\"to\":");
        cli_text_place_written(cli_servers_place(json, cli_json_member(json, range, "end")), out);
        unsigned long long size = 0ull;
        const char *const name = cli_json_string(json, cli_json_member(json, root, "placeholder"), &size);
        cli_line_text(out, ",\"name\":");
        if (name != NULL)
        {
            cli_json_quoted(out, name, size);
        }
        else
        {
            cli_line_text(out, "null");
        }
        cli_line_character(out, '}');
    }
    cli_servers_reading_released(&s_quick);
    return asked;
}

int cli_servers_rename(const char *tree, const char *path, unsigned int line, unsigned int col, const char *name,
                       CliLine *out, CliLine *said)
{
    CliLsp *const server = ask_asked(path, said);
    if (server == NULL)
    {
        return 0;
    }
    cli_servers_reading_held(&s_search);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine added;
    cli_pieces_begun(&pieces, &added, piece, sizeof(piece));
    cli_line_text(&added, "\"newName\":");
    cli_json_quoted(&added, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    unsigned long long added_size = 0ull;
    char *const named = cli_pieces_joined(&pieces, &added, "a rename's new name", &added_size);
    int asked = 0;
    unsigned int root = CLI_JSON_NONE;
    if (named != NULL)
    {
        root = ask_read(server, &s_search, "textDocument/rename", path, line, col, named, CLI_SERVERS_SEARCHING, &asked,
                        said);
    }
    else
    {
        cli_lsp_given(server);
        cli_line_text(said, "the rename could not be written: the cells cannot meet it");
    }
    cli_cell_given(named);
    if (asked && cli_json_is(&s_search.json, root, CFG_JSON_NULL))
    {
        cli_line_text(said, "the server found nothing to rename");
        asked = 0;
    }
    if (asked)
    {
        cli_servers_workspace_edit_of(&s_search.json, root, tree, out);
    }
    cli_servers_reading_released(&s_search);
    return asked;
}

int cli_servers_signature(const char *path, unsigned int line, unsigned int col, CliLine *out, CliLine *said)
{
    CliLsp *const server = cli_servers_holding(path);
    if (server == NULL)
    {
        cli_line_text(out, "null");
        return 1;
    }
    cli_servers_reading_held(&s_quick);
    int asked = 0;
    const unsigned int root = ask_read(server, &s_quick, "textDocument/signatureHelp", path, line, col, NULL,
                                       CLI_SERVERS_ASKING, &asked, said);
    if (asked)
    {
        cli_servers_signature_of(&s_quick.json, root, out);
    }
    cli_servers_reading_released(&s_quick);
    return asked;
}
