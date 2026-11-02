// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_args.c: a call's arguments, each found by its own name or by the page's, as the page writes names of more than
// one word, `with_errors` as `withErrors`; one of the wrong kind refused as serde refuses it, its name before why
#include "../command_line/cells.h"
#include "../text/text.h"
#include "serve_internal.h"

void *cli_serve_cell(const CliServeSizeErrMsgCtx *args)
{
    void *const cell = g_cli_cells->persistent_buf_alloc(args->size, &g_cli_serve_strings_cells);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, args->what);
        mmgr_line_put(args->err_msg, ": the cells cannot meet it");
    }
    return cell;
}

CliServeFiles *cli_serve_files_list(const CliServeErrMsgRootCtx *args)
{
    const char *root = args->root_read;
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root);
    if (files == NULL)
    {
        mmgr_line_put(args->err_msg, "the tree's files: the cells cannot meet it");
        return NULL;
    }
    CliServeFiles *const listed =
        EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                   .size = sizeof(CliServeFiles) + ((unsigned long long)files->count * sizeof(const char *)),
                   .what = "the tree's files", .err_msg = args->err_msg);
    if (listed == NULL)
    {
        EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
        return NULL;
    }
    // the pointers lie in the cell past the list, which the cell's alignment and the list's size maintain aligned
    *listed = (CliServeFiles){files, files->count, (const char **)(void *)(listed + 1)};
    for (unsigned int at = 0u; at < listed->count; at += 1u)
    {
        listed->paths[at] = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at);
    }
    return listed;
}

void cli_serve_files_free(const CliServeListedCtx *args)
{
    if (args->listed != NULL)
    {
        EMBED_CALL(cli_files.let, CliFilesCfg, .files = args->listed->files);
        g_cli_cells->persistent_buf_release(args->listed, &g_cli_serve_strings_cells);
    }
}

typedef struct
{
    const char *key;
    char *out;
    unsigned long long room;
} ServeArgsCtx;
// the page's name for `key`: each letter after a `_` in upper case, the `_` left out, into `out`, sized for `room`
EMBED_INLINE void args_camel(const ServeArgsCtx *args)
{
    VerbaLinea line = {args->out, args->room, 0ull};
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL});
    int upper = 0;
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                   .text = args->key, .at = at, .size = size, .set = "_");
        if (stop > at)
        {
            const char first = args->key[at];
            mmgr_line_ch(&line, (upper && (first >= 'a') && (first <= 'z')) ? (char)(first - 'a' + 'A') : first);
            mmgr_line_put_n(&line, args->key + at + 1ull, stop - at - 1ull);
        }
        upper = 1;
        at = stop + 1ull;
    }
    (void)mmgr_line_finish(&line);
}

unsigned int cli_serve_arg(const CliServeErrMsgCtx *args)
{
    const unsigned int found =
        EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->request->json,
                   .object = args->request->object, .name = args->key);
    if (found != CLI_JSON_NONE)
    {
        return found;
    }
    char camel[CLI_NAME_MAX_LEN];
    EMBED_CALL(args_camel, ServeArgsCtx, .key = args->key, .out = camel, .room = sizeof(camel));
    return EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->request->json,
                      .object = args->request->object, .name = camel);
}

// why an argument of the wrong kind is refused, as serde reports it: its name, what it is, and what was expected
void cli_serve_reject(const CliServeErrMsgRequestCtx *args)
{
    static const char *const s_kinds[7] = {"map", "sequence", "string", "number", "boolean `true`", "boolean `false`",
                                           "null"};
    mmgr_line_put(args->err_msg, args->key);
    mmgr_line_put(args->err_msg, ": invalid type: ");
    const CfgJsonKind kind = (args->node == CLI_JSON_NONE) ? CFG_JSON_NULL : args->request->json->node[args->node].kind;
    mmgr_line_put(args->err_msg, s_kinds[kind]);
    if (kind == CFG_JSON_STRING)
    {
        mmgr_line_ch(args->err_msg, ' ');
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->err_msg,
                   .text = args->request->json->text + args->request->json->node[args->node].text,
                   .length = args->request->json->node[args->node].length);
    }
    else if (kind == CFG_JSON_NUMBER)
    {
        mmgr_line_ch(args->err_msg, ' ');
        mmgr_line_put_n(args->err_msg, args->request->json->text + args->request->json->node[args->node].text,
                        args->request->json->node[args->node].length);
    }
    mmgr_line_put(args->err_msg, ", expected ");
    mmgr_line_put(args->err_msg, args->expected);
}

