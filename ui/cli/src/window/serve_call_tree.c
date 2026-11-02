// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_tree.c: the calls of the tree as a whole: the tree open, set and reported; the page's answer to a command
// line; the definitions; a folder's entries, the files found by name and every file; a file's indentation; the patterns
// of a part of the window; the tree's files and what they declare listed and walked behind the window, one time from
// the tree's opening, and whether each is done yet; a quick open's list stored on this side, matched as each letter is
// typed, and let go; the reading of the tree's symbols ended where it stands, Go to Symbol answering from what it read;
// the signature of the call the cursor stands in, from the tree; a search of every line; and the folders mounted beside
// the tree
#include "../code/symbols.h"
#include "../editor/defs.h"
#include "../editor/editorconfig.h"
#include "../editor/files.h"
#include "../editor/picks.h"
#include "../text/patterns.h"
#include "serve_internal.h"

// the most declarations one search of the tree's symbols gives, and the most lines of patterns and recent files a call
// gives
#define CLI_SERVE_SYMBOLS 200u
#define CLI_SERVE_LINES 4096u

static int root_get(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char root[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_serve_err_msg_root.root_now, CliServeErrMsgRootCfg, .root_out = root))
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)root,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    return 1;
}

static int root_set(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    return (path != NULL) && EMBED_CALL(cli_serve_err_msg_path_out_internal.root_set, CliServeErrMsgPathOutCtx,
                                        .path = path, .out = out, .err_msg = err_msg);
}

static int command_answer(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    int done = 0;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    if ((id == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = arguments, .key = "done",
                    .value_i32s = &done, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_id_internal.command_answer, CliServeIdCtx, .id = id, .done = done,
                    .answer_text = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                              .request = arguments, .key = "message", .otherwise = ""),
                    .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

static int definitions_read(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_defs_table.read, CliDefsCfg, .root = root, .out = out);
    return 1;
}

static int tree_list(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const dir = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "dir", .err_msg = err_msg);
    return (dir != NULL) &&
           EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_files_root_out_dir.list, CliFilesRootOutDirCfg, .root = root, .dir = dir, .out = out,
                      .err_msg = err_msg);
}

static int tree_find(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const query = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "query", .err_msg = err_msg);
    if ((query == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                       .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_files_root_out_err_msg.find, CliFilesRootOutErrMsgCfg, .root = root, .query = query, .out = out);
    return 1;
}

static int tree_files(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
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
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < listed->count; at += 1u)
    {
        const char *const path = listed->paths[at];
        mmgr_line_put(out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)path,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_ch(out, ']');
    EMBED_CALL(cli_serve_listed_internal.files_free, CliServeListedCtx, .listed = listed);
    return 1;
}

static int files_find(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static const char *s_recent[CLI_SERVE_LINES];
    char root[CLI_PATH_MAX_LEN];
    unsigned int recents = 0u;
    unsigned long long most = 0ull;
    const char *const query = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "query", .err_msg = err_msg);
    if ((query == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.strings, CliServeErrMsgCtx, .request = arguments, .key = "recent",
                    .items = s_recent, .room = CLI_SERVE_LINES, .count = &recents, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "most",
                    .value = &most, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    // the most is cut to its low word, as no list has more
    EMBED_CALL(cli_files_root_out.rank, CliFilesRootOutCfg, .root = root, .query = query, .recent = s_recent,
               .recents = recents, .most = (unsigned int)most,
               .fuzzy = EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = arguments,
                                   .key = "fuzzy", .otherwise_i32 = 1),
               .out = out);
    return 1;
}

