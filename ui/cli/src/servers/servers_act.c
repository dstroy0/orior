// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_act.c: a quick fix or refactoring carried out: orior's own edits returned to be written; a server's action,
// its edits filled in by the server first where it left them out, returned to be written, and its command run, whose
// edits the server requests on its own; and an action of a plugin's server carried out by that server, a command it
// does not run itself passed to its plugin. A fix that runs a line in the terminal is the page's to run
#include "../command_line/cells.h"
#include "servers_internal.h"

// what is read, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};
static CliServersReading s_resolved = {CLI_LOCK_INIT};

// a place of an edit orior_cli wrote, `{"line", "col"}`, written again: 1, or 0 with why given where it is none
static int actions_place_written(const CliJson *json, unsigned int edit, const char *name, VerbaLinea *out,
                                 VerbaLinea *err_msg)
{
    const unsigned int place = cli_json_member(json, edit, name);
    const unsigned long long none = ~0ull;
    const unsigned long long line = cli_servers_whole(json, cli_json_member(json, place, "line"), none);
    const unsigned long long col = cli_servers_whole(json, cli_json_member(json, place, "col"), none);
    if ((place == CLI_JSON_NONE) || (line > 0xffffffffull) || (col > 0xffffffffull))
    {
        mmgr_line_put(err_msg, (place == CLI_JSON_NONE) ? "missing field `" : "invalid type: expected field `");
        mmgr_line_put(err_msg, (place == CLI_JSON_NONE) ? name : ((line > 0xffffffffull) ? "line" : "col"));
        mmgr_line_ch(err_msg, '`');
        return 0;
    }
    cli_text_place_written((CliTextPlace){(unsigned int)line, (unsigned int)col}, out);
    return 1;
}

// orior's own edits of a fix written as the file's edits: 1, or 0 with why given where they are not edits
static int actions_ours_written(const char *tree, const char *path, const CliJson *json, unsigned int edits,
                                VerbaLinea *out, VerbaLinea *err_msg)
{
    if (!cli_json_is(json, edits, CFG_JSON_ARRAY))
    {
        mmgr_line_put(err_msg, "invalid type: expected a sequence");
        return 0;
    }
    char shown[CLI_PATH_MAX_LEN];
    VerbaLinea where = {shown, sizeof(shown), 0ull};
    cli_servers_tree_path(tree, path, &where);
    mmgr_line_put(out, "[{\"path\":");
    cli_json_quoted_lossy(out, (const unsigned char *)shown, mmgr_line_ok(&where) ? where.at : 0ull);
    mmgr_line_put(out, ",\"edits\":[");
    for (unsigned int edit = json->node[edits].first; edit != CLI_JSON_NONE; edit = json->node[edit].next)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, cli_json_member(json, edit, "text"), &size);
        mmgr_line_put(out, (edit != json->node[edits].first) ? ",{\"from\":" : "{\"from\":");
        if (!actions_place_written(json, edit, "from", out, err_msg))
        {
            return 0;
        }
        mmgr_line_put(out, ",\"to\":");
        if (!actions_place_written(json, edit, "to", out, err_msg))
        {
            return 0;
        }
        if (text == NULL)
        {
            mmgr_line_put(err_msg, "missing field `text`");
            return 0;
        }
        mmgr_line_put(out, ",\"text\":");
        cli_json_quoted(out, text, size);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_put(out, "]}]");
    return 1;
}

// the server's command `command`, named by `name` with its `arguments`, run: by the server where it runs it, and passed
// to the plugin `plugin` where the server is a plugin's and does not: 1, or 0 with why given
static int actions_command(CliLsp *server, const char *plugin, const CliJson *json, unsigned int command,
                           const char *name, VerbaLinea *err_msg)
{
    // a command and its arguments come in a server's answer, and so fit what an answer holds
    char *const params_text = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_ANSWER, &g_cli_servers_cells);
    VerbaLinea params = mmgr_line_over(params_text, CLI_SERVERS_ANSWER);
    mmgr_line_put(&params, "{\"command\":");
    cli_servers_member_written(json, command, name, &params);
    mmgr_line_put(&params, ",\"arguments\":");
    cli_servers_member_written(json, command, "arguments", &params);
    mmgr_line_ch(&params, '}');
    const int whole = mmgr_line_ok(&params);
    if (!whole)
    {
        mmgr_line_put(err_msg, "the command could not be written: its params do not fit");
    }
    const char *const named = cli_json_string(json, cli_json_member(json, command, name), NULL);
    const int passed = (plugin != NULL) && ((named == NULL) || !cli_servers_capable_command(server, named));
    if (passed && whole)
    {
        cli_servers_plugin_command(plugin, params.out, params.at);
    }
    // nothing reads the command's answer
    CliLspAnswer answer = {NULL, NULL, 0ull};
    const int done = whole && (passed || cli_lsp_requested(server, "workspace/executeCommand", params.out, params.at,
                                                           CLI_SERVERS_SEARCHING, &answer, err_msg));
    g_cli_cells->persistent_buf_release(answer.message, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(params_text, &g_cli_servers_cells);
    return done;
}

