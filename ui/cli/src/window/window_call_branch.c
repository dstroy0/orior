// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_branch.c: a branch of the tree opened in the editor: the file read as the tree reads it, a text read
// whole stored under the page's key, one too large to read whole opened in the pages under it, and one as a commit left
// it stored read only, each colored by the grammar its extension points to, the lines in view colored at once and the
// rest walked out from them, each line colored after the answer handed to the page as `branch-colors`; the view moved,
// and the lines of a branch the page mirrors; the language a branch is colored in chosen, its lines colored again and
// handed to the page; the tree's signature for a call in a branch and the bracket pair at a place in one, each read off
// the branch's own lines; the colors of a branch's lines as a print of them asks; and a notebook's cells, each a branch
// stored and colored, and a cell the reader adds
#include "../code/symbols.h"
#include "../command_line/callback_cfg.h"
#include "../editor/document.h"
#include "../editor/pages.h"
#include "../plugins/plugins.h"
#include "window_internal.h"

// the most lines above a call's line its signature is read from, the most lines above and below a place its bracket
// pair is read across, the room the tree's call is written in, a state's longest text, and the first lines of a branch
// stored for itself, a cell's, colored as it is stored
#define CLI_BRANCH_SIGNATURE_LINES 4000ull
#define CLI_BRANCH_PAIR_LINES 3000ull
#define CLI_BRANCH_CALL_ROOM (4ull << 20u)
#define CLI_BRANCH_STATE 1024u
#define CLI_BRANCH_FIRST_LINES 120ull

// a branch's colors handed to the page as they are worked out
static void branch_colors_send(const CliDocColoredCfg *args)
{
    const char *body = args->body;
    unsigned long long size = args->size;
    EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "branch-colors", .body = body, .size = size);
}

typedef struct
{
    const char *id;
    char *grammar;
    const char *key;
    VerbaLinea *out;
    const char *path;
    char *id_out;
    const char *text;
    unsigned long long size;
    unsigned long long first;
    unsigned long long last;
    unsigned long long line;
    unsigned long long above;
    unsigned long long below;
    unsigned long long *first_pointer;
    unsigned long long *last_pointer;
} WindowCallBranchCtx;
// the key the grammar of the language `id` is stored under for the colorer into `grammar`, sized for CLI_NAME_MAX_LEN,
// empty where the language has none
EMBED_INLINE void branch_grammar_by_name(const WindowCallBranchCtx *args)
{
    const char *id = args->id;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if ((id[0] == '\0') || !EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = id,
                                       .key = args->grammar, .err_msg = &err_msg))
    {
        args->grammar[0] = '\0';
    }
}

// the branch under `key` colored in the language `id`, empty for none: the names its tokens take written into `out`
// as cli_document_names_write writes them, where `out` is not NULL, else handed on with its first colors
EMBED_INLINE void branch_colored_in(const WindowCallBranchCtx *args)
{
    const char *key = args->key;
    VerbaLinea *out = args->out;
    char grammar[CLI_NAME_MAX_LEN];
    EMBED_CALL(branch_grammar_by_name, WindowCallBranchCtx, .id = args->id, .grammar = grammar);
    (void)EMBED_CALL(cli_document_key_grammar.colored_by, CliDocumentKeyGrammarCfg, .key = key, .grammar = grammar,
                     .named = out != NULL);
    if (out != NULL)
    {
        EMBED_CALL(cli_document_out_grammar.names_write, CliDocumentOutGrammarCfg, .grammar = grammar, .out = out);
    }
}

// the language the extension of `path` points to, by its id, into `id`, sized for CLI_NAME_MAX_LEN, empty for none
EMBED_INLINE void branch_language_get(const WindowCallBranchCtx *args)
{
    const char *path = args->path;
    char *id = args->id_out;
    CliLanguages languages;
    EMBED_CALL(cli_plugins_table.languages_for_call, CliPluginsCfg, .languages = &languages);
    if (!EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg, .languages_read = &languages,
                    .path = path, .id_out = id))
    {
        id[0] = '\0';
    }
    EMBED_CALL(cli_plugins_table.languages_free, CliPluginsCfg, .languages = &languages);
}