const char *cli_serve_string(const CliServeErrMsgCtx *args)
{
    const unsigned int node = cli_serve_err_msg_internal.arg(args);
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = args->request->json, .node = node, .length_pointer = NULL);
    if (text == NULL)
    {
        EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = args->request,
                   .node = node, .key = args->key, .expected = "a string", .err_msg = args->err_msg);
    }
    return text;
}

const char *cli_serve_string_or(const CliServeErrMsgCtx *args)
{
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->request->json,
                   .node = cli_serve_err_msg_internal.arg(args), .length_pointer = NULL);
    return (text != NULL) ? text : args->otherwise;
}

void cli_serve_secret_wiped(const CliServeErrMsgCtx *args)
{
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->request->json,
                   .node = cli_serve_err_msg_internal.arg(args), .length_pointer = NULL);
    if (text != NULL)
    {
        // the text lies in the arguments' cell, which this call owns; const is the reader's view of it
        mmgr_zero_buf((void *)(uintptr_t)text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    }
}

int cli_serve_whole(const CliServeErrMsgCtx *args)
{
    unsigned long long *value = args->value;
    const unsigned int node = cli_serve_err_msg_internal.arg(args);
    const unsigned long long none = ~0ull;
    *value = EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx,
                        .json = args->request->json, .node = node, .otherwise = none);
    if (*value == none)
    {
        EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = args->request,
                   .node = node, .key = args->key, .expected = "u64", .err_msg = args->err_msg);
        return 0;
    }
    return 1;
}

unsigned long long cli_serve_whole_or(const CliServeErrMsgCtx *args)
{
    unsigned long long otherwise = args->otherwise_u64;
    return EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx,
                      .json = args->request->json, .node = cli_serve_err_msg_internal.arg(args),
                      .otherwise = otherwise);
}

int cli_serve_sign(const CliServeErrMsgCtx *args)
{
    long long *value = args->value_i64s;
    const unsigned int node = cli_serve_err_msg_internal.arg(args);
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->request->json,
                    .node = node, .kind = CFG_JSON_NUMBER) ||
        (args->request->json->node[node].length == 0ull))
    {
        EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = args->request,
                   .node = node, .key = args->key, .expected = "i64", .err_msg = args->err_msg);
        return 0;
    }
    const char *const digits = args->request->json->text + args->request->json->node[node].text;
    const unsigned long long length = args->request->json->node[node].length;
    const int below = (digits[0] == '-');
    unsigned long long magnitude = 0ull;
    for (unsigned long long at = below ? 1ull : 0ull; at < length; at += 1ull)
    {
        if ((digits[at] < '0') || (digits[at] > '9') || (magnitude > 922337203685477580ull))
        {
            EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = args->request,
                       .node = node, .key = args->key, .expected = "i64", .err_msg = args->err_msg);
            return 0;
        }
        magnitude = (magnitude * 10ull) + (unsigned long long)(digits[at] - '0');
    }
    // the magnitude is at most 2^63 - 1 and is negated within the signed range
    *value = below ? -(long long)magnitude : (long long)magnitude;
    return 1;
}

int cli_serve_bool_or(const CliServeErrMsgCtx *args)
{
    const unsigned int node = cli_serve_err_msg_internal.arg(args);
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->request->json,
                   .node = node, .kind = CFG_JSON_TRUE))
    {
        return 1;
    }
    return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->request->json,
                      .node = node, .kind = CFG_JSON_FALSE)
               ? 0
               : args->otherwise_i32;
}

int cli_serve_bool(const CliServeErrMsgCtx *args)
{
    int *value = args->value_i32s;
    const unsigned int node = cli_serve_err_msg_internal.arg(args);
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->request->json,
                    .node = node, .kind = CFG_JSON_TRUE) &&
        !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->request->json,
                    .node = node, .kind = CFG_JSON_FALSE))
    {
        EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = args->request,
                   .node = node, .key = args->key, .expected = "a boolean", .err_msg = args->err_msg);
        return 0;
    }
    *value = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->request->json,
                        .node = node, .kind = CFG_JSON_TRUE);
    return 1;
}

