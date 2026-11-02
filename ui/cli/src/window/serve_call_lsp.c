// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_lsp.c: the calls of the language servers and of what reads the code: a file opened, changed and closed,
// and the view the page rests on; a hover, the definitions, the completions, the usages, a rename, a signature, the
// quick fixes and an action carried out; a file's colors, spans and hints; the call hierarchy; the tree's check started
// and stopped; the checkers named; a plugin's server started and stopped, queried, notified, and its own request
// answered; and a plugin's secrets stored in the system's keychain of the machine its server runs on, and read back.
// Each path is written as it stands in the tree
#include "../editor/document.h"
#include "../editor/files.h"
#include "serve_internal.h"

// the bytes an argument handed to a server is written in, and the most lines a view's openers name
#define CLI_SERVE_LSP_OPENERS 64u

typedef struct
{
    const CliServeArgs *request;
    char *root;
    char *full;
    VerbaLinea *err_msg;
    unsigned int *line;
    unsigned int *col;
    const char *key;
    VerbaLinea *line_verba_linea_pointer;
    VerbaLinea *json;
    char *service;
} ServeCallLspCtx;
// the tree open into `root` and the file of the argument `path` joined to it into `full`, the servers informed where
// what they send goes: 1, or 0 with why given
EMBED_INLINE int lsp_file(const ServeCallLspCtx *args)
{
    const CliServeArgs *arguments = args->request;
    char *root = args->root;
    char *full = args->full;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                      .root = root, .full = full, .err_msg = err_msg);
}

// the arguments `line` and `col`: 1, or 0 with why given
EMBED_INLINE int lsp_line_col(const ServeCallLspCtx *args)
{
    const CliServeArgs *arguments = args->request;
    unsigned int *line = args->line;
    unsigned int *col = args->col;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long read_line = 0ull;
    unsigned long long read_col = 0ull;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "line",
                    .value = &read_line, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "col",
                    .value = &read_col, .err_msg = err_msg))
    {
        return 0;
    }
    // a place is cut to its low words, as the protocol counts them
    *line = (unsigned int)read_line;
    *col = (unsigned int)read_col;
    return 1;
}

static int lsp_open(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const language = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                            .key = "language", .err_msg = err_msg);
    const char *const text = (language != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                             .request = arguments, .key = "text", .err_msg = err_msg)
                                                : NULL;
    if ((text == NULL) ||
        !EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full, .err_msg = err_msg))
    {
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &length);
    const CliServersSink emit = EMBED_CALL(cli_serve_internal.emitter, CliServeCtx, .tree = root);
    return EMBED_CALL(cli_servers_root.open, CliServersRootCfg, .root = root, .path = full, .language = language,
                      .text = text, .size = length, .sink = &emit, .out = out, .err_msg = err_msg);
}

static int lsp_change(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const text = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "text", .err_msg = err_msg);
    if ((text == NULL) ||
        !EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full, .err_msg = err_msg))
    {
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &length);
    if (!EMBED_CALL(cli_servers_root.change, CliServersRootCfg, .path = full, .text = text, .size = length,
                    .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

// the view the page rests on: its first and last lines, the lines opening the regions its first line stands inside,
// outermost first, read off the branch under `key` as its lines were colored
static int lsp_view(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if ((key == NULL) ||
        !EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "first",
                    .value = &first, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "last",
                    .value = &last, .err_msg = err_msg))
    {
        return 0;
    }
    unsigned int opener[CLI_SERVE_LSP_OPENERS];
    // a line of a text the page holds is below 2^32
    const unsigned int openers =
        EMBED_CALL(cli_document_key_line.openers, CliDocumentKeyLineCfg, .key = key, .line = (unsigned int)first,
                   .opener = opener, .most_u32 = CLI_SERVE_LSP_OPENERS);
    return EMBED_CALL(cli_servers_path_out_err_msg.view, CliServersPathOutErrMsgCfg, .path = full,
                      .first = (unsigned int)first, .last = (unsigned int)last, .opener = opener, .openers = openers,
                      .out = out, .err_msg = err_msg);
}

static int lsp_close(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full, .err_msg = err_msg) ||
        !EMBED_CALL(cli_servers_path_out_err_msg_from_to.close, CliServersPathOutErrMsgFromToCfg, .path = full,
                    .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

static int lsp_hover_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(lsp_line_col, ServeCallLspCtx, .request = arguments, .line = &line, .col = &col,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out.hover, CliServersPathOutCfg, .path = full, .line = line, .col = col,
                      .out = out, .err_msg = err_msg);
}

static int lsp_definition_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(lsp_line_col, ServeCallLspCtx, .request = arguments, .line = &line, .col = &col,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out.definition, CliServersPathOutCfg, .tree = root, .path = full, .line = line,
                      .col = col, .out = out, .err_msg = err_msg);
}

