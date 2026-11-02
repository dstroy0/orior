// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_notebook.c: the page's calls on notebooks: a notebook read, written, written for a diff, merged from a
// merge's stages and written without its outputs, each read into a tree sized to the notebooks it reads
#include "../code/git.h"
#include "../command_line/cells.h"
#include "../notebooks/notebook.h"
#include "../text/text.h"
#include "serve_internal.h"

// A notebook's room: the tree its notebooks are read into and the tokens each is read with; the nodes and bytes of the
// tree, from what the notebooks contain, the tokens of the largest, and four times the nodes, the bytes and the lines
// of each for what reading it makes of it
typedef struct
{
    CliJson json;
    CfgJsonToken *tokens;
    unsigned int tokens_most;
    unsigned int nodes;
    unsigned long long text;
} CliBookRoom;
// the most bytes of a notebook, the most nodes a notebook's tree contains, the bytes past its own a notebook's text is
// written in, and the bytes a stage of a merge is read in
#define CLI_SERVE_NOTEBOOK (32ull << 20u)
#define CLI_SERVE_BOOK_NODES (1u << 19u)
#define CLI_SERVE_BOOK_MORE (64ull << 10u)
#define CLI_SERVE_STAGE_ROOM (64ull << 20u)

typedef struct
{
    CliBookRoom *room;
    const char *text;
    unsigned long long size;
    VerbaLinea *err_msg;
    const CliServeArgs *request;
    char *full;
    unsigned long long *size_pointer;
    const char *root;
    const char *given;
    unsigned int stage;
    VerbaLinea *text_verba_linea_pointer;
    const VerbaLinea *stages;
    int has_base;
    VerbaLinea *out;
} ServeCallNotebookCtx;
// a notebook's text of `size` bytes added to what the room is taken for
EMBED_INLINE void book_room_add(const ServeCallNotebookCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    const unsigned int tokens =
        EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = text, .size = size) + 1u;
    const unsigned int lines = (unsigned int)EMBED_CALL(cli_text_size_byte.count, CliTextSizeByteCfg, .text = text,
                                                        .size = size, .byte = '\n') +
                               1u;
    args->room->tokens_most = (tokens > args->room->tokens_most) ? tokens : args->room->tokens_most;
    const unsigned long long nodes = (unsigned long long)args->room->nodes + (4ull * tokens) + (4ull * lines) + 64ull;
    args->room->nodes = (nodes < CLI_SERVE_BOOK_NODES) ? (unsigned int)nodes : CLI_SERVE_BOOK_NODES;
    args->room->text += (4ull * size) + 4096ull;
}

// the room's tree and tokens taken: 1, or 0 with why given where the cells cannot meet them
EMBED_INLINE int book_room_take(const ServeCallNotebookCtx *args)
{
    args->room->tokens = g_cli_cells->persistent_buf_alloc(
        (unsigned long long)args->room->tokens_most * sizeof(CfgJsonToken), &g_cli_window_cells);
    if ((args->room->tokens == NULL) ||
        !EMBED_CALL(cli_json_tree_nodes_most.json_reserve, CliJsonTreeNodesMostCfg, .json = &args->room->json,
                    .nodes_most = args->room->nodes, .text_most = args->room->text, .what = "a notebook's tree",
                    .owner = &g_cli_window_cells))
    {
        g_cli_cells->persistent_buf_release(args->room->tokens, &g_cli_window_cells);
        args->room->tokens = NULL;
        mmgr_line_put(args->err_msg, "the notebook's cells could not be taken");
        return 0;
    }
    return 1;
}

// the room's tree and tokens given back
EMBED_INLINE void book_room_free(const ServeCallNotebookCtx *args)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &args->room->json);
    g_cli_cells->persistent_buf_release(args->room->tokens, &g_cli_window_cells);
    args->room->tokens = NULL;
}

// the file of the argument `path` mapped, its path into `full`: its bytes and size, or NULL with why given where it
// cannot be read, is larger than a notebook is read or is not whole UTF-8
EMBED_INLINE const char *notebook_file(const ServeCallNotebookCtx *args)
{
    const CliServeArgs *arguments = args->request;
    char *full = args->full;
    unsigned long long *size = args->size_pointer;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    *size = 0ull;
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                    .root = root, .full = full, .err_msg = err_msg))
    {
        return NULL;
    }
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = full,
                                                  .size_pointer = size, .owner = &g_cli_window_cells);
    // the file's bytes read as the text they are
    const char *const text = (const char *)bytes;
    if ((bytes == NULL) || (*size > CLI_SERVE_NOTEBOOK) ||
        !EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg, .bytes = text, .size = *size))
    {
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_window_cells);
        mmgr_line_put(err_msg, full);
        mmgr_line_put(err_msg, ": it could not be read");
        return NULL;
    }
    return text;
}

