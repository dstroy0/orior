// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_act.c: a quick fix or refactoring carried out: orior's own edits returned to be written; a server's action,
// its edits filled in by the server first where it left them out, returned to be written, and its command run, whose
// edits the server requests on its own; and an action of a plugin's server carried out by that server, a command it
// does not run itself passed to its plugin. A fix that runs a line in the terminal is the page's to run
#include "../command_line/cells.h"
#include "servers_internal.h"

// what is read, under the reading's lock
static CliServersReading s_reading = {0};
static CliServersReading s_resolved = {0};

typedef struct
{
    const CliJson *json;
    unsigned int edit;
    const char *name;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *tree;
    const char *path;
    unsigned int edits;
    CliLsp *server;
    const char *plugin;
    unsigned int command;
    unsigned int action;
    const char *raw;
    unsigned long long size;
    unsigned int wrapped;
} ServersActCtx;
// a place of an edit orior_cli wrote, `{"line", "col"}`, written again: 1, or 0 with why given where it is none
EMBED_INLINE int actions_place_write(const ServersActCtx *args)
{
    const CliJson *json = args->json;
    const char *name = args->name;
    VerbaLinea *out = args->out;
    const unsigned int place = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->edit, .name = name);
    const unsigned long long none = ~0ull;
    const unsigned long long line =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = place, .name = "line"),
                   .otherwise = none);
    const unsigned long long col =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = place, .name = "col"),
                   .otherwise = none);
    if ((place == CLI_JSON_NONE) || (line > 0xffffffffull) || (col > 0xffffffffull))
    {
        mmgr_line_put(args->err_msg, (place == CLI_JSON_NONE) ? "missing field `" : "invalid type: expected field `");
        mmgr_line_put(args->err_msg, (place == CLI_JSON_NONE) ? name : ((line > 0xffffffffull) ? "line" : "col"));
        mmgr_line_ch(args->err_msg, '`');
        return 0;
    }
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg,
               .place = (CliTextPlace){(unsigned int)line, (unsigned int)col}, .out = out);
    return 1;
}

// orior's own edits of a fix written as the file's edits: 1, or 0 with why given where they are not edits
EMBED_INLINE int actions_ours_write(const ServersActCtx *args)
{
    const char *tree = args->tree;
    const char *path = args->path;
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = args->edits,
                    .kind = CFG_JSON_ARRAY))
    {
        mmgr_line_put(args->err_msg, "invalid type: expected a sequence");
        return 0;
    }
    char shown[CLI_PATH_MAX_LEN];
    VerbaLinea where = {shown, sizeof(shown), 0ull};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = tree, .path = path,
               .out = &where);
    mmgr_line_put(out, "[{\"path\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)shown, .length = mmgr_line_ok(&where) ? where.at : 0ull);
    mmgr_line_put(out, ",\"edits\":[");
    for (unsigned int edit = json->node[args->edits].first; edit != CLI_JSON_NONE; edit = json->node[edit].next)
    {
        unsigned long long size = 0ull;
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = edit, .name = "text"),
                       .length_pointer = &size);
        mmgr_line_put(out, (edit != json->node[args->edits].first) ? ",{\"from\":" : "{\"from\":");
        if (!EMBED_CALL(actions_place_write, ServersActCtx, .json = json, .edit = edit, .name = "from", .out = out,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        mmgr_line_put(out, ",\"to\":");
        if (!EMBED_CALL(actions_place_write, ServersActCtx, .json = json, .edit = edit, .name = "to", .out = out,
                        .err_msg = args->err_msg))
        {
            return 0;
        }
        if (text == NULL)
        {
            mmgr_line_put(args->err_msg, "missing field `text`");
            return 0;
        }
        mmgr_line_put(out, ",\"text\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = text,
                   .length = size);
        mmgr_line_ch(out, '}');
    }
    mmgr_line_put(out, "]}]");
    return 1;
}