static int lsp_complete_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(lsp_line_col, ServeCallLspCtx, .request = arguments, .line = &line, .col = &col,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out.complete, CliServersPathOutCfg, .path = full, .line = line, .col = col,
                      .out = out, .err_msg = err_msg);
}

static int lsp_references_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(lsp_line_col, ServeCallLspCtx, .request = arguments, .line = &line, .col = &col,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out.references, CliServersPathOutCfg, .tree = root, .path = full, .line = line,
                      .col = col, .out = out, .err_msg = err_msg);
}

static int lsp_renamable_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(lsp_line_col, ServeCallLspCtx, .request = arguments, .line = &line, .col = &col,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out.renamable, CliServersPathOutCfg, .path = full, .line = line, .col = col,
                      .out = out, .err_msg = err_msg);
}

static int lsp_rename_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned int line = 0u;
    unsigned int col = 0u;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    return (name != NULL) &&
           EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(lsp_line_col, ServeCallLspCtx, .request = arguments, .line = &line, .col = &col,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out.rename, CliServersPathOutCfg, .tree = root, .path = full, .line = line,
                      .col = col, .name = name, .out = out, .err_msg = err_msg);
}

static int lsp_signature_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(lsp_line_col, ServeCallLspCtx, .request = arguments, .line = &line, .col = &col,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out.signature, CliServersPathOutCfg, .path = full, .line = line, .col = col,
                      .out = out, .err_msg = err_msg);
}

static int lsp_actions_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    CliTextPlace from = {0u, 0u};
    CliTextPlace to = {0u, 0u};
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.place, CliServeErrMsgCtx, .request = arguments, .key = "from",
                      .place = &from, .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.place, CliServeErrMsgCtx, .request = arguments, .key = "to",
                      .place = &to, .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out_err_msg_from.actions, CliServersPathOutErrMsgFromCfg, .path = full,
                      .from = from, .to = to, .out = out, .err_msg = err_msg);
}

// the argument `key` written as JSON into `line`, null where it is not given, the line given back with
// g_cli_cells->persistent_buf_release(line->out, &g_cli_window_cells) either way: 1, or 0 with why given where the
// cellblock could not meet it
EMBED_INLINE int lsp_json_write(const ServeCallLspCtx *args)
{
    const CliServeArgs *arguments = args->request;
    const char *key = args->key;
    VerbaLinea *line = args->line_verba_linea_pointer;
    const unsigned long long room =
        EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = arguments->json);
    char *const cell = g_cli_cells->temporary_buf_alloc(room, &g_cli_window_cells);
    *line = mmgr_line_over(cell, room);
    const unsigned int given =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = key);
    if (given != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = arguments->json,
                   .node = given, .out = line, .indent = 0u, .sorted = 0);
    }
    else
    {
        mmgr_line_put(line, "null");
    }
    if (!mmgr_line_ok(line))
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet the argument ");
        mmgr_line_put(args->err_msg, key);
        return 0;
    }
    return 1;
}

static int lsp_act_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full, .err_msg = err_msg))
    {
        return 0;
    }
    VerbaLinea raw = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    const int done = EMBED_CALL(lsp_json_write, ServeCallLspCtx, .request = arguments, .key = "raw",
                                .line_verba_linea_pointer = &raw, .err_msg = err_msg) &&
                     EMBED_CALL(cli_servers_path_size.act, CliServersPathSizeCfg, .tree = root, .path = full,
                                .raw = raw.out, .size = raw.at, .out = out, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(raw.out, &g_cli_window_cells);
    return done;
}

static int parse_colors_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    // the lines are cut to their low words, as no file has more
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "from",
                      .value = &from, .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "to",
                      .value = &to, .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out_err_msg_from_to.colors, CliServersPathOutErrMsgFromToCfg, .path = full,
                      .from_u32 = (unsigned int)from, .to_u32 = (unsigned int)to, .out = out, .err_msg = err_msg);
}

