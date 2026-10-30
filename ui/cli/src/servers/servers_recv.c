// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_recv.c: what a server sends unprompted, and the answer where it requests: a file's diagnostics stored and
// passed on, a sliced file's moved to its own lines first, unless they come empty for a file the tree's check lets go,
// are of a slice it was given before its last, or are a window's of a file the check gathers them for; an edit it
// requests passed on and marked to be
// made; each setting it requests left to its own default but Python's; and its hints and its tokens requested again.
//
// Python's settings name the tree's environment's Python, where the tree has one, and, as its analysis, the folders
// the tree's scripts put on `sys.path` as they run, for their imports to be found. Pyright requests the analysis
// within `python`, and other servers for `python.analysis` alone. A setting the analysis leaves out is taken as off:
// the ones on by default are given
#include "../code/python_paths.h"
#include "../toolchains/toolchains.h"
#include "servers_internal.h"

// the most folders the analysis is given
#define CLI_SERVERS_EXTRA_PATHS 256u

// what is received read, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};

// whether `length` bytes of JSON contain an array with no items
static int recv_empty_array(const char *text, unsigned long long length)
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

// a file's diagnostics stored, by its key with its URI, and passed on whole
static void recv_diagnostics(const CliServersSink *sink, const char *params, unsigned long long size)
{
    unsigned long long uri_at = 0ull;
    unsigned long long uri_length = 0ull;
    unsigned long long all_at = 0ull;
    unsigned long long all_length = 0ull;
    char uri[CLI_PATH_MAX_LEN];
    if (!cli_lsp_member(params, size, "uri", &uri_at, &uri_length) ||
        !cli_lsp_member(params, size, "diagnostics", &all_at, &all_length) ||
        !cli_lsp_string(params + uri_at, uri_length, uri, sizeof(uri)))
    {
        return;
    }
    const char *all = params + all_at;
    const unsigned long long lead = cli_text_lead(all, all_length);
    if ((lead == all_length) || (all[lead] != '['))
    {
        return;
    }
    // a sliced file's diagnostics are of its slice, and stand on the file's own lines once moved there
    const unsigned long long uri_size = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL});
    char path[CLI_PATH_MAX_LEN];
    const int sliced = cli_lsp_path_of(uri, uri_size, path) && cli_servers_sliced(path);
    char *const mapped =
        sliced ? cli_servers_slice_diagnostics(path, params, size, all, all_length, &all_length, &g_cli_servers_cells)
               : NULL;
    if (sliced && (mapped == NULL))
    {
        return;
    }
    all = sliced ? mapped : all;
    char key[CLI_PATH_MAX_LEN];
    cli_lsp_key_of(uri, uri_size, key);
    if ((recv_empty_array(all, all_length) && cli_servers_letting_go(key)) ||
        cli_servers_gathered(key, all, all_length))
    {
        g_cli_cells->persistent_buf_release(mapped, &g_cli_servers_cells);
        return;
    }
    cli_servers_woken(key);
    cli_lock_take(&g_cli_servers_published_lock);
    (void)cli_keyed_put(&g_cli_servers_published, key, uri, all, all_length);
    cli_lock_give(&g_cli_servers_published_lock);
    g_cli_cells->persistent_buf_release(mapped, &g_cli_servers_cells);
    cli_servers_emit_whole(key, uri, sink);
}

// an edit the server requests passed on as each file's edits
static void recv_edit(const CliServersSink *sink, const char *params, unsigned long long size)
{
    cli_servers_reading_locked(&s_reading);
    const unsigned int root = cli_servers_read(&s_reading, params, size);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    VerbaLinea edits_line;
    cli_pieces_begun(&pieces, &edits_line, piece, sizeof(piece), &g_cli_servers_cells);
    cli_servers_workspace_edit_of(&s_reading.json, cli_json_member(&s_reading.json, root, "edit"), sink->tree,
                                  &edits_line);
    unsigned long long edits_size = 0ull;
    char *const edits = cli_pieces_joined(&pieces, &edits_line, "the edits a server requests", &edits_size);
    if ((edits != NULL) && (sink->emit != NULL))
    {
        sink->emit(sink->context, "lsp-edits", edits, edits_size);
    }
    g_cli_cells->persistent_buf_release(edits, &g_cli_servers_cells);
    cli_servers_reading_released(&s_reading);
}

// One folder of the analysis written: where it is written and whether one is
typedef struct
{
    VerbaLinea *answer;
    int written;
} CliRecvPaths;

