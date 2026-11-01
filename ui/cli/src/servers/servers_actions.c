// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_actions.c: the quick fixes and refactorings for a span of a file: the inspections' and the checkers' fixes
// of the findings that touch it first, then what its server offers, then what each plugin server that has open it
// offers, each given the diagnostics it gave that touch it, a fix offered for a diagnostic and again for its note
// listed once, those that can be carried out first and the preferred fixes before the rest. An action of a plugin's
// server carries the plugin's id, so it is carried out by that server (servers_act.c). A server given the file as a
// slice is asked about the slice's lines, with the diagnostics on them
#include "../command_line/cells.h"
#include "servers_internal.h"

// what is read, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};

void cli_servers_member_written(const CliJson *json, unsigned int node, const char *name, VerbaLinea *out)
{
    const unsigned int member = cli_json_member(json, node, name);
    if (member == CLI_JSON_NONE)
    {
        mmgr_line_put(out, "null");
        return;
    }
    cli_json_write(json, member, out, 0u, 0);
}

// the text stored under `key` in `keyed` read: its root, CLI_JSON_NONE where none is
static unsigned int actions_stored_read(CliKeyed *keyed, CliLock *lock, const char *key)
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
static void actions_ours(unsigned int array, CliTextPlace from, CliTextPlace to, int *written, VerbaLinea *out)
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
            mmgr_line_put(out, *written ? ",{\"title\":" : "{\"title\":");
            cli_json_quoted(out, (title != NULL) ? title : "", size);
            mmgr_line_put(out, ",\"kind\":\"quickfix\",\"preferred\":true,\"disabled\":null,\"raw\":");
            const unsigned int run = cli_json_member(json, fix, "run");
            if (cli_json_is(json, run, CFG_JSON_OBJECT))
            {
                mmgr_line_put(out, "{\"orior_run\":");
                cli_json_write(json, run, out, 0u, 0);
            }
            else
            {
                mmgr_line_put(out, "{\"orior\":");
                cli_servers_member_written(json, fix, "edits", out);
            }
            mmgr_line_put(out, "}}");
            *written = 1;
        }
    }
}

// the params that request from the server of `uri` its actions on `from` to `to`, with its diagnostics that touch it,
// each on the slice's lines where the server was given the file at `path` as a slice, `path` empty where the server
// was given it whole, written into `params`: 1, or 0 where the slice holds no line of the span
static int actions_params(unsigned int all, const char *path, const char *uri, CliTextPlace from, CliTextPlace to,
                          VerbaLinea *params)
{
    unsigned int first = from.line;
    unsigned int last = to.line;
    if (!cli_servers_slice_in(path, from.line, &first) || !cli_servers_slice_in(path, to.line, &last))
    {
        return 0;
    }
    mmgr_line_put(params, "{\"textDocument\":{\"uri\":");
    cli_json_quoted(params, uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(params, "},\"range\":{\"start\":{\"line\":");
    mmgr_line_decimal(params, first, 1u);
    mmgr_line_put(params, ",\"character\":");
    mmgr_line_decimal(params, from.col, 1u);
    mmgr_line_put(params, "},\"end\":{\"line\":");
    mmgr_line_decimal(params, last, 1u);
    mmgr_line_put(params, ",\"character\":");
    mmgr_line_decimal(params, to.col, 1u);
    mmgr_line_put(params, "}},\"context\":{\"diagnostics\":[");

    const CliJson *const json = &s_reading.json;
    int written = 0;
    for (unsigned int one = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].first : CLI_JSON_NONE;
         one != CLI_JSON_NONE; one = json->node[one].next)
    {
        // a diagnostic the slice holds neither line of is none the server gave
        const unsigned int range = cli_json_member(json, one, "range");
        unsigned int slice_line = 0u;
        if (cli_servers_overlaps(from, to, json, range) &&
            cli_servers_slice_in(path, cli_servers_place(json, cli_json_member(json, range, "start")).line,
                                 &slice_line) &&
            cli_servers_slice_in(path, cli_servers_place(json, cli_json_member(json, range, "end")).line, &slice_line))
        {
            mmgr_line_put(params, written ? "," : "");
            (void)cli_servers_slice_ranged_in(path, json, one, params);
            written = 1;
        }
    }
    mmgr_line_put(params, "],\"triggerKind\":1}}");
    return 1;
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

// the server's actions written after those written, each distinguished by its title and its edit once; the actions
// retained in a cell of one place for each the server offers, none written where it cannot be taken
static void actions_offered(unsigned int all, const char *plugin, int *written, VerbaLinea *out)
{
    const CliJson *const json = &s_reading.json;
    const unsigned int offered = cli_json_is(json, all, CFG_JSON_ARRAY) ? json->node[all].count : 0u;
    unsigned int *const action =
        (offered > 0u)
            ? g_cli_cells->temporary_buf_alloc((unsigned long long)offered * sizeof(unsigned int), &g_cli_servers_cells)
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
        for (unsigned int before = 0u; !seen && (before < count); before += 1u)
        {
            seen = cli_json_same(json, title, json, cli_json_member(json, action[before], "title")) &&
                   cli_json_same(json, edit, json, cli_json_member(json, action[before], "edit"));
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
                mmgr_line_put(out, *written ? "," : "");
                (void)cli_servers_action_of(json, action[at], plugin, out);
                *written = 1;
            }
        }
    }
    g_cli_cells->persistent_buf_release(action, &g_cli_servers_cells);
}

