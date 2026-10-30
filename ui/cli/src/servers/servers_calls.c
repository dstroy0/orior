// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_calls.c: the call hierarchy: the function at a place as its top, as its server finds it where the server
// answers for calls, as the tree's leaves give it where the file's grammar names its functions and regions
// (symbols_calls.c), and as the inspections' tree finds it elsewhere; and the functions that call a function, or that
// it calls, each with the places of its calls, from where the hierarchy's top came from. A function is written with its
// name and what the server reports of it, its file and the span of its name, the file its calls stand in and where
// each starts, and the server's or the tree's own name for it, which is given back to read the level below it
#include "../code/symbols.h"
#include "../inspect/inspect_tree.h"
#include "servers_internal.h"

#include "../text/text.h"

// What a level of the tree's hierarchy is written with: the tree and the tree's root, the file the hierarchy is
// at, whether its calls come in, where it is written and whether one is
typedef struct
{
    const char *tree;
    const char *root;
    const char *file;
    int incoming;
    VerbaLinea *out;
    int written;
} CliServersCalls;

// what the server answers, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};
static CliServersReading s_item = {CLI_LOCK_INIT};

// a path written as a JSON string as it stands in `tree`
static void calls_path_written(const char *tree, const char *path, VerbaLinea *out)
{
    char shown[CLI_PATH_MAX_LEN];
    VerbaLinea where = {shown, sizeof(shown), 0ull};
    cli_servers_tree_path(tree, path, &where);
    cli_json_quoted_lossy(out, (const unsigned char *)shown, mmgr_line_ok(&where) ? where.at : 0ull);
}

// the places of calls written as an array
static void calls_sites_written(const CliTextPlace *sites, unsigned int count, VerbaLinea *out)
{
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        cli_text_place_written(sites[at], out);
    }
    mmgr_line_ch(out, ']');
}

// a function of a server's item written, its calls in `site`, the item's own file where it is NULL, at `sites`: 1, or
// 0 where the item names no file
static int calls_of(const CliJson *json, unsigned int item, const char *site, const CliTextPlace *sites,
                    unsigned int count, const char *tree, VerbaLinea *out)
{
    unsigned long long length = 0ull;
    const char *const uri = cli_json_string(json, cli_json_member(json, item, "uri"), &length);
    char path[CLI_PATH_MAX_LEN];
    if ((uri == NULL) || !cli_lsp_path_of(uri, length, path))
    {
        return 0;
    }
    const unsigned int selection = cli_json_member(json, item, "selectionRange");
    const unsigned int span =
        cli_json_is(json, selection, CFG_JSON_OBJECT) ? selection : cli_json_member(json, item, "range");
    const CliTextPlace from = cli_servers_place(json, cli_json_member(json, span, "start"));
    const CliTextPlace to = cli_servers_place(json, cli_json_member(json, span, "end"));
    const char *const calls_in = (site != NULL) ? site : path;
    unsigned long long size = 0ull;
    const char *const name = cli_json_string(json, cli_json_member(json, item, "name"), &size);
    mmgr_line_put(out, "{\"name\":");
    cli_json_quoted(out, (name != NULL) ? name : "", size);
    const char *const detail = cli_json_string(json, cli_json_member(json, item, "detail"), &size);
    mmgr_line_put(out, ",\"detail\":");
    cli_json_quoted(out, (detail != NULL) ? detail : "", (detail != NULL) ? size : 0ull);
    mmgr_line_put(out, ",\"path\":");
    calls_path_written(tree, path, out);
    mmgr_line_put(out, ",\"from\":");
    cli_text_place_written(from, out);
    mmgr_line_put(out, ",\"to\":");
    cli_text_place_written(to, out);
    mmgr_line_put(out, ",\"site\":");
    calls_path_written(tree, calls_in, out);
    mmgr_line_put(out, ",\"at\":");
    calls_sites_written(sites, count, out);
    mmgr_line_put(out, ",\"item\":");
    cli_json_write(json, item, out, 0u, 0);
    mmgr_line_ch(out, '}');
    return 1;
}