static int notebook_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char full[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    const char *const text = EMBED_CALL(notebook_file, ServeCallNotebookCtx, .request = arguments, .full = full,
                                        .size_pointer = &size, .err_msg = err_msg);
    if (text == NULL)
    {
        return 0;
    }
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    EMBED_CALL(book_room_add, ServeCallNotebookCtx, .room = &room, .text = text, .size = size);
    const unsigned int book =
        EMBED_CALL(book_room_take, ServeCallNotebookCtx, .room = &room, .err_msg = err_msg)
            ? EMBED_CALL(cli_notebook_path.read, CliNotebookPathCfg, .json = &room.json, .path = full, .text = text,
                         .size = size, .tokens = room.tokens, .tokens_most = room.tokens_most, .err_msg = err_msg)
            : CLI_JSON_NONE;
    if (book != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &room.json, .node = book,
                   .out = out, .indent = 0u, .sorted = 0);
    }
    EMBED_CALL(book_room_free, ServeCallNotebookCtx, .room = &room);
    // the bytes were mapped from the text
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = (const unsigned char *)text,
               .owner = &g_cli_window_cells);
    return book != CLI_JSON_NONE;
}

static int notebook_write(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx,
                                      .request = arguments, .root = root, .full = full, .err_msg = err_msg))
    {
        return 0;
    }
    const unsigned long long text_room =
        EMBED_CALL(cli_json_tree_node_out.json_write_room, CliJsonTreeNodeOutCfg, .json_read = arguments->json) +
        CLI_SERVE_BOOK_MORE;
    char *const text_cell = g_cli_cells->temporary_buf_alloc(text_room, &g_cli_window_cells);
    VerbaLinea text = mmgr_line_over(text_cell, text_room);
    // the arguments' tree is the call's own room, which the writer may add nodes to
    EMBED_CALL(cli_notebook_path_book.write, CliNotebookPathBookCfg, .json = (CliJson *)arguments->json, .path = full,
               .book =
                   EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "book"),
               .out = &text);
    const int done =
        mmgr_line_ok(&text) && EMBED_CALL(cli_serve_err_msg_path_internal.file_write, CliServeErrMsgPathCtx,
                                          .path = path, .text = text.out, .length = text.at, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(text.out, &g_cli_window_cells);
    mmgr_line_put(out, done ? "null" : "");
    return done;
}

static int notebook_diff_text(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long size = 0ull;
    const char *const text = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "text", .err_msg = err_msg);
    if (text == NULL)
    {
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &size);
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    EMBED_CALL(book_room_add, ServeCallNotebookCtx, .room = &room, .text = text, .size = size);
    const int done =
        EMBED_CALL(book_room_take, ServeCallNotebookCtx, .room = &room, .err_msg = err_msg) &&
        EMBED_CALL(cli_notebook_text.for_diff, CliNotebookTextCfg, .json = &room.json, .text = text, .size = size,
                   .tokens = room.tokens, .tokens_most = room.tokens_most, .out = out, .err_msg = err_msg);
    EMBED_CALL(book_room_free, ServeCallNotebookCtx, .room = &room);
    return done;
}

static int notebook_without_outputs(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char full[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    const char *const text = EMBED_CALL(notebook_file, ServeCallNotebookCtx, .request = arguments, .full = full,
                                        .size_pointer = &size, .err_msg = err_msg);
    if (text == NULL)
    {
        return 0;
    }
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    EMBED_CALL(book_room_add, ServeCallNotebookCtx, .room = &room, .text = text, .size = size);
    const int done = EMBED_CALL(book_room_take, ServeCallNotebookCtx, .room = &room, .err_msg = err_msg) &&
                     EMBED_CALL(cli_notebook_text.without_outputs, CliNotebookTextCfg, .json = &room.json, .text = text,
                                .size = size, .tokens = room.tokens, .tokens_most = room.tokens_most, .out = out,
                                .err_msg = err_msg);
    EMBED_CALL(book_room_free, ServeCallNotebookCtx, .room = &room);
    // the bytes were mapped from the text
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = (const unsigned char *)text,
               .owner = &g_cli_window_cells);
    return done;
}

// stage `stage` of a merge of the notebook `given` read into `text`, a line: 1, or 0 with why given
EMBED_INLINE int notebook_stage(const ServeCallNotebookCtx *args)
{
    const char *root = args->root;
    unsigned int stage = args->stage;
    VerbaLinea *text = args->text_verba_linea_pointer;
    char *const text_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVE_STAGE_ROOM, &g_cli_window_cells);
    *text = mmgr_line_over(text_cell, CLI_SERVE_STAGE_ROOM);
    if (!EMBED_CALL(cli_git_root_file_stage.stage_text, CliGitRootFileStageCfg, .root = root, .file = args->given,
                    .stage = stage, .out = text) ||
        !mmgr_line_ok(text))
    {
        mmgr_line_put(args->err_msg, args->given);
        mmgr_line_put(args->err_msg, " contains no stage ");
        mmgr_line_decimal(args->err_msg, stage, 1u);
        mmgr_line_put(args->err_msg, " of a merge");
        return 0;
    }
    return 1;
}