// `server`, the server of the plugin `plugin` where that is not NULL, queried for its actions on `from` to `to` of the
// file at `path`, of `uri`, with its diagnostics `all` that touch it, and what it offers written after those written,
// none where its own server was given the file as a slice that holds no line of the span: 1, or 0 with why given
static int actions_request(CliLsp *server, const char *plugin, unsigned int all, const char *path, const char *uri,
                           CliTextPlace from, CliTextPlace to, int *written, VerbaLinea *out, VerbaLinea *err_msg)
{
    // a span the slice the server was given does not hold is one it offers nothing on
    unsigned int slice_line = 0u;
    if ((plugin == NULL) &&
        (!cli_servers_slice_in(path, from.line, &slice_line) || !cli_servers_slice_in(path, to.line, &slice_line)))
    {
        return 1;
    }
    // the diagnostics written with the span come in a server's answer, and so fit what an answer holds
    char *const params_text = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_ANSWER, &g_cli_servers_cells);
    VerbaLinea params = mmgr_line_over(params_text, CLI_SERVERS_ANSWER);
    const int whole =
        actions_params(all, (plugin == NULL) ? path : "", uri, from, to, &params) && mmgr_line_ok(&params);
    if (!whole)
    {
        mmgr_line_put(err_msg, "the request for actions could not be written: its params do not fit");
    }
    CliLspAnswer answer = {NULL, NULL, 0ull};
    const int requested = whole && cli_lsp_requested(server, "textDocument/codeAction", params.out, params.at,
                                                     CLI_SERVERS_REQUEST_TIMEOUT_MS, &answer, err_msg);
    g_cli_cells->persistent_buf_release(params_text, &g_cli_servers_cells);
    if (requested)
    {
        actions_offered(cli_servers_answer_read(&s_reading, &answer), plugin, written, out);
    }
    return requested;
}

int cli_servers_actions(const char *path, CliTextPlace from, CliTextPlace to, VerbaLinea *out, VerbaLinea *err_msg)
{
    char uri[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    cli_servers_uri_of(path, uri);
    cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
    cli_servers_reading_locked(&s_reading);
    const unsigned long long start = out->at;
    mmgr_line_ch(out, '[');
    int written = 0;
    actions_ours(actions_stored_read(&g_cli_servers_inspected, &g_cli_servers_checks_lock, key), from, to, &written,
                 out);
    const unsigned int by_checker = actions_stored_read(&g_cli_servers_checked, &g_cli_servers_checks_lock, key);
    const CliJson *const json = &s_reading.json;
    for (unsigned int id = cli_json_is(json, by_checker, CFG_JSON_OBJECT) ? json->node[by_checker].first
                                                                          : CLI_JSON_NONE;
         id != CLI_JSON_NONE; id = json->node[id].next)
    {
        actions_ours(id, from, to, &written, out);
    }
    // the tree's own server queried first, its refusal reported only where no other offers anything
    CliLsp *const server = cli_servers_owner_started(path);
    int requested = 1;
    char refused[256];
    VerbaLinea refusal = {refused, sizeof(refused), 0ull};
    if (server != NULL)
    {
        const unsigned int all = actions_stored_read(&g_cli_servers_published, &g_cli_servers_published_lock, key);
        requested = actions_request(server, NULL, all, path, uri, from, to, &written, out, &refusal);
        cli_lsp_unref(server);
    }
    // each plugin server that has the file open queried as well, given the diagnostics it gave, a server that does not
    // answer offering nothing
    int plugged = 0;
    char plugin[CLI_NAME_MAX_LEN];
    unsigned int place = 0u;
    for (CliLsp *server_at = cli_servers_plugin_owner_of(&place, path, plugin); server_at != NULL;
         server_at = cli_servers_plugin_owner_of(&place, path, plugin))
    {
        char why[256];
        VerbaLinea quiet = {why, sizeof(why), 0ull};
        (void)actions_request(server_at, plugin, cli_json_member(json, by_checker, plugin), path, uri, from, to,
                              &written, out, &quiet);
        cli_lsp_unref(server_at);
        plugged = 1;
    }
    if ((server == NULL) && !written && !plugged)
    {
        mmgr_line_put(err_msg, "no language server has this file open");
        requested = 0;
    }
    if (!requested && (written || plugged))
    {
        requested = 1;
    }
    else if (!requested)
    {
        mmgr_line_put_n(err_msg, refused, mmgr_line_ok(&refusal) ? refusal.at : sizeof(refused));
    }
    mmgr_line_ch(out, ']');
    out->at = requested ? out->at : start;
    cli_servers_reading_released(&s_reading);
    return requested;
}
