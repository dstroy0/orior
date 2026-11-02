// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_files.c: the calls of one file of the tree: made, a folder made, its path as the tree names it, read for
// the editor, read as a window of lines or a slice, read as bytes, written, written with the rights the system requires
// for, its snapshots listed and read; the bridge read; files from outside the window copied into a folder of the tree
// (serve_call_bring.c); a file brought from the window's machine as files bring it over, a name for it then its
// parts; and the tree's trust reported and set, the servers starting again with the tree's code or without it as the
// answer has it
#include "../command_line/cells.h"
#include "../editor/bridge.h"
#include "../editor/files.h"
#include "../editor/history.h"
#include "../machine/raised.h"
#include "../machine/trust.h"
#include "../text/base64.h"
#include "serve_internal.h"

// the bytes a file's text read is written in
#define CLI_SERVE_TEXT_ROOM (64ull << 20u)
#define CLI_SERVE_TEXT_MORE 64ull

typedef struct
{
    VerbaLinea *text;
    unsigned long long room;
    VerbaLinea *err_msg;
} ServeCallFilesCtx;
// a line begun for a file's text in a cell of `room` bytes: 1, or 0 with why given where the cells cannot meet it
EMBED_INLINE int text_line_begin(const ServeCallFilesCtx *args)
{
    VerbaLinea *text = args->text;
    char *const cell = g_cli_cells->temporary_buf_alloc(args->room, &g_cli_serve_files_cells);
    *text = mmgr_line_over(cell, args->room);
    const int begun = cell != NULL;
    if (!begun)
    {
        mmgr_line_put(args->err_msg, "a file's text could not be taken: the cells cannot meet it");
    }
    return begun;
}

static int file_create(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_files_root_err_msg_text.create_file, CliFilesRootErrMsgTextCfg, .root = root, .file = path,
                    .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

static int folder_create(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_files_root_out_dir.create_folder, CliFilesRootOutDirCfg, .root = root, .dir = path,
                    .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

static int tree_relative(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if (path == NULL)
    {
        return 0;
    }
    // a file under a folder mounted beside the tree goes by its mount's name as well
    if (!EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = root) ||
        !EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = path, .full = full,
                    .room = sizeof(full)))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    char shown[CLI_PATH_MAX_LEN];
    VerbaLinea line = {shown, sizeof(shown), 0ull};
    EMBED_CALL(cli_servers_path_err_msg_internal.tree_path, CliServersPathErrMsgCtx, .tree = root, .path = full,
               .out = &line);
    const int inside =
        mmgr_line_ok(&line) && (!cellul.eq(&(CatenaFinitaCfg){.src = shown, .other = full, .cap = CLI_CELLS_POOL}));
    if (inside)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)shown, .length = line.at);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    return 1;
}

static int file_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    return (path != NULL) &&
           EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_files_root_out_err_msg_file.read, CliFilesRootOutErrMsgFileCfg, .root = root, .file = path,
                      .out = out, .err_msg = err_msg);
}

// whether git ignores the file of the argument `path`: true where it neither tracks it nor would
static int file_ignore(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                      .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, EMBED_CALL(cli_files_root_path.track, CliFilesRootPathCfg, .root = root, .path = path)
                                 ? "false"
                                 : "true");
    return 1;
}

static int bridge_read(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    CliJson json;
    const unsigned int bridge = EMBED_CALL(cli_bridge.read, CliBridgeCfg, .json = &json, .root = root);
    if (bridge != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = bridge,
                   .out = out, .indent = 0u, .sorted = 0);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return 1;
}

