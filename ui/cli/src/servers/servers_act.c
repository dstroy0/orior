// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_act.c: a quick fix or refactoring carried out: orior's own edits returned to be written; a server's action,
// its edits filled in by the server first where it left them out, returned to be written, and its command run, whose
// edits the server asks for on its own; and an action of a plugin's server carried out by that server, a command it
// does not run itself passed to its plugin. A fix that runs a line in the terminal is the page's to run
#include "../command_line/cells.h"
#include "servers_internal.h"

// what is read, under the reading's lock
static CliServersReading s_reading = {CLI_LOCK_INIT};
static CliServersReading s_resolved = {CLI_LOCK_INIT};

// a piece of what is written put on the line that is `context`
static void actions_put(void *context, const char *bytes, unsigned long long size)
{
    cli_line_counted((CliLine *)context, bytes, size);
}

// what a line is handed let go, for an answer nothing reads
static int actions_dropped(void *context, const char *bytes, unsigned long long size)
{
    (void)context;
    (void)bytes;
    (void)size;
    return 1;
}

// a place of an edit orior_cli wrote, `{"line", "col"}`, written again: 1, or 0 with why said where it is none
static int actions_place_written(const CliJson *json, unsigned int edit, const char *name, CliLine *out, CliLine *said)
{
    const unsigned int place = cli_json_member(json, edit, name);
    const unsigned long long none = ~0ull;
    const unsigned long long line = cli_servers_whole(json, cli_json_member(json, place, "line"), none);
    const unsigned long long col = cli_servers_whole(json, cli_json_member(json, place, "col"), none);
    if ((place == CLI_JSON_NONE) || (line > 0xffffffffull) || (col > 0xffffffffull))
    {
        cli_line_text(said, (place == CLI_JSON_NONE) ? "missing field `" : "invalid type: expected field `");
        cli_line_text(said, (place == CLI_JSON_NONE) ? name : ((line > 0xffffffffull) ? "line" : "col"));
        cli_line_character(said, '`');
        return 0;
    }
    cli_text_place_written((CliTextPlace){(unsigned int)line, (unsigned int)col}, out);
    return 1;
}

// orior's own edits of a fix written as the file's edits: 1, or 0 with why said where they are not edits
static int actions_ours_written(const char *tree, const char *path, const CliJson *json, unsigned int edits,
                                CliLine *out, CliLine *said)
{
    if (!cli_json_is(json, edits, CFG_JSON_ARRAY))
    {
        cli_line_text(said, "invalid type: expected a sequence");
        return 0;
    }
    char shown[CLI_PATH_LONGEST];
    CliLine where = {shown, sizeof(shown), 0ull};
    cli_servers_tree_path(tree, path, &where);
    cli_line_text(out, "[{\"path\":");
    cli_json_quoted_lossy(out, (const unsigned char *)shown, cli_line_fits(&where) ? where.at : 0ull);
    cli_line_text(out, ",\"edits\":[");
    for (unsigned int edit = json->node[edits].first; edit != CLI_JSON_NONE; edit = json->node[edit].next)
    {
        unsigned long long size = 0ull;
        const char *const text = cli_json_string(json, cli_json_member(json, edit, "text"), &size);
        cli_line_text(out, (edit != json->node[edits].first) ? ",{\"from\":" : "{\"from\":");
        if (!actions_place_written(json, edit, "from", out, said))
        {
            return 0;
        }
        cli_line_text(out, ",\"to\":");
        if (!actions_place_written(json, edit, "to", out, said))
        {
            return 0;
        }
        if (text == NULL)
        {
            cli_line_text(said, "missing field `text`");
            return 0;
        }
        cli_line_text(out, ",\"text\":");
        cli_json_quoted(out, text, size);
        cli_line_character(out, '}');
    }
    cli_line_text(out, "]}]");
    return 1;
}

// the server's command `command`, named by `name` with its `arguments`, run: by the server where it runs it, and passed
// to the plugin `plugin` where the server is a plugin's and does not: 1, or 0 with why said
static int actions_command(CliLsp *server, const char *plugin, const CliJson *json, unsigned int command,
                           const char *name, CliLine *said)
{
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine params;
    cli_pieces_begun(&pieces, &params, piece, sizeof(piece));
    cli_line_text(&params, "{\"command\":");
    cli_servers_member_written(json, command, name, &params);
    cli_line_text(&params, ",\"arguments\":");
    cli_servers_member_written(json, command, "arguments", &params);
    cli_line_character(&params, '}');
    unsigned long long length = 0ull;
    char *const asked = cli_pieces_joined(&pieces, &params, "the params a command is run with", &length);
    if (asked == NULL)
    {
        cli_line_text(said, "the command could not be written: the cells cannot meet it");
    }
    const char *const named = cli_json_string(json, cli_json_member(json, command, name), NULL);
    const int passed = (plugin != NULL) && ((named == NULL) || !cli_servers_capable_command(server, named));
    if (passed && (asked != NULL))
    {
        cli_servers_plugin_command(plugin, asked, length);
    }
    // what the command answers is read by nothing, so each piece of it is let go as it comes
    char ran[256];
    CliLine result = {ran, sizeof(ran), 0ull, actions_dropped, NULL, 0ull};
    const int done = (asked != NULL) && (passed || cli_lsp_requested(server, "workspace/executeCommand", asked, length,
                                                                     CLI_SERVERS_SEARCHING, &result, said));
    cli_cell_given(asked);
    return done;
}