static int indent_for(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
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
    CliIndent indent = {-1, 0, 0u};
    EMBED_CALL(cli_editorconfig.indent, CliEditorconfigCfg, .path = full, .indent = &indent);
    mmgr_line_put(args->out,
                  (indent.tabs < 0) ? "{\"tabs\":null" : (indent.tabs ? "{\"tabs\":true" : "{\"tabs\":false"));
    mmgr_line_put(args->out, ",\"size\":");
    if (indent.sized)
    {
        mmgr_line_decimal(args->out, indent.size, 1u);
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_ch(args->out, '}');
    return 1;
}

static int patterns_set(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    static const char *const s_parts[CLI_PARTS] = {"explorer", "search", "watching"};
    static const char *s_lines[CLI_SERVE_LINES];
    unsigned int count = 0u;
    const char *const part = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "part", .err_msg = err_msg);
    if ((part == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.strings, CliServeErrMsgCtx, .request = arguments, .key = "lines",
                    .items = s_lines, .room = CLI_SERVE_LINES, .count = &count, .err_msg = err_msg))
    {
        return 0;
    }
    unsigned int at = 0u;
    while ((at < CLI_PARTS) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = s_parts[at], .other = part, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    if (at == CLI_PARTS)
    {
        mmgr_line_put(err_msg, "part: unknown variant `");
        mmgr_line_put(err_msg, part);
        mmgr_line_put(err_msg, "`, expected one of `explorer`, `search`, `watching`");
        return 0;
    }
    const int changed = EMBED_CALL(cli_patterns_lines_count.set, CliPatternsLinesCountCfg, .part = at, .lines = s_lines,
                                   .count = count);
    mmgr_line_put(args->out, changed ? "true" : "false");
    return 1;
}

static int tree_read(const CliServeAnswerCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_files_root_paths.read_behind, CliFilesRootPathsCfg, .root = root);
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = root);
    mmgr_line_put(args->out, "null");
    return 1;
}

static int tree_unread(const CliServeAnswerCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    unsigned long long done = 0ull;
    unsigned long long of = 0ull;
    if (!EMBED_CALL(cli_symbols_done.tree_walk_progress, CliSymbolsDoneCfg, .done = &done, .total = &of))
    {
        done = 0ull;
        of = 0ull;
    }
    mmgr_line_put(args->out, EMBED_CALL(cli_files_root_paths.unread, CliFilesRootPathsCfg, .root = root)
                                 ? "{\"files\":true"
                                 : "{\"files\":false");
    mmgr_line_put(args->out, cli_tree_unwalked() ? ",\"symbols\":true,\"done\":" : ",\"symbols\":false,\"done\":");
    mmgr_line_decimal(args->out, done, 1u);
    mmgr_line_put(args->out, ",\"of\":");
    mmgr_line_decimal(args->out, of, 1u);
    mmgr_line_ch(args->out, '}');
    return 1;
}

static int pick_store(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long length = 0ull;
    const char *const texts = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "texts", .err_msg = err_msg);
    if (texts == NULL)
    {
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "texts"),
        .length_pointer = &length);
    mmgr_line_decimal(args->out, EMBED_CALL(cli_picks.store, CliPicksCfg, .texts = texts, .length = length), 1u);
    return 1;
}

static int pick_find(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long id = 0ull;
    unsigned long long most = 0ull;
    const char *const query = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "query", .err_msg = err_msg);
    return (query != NULL) &&
           EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "id",
                      .value = &id, .err_msg = err_msg) &&
           EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "most",
                      .value = &most, .err_msg = err_msg) &&
           EMBED_CALL(cli_picks_id.find, CliPicksIdCfg, .id = id, .query = query, .most = most, .out = out,
                      .err_msg = err_msg);
}

static int pick_forget(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long id = 0ull;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "id",
                    .value = &id, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_picks_id.forget, CliPicksIdCfg, .id = id);
    mmgr_line_put(args->out, "null");
    return 1;
}

static int tree_read_stop(const CliServeAnswerCfg *args)
{
    cli_tree_walk_stop();
    mmgr_line_put(args->out, "null");
    return 1;
}

static int symbols_find(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const query = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "query", .err_msg = err_msg);
    if ((query == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                       .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = root);
    EMBED_CALL(cli_symbols_query_most.tree_declarations_find, CliSymbolsQueryMostCfg, .query = query,
               .most = CLI_SERVE_SYMBOLS, .out = out);
    return 1;
}

