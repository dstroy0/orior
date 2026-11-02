// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_editor.c: the window's calls on the editor's view of a text stored as a branch opened it: the view opened
// on the text under a key, sized to the window's rows and columns, each input the window sends taken, the minimap's
// lines asked for, and the view closed; an open and every input answered with the view's frame, which the window
// draws. The view is handed to its file's language server from the branch's own text and the server told each change
// as the window asks after the frame that shows it; the hover at a place, the completions at the cursor and the
// signature of the call the cursor is in are answered from the tree's declarations and, where the view is served and
// the call's `server` is not false, the server, whose answer waits on it: the window asks without the server first and
// with it after
#include "../editor/view.h"
#include "../plugins/plugins.h"
#include "../servers/servers.h"
#include "../servers/servers_internal.h"
#include "window_internal.h"

// the bytes an answer of the tree's or the server's is written in, the most declarations a hover shows, and the most
// bytes a word takes
#define CLI_EDITOR_ANSWER_ROOM (4ull << 20u)

// a server taking a file and the view marked served, as one step against a server letting a file go
static CliLock s_serving_lock = CLI_LOCK_INIT;
#define CLI_EDITOR_HOVER_MOST 6u
#define CLI_EDITOR_WORD 256u

typedef struct
{
    const char *key;
} WindowCallEditorCtx;
// the server of the view under `key` told the view's text where the view is served and its text changed since it was
// last told
EMBED_INLINE void editor_server_tell(const WindowCallEditorCtx *args)
{
    const char *key = args->key;
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full) ||
        !EMBED_CALL(cli_view_table.changed_take, CliViewCfg, .key = key))
    {
        return;
    }
    unsigned long long size = 0ull;
    char *const text = EMBED_CALL(cli_document_key.text_take, CliDocumentKeyCfg, .key = key, .size_pointer = &size,
                                  .owner = &g_cli_window_cells);
    if (text != NULL)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea ignored = {why, sizeof(why), 0ull};
        (void)EMBED_CALL(cli_servers_root.change, CliServersRootCfg, .path = full, .text = text, .size = size,
                         .err_msg = &ignored);
        g_cli_cells->persistent_buf_release(text, &g_cli_window_cells);
    }
}

// the view opened on the text under `key`, its indentation the file at `path`'s, `rows` by `columns`, answered with its
// first frame
static int editor_view_open(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    unsigned long long rows = 0ull;
    unsigned long long columns = 0ull;
    if ((key == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "rows",
                    .value = &rows, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "columns",
                    .value = &columns, .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_view_table.open, CliViewCfg, .key = key,
                    .path = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                       .key = "path", .otherwise = "")))
    {
        mmgr_line_put(err_msg, key);
        mmgr_line_put(err_msg, ": no text is stored under the key, or no more views can be kept");
        return 0;
    }
    // the rows and columns a window shows are far inside what an unsigned int represents
    const CliViewInput sized = {
        CLI_VIEW_SIZE, NULL, NULL, 0ull, 0u, 0ull, 0ull, 0u, 0, 0ull, (unsigned int)rows, (unsigned int)columns, 0};
    return EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &sized, .err_msg = err_msg) &&
           EMBED_CALL(cli_view_out.frame_write, CliViewOutCfg, .key = key, .out = out);
}

// one input of the window's taken by the view under `key`, `{"key", "kind", ...}` with what the kind carries: a key's
// `name`; a `text`; the pointer's `pointer`, `row`, `column`, `clicks`, `shift` and `gutter`; a scroll's `top`; a
// size's `rows` and `columns`; answered with the view's frame
static int editor_view_input(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    unsigned long long kind = 0ull;
    if ((key == NULL) || !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments,
                                     .key = "kind", .value = &kind, .err_msg = err_msg))
    {
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const text = EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &length);
    CliViewInput input;
    memor.set(&(MemoriaCfg){.dst = &input, .val = 0, .bytes = sizeof(input)});
    // a kind, a pointer's event, a click count and a size are each far inside what an unsigned int represents
    input.kind = (unsigned int)kind;
    input.name = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                            .key = "name", .otherwise = "");
    input.text = (text != NULL) ? text : "";
    input.length = length;
    input.pointer = (unsigned int)EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                             .request = arguments, .key = "pointer", .otherwise_u64 = 0ull);
    input.row = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments, .key = "row",
                           .otherwise_u64 = 0ull);
    input.column = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                              .key = "column", .otherwise_u64 = 0ull);
    input.clicks = (unsigned int)EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                            .request = arguments, .key = "clicks", .otherwise_u64 = 1ull);
    input.shift = EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = arguments,
                             .key = "shift", .otherwise_i32 = 0);
    input.top = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments, .key = "top",
                           .otherwise_u64 = 0ull);
    input.rows = (unsigned int)EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "rows", .otherwise_u64 = 0ull);
    input.columns = (unsigned int)EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                             .request = arguments, .key = "columns", .otherwise_u64 = 0ull);
    input.gutter = EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = arguments,
                              .key = "gutter", .otherwise_i32 = 0);
    if (!EMBED_CALL(cli_view_input_table.input, CliViewInputCfg, .key = key, .input = &input, .err_msg = err_msg))
    {
        return 0;
    }
    return EMBED_CALL(cli_view_out.frame_write, CliViewOutCfg, .key = key, .out = out);
}

