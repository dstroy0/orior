// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_git_acts.c: the calls that have git act on the tree: commits, pushes, pulls, rollbacks, branch and stash
// acts, cherry-picks, resolves and a repository made; and a file's text as the last commit stores it and at a commit.
// Where git answers with its own words, they are written as one JSON string
#include "../code/git.h"
#include "serve_internal.h"

// the bytes git's words and a file's text at a commit are written in
#define CLI_SERVE_WORDS_ROOM (1ull << 20u)
#define CLI_SERVE_TEXT_AT_ROOM (64ull << 20u)

typedef struct
{
    VerbaLinea *line;
    unsigned long long room;
    VerbaLinea *err_msg;
    const CliJson *json;
    unsigned int node;
    int done;
    VerbaLinea *words;
    VerbaLinea *out;
    const CliServeArgs *request;
    int push;
} ServeCallGitActsCtx;
// a line begun of `first` bytes for git's words or a file's text: 1, or 0 with why given where its cell is refused
EMBED_INLINE int git_line_begin(const ServeCallGitActsCtx *args)
{
    VerbaLinea *line = args->line;
    char *const cell = g_cli_cells->temporary_buf_alloc(args->room, &g_cli_serve_git_cells);
    *line = mmgr_line_over(cell, args->room);
    const int begun = cell != NULL;
    if (!begun)
    {
        mmgr_line_put(args->err_msg, "git's words could not be given a cell");
    }
    return begun;
}

// the items of the array `node`, 0 where it is not one
EMBED_INLINE unsigned int git_items(const ServeCallGitActsCtx *args)
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

// git's words written as a JSON string, where `done`, and their cell given back
EMBED_INLINE int git_words_free(const ServeCallGitActsCtx *args)
{
    VerbaLinea *out = args->out;
    if (args->done)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)args->words->out,
                   .length = mmgr_line_ok(args->words) ? args->words->at : 0ull);
    }
    g_cli_cells->persistent_buf_release(args->words->out, &g_cli_serve_git_cells);
    return args->done;
}

static int git_branch(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    const char *const act = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "act", .err_msg = err_msg);
    const char *const name = (act != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "name", .err_msg = err_msg)
                                           : NULL;
    VerbaLinea words;
    if ((name == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg) ||
        !EMBED_CALL(git_line_begin, ServeCallGitActsCtx, .line = &words, .room = CLI_SERVE_WORDS_ROOM,
                    .err_msg = err_msg))
    {
        return 0;
    }
    return EMBED_CALL(git_words_free, ServeCallGitActsCtx,
                      .done = EMBED_CALL(cli_git_root_out.branch_act, CliGitRootOutCfg, .root = folder, .act = act,
                                         .name_read = name,
                                         .to = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                                          .request = arguments, .key = "to", .otherwise = ""),
                                         .out = &words, .err_msg = err_msg),
                      .words = &words, .out = out);
}

static int git_commit_parts(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned int whole = 0u;
    const unsigned int parts =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "parts");
    const CliJson *const json = arguments->json;
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = parts, .kind = CFG_JSON_ARRAY)
                                   ? json->node[parts].first
                                   : CLI_JSON_NONE;
    const unsigned int whole_most = EMBED_CALL(
        git_items, ServeCallGitActsCtx, .json = json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "whole"));
    const unsigned int parts_most = EMBED_CALL(git_items, ServeCallGitActsCtx, .json = json, .node = parts);
    const char **const whole_of =
        g_cli_cells->temporary_buf_alloc((whole_most + 1ull) * sizeof(*whole_of), &g_cli_serve_git_cells);
    CliGitPart *const part_of =
        g_cli_cells->temporary_buf_alloc((parts_most + 1ull) * sizeof(*part_of), &g_cli_serve_git_cells);
    const char *const message = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                           .key = "message", .err_msg = err_msg);
    VerbaLinea words = {NULL, 0ull, 0ull};
    int done = (whole_of != NULL) && (part_of != NULL) && (message != NULL) &&
               EMBED_CALL(cli_serve_err_msg_internal.strings, CliServeErrMsgCtx, .request = arguments, .key = "whole",
                          .items = whole_of, .room = whole_most, .count = &whole, .err_msg = err_msg) &&
               EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                          .err_msg = err_msg);
    if ((whole_of == NULL) || (part_of == NULL))
    {
        mmgr_line_put(err_msg, "a commit's files could not be given cells");
    }
    unsigned int count = 0u;
    for (unsigned int one = done ? first : CLI_JSON_NONE; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        unsigned long long length = 0ull;
        const char *const path =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "path"),
                       .length_pointer = NULL);
        const char *const text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "text"),
                       .length_pointer = &length);
        if ((path == NULL) || (text == NULL))
        {
            mmgr_line_put(err_msg, "parts: missing field `");
            mmgr_line_put(err_msg, (path == NULL) ? "path" : "text");
            mmgr_line_ch(err_msg, '`');
            done = 0;
            break;
        }
        part_of[count] = (CliGitPart){path, text, length};
        count += 1u;
    }
    if (done && EMBED_CALL(git_line_begin, ServeCallGitActsCtx, .line = &words, .room = CLI_SERVE_WORDS_ROOM,
                           .err_msg = err_msg))
    {
        done = EMBED_CALL(git_words_free, ServeCallGitActsCtx,
                          .done = EMBED_CALL(cli_git_root.commit_parts, CliGitRootCfg, .root = root, .message = message,
                                             .whole = whole_of, .whole_count = whole, .parts = part_of,
                                             .part_count = count, .out = &words, .err_msg = err_msg),
                          .words = &words, .out = out);
    }
    else
    {
        done = 0;
    }
    g_cli_cells->persistent_buf_release(part_of, &g_cli_serve_git_cells);
    g_cli_cells->persistent_buf_release(whole_of, &g_cli_serve_git_cells);
    return done;
}

