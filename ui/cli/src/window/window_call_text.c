// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_text.c: the window's calls on a text stored as a branch opened it (window_call_branch.c): its changes
// written as the page makes them, the text let go, its gutter's marks against the last commit and find's matches in
// it; two texts compared by structure; and a file anywhere on the machine read or written as the reader chose it
#include "../code/structure.h"
#include "../command_line/cells.h"
#include "../deploy/devcontainer.h"
#include "../editor/document.h"
#include "window_internal.h"

// the bytes the tree's answer is written in
#define CLI_WINDOW_TEXT_ANSWER (8ull << 20u)

typedef struct
{
    const CliShape *shape;
    VerbaLinea *out;
    const CliJson *json;
    unsigned int edit;
    const char *name;
    unsigned int node;
} WindowCallTextCtx;
// a text's shape written as the page reads it
EMBED_INLINE void text_shape_write(const WindowCallTextCtx *args)
{
    mmgr_line_put(args->out, "{\"lines\":");
    mmgr_line_decimal(args->out, args->shape->lines, 1u);
    mmgr_line_put(args->out, ",\"units\":");
    mmgr_line_decimal(args->out, args->shape->units, 1u);
    mmgr_line_ch(args->out, '}');
}

unsigned int cli_window_tree_on_call(const CliWindowErrMsgCtx *args)
{
    CliJson *json = args->json;
    VerbaLinea *err_msg = args->err_msg;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    char arguments[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {arguments, sizeof(arguments), 0ull};
    mmgr_line_put(&line, "{\"path\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = &line, .text = args->path);
    mmgr_line_put(&line, args->more);
    mmgr_line_ch(&line, '}');
    char *const answer_cell = g_cli_cells->temporary_buf_alloc(CLI_WINDOW_TEXT_ANSWER, &g_cli_window_text_cells);
    VerbaLinea answer = mmgr_line_over(answer_cell, CLI_WINDOW_TEXT_ANSWER);
    if (answer.out == NULL)
    {
        mmgr_line_put(err_msg, "the tree's answer on a text could not be taken");
        return CLI_JSON_NONE;
    }
    unsigned int read = CLI_JSON_NONE;
    if (mmgr_line_ok(&line) &&
        EMBED_CALL(cli_window_out_size_internal.tree_call, CliWindowOutSizeCtx, .name = args->name,
                   .args_text = arguments, .size = line.at, .out = &answer, .err_msg = err_msg))
    {
        read = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = json,
                          .text = answer.out, .size = mmgr_line_ok(&answer) ? answer.at : 0ull, .nodes_more = 0u,
                          .text_more = 0ull, .err_msg = err_msg, .owner = &g_cli_window_text_cells);
    }
    g_cli_cells->persistent_buf_release(answer.out, &g_cli_window_text_cells);
    return read;
}

// a place argument `{line, col}` of an edit read
EMBED_INLINE CliPlace text_place(const WindowCallTextCtx *args)
{
    const CliJson *json = args->json;
    const char *name = args->name;
    const unsigned int place = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->edit, .name = name);
    CliPlace read = {0ull, 0ull};
    const char *const keys[2] = {"line", "col"};
    unsigned long long *const into[2] = {&read.line, &read.col};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = place, .name = keys[at]);
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = number,
                       .kind = CFG_JSON_NUMBER))
        {
            for (unsigned long long digit = 0ull; digit < json->node[number].length; digit += 1ull)
            {
                const char letter = json->text[json->node[number].text + digit];
                if ((letter < '0') || (letter > '9'))
                {
                    break;
                }
                *into[at] = (*into[at] * 10ull) + (unsigned long long)(letter - '0');
            }
        }
    }
    return read;
}

// the items of the array `node`, 0 where it is not one
EMBED_INLINE unsigned int text_items(const WindowCallTextCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    unsigned int count = 0u;
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = node, .kind = CFG_JSON_ARRAY)
                                   ? json->node[node].first
                                   : CLI_JSON_NONE;
    for (unsigned int item = first; item != CLI_JSON_NONE; item = json->node[item].next)
    {
        count += 1u;
    }
    return count;
}