// the server of the view under `key` told the view's text where it changed since it was last told: null; the window
// calls this off the inputs' lane after each frame, the telling waiting on the server
static int editor_view_tell(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if (key == NULL)
    {
        return 0;
    }
    EMBED_CALL(editor_server_tell, WindowCallEditorCtx, .key = key);
    mmgr_line_put(args->out, "null");
    return 1;
}

// the view under `key` handed to its file's language server, where the text is read whole and its language has a
// server that takes it: `{"served": true}`, else `{"served": false}`
static int editor_view_serve(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    char path[CLI_PATH_MAX_LEN];
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (key == NULL)
    {
        return 0;
    }
    if (!EMBED_CALL(cli_view_path_kind.path, CliViewPathKindCfg, .key = key, .path_out = path))
    {
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
    char id[CLI_NAME_MAX_LEN];
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    const int known = EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                 .languages_read = &languages, .path = path, .id_out = id);
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &languages);
    unsigned long long size = 0ull;
    char *const text = known ? EMBED_CALL(cli_document_key.text_take, CliDocumentKeyCfg, .key = key,
                                          .size_pointer = &size, .owner = &g_cli_window_cells)
                             : NULL;
    int served = 0;
    if (text != NULL)
    {
        char said_text[16];
        VerbaLinea said = {said_text, sizeof(said_text), 0ull};
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea refused = {why, sizeof(why), 0ull};
        const CliServersSink sink = EMBED_CALL(cli_serve_internal.emitter, CliServeCtx, .tree = root);
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_serving_lock);
        served = EMBED_CALL(cli_servers_root.open, CliServersRootCfg, .root = root, .path = full, .language = id,
                            .text = text, .size = size, .sink = &sink, .out = &said, .err_msg = &refused) &&
                 mmgr_line_ok(&said) && (said.at == 4ull) &&
                 (memor.cmp(&(MemoriaCfg){.src = said.out, .other = "true", .bytes = 4u}) == 0);
        // a view closed while the server took its file leaves the file with no view to let it go
        if (served && !EMBED_CALL(cli_view_full.served_set, CliViewFullCfg, .key = key, .full = full))
        {
            served = 0;
            if (!EMBED_CALL(cli_view_full.serving, CliViewFullCfg, .full = full))
            {
                refused.at = 0ull;
                (void)EMBED_CALL(cli_servers_path_out_err_msg_from_to.close, CliServersPathOutErrMsgFromToCfg,
                                 .path = full, .err_msg = &refused);
            }
        }
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_serving_lock);
        g_cli_cells->persistent_buf_release(text, &g_cli_window_cells);
    }
    mmgr_line_put(out, served ? "{\"served\":true}" : "{\"served\":false}");
    return 1;
}