// a function of the tree written, its calls in `site` at `sites`
static void calls_tree_written(const CliServersCalls *calls, const char *file, const CliFacts *facts,
                               unsigned int function, const char *site, const CliInspectSpan *sites, unsigned int count)
{
    const CliInspectFunction *const one = &facts->function[function];
    VerbaLinea *const out = calls->out;
    char path[CLI_PATH_MAX_LEN];
    mmgr_line_put(out, "{\"name\":");
    cli_json_quoted(out, cli_inspect_text(facts->text, one->name), one->name.size);
    mmgr_line_put(out, ",\"detail\":\"\",\"path\":");
    cli_full(calls->root, file, path);
    calls_path_written(calls->tree, path, out);
    mmgr_line_put(out, ",\"from\":");
    cli_text_place_written((CliTextPlace){one->from.line, one->from.col}, out);
    mmgr_line_put(out, ",\"to\":");
    cli_text_place_written((CliTextPlace){one->to.line, one->to.col}, out);
    mmgr_line_put(out, ",\"site\":");
    cli_full(calls->root, site, path);
    calls_path_written(calls->tree, path, out);
    mmgr_line_put(out, ",\"at\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        cli_text_place_written((CliTextPlace){sites[at].from.line, sites[at].from.col}, out);
    }
    mmgr_line_put(out, "],\"item\":{\"orior\":{\"path\":");
    cli_json_quoted(out, file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"line\":");
    mmgr_line_decimal(out, one->from.line, 1u);
    mmgr_line_put(out, "}}}");
}

// a function the tree reaches written after those written
static void calls_reached(void *context, const char *path, const CliFacts *facts, unsigned int function,
                          const CliInspectSpan *sites, unsigned int count)
{
    CliServersCalls *const calls = context;
    mmgr_line_put(calls->out, calls->written ? "," : "");
    calls_tree_written(calls, path, facts, function, calls->incoming ? path : calls->file, sites, count);
    calls->written = 1;
}

// The tree's call hierarchy being written: the tree's top, the tree's file the calls stand in where they are the
// function's asked about, NULL where each stands in its own, where it is written, and whether one is
typedef struct
{
    const char *tree;
    const char *site;
    VerbaLinea *out;
    int written;
} CliServersTreeCalls;

// the text the editor holds of the tree's file at `path`, in a cell the caller gives back: NULL where it holds none
static char *calls_held(void *context, const char *path, unsigned long long *size)
{
    const CliServersTreeCalls *const calls = (const CliServersTreeCalls *)context;
    char full[CLI_PATH_MAX_LEN];
    cli_full(calls->tree, path, full);
    return cli_servers_text_taken(full, size);
}

// a function the tree's hierarchy reaches written after those written, its calls at `sites`, `count` of them, in its
// own file or the one the hierarchy was asked about, the tree's own item for it naming its file and line
static void calls_symbols_written(void *context, const char *path, unsigned int line, unsigned int from,
                                  unsigned int to, const char *name, unsigned long long length,
                                  const CliSymbolPlace *sites, unsigned int count)
{
    CliServersTreeCalls *const calls = (CliServersTreeCalls *)context;
    VerbaLinea *const out = calls->out;
    char full[CLI_PATH_MAX_LEN];
    mmgr_line_put(out, calls->written ? ",{\"name\":" : "{\"name\":");
    cli_json_quoted(out, name, length);
    mmgr_line_put(out, ",\"detail\":\"\",\"path\":");
    cli_full(calls->tree, path, full);
    calls_path_written(calls->tree, full, out);
    mmgr_line_put(out, ",\"from\":");
    cli_text_place_written((CliTextPlace){line, from}, out);
    mmgr_line_put(out, ",\"to\":");
    cli_text_place_written((CliTextPlace){line, to}, out);
    mmgr_line_put(out, ",\"site\":");
    cli_full(calls->tree, (calls->site != NULL) ? calls->site : path, full);
    calls_path_written(calls->tree, full, out);
    mmgr_line_put(out, ",\"at\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        cli_text_place_written((CliTextPlace){sites[at].line, sites[at].col}, out);
    }
    mmgr_line_put(out, "],\"item\":{\"tree\":{\"path\":");
    cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"line\":");
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_put(out, "}}}");
    calls->written = 1;
}

// the root of the tree and the path of `path` in it into `root` and `file`, each sized for CLI_PATH_MAX_LEN:
// 1, or 0 with why given
static int calls_tree_file(const char *path, char *root, char *file, VerbaLinea *err_msg)
{
    if (!cli_servers_tree_read(root))
    {
        mmgr_line_put(err_msg, "no language server answers calls here, and orior's own tree is still being read");
        return 0;
    }
    const size_t top = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    if (!cli_text_same(path, root, top) || ((path[top] != '/') && (path[top] != '\\') && (path[top] != '\0')))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, " is outside the tree");
        return 0;
    }
    const char *rest = path + top;
    rest += ((*rest == '/') || (*rest == '\\')) ? 1 : 0;
    VerbaLinea line = {file, CLI_PATH_MAX_LEN, 0ull};
    for (; *rest != '\0'; rest += 1)
    {
        mmgr_line_ch(&line, (*rest == '\\') ? '/' : *rest);
    }
    (void)mmgr_line_finish(&line);
    return 1;
}