// the changes the page made to the text stored under `key` written, in order: its shape
static int text_document_edit(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const unsigned int changes =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "changes");
    if (key == NULL)
    {
        return 0;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = arguments->json,
                    .node = changes, .kind = CFG_JSON_ARRAY))
    {
        EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = arguments,
                   .node = changes, .key = "changes", .expected = "a sequence", .err_msg = err_msg);
        return 0;
    }
    const unsigned int changes_count =
        EMBED_CALL(text_items, WindowCallTextCtx, .json = arguments->json, .node = changes);
    unsigned int edits_count = 0u;
    for (unsigned int change = arguments->json->node[changes].first; change != CLI_JSON_NONE;
         change = arguments->json->node[change].next)
    {
        edits_count += EMBED_CALL(text_items, WindowCallTextCtx, .json = arguments->json, .node = change);
    }
    // a cell is taken for one of each where the page sent none: a taken cell is never of no bytes
    CliEdit *const edit_of =
        g_cli_cells->temporary_buf_alloc((edits_count + 1ull) * sizeof(*edit_of), &g_cli_window_text_cells);
    const CliEdit **const change_of =
        g_cli_cells->temporary_buf_alloc((changes_count + 1ull) * sizeof(*change_of), &g_cli_window_text_cells);
    unsigned int *const edits_of =
        g_cli_cells->temporary_buf_alloc((changes_count + 1ull) * sizeof(*edits_of), &g_cli_window_text_cells);
    int edited = 0;
    if ((edit_of == NULL) || (change_of == NULL) || (edits_of == NULL))
    {
        mmgr_line_put(err_msg, "the changes of a writing could not be given cells");
    }
    else
    {
        unsigned int count = 0u;
        unsigned int used = 0u;
        for (unsigned int change = arguments->json->node[changes].first; change != CLI_JSON_NONE;
             change = arguments->json->node[change].next)
        {
            change_of[count] = &edit_of[used];
            edits_of[count] = 0u;
            const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                  .json_read = arguments->json, .node = change, .kind = CFG_JSON_ARRAY)
                                           ? arguments->json->node[change].first
                                           : CLI_JSON_NONE;
            for (unsigned int edit = first; edit != CLI_JSON_NONE; edit = arguments->json->node[edit].next)
            {
                unsigned long long length = 0ull;
                const char *const text = EMBED_CALL(
                    cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                       .json_read = arguments->json, .object = edit, .name = "text"),
                    .length_pointer = &length);
                edit_of[used].from =
                    EMBED_CALL(text_place, WindowCallTextCtx, .json = arguments->json, .edit = edit, .name = "from");
                edit_of[used].to =
                    EMBED_CALL(text_place, WindowCallTextCtx, .json = arguments->json, .edit = edit, .name = "to");
                edit_of[used].text = (text != NULL) ? text : "";
                edit_of[used].length = length;
                used += 1u;
                edits_of[count] += 1u;
            }
            count += 1u;
        }
        CliShape shape;
        edited = EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .key = key, .changes = change_of,
                            .edits = edits_of, .count_u32 = count, .shape = &shape, .err_msg = err_msg);
        if (edited)
        {
            EMBED_CALL(text_shape_write, WindowCallTextCtx, .shape = &shape, .out = out);
        }
    }
    g_cli_cells->persistent_buf_release(edits_of, &g_cli_window_text_cells);
    g_cli_cells->persistent_buf_release(change_of, &g_cli_window_text_cells);
    g_cli_cells->persistent_buf_release(edit_of, &g_cli_window_text_cells);
    return edited;
}