// the hover at the cell `column` of the row `row` from the top of the view under `key`: the word the place touches,
// `{"from", "to", "parts"}`, the places `{"line", "col"}` in units, the parts the tree's declarations of the word, each
// its own line as code with its kind and place after, and the server's hover as Markdown where the view is served;
// null where no word stands at the place
static int editor_view_hover(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    unsigned long long row = 0ull;
    unsigned long long column = 0ull;
    if ((key == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "row",
                    .value = &row, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "column",
                    .value = &column, .err_msg = err_msg))
    {
        return 0;
    }
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    if (!EMBED_CALL(cli_view_row.place_at, CliViewRowCfg, .key = key, .row = row, .column = column, .line = &line,
                    .col = &col))
    {
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    char word[CLI_EDITOR_WORD];
    if (!EMBED_CALL(cli_view_line.word_at, CliViewLineCfg, .key = key, .line_value = line, .col_value = col,
                    .from = &from, .to = &to, .word = word, .room = sizeof(word)))
    {
        mmgr_line_put(out, "null");
        return 1;
    }
    mmgr_line_put(out, "{\"from\":{\"line\":");
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_put(out, ",\"col\":");
    mmgr_line_decimal(out, from, 1u);
    mmgr_line_put(out, "},\"to\":{\"line\":");
    mmgr_line_decimal(out, line, 1u);
    mmgr_line_put(out, ",\"col\":");
    mmgr_line_decimal(out, to, 1u);
    mmgr_line_put(out, "},\"parts\":[");
    unsigned int parts = 0u;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    // the tree's declarations of the word
    char query_text[CLI_EDITOR_WORD + 32u];
    VerbaLinea query = {query_text, sizeof(query_text), 0ull};
    mmgr_line_put(&query, "{\"query\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = &query, .text = word);
    mmgr_line_ch(&query, '}');
    char *const found_cell = g_cli_cells->temporary_buf_alloc(CLI_EDITOR_ANSWER_ROOM, &g_cli_window_cells);
    VerbaLinea found = mmgr_line_over(found_cell, CLI_EDITOR_ANSWER_ROOM);
    CliJson json;
    const unsigned int root =
        (mmgr_line_ok(&query) &&
         EMBED_CALL(cli_window_out_size_internal.tree_call, CliWindowOutSizeCtx, .name = "symbols_find",
                    .args_text = query.out, .size = query.at, .out = &found, .err_msg = &ignored) &&
         mmgr_line_ok(&found))
            ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                         .text = found.out, .size = found.at, .nodes_more = 0u, .text_more = 0ull, .err_msg = &ignored,
                         .owner = &g_cli_window_cells)
            : CLI_JSON_NONE;
    const unsigned int symbols = (root != CLI_JSON_NONE)
                                     ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = &json, .object = root, .name = "symbols")
                                     : CLI_JSON_NONE;
    const unsigned int count =
        ((symbols != CLI_JSON_NONE) && EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                  .json_read = &json, .node = symbols, .kind = CFG_JSON_ARRAY))
            ? json.node[symbols].count
            : 0u;
    for (unsigned int at = 0u; (at < count) && (parts < CLI_EDITOR_HOVER_MOST); at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = &json, .array = symbols, .index = at);
        unsigned long long name_length = 0ull;
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = one, .name = "name"),
                       .length_pointer = &name_length);
        if ((name == NULL) || (name_length != cellul.len(&(CatenaFinitaCfg){.src = word, .cap = sizeof(word)})) ||
            (memor.cmp(&(MemoriaCfg){.src = name, .other = word, .bytes = name_length}) != 0))
        {
            continue;
        }
        unsigned long long text_length = 0ull;
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = one, .name = "text"),
                       .length_pointer = &text_length);
        unsigned long long kind_length = 0ull;
        const char *const kind =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = one, .name = "kind"),
                       .length_pointer = &kind_length);
        unsigned long long path_length = 0ull;
        const char *const path =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = one, .name = "path"),
                       .length_pointer = &path_length);
        const unsigned long long declared =
            EMBED_CALL(cli_servers_json_node_otherwise_internal.whole, CliServersJsonNodeOtherwiseCtx, .json = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = one, .name = "line"),
                       .otherwise = 0ull);
        const unsigned long long part_room = text_length + kind_length + path_length + 64ull;
        char *const part_cell = g_cli_cells->temporary_buf_alloc(part_room, &g_cli_window_cells);
        VerbaLinea part = mmgr_line_over(part_cell, part_room);
        if (part.out == NULL)
        {
            break;
        }
        if ((text != NULL) && (text_length != 0ull))
        {
            mmgr_line_put(&part, "```\n");
            mmgr_line_put_n(&part, text, text_length);
            mmgr_line_put(&part, "\n```\n\n");
        }
        mmgr_line_put_n(&part, (kind != NULL) ? kind : "", kind_length);
        mmgr_line_put(&part, " `");
        mmgr_line_put_n(&part, (path != NULL) ? path : "", path_length);
        mmgr_line_ch(&part, ':');
        mmgr_line_decimal(&part, declared + 1ull, 1u);
        mmgr_line_ch(&part, '`');
        if (mmgr_line_ok(&part))
        {
            mmgr_line_put(out, (parts > 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = part.out,
                       .length = part.at);
            parts += 1u;
        }
        g_cli_cells->persistent_buf_release(part.out, &g_cli_window_cells);
    }
    if (root != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    }
    g_cli_cells->persistent_buf_release(found.out, &g_cli_window_cells);
    // the server's hover, a Markdown string, where the view is served and the server is asked
    char full[CLI_PATH_MAX_LEN];
    char *const said_cell = (EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "server", .otherwise_i32 = 1) &&
                             EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full))
                                ? g_cli_cells->temporary_buf_alloc(CLI_EDITOR_ANSWER_ROOM, &g_cli_window_cells)
                                : NULL;
    VerbaLinea said = mmgr_line_over(said_cell, CLI_EDITOR_ANSWER_ROOM);
    // a place in a text a view shows stands below 2^32 lines and columns
    if (EMBED_CALL(cli_servers_path_out.hover, CliServersPathOutCfg, .path = full, .line = (unsigned int)line,
                   .col = (unsigned int)col, .out = &said, .err_msg = &ignored) &&
        mmgr_line_ok(&said) && (said.at > 0ull) && (said.out[0] == '"'))
    {
        mmgr_line_put(out, (parts > 0u) ? "," : "");
        mmgr_line_put_n(out, said.out, said.at);
        parts += 1u;
    }
    g_cli_cells->persistent_buf_release(said.out, &g_cli_window_cells);
    mmgr_line_put(out, "]}");
    return 1;
}