// the server's command `command`, named by `name` with its `arguments`, run: by the server where it runs it, and passed
// to the plugin `plugin` where the server is a plugin's and does not: 1, or 0 with why given
static int actions_command(const ServersActCtx *args)
{
    CliLsp *server = args->server;
    const CliJson *json = args->json;
    unsigned int command = args->command;
    const char *name = args->name;
    VerbaLinea *err_msg = args->err_msg;
    // a command and its arguments come in a server's answer, and so fit what an answer holds
    char *const params_text = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_ANSWER, &g_cli_servers_cells);
    VerbaLinea params = mmgr_line_over(params_text, CLI_SERVERS_ANSWER);
    mmgr_line_put(&params, "{\"command\":");
    EMBED_CALL(cli_servers_json_node_internal.member_write, CliServersJsonNodeCtx, .json = json, .node = command,
               .name = name, .out = &params);
    mmgr_line_put(&params, ",\"arguments\":");
    EMBED_CALL(cli_servers_json_node_internal.member_write, CliServersJsonNodeCtx, .json = json, .node = command,
               .name = "arguments", .out = &params);
    mmgr_line_ch(&params, '}');
    const int whole = mmgr_line_ok(&params);
    if (!whole)
    {
        mmgr_line_put(err_msg, "the command could not be written: its params do not fit");
    }
    const char *const named =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = command, .name = name),
                   .length_pointer = NULL);
    const int passed = (args->plugin != NULL) &&
                       ((named == NULL) || !EMBED_CALL(cli_servers_server_command_internal.capable_command,
                                                       CliServersServerCommandCtx, .server = server, .command = named));
    if (passed && whole)
    {
        EMBED_CALL(cli_servers_size_command_internal.plugin_command, CliServersSizeCommandCtx, .id = args->plugin,
                   .command = params.out, .size = params.at);
    }
    // nothing reads the command's answer
    CliLspAnswer answer = {NULL, NULL, 0ull};
    const int done =
        whole && (passed || EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = server,
                                       .method = "workspace/executeCommand", .params = params.out, .length = params.at,
                                       .wait = CLI_SERVERS_SEARCHING, .answer = &answer, .err_msg = err_msg));
    g_cli_cells->persistent_buf_release(answer.message, &g_cli_servers_cells);
    g_cli_cells->persistent_buf_release(params_text, &g_cli_servers_cells);
    return done;
}

// the action at `action` of `json`, `size` bytes of `raw` as the server gave it, carried out by `server`, the server of
// the plugin `plugin` where that is not NULL: its edits written to `out`, filled in by the server first where it left
// them out, and its command run: 1, or 0 with why given
static int actions_by(const ServersActCtx *args)
{
    CliLsp *server = args->server;
    const char *tree = args->tree;
    const CliJson *json = args->json;
    unsigned int action = args->action;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = action, .name = "command"),
                   .kind = CFG_JSON_STRING))
    {
        const int done = EMBED_CALL(actions_command, ServersActCtx, .server = server, .plugin = args->plugin,
                                    .json = json, .command = action, .name = "command", .err_msg = err_msg);
        mmgr_line_put(out, done ? "[]" : "");
        return done;
    }
    int done = 1;
    const unsigned int edit = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = action, .name = "edit");
    const unsigned int command = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = action, .name = "command");
    if (((edit == CLI_JSON_NONE) || EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                               .json_read = json, .node = edit, .kind = CFG_JSON_NULL)) &&
        ((command == CLI_JSON_NONE) || EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                  .json_read = json, .node = command, .kind = CFG_JSON_NULL)) &&
        EMBED_CALL(cli_servers_server_internal.capable_true, CliServersServerCtx, .server = server,
                   .name = "codeActionProvider", .inner = "resolveProvider"))
    {
        CliLspAnswer answer = {NULL, NULL, 0ull};
        done = EMBED_CALL(cli_lsp_err_msg_length.request, CliLspErrMsgLengthCfg, .lsp = server,
                          .method = "codeAction/resolve", .params = args->raw, .length = args->size,
                          .wait = CLI_SERVERS_REQUEST_TIMEOUT_MS, .answer = &answer, .err_msg = err_msg);
        json = &s_resolved.json;
        action = done ? EMBED_CALL(cli_servers_reading_answer_internal.answer_read, CliServersReadingAnswerCtx,
                                   .reading = &s_resolved, .answer = &answer)
                      : CLI_JSON_NONE;
    }
    // the edits saved until the command after them has run, and written only where it ran
    char *const edits_text = g_cli_cells->temporary_buf_alloc(CLI_SERVERS_ANSWER, &g_cli_servers_cells);
    VerbaLinea edits = mmgr_line_over(edits_text, CLI_SERVERS_ANSWER);
    if (done)
    {
        EMBED_CALL(cli_servers_json_tree_out_internal.workspace_edit_get, CliServersJsonTreeOutCtx, .json = json,
                   .edit = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = action, .name = "edit"),
                   .tree = tree, .out = &edits);
        done = mmgr_line_ok(&edits);
        if (!done)
        {
            mmgr_line_put(err_msg, "the action's edits could not be stored: they do not fit");
        }
    }
    const unsigned int then = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = action, .name = "command");
    if (done && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = then,
                           .kind = CFG_JSON_OBJECT))
    {
        done = EMBED_CALL(actions_command, ServersActCtx, .server = server, .plugin = args->plugin, .json = json,
                          .command = then, .name = "command", .err_msg = err_msg);
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
EMBED_INLINE int actions_plugin_by(const ServersActCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const plugin =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->wrapped, .name = "orior_plugin"),
                   .length_pointer = NULL);
    CliLsp *const server = (plugin != NULL) ? EMBED_CALL(cli_servers_err_msg_server_internal.plugin_get,
                                                         CliServersErrMsgServerCtx, .id = plugin, .err_msg = err_msg)
                                            : NULL;
    if (server == NULL)
    {
        mmgr_line_put(err_msg, (plugin == NULL) ? "invalid type: expected a string `orior_plugin`" : "");
        return 0;
    }
    // the action as the server gave it, for the server to fill in
    const unsigned int action = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->wrapped, .name = "action");
    const unsigned long long room =
        EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = json);
    char *const raw = g_cli_cells->temporary_buf_alloc(room, &g_cli_servers_cells);
    VerbaLinea text = mmgr_line_over(raw, room);
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = action,
               .out = &text, .indent = 0u, .sorted = 0);
    const int done =
        mmgr_line_ok(&text) &&
        EMBED_CALL(actions_by, ServersActCtx, .server = server, .plugin = plugin, .tree = args->tree, .json = json,
                   .action = action, .raw = text.out, .size = text.at, .out = out, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(raw, &g_cli_servers_cells);
    EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
    return done;
}

