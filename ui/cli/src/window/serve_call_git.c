// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_git.c: the calls that ask git of the tree or have it act: the files that differ, the repositories, the
// branches, the stashes, the commits elsewhere and a file's, the graph, what a commit touched, a line's history, a
// file's text at a commit, and commits, pushes, pulls, rollbacks, branch and stash acts, cherry-picks and resolves.
// A repository of the tree is named by its path in the tree, the tree's own where none is named. Where git answers
// with its own words, they are written as one JSON string
#include "../code/git.h"
#include "serve_internal.h"

// the most paths and parts one commit takes, and the most bytes of a file's text at a commit
#define CLI_SERVE_COMMITTED 4096u
#define CLI_SERVE_TEXT_AT (16u << 20u)

// git's words, each call's, written before they go as a string
static char s_words[1u << 20u];
static CliLock s_words_lock = CLI_LOCK_INIT;

// the folder of the repository the argument `repo` names, the tree's own where it names none, into `folder`, which
// holds CLI_PATH_LONGEST: 1, or 0 with why said
static int git_repo_of(const CliServeArgs *args, char *folder, CliLine *said)
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

// git's words written as a JSON string, where `done`
static int git_words_given(int done, const CliLine *words, CliLine *out)
{
    if (done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)words->out, cli_line_fits(words) ? words->at : 0ull);
    }
    return done;
}

static int git_branches(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    CliBranches *const found = cli_serve_cell(sizeof(*found), "what git finds", said);
    if ((found == NULL) || !git_repo_of(args, folder, said))
    {
        cli_cell_given(found);
        return 0;
    }
    cli_git_branches(folder, found);
    cli_git_branches_written(found, out);
    cli_cell_given(found);
    return 1;
}

static int git_branch(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    const char *const act = cli_serve_string(args, "act", said);
    const char *const name = (act != NULL) ? cli_serve_string(args, "name", said) : NULL;
    if ((name == NULL) || !git_repo_of(args, folder, said))
    {
        return 0;
    }
    cli_lock_take(&s_words_lock);
    CliLine words = {s_words, sizeof(s_words), 0ull};
    const int done = git_words_given(
        cli_git_branch_act(folder, act, name, cli_serve_string_or(args, "to", ""), &words, said), &words, out);
    cli_lock_give(&s_words_lock);
    return done;
}

static int git_commit_parts(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static const char *s_whole[CLI_SERVE_COMMITTED];
    static CliGitPart s_part[CLI_SERVE_COMMITTED];
    char root[CLI_PATH_LONGEST];
    unsigned int whole = 0u;
    const char *const message = cli_serve_string(args, "message", said);
    if ((message == NULL) || !cli_serve_strings(args, "whole", s_whole, CLI_SERVE_COMMITTED, &whole, said) ||
        !cli_serve_tree(root, said))
    {
        return 0;
    }
    const unsigned int parts = cli_serve_arg(args, "parts");
    const CliJson *const json = args->json;
    unsigned int count = 0u;
    for (unsigned int one = cli_json_is(json, parts, CFG_JSON_ARRAY) ? json->node[parts].first : CLI_JSON_NONE;
         (one != CLI_JSON_NONE) && (count < CLI_SERVE_COMMITTED); one = json->node[one].next)
    {
        unsigned long long length = 0ull;
        const char *const path = cli_json_string(json, cli_json_member(json, one, "path"), NULL);
        const char *const text = cli_json_string(json, cli_json_member(json, one, "text"), &length);
        if ((path == NULL) || (text == NULL))
        {
            cli_line_text(said, "parts: missing field `");
            cli_line_text(said, (path == NULL) ? "path" : "text");
            cli_line_character(said, '`');
            return 0;
        }
        s_part[count] = (CliGitPart){path, text, length};
        count += 1u;
    }
    cli_lock_take(&s_words_lock);
    CliLine words = {s_words, sizeof(s_words), 0ull};
    const int done =
        git_words_given(cli_git_commit_parts(root, message, s_whole, whole, s_part, count, &words, said), &words, out);
    cli_lock_give(&s_words_lock);
    return done;
}