// the action at `action` of `json`, `size` bytes of `raw` as the server gave it, carried out by `server`, the server of
// the plugin `plugin` where that is not NULL: its edits written to `out`, filled in by the server first where it left
// them out, and its command run: 1, or 0 with why given
static int actions_by(CliLsp *server, const char *plugin, const char *tree, const CliJson *json, unsigned int action,
                      const char *raw, unsigned long long size, VerbaLinea *out, VerbaLinea *err_msg)
{
    if (cli_json_is(json, cli_json_member(json, action, "command"), CFG_JSON_STRING))
    {
        const int done = actions_command(server, plugin, json, action, "command", err_msg);
        mmgr_line_put(out, done ? "[]" : "");
        return done;
    }
    int done = 1;
    const unsigned int edit = cli_json_member(json, action, "edit");
    const unsigned int command = cli_json_member(json, action, "command");
    if (((edit == CLI_JSON_NONE) || cli_json_is(json, edit, CFG_JSON_NULL)) &&
        ((command == CLI_JSON_NONE) || cli_json_is(json, command, CFG_JSON_NULL)) &&
        cli_servers_capable_true(server, "codeActionProvider", "resolveProvider"))
    {
        CliLspAnswer answer = {NULL, NULL, 0ull};
        done = cli_lsp_requested(server, "codeAction/resolve", raw, size, CLI_SERVERS_REQUEST_TIMEOUT_MS, &answer,
                                 err_msg);
        json = &s_resolved.json;
        action = done ? cli_servers_answer_read(&s_resolved, &answer) : CLI_JSON_NONE;
    }
    // the edits kept until the command after them has run, and written only where it ran
    char *const edits_text = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_ANSWER, &g_cli_servers_cells);
    VerbaLinea edits = mmgr_line_over(edits_text, CLI_SERVERS_ANSWER);
    if (done)
    {
        cli_servers_workspace_edit_of(json, cli_json_member(json, action, "edit"), tree, &edits);
        done = mmgr_line_ok(&edits);
        if (!done)
        {
            mmgr_line_put(err_msg, "the action's edits could not be stored: they do not fit");
        }
    }
    const unsigned int then = cli_json_member(json, action, "command");
    if (done && cli_json_is(json, then, CFG_JSON_OBJECT))
    {
        done = actions_command(server, plugin, json, then, "command", err_msg);
    }
    if (done)
    {
        (void)mmgr_line_put_n(out, edits.out, edits.at);
    }
    g_cli_cells->persistent_buf_release(edits_text, &g_cli_servers_cells);
    return done;
}

// an action of a plugin's server, `{"orior_plugin", "action"}` at `wrapped`, carried out by the plugin's server: 1, or
// 0 with why given
static int actions_plugin_by(const char *tree, const CliJson *json, unsigned int wrapped, VerbaLinea *out,
                             VerbaLinea *err_msg)
{
    const char *const plugin = cli_json_string(json, cli_json_member(json, wrapped, "orior_plugin"), NULL);
    CliLsp *const server = (plugin != NULL) ? cli_servers_plugin_of(plugin, err_msg) : NULL;
    if (server == NULL)
    {
        mmgr_line_put(err_msg, (plugin == NULL) ? "invalid type: expected a string `orior_plugin`" : "");
        return 0;
    }
    // the action as the server gave it, for the server to fill in
    const unsigned int action = cli_json_member(json, wrapped, "action");
    const unsigned long long room = cli_json_written_room(json);
    char *const raw = g_cli_cells->temporary_buf_alloc(room, &g_cli_servers_cells);
    VerbaLinea text = mmgr_line_over(raw, room);
    cli_json_write(json, action, &text, 0u, 0);
    const int done =
        mmgr_line_ok(&text) && actions_by(server, plugin, tree, json, action, text.out, text.at, out, err_msg);
    g_cli_cells->persistent_buf_release(raw, &g_cli_servers_cells);
    cli_lsp_unref(server);
    return done;
}

int cli_servers_act(const char *tree, const char *path, const char *raw, unsigned long long size, VerbaLinea *out,
                    VerbaLinea *err_msg)
{
    cli_servers_reading_locked(&s_reading);
    cli_servers_reading_locked(&s_resolved);
    const CliJson *const json = &s_reading.json;
    const unsigned int action = cli_servers_read(&s_reading, raw, size);
    int done = 1;
    if (cli_json_member(json, action, "orior_run") != CLI_JSON_NONE)
    {
        mmgr_line_put(out, "[]");
    }
    else if (cli_json_member(json, action, "orior") != CLI_JSON_NONE)
    {
        done = actions_ours_written(tree, path, json, cli_json_member(json, action, "orior"), out, err_msg);
    }
    else if (cli_json_member(json, action, "orior_plugin") != CLI_JSON_NONE)
    {
        done = actions_plugin_by(tree, json, action, out, err_msg);
    }
    else
    {
        CliLsp *const server = cli_servers_owner_started(path);
        done = (server != NULL) && actions_by(server, NULL, tree, json, action, raw, size, out, err_msg);
        if (server == NULL)
        {
            mmgr_line_put(err_msg, "no language server has this file open");
        }
        else
        {
            cli_lsp_unref(server);
        }
    }
    cli_servers_reading_released(&s_resolved);
    cli_servers_reading_released(&s_reading);
    return done;
}
