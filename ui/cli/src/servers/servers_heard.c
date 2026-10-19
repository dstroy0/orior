// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_heard.c: what a server says on its own, and the answer where it asks: a file's diagnostics kept and passed
// on, unless they come empty for a file the tree's check lets go; an edit it asks for passed on and said to be made;
// each setting it asks for left to its own default but Python's; and its hints and its tokens asked for again.
//
// Python's settings name the tree's environment's Python, where the tree has one, and, as its analysis, the folders
// the tree's scripts put on `sys.path` as they run, for their imports to be found. Pyright asks for the analysis
// within `python`, and other servers for `python.analysis` alone. A setting the analysis leaves out is taken as off:
// the ones on by default are given
#include "../code/python_paths.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

// the most folders the analysis is given
#define CLI_SERVERS_EXTRA_PATHS 256u

// what is heard read, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};

// whether `length` bytes of JSON hold an array with no items
static int heard_empty_array(const char *text, unsigned long long length)
{
    unsigned long long at = cli_text_lead(text, length);
    if ((at == length) || (text[at] != '['))
    {
        return 0;
    }
    at += 1ull;
    at += cli_text_lead(text + at, length - at);
    return (at < length) && (text[at] == ']');
}

// a file's diagnostics kept, by its key with its URI, and passed on whole
static void heard_diagnostics(const CliServersEmit *emit, const char *params, unsigned long long size)
{
    unsigned long long uri_at = 0ull;
    unsigned long long uri_length = 0ull;
    unsigned long long all_at = 0ull;
    unsigned long long all_length = 0ull;
    char uri[CLI_PATH_LONGEST];
    if (!cli_lsp_member(params, size, "uri", &uri_at, &uri_length) ||
        !cli_lsp_member(params, size, "diagnostics", &all_at, &all_length) ||
        !cli_lsp_string(params + uri_at, uri_length, uri, sizeof(uri)))
    {
        return;
    }
    const char *const all = params + all_at;
    const unsigned long long lead = cli_text_lead(all, all_length);
    if ((lead == all_length) || (all[lead] != '['))
    {
        return;
    }
    char key[CLI_PATH_LONGEST];
    cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
    if (heard_empty_array(all, all_length) && cli_servers_letting_go(key))
    {
        return;
    }
    cli_servers_woken(key);
    cli_lock_take(&g_cli_servers_published_lock);
    (void)cli_keyed_put(&g_cli_servers_published, key, uri, all, all_length);
    cli_lock_give(&g_cli_servers_published_lock);
    cli_servers_emit_whole(key, uri, emit);
}

// an edit the server asks for passed on as each file's edits
static void heard_edit(const CliServersEmit *emit, const char *params, unsigned long long size)
{
    cli_servers_reading_held(&s_reading);
    const unsigned int root = cli_servers_read(&s_reading, params, size);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine told;
    cli_pieces_begun(&pieces, &told, piece, sizeof(piece));
    cli_servers_workspace_edit_of(&s_reading.json, cli_json_member(&s_reading.json, root, "edit"), emit->tree, &told);
    unsigned long long told_size = 0ull;
    char *const edits = cli_pieces_joined(&pieces, &told, "the edits a server asks for", &told_size);
    if ((edits != NULL) && (emit->told != NULL))
    {
        emit->told(emit->context, "lsp-edits", edits, told_size);
    }
    cli_cell_given(edits);
    cli_servers_reading_released(&s_reading);
}

// One folder of the analysis written: where it is written and whether one is
typedef struct
{
    CliLine *answer;
    int written;
} CliHeardPaths;