// `size` bytes of `text` the branch under `key`, the page mirroring its lines from `first` to `last`: 1, or 0 where it
// could not be stored
EMBED_INLINE int branch_store(const WindowCallBranchCtx *args)
{
    const char *key = args->key;
    const char *text = args->text;
    unsigned long long size = args->size;
    CliShape shape;
    return EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = key, .text = text, .size = size,
                      .shape = &shape) &&
           EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = key, .from = args->first,
                      .to = args->last + 1ull);
}

// the branch under `key` colored in the language `id` and its lines from `first` to `last` colored, written into `out`
// as `,"names":` the names its grammar's tokens take `,"colors":` those lines' colors
EMBED_INLINE void branch_colors_answer(const WindowCallBranchCtx *args)
{
    const char *key = args->key;
    unsigned long long first = args->first;
    unsigned long long last = args->last;
    VerbaLinea *out = args->out;
    mmgr_line_put(out, ",\"names\":");
    EMBED_CALL(branch_colored_in, WindowCallBranchCtx, .key = key, .id = args->id, .out = out);
    mmgr_line_put(out, ",\"colors\":");
    (void)EMBED_CALL(cli_document_out_key_first.view, CliDocumentOutKeyFirstCfg, .key = key, .first = first,
                     .last = last, .out = out);
}

// the file at `path` read as file_read reads it, a text read whole stored under `key` and one file_read windows opened
// in the pages under `key`, either one's lines from `first` to `last` the page's mirror and the text colored in the
// language its extension points to, each line as far as `columns` letters where it is named, and the lines from
// `first` to `last` colored: file_read's answer with the letters each line is colored to, `columns`, 0 for every one,
// the paged
// text's shape, `shape`, null where the pages could not open it, the names its grammar's tokens take, `names`, and
// those lines' colors, `colors`, where it is stored
static int branch_open(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const char *const path = (key != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "path", .err_msg = err_msg)
                                           : NULL;
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "first",
                    .value = &first, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "last",
                    .value = &last, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = branch_colors_send, .threaded = 1);
    CliJson json;
    const unsigned int opened = EMBED_CALL(cli_window_err_msg_internal.tree_call, CliWindowErrMsgCtx, .json = &json,
                                           .name = "file_read", .path = path, .more = "", .err_msg = err_msg);
    if (opened == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = opened, .name = "text"),
                   .length_pointer = &length);
    const int windowed = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                       .json_read = &json, .object = opened, .name = "windowed"),
                                    .kind = CFG_JSON_TRUE);
    // a text read whole a branch whose lines in view are the page's mirror, until it names more
    const int whole = (text != NULL) && EMBED_CALL(branch_store, WindowCallBranchCtx, .key = key, .text = text,
                                                   .size = length, .first = first, .last = last);
    const unsigned long long start = out->at;
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = opened, .out = out,
               .indent = 0u, .sorted = 0);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    // the answer's closing brace, put back after the branch's names and colors
    out->at -= (out->at > start) ? 1ull : 0ull;
    // a text too large to read whole opened in the pages under the key, its shape answered, and a branch whose lines in
    // view are the page's mirror
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (windowed)
    {
        mmgr_line_put(out, ",\"shape\":");
    }
    const unsigned long long shaped = out->at;
    const int paged =
        windowed &&
        EMBED_CALL(cli_serve_err_msg_request_root_internal.at, CliServeErrMsgRequestRootCtx, .request = arguments,
                   .root = root, .full = full, .err_msg = err_msg) &&
        EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = key, .path = full, .out = out, .err_msg = err_msg) &&
        EMBED_CALL(cli_document_key_from.page, CliDocumentKeyFromCfg, .key = key, .from = first, .to = last + 1ull);
    if (windowed && !paged)
    {
        out->at = shaped;
        mmgr_line_put(out, "null");
    }
    if (whole || paged)
    {
        // each line's runs handed as far as the view draws, where it names how far, and the answer says how far
        const unsigned long long columns = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                                      .request = arguments, .key = "columns", .otherwise_u64 = 0ull);
        if (columns != 0ull)
        {
            (void)EMBED_CALL(cli_document_key_columns.columns, CliDocumentKeyColumnsCfg, .key = key,
                             .columns = columns);
        }
        mmgr_line_put(out, ",\"columns\":");
        mmgr_line_decimal(out, columns, 1u);
        char id[CLI_NAME_MAX_LEN];
        EMBED_CALL(branch_language_get, WindowCallBranchCtx, .path = path, .id_out = id);
        EMBED_CALL(branch_colors_answer, WindowCallBranchCtx, .key = key, .id = id, .first = first, .last = last,
                   .out = out);
    }
    mmgr_line_ch(out, '}');
    return 1;
}

