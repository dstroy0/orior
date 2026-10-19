// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_calls.c: the call hierarchy: the function at a place as its top, as its server finds it where the server
// answers for calls and as the inspections' index finds it elsewhere; and the functions that call a function, or that
// it calls, each with the places of its calls, from the server of the file the hierarchy started in or from the index
// where the function is its. A function is written with its name and what the server says of it, its file and the span
// of its name, the file its calls stand in and where each starts, and the server's or the index's own name for it,
// which is given back to read the level below it
#include "../inspect/inspect_tree.h"
#include "servers_internal.h"

#include "../text/text.h"

// What a level of the index's hierarchy is written with: the tree and the index's tree, the file the hierarchy is
// at, whether its calls come in, where it is written and whether one is
typedef struct
{
    const char *tree;
    const char *root;
    const char *file;
    int incoming;
    CliLine *out;
    int written;
} CliServersCalls;

// what the server answers, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};
static CliServersReading s_item = {CLI_LOCK_INIT};

// a path written as a JSON string as it stands in `tree`
static void calls_path_written(const char *tree, const char *path, CliLine *out)
{
    char shown[CLI_PATH_LONGEST];
    CliLine where = {shown, sizeof(shown), 0ull};
    cli_servers_tree_path(tree, path, &where);
    cli_json_quoted_lossy(out, (const unsigned char *)shown, cli_line_fits(&where) ? where.at : 0ull);
}

// the places of calls written as an array
static void calls_sites_written(const CliTextPlace *sites, unsigned int count, CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_text_place_written(sites[at], out);
    }
    cli_line_character(out, ']');
}

// a function of a server's item written, its calls in `site`, the item's own file where it is NULL, at `sites`: 1, or
// 0 where the item names no file
static int calls_of(const CliJson *json, unsigned int item, const char *site, const CliTextPlace *sites,
                    unsigned int count, const char *tree, CliLine *out)
{
    unsigned long long length = 0ull;
    const char *const uri = cli_json_string(json, cli_json_member(json, item, "uri"), &length);
    char path[CLI_PATH_LONGEST];
    if ((uri == NULL) || !cli_lsp_path_of(uri, length, path))
    {
        return 0;
    }
    const unsigned int selection = cli_json_member(json, item, "selectionRange");
    const unsigned int span =
        cli_json_is(json, selection, CFG_JSON_OBJECT) ? selection : cli_json_member(json, item, "range");
    unsigned long long size = 0ull;
    const char *const name = cli_json_string(json, cli_json_member(json, item, "name"), &size);
    cli_line_text(out, "{\"name\":");
    cli_json_quoted(out, (name != NULL) ? name : "", size);
    const char *const detail = cli_json_string(json, cli_json_member(json, item, "detail"), &size);
    cli_line_text(out, ",\"detail\":");
    cli_json_quoted(out, (detail != NULL) ? detail : "", (detail != NULL) ? size : 0ull);
    cli_line_text(out, ",\"path\":");
    calls_path_written(tree, path, out);
    cli_line_text(out, ",\"from\":");
    cli_text_place_written(cli_servers_place(json, cli_json_member(json, span, "start")), out);
    cli_line_text(out, ",\"to\":");
    cli_text_place_written(cli_servers_place(json, cli_json_member(json, span, "end")), out);
    cli_line_text(out, ",\"site\":");
    calls_path_written(tree, (site != NULL) ? site : path, out);
    cli_line_text(out, ",\"at\":");
    calls_sites_written(sites, count, out);
    cli_line_text(out, ",\"item\":");
    cli_json_write(json, item, out, 0u, 0);
    cli_line_character(out, '}');
    return 1;
}