static int file_slice(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long start = 0ull;
    unsigned long long end = 0ull;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "start",
                    .value = &start, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "end",
                    .value = &end, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    VerbaLinea slice;
    if (!EMBED_CALL(text_line_begin, ServeCallFilesCtx, .text = &slice, .room = (end - start) + CLI_SERVE_TEXT_MORE,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const int done = EMBED_CALL(cli_files_root_out_err_msg_file.slice, CliFilesRootOutErrMsgFileCfg, .root = root,
                                .file = path, .start = start, .end = end, .out = &slice, .err_msg = err_msg);
    if (done)
    {
        // the page takes the slice's start, end, size and text
        static const char *const s_slice_fields[4] = {"start", "end", "size", "text"};
        mmgr_line_ch(out, '{');
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            unsigned long long from = 0ull;
            unsigned long long length = 0ull;
            mmgr_line_put(out, (at > 0u) ? ",\"" : "\"");
            mmgr_line_put(out, s_slice_fields[at]);
            mmgr_line_put(out, "\":");
            if (EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = slice.out, .size_value = slice.at,
                           .name = s_slice_fields[at], .start = &from, .length_pointer = &length))
            {
                mmgr_line_put_n(out, slice.out + from, length);
            }
            else
            {
                mmgr_line_put(out, "null");
            }
        }
        mmgr_line_ch(out, '}');
    }
    g_cli_cells->persistent_buf_release(slice.out, &g_cli_serve_files_cells);
    return done;
}

static int file_bytes(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = path, .joined = full,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = full))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, " is not a file");
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = full,
                                                   .size_pointer = &size, .owner = &g_cli_serve_files_cells);
    if ((mapped == NULL) &&
        (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = full, .size_pointer = &size) ||
         (size > 0ull)))
    {
        mmgr_line_put(err_msg, full);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    mmgr_line_ch(out, '"');
    EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = out,
               .bytes = (mapped != NULL) ? mapped : (const unsigned char *)"", .size = size);
    mmgr_line_ch(out, '"');
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
               .owner = &g_cli_serve_files_cells);
    return 1;
}

static int file_write_raise(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    const char *const text = (path != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "text", .err_msg = err_msg)
                                            : NULL;
    if ((text == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = path, .joined = full,
                    .err_msg = err_msg))
    {
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &length);
    if (!EMBED_CALL(cli_raised_err_msg.write, CliRaisedErrMsgCfg, .path = full, .text = text, .size = length,
                    .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

int cli_serve_file_write(const CliServeErrMsgPathCtx *args)
{
    const char *path = args->path;
    const char *text = args->text;
    unsigned long long length = args->length;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
    // the text the file contained is copied out before it is written over, as the write puts a new file in its place
    unsigned long long old_size = 0ull;
    char *const was =
        EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = full, .size_pointer = &old_size)
            ? g_cli_cells->temporary_buf_alloc(old_size + 1ull, &g_cli_serve_files_cells)
            : NULL;
    const unsigned long long before =
        (was != NULL) ? qry_member_file_read(full, (unsigned char *)was, old_size + 1ull) : QRY_MEMBER_UNREAD;
    const int had = (before != QRY_MEMBER_UNREAD) &&
                    EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg, .bytes = was, .size = before);
    if (!EMBED_CALL(cli_files_root_err_msg_text.write, CliFilesRootErrMsgTextCfg, .root = root, .file = path,
                    .text = text, .length = length, .err_msg = err_msg))
    {
        g_cli_cells->persistent_buf_release(was, &g_cli_serve_files_cells);
        return 0;
    }
    char why[1024];
    VerbaLinea history_err = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_history_before.save, CliHistoryBeforeCfg, .root = root, .path = path,
                    .before = had ? was : NULL, .before_size = had ? before : 0ull, .text = text, .size = length,
                    .err_msg = &history_err))
    {
        char msg_buf[2048];
        VerbaLinea line = {msg_buf, sizeof(msg_buf), 0ull};
        mmgr_line_put(&line, "orior: local history of ");
        mmgr_line_put(&line, path);
        mmgr_line_put(&line, ": ");
        mmgr_line_put_n(&line, why, mmgr_line_ok(&history_err) ? history_err.at : 0ull);
        mmgr_line_ch(&line, '\n');
        EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = msg_buf,
                   .length = mmgr_line_ok(&line) ? line.at : 0ull);
    }
    g_cli_cells->persistent_buf_release(was, &g_cli_serve_files_cells);
    return 1;
}