// the file at `path` as the commit `at` left it, read through `file_at`, the branch under `key`, read only, its lines
// from `first` to `last` the page's mirror and the text colored in the language its extension points to: `{"text",
// "names", "colors"}`, the names and colors where it is stored
static int branch_at(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const char *const path = (key != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "path", .err_msg = err_msg)
                                           : NULL;
    const char *const at = (path != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                       .request = arguments, .key = "at", .err_msg = err_msg)
                                          : NULL;
    unsigned long long first = 0ull;
    unsigned long long last = 0ull;
    if ((at == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "first",
                    .value = &first, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "last",
                    .value = &last, .err_msg = err_msg))
    {
        return 0;
    }
    char more[CLI_NAME_MAX_LEN * 2u];
    VerbaLinea commit = {more, sizeof(more), 0ull};
    mmgr_line_put(&commit, ",\"id\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = &commit, .text = at);
    if (mmgr_line_finish(&commit) == 0ull)
    {
        mmgr_line_put(err_msg, "the commit's id is longer than an id");
        return 0;
    }
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = branch_colors_send, .threaded = 1);
    CliJson json;
    const unsigned int read = EMBED_CALL(cli_window_err_msg_internal.tree_call, CliWindowErrMsgCtx, .json = &json,
                                         .name = "file_at", .path = path, .more = more, .err_msg = err_msg);
    if (read == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        return 0;
    }
    unsigned long long length = 0ull;
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = &json, .node = read, .length_pointer = &length);
    const int stored = (text != NULL) && EMBED_CALL(branch_store, WindowCallBranchCtx, .key = key, .text = text,
                                                    .size = length, .first = first, .last = last);
    mmgr_line_put(out, "{\"text\":");
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = read, .out = out,
               .indent = 0u, .sorted = 0);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    if (stored)
    {
        char id[CLI_NAME_MAX_LEN];
        EMBED_CALL(branch_language_get, WindowCallBranchCtx, .path = path, .id_out = id);
        EMBED_CALL(branch_colors_answer, WindowCallBranchCtx, .key = key, .id = id, .first = first, .last = last,
                   .out = out);
    }
    mmgr_line_ch(out, '}');
    return 1;
}

// the view of the branch under `key` moved to the lines from `first` to `last`, those not colored yet colored and
// handed to the page, the lines the page mirrors about the view, from `from` to before `to`, and the letters of each
// line the view draws, `columns`, 0 for every one, where it names them
static int branch_view(const CliServeAnswerCfg *args)
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
    const unsigned long long none = ~0ull;
    const unsigned long long columns = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                                  .request = arguments, .key = "columns", .otherwise_u64 = none);
    if (columns != none)
    {
        (void)EMBED_CALL(cli_document_key_columns.columns, CliDocumentKeyColumnsCfg, .key = key, .columns = columns);
    }
    const unsigned long long from = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                               .request = arguments, .key = "from", .otherwise_u64 = none);
    const unsigned long long to = EMBED_CALL(cli_serve_err_msg_internal.whole_or, CliServeErrMsgCtx,
                                             .request = arguments, .key = "to", .otherwise_u64 = none);
    if ((from != none) && (to != none))
    {
        (void)EMBED_CALL(cli_document_key_from.cache, CliDocumentKeyFromCfg, .key = key, .from = from, .to = to);
    }
    (void)EMBED_CALL(cli_document_out_key_first.view, CliDocumentOutKeyFirstCfg, .key = key, .first = first,
                     .last = last, .out = NULL);
    mmgr_line_put(out, "null");
    return 1;
}

