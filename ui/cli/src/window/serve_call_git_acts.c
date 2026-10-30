// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_git_acts.c: the calls that have git act on the tree: commits, pushes, pulls, rollbacks, branch and stash
// acts, cherry-picks, resolves and a repository made; and a file's text as the last commit stores it and at a commit.
// Where git answers with its own words, they are written as one JSON string
#include "../code/git.h"
#include "serve_internal.h"

// the bytes git's words and a file's text at a commit are begun in before they grow
#define CLI_SERVE_WORDS_FIRST (4ull << 10u)
#define CLI_SERVE_TEXT_AT_FIRST (64ull << 10u)

// the cells a call having git act occupies while it answers, each sized to what it contains
static CellOwner s_cells = MMGR_CELL_OWNER("cli_serve_git");

// a growing line begun of `first` bytes for git's words or a file's text: 1, or 0 with why given where its cell is
// refused
static int git_line_begun(VerbaLinea *line, unsigned long long first, VerbaLinea *err_msg)
{
    const int begun = cli_growing_line_made(line, first, &s_cells);
    if (!begun)
    {
        mmgr_line_put(err_msg, "git's words could not be given a cell");
    }
    return begun;
}

// the items of the array `node`, 0 where it is not one
static unsigned int git_items(const CliJson *json, unsigned int node)
{
    unsigned int count = 0u;
    const unsigned int first = cli_json_is(json, node, CFG_JSON_ARRAY) ? json->node[node].first : CLI_JSON_NONE;
    for (unsigned int item = first; item != CLI_JSON_NONE; item = json->node[item].next)
    {
        count += 1u;
    }
    return count;
}

// git's words written as a JSON string, where `done`, and their cell given back
static int git_words_given(int done, VerbaLinea *words, VerbaLinea *out)
{
    if (done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)words->out, mmgr_line_ok(words) ? words->at : 0ull);
    }
    g_cli_cells->persistent_buf_release(words->out, &s_cells);
    return done;
}

static int git_branch(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    const char *const act = cli_serve_string(args, "act", err_msg);
    const char *const name = (act != NULL) ? cli_serve_string(args, "name", err_msg) : NULL;
    VerbaLinea words;
    if ((name == NULL) || !cli_serve_git_repo_of(args, folder, err_msg) ||
        !git_line_begun(&words, CLI_SERVE_WORDS_FIRST, err_msg))
    {
        return 0;
    }
    return git_words_given(cli_git_branch_act(folder, act, name, cli_serve_string_or(args, "to", ""), &words, err_msg),
                           &words, out);
}