// the action at `action` of `json`, `size` bytes of `raw` as the server gave it, carried out by `server`, the server of
// the plugin `plugin` where that is not NULL: its edits written to `out`, filled in by the server first where it left
// them out, and its command run: 1, or 0 with why said
static int actions_by(CliLsp *server, const char *plugin, const char *tree, const CliJson *json, unsigned int action,
                      const char *raw, unsigned long long size, CliLine *out, CliLine *said)
{
    if (cli_json_is(json, cli_json_member(json, action, "command"), CFG_JSON_STRING))
    {
        const int done = actions_command(server, plugin, json, action, "command", said);
        cli_line_text(out, done ? "[]" : "");
        return done;
    }
    int done = 1;
    const unsigned int edit = cli_json_member(json, action, "edit");
    const unsigned int command = cli_json_member(json, action, "command");
    if (((edit == CLI_JSON_NONE) || cli_json_is(json, edit, CFG_JSON_NULL)) &&
        ((command == CLI_JSON_NONE) || cli_json_is(json, command, CFG_JSON_NULL)) &&
        cli_servers_capable_true(server, "codeActionProvider", "resolveProvider"))
    {
        CliLine result;
        cli_servers_answer_begun(&s_resolved, &result);
        done = cli_lsp_requested(server, "codeAction/resolve", raw, size, CLI_SERVERS_ASKING, &result, said);
        json = &s_resolved.json;
        action = done ? cli_servers_answer_read(&s_resolved, &result) : CLI_JSON_NONE;
    }
    // the edits held in pieces until the command after them has run, and written only where it ran
    char piece[CLI_SERVERS_PIECE];
    CliPieces held;
    CliLine edits;
    cli_pieces_begun(&held, &edits, piece, sizeof(piece));
    if (done)
    {
        cli_servers_workspace_edit_of(json, cli_json_member(json, action, "edit"), tree, &edits);
        done = cli_pieces_ended(&held, &edits);
        if (!done)
        {
            cli_line_text(said, "the action's edits could not be held: the cells cannot meet them");
        }
    }
    const unsigned int then = cli_json_member(json, action, "command");
    if (done && cli_json_is(json, then, CFG_JSON_OBJECT))
    {
        done = actions_command(server, plugin, json, then, "command", said);
    }
    if (done)
    {
        cli_pieces_each(&held, actions_put, out);
    }
    cli_pieces_given(&held);
    return done;
}

// an action of a plugin's server, `{"orior_plugin", "action"}` at `wrapped`, carried out by the plugin's server: 1, or
// 0 with why said
static int actions_plugin_by(const char *tree, const CliJson *json, unsigned int wrapped, CliLine *out, CliLine *said)
{
    const char *const plugin = cli_json_string(json, cli_json_member(json, wrapped, "orior_plugin"), NULL);
    CliLsp *const server = (plugin != NULL) ? cli_servers_plugin_of(plugin, said) : NULL;
    if (server == NULL)
    {
        cli_line_text(said, (plugin == NULL) ? "invalid type: expected a string `orior_plugin`" : "");
        return 0;
    }
    // the action as the server gave it, for the server to fill in
    const unsigned int action = cli_json_member(json, wrapped, "action");
    CliLine text = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_line_growing(&text, CLI_SERVERS_PIECE, "a plugin server's action");
    cli_json_write(json, action, &text, 0u, 0);
    const int done =
        cli_line_fits(&text) && actions_by(server, plugin, tree, json, action, text.out, text.at, out, said);
    cli_cell_given(text.out);
    cli_lsp_given(server);
    return done;
}

int cli_servers_act(const char *tree, const char *path, const char *raw, unsigned long long size, CliLine *out,
                    CliLine *said)
{
    cli_servers_reading_held(&s_reading);
    cli_servers_reading_held(&s_resolved);
    const CliJson *const json = &s_reading.json;
    const unsigned int action = cli_servers_read(&s_reading, raw, size);
    int done = 1;
    if (cli_json_member(json, action, "orior_run") != CLI_JSON_NONE)
    {
        cli_line_text(out, "[]");
    }
    else if (cli_json_member(json, action, "orior") != CLI_JSON_NONE)
    {
        done = actions_ours_written(tree, path, json, cli_json_member(json, action, "orior"), out, said);
    }
    else if (cli_json_member(json, action, "orior_plugin") != CLI_JSON_NONE)
    {
        done = actions_plugin_by(tree, json, action, out, said);
    }
    else
    {
        CliLsp *const server = cli_servers_holding(path);
        done = (server != NULL) && actions_by(server, NULL, tree, json, action, raw, size, out, said);
        if (server == NULL)
        {
            cli_line_text(said, "no language server has this file open");
        }
        else
        {
            cli_lsp_given(server);
        }
    }
    cli_servers_reading_released(&s_resolved);
    cli_servers_reading_released(&s_reading);
    return done;
}