// the branch under `key` colored in the language `id` from here on, empty for none, its lines colored again from the
// view out and handed to the page, the names its grammar's tokens take with the first
static int branch_language(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const char *const id = (key != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                                      .request = arguments, .key = "id", .otherwise = "")
                                         : NULL;
    if (id == NULL)
    {
        return 0;
    }
    EMBED_CALL(branch_colored_in, WindowCallBranchCtx, .key = key, .id = id, .out = NULL);
    mmgr_line_put(out, "null");
    return 1;
}

// the lines of the branch under `key` read about its line `line`, as many as `above` above it and `below` below it at
// the most and none outside those read for the branch, a paged text's those the page holds: the first into `*first`
// and the last into `*last`, each the line itself where it stands past them
EMBED_INLINE void branch_lines_about(const WindowCallBranchCtx *args)
{
    const char *key = args->key;
    unsigned long long *first = args->first_pointer;
    unsigned long long *last = args->last_pointer;
    unsigned long long low = 0ull;
    unsigned long long high = 0ull;
    (void)EMBED_CALL(cli_document_key_low.bounds, CliDocumentKeyLowCfg, .key = key, .low = &low, .high = &high);
    const unsigned long long reach = (args->line > args->above) ? (args->line - args->above) : 0ull;
    const unsigned long long start = (reach > low) ? reach : low;
    const unsigned long long end =
        ((high != 0ull) && ((args->line + args->below) >= high)) ? (high - 1ull) : (args->line + args->below);
    *first = (start < args->line) ? start : args->line;
    *last = (end > args->line) ? end : args->line;
}

// the tree's signature for the call at the line `line`, column `col`, of the branch under `key`, the file at
// `path`: the text's lines from as far above as CLI_BRANCH_SIGNATURE_LINES reach to the line, a paged text's those the
// page holds, read as the tree reads a text from the state the first was entered in, the tree's answer as
// `symbols_signature` gives it
static int branch_signature(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const char *const path = (key != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "path", .err_msg = err_msg)
                                           : NULL;
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "line",
                    .value = &line, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "col",
                    .value = &col, .err_msg = err_msg))
    {
        return 0;
    }
    return EMBED_CALL(cli_window_out_internal.branch_signature, CliWindowOutCtx, .key = key, .path = path, .line = line,
                      .col = col, .out = out, .err_msg = err_msg);
}

