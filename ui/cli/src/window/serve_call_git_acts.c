// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_git_acts.c: the calls that have git act on the tree: commits, pushes, pulls, rollbacks, branch and stash
// acts, cherry-picks, resolves and a repository made; and a file's text as the last commit holds it and at a commit.
// Where git answers with its own words, they are written as one JSON string
#include "../code/git.h"
#include "serve_internal.h"

// the bytes git's words and a file's text at a commit are begun in before they grow
#define CLI_SERVE_WORDS_FIRST (4ull << 10u)
#define CLI_SERVE_TEXT_AT_FIRST (64ull << 10u)

// the cells a call having git act holds while it answers, each sized to what it holds
static CliCellsTally s_cells = CLI_CELLS_TALLY("cli_serve_git");

// a growing line begun of `first` bytes for git's words or a file's text: 1, or 0 with why said where its cell is
// refused
static int git_line_begun(CliLine *line, unsigned long long first, CliLine *said)
{
    const int begun = cli_line_growing(line, first, "git's words");
    if (!begun)
    {
        cli_line_text(said, "git's words could not be given a cell");
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
static int git_words_given(int done, CliLine *words, CliLine *out)
{
    if (done)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)words->out, cli_line_fits(words) ? words->at : 0ull);
    }
    cli_cell_given(words->out);
    return done;
}

static int git_branch(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    const char *const act = cli_serve_string(args, "act", said);
    const char *const name = (act != NULL) ? cli_serve_string(args, "name", said) : NULL;
    CliLine words;
    if ((name == NULL) || !cli_serve_git_repo_of(args, folder, said) ||
        !git_line_begun(&words, CLI_SERVE_WORDS_FIRST, said))
    {
        return 0;
    }
    return git_words_given(cli_git_branch_act(folder, act, name, cli_serve_string_or(args, "to", ""), &words, said),
                           &words, out);
}

static int git_commit_parts(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned int whole = 0u;
    const unsigned int parts = cli_serve_arg(args, "parts");
    const CliJson *const json = args->json;
    const unsigned int first = cli_json_is(json, parts, CFG_JSON_ARRAY) ? json->node[parts].first : CLI_JSON_NONE;
    const unsigned int whole_most = git_items(json, cli_serve_arg(args, "whole"));
    const unsigned int parts_most = git_items(json, parts);
    const char **const whole_of =
        cli_cell_taken_by(&s_cells, (whole_most + 1ull) * sizeof(*whole_of), "the files a commit takes whole");
    CliGitPart *const part_of =
        cli_cell_taken_by(&s_cells, (parts_most + 1ull) * sizeof(*part_of), "the parts a commit takes");
    const char *const message = cli_serve_string(args, "message", said);
    CliLine words = {NULL, 0ull, 0ull};
    int done = (whole_of != NULL) && (part_of != NULL) && (message != NULL) &&
               cli_serve_strings(args, "whole", whole_of, whole_most, &whole, said) && cli_serve_tree(root, said);
    if ((whole_of == NULL) || (part_of == NULL))
    {
        cli_line_text(said, "a commit's files could not be given cells");
    }
    unsigned int count = 0u;
    for (unsigned int one = done ? first : CLI_JSON_NONE; one != CLI_JSON_NONE; one = json->node[one].next)
    {
        unsigned long long length = 0ull;
        const char *const path = cli_json_string(json, cli_json_member(json, one, "path"), NULL);
        const char *const text = cli_json_string(json, cli_json_member(json, one, "text"), &length);
        if ((path == NULL) || (text == NULL))
        {
            cli_line_text(said, "parts: missing field `");
            cli_line_text(said, (path == NULL) ? "path" : "text");
            cli_line_character(said, '`');
            done = 0;
            break;
        }
        part_of[count] = (CliGitPart){path, text, length};
        count += 1u;
    }
    if (done && git_line_begun(&words, CLI_SERVE_WORDS_FIRST, said))
    {
        done = git_words_given(cli_git_commit_parts(root, message, whole_of, whole, part_of, count, &words, said),
                               &words, out);
    }
    else
    {
        done = 0;
    }
    cli_cell_given_by(&s_cells, part_of);
    cli_cell_given_by(&s_cells, whole_of);
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

static int file_head(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    CliLine text;
    if ((path == NULL) || !cli_serve_tree(root, said) || !git_line_begun(&text, CLI_SERVE_TEXT_AT_FIRST, said))
    {
        return 0;
    }
    if (cli_git_head_text(root, path, &text) && cli_line_fits(&text))
    {
        cli_json_quoted_lossy(out, (const unsigned char *)text.out, text.at);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_cell_given(text.out);
    return 1;
}

static int git_commit(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    unsigned int count = 0u;
    const unsigned int most = git_items(args->json, cli_serve_arg(args, "paths"));
    const char **const paths = cli_cell_taken_by(&s_cells, (most + 1ull) * sizeof(*paths), "the files a commit takes");
    const char *const message = (paths != NULL) ? cli_serve_string(args, "message", said) : NULL;
    CliLine words;
    int done = 0;
    if (paths == NULL)
    {
        cli_line_text(said, "a commit's files could not be given a cell");
    }
    else if ((message != NULL) && cli_serve_strings(args, "paths", paths, most, &count, said) &&
             cli_serve_tree(root, said) && git_line_begun(&words, CLI_SERVE_WORDS_FIRST, said))
    {
        done = git_words_given(cli_git_commit(root, message, paths, count, &words, said), &words, out);
    }
    cli_cell_given_by(&s_cells, paths);
    return done;
}

// git pushed or pulled at the repository of the arguments: its words written
static int git_pushed_or_pulled(const CliServeArgs *args, int push, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    CliLine words;
    if (!cli_serve_git_repo_of(args, folder, said) || !git_line_begun(&words, CLI_SERVE_WORDS_FIRST, said))
    {
        return 0;
    }
    return git_words_given(push ? cli_git_push(folder, &words, said) : cli_git_pull(folder, &words, said), &words, out);
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

static int git_stash(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    const char *const act = cli_serve_string(args, "act", said);
    CliLine words;
    if ((act == NULL) || !cli_serve_git_repo_of(args, folder, said) ||
        !git_line_begun(&words, CLI_SERVE_WORDS_FIRST, said))
    {
        return 0;
    }
    return git_words_given(cli_git_stash_act(folder, act, cli_serve_string_or(args, "name", ""),
                                             cli_serve_string_or(args, "message", ""), &words, said),
                           &words, out);
}

static int git_cherry_pick(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    const char *const id = cli_serve_string(args, "id", said);
    CliLine words;
    if ((id == NULL) || !cli_serve_git_repo_of(args, folder, said) ||
        !git_line_begun(&words, CLI_SERVE_WORDS_FIRST, said))
    {
        return 0;
    }
    return git_words_given(cli_git_cherry_pick(folder, id, &words, said), &words, out);
}

static int file_at(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    const char *const id = (path != NULL) ? cli_serve_string(args, "id", said) : NULL;
    CliLine text;
    if ((id == NULL) || !cli_serve_tree(root, said) || !git_line_begun(&text, CLI_SERVE_TEXT_AT_FIRST, said))
    {
        return 0;
    }
    return git_words_given(cli_git_text_at(root, path, id, &text, said), &text, out);
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
    CliLine words;
    if (!git_line_begun(&words, CLI_SERVE_WORDS_FIRST, said))
    {
        return 0;
    }
    const int done = cli_git_init(folder, &words, said);
    cli_cell_given(words.out);
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