int cli_servers_act(const CliServersPathSizeCfg *args)
{
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_reading);
    EMBED_CALL(cli_servers_reading_answer_internal.reading_lock, CliServersReadingAnswerCtx, .reading = &s_resolved);
    const CliJson *const json = &s_reading.json;
    const unsigned int action = EMBED_CALL(cli_servers_reading_internal.read, CliServersReadingCtx,
                                           .reading = &s_reading, .text = args->raw, .size = args->size);
    int done = 1;
    if (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = action,
                   .name = "orior_run") != CLI_JSON_NONE)
    {
        mmgr_line_put(args->out, "[]");
    }
    else if (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = action,
                        .name = "orior") != CLI_JSON_NONE)
    {
        done = EMBED_CALL(actions_ours_write, ServersActCtx, .tree = args->tree, .path = args->path, .json = json,
                          .edits = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = action, .name = "orior"),
                          .out = args->out, .err_msg = args->err_msg);
    }
    else if (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = action,
                        .name = "orior_plugin") != CLI_JSON_NONE)
    {
        done = EMBED_CALL(actions_plugin_by, ServersActCtx, .tree = args->tree, .json = json, .wrapped = action,
                          .out = args->out, .err_msg = args->err_msg);
    }
    else
    {
        CliLsp *const server =
            EMBED_CALL(cli_servers_path_lsp_internal.owner_start, CliServersPathLspCtx, .path = args->path);
        done = (server != NULL) && EMBED_CALL(actions_by, ServersActCtx, .server = server, .plugin = NULL,
                                              .tree = args->tree, .json = json, .action = action, .raw = args->raw,
                                              .size = args->size, .out = args->out, .err_msg = args->err_msg);
        if (server == NULL)
        {
            mmgr_line_put(args->err_msg, "no language server has this file open");
        }
        else
        {
            EMBED_CALL(cli_lsp_context.unref, CliLspContextCfg, .lsp = server);
        }
    }
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_resolved);
    EMBED_CALL(cli_servers_reading_answer_internal.reading_release, CliServersReadingAnswerCtx, .reading = &s_reading);
    return done;
}