// a folder the tree's scripts put on `sys.path` written as a JSON string after those written
static void recv_path(void *context, const char *path)
{
    CliRecvPaths *const paths = context;
    mmgr_line_put(paths->answer, paths->written ? "," : "");
    cli_json_quoted_lossy(paths->answer, (const unsigned char *)path,
                          cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    paths->written = 1;
}

// Python's analysis for the item `item` requests of: the folders the scripts of its tree put on `sys.path`
static void recv_analysis(const CliJson *json, unsigned int item, VerbaLinea *answer)
{
    unsigned long long length = 0ull;
    const char *const scope = cli_json_string(json, cli_json_member(json, item, "scopeUri"), &length);
    char root[CLI_PATH_MAX_LEN];
    mmgr_line_put(answer, "{\"autoImportCompletions\":true,\"autoSearchPaths\":true,\"extraPaths\":[");
    CliRecvPaths paths = {answer, 0};
    if ((scope != NULL) && cli_lsp_path_of(scope, length, root))
    {
        (void)cli_python_paths(root, CLI_SERVERS_EXTRA_PATHS, recv_path, &paths);
    }
    mmgr_line_put(answer, "],\"useLibraryCodeForTypes\":true}");
}

// each setting the server requests: Python's and its analysis given, every other left to its own default
static void recv_configuration(const char *params, unsigned long long size, VerbaLinea *answer)
{
    cli_servers_reading_locked(&s_reading);
    const CliJson *const json = &s_reading.json;
    const unsigned int items = cli_json_member(json, cli_servers_read(&s_reading, params, size), "items");
    static CliTreeEnvironment s_environment;
    const int python = cli_toolchains_environment(&s_environment) && s_environment.has_python;
    mmgr_line_ch(answer, '[');
    int first = 1;
    for (unsigned int item = cli_json_is(json, items, CFG_JSON_ARRAY) ? json->node[items].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        mmgr_line_put(answer, first ? "" : ",");
        first = 0;
        unsigned long long length = 0ull;
        const char *const section = cli_json_string(json, cli_json_member(json, item, "section"), &length);
        if ((section != NULL) && (length == 6ull) &&
            (memor.cmp(&(MemoriaCfg){.src = section, .other = "python", .bytes = 6u}) == 0))
        {
            mmgr_line_put(answer, "{\"analysis\":");
            recv_analysis(json, item, answer);
            if (python)
            {
                mmgr_line_put(answer, ",\"defaultInterpreterPath\":");
                cli_json_quoted_lossy(
                    answer, (const unsigned char *)s_environment.python,
                    cellul.len(&(CatenaFinitaCfg){.src = s_environment.python, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(answer, ",\"pythonPath\":");
                cli_json_quoted_lossy(
                    answer, (const unsigned char *)s_environment.python,
                    cellul.len(&(CatenaFinitaCfg){.src = s_environment.python, .cap = CLI_CELLS_POOL}));
            }
            mmgr_line_ch(answer, '}');
        }
        else if ((section != NULL) && (length == 15ull) &&
                 (memor.cmp(&(MemoriaCfg){.src = section, .other = "python.analysis", .bytes = 15u}) == 0))
        {
            recv_analysis(json, item, answer);
        }
        else
        {
            mmgr_line_put(answer, "null");
        }
    }
    mmgr_line_ch(answer, ']');
    cli_servers_reading_released(&s_reading);
}

void cli_servers_recv(void *context, const char *method, const char *params, unsigned long long size,
                      VerbaLinea *answer)
{
    const CliServersSink *const sink = context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "textDocument/publishDiagnostics", .cap = CLI_CELLS_POOL}))
    {
        recv_diagnostics(sink, params, size);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "workspace/applyEdit", .cap = CLI_CELLS_POOL}))
    {
        recv_edit(sink, params, size);
        mmgr_line_put(answer, "{\"applied\":true}");
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "workspace/configuration", .cap = CLI_CELLS_POOL}))
    {
        recv_configuration(params, size, answer);
    }
    else if ((cellul.eq(
                 &(CatenaFinitaCfg){.src = method, .other = "workspace/inlayHint/refresh", .cap = CLI_CELLS_POOL})) &&
             (sink->emit != NULL))
    {
        sink->emit(sink->context, "lsp-hints", "null", 4ull);
    }
    else if (cellul.eq(
                 &(CatenaFinitaCfg){.src = method, .other = "workspace/semanticTokens/refresh", .cap = CLI_CELLS_POOL}))
    {
        cli_servers_forget_tokens();
        if (sink->emit != NULL)
        {
            sink->emit(sink->context, "lsp-parse", "null", 4ull);
        }
    }
}