// the text stored under `key` let go
static int text_document_release(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if (key == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
    return 1;
}

// the gutter's marks for the text stored under `key` against the file at `path` as the last commit stores it, read from
// the tree where it is not stored beside the text yet or `fresh` requests; null where no text is stored under `key`
static int text_document_changes(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const char *const path = (key != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "path", .err_msg = err_msg)
                                           : NULL;
    int fresh = 0;
    if ((path == NULL) || !EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments,
                                      .key = "fresh", .value_i32s = &fresh, .err_msg = err_msg))
    {
        return 0;
    }
    const unsigned long long start = out->at;
    if (!fresh && EMBED_CALL(cli_document.has_head, CliDocumentCfg, .key = key))
    {
        if (!EMBED_CALL(cli_document_out_key_head_given.marks, CliDocumentOutKeyHeadGivenCfg, .key = key,
                        .head_free = 0, .head = NULL, .head_size = 0ull, .out = out))
        {
            out->at = start;
            mmgr_line_put(out, "null");
        }
        return 1;
    }
    CliJson json;
    const unsigned int head = EMBED_CALL(cli_window_err_msg_internal.tree_call, CliWindowErrMsgCtx, .json = &json,
                                         .name = "file_head", .path = path, .more = "", .err_msg = err_msg);
    unsigned long long length = 0ull;
    const char *const text = (head != CLI_JSON_NONE)
                                 ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                              .json_read = &json, .node = head, .length_pointer = &length)
                                 : NULL;
    const int read = (head != CLI_JSON_NONE);
    if (read && !EMBED_CALL(cli_document_out_key_head_given.marks, CliDocumentOutKeyHeadGivenCfg, .key = key,
                            .head_free = 1, .head = text, .head_size = length, .out = out))
    {
        out->at = start;
        mmgr_line_put(out, "null");
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return read;
}

// find's matches in the text stored under `key`, each [line, from col, to col], at most `most`
static int text_document_find(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const unsigned int sought =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "sought");
    unsigned long long most = 0ull;
    if ((key == NULL) || !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments,
                                     .key = "most", .value = &most, .err_msg = err_msg))
    {
        return 0;
    }
    const char *const query =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = arguments->json, .object = sought, .name = "query"),
                   .length_pointer = NULL);
    if (query == NULL)
    {
        EMBED_CALL(cli_serve_err_msg_request_internal.reject, CliServeErrMsgRequestCtx, .request = arguments,
                   .node = sought, .key = "sought", .expected = "struct Sought", .err_msg = err_msg);
        return 0;
    }
    const CliDocSought read = {
        query,
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = arguments->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = arguments->json, .object = sought, .name = "regex"),
                   .kind = CFG_JSON_TRUE),
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = arguments->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = arguments->json, .object = sought, .name = "case"),
                   .kind = CFG_JSON_TRUE),
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = arguments->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = arguments->json, .object = sought, .name = "word"),
                   .kind = CFG_JSON_TRUE)};
    return EMBED_CALL(cli_document_out_sought.find, CliDocumentOutSoughtCfg, .key = key, .sought = &read, .most = most,
                      .out = out, .err_msg = err_msg);
}

// two texts compared by structure, the older first, or null where they are too far apart
static int text_structure_compare(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const then = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "then", .otherwise = "");
    const char *const now = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "now", .err_msg = err_msg);
    if (now == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_structure.compare, CliStructureCfg, .then = then,
               .then_size = cellul.len(&(CatenaFinitaCfg){.src = then, .cap = CLI_CELLS_POOL}), .now = now,
               .now_size = cellul.len(&(CatenaFinitaCfg){.src = now, .cap = CLI_CELLS_POOL}), .out = out);
    return 1;
}

// the text of the file at `path`, anywhere on the machine, as the reader chose it
static int text_file_read_any(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if (path == NULL)
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                   .size_pointer = &size, .owner = &g_cli_window_text_cells);
    const int read = (mapped != NULL) && EMBED_CALL(cli_json_tree_size.utf8_whole, CliJsonTreeSizeCfg,
                                                    .bytes = (const char *)mapped, .size = size);
    if (read)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = (const char *)mapped, .length = size);
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
               .owner = &g_cli_window_text_cells);
    if (!read)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
    }
    return read;
}