static int file_write(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long length = 0ull;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    const char *const text = (path != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "text", .err_msg = err_msg)
                                            : NULL;
    if (text == NULL)
    {
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &length);
    if (!EMBED_CALL(cli_serve_err_msg_path_internal.file_write, CliServeErrMsgPathCtx, .path = path, .text = text,
                    .length = length, .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

static int file_part(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long at = 0ull;
    int last = 0;
    unsigned long long length = 0ull;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    const char *const data = (path != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "data", .err_msg = err_msg)
                                            : NULL;
    if ((data == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "at",
                    .value = &at, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments, .key = "last",
                    .value_i32s = &last, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "data"),
        .length_pointer = &length);
    // base64 encodes three bytes in each four letters
    const unsigned long long room = ((length / 4ull) + 1ull) * 3ull;
    char *const bytes = g_cli_cells->temporary_buf_alloc(room, &g_cli_serve_files_cells);
    if (bytes == NULL)
    {
        mmgr_line_put(err_msg, "a file's part could not be taken: the cells cannot meet it");
        return 0;
    }
    unsigned long long size = 0ull;
    int done = EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = data, .length = length,
                          .out_bytes = (unsigned char *)bytes, .room = room, .size_pointer = &size);
    if (!done)
    {
        mmgr_line_put(err_msg, "a part not in base64");
    }
    done = done && EMBED_CALL(cli_files_root_path.write_part, CliFilesRootPathCfg, .root = root, .path = path, .at = at,
                              .data = bytes, .size = size, .last = last, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(bytes, &g_cli_serve_files_cells);
    mmgr_line_put(out, done ? "null" : "");
    return done;
}

static int trust_read(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_trust_tree_out.state_write, CliTrustTreeOutCfg, .tree = root, .out = out);
    return 1;
}

static int trust_set(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    int trusted = 0;
    const char *const folder = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                          .key = "folder", .err_msg = err_msg);
    if ((folder == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments, .key = "trusted",
                    .value_i32s = &trusted, .err_msg = err_msg) ||
        !EMBED_CALL(cli_trust_folder.set, CliTrustFolderCfg, .folder = folder, .trusted_value = trusted,
                    .err_msg = err_msg))
    {
        return 0;
    }
    // the servers start again, with the tree's code or without it as the answer has it
    cli_servers_let_go();
    mmgr_line_put(args->out, "null");
    return 1;
}

static int history_list(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                      .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_history.list, CliHistoryCfg, .root = root, .path = path, .out = out);
    return 1;
}

static int history_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long at = 0ull;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "at",
                    .value = &at, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    VerbaLinea text;
    if (!EMBED_CALL(text_line_begin, ServeCallFilesCtx, .text = &text, .room = CLI_SERVE_TEXT_ROOM, .err_msg = err_msg))
    {
        return 0;
    }
    const int done = EMBED_CALL(cli_history.read, CliHistoryCfg, .root = root, .path = path, .at = at, .out = &text,
                                .err_msg = err_msg);
    if (done)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)text.out, .length = mmgr_line_ok(&text) ? text.at : 0ull);
    }
    g_cli_cells->persistent_buf_release(text.out, &g_cli_serve_files_cells);
    return done;
}

const CliServeCall g_cli_serve_files_calls[] = {
    {"file_create", file_create},
    {"folder_create", folder_create},
    {"tree_relative", tree_relative},
    {"file_read", file_read},
    {"file_ignored", file_ignore},
    {"bridge_read", bridge_read},
    {"file_slice", file_slice},
    {"file_bytes", file_bytes},
    {"file_write_raised", file_write_raise},
    {"file_write", file_write},
    {"files_bring", cli_serve_files_bring},
    {"bring_name", cli_serve_bring_name},
    {"file_part", file_part},
    {"trust_read", trust_read},
    {"trust_set", trust_set},
    {"history_list", history_list},
    {"history_read", history_read},
};
const unsigned int g_cli_serve_files_count = sizeof(g_cli_serve_files_calls) / sizeof(g_cli_serve_files_calls[0]);
