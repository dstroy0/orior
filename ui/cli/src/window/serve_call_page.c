// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_page.c: the calls of the page's own kinds of file and of the machine's containers: a file too large for
// the page stored here as the file and the lines written over it, opened, shaped, read, edited, changed, found in,
// saved and let go; a file of HTTP requests parsed, its environments read, a request sent, its answers stored, written
// as curl and read from curl, and its private environment made; and the containers and pods, their logs, and a
// container acted on or a line run in it
#include "../command_line/cells.h"
#include "../deploy/containers.h"
#include "../editor/document.h"
#include "../editor/pages.h"
#include "../requests/requests.h"
#include "serve_internal.h"

// the most changes and edits one edit of a stored text makes
#define CLI_SERVE_CHANGES 4096u
#define CLI_SERVE_PAGE_EDITS 65536u

typedef struct
{
    const CliJson *json;
    unsigned int node;
    CliPlace *place;
    const CliServeArgs *request;
    char *root;
    const char **path;
    const char **text;
    unsigned long long *size;
    unsigned long long *line;
    VerbaLinea *err_msg;
} ServeCallPageCtx;
// a place `{line, col}` of the page's read into `place`: 1, or 0 where it is none
EMBED_INLINE int page_place(const ServeCallPageCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    const unsigned long long none = ~0ull;
    args->place->line =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = node, .name = "line"),
                   .otherwise = none);
    args->place->col =
        EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = node, .name = "col"),
                   .otherwise = none);
    return (args->place->line != none) && (args->place->col != none);
}

static int pages_open(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    return (key != NULL) &&
           EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                      .root = root, .full = full, .err_msg = err_msg) &&
           EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = key, .path = full, .out = out, .err_msg = err_msg);
}

static int pages_shape(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    return (key != NULL) &&
           EMBED_CALL(cli_pages_path.shape, CliPagesPathCfg, .key = key, .out = out, .err_msg = err_msg);
}

// the lines from `from` to before `to` of the paged text under `key` and their colors, answered together as
// `{"lines", "colors"}`: the lines colored as they are read, no further than the first `columns` letters of each where
// the page names how far its view draws, the page mirroring them from here on, and no colors where the text is no
// branch
static int pages_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if ((key == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "from",
                    .value = &from, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "to",
                    .value = &to, .err_msg = err_msg))
    {
        return 0;
    }
    const unsigned long long columns = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                                  .request = arguments, .key = "columns", .otherwise_u64 = 0ull);
    mmgr_line_put(out, "{\"lines\":");
    if (!EMBED_CALL(cli_pages_out.read, CliPagesOutCfg, .key = key, .from = from, .to = to, .cut = 0ull, .out = out,
                    .lengths = NULL, .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(out, ",\"colors\":");
    if (columns != 0ull)
    {
        (void)EMBED_CALL(cli_document_key_columns.columns, CliDocumentKeyColumnsCfg, .key = key, .columns = columns);
    }
    if ((to <= from) || !EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = key,
                                    .first = from, .last = to - 1ull, .mirrored = 1, .out = out))
    {
        mmgr_line_put(out, "[]");
    }
    mmgr_line_ch(out, '}');
    return 1;
}