int cli_window_branch_signature(const CliWindowOutCtx *args)
{
    const char *key = args->key;
    VerbaLinea *out = args->out;
    unsigned long long top = 0ull;
    unsigned long long bottom = 0ull;
    EMBED_CALL(branch_lines_about, WindowCallBranchCtx, .key = key, .line = args->line,
               .above = CLI_BRANCH_SIGNATURE_LINES - 1ull, .below = 0ull, .first_pointer = &top,
               .last_pointer = &bottom);
    char *const call_cell = g_cli_cells->temporary_buf_alloc(CLI_BRANCH_CALL_ROOM, &g_cli_window_cells);
    VerbaLinea call = mmgr_line_over(call_cell, CLI_BRANCH_CALL_ROOM);
    if (call.out == NULL)
    {
        mmgr_line_put(args->err_msg, "the signature's call could not be given a cell");
        return 0;
    }
    char state[CLI_BRANCH_STATE];
    mmgr_line_put(&call, "{\"path\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = &call, .text = args->path);
    mmgr_line_put(&call, ",\"text\":");
    const int stored = EMBED_CALL(cli_document_out_key.text_write, CliDocumentOutKeyCfg, .key = key, .first = top,
                                  .last = args->line, .out = &call, .state = state, .room = sizeof(state));
    mmgr_line_put(&call, ",\"state\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = &call,
               .text = (state[0] != '\0') ? state : "root");
    mmgr_line_put(&call, ",\"line\":");
    mmgr_line_decimal(&call, args->line - top, 1u);
    mmgr_line_put(&call, ",\"col\":");
    mmgr_line_decimal(&call, args->col, 1u);
    mmgr_line_ch(&call, '}');
    int answered = 0;
    if (!stored)
    {
        mmgr_line_put(args->err_msg, "no branch is stored under ");
        mmgr_line_put(args->err_msg, key);
    }
    else if (mmgr_line_ok(&call))
    {
        answered = EMBED_CALL(cli_window_out_size_internal.tree_call, CliWindowOutSizeCtx, .name = "symbols_signature",
                              .args_text = call.out, .size = call.at, .out = out, .err_msg = args->err_msg);
    }
    g_cli_cells->persistent_buf_release(call.out, &g_cli_window_cells);
    return answered;
}

// the bracket pair at the line `line`, column `col`, of the branch under `key`, the file at `path`: the text's lines
// from as far above and below as CLI_BRANCH_PAIR_LINES reach, a paged text's those the page holds, read as the tree
// reads a text from the state the first was entered in, the bracket that ends at the place, else the one that starts at
// it, and the one it pairs with, written as `[{"line", "col"}, {"line", "col"}]`, or null where no bracket of a pair
// stands there
static int branch_pair(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const char *const path = (key != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "path", .err_msg = err_msg)
                                           : NULL;
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "line",
                    .value = &line, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "col",
                    .value = &col, .err_msg = err_msg))
    {
        return 0;
    }
    unsigned long long top = 0ull;
    unsigned long long bottom = 0ull;
    EMBED_CALL(branch_lines_about, WindowCallBranchCtx, .key = key, .line = line, .above = CLI_BRANCH_PAIR_LINES,
               .below = CLI_BRANCH_PAIR_LINES, .first_pointer = &top, .last_pointer = &bottom);
    char state[CLI_BRANCH_STATE];
    unsigned long long size = 0ull;
    char *const text =
        EMBED_CALL(cli_document_key.lines_take, CliDocumentKeyCfg, .key = key, .first = top, .last = bottom,
                   .size_pointer = &size, .state = state, .room = sizeof(state), .owner = &g_cli_window_cells);
    if (text == NULL)
    {
        mmgr_line_put(err_msg, "no branch is stored under ");
        mmgr_line_put(err_msg, key);
        return 0;
    }
    CliSymbolPlace pair[2] = {{0u, 0u}, {0u, 0u}};
    // a place in a text the page holds stands below 2^32 lines and columns
    const int paired =
        EMBED_CALL(cli_symbols_path_text_size_state.pair, CliSymbolsPathTextSizeStateCfg, .path = path,
                   .state = (state[0] != '\0') ? state : "root", .text = text, .size = size,
                   .at_symbol_place = (CliSymbolPlace){(unsigned int)(line - top), (unsigned int)col}, .pair = pair);
    g_cli_cells->persistent_buf_release(text, &g_cli_window_cells);
    if (!paired)
    {
        mmgr_line_put(args->out, "null");
        return 1;
    }
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? ",{\"line\":" : "{\"line\":");
        mmgr_line_decimal(args->out, top + pair[at].line, 1u);
        mmgr_line_put(args->out, ",\"col\":");
        mmgr_line_decimal(args->out, pair[at].col, 1u);
        mmgr_line_ch(args->out, '}');
    }
    mmgr_line_ch(args->out, ']');
    return 1;
}

// `size` bytes of `text` the branch under `key`, its first lines the page's mirror and colored in the language `id`,
// written into `out` as `{"key", "names", "colors"}`, or null where it could not be stored
EMBED_INLINE void branch_text_answer(const WindowCallBranchCtx *args)
{
    const char *text = args->text;
    VerbaLinea *out = args->out;
    if (!EMBED_CALL(branch_store, WindowCallBranchCtx, .key = args->key, .text = text, .size = args->size,
                    .first = 0ull, .last = CLI_BRANCH_FIRST_LINES - 1ull))
    {
        mmgr_line_put(out, "null");
        return;
    }
    mmgr_line_put(out, "{\"key\":");
    EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = args->key);
    EMBED_CALL(branch_colors_answer, WindowCallBranchCtx, .key = args->key, .id = args->id, .first = 0ull,
               .last = CLI_BRANCH_FIRST_LINES - 1ull, .out = out);
    mmgr_line_ch(out, '}');
}