// a function of the index written, its calls in `site` at `sites`
static void calls_indexed_written(const CliServersCalls *calls, const char *file, const CliFacts *facts,
                                  unsigned int function, const char *site, const CliInspectSpan *sites,
                                  unsigned int count)
{
    const CliInspectFunction *const one = &facts->function[function];
    CliLine *const out = calls->out;
    char path[CLI_PATH_LONGEST];
    cli_line_text(out, "{\"name\":");
    cli_json_quoted(out, cli_inspect_text(facts->text, one->name), one->name.size);
    cli_line_text(out, ",\"detail\":\"\",\"path\":");
    cli_full(calls->root, file, path);
    calls_path_written(calls->tree, path, out);
    cli_line_text(out, ",\"from\":");
    cli_text_place_written((CliTextPlace){one->from.line, one->from.col}, out);
    cli_line_text(out, ",\"to\":");
    cli_text_place_written((CliTextPlace){one->to.line, one->to.col}, out);
    cli_line_text(out, ",\"site\":");
    cli_full(calls->root, site, path);
    calls_path_written(calls->tree, path, out);
    cli_line_text(out, ",\"at\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_text_place_written((CliTextPlace){sites[at].from.line, sites[at].from.col}, out);
    }
    cli_line_text(out, "],\"item\":{\"orior\":{\"path\":");
    cli_json_quoted(out, file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"line\":");
    cli_line_decimal(out, one->from.line, 1u);
    cli_line_text(out, "}}}");
}

// a function the index reaches written after those written
static void calls_reached(void *context, const char *path, const CliFacts *facts, unsigned int function,
                          const CliInspectSpan *sites, unsigned int count)
{
    CliServersCalls *const calls = context;
    cli_line_text(calls->out, calls->written ? "," : "");
    calls_indexed_written(calls, path, facts, function, calls->incoming ? path : calls->file, sites, count);
    calls->written = 1;
}

// the tree the index holds and the path of `path` in it into `root` and `file`, each of which holds CLI_PATH_LONGEST:
// 1, or 0 with why said
static int calls_indexed_file(const char *path, char *root, char *file, CliLine *said)
{
    if (!cli_servers_indexed(root))
    {
        cli_line_text(said, "no language server answers calls here, and orior's own index is still being read");
        return 0;
    }
    const size_t top = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    if (!cli_text_same(path, root, top) || ((path[top] != '/') && (path[top] != '\\') && (path[top] != '\0')))
    {
        cli_line_text(said, path);
        cli_line_text(said, " is outside the tree");
        return 0;
    }
    const char *rest = path + top;
    rest += ((*rest == '/') || (*rest == '\\')) ? 1 : 0;
    CliLine line = {file, CLI_PATH_LONGEST, 0ull};
    for (; *rest != '\0'; rest += 1)
    {
        cli_line_character(&line, (*rest == '\\') ? '/' : *rest);
    }
    (void)cli_line_closed(&line);
    return 1;
}

int cli_servers_call_root(const char *tree, const char *path, unsigned int line, unsigned int col, CliLine *out,
                          CliLine *said)
{
    CliLsp *const server = cli_servers_holding(path);
    if ((server != NULL) && cli_servers_capable(server, "callHierarchyProvider"))
    {
        cli_servers_reading_held(&s_reading);
        CliLine result;
        cli_servers_answer_begun(&s_reading, &result);
        const int asked = cli_servers_asked_at(server, "textDocument/prepareCallHierarchy", path, line, col, NULL,
                                               CLI_SERVERS_ASKING, &result, said);
        cli_lsp_given(server);
        if (asked)
        {
            const CliJson *const json = &s_reading.json;
            const unsigned int first = cli_json_item(json, cli_servers_answer_read(&s_reading, &result), 0u);
            if ((first == CLI_JSON_NONE) || !calls_of(json, first, NULL, NULL, 0u, tree, out))
            {
                cli_line_text(out, "null");
            }
        }
        cli_servers_reading_released(&s_reading);
        return asked;
    }
    if (server != NULL)
    {
        cli_lsp_given(server);
    }
    char root[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    if (!calls_indexed_file(path, root, file, said))
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
        cli_line_text(out, "null");
    }
    return 1;
}