static int git_resolve(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_tree(root, said) || !cli_git_resolve(root, path, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
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
    cli_cell_given(found);
    return 1;
}

static int tree_branch(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (!git_repo_of(args, folder, said))
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

static int file_head(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    char *const head = (path != NULL) ? cli_serve_cell(CLI_SERVE_TEXT_AT, "a file's text at a commit", said) : NULL;
    if ((head == NULL) || !cli_serve_tree(root, said))
    {
        cli_cell_given(head);
        return 0;
    }
    CliLine text = {head, CLI_SERVE_TEXT_AT, 0ull};
    if (cli_git_head_text(root, path, &text) && cli_line_fits(&text))
    {
        cli_json_quoted_lossy(out, (const unsigned char *)head, text.at);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_cell_given(head);
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
    cli_cell_given(found);
    return done;
}

static int git_commit(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static const char *s_paths[CLI_SERVE_COMMITTED];
    char root[CLI_PATH_LONGEST];
    unsigned int count = 0u;
    const char *const message = cli_serve_string(args, "message", said);
    if ((message == NULL) || !cli_serve_strings(args, "paths", s_paths, CLI_SERVE_COMMITTED, &count, said) ||
        !cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_lock_take(&s_words_lock);
    CliLine words = {s_words, sizeof(s_words), 0ull};
    const int done = git_words_given(cli_git_commit(root, message, s_paths, count, &words, said), &words, out);
    cli_lock_give(&s_words_lock);
    return done;
}

// git pushed or pulled at the repository of the arguments: its words written
static int git_pushed_or_pulled(const CliServeArgs *args, int push, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (!git_repo_of(args, folder, said))
    {
        return 0;
    }
    cli_lock_take(&s_words_lock);
    CliLine words = {s_words, sizeof(s_words), 0ull};
    const int done =
        git_words_given(push ? cli_git_push(folder, &words, said) : cli_git_pull(folder, &words, said), &words, out);
    cli_lock_give(&s_words_lock);
    return done;
}

static int git_push(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    return git_pushed_or_pulled(args, 1, out, said);
}

static int git_pull(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    return git_pushed_or_pulled(args, 0, out, said);
}

static int git_rollback(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_tree(root, said) || !cli_git_rollback(root, path, said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

static int git_ahead_behind(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    if (!git_repo_of(args, folder, said))
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
    if ((found == NULL) || !git_repo_of(args, folder, said))
    {
        cli_cell_given(found);
        return 0;
    }
    cli_git_graph(folder, cli_serve_string_or(args, "query", ""), cli_serve_bool_or(args, "authors", 0), found);
    cli_git_graph_written(found, out);
    cli_cell_given(found);
    return 1;
}

static int git_touched(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    const char *const id = cli_serve_string(args, "id", said);
    CliTouchedList *const found = (id != NULL) ? cli_serve_cell(sizeof(*found), "what git finds", said) : NULL;
    if ((found == NULL) || !git_repo_of(args, folder, said))
    {
        cli_cell_given(found);
        return 0;
    }
    const int done = cli_git_touched(folder, id, found, said);
    if (done)
    {
        cli_git_touched_written(found, cli_serve_string_or(args, "repo", ""), out);
    }
    cli_cell_given(found);
    return done;
}

static int git_stashes(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    CliStashes *const found = cli_serve_cell(sizeof(*found), "what git finds", said);
    if ((found == NULL) || !git_repo_of(args, folder, said))
    {
        cli_cell_given(found);
        return 0;
    }
    cli_git_stashes(folder, found);
    cli_git_stashes_written(found, out);
    cli_cell_given(found);
    return 1;
}

static int git_stash(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    const char *const act = cli_serve_string(args, "act", said);
    if ((act == NULL) || !git_repo_of(args, folder, said))
    {
        return 0;
    }
    cli_lock_take(&s_words_lock);
    CliLine words = {s_words, sizeof(s_words), 0ull};
    const int done = git_words_given(cli_git_stash_act(folder, act, cli_serve_string_or(args, "name", ""),
                                                       cli_serve_string_or(args, "message", ""), &words, said),
                                     &words, out);
    cli_lock_give(&s_words_lock);
    return done;
}

static int git_elsewhere(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    CliCommits *const found = cli_serve_cell(sizeof(*found), "what git finds", said);
    if ((found == NULL) || !git_repo_of(args, folder, said))
    {
        cli_cell_given(found);
        return 0;
    }
    cli_git_elsewhere(folder, found);
    cli_git_commits_written(found, out);
    cli_cell_given(found);
    return 1;
}

static int git_cherry_pick(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    const char *const id = cli_serve_string(args, "id", said);
    if ((id == NULL) || !git_repo_of(args, folder, said))
    {
        return 0;
    }
    cli_lock_take(&s_words_lock);
    CliLine words = {s_words, sizeof(s_words), 0ull};
    const int done = git_words_given(cli_git_cherry_pick(folder, id, &words, said), &words, out);
    cli_lock_give(&s_words_lock);
    return done;
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
    cli_cell_given(found);
    return done;
}

static int file_at(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    const char *const id = (path != NULL) ? cli_serve_string(args, "id", said) : NULL;
    if ((id == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    char *const room = cli_serve_cell(CLI_SERVE_TEXT_AT, "a file's text at a commit", said);
    if (room == NULL)
    {
        return 0;
    }
    CliLine text = {room, CLI_SERVE_TEXT_AT, 0ull};
    const int done = git_words_given(cli_git_text_at(root, path, id, &text, said), &text, out);
    cli_cell_given(room);
    return done;
}

static int repo_init(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    const char *const named = cli_serve_string_or(args, "folder", NULL);
    if (named != NULL)
    {
        CliLine line = {folder, sizeof(folder), 0ull};
        cli_line_text(&line, named);
        (void)cli_line_closed(&line);
    }
    else if (!cli_serve_tree(folder, said))
    {
        return 0;
    }
    cli_lock_take(&s_words_lock);
    CliLine words = {s_words, sizeof(s_words), 0ull};
    const int done = cli_git_init(folder, &words, said);
    cli_lock_give(&s_words_lock);
    if (done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)folder,
                              cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
    }
    return done;
}

const CliServeCall g_cli_serve_git_calls[] = {
    {"git_branches", git_branches},
    {"git_branch", git_branch},
    {"git_commit_parts", git_commit_parts},
    {"git_resolve", git_resolve},
    {"tree_changed", tree_changed},
    {"git_repositories", git_repositories},
    {"tree_branch", tree_branch},
    {"file_head", file_head},
    {"file_commits", file_commits},
    {"git_commit", git_commit},
    {"git_push", git_push},
    {"git_pull", git_pull},
    {"git_rollback", git_rollback},
    {"git_ahead_behind", git_ahead_behind},
    {"git_graph", git_graph},
    {"git_touched", git_touched},
    {"git_stashes", git_stashes},
    {"git_stash", git_stash},
    {"git_elsewhere", git_elsewhere},
    {"git_cherry_pick", git_cherry_pick},
    {"git_line_history", git_line_history},
    {"file_at", file_at},
    {"repo_init", repo_init},
};
const unsigned int g_cli_serve_git_count = sizeof(g_cli_serve_git_calls) / sizeof(g_cli_serve_git_calls[0]);