// the completions at the cursor of the view under `key`: `{"from", "prefix", "items", "words"}`, the prefix the word
// before the cursor and `from` its first unit, the items the server's where the view is served, else none, and the
// words the file's own near the cursor
static int editor_view_complete(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    unsigned long long from = 0ull;
    char prefix[CLI_EDITOR_WORD];
    CliPlace cursor;
    CliPlace anchor;
    if (key == NULL)
    {
        return 0;
    }
    if (!EMBED_CALL(cli_view_from.prefix, CliViewFromCfg, .key = key, .from = &from, .prefix = prefix,
                    .room = sizeof(prefix)) ||
        !EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = key, .cursor = &cursor, .anchor = &anchor))
    {
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    mmgr_line_put(out, "{\"from\":{\"line\":");
    mmgr_line_decimal(out, cursor.line, 1u);
    mmgr_line_put(out, ",\"col\":");
    mmgr_line_decimal(out, from, 1u);
    mmgr_line_put(out, "},\"prefix\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = prefix);
    mmgr_line_put(out, ",\"items\":");
    char full[CLI_PATH_MAX_LEN];
    char *const said_cell = (EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "server", .otherwise_i32 = 1) &&
                             EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full))
                                ? g_cli_cells->temporary_buf_alloc(CLI_EDITOR_ANSWER_ROOM, &g_cli_window_cells)
                                : NULL;
    VerbaLinea said = mmgr_line_over(said_cell, CLI_EDITOR_ANSWER_ROOM);
    int answered = 0;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    // a place in a text a view shows stands below 2^32 lines and columns
    if (EMBED_CALL(cli_servers_path_out.complete, CliServersPathOutCfg, .path = full, .line = (unsigned int)cursor.line,
                   .col = (unsigned int)cursor.col, .out = &said, .err_msg = &ignored) &&
        mmgr_line_ok(&said) && (said.at > 0ull) && (said.out[0] == '['))
    {
        mmgr_line_put_n(out, said.out, said.at);
        answered = 1;
    }
    g_cli_cells->persistent_buf_release(said.out, &g_cli_window_cells);
    mmgr_line_put(out, answered ? ",\"words\":" : "[],\"words\":");
    (void)EMBED_CALL(cli_view_out.words_write, CliViewOutCfg, .key = key, .out = out);
    mmgr_line_ch(out, '}');
    return 1;
}