// the signature of the call the cursor stands in, read off the lines the page hands from the file at `path`
static int symbols_signature(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long line = 0ull;
    unsigned long long col = 0ull;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    const char *const text = (path != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "text", .err_msg = err_msg)
                                            : NULL;
    if ((text == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "line",
                    .value = &line, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "col",
                    .value = &col, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_symbols_root.tree_walk, CliSymbolsRootCfg, .root = root);
    // a line and a column of a text the page holds are below 2^32
    return EMBED_CALL(cli_symbols_path_text_size_out.signature, CliSymbolsPathTextSizeOutCfg, .path = path,
                      .state = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                          .key = "state", .otherwise = "root"),
                      .text = text, .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}),
                      .line = (unsigned int)line, .col = (unsigned int)col, .out = out, .err_msg = err_msg);
}

static int tree_search(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static const char *s_set[CLI_SERVE_LINES];
    static CliPatterns s_patterns;
    static CliLock s_patterns_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    const char *const query = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                         .key = "query", .err_msg = err_msg);
    const unsigned int how =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "how");
    if ((query == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_internal.strings_or, CliServeErrMsgCtx, .request = arguments, .key = "set",
                    .items = s_set, .room = CLI_SERVE_LINES, .count = &count, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = arguments->json, .node = how,
                    .kind = CFG_JSON_OBJECT))
    {
        mmgr_line_put(err_msg, "how: invalid type: null, expected struct Searching");
        return 0;
    }
    const CliServeArgs read = {arguments->json, how};
    const CliSearching searching = {EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = &read,
                                               .key = "case", .otherwise_i32 = 0),
                                    EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = &read,
                                               .key = "word", .otherwise_i32 = 0),
                                    EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = &read,
                                               .key = "regex", .otherwise_i32 = 0)};
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_patterns_lock);
    EMBED_CALL(cli_patterns_lines.read_set, CliPatternsLinesCfg, .lines = s_set, .count = count,
               .patterns_out = &s_patterns);
    const int done = EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = root,
                                .query = query, .how = searching, .set = &s_patterns, .out = out, .err_msg = err_msg);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_patterns_lock);
    return done;
}

static int tree_mount(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char name[CLI_NAME_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) || !EMBED_CALL(cli_path_err_msg_name_internal.mount, CliPathErrMsgNameCtx, .path = path,
                                      .name_out = name, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    return 1;
}

static int tree_unmount(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const name = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "name", .err_msg = err_msg);
    if (name == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_err_msg_name_internal.unmount, CliErrMsgNameCtx, .name = name);
    mmgr_line_put(args->out, "null");
    return 1;
}

static int tree_mounts(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    const char *names[CLI_MOUNTS_MOST];
    const char *paths[CLI_MOUNTS_MOST];
    const unsigned int count = EMBED_CALL(cli_room_names_internal.mounts, CliRoomNamesCtx, .names = names,
                                          .paths_read = paths, .room_u32 = CLI_MOUNTS_MOST);
    mmgr_line_ch(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)names[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = names[at], .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"path\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)paths[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = paths[at])
                               ? ",\"dir\":true}"
                               : ",\"dir\":false}");
    }
    mmgr_line_ch(out, ']');
    return 1;
}

const CliServeCall g_cli_serve_tree_calls[] = {
    {"root_get", root_get},
    {"root_set", root_set},
    {"command_answer", command_answer},
    {"definitions_read", definitions_read},
    {"tree_list", tree_list},
    {"tree_find", tree_find},
    {"tree_files", tree_files},
    {"files_find", files_find},
    {"indent_for", indent_for},
    {"patterns_set", patterns_set},
    {"tree_read", tree_read},
    {"tree_unread", tree_unread},
    {"pick_store", pick_store},
    {"pick_find", pick_find},
    {"pick_forget", pick_forget},
    {"tree_read_stop", tree_read_stop},
    {"symbols_find", symbols_find},
    {"symbols_signature", symbols_signature},
    {"tree_search", tree_search},
    {"tree_mount", tree_mount},
    {"tree_unmount", tree_unmount},
    {"tree_mounts", tree_mounts},
};
const unsigned int g_cli_serve_tree_count = sizeof(g_cli_serve_tree_calls) / sizeof(g_cli_serve_tree_calls[0]);