// where the page's view of the paged text under `key` stands, `{top, rows, columns, map_from, map_to, cached_from,
// cached_to, cached_cut, width}` as cli_pages_on_view reads them, placed here: answered `{"save": [from, to], "from",
// "cut", "lines", "lengths", "colors"}`, the lines the page is to hold with their colors no further than the view
// reaches, and where the lines are cut each one's whole length in the page's units; `{"save"}` alone where it holds
// what it needs
static int pages_view(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if (key == NULL)
    {
        return 0;
    }
    const CliPagesView view = {EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "top", .otherwise_u64 = 0ull),
                               EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "rows", .otherwise_u64 = 1ull),
                               EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "columns", .otherwise_u64 = 0ull),
                               EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "map_from", .otherwise_u64 = 0ull),
                               EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "map_to", .otherwise_u64 = 0ull),
                               EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "cached_from", .otherwise_u64 = 0ull),
                               EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "cached_to", .otherwise_u64 = 0ull),
                               EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "cached_cut", .otherwise_u64 = 0ull),
                               EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "width", .otherwise_u64 = 0ull)};
    CliPagesPlaced placed = {0ull, 0ull, 0ull, 0ull, 0ull};
    if (!EMBED_CALL(cli_pages_view.view, CliPagesViewCfg, .key = key, .view = &view, .placed = &placed))
    {
        mmgr_line_put(err_msg, key);
        mmgr_line_put(err_msg, ": no paged text is stored under the key");
        return 0;
    }
    mmgr_line_put(out, "{\"keep\":[");
    mmgr_line_decimal(out, placed.save_from, 1u);
    mmgr_line_ch(out, ',');
    mmgr_line_decimal(out, placed.save_to, 1u);
    mmgr_line_ch(out, ']');
    if (placed.from < placed.to)
    {
        // each line's length in a cell of its own, twenty digits at the most and a comma, and the brackets
        const unsigned long long room = ((placed.to - placed.from) * 21ull) + 2ull;
        char *const cell = (placed.cut != 0ull)
                               ? EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx, .size = room,
                                            .what = "the lengths of a view's lines", .err_msg = err_msg)
                               : NULL;
        VerbaLinea lengths = {cell, room, 0ull};
        mmgr_line_put(out, ",\"from\":");
        mmgr_line_decimal(out, placed.from, 1u);
        mmgr_line_put(out, ",\"lines\":");
        // the cut the lines were read at, 0 where none of them was longer
        unsigned long long cut = placed.cut;
        const int read = ((placed.cut == 0ull) || (cell != NULL)) &&
                         EMBED_CALL(cli_pages_out.read, CliPagesOutCfg, .key = key, .from = placed.from,
                                    .to = placed.to, .cut = &cut, .out = out, .lengths = &lengths, .err_msg = err_msg);
        if (cell != NULL)
        {
            mmgr_line_put(out, (cut != 0ull) ? ",\"lengths\":" : "");
            mmgr_line_put_n(out, cell, (cut != 0ull) ? lengths.at : 0ull);
            g_cli_cells->persistent_buf_release(cell, &g_cli_window_cells);
        }
        if (!read)
        {
            return 0;
        }
        mmgr_line_put(out, ",\"cut\":");
        mmgr_line_decimal(out, cut, 1u);
        mmgr_line_put(out, ",\"colors\":");
        if (view.columns != 0ull)
        {
            (void)EMBED_CALL(cli_document_key_columns.columns, CliDocumentKeyColumnsCfg, .key = key,
                             .columns = view.columns);
        }
        if (!EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = key,
                        .first = placed.from, .last = placed.to - 1ull, .mirrored = 1, .out = out))
        {
            mmgr_line_put(out, "[]");
        }
    }
    mmgr_line_ch(out, '}');
    return 1;
}

static int pages_edit(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if (key == NULL)
    {
        return 0;
    }
    const CliJson *const json = arguments->json;
    const unsigned int changes =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "changes");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = changes,
                    .kind = CFG_JSON_ARRAY))
    {
        mmgr_line_put(err_msg, "changes: invalid type: null, expected a sequence");
        return 0;
    }
    // the changes and their edits counted, CLI_SERVE_CHANGES and CLI_SERVE_PAGE_EDITS at the most, and one cell taken
    // for the edits, each change's first and each change's count
    unsigned int changes_most = 0u;
    unsigned int edits_most = 0u;
    for (unsigned int change = json->node[changes].first;
         (change != CLI_JSON_NONE) && (changes_most < CLI_SERVE_CHANGES); change = json->node[change].next)
    {
        changes_most += 1u;
        for (unsigned int edit = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                            .node = change, .kind = CFG_JSON_ARRAY)
                                     ? json->node[change].first
                                     : CLI_JSON_NONE;
             (edit != CLI_JSON_NONE) && (edits_most < CLI_SERVE_PAGE_EDITS); edit = json->node[edit].next)
        {
            edits_most += 1u;
        }
    }
    const unsigned long long edit_bytes = (edits_most + 1ull) * sizeof(CliEdit);
    const unsigned long long change_bytes = (changes_most + 1ull) * sizeof(const CliEdit *);
    char *const cell = EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                                  .size = edit_bytes + change_bytes + ((changes_most + 1ull) * sizeof(unsigned int)),
                                  .what = "the edits of a stored text", .err_msg = err_msg);
    if (cell == NULL)
    {
        return 0;
    }
    // the edits on the cell's boundary, the changes' pointers then their counts after them
    CliEdit *const edit_list = (CliEdit *)(void *)cell;
    const CliEdit **const change_list = (const CliEdit **)(void *)(cell + edit_bytes);
    unsigned int *const edit_counts = (unsigned int *)(void *)(cell + edit_bytes + change_bytes);
    unsigned int count = 0u;
    unsigned int used = 0u;
    const char *missing = NULL;
    for (unsigned int change = json->node[changes].first;
         (missing == NULL) && (change != CLI_JSON_NONE) && (count < changes_most); change = json->node[change].next)
    {
        change_list[count] = &edit_list[used];
        edit_counts[count] = 0u;
        for (unsigned int edit = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                            .node = change, .kind = CFG_JSON_ARRAY)
                                     ? json->node[change].first
                                     : CLI_JSON_NONE;
             (missing == NULL) && (edit != CLI_JSON_NONE) && (used < edits_most); edit = json->node[edit].next)
        {
            CliEdit *const one = &edit_list[used];
            one->text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                      .json_read = json, .object = edit, .name = "text"),
                                   .length_pointer = &one->length);
            missing = !EMBED_CALL(page_place, ServeCallPageCtx, .json = json,
                                  .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = json, .object = edit, .name = "from"),
                                  .place = &one->from)
                          ? "from"
                      : !EMBED_CALL(page_place, ServeCallPageCtx, .json = json,
                                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                       .json_read = json, .object = edit, .name = "to"),
                                    .place = &one->to)
                          ? "to"
                      : (one->text == NULL) ? "text"
                                            : NULL;
            used += 1u;
            edit_counts[count] += 1u;
        }
        count += 1u;
    }
    const int whole = (missing == NULL);
    if (!whole)
    {
        mmgr_line_put(err_msg, "changes: missing field `");
        mmgr_line_put(err_msg, missing);
        mmgr_line_ch(err_msg, '`');
    }
    const int done =
        whole && EMBED_CALL(cli_document_out.paged_edit, CliDocumentOutCfg, .key = key, .changes = change_list,
                            .edits = edit_counts, .count_u32 = count, .out = out, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(cell, &g_cli_window_cells);
    return done;
}