static int git_resolve(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_git_root_err_msg_path.resolve, CliGitRootErrMsgPathCfg, .root = root, .path = path,
                    .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

static int file_head(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    VerbaLinea text;
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(git_line_begin, ServeCallGitActsCtx, .line = &text, .room = CLI_SERVE_TEXT_AT_ROOM,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (EMBED_CALL(cli_git_root_file_stage.head_text, CliGitRootFileStageCfg, .root = root, .file = path,
                   .out = &text) &&
        mmgr_line_ok(&text))
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)text.out, .length = text.at);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    g_cli_cells->persistent_buf_release(text.out, &g_cli_serve_git_cells);
    return 1;
}

static int git_commit(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    const unsigned int most = EMBED_CALL(
        git_items, ServeCallGitActsCtx, .json = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "paths"));
    const char **const paths = g_cli_cells->temporary_buf_alloc((most + 1ull) * sizeof(*paths), &g_cli_serve_git_cells);
    const char *const message = (paths != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                             .request = arguments, .key = "message", .err_msg = err_msg)
                                                : NULL;
    VerbaLinea words;
    int done = 0;
    if (paths == NULL)
    {
        mmgr_line_put(err_msg, "a commit's files could not be given a cell");
    }
    else if ((message != NULL) &&
             EMBED_CALL(cli_serve_err_msg_internal.strings, CliServeErrMsgCtx, .request = arguments, .key = "paths",
                        .items = paths, .room = most, .count = &count, .err_msg = err_msg) &&
             EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                        .err_msg = err_msg) &&
             EMBED_CALL(git_line_begin, ServeCallGitActsCtx, .line = &words, .room = CLI_SERVE_WORDS_ROOM,
                        .err_msg = err_msg))
    {
        done = EMBED_CALL(git_words_free, ServeCallGitActsCtx,
                          .done = EMBED_CALL(cli_git_root_out_count.commit, CliGitRootOutCountCfg, .root = root,
                                             .message = message, .paths = paths, .count = count, .out = &words,
                                             .err_msg = err_msg),
                          .words = &words, .out = out);
    }
    g_cli_cells->persistent_buf_release(paths, &g_cli_serve_git_cells);
    return done;
}

// git pushed or pulled at the repository of the arguments: its words written
EMBED_INLINE int git_pushed_or_pull(const ServeCallGitActsCtx *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea words;
    if (!EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg) ||
        !EMBED_CALL(git_line_begin, ServeCallGitActsCtx, .line = &words, .room = CLI_SERVE_WORDS_ROOM,
                    .err_msg = err_msg))
    {
        return 0;
    }
    return EMBED_CALL(git_words_free, ServeCallGitActsCtx,
                      .done = args->push ? EMBED_CALL(cli_git_root_out_count_err_msg.push, CliGitRootOutCountErrMsgCfg,
                                                      .root = folder, .out = &words, .err_msg = err_msg)
                                         : EMBED_CALL(cli_git_root_out_count_err_msg.pull, CliGitRootOutCountErrMsgCfg,
                                                      .root = folder, .out = &words, .err_msg = err_msg),
                      .words = &words, .out = out);
}