static int parse_spans_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    CliTextPlace from = {0u, 0u};
    CliTextPlace to = {0u, 0u};
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .otherwise = NULL);
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.place, CliServeErrMsgCtx, .request = arguments, .key = "from",
                      .place = &from, .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.place, CliServeErrMsgCtx, .request = arguments, .key = "to",
                      .place = &to, .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out_err_msg_from.spans, CliServersPathOutErrMsgFromCfg, .path = full, .key = key,
                      .from = from, .to = to, .out = out, .err_msg = err_msg);
}

static int lsp_hints_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    // the lines are cut to their low words, as no file has more
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "from",
                      .value = &from, .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "to",
                      .value = &to, .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out_err_msg_from_to.hints, CliServersPathOutErrMsgFromToCfg, .path = full,
                      .from_u32 = (unsigned int)from, .to_u32 = (unsigned int)to, .out = out, .err_msg = err_msg);
}

static int calls_root_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned int line = 0u;
    unsigned int col = 0u;
    return EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full,
                      .err_msg = err_msg) &&
           EMBED_CALL(lsp_line_col, ServeCallLspCtx, .request = arguments, .line = &line, .col = &col,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_servers_path_out.call_root, CliServersPathOutCfg, .tree = root, .path = full, .line = line,
                      .col = col, .out = out, .err_msg = err_msg);
}

static int calls_of_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    int incoming = 0;
    if (!EMBED_CALL(lsp_file, ServeCallLspCtx, .request = arguments, .root = root, .full = full, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments, .key = "incoming",
                    .value_i32s = &incoming, .err_msg = err_msg))
    {
        return 0;
    }
    VerbaLinea item = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    const int done = EMBED_CALL(lsp_json_write, ServeCallLspCtx, .request = arguments, .key = "item",
                                .line_verba_linea_pointer = &item, .err_msg = err_msg) &&
                     EMBED_CALL(cli_servers_path.calls, CliServersPathCfg, .tree = root, .path = full, .item = item.out,
                                .size = item.at, .incoming = incoming, .out = out, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(item.out, &g_cli_window_cells);
    return done;
}

static int problems_check(const CliServeAnswerCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    CliServeFiles *const listed = EMBED_CALL(cli_serve_err_msg_root_internal.files_list, CliServeErrMsgRootCtx,
                                             .root_read = root, .err_msg = err_msg);
    if (listed == NULL)
    {
        return 0;
    }
    const CliServersSink emit = EMBED_CALL(cli_serve_internal.emitter, CliServeCtx, .tree = root);
    mmgr_line_decimal(args->out,
                      EMBED_CALL(cli_servers_root_sink_count.check_tree, CliServersRootSinkCountCfg, .root = root,
                                 .files = listed->paths, .count = listed->count, .sink = &emit),
                      1u);
    EMBED_CALL(cli_serve_listed_internal.files_free, CliServeListedCtx, .listed = listed);
    return 1;
}

static int problems_check_stop(const CliServeAnswerCfg *args)
{
    cli_servers_stop_check();
    mmgr_line_put(args->out, "null");
    return 1;
}

static int checkers_set(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static const char *s_names[CLI_SERVERS_CHECKERS];
    unsigned int count = 0u;
    if (!EMBED_CALL(cli_serve_err_msg_internal.strings, CliServeErrMsgCtx, .request = arguments, .key = "names",
                    .items = s_names, .room = CLI_SERVERS_CHECKERS, .count = &count, .err_msg = err_msg))
    {
        return 0;
    }
    char root[CLI_PATH_MAX_LEN];
    const int has_root = EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = root);
    // with no tree open what the checkers find goes nowhere
    const CliServersSink emit =
        has_root ? EMBED_CALL(cli_serve_internal.emitter, CliServeCtx, .tree = root) : (CliServersSink){NULL, NULL, ""};
    return EMBED_CALL(cli_servers_root_sink_out.set_checkers, CliServersRootSinkOutCfg, .names = s_names,
                      .count = count, .root = has_root ? root : NULL, .sink = &emit, .out = out, .err_msg = err_msg);
}

// the argument `key` written as JSON into `json`, a line given back by the caller, `null` where there is none: 1, or 0
// with why given where the cells cannot meet it
EMBED_INLINE int plugin_json_get(const ServeCallLspCtx *args)
{
    const CliServeArgs *arguments = args->request;
    const char *key = args->key;
    VerbaLinea *json = args->json;
    const unsigned long long room =
        EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = arguments->json);
    char *const cell = g_cli_cells->temporary_buf_alloc(room, &g_cli_window_cells);
    *json = mmgr_line_over(cell, room);
    const unsigned int node =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = key);
    if (node == CLI_JSON_NONE)
    {
        mmgr_line_put(json, "null");
    }
    else
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = arguments->json, .node = node,
                   .out = json, .indent = 0u, .sorted = 0);
    }
    if (!mmgr_line_ok(json))
    {
        mmgr_line_put(args->err_msg, "the cells cannot meet what the plugin hands its server");
        return 0;
    }
    return 1;
}