static int pages_change(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    return (key != NULL) &&
           EMBED_CALL(cli_pages_path.change, CliPagesPathCfg, .key = key, .out = out, .err_msg = err_msg);
}

static int pages_find(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long most = 0ull;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const unsigned int node =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "sought");
    const CliServeArgs sought_args = {arguments->json, node};
    int regex = 0;
    int match_case = 0;
    int word = 0;
    const char *const query = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                         .json_read = arguments->json, .node = node, .kind = CFG_JSON_OBJECT)
                                  ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                               .request = &sought_args, .key = "query", .err_msg = err_msg)
                                  : NULL;
    if ((key == NULL) || (query == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = &sought_args, .key = "regex",
                    .value_i32s = &regex, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = &sought_args, .key = "case",
                    .value_i32s = &match_case, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = &sought_args, .key = "word",
                    .value_i32s = &word, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "most",
                    .value = &most, .err_msg = err_msg))
    {
        if ((key != NULL) && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                         .json_read = arguments->json, .node = node, .kind = CFG_JSON_OBJECT))
        {
            mmgr_line_put(err_msg, "sought: invalid type: null, expected struct Sought");
        }
        return 0;
    }
    const CliDocSought sought = {query, regex, match_case, word};
    return EMBED_CALL(cli_pages_out_err_msg_sought.find, CliPagesOutErrMsgSoughtCfg, .key = key, .sought = &sought,
                      .most = most, .out = out, .err_msg = err_msg);
}

static int pages_save(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if (key == NULL)
    {
        return 0;
    }
    // a paged file is written as every file of the tree is, one write at a time
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_serve_write_lock);
    const int saved = EMBED_CALL(cli_pages_path.save, CliPagesPathCfg, .key = key, .out = out, .err_msg = err_msg);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_serve_write_lock);
    return saved;
}

static int pages_release(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if (key == NULL)
    {
        return 0;
    }
    // the branch its colors are walked in let go before the pages its lines are read from
    EMBED_CALL(cli_document.release, CliDocumentCfg, .key = key);
    EMBED_CALL(cli_pages.release, CliPagesCfg, .key = key);
    mmgr_line_put(args->out, "null");
    return 1;
}

static int requests_parse(const CliServeAnswerCfg *args)
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
    return EMBED_CALL(cli_requests_text_table.parse, CliRequestsTextCfg, .text = text, .size = size, .out = out,
                      .err_msg = err_msg);
}

static int requests_environments(const CliServeAnswerCfg *args)
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
           EMBED_CALL(cli_requests_text_table.environments, CliRequestsTextCfg, .root = root, .path = path, .out = out,
                      .err_msg = err_msg);
}

// the arguments `path`, `text` and `line` of a request, and the tree: 1, or 0 with why given
EMBED_INLINE int requests_at(const ServeCallPageCtx *args)
{
    const CliServeArgs *arguments = args->request;
    char *root = args->root;
    const char **path = args->path;
    const char **text = args->text;
    VerbaLinea *err_msg = args->err_msg;
    *path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments, .key = "path",
                       .err_msg = err_msg);
    *text = (*path != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "text", .err_msg = err_msg)
                            : NULL;
    if ((*text == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "line",
                    .value = args->line, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = args->size);
    return 1;
}