static int git_push(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(git_pushed_or_pull, ServeCallGitActsCtx, .request = arguments, .push = 1, .out = out,
                      .err_msg = err_msg);
}

static int git_pull(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(git_pushed_or_pull, ServeCallGitActsCtx, .request = arguments, .push = 0, .out = out,
                      .err_msg = err_msg);
}

static int git_rollback(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    if ((path == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_git_root_err_msg_path.rollback, CliGitRootErrMsgPathCfg, .root = root, .path = path,
                    .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_put(args->out, "null");
    return 1;
}

static int git_stash(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    const char *const act = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "act", .err_msg = err_msg);
    VerbaLinea words;
    if ((act == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg) ||
        !EMBED_CALL(git_line_begin, ServeCallGitActsCtx, .line = &words, .room = CLI_SERVE_WORDS_ROOM,
                    .err_msg = err_msg))
    {
        return 0;
    }
    return EMBED_CALL(
        git_words_free, ServeCallGitActsCtx,
        .done = EMBED_CALL(cli_git_root_out_message.stash_act, CliGitRootOutMessageCfg, .root = folder, .act = act,
                           .name_read = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                                   .request = arguments, .key = "name", .otherwise = ""),
                           .message = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                                 .request = arguments, .key = "message", .otherwise = ""),
                           .out = &words, .err_msg = err_msg),
        .words = &words, .out = out);
}

static int git_cherry_pick(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    VerbaLinea words;
    if ((id == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg) ||
        !EMBED_CALL(git_line_begin, ServeCallGitActsCtx, .line = &words, .room = CLI_SERVE_WORDS_ROOM,
                    .err_msg = err_msg))
    {
        return 0;
    }
    return EMBED_CALL(git_words_free, ServeCallGitActsCtx,
                      .done = EMBED_CALL(cli_git_root_file.cherry_pick, CliGitRootFileCfg, .root = folder, .id = id,
                                         .out = &words, .err_msg = err_msg),
                      .words = &words, .out = out);
}

static int file_at(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    const char *const id = (path != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                       .request = arguments, .key = "id", .err_msg = err_msg)
                                          : NULL;
    VerbaLinea text;
    if ((id == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(git_line_begin, ServeCallGitActsCtx, .line = &text, .room = CLI_SERVE_TEXT_AT_ROOM,
                    .err_msg = err_msg))
    {
        return 0;
    }
    return EMBED_CALL(git_words_free, ServeCallGitActsCtx,
                      .done = EMBED_CALL(cli_git_root_file.text_at, CliGitRootFileCfg, .root = root, .file = path,
                                         .id = id, .out = &text, .err_msg = err_msg),
                      .words = &text, .out = out);
}

static int repo_init(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    const char *const named = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                         .key = "folder", .otherwise = NULL);
    if (named != NULL)
    {
        VerbaLinea line = {folder, sizeof(folder), 0ull};
        mmgr_line_put(&line, named);
        (void)mmgr_line_finish(&line);
    }
    else if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = folder,
                         .err_msg = err_msg))
    {
        return 0;
    }
    VerbaLinea words;
    if (!EMBED_CALL(git_line_begin, ServeCallGitActsCtx, .line = &words, .room = CLI_SERVE_WORDS_ROOM,
                    .err_msg = err_msg))
    {
        return 0;
    }
    const int done =
        EMBED_CALL(cli_git_out.init, CliGitOutCfg, .folder_read = folder, .out = &words, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(words.out, &g_cli_serve_git_cells);
    if (done)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)folder,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    }
    return done;
}

const CliServeCall g_cli_serve_git_acts_calls[] = {
    {"git_branch", git_branch},   {"git_commit_parts", git_commit_parts},
    {"git_resolve", git_resolve}, {"file_head", file_head},
    {"git_commit", git_commit},   {"git_push", git_push},
    {"git_pull", git_pull},       {"git_rollback", git_rollback},
    {"git_stash", git_stash},     {"git_cherry_pick", git_cherry_pick},
    {"file_at", file_at},         {"repo_init", repo_init},
};
const unsigned int g_cli_serve_git_acts_count =
    sizeof(g_cli_serve_git_acts_calls) / sizeof(g_cli_serve_git_acts_calls[0]);
