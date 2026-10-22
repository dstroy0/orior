// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_actions.c: the quick fixes and refactorings for a span of a file: the inspections' and the checkers' fixes
// of the findings that touch it first, then what its server offers, then what each plugin server that holds it offers,
// each given the diagnostics it gave that touch it, a fix offered for a diagnostic and again for its note listed once,
// those that can be carried out first and the preferred fixes before the rest. An action of a plugin's server carries
// the plugin's id, so it is carried out by that server (servers_act.c)
#include "../command_line/cells.h"
#include "servers_internal.h"

// what is read, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};

void cli_servers_member_written(const CliJson *json, unsigned int node, const char *name, CliLine *out)
{
    const unsigned int member = cli_json_member(json, node, name);
    if (member == CLI_JSON_NONE)
    {
        cli_line_text(out, "null");
        return;
    }
    cli_json_write(json, member, out, 0u, 0);
}

// the text kept under `key` in `keyed` read: its root, CLI_JSON_NONE where none is
static unsigned int actions_kept_read(CliKeyed *keyed, CliLock *lock, const char *key)
{
    cli_lock_take(lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const text = cli_keyed_get(keyed, key, &beside, &size);
    const unsigned int root =
        ((text != NULL) && (size > 0ull)) ? cli_servers_read(&s_reading, text, size) : CLI_JSON_NONE;
    cli_lock_give(lock);
    return root;
}

// the fixes of each finding of `array` that touches `from` to `to`, written after those written
static void actions_ours(unsigned int array, CliTextPlace from, CliTextPlace to, int *written, CliLine *out)
{
    const CliJson *const json = &s_reading.json;
    for (unsigned int one = cli_json_is(json, array, CFG_JSON_ARRAY) ? json->node[array].first : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        if (!cli_servers_overlaps(from, to, json, cli_json_member(json, one, "range")))
        {
            continue;
        }
        const unsigned int fixes = cli_json_member(json, cli_json_member(json, one, "data"), "orior");
        for (unsigned int fix = cli_json_is(json, fixes, CFG_JSON_ARRAY) ? json->node[fixes].first : CLI_JSON_NONE;
             fix != CLI_JSON_NONE; fix = json->node[fix].next)
        {
            unsigned long long size = 0ull;
            const char *const title = cli_json_string(json, cli_json_member(json, fix, "title"), &size);
            cli_line_text(out, *written ? ",{\"title\":" : "{\"title\":");
            cli_json_quoted(out, (title != NULL) ? title : "", size);
            cli_line_text(out, ",\"kind\":\"quickfix\",\"preferred\":true,\"disabled\":null,\"raw\":");
            const unsigned int run = cli_json_member(json, fix, "run");
            if (cli_json_is(json, run, CFG_JSON_OBJECT))
            {
                cli_line_text(out, "{\"orior_run\":");
                cli_json_write(json, run, out, 0u, 0);
            }
            else
            {
                cli_line_text(out, "{\"orior\":");
                cli_servers_member_written(json, fix, "edits", out);
            }
            cli_line_text(out, "}}");
            *written = 1;
        }
    }
}

// the params that ask the server of `uri` for its actions on `from` to `to`, with its diagnostics that touch it, in a
// cell of their size, their size into `length`: the cell, or NULL where the cells cannot meet them
static char *actions_params(unsigned int all, const char *uri, CliTextPlace from, CliTextPlace to,
                            unsigned long long *length)
{
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine params;
    cli_pieces_begun(&pieces, &params, piece, sizeof(piece));
    cli_line_text(&params, "{\"textDocument\":{\"uri\":");
    cli_json_quoted(&params, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    cli_line_text(&params, "},\"range\":{\"start\":{\"line\":");
    cli_line_decimal(&params, from.line, 1u);
    cli_line_text(&params, ",\"character\":");
    cli_line_decimal(&params, from.col, 1u);
    cli_line_text(&params, "},\"end\":{\"line\":");
    cli_line_decimal(&params, to.line, 1u);
    cli_line_text(&params, ",\"character\":");
    cli_line_decimal(&params, to.col, 1u);
    cli_line_text(&params, "}},\"context\":{\"diagnostics\":[");

    const CliJson *const json = &s_reading.json;
    int written = 0;
    for (unsigned int one = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].first : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        if (cli_servers_overlaps(from, to, json, cli_json_member(json, one, "range")))
        {
            cli_line_text(&params, written ? "," : "");
            cli_json_write(json, one, &params, 0u, 0);
            written = 1;
        }
    }
    cli_line_text(&params, "],\"triggerKind\":1}}");
    return cli_pieces_joined(&pieces, &params, "the params an asking for actions is sent with", length);
}

// the order an action goes in: those that can be carried out first, then the preferred, then the quick fixes
static unsigned int actions_rank(const CliJson *json, unsigned int one)
{
    unsigned long long size = 0ull;
    const char *const kind = cli_json_string(json, cli_json_member(json, one, "kind"), &size);
    const int disabled =
        cli_json_string(json, cli_json_member(json, cli_json_member(json, one, "disabled"), "reason"), NULL) != NULL;
    const int preferred = cli_json_is(json, cli_json_member(json, one, "isPreferred"), CFG_JSON_TRUE);
    const int quickfix = (kind != NULL) && cli_text_starts(kind, size, "quickfix");
    return (disabled ? 4u : 0u) + (preferred ? 0u : 2u) + (quickfix ? 0u : 1u);
}

// the server's actions written after those written, each told apart by its title and its edit once; the actions kept
// in a cell of one place for each the server offers, none written where it cannot be taken
static void actions_offered(unsigned int all, const char *plugin, int *written, CliLine *out)
{
    const CliJson *const json = &s_reading.json;
    const unsigned int offered = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].count : 0u;
    unsigned int *const action =
        (offered > 0u) ? cli_cell_taken((unsigned long long)offered * sizeof(unsigned int), "the actions offered")
                       : NULL;
    unsigned int count = 0u;
    for (unsigned int one = (action != NULL) ? json->node[all].first : CLI_JSON_NONE; one != CLI_JSON_NONE;
         one = json->node[one].next)
    {
        const unsigned int title = cli_json_member(json, one, "title");
        if (cli_json_string(json, title, NULL) == NULL)
        {
            continue;
        }
        const unsigned int edit = cli_json_member(json, one, "edit");
        int seen = 0;
        for (unsigned int kept = 0u; !seen && (kept < count); kept += 1u)
        {
            seen = cli_json_same(json, title, json, cli_json_member(json, action[kept], "title")) &&
                   cli_json_same(json, edit, json, cli_json_member(json, action[kept], "edit"));
        }
        if (!seen)
        {
            action[count] = one;
            count += 1u;
        }
    }
    for (unsigned int rank = 0u; rank < 8u; rank += 1u)
    {
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            if (actions_rank(json, action[at]) == rank)
            {
                cli_line_text(out, *written ? "," : "");
                (void)cli_servers_action_of(json, action[at], plugin, out);
                *written = 1;
            }
        }
    }
    cli_cell_given(action);
}