static int git_commit_parts(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    unsigned int whole = 0u;
    const unsigned int parts = cli_serve_arg(args, "parts");
    const CliJson *const json = args->json;
    const unsigned int first = cli_json_is(json, parts, CFG_JSON_ARRAY) ? json->node[parts].first : CLI_JSON_NONE;
    const unsigned int whole_most = git_items(json, cli_serve_arg(args, "whole"));
    const unsigned int parts_most = git_items(json, parts);
    const char **const whole_of = g_cli_cells->temporary_buf_alloc((whole_most + 1ull) * sizeof(*whole_of), &s_cells);
    CliGitPart *const part_of = g_cli_cells->temporary_buf_alloc((parts_most + 1ull) * sizeof(*part_of), &s_cells);
    const char *const message = cli_serve_string(args, "message", err_msg);
    VerbaLinea words = {NULL, 0ull, 0ull};
    int done = (whole_of != NULL) && (part_of != NULL) && (message != NULL) &&
               cli_serve_strings(args, "whole", whole_of, whole_most, &whole, err_msg) && cli_serve_tree(root, err_msg);
    if ((whole_of == NULL) || (part_of == NULL))
    {
        mmgr_line_put(err_msg, "a commit's files could not be given cells");
    }
    unsigned int count = 0u;
    for (unsigned int one = done ? first : CLI_JSON_NONE; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        unsigned long long length = 0ull;
        const char *const path = cli_json_string(json, cli_json_member(json, one, "path"), NULL);
        const char *const text = cli_json_string(json, cli_json_member(json, one, "text"), &length);
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
    if (done && git_line_begun(&words, CLI_SERVE_WORDS_FIRST, err_msg))
    {
        done = git_words_given(cli_git_commit_parts(root, message, whole_of, whole, part_of, count, &words, err_msg),
                               &words, out);
    }
    else
    {
        done = 0;
    }
    g_cli_cells->persistent_buf_release(part_of, &s_cells);
    g_cli_cells->persistent_buf_release(whole_of, &s_cells);
    return done;
}

static int git_resolve(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    const char *const path = cli_serve_string(args, "path", err_msg);
    if ((path == NULL) || !cli_serve_tree(root, err_msg) || !cli_git_resolve(root, path, err_msg))
    {
        return 0;
    }
    mmgr_line_put(out, "null");
    return 1;
}

static int file_head(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    const char *const path = cli_serve_string(args, "path", err_msg);
    VerbaLinea text;
    if ((path == NULL) || !cli_serve_tree(root, err_msg) || !git_line_begun(&text, CLI_SERVE_TEXT_AT_FIRST, err_msg))
    {
        return 0;
    }
    if (cli_git_head_text(root, path, &text) && mmgr_line_ok(&text))
    {
        cli_json_quoted_lossy(out, (const unsigned char *)text.out, text.at);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    g_cli_cells->persistent_buf_release(text.out, &s_cells);
    return 1;
}

static int git_commit(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    const unsigned int most = git_items(args->json, cli_serve_arg(args, "paths"));
    const char **const paths = g_cli_cells->temporary_buf_alloc((most + 1ull) * sizeof(*paths), &s_cells);
    const char *const message = (paths != NULL) ? cli_serve_string(args, "message", err_msg) : NULL;
    VerbaLinea words;
    int done = 0;
    if (paths == NULL)
    {
        mmgr_line_put(err_msg, "a commit's files could not be given a cell");
    }
    else if ((message != NULL) && cli_serve_strings(args, "paths", paths, most, &count, err_msg) &&
             cli_serve_tree(root, err_msg) && git_line_begun(&words, CLI_SERVE_WORDS_FIRST, err_msg))
    {
        done = git_words_given(cli_git_commit(root, message, paths, count, &words, err_msg), &words, out);
    }
    g_cli_cells->persistent_buf_release(paths, &s_cells);
    return done;
}

// git pushed or pulled at the repository of the arguments: its words written
static int git_pushed_or_pulled(const CliServeArgs *args, int push, VerbaLinea *out, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    VerbaLinea words;
    if (!cli_serve_git_repo_of(args, folder, err_msg) || !git_line_begun(&words, CLI_SERVE_WORDS_FIRST, err_msg))
    {
        return 0;
    }
    return git_words_given(push ? cli_git_push(folder, &words, err_msg) : cli_git_pull(folder, &words, err_msg), &words,
                           out);
}

static int git_push(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    return git_pushed_or_pulled(args, 1, out, err_msg);
}

static int git_pull(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    return git_pushed_or_pulled(args, 0, out, err_msg);
}

static int git_rollback(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    const char *const path = cli_serve_string(args, "path", err_msg);
    if ((path == NULL) || !cli_serve_tree(root, err_msg) || !cli_git_rollback(root, path, err_msg))
    {
        return 0;
    }
    mmgr_line_put(out, "null");
    return 1;
}

static int git_stash(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    const char *const act = cli_serve_string(args, "act", err_msg);
    VerbaLinea words;
    if ((act == NULL) || !cli_serve_git_repo_of(args, folder, err_msg) ||
        !git_line_begun(&words, CLI_SERVE_WORDS_FIRST, err_msg))
    {
        return 0;
    }
    return git_words_given(cli_git_stash_act(folder, act, cli_serve_string_or(args, "name", ""),
                                             cli_serve_string_or(args, "message", ""), &words, err_msg),
                           &words, out);
}

static int git_cherry_pick(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    const char *const id = cli_serve_string(args, "id", err_msg);
    VerbaLinea words;
    if ((id == NULL) || !cli_serve_git_repo_of(args, folder, err_msg) ||
        !git_line_begun(&words, CLI_SERVE_WORDS_FIRST, err_msg))
    {
        return 0;
    }
    return git_words_given(cli_git_cherry_pick(folder, id, &words, err_msg), &words, out);
}

static int file_at(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char root[CLI_PATH_MAX_LEN];
    const char *const path = cli_serve_string(args, "path", err_msg);
    const char *const id = (path != NULL) ? cli_serve_string(args, "id", err_msg) : NULL;
    VerbaLinea text;
    if ((id == NULL) || !cli_serve_tree(root, err_msg) || !git_line_begun(&text, CLI_SERVE_TEXT_AT_FIRST, err_msg))
    {
        return 0;
    }
    return git_words_given(cli_git_text_at(root, path, id, &text, err_msg), &text, out);
}

static int repo_init(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    const char *const named = cli_serve_string_or(args, "folder", NULL);
    if (named != NULL)
    {
        VerbaLinea line = {folder, sizeof(folder), 0ull};
        mmgr_line_put(&line, named);
        (void)mmgr_line_finish(&line);
    }
    else if (!cli_serve_tree(folder, err_msg))
    {
        return 0;
    }
    VerbaLinea words;
    if (!git_line_begun(&words, CLI_SERVE_WORDS_FIRST, err_msg))
    {
        return 0;
    }
    const int done = cli_git_init(folder, &words, err_msg);
    g_cli_cells->persistent_buf_release(words.out, &s_cells);
    if (done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)folder,
                              cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}));
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