static int requests_send(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *path = NULL;
    const char *text = NULL;
    unsigned long long size = 0ull;
    unsigned long long line = 0ull;
    return EMBED_CALL(requests_at, ServeCallPageCtx, .request = arguments, .root = root, .path = &path, .text = &text,
                      .size = &size, .line = &line, .err_msg = err_msg) &&
           EMBED_CALL(cli_requests_text_table.sent_at, CliRequestsTextCfg, .root = root, .path = path, .text = text,
                      .size = size, .line = line,
                      .env = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "env", .otherwise = NULL),
                      .out = out, .err_msg = err_msg);
}

static int requests_answers(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *path = NULL;
    const char *text = NULL;
    unsigned long long size = 0ull;
    unsigned long long line = 0ull;
    return EMBED_CALL(requests_at, ServeCallPageCtx, .request = arguments, .root = root, .path = &path, .text = &text,
                      .size = &size, .line = &line, .err_msg = err_msg) &&
           EMBED_CALL(cli_requests_text_table.answers, CliRequestsTextCfg, .root = root, .path = path, .text = text,
                      .size = size, .line = line, .out = out, .err_msg = err_msg);
}

static int requests_curl(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *path = NULL;
    const char *text = NULL;
    unsigned long long size = 0ull;
    unsigned long long line = 0ull;
    return EMBED_CALL(requests_at, ServeCallPageCtx, .request = arguments, .root = root, .path = &path, .text = &text,
                      .size = &size, .line = &line, .err_msg = err_msg) &&
           EMBED_CALL(cli_requests_text_table.curl, CliRequestsTextCfg, .root = root, .path = path, .text = text,
                      .size = size, .line = line,
                      .env = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "env", .otherwise = NULL),
                      .out = out, .err_msg = err_msg);
}

static int requests_from_curl(const CliServeAnswerCfg *args)
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
    return EMBED_CALL(cli_requests_table.from_curl, CliRequestsCfg, .line_text = text, .size = size, .out = out,
                      .err_msg = err_msg);
}

static int requests_private(const CliServeAnswerCfg *args)
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
           EMBED_CALL(cli_requests_text_table.private_init, CliRequestsTextCfg, .root = root, .path = path, .out = out,
                      .err_msg = err_msg);
}

static int containers_list(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_containers_out_line.list, CliContainersOutLineCfg, .out = out);
    return 1;
}

static int container_act(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    const char *const act = (name != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "act", .err_msg = err_msg)
                                           : NULL;
    if ((act == NULL) ||
        !EMBED_CALL(cli_containers.act, CliContainersCfg, .name = name, .act = act, .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

static int container_logs(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    return (name != NULL) && EMBED_CALL(cli_containers_out_line.logs, CliContainersOutLineCfg, .name = name, .out = out,
                                        .err_msg = err_msg);
}

static int container_run(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    const char *const line = (name != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "line", .err_msg = err_msg)
                                            : NULL;
    return (line != NULL) && EMBED_CALL(cli_containers_out_line.run_in, CliContainersOutLineCfg, .name = name,
                                        .line = line, .out = out, .err_msg = err_msg);
}

static int pods_list(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_containers_out_line.pods, CliContainersOutLineCfg, .out = out);
    return 1;
}

static int pod_logs(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const space = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "namespace", .err_msg = err_msg);
    const char *const name = (space != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                          .request = arguments, .key = "name", .err_msg = err_msg)
                                             : NULL;
    return (name != NULL) &&
           EMBED_CALL(cli_containers_out.pod_logs, CliContainersOutCfg, .space = space, .name = name,
                      .container = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                              .request = arguments, .key = "container", .otherwise = NULL),
                      .out = out, .err_msg = err_msg);
}

const CliServeCall g_cli_serve_page_calls[] = {
    {"pages_open", pages_open},
    {"pages_shape", pages_shape},
    {"pages_read", pages_read},
    {"pages_view", pages_view},
    {"pages_edit", pages_edit},
    {"pages_changed", pages_change},
    {"pages_find", pages_find},
    {"pages_save", pages_save},
    {"pages_release", pages_release},
    {"requests_parse", requests_parse},
    {"requests_environments", requests_environments},
    {"requests_send", requests_send},
    {"requests_answers", requests_answers},
    {"requests_curl", requests_curl},
    {"requests_from_curl", requests_from_curl},
    {"requests_private", requests_private},
    {"containers_list", containers_list},
    {"container_act", container_act},
    {"container_logs", container_logs},
    {"container_run", container_run},
    {"pods_list", pods_list},
    {"pod_logs", pod_logs},
};
const unsigned int g_cli_serve_page_count = sizeof(g_cli_serve_page_calls) / sizeof(g_cli_serve_page_calls[0]);