// `server`, the server of the plugin `plugin` where that is not NULL, asked for its actions on `from` to `to` of the
// file `uri` with its diagnostics `all` that touch it, and what it offers written after those written: 1, or 0 with
// why said
static int actions_asked(CliLsp *server, const char *plugin, unsigned int all, const char *uri, CliTextPlace from,
                         CliTextPlace to, int *written, CliLine *out, CliLine *said)
{
    unsigned long long length = 0ull;
    char *const params = actions_params(all, uri, from, to, &length);
    if (params == NULL)
    {
        cli_line_text(said, "the asking for actions could not be written: the cells cannot meet it");
    }
    CliLine result;
    cli_servers_answer_begun(&s_reading, &result);
    const int asked = (params != NULL) && cli_lsp_requested(server, "textDocument/codeAction", params, length,
                                                            CLI_SERVERS_ASKING, &result, said);
    cli_cell_given(params);
    if (asked)
    {
        actions_offered(cli_servers_answer_read(&s_reading, &result), plugin, written, out);
    }
    return asked;
}

int cli_servers_actions(const char *path, CliTextPlace from, CliTextPlace to, CliLine *out, CliLine *said)
{
    char uri[CLI_PATH_LONGEST];
    char key[CLI_PATH_LONGEST];
    cli_servers_uri_of(path, uri);
    cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
    cli_servers_reading_held(&s_reading);
    const unsigned long long start = out->at;
    cli_line_character(out, '[');
    int written = 0;
    actions_ours(actions_kept_read(&g_cli_servers_inspected, &g_cli_servers_checks_lock, key), from, to, &written, out);
    const unsigned int by_checker = actions_kept_read(&g_cli_servers_checked, &g_cli_servers_checks_lock, key);
    const CliJson *const json = &s_reading.json;
    for (unsigned int id = cli_json_is(json, by_checker, CFG_JSON_OBJECT) ? json->node[by_checker].first
                                                                          : CLI_JSON_NONE;
         id != CLI_JSON_NONE; id = json->node[id].next)
    {
        actions_ours(id, from, to, &written, out);
    }
    CliLsp *const server = cli_servers_holding(path);
    int asked = 1;
    if (server != NULL)
    {
        const unsigned int all = actions_kept_read(&g_cli_servers_published, &g_cli_servers_published_lock, key);
        asked = actions_asked(server, NULL, all, uri, from, to, &written, out, said);
        cli_lsp_given(server);
    }
    // each plugin server that holds the file asked as well, given the diagnostics it gave, a server that does not
    // answer offering nothing
    int plugged = 0;
    char plugin[CLI_NAME_LONGEST];
    unsigned int place = 0u;
    for (CliLsp *held = cli_servers_plugin_holding(&place, path, plugin); held != NULL;
         held = cli_servers_plugin_holding(&place, path, plugin))
    {
        char why[256];
        CliLine quiet = {why, sizeof(why), 0ull};
        (void)actions_asked(held, plugin, cli_json_member(json, by_checker, plugin), uri, from, to, &written, out,
                            &quiet);
        cli_lsp_given(held);
        plugged = 1;
    }
    if ((server == NULL) && !written && !plugged)
    {
        cli_line_text(said, "no language server has this file open");
        asked = 0;
    }
    cli_line_character(out, ']');
    out->at = asked ? out->at : start;
    cli_servers_reading_released(&s_reading);
    return asked;
}
