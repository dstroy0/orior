// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_actions.c: the quick fixes and refactorings for a span of a file: the inspections' and the checkers' fixes
// of the findings that touch it first, then what its server offers, given the diagnostics it gave that touch it, a fix
// it offers for a diagnostic and again for its note listed once, those it can carry out first and its preferred fixes
// before the rest; and an action carried out, its edits, filled in by the server first where it left them out, returned
// to be written, and its command run, whose edits the server asks for on its own. A fix that runs a line in the
// terminal is the page's to run
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

// a member of `node` written as JSON, `null` where it has none
static void actions_member_written(const CliJson *json, unsigned int node, const char *name, CliLine *out)
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
                actions_member_written(json, fix, "edits", out);
            }
            cli_line_text(out, "}}");
            *written = 1;
        }
    }
}

// the params that ask the server of `uri` for its actions on `from` to `to`, with its diagnostics that touch it, in a
// cell of their size, their size into `length`: the cell, or NULL where the cells cannot meet them
static char *actions_params(const char *key, const char *uri, CliTextPlace from, CliTextPlace to,
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
    const unsigned int all = actions_kept_read(&g_cli_servers_published, &g_cli_servers_published_lock, key);
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
static void actions_offered(unsigned int all, int *written, CliLine *out)
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
                (void)cli_servers_action_of(json, action[at], out);
                *written = 1;
            }
        }
    }
    cli_cell_given(action);
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
        unsigned long long length = 0ull;
        char *const params = actions_params(key, uri, from, to, &length);
        if (params == NULL)
        {
            cli_line_text(said, "the asking for actions could not be written: the cells cannot meet it");
        }
        CliLine result;
        cli_servers_answer_begun(&s_reading, &result);
        asked = (params != NULL) &&
                cli_lsp_requested(server, "textDocument/codeAction", params, length, CLI_SERVERS_ASKING, &result, said);
        cli_cell_given(params);
        cli_lsp_given(server);
        if (asked)
        {
            actions_offered(cli_servers_answer_read(&s_reading, &result), &written, out);
        }
    }
    else if (!written)
    {
        cli_line_text(said, "no language server has this file open");
        asked = 0;
    }
    cli_line_character(out, ']');
    out->at = asked ? out->at : start;
    cli_servers_reading_released(&s_reading);
    return asked;
}

// a place of an edit orior wrote, `{"line", "col"}`, written again: 1, or 0 with why said where it is none
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

// the server's command `command`, named by `name` with its `arguments`, run: 1, or 0 with why said
static int actions_command(CliLsp *server, const CliJson *json, unsigned int command, const char *name, CliLine *said)
{
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine params;
    cli_pieces_begun(&pieces, &params, piece, sizeof(piece));
    cli_line_text(&params, "{\"command\":");
    actions_member_written(json, command, name, &params);
    cli_line_text(&params, ",\"arguments\":");
    actions_member_written(json, command, "arguments", &params);
    cli_line_character(&params, '}');
    unsigned long long length = 0ull;
    char *const asked = cli_pieces_joined(&pieces, &params, "the params a command is run with", &length);
    if (asked == NULL)
    {
        cli_line_text(said, "the command could not be written: the cells cannot meet it");
    }
    // what the command answers is read by nothing, so each piece of it is let go as it comes
    char ran[256];
    CliLine result = {ran, sizeof(ran), 0ull, actions_dropped, NULL, 0ull};
    const int done = (asked != NULL) && cli_lsp_requested(server, "workspace/executeCommand", asked, length,
                                                          CLI_SERVERS_SEARCHING, &result, said);
    cli_cell_given(asked);
    return done;
}

int cli_servers_act(const char *tree, const char *path, const char *raw, unsigned long long size, CliLine *out,
                    CliLine *said)
{
    cli_servers_reading_held(&s_reading);
    cli_servers_reading_held(&s_resolved);
    const CliJson *json = &s_reading.json;
    unsigned int action = cli_servers_read(&s_reading, raw, size);
    int done = 1;
    if (cli_json_member(json, action, "orior_run") != CLI_JSON_NONE)
    {
        cli_line_text(out, "[]");
    }
    else if (cli_json_member(json, action, "orior") != CLI_JSON_NONE)
    {
        done = actions_ours_written(tree, path, json, cli_json_member(json, action, "orior"), out, said);
    }
    else
    {
        CliLsp *const server = cli_servers_holding(path);
        done = (server != NULL);
        if (server == NULL)
        {
            cli_line_text(said, "no language server has this file open");
        }
        else if (cli_json_is(json, cli_json_member(json, action, "command"), CFG_JSON_STRING))
        {
            done = actions_command(server, json, action, "command", said);
            cli_line_text(out, done ? "[]" : "");
        }
        else
        {
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
                done = actions_command(server, json, then, "command", said);
            }
            if (done)
            {
                cli_pieces_each(&held, actions_put, out);
            }
            cli_pieces_given(&held);
        }
        if (server != NULL)
        {
            cli_lsp_given(server);
        }
    }
    cli_servers_reading_released(&s_resolved);
    cli_servers_reading_released(&s_reading);
    return done;
}