// `text` written to the file at `path`, anywhere on the machine, the folders it stands in made where they are not
static int text_file_write_any(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    (void)out;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    unsigned long long length = 0ull;
    const char *const text =
        (path != NULL)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
                         .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments,
                                            .key = "text"),
                         .length_pointer = &length)
            : NULL;
    if ((text == NULL) && (path != NULL))
    {
        (void)EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments, .key = "text",
                         .err_msg = err_msg);
    }
    if (text == NULL)
    {
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder) &&
        (folder[0] != '\0') &&
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    if (!EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text,
                    .size = length))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

// the folders a .code-workspace file names, each made whole against the file's own folder, as JSON with comments and
// trailing commas reads them
static int text_workspace_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if (path == NULL)
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                   .size_pointer = &size, .owner = &g_cli_window_text_cells);
    // a text made plain is never longer than the text, as it only leaves comments and commas out
    char *const plain_buf =
        (mapped != NULL) ? g_cli_cells->temporary_buf_alloc(size + 1ull, &g_cli_window_text_cells) : NULL;
    VerbaLinea plain = mmgr_line_over(plain_buf, size + 1ull);
    EMBED_CALL(cli_devcontainer_text.plain_json, CliDevcontainerTextCfg, .text = (const char *)mapped, .size = size,
               .plain = &plain);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
               .owner = &g_cli_window_text_cells);
    if (plain_buf == NULL)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    char why[1024];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int value =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = plain_buf, .size = mmgr_line_ok(&plain) ? plain.at : 0ull, .nodes_more = 0u,
                   .text_more = 0ull, .err_msg = &unread, .owner = &g_cli_window_text_cells);
    g_cli_cells->persistent_buf_release(plain_buf, &g_cli_window_text_cells);
    if (value == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, why, mmgr_line_ok(&unread) ? unread.at : 0ull);
        return 0;
    }
    char base[CLI_PATH_MAX_LEN] = "";
    (void)EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = base);
    const unsigned int folders = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = value, .name = "folders");
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                          .node = folders, .kind = CFG_JSON_ARRAY)
                                   ? json.node[folders].first
                                   : CLI_JSON_NONE;
    mmgr_line_ch(out, '[');
    int any = 0;
    for (unsigned int folder = first; folder != CLI_JSON_NONE; folder = json.node[folder].next)
    {
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = folder, .name = "path"),
                       .length_pointer = NULL);
        if (named == NULL)
        {
            continue;
        }
        const int whole = (named[0] == '/') || (named[0] == '\\') ||
                          ((named[0] != '\0') && (named[1] == ':') && ((named[2] == '/') || (named[2] == '\\')));
        char joined[CLI_PATH_MAX_LEN];
        char full[CLI_PATH_MAX_LEN];
        if (whole)
        {
            memor.cpy(&(MemoriaCfg){.dst = joined,
                                    .src = named,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
        }
        else
        {
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = named, .joined = joined);
        }
        mmgr_line_put(out, any ? "," : "");
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out,
                   .text = EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = joined,
                                      .full = full, .room = sizeof(full))
                               ? full
                               : joined);
        any = 1;
    }
    mmgr_line_ch(out, ']');
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return 1;
}

const CliServeCall g_cli_window_text_calls[] = {
    {"document_edit", text_document_edit},         {"document_release", text_document_release},
    {"document_changes", text_document_changes},   {"document_find", text_document_find},
    {"structure_compare", text_structure_compare}, {"file_read_any", text_file_read_any},
    {"file_write_any", text_file_write_any},       {"workspace_read", text_workspace_read},
};
const unsigned int g_cli_window_text_count = sizeof(g_cli_window_text_calls) / sizeof(g_cli_window_text_calls[0]);