int cli_servers_calls(const char *tree, const char *path, const char *item, unsigned long long size, int incoming,
                      CliLine *out, CliLine *said)
{
    cli_servers_reading_held(&s_item);
    const CliJson *const json = &s_item.json;
    const unsigned int given = cli_servers_read(&s_item, item, size);
    const unsigned int own = cli_json_member(json, given, "orior");
    int done = 1;
    if (own != CLI_JSON_NONE)
    {
        char root[CLI_PATH_LONGEST];
        char ignored[CLI_PATH_LONGEST];
        done = calls_indexed_file(path, root, ignored, said);
        if (done)
        {
            char file[CLI_PATH_LONGEST];
            unsigned long long length = 0ull;
            const char *const named = cli_json_string(json, cli_json_member(json, own, "path"), &length);
            const unsigned long long kept = ((named != NULL) && (length < sizeof(file))) ? length : 0ull;
            memor.cpy(&(MemoriaCfg){.dst = file, .src = (named != NULL) ? named : "", .bytes = (size_t)kept});
            file[kept] = '\0';
            // the line is cut to its low word, as the index counts lines
            const unsigned int line = (unsigned int)cli_servers_whole(json, cli_json_member(json, own, "line"), 0ull);
            CliServersCalls calls = {tree, root, file, incoming, out, 0};
            unsigned int at = 0u;
            cli_line_character(out, '[');
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
            cli_line_character(out, ']');
        }
        cli_servers_reading_released(&s_item);
        return done;
    }
    CliLsp *const server = cli_servers_holding(path);
    if (server == NULL)
    {
        cli_line_text(said, "no language server has this file open");
        cli_servers_reading_released(&s_item);
        return 0;
    }
    cli_servers_reading_held(&s_reading);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine params;
    cli_pieces_begun(&pieces, &params, piece, sizeof(piece));
    cli_line_text(&params, "{\"item\":");
    cli_json_write(json, given, &params, 0u, 0);
    cli_line_character(&params, '}');
    CliLine result;
    cli_servers_answer_begun(&s_reading, &result);
    done = cli_servers_asked(server, incoming ? "callHierarchy/incomingCalls" : "callHierarchy/outgoingCalls", &pieces,
                             &params, CLI_SERVERS_SEARCHING, &result, said);
    cli_lsp_given(server);
    if (done)
    {
        unsigned long long length = 0ull;
        const char *const uri = cli_json_string(json, cli_json_member(json, given, "uri"), &length);
        char parent[CLI_PATH_LONGEST];
        const int has_parent = (uri != NULL) && cli_lsp_path_of(uri, length, parent);
        const CliJson *const answer = &s_reading.json;
        const unsigned int all = cli_servers_answer_read(&s_reading, &result);
        cli_line_character(out, '[');
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
                    ? cli_cell_taken((unsigned long long)given_ranges * sizeof(CliTextPlace), "a call's sites")
                    : NULL;
            unsigned int count = 0u;
            for (unsigned int range = (site != NULL) ? answer->node[ranges].first : CLI_JSON_NONE;
                 range != CLI_JSON_NONE; range = answer->node[range].next)
            {
                site[count] = cli_servers_place(answer, cli_json_member(answer, range, "start"));
                count += 1u;
            }
            const unsigned long long before = out->at;
            cli_line_text(out, written ? "," : "");
            const int wrote = incoming
                                  ? calls_of(answer, cli_json_member(answer, one, "from"), NULL, site, count, tree, out)
                                  : calls_of(answer, cli_json_member(answer, one, "to"), has_parent ? parent : NULL,
                                             site, count, tree, out);
            cli_cell_given(site);
            if (!wrote)
            {
                out->at = before;
                continue;
            }
            written = 1;
        }
        cli_line_character(out, ']');
    }
    cli_servers_reading_released(&s_reading);
    cli_servers_reading_released(&s_item);
    return done;
}