static int plugin_server_start(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    char root[CLI_PATH_MAX_LEN];
    if ((id == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                    .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    // the arguments the plugin puts before its plugin.json's, a Java machine's own among them
    const char *first[64];
    unsigned int firsts = 0u;
    VerbaLinea options;
    const int read = EMBED_CALL(plugin_json_get, ServeCallLspCtx, .request = arguments, .key = "options",
                                .json = &options, .err_msg = err_msg) &&
                     EMBED_CALL(cli_serve_err_msg_internal.strings_or, CliServeErrMsgCtx, .request = arguments,
                                .key = "args", .items = first, .room = 64u, .count = &firsts, .err_msg = err_msg);
    const CliServersSink emit = EMBED_CALL(cli_serve_internal.emitter, CliServeCtx, .tree = root);
    const int started = read && EMBED_CALL(cli_servers_root_sink.plugin_start, CliServersRootSinkCfg, .root = root,
                                           .id = id, .options = options.out, .length = options.at,
                                           .first_letters = first, .firsts = firsts, .sink = &emit, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(options.out, &g_cli_window_cells);
    mmgr_line_put(args->out, "null");
    return started;
}

static int plugin_server_stop(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    if (id == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_servers_root_sink.plugin_stop, CliServersRootSinkCfg, .id = id);
    mmgr_line_put(args->out, "null");
    return 1;
}

static int plugin_server_request_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    const char *const method = (id != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "method", .err_msg = err_msg)
                                            : NULL;
    VerbaLinea params;
    if ((method == NULL) || !EMBED_CALL(plugin_json_get, ServeCallLspCtx, .request = arguments, .key = "params",
                                        .json = &params, .err_msg = err_msg))
    {
        return 0;
    }
    const int done = EMBED_CALL(cli_servers_err_msg.plugin_request, CliServersErrMsgCfg, .id = id, .method = method,
                                .params = params.out, .length = params.at,
                                .wait = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                                   .request = arguments, .key = "wait", .otherwise_u64 = 30000ull),
                                .result = args->out, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(params.out, &g_cli_window_cells);
    return done;
}

static int plugin_server_notify(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    const char *const method = (id != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "method", .err_msg = err_msg)
                                            : NULL;
    VerbaLinea params;
    if ((method == NULL) || !EMBED_CALL(plugin_json_get, ServeCallLspCtx, .request = arguments, .key = "params",
                                        .json = &params, .err_msg = err_msg))
    {
        return 0;
    }
    const int done = EMBED_CALL(cli_servers_err_msg.plugin_notify, CliServersErrMsgCfg, .id = id, .method = method,
                                .params = params.out, .length = params.at, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(params.out, &g_cli_window_cells);
    mmgr_line_put(args->out, "null");
    return done;
}

static int plugin_server_answer(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    VerbaLinea request;
    VerbaLinea result;
    if ((id == NULL) || !EMBED_CALL(plugin_json_get, ServeCallLspCtx, .request = arguments, .key = "request",
                                    .json = &request, .err_msg = err_msg))
    {
        return 0;
    }
    const int read = EMBED_CALL(plugin_json_get, ServeCallLspCtx, .request = arguments, .key = "result",
                                .json = &result, .err_msg = err_msg);
    // the request's id as the server gave it, a number or a string, ended with a 0 to be written back as it came
    mmgr_line_ch(&request, '\0');
    const int done =
        read && mmgr_line_ok(&request) &&
        EMBED_CALL(cli_servers_err_msg_id.plugin_answer, CliServersErrMsgIdCfg, .id = id, .request = request.out,
                   .result_text = result.out, .length = result.at, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(result.out, &g_cli_window_cells);
    g_cli_cells->persistent_buf_release(request.out, &g_cli_window_cells);
    mmgr_line_put(args->out, "null");
    return done;
}

// the keychain's service for the plugin of the argument `id`, "plugin " and the id, into `service`, sized for
// CLI_NAME_MAX_LEN: 1, or 0 with why given
EMBED_INLINE int plugin_service(const ServeCallLspCtx *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    VerbaLinea named = {args->service, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&named, "plugin ");
    mmgr_line_put(&named, (id != NULL) ? id : "");
    return (id != NULL) && (mmgr_line_finish(&named) != 0ull);
}

static int plugin_secret_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char service[CLI_NAME_MAX_LEN];
    const char *const key =
        EMBED_CALL(plugin_service, ServeCallLspCtx, .request = arguments, .service = service, .err_msg = err_msg)
            ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments, .key = "key",
                         .err_msg = err_msg)
            : NULL;
    if (key == NULL)
    {
        return 0;
    }
    char secret[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_service_internal.keychain_read, CliServiceCtx, .service = service, .account = key, .out = secret,
                   .room = sizeof(secret)))
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)secret,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = secret, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    return 1;
}

