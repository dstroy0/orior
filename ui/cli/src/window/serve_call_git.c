// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_git.c: the calls that query git about the tree: the files that differ, the repositories, the branches, the
// stashes, the commits elsewhere and a file's, the graph, what a commit touched and a line's history; git's acts are
// serve_call_git_acts.c's. A repository of the tree is named by its path in the tree, the tree's own where none is
// named
#include "../code/git.h"
#include "serve_internal.h"

int cli_serve_git_repo_get(const CliServeErrMsgRequestFolderCtx *args)
{
    char root[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    const char *const repo = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx,
                                        .request = args->request, .key = "repo", .otherwise = "");
    if (repo[0] == '\0')
    {
        memor.cpy(&(MemoriaCfg){.dst = args->folder, .src = root, .bytes = sizeof(root)});
        return 1;
    }
    return EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = repo, .joined = args->folder,
                      .err_msg = args->err_msg);
}

static int git_branches(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    CliBranches *const found = EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                                          .size = sizeof(*found), .what = "what git finds", .err_msg = err_msg);
    if ((found == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg))
    {
        g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
        return 0;
    }
    EMBED_CALL(cli_git_root_found_branches.branches, CliGitRootFoundBranchesCfg, .root = folder,
               .found_branches_pointer = found);
    EMBED_CALL(cli_git_out_found_branches.branches_write, CliGitOutFoundBranchesCfg,
               .found_branches_pointer_other = found, .out = out);
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
    return 1;
}

static int tree_change(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static const char *s_repos[CLI_GIT_REPOSITORIES];
    static CliLock s_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    const unsigned int repos =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "repos");
    const int named =
        (repos != CLI_JSON_NONE) && !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                .json_read = arguments->json, .node = repos, .kind = CFG_JSON_NULL);
    CliChangedList *const found = EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                                             .size = sizeof(*found), .what = "what git finds", .err_msg = err_msg);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if ((found == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.strings_or, CliServeErrMsgCtx, .request = arguments, .key = "repos",
                    .items = s_repos, .room = CLI_GIT_REPOSITORIES, .count = &count, .err_msg = err_msg))
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
        return 0;
    }
    EMBED_CALL(cli_git_root_repos.change, CliGitRootReposCfg, .root = root, .repos = s_repos, .count = count,
               .named = named, .found_changed_list_pointer = found);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    EMBED_CALL(cli_git_out_found_changed_list.changed_write, CliGitOutFoundChangedListCfg,
               .found_changed_list_pointer_other = found, .out = out);
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
    return 1;
}

static int git_repositories(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    CliRepositories *const found = EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                                              .size = sizeof(*found), .what = "what git finds", .err_msg = err_msg);
    if ((found == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                       .root = root, .err_msg = err_msg))
    {
        g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
        return 0;
    }
    EMBED_CALL(cli_git_root_found_repositories.repositories, CliGitRootFoundRepositoriesCfg, .root = root,
               .found_repositories_pointer = found);
    EMBED_CALL(cli_git_out_found_repositories.repositories_write, CliGitOutFoundRepositoriesCfg,
               .found_repositories_pointer_other = found, .out = out);
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
    return 1;
}

static int tree_branch(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg))
    {
        return 0;
    }
    char name[CLI_NAME_MAX_LEN];
    if (EMBED_CALL(cli_git_table.branch, CliGitCfg, .root = folder, .name = name))
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    return 1;
}

static int file_commits(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    CliCommits *const found = (path != NULL)
                                  ? EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                                               .size = sizeof(*found), .what = "what git finds", .err_msg = err_msg)
                                  : NULL;
    if ((found == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                       .root = root, .err_msg = err_msg))
    {
        g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
        return 0;
    }
    const int done = EMBED_CALL(cli_git_root_file_err_msg.commits, CliGitRootFileErrMsgCfg, .root = root, .file = path,
                                .found = found, .err_msg = err_msg);
    if (done)
    {
        EMBED_CALL(cli_git_out_found.commits_write, CliGitOutFoundCfg, .found_read = found, .out = out);
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
    return done;
}

static int git_ahead_behind(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg))
    {
        return 0;
    }
    unsigned int ahead = 0u;
    unsigned int behind = 0u;
    if (!EMBED_CALL(cli_git_root_ahead.ahead_behind, CliGitRootAheadCfg, .root = folder, .ahead = &ahead,
                    .behind = &behind))
    {
        mmgr_line_put(args->out, "null");
        return 1;
    }
    mmgr_line_ch(args->out, '[');
    mmgr_line_decimal(args->out, ahead, 1u);
    mmgr_line_ch(args->out, ',');
    mmgr_line_decimal(args->out, behind, 1u);
    mmgr_line_ch(args->out, ']');
    return 1;
}