// the cells of the notebook at `path`, read as notebook_read reads them, each cell's text a branch under `key`, `#` and
// its place among the cells, a code cell colored in the language `code` and a Markdown cell in `markdown`:
// `[{"key", "names", "colors"}]` in the cells' order, null for a cell of another kind
static int branch_cells(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const char *const path = (key != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "path", .err_msg = err_msg)
                                           : NULL;
    if (path == NULL)
    {
        return 0;
    }
    const char *const code = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                        .key = "code", .otherwise = "");
    const char *const markdown = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                            .request = arguments, .key = "markdown", .otherwise = "");
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = branch_colors_send, .threaded = 1);
    CliJson json;
    const unsigned int book = EMBED_CALL(cli_window_err_msg_internal.tree_call, CliWindowErrMsgCtx, .json = &json,
                                         .name = "notebook_read", .path = path, .more = "", .err_msg = err_msg);
    if (book == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        return 0;
    }
    const unsigned int cells = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = book, .name = "cells");
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = &json, .array = cells, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int cell = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = &json, .array = cells, .index = at);
        const char *const kind =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = cell, .name = "cell_type"),
                       .length_pointer = NULL);
        const int markdown_cell =
            (kind != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "markdown", .cap = CLI_CELLS_POOL});
        const int code_cell =
            (kind == NULL) || cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "code", .cap = CLI_CELLS_POOL});
        unsigned long long length = 0ull;
        const char *const source =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = cell, .name = "source"),
                       .length_pointer = &length);
        char cell_key[CLI_NAME_MAX_LEN];
        VerbaLinea named = {cell_key, sizeof(cell_key), 0ull};
        mmgr_line_put(&named, key);
        mmgr_line_ch(&named, '#');
        mmgr_line_decimal(&named, at, 1u);
        const int keyed = mmgr_line_finish(&named) != 0ull;
        mmgr_line_put(out, (at != 0u) ? "," : "");
        if (keyed && (markdown_cell || code_cell))
        {
            EMBED_CALL(branch_text_answer, WindowCallBranchCtx, .key = cell_key, .text = (source != NULL) ? source : "",
                       .size = length, .id = markdown_cell ? markdown : code, .out = out);
        }
        else
        {
            mmgr_line_put(out, "null");
        }
    }
    mmgr_line_ch(out, ']');
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return 1;
}

// a branch of no text under `key`, a cell the reader adds, colored in the language `id`: `{"key", "names", "colors"}`
static int branch_new(const CliServeAnswerCfg *args)
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
    EMBED_CALL(cli_document_colored.colors_to, CliDocumentColoredCfg, .colored = branch_colors_send, .threaded = 1);
    EMBED_CALL(branch_text_answer, WindowCallBranchCtx, .key = key, .text = "", .size = 0ull,
               .id = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                .key = "id", .otherwise = ""),
               .out = out);
    return 1;
}

// the colors of the lines from `first` to `last` of the branch under `key`, as a print of them asks, whatever lines
// the page mirrors, written as `[{"from", "runs"}]`
static int branch_runs(const CliServeAnswerCfg *args)
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
    if (!EMBED_CALL(cli_document_out_key_first.runs_write, CliDocumentOutKeyFirstCfg, .key = key, .first = first,
                    .last = last, .mirrored = 0, .out = out))
    {
        mmgr_line_put(err_msg, "no branch is stored under ");
        mmgr_line_put(err_msg, key);
        return 0;
    }
    return 1;
}

const CliServeCall g_cli_window_branch_calls[] = {
    {"branch_open", branch_open},
    {"branch_at", branch_at},
    {"branch_view", branch_view},
    {"branch_language", branch_language},
    {"branch_signature", branch_signature},
    {"branch_pair", branch_pair},
    {"branch_runs", branch_runs},
    {"branch_cells", branch_cells},
    {"branch_new", branch_new},
};
const unsigned int g_cli_window_branch_count = sizeof(g_cli_window_branch_calls) / sizeof(g_cli_window_branch_calls[0]);