// the merge of the stages read into the room's tree and written into `out`: 1 where it is, else 0 with why given
EMBED_INLINE int notebook_on_merge(const ServeCallNotebookCtx *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char quiet[1024];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    unsigned int base =
        args->has_base
            ? EMBED_CALL(cli_notebook_path.read, CliNotebookPathCfg, .json = &args->room->json, .path = "x.ipynb",
                         .text = args->stages[0].out, .size = args->stages[0].at, .tokens = args->room->tokens,
                         .tokens_most = args->room->tokens_most, .err_msg = &ignored)
            : CLI_JSON_NONE;
    if (base == CLI_JSON_NONE)
    {
        base = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &args->room->json,
                          .kind = CFG_JSON_OBJECT);
        EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &args->room->json,
                   .object = base, .name = "cells",
                   .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &args->room->json,
                                       .kind = CFG_JSON_ARRAY));
    }
    const unsigned int ours =
        EMBED_CALL(cli_notebook_path.read, CliNotebookPathCfg, .json = &args->room->json, .path = "x.ipynb",
                   .text = args->stages[1].out, .size = args->stages[1].at, .tokens = args->room->tokens,
                   .tokens_most = args->room->tokens_most, .err_msg = err_msg);
    const unsigned int theirs =
        (ours != CLI_JSON_NONE)
            ? EMBED_CALL(cli_notebook_path.read, CliNotebookPathCfg, .json = &args->room->json, .path = "x.ipynb",
                         .text = args->stages[2].out, .size = args->stages[2].at, .tokens = args->room->tokens,
                         .tokens_most = args->room->tokens_most, .err_msg = err_msg)
            : CLI_JSON_NONE;
    if (theirs != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &args->room->json,
                   .node = EMBED_CALL(cli_notebook.merge, CliNotebookCfg, .json = &args->room->json, .base = base,
                                      .ours = ours, .theirs = theirs),
                   .out = out, .indent = 0u, .sorted = 0);
    }
    return theirs != CLI_JSON_NONE;
}

static int notebook_merge(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const given = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "path", .err_msg = err_msg);
    if ((given == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                       .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    char quiet[1024];
    VerbaLinea ignored = {quiet, sizeof(quiet), 0ull};
    VerbaLinea stages[3] = {{NULL, 0ull, 0ull, NULL, NULL, 0ull},
                            {NULL, 0ull, 0ull, NULL, NULL, 0ull},
                            {NULL, 0ull, 0ull, NULL, NULL, 0ull}};
    const int has_base = EMBED_CALL(notebook_stage, ServeCallNotebookCtx, .root = root, .given = given, .stage = 1u,
                                    .text_verba_linea_pointer = &stages[0], .err_msg = &ignored);
    int read = EMBED_CALL(notebook_stage, ServeCallNotebookCtx, .root = root, .given = given, .stage = 2u,
                          .text_verba_linea_pointer = &stages[1], .err_msg = err_msg);
    read = read && EMBED_CALL(notebook_stage, ServeCallNotebookCtx, .root = root, .given = given, .stage = 3u,
                              .text_verba_linea_pointer = &stages[2], .err_msg = err_msg);
    CliBookRoom room = {{0}, NULL, 0u, 0u, 0ull};
    // the merge contains the three sides and copies of their cells, a fourth reading
    for (unsigned int at = 0u; read && (at < 4u); at += 1u)
    {
        const unsigned int side = (at < 3u) ? at : 1u;
        EMBED_CALL(book_room_add, ServeCallNotebookCtx, .room = &room, .text = stages[side].out,
                   .size = mmgr_line_ok(&stages[side]) ? stages[side].at : 0ull);
    }
    const int done = read && EMBED_CALL(book_room_take, ServeCallNotebookCtx, .room = &room, .err_msg = err_msg) &&
                     EMBED_CALL(notebook_on_merge, ServeCallNotebookCtx, .room = &room, .stages = stages,
                                .has_base = has_base, .out = out, .err_msg = err_msg);
    EMBED_CALL(book_room_free, ServeCallNotebookCtx, .room = &room);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        g_cli_cells->persistent_buf_release(stages[at].out, &g_cli_window_cells);
    }
    return done;
}

const CliServeCall g_cli_serve_notebook_calls[] = {
    {"notebook_read", notebook_read},
    {"notebook_write", notebook_write},
    {"notebook_diff_text", notebook_diff_text},
    {"notebook_merge", notebook_merge},
    {"notebook_without_outputs", notebook_without_outputs},
};
const unsigned int g_cli_serve_notebook_count =
    sizeof(g_cli_serve_notebook_calls) / sizeof(g_cli_serve_notebook_calls[0]);
