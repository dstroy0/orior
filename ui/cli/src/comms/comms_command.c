// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_command.c: the catalog's Comms commands run by name, each on the request the shown view's cursor stands in
// where it takes one, so that the window reaches all of them through one entry
#include "../command_line/command_line.h"
#include "../editor/view.h"
#include "../machine/net.h"
#include "../requests/requests.h"
#include "../text/json_tree.h"
#include "comms_internal.h"

// what a command chooses: the view it shows, or none
#define COMMAND_NO_VIEW 0xffffffffu

// A command of the Comms pane: its name, and the view of the pane it shows, COMMAND_NO_VIEW for none
typedef struct
{
    const char *name;
    unsigned int view;
} CommsCommand;

// The view shown: its key and its path, each a copy out of the views table in a buffer of the record's own, NULL where
// no view is shown, and its cursor's line
typedef struct
{
    const char *key;
    const char *path;
    unsigned long long line;
    char key_text[CLI_NAME_MAX_LEN];
    char path_text[CLI_PATH_MAX_LEN];
} CommsShown;

static const CommsCommand s_commands[] = {
    {"send-request", CLI_COMMS_VIEW_REQUESTS}, {"copy-as-curl", COMMAND_NO_VIEW},
    {"private-environment", COMMAND_NO_VIEW},  {"network", CLI_COMMS_VIEW_NETWORK},
    {"network-off", COMMAND_NO_VIEW},          {"report", CLI_COMMS_VIEW_REPORT},
    {"remote-rejoin", COMMAND_NO_VIEW},        {"remote-copy-address", COMMAND_NO_VIEW},
};

typedef struct
{
    const char *path;
    VerbaLinea *out;
    CommsShown *shown_out;
} CommsCommandCtx;

// the shown view's key, its path and its cursor's line into `shown_out`: 1, or 0 where no view is shown
EMBED_INLINE int command_shown(const CommsCommandCtx *args)
{
    CommsShown *const shown_out = args->shown_out;
    const int shown = cli_view_shown();
    if (shown < 0)
    {
        return 0;
    }
    // the view shown is among the views: its key and path copied into the shown record's own buffers
    const int named = cli_view_key((unsigned int)shown, shown_out->key_text) &&
                      cli_view_path_at((unsigned int)shown, shown_out->path_text);
    shown_out->key = named ? shown_out->key_text : NULL;
    shown_out->path = named ? shown_out->path_text : NULL;
    CliPosition cursor = {0ULL, 0ULL};
    const int placed = (shown_out->key != NULL) && EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg,
                                                              .key = shown_out->key, .cursor = &cursor);
    shown_out->line = cursor.line;
    return placed && (shown_out->path != NULL);
}

// the reader's private environment file beside the file at `path` written into `out` as its path in the tree, made
// where none is: 1, or 0 where it could not be
EMBED_INLINE int command_private(const CommsCommandCtx *args)
{
    char written[CLI_PATH_MAX_LEN + 8u];
    VerbaLinea quoted = {written, sizeof(written), 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_requests_text_table.private_init, CliRequestsTextCfg, .root = cli_command_line_root(),
                    .path = args->path, .out = &quoted, .err_msg = &err_msg) ||
        !mmgr_line_ok(&quoted))
    {
        return 0;
    }
    // the path is written as a JSON string: its letters read through a tree of this call alone
    CliJson read = {0};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &read,
                   .text = written, .size = quoted.at, .err_msg = &err_msg, .owner = g_cli_comms_owner);
    unsigned long long length = 0ULL;
    const char *const letters = (root != CLI_JSON_NONE)
                                    ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                 .json_read = &read, .node = root, .length_pointer = &length)
                                    : NULL;
    if (letters != NULL)
    {
        (void)mmgr_line_put_n(args->out, letters, length);
    }
    g_cli_guard->persistent_buf_release(read.node, g_cli_comms_owner);
    return letters != NULL;
}

int cli_comms_command_run(const CliCommsCommandCfg *args)
{
    unsigned int found = sizeof(s_commands) / sizeof(s_commands[0]);
    for (unsigned int at = 0u; at < sizeof(s_commands) / sizeof(s_commands[0]); at += 1u)
    {
        found =
            cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_commands[at].name, .cap = 32ULL}) ? at : found;
    }
    if (found == sizeof(s_commands) / sizeof(s_commands[0]))
    {
        return 0;
    }
    const CommsCommand *const command = &s_commands[found];
    if (command->view != COMMAND_NO_VIEW)
    {
        EMBED_CALL(cli_comms_pane.choose, CliCommsPaneCfg, .view = command->view);
        *args->shown_out = 1;
    }
    CommsShown shown = {NULL, NULL, 0ULL};
    const int on_view = EMBED_CALL(command_shown, CommsCommandCtx, .shown_out = &shown);
    const char *const root = cli_command_line_root();
    switch (found)
    {
    case 0u:
        if (on_view)
        {
            (void)EMBED_CALL(cli_requests_pane.send, CliRequestsPaneCfg, .root = root, .path = shown.path,
                             .key = shown.key, .line = shown.line);
        }
        return 1;
    case 1u:
        if (on_view)
        {
            (void)EMBED_CALL(cli_requests_pane.curl, CliRequestsPaneCfg, .root = root, .path = shown.path,
                             .key = shown.key, .line = shown.line, .out = args->clipboard);
        }
        return 1;
    case 2u:
        if (on_view)
        {
            (void)EMBED_CALL(command_private, CommsCommandCtx, .path = shown.path, .out = args->open_out);
        }
        return 1;
    case 4u: {
        // on or off by its word, else turned from what it is
        const char *const word = (args->count != 0u) ? args->words[0] : "";
        const int on = cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "on", .cap = 8ULL});
        const int off = cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "off", .cap = 8ULL});
        EMBED_CALL(cli_net.disable, CliNetCfg, .off = on || (!off && !cli_net_is_disable()));
        return 1;
    }
    default:
        return 1;
    }
}