// the signature of the call the cursor of the view under `key` is in: `{"own", "server"}`, the tree's as
// `symbols_signature` writes it, or null, and the server's where the view is served, or null
static int editor_view_signature(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    char path[CLI_PATH_MAX_LEN];
    CliPlace cursor;
    CliPlace anchor;
    if (key == NULL)
    {
        return 0;
    }
    if (!EMBED_CALL(cli_view_path_kind.path, CliViewPathKindCfg, .key = key, .path_out = path) ||
        !EMBED_CALL(cli_view_cursor_table.cursor, CliViewCursorCfg, .key = key, .cursor = &cursor, .anchor = &anchor))
    {
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    mmgr_line_put(out, "{\"own\":");
    char *const own_cell = g_cli_cells->temporary_buf_alloc(CLI_EDITOR_ANSWER_ROOM, &g_cli_window_cells);
    VerbaLinea own = mmgr_line_over(own_cell, CLI_EDITOR_ANSWER_ROOM);
    int answered = 0;
    if (EMBED_CALL(cli_window_out_internal.branch_signature, CliWindowOutCtx, .key = key, .path = path,
                   .line = cursor.line, .col = cursor.col, .out = &own, .err_msg = &ignored) &&
        mmgr_line_ok(&own) && (own.at > 0ull))
    {
        mmgr_line_put_n(out, own.out, own.at);
        answered = 1;
    }
    g_cli_cells->persistent_buf_release(own.out, &g_cli_window_cells);
    mmgr_line_put(out, answered ? ",\"server\":" : "null,\"server\":");
    char full[CLI_PATH_MAX_LEN];
    char *const said_cell = (EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "server", .otherwise_i32 = 1) &&
                             EMBED_CALL(cli_view_full_kind.serve, CliViewFullKindCfg, .key = key, .full_out = full))
                                ? g_cli_cells->temporary_buf_alloc(CLI_EDITOR_ANSWER_ROOM, &g_cli_window_cells)
                                : NULL;
    VerbaLinea said = mmgr_line_over(said_cell, CLI_EDITOR_ANSWER_ROOM);
    answered = 0;
    // a place in a text a view shows stands below 2^32 lines and columns
    if (EMBED_CALL(cli_servers_path_out.signature, CliServersPathOutCfg, .path = full,
                   .line = (unsigned int)cursor.line, .col = (unsigned int)cursor.col, .out = &said,
                   .err_msg = &ignored) &&
        mmgr_line_ok(&said) && (said.at > 0ull))
    {
        mmgr_line_put_n(out, said.out, said.at);
        answered = 1;
    }
    g_cli_cells->persistent_buf_release(said.out, &g_cli_window_cells);
    mmgr_line_put(out, answered ? "}" : "null}");
    return 1;
}

// the minimap's lines from `first` to `last` of the view under `key`, answered as their bars
static int editor_view_map(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    if ((key == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "first",
                    .value = &first, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "last",
                    .value = &last, .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_view_out.map_write, CliViewOutCfg, .key = key, .first = first, .last = last, .out = out))
    {
        mmgr_line_put(err_msg, "no view is open under the key");
        return 0;
    }
    return 1;
}

// the view under `key` closed, on the lane its open comes on, so a later open under the key finds it closed
static int editor_view_close(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if (key == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_view_table.close, CliViewCfg, .key = key);
    mmgr_line_put(args->out, "null");
    return 1;
}

// the server of the file at `path`, from the tree's top, let go of it, unless a view of the file is served by now
static int editor_server_close(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    char root[CLI_PATH_MAX_LEN];
    if ((path == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                      .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_serving_lock);
    if (!EMBED_CALL(cli_view_full.serving, CliViewFullCfg, .full = full))
    {
        (void)EMBED_CALL(cli_servers_path_out_err_msg_from_to.close, CliServersPathOutErrMsgFromToCfg, .path = full,
                         .err_msg = &ignored);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_serving_lock);
    mmgr_line_put(args->out, "null");
    return 1;
}

const CliServeCall g_cli_window_editor_calls[] = {
    {"editor_view_open", editor_view_open},         {"editor_view_input", editor_view_input},
    {"editor_view_told", editor_view_tell},         {"editor_view_map", editor_view_map},
    {"editor_view_serve", editor_view_serve},       {"editor_view_hover", editor_view_hover},
    {"editor_view_complete", editor_view_complete}, {"editor_view_signature", editor_view_signature},
    {"editor_view_close", editor_view_close},       {"editor_server_close", editor_server_close},
};
const unsigned int g_cli_window_editor_count = sizeof(g_cli_window_editor_calls) / sizeof(g_cli_window_editor_calls[0]);