int cli_servers_call_root(const char *tree, const char *path, unsigned int line, unsigned int col, VerbaLinea *out,
                          VerbaLinea *err_msg)
{
    CliLsp *const server = cli_servers_owner_answering(path);
    if ((server != NULL) && cli_servers_capable(server, "callHierarchyProvider"))
    {
        cli_servers_reading_locked(&s_reading);
        VerbaLinea result;
        cli_servers_answer_begun(&s_reading, &result);
        const int requested = cli_servers_request_at(server, "textDocument/prepareCallHierarchy", path, line, col, NULL,
                                                     CLI_SERVERS_REQUEST_TIMEOUT_MS, &result, err_msg);
        cli_lsp_unref(server);
        if (requested)
        {
            const CliJson *const json = &s_reading.json;
            const unsigned int first = cli_json_item(json, cli_servers_answer_read(&s_reading, &result), 0u);
            if ((first == CLI_JSON_NONE) || !calls_of(json, first, NULL, NULL, 0u, tree, out))
            {
                mmgr_line_put(out, "null");
            }
        }
        cli_servers_reading_released(&s_reading);
        return requested;
    }
    if (server != NULL)
    {
        cli_lsp_unref(server);
    }
    // the function the tree's leaves give at the place, where the file's grammar names its functions and regions
    char own[CLI_PATH_MAX_LEN];
    VerbaLinea own_line = {own, sizeof(own), 0ull};
    cli_servers_tree_path(tree, path, &own_line);
    if (mmgr_line_finish(&own_line) != 0ull)
    {
        CliServersTreeCalls symbols = {tree, NULL, out, 0};
        const int found =
            cli_symbols_function_at(own, line, calls_held, &symbols, calls_symbols_written, &symbols, err_msg);
        if (found != 0)
        {
            return found > 0;
        }
    }
    char root[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    if (!calls_tree_file(path, root, file, err_msg))
    {
        return 0;
    }
    unsigned int at = 0u;
    CliServersCalls calls = {tree, root, file, 0, out, 0};
    const int found =
        (cli_inspect_tree_function_at(file, line, &at) || cli_inspect_tree_function_on(file, line, &at)) &&
        cli_inspect_tree_function(file, at, calls_reached, &calls);
    if (!found)
    {
        mmgr_line_put(out, "null");
    }
    return 1;
}

int cli_servers_calls(const char *tree, const char *path, const char *item, unsigned long long size, int incoming,
                      VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_servers_reading_locked(&s_item);
    const CliJson *const json = &s_item.json;
    const unsigned int given = cli_servers_read(&s_item, item, size);
    const unsigned int leaves = cli_json_member(json, given, "tree");
    if (leaves != CLI_JSON_NONE)
    {
        char file[CLI_PATH_MAX_LEN];
        unsigned long long length = 0ull;
        const char *const named = cli_json_string(json, cli_json_member(json, leaves, "path"), &length);
        const unsigned long long copied_len = ((named != NULL) && (length < sizeof(file))) ? length : 0ull;
        memor.cpy(&(MemoriaCfg){.dst = file, .src = (named != NULL) ? named : "", .bytes = (size_t)copied_len});
        file[copied_len] = '\0';
        // the line is cut to its low word, as the tree counts lines
        const unsigned int line = (unsigned int)cli_servers_whole(json, cli_json_member(json, leaves, "line"), 0ull);
        CliServersTreeCalls symbols = {tree, incoming ? NULL : file, out, 0};
        mmgr_line_ch(out, '[');
        const int found =
            incoming ? cli_symbols_callers(file, line, calls_held, &symbols, calls_symbols_written, &symbols, err_msg)
                     : cli_symbols_callees(file, line, calls_held, &symbols, calls_symbols_written, &symbols, err_msg);
        mmgr_line_ch(out, ']');
        cli_servers_reading_released(&s_item);
        return found >= 0;
    }
    const unsigned int own = cli_json_member(json, given, "orior");
    int done = 1;
    if (own != CLI_JSON_NONE)
    {
        char root[CLI_PATH_MAX_LEN];
        char ignored[CLI_PATH_MAX_LEN];
        done = calls_tree_file(path, root, ignored, err_msg);
        if (done)
        {
            char file[CLI_PATH_MAX_LEN];
            unsigned long long length = 0ull;
            const char *const named = cli_json_string(json, cli_json_member(json, own, "path"), &length);
            const unsigned long long copied_len = ((named != NULL) && (length < sizeof(file))) ? length : 0ull;
            memor.cpy(&(MemoriaCfg){.dst = file, .src = (named != NULL) ? named : "", .bytes = (size_t)copied_len});
            file[copied_len] = '\0';
            // the line is cut to its low word, as the tree counts lines
            const unsigned int line = (unsigned int)cli_servers_whole(json, cli_json_member(json, own, "line"), 0ull);
            CliServersCalls calls = {tree, root, file, incoming, out, 0};
            unsigned int at = 0u;
            mmgr_line_ch(out, '[');
            if (cli_inspect_tree_function_on(file, line, &at))
            {
                if (incoming)
                {
                    cli_inspect_tree_callers(file, at, calls_reached, &calls);
                }
                else
                {
                    cli_inspect_tree_callees(file, at, calls_reached, &calls);
                }
            }
            mmgr_line_ch(out, ']');
        }
        cli_servers_reading_released(&s_item);
        return done;
    }
    CliLsp *const server = cli_servers_owner_answering(path);
    if (server == NULL)
    {
        mmgr_line_put(err_msg, "no language server has this file open");
        cli_servers_reading_released(&s_item);
        return 0;
    }
    cli_servers_reading_locked(&s_reading);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    VerbaLinea params;
    cli_pieces_begun(&pieces, &params, piece, sizeof(piece), &g_cli_servers_cells);
    mmgr_line_put(&params, "{\"item\":");
    cli_json_write(json, given, &params, 0u, 0);
    mmgr_line_ch(&params, '}');
    VerbaLinea result;
    cli_servers_answer_begun(&s_reading, &result);
    done = cli_servers_request(server, incoming ? "callHierarchy/incomingCalls" : "callHierarchy/outgoingCalls",
                               &pieces, &params, CLI_SERVERS_SEARCHING, &result, err_msg);
    cli_lsp_unref(server);
    if (done)
    {
        unsigned long long length = 0ull;
        const char *const uri = cli_json_string(json, cli_json_member(json, given, "uri"), &length);
        char parent[CLI_PATH_MAX_LEN];
        const int has_parent = (uri != NULL) && cli_lsp_path_of(uri, length, parent);
        const CliJson *const answer = &s_reading.json;
        const unsigned int all = cli_servers_answer_read(&s_reading, &result);
        mmgr_line_ch(out, '[');
        int written = 0;
        for (unsigned int one = cli_json_is(answer, all, CFG_JSON_ARRAY) ? answer->node[all].first : CLI_JSON_NONE;
             one != CLI_JSON_NONE; one = answer->node[one].next)
        {
            // the call's sites in a cell of one place for each range it gives, none where it cannot be taken
            const unsigned int ranges = cli_json_member(answer, one, "fromRanges");
            const unsigned int given_ranges =
                cli_json_is(answer, ranges, CFG_JSON_ARRAY) ? answer->node[ranges].count : 0u;
            CliTextPlace *const site =
                (given_ranges > 0u)
                    ? g_cli_cells->temporary_buf_alloc((unsigned long long)given_ranges * sizeof(CliTextPlace), &g_cli_servers_cells)
                    : NULL;
            unsigned int count = 0u;
            for (unsigned int range = (site != NULL) ? answer->node[ranges].first : CLI_JSON_NONE;
                 range != CLI_JSON_NONE; range = answer->node[range].next)
            {
                site[count] = cli_servers_place(answer, cli_json_member(answer, range, "start"));
                count += 1u;
            }
            const unsigned long long before = out->at;
            mmgr_line_put(out, written ? "," : "");
            const int wrote = incoming
                                  ? calls_of(answer, cli_json_member(answer, one, "from"), NULL, site, count, tree, out)
                                  : calls_of(answer, cli_json_member(answer, one, "to"), has_parent ? parent : NULL,
                                             site, count, tree, out);
            g_cli_cells->persistent_buf_release(site, &g_cli_servers_cells);
            if (!wrote)
            {
                out->at = before;
                continue;
            }
            written = 1;
        }
        mmgr_line_ch(out, ']');
    }
    cli_servers_reading_released(&s_reading);
    cli_servers_reading_released(&s_item);
    return done;
}