int cli_serve_strings(const CliServeErrMsgCtx *args)
{
    unsigned int *count = args->count;
    const unsigned int node = cli_serve_err_msg_internal.arg(args);
    *count = 0u;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->request->json,
                    .node = node, .kind = CFG_JSON_ARRAY))
    {
        EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = args->request,
                   .node = node, .key = args->key, .expected = "a sequence", .err_msg = args->err_msg);
        return 0;
    }
    for (unsigned int item = args->request->json->node[node].first; item != CLI_JSON_NONE;
         item = args->request->json->node[item].next)
    {
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = args->request->json, .node = item, .length_pointer = NULL);
        if (text == NULL)
        {
            EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = args->request,
                       .node = item, .key = args->key, .expected = "a string", .err_msg = args->err_msg);
            return 0;
        }
        if (*count < args->room)
        {
            args->items[*count] = text;
            *count += 1u;
        }
    }
    return 1;
}

int cli_serve_strings_or(const CliServeErrMsgCtx *args)
{
    unsigned int *count = args->count;
    const unsigned int node = cli_serve_err_msg_internal.arg(args);
    *count = 0u;
    if ((node == CLI_JSON_NONE) || EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                              .json_read = args->request->json, .node = node, .kind = CFG_JSON_NULL))
    {
        return 1;
    }
    return EMBED_CALL(cli_serve_err_msg_internal.strings, CliServeErrMsgCtx, .request = args->request, .key = args->key,
                      .items = args->items, .room = args->room, .count = count, .err_msg = args->err_msg);
}

int cli_serve_strings_take(const CliServeErrMsgCtx *args)
{
    const char ***items = args->items_pointer;
    unsigned int *count = args->count;
    const unsigned int node = cli_serve_err_msg_internal.arg(args);
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = args->request->json, .node = node, .kind = CFG_JSON_ARRAY)
                                   ? args->request->json->node[node].first
                                   : CLI_JSON_NONE;
    unsigned int room = 0u;
    for (unsigned int item = first; (item != CLI_JSON_NONE) && (room < args->most);
         item = args->request->json->node[item].next)
    {
        room += 1u;
    }
    *count = 0u;
    *items = g_cli_cells->persistent_buf_alloc((room + 1ull) * sizeof(**items), &g_cli_serve_strings_cells);
    if (*items == NULL)
    {
        mmgr_line_put(args->err_msg, "the strings a call names could not be given a cell");
        return 0;
    }
    const int read =
        EMBED_CALL(cli_serve_err_msg_internal.strings, CliServeErrMsgCtx, .request = args->request, .key = args->key,
                   .items = *items, .room = room, .count = count, .err_msg = args->err_msg);
    (*items)[*count] = NULL;
    return read;
}

void cli_serve_strings_free(const CliServeItemsCtx *args)
{
    g_cli_cells->persistent_buf_release(args->items, &g_cli_serve_strings_cells);
}

int cli_serve_tree(const CliServeErrMsgRequestRootCtx *args)
{
    char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = root, .err_msg = err_msg);
}

int cli_serve_at(const CliServeErrMsgRequestRootCtx *args)
{
    char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = args->request,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_root.root, CliServeErrMsgRootCfg, .root_out = root, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = args->full);
    return 1;
}

int cli_serve_place(const CliServeErrMsgCtx *args)
{
    CliTextPlace *place = args->place;
    const unsigned int node = cli_serve_err_msg_internal.arg(args);
    const CliServeArgs inner = {args->request->json, node};
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->request->json,
                    .node = node, .kind = CFG_JSON_OBJECT))
    {
        EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = args->request,
                   .node = node, .key = args->key, .expected = "struct Place", .err_msg = args->err_msg);
        return 0;
    }
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = &inner, .key = "line",
                    .value = &line, .err_msg = args->err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = &inner, .key = "col", .value = &col,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    // a place is cut to its low words, as the protocol counts them
    *place = (CliTextPlace){(unsigned int)line, (unsigned int)col};
    return 1;
}