static int git_graph(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    CliGraph *const found = EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                                       .size = sizeof(*found), .what = "what git finds", .err_msg = err_msg);
    if ((found == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg))
    {
        g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
        return 0;
    }
    EMBED_CALL(cli_git_root_query.graph, CliGitRootQueryCfg, .root = folder,
               .query = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                   .key = "query", .otherwise = ""),
               .authors = EMBED_CALL(cli_serve_err_msg_internal.bool_or, CliServeErrMsgCtx, .request = arguments,
                                     .key = "authors", .otherwise_i32 = 0),
               .found_graph_pointer = found);
    EMBED_CALL(cli_git_out_found_graph.graph_write, CliGitOutFoundGraphCfg, .found_graph_pointer_other = found,
               .out = out);
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
    return 1;
}

static int git_touch(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    CliTouchedList *const found = (id != NULL)
                                      ? EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                                                   .size = sizeof(*found), .what = "what git finds", .err_msg = err_msg)
                                      : NULL;
    if ((found == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg))
    {
        g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
        return 0;
    }
    const int done = EMBED_CALL(cli_git_root_err_msg.touch, CliGitRootErrMsgCfg, .root = folder, .id = id,
                                .found_touched_list_pointer = found, .err_msg = err_msg);
    if (done)
    {
        EMBED_CALL(cli_git_out_found_touched_list.touched_write, CliGitOutFoundTouchedListCfg,
                   .found_touched_list_pointer_other = found,
                   .repo = EMBED_CALL(cli_serve_err_msg_internal.string_or, CliServeErrMsgCtx, .request = arguments,
                                      .key = "repo", .otherwise = ""),
                   .out = out);
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
    return done;
}

static int git_stashes(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    CliStashes *const found = EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                                         .size = sizeof(*found), .what = "what git finds", .err_msg = err_msg);
    if ((found == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg))
    {
        g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
        return 0;
    }
    EMBED_CALL(cli_git_root_found_stashes.stashes, CliGitRootFoundStashesCfg, .root = folder,
               .found_stashes_pointer = found);
    EMBED_CALL(cli_git_out_found_stashes.stashes_write, CliGitOutFoundStashesCfg, .found_stashes_pointer_other = found,
               .out = out);
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
    return 1;
}

static int git_elsewhere(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    CliCommits *const found = EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                                         .size = sizeof(*found), .what = "what git finds", .err_msg = err_msg);
    if ((found == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_folder_internal.git_repo_get, CliServeErrMsgRequestFolderCtx,
                    .request = arguments, .folder = folder, .err_msg = err_msg))
    {
        g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
        return 0;
    }
    EMBED_CALL(cli_git_root_file_err_msg.elsewhere, CliGitRootFileErrMsgCfg, .root = folder, .found = found);
    EMBED_CALL(cli_git_out_found.commits_write, CliGitOutFoundCfg, .found_read = found, .out = out);
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
    return 1;
}

static int git_line_history(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const path = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "path", .err_msg = err_msg);
    const char *const text = (path != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "text", .err_msg = err_msg)
                                            : NULL;
    CliLineHistory *const found = (text != NULL)
                                      ? EMBED_CALL(cli_serve_size_err_msg_internal.cell, CliServeSizeErrMsgCtx,
                                                   .size = sizeof(*found), .what = "what git finds", .err_msg = err_msg)
                                      : NULL;
    if ((found == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                       .root = root, .err_msg = err_msg))
    {
        g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
        return 0;
    }
    (void)EMBED_CALL(
        cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = arguments->json,
        .node = EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "text"),
        .length_pointer = &length);
    const int done =
        EMBED_CALL(cli_git_length.line_history, CliGitLengthCfg, .root = root, .file = path, .text_text = text,
                   .length = length, .found_line_history_pointer = found, .err_msg = err_msg);
    if (done)
    {
        EMBED_CALL(cli_git_out_found_line_history.line_history_write, CliGitOutFoundLineHistoryCfg,
                   .found_line_history_pointer_other = found, .out = out);
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_window_cells);
    return done;
}

const CliServeCall g_cli_serve_git_calls[] = {
    {"git_branches", git_branches},
    {"tree_changed", tree_change},
    {"git_repositories", git_repositories},
    {"tree_branch", tree_branch},
    {"file_commits", file_commits},
    {"git_ahead_behind", git_ahead_behind},
    {"git_graph", git_graph},
    {"git_touched", git_touch},
    {"git_stashes", git_stashes},
    {"git_elsewhere", git_elsewhere},
    {"git_line_history", git_line_history},
};
const unsigned int g_cli_serve_git_count = sizeof(g_cli_serve_git_calls) / sizeof(g_cli_serve_git_calls[0]);