// a folder the tree's scripts put on `sys.path` written as a JSON string after those written
static void heard_path(void *context, const char *path)
{
    CliHeardPaths *const paths = context;
    cli_line_text(paths->answer, paths->written ? "," : "");
    cli_json_quoted_lossy(paths->answer, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    paths->written = 1;
}

// Python's analysis for the item `item` asks of: the folders the scripts of its tree put on `sys.path`
static void heard_analysis(const CliJson *json, unsigned int item, CliLine *answer)
{
    unsigned long long length = 0ull;
    const char *const scope = cli_json_string(json, cli_json_member(json, item, "scopeUri"), &length);
    char root[CLI_PATH_LONGEST];
    cli_line_text(answer, "{\"autoImportCompletions\":true,\"autoSearchPaths\":true,\"extraPaths\":[");
    CliHeardPaths paths = {answer, 0};
    if ((scope != NULL) && cli_lsp_path_of(scope, length, root))
    {
        (void)cli_python_paths(root, CLI_SERVERS_EXTRA_PATHS, heard_path, &paths);
    }
    cli_line_text(answer, "],\"useLibraryCodeForTypes\":true}");
}

// each setting the server asks for: Python's and its analysis given, every other left to its own default
static void heard_configuration(const char *params, unsigned long long size, CliLine *answer)
{
    cli_servers_reading_held(&s_reading);
    const CliJson *const json = &s_reading.json;
    const unsigned int items = cli_json_member(json, cli_servers_read(&s_reading, params, size), "items");
    static CliTreeEnvironment s_environment;
    const int python = cli_toolchains_environment(&s_environment) && s_environment.has_python;
    cli_line_character(answer, '[');
    int first = 1;
    for (unsigned int item = cli_json_is(json, items, CFG_JSON_ARRAY) ? json->node[items].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        cli_line_text(answer, first ? "" : ",");
        first = 0;
        unsigned long long length = 0ull;
        const char *const section = cli_json_string(json, cli_json_member(json, item, "section"), &length);
        if ((section != NULL) && (length == 6ull) &&
            (memor.cmp(&(MemoriaCfg){.src = section, .other = "python", .bytes = 6u}) == 0))
        {
            cli_line_text(answer, "{\"analysis\":");
            heard_analysis(json, item, answer);
            if (python)
            {
                cli_line_text(answer, ",\"defaultInterpreterPath\":");
                cli_json_quoted_lossy(
                    answer, (const unsigned char *)s_environment.python,
                    cellul.len(&(CatenaFinitaCfg){.src = s_environment.python, .cap = CLI_CELLS_POOL}));
                cli_line_text(answer, ",\"pythonPath\":");
                cli_json_quoted_lossy(
                    answer, (const unsigned char *)s_environment.python,
                    cellul.len(&(CatenaFinitaCfg){.src = s_environment.python, .cap = CLI_CELLS_POOL}));
            }
            cli_line_character(answer, '}');
        }
        else if ((section != NULL) && (length == 15ull) &&
                 (memor.cmp(&(MemoriaCfg){.src = section, .other = "python.analysis", .bytes = 15u}) == 0))
        {
            heard_analysis(json, item, answer);
        }
        else
        {
            cli_line_text(answer, "null");
        }
    }
    cli_line_character(answer, ']');
    cli_servers_reading_released(&s_reading);
}

void cli_servers_heard(void *context, const char *method, const char *params, unsigned long long size, CliLine *answer)
{
    const CliServersEmit *const emit = context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "textDocument/publishDiagnostics", .cap = CLI_CELLS_POOL}))
    {
        heard_diagnostics(emit, params, size);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "workspace/applyEdit", .cap = CLI_CELLS_POOL}))
    {
        heard_edit(emit, params, size);
        cli_line_text(answer, "{\"applied\":true}");
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "workspace/configuration", .cap = CLI_CELLS_POOL}))
    {
        heard_configuration(params, size, answer);
    }
    else if ((cellul.eq(
                 &(CatenaFinitaCfg){.src = method, .other = "workspace/inlayHint/refresh", .cap = CLI_CELLS_POOL})) &&
             (emit->told != NULL))
    {
        emit->told(emit->context, "lsp-hints", "null", 4ull);
    }
    else if (cellul.eq(
                 &(CatenaFinitaCfg){.src = method, .other = "workspace/semanticTokens/refresh", .cap = CLI_CELLS_POOL}))
    {
        cli_servers_forget_tokens();
        if (emit->told != NULL)
        {
            emit->told(emit->context, "lsp-parse", "null", 4ull);
        }
    }
}
