// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_git.c: the calls that ask git of the tree: the files that differ, the repositories, the branches, the
// stashes, the commits elsewhere and a file's, the graph, what a commit touched and a line's history; git's acts are
// serve_call_git_acts.c's. A repository of the tree is named by its path in the tree, the tree's own where none is
// named
#include "../code/git.h"
#include "serve_internal.h"

int cli_serve_git_repo_of(const CliServeArgs *args, char *folder, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    const char *const repo = cli_serve_string_or(args, "repo", "");
    if (repo[0] == '\0')
    {
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = root, .bytes = sizeof(root)});
        return 1;
    }
    return cli_inside(root, repo, folder, said);
}

static int git_branches(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    CliBranches *const found = cli_serve_cell(sizeof(*found), "what git finds", said);
    if ((found == NULL) || !cli_serve_git_repo_of(args, folder, said))
    {
        cli_cell_given(found);
        return 0;
    }
    cli_git_branches(folder, found);
    cli_git_branches_written(found, out);
    cli_git_text_given(&found->text);
    cli_cell_given(found);
    return 1;
}

static int tree_changed(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static const char *s_repos[CLI_GIT_REPOSITORIES];
    static CliLock s_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_LONGEST];
    unsigned int count = 0u;
    const unsigned int repos = cli_serve_arg(args, "repos");
    const int named = (repos != CLI_JSON_NONE) && !cli_json_is(args->json, repos, CFG_JSON_NULL);
    CliChangedList *const found = cli_serve_cell(sizeof(*found), "what git finds", said);
    cli_lock_take(&s_lock);
    if ((found == NULL) || !cli_serve_tree(root, said) ||
        !cli_serve_strings_or(args, "repos", s_repos, CLI_GIT_REPOSITORIES, &count, said))
    {
        cli_lock_give(&s_lock);
        cli_cell_given(found);
        return 0;
    }
    cli_git_changed(root, s_repos, count, named, found);
    cli_lock_give(&s_lock);
    cli_git_changed_written(found, out);
    cli_git_text_given(&found->text);
    cli_cell_given(found);
    return 1;
}

static int git_repositories(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    CliRepositories *const found = cli_serve_cell(sizeof(*found), "what git finds", said);
    if ((found == NULL) || !cli_serve_tree(root, said))
    {
        cli_cell_given(found);
        return 0;
    }
    cli_git_repositories(root, found);
    cli_git_repositories_written(found, out);
    cli_git_text_given(&found->text);
    cli_cell_given(found);
    return 1;
}

static int tree_branch(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_serve_git_repo_of(args, folder, said))
    {
        return 0;
    }
    char name[CLI_NAME_LONGEST];
    if (cli_git_branch(folder, name))
    {
        cli_json_quoted_lossy(out, (const unsigned char *)name,
                              cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        cli_line_text(out, "null");
    }
    return 1;
}

static int file_commits(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    CliCommits *const found = (path != NULL) ? cli_serve_cell(sizeof(*found), "what git finds", said) : NULL;
    if ((found == NULL) || !cli_serve_tree(root, said))
    {
        cli_cell_given(found);
        return 0;
    }
    const int done = cli_git_commits(root, path, found, said);
    if (done)
    {
        cli_git_commits_written(found, out);
    }
    cli_git_text_given(&found->text);
    cli_cell_given(found);
    return done;
}

static int git_ahead_behind(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_serve_git_repo_of(args, folder, said))
    {
        return 0;
    }
    unsigned int ahead = 0u;
    unsigned int behind = 0u;
    if (!cli_git_ahead_behind(folder, &ahead, &behind))
    {
        cli_line_text(out, "null");
        return 1;
    }
    cli_line_character(out, '[');
    cli_line_decimal(out, ahead, 1u);
    cli_line_character(out, ',');
    cli_line_decimal(out, behind, 1u);
    cli_line_character(out, ']');
    return 1;
}

static int git_graph(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    CliGraph *const found = cli_serve_cell(sizeof(*found), "what git finds", said);
    if ((found == NULL) || !cli_serve_git_repo_of(args, folder, said))
    {
        cli_cell_given(found);
        return 0;
    }
    cli_git_graph(folder, cli_serve_string_or(args, "query", ""), cli_serve_bool_or(args, "authors", 0), found);
    cli_git_graph_written(found, out);
    cli_git_text_given(&found->text);
    cli_cell_given(found);
    return 1;
}

static int git_touched(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    const char *const id = cli_serve_string(args, "id", said);
    CliTouchedList *const found = (id != NULL) ? cli_serve_cell(sizeof(*found), "what git finds", said) : NULL;
    if ((found == NULL) || !cli_serve_git_repo_of(args, folder, said))
    {
        cli_cell_given(found);
        return 0;
    }
    const int done = cli_git_touched(folder, id, found, said);
    if (done)
    {
        cli_git_touched_written(found, cli_serve_string_or(args, "repo", ""), out);
    }
    cli_git_text_given(&found->text);
    cli_cell_given(found);
    return done;
}

static int git_stashes(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    CliStashes *const found = cli_serve_cell(sizeof(*found), "what git finds", said);
    if ((found == NULL) || !cli_serve_git_repo_of(args, folder, said))
    {
        cli_cell_given(found);
        return 0;
    }
    cli_git_stashes(folder, found);
    cli_git_stashes_written(found, out);
    cli_git_text_given(&found->text);
    cli_cell_given(found);
    return 1;
}

static int git_elsewhere(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    CliCommits *const found = cli_serve_cell(sizeof(*found), "what git finds", said);
    if ((found == NULL) || !cli_serve_git_repo_of(args, folder, said))
    {
        cli_cell_given(found);
        return 0;
    }
    cli_git_elsewhere(folder, found);
    cli_git_commits_written(found, out);
    cli_git_text_given(&found->text);
    cli_cell_given(found);
    return 1;
}

static int git_line_history(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned long long length = 0ull;
    const char *const path = cli_serve_string(args, "path", said);
    const char *const text = (path != NULL) ? cli_serve_string(args, "text", said) : NULL;
    CliLineHistory *const found = (text != NULL) ? cli_serve_cell(sizeof(*found), "what git finds", said) : NULL;
    if ((found == NULL) || !cli_serve_tree(root, said))
    {
        cli_cell_given(found);
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "text"), &length);
    const int done = cli_git_line_history(root, path, text, length, found, said);
    if (done)
    {
        cli_git_line_history_written(found, out);
    }
    cli_git_text_given(&found->text);
    cli_cell_given(found);
    return done;
}

const CliServeCall g_cli_serve_git_calls[] = {
    {"git_branches", git_branches},   {"tree_changed", tree_changed},         {"git_repositories", git_repositories},
    {"tree_branch", tree_branch},     {"file_commits", file_commits},         {"git_ahead_behind", git_ahead_behind},
    {"git_graph", git_graph},         {"git_touched", git_touched},           {"git_stashes", git_stashes},
    {"git_elsewhere", git_elsewhere}, {"git_line_history", git_line_history},
};
const unsigned int g_cli_serve_git_count = sizeof(g_cli_serve_git_calls) / sizeof(g_cli_serve_git_calls[0]);