static int plugin_secret_write(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char service[CLI_NAME_MAX_LEN];
    const char *const key =
        EMBED_CALL(plugin_service, ServeCallLspCtx, .request = arguments, .service = service, .err_msg = err_msg)
            ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments, .key = "key",
                         .err_msg = err_msg)
            : NULL;
    if (key == NULL)
    {
        return 0;
    }
    // a value of null forgets the secret
    const char *const value = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                         .key = "value", .otherwise = NULL);
    const int done = (value != NULL)
                         ? EMBED_CALL(cli_err_msg_service_internal.keychain_store, CliErrMsgServiceCtx,
                                      .service = service, .account = key, .secret = value, .err_msg = err_msg)
                         : EMBED_CALL(cli_err_msg_service_internal.keychain_forget, CliErrMsgServiceCtx,
                                      .service = service, .account = key, .err_msg = err_msg);
    EMBED_CALL(cli_serve_err_msg_internal.secret_wiped, CliServeErrMsgCtx, .request = arguments, .key = "value");
    mmgr_line_put(args->out, "null");
    return done;
}

// each request of a server is answered later where the page requests so
CLI_SERVE_DEFERRED(lsp_hover, lsp_hover_now)
CLI_SERVE_DEFERRED(lsp_definition, lsp_definition_now)
CLI_SERVE_DEFERRED(lsp_complete, lsp_complete_now)
CLI_SERVE_DEFERRED(lsp_references, lsp_references_now)
CLI_SERVE_DEFERRED(lsp_renamable, lsp_renamable_now)
CLI_SERVE_DEFERRED(lsp_rename, lsp_rename_now)
CLI_SERVE_DEFERRED(lsp_signature, lsp_signature_now)
CLI_SERVE_DEFERRED(lsp_actions, lsp_actions_now)
CLI_SERVE_DEFERRED(lsp_act, lsp_act_now)
CLI_SERVE_DEFERRED(parse_colors, parse_colors_now)
CLI_SERVE_DEFERRED(parse_spans, parse_spans_now)
CLI_SERVE_DEFERRED(lsp_hints, lsp_hints_now)
CLI_SERVE_DEFERRED(calls_root, calls_root_now)
CLI_SERVE_DEFERRED(calls_get, calls_of_now)
CLI_SERVE_DEFERRED(plugin_server_request, plugin_server_request_now)

const CliServeCall g_cli_serve_lsp_calls[] = {
    {"lsp_open", lsp_open},
    {"lsp_change", lsp_change},
    {"lsp_view", lsp_view},
    {"lsp_close", lsp_close},
    {"lsp_hover", lsp_hover},
    {"lsp_definition", lsp_definition},
    {"lsp_complete", lsp_complete},
    {"lsp_references", lsp_references},
    {"lsp_renamable", lsp_renamable},
    {"lsp_rename", lsp_rename},
    {"lsp_signature", lsp_signature},
    {"lsp_actions", lsp_actions},
    {"lsp_act", lsp_act},
    {"parse_colors", parse_colors},
    {"parse_spans", parse_spans},
    {"lsp_hints", lsp_hints},
    {"calls_root", calls_root},
    {"calls_of", calls_get},
    {"problems_check", problems_check},
    {"problems_check_stop", problems_check_stop},
    {"checkers_set", checkers_set},
    {"plugin_server_start", plugin_server_start},
    {"plugin_server_stop", plugin_server_stop},
    {"plugin_server_request", plugin_server_request},
    {"plugin_server_notify", plugin_server_notify},
    {"plugin_server_answer", plugin_server_answer},
    {"plugin_secret_read", plugin_secret_read},
    {"plugin_secret_write", plugin_secret_write},
};
const unsigned int g_cli_serve_lsp_count = sizeof(g_cli_serve_lsp_calls) / sizeof(g_cli_serve_lsp_calls[0]);
