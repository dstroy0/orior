// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_test.c: git's check: a clone is named as git names it; a file taken in part commits that part alone; a branch is
// made, switched to, renamed, merged and deleted; a merged branch takes a column of its own and joins where it began;
// a repository owns the deepest folder that holds a path; and each status has one letter
#include "../../../../../ui/cli/src/code/git_internal.h"
#include "cli_check.h"

static CliBranches s_branches;
static CliGraph s_graph;
static char s_out[1u << 16u];

// git run at `root` with the words given, NULL past the last: 1, its output trimmed into s_out, or 0
static int git_do(const char *root, const char *one, const char *two, const char *three, const char *four)
{
    const char *const words[4] = {one, two, three, four};
    unsigned int count = 0u;
    while ((count < 4u) && (words[count] != NULL))
    {
        count += 1u;
    }
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    const int ran = cli_git_run(root, words, count, &out, &said);
    (void)cli_line_closed(&out);
    return ran;
}

// a repository made at the folder `name` of the run's, into `root`, its user t
static void git_started(const char *name, char *root)
{
    cli_check_folder(name, root);
    cli_check(git_do(root, "init", "-q", "-b", "main") && git_do(root, "config", "user.name", "t", NULL) &&
                  git_do(root, "config", "user.email", "t@t", NULL),
              "a repository is made");
}

// a file of the tree `root` written
static void git_written(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the file `name` of the tree `root` read into s_out
static const char *git_read(const char *root, const char *name)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_out, sizeof(s_out) - 1u);
    s_out[(size != QRY_MEMBER_UNREAD) ? size : 0ull] = '\0';
    return s_out;
}

// the folder a clone of `url` makes under work checked against work/orior, or none where `wanted` is 0
static void git_clone_is(const char *url, int wanted)
{
    char folder[CLI_PATH_LONGEST];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    const int named = cli_git_clone_folder(url, "work", folder, &said);
    cli_check(
        wanted ? (named && (cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = "work/orior", .cap = CLI_CELLS_POOL})))
               : !named,
        url);
}

// a file taken in part commits that part alone
static void git_parts(void)
{
    char root[CLI_PATH_LONGEST];
    char why[1024];
    char out[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine line = {out, sizeof(out), 0ull};
    git_started("git_parts", root);
    git_written(root, "a.txt", "one\ntwo\nthree\nfour\n");
    (void)git_do(root, "add", ".", NULL, NULL);
    (void)git_do(root, "commit", "-q", "-m", "first");
    git_written(root, "a.txt", "one\nTWO\nthree\nFOUR\n");
    git_written(root, "other.txt", "staged apart\n");
    (void)git_do(root, "add", "other.txt", NULL, NULL);
    static const char s_part[] = "one\nTWO\nthree\nfour\n";
    const CliGitPart part = {"a.txt", s_part, sizeof(s_part) - 1u};
    const unsigned long long held = g_cli_git_cells.holds;
    cli_check(!cli_git_commit_parts(root, "  \n", NULL, 0u, &part, 1u, &line, &said) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = why, .other = "a commit needs a message", .cap = sizeof(why)}),
              "a commit with no message is refused");
    said.at = 0ull;
    cli_check(cli_git_commit_parts(root, "part of a", NULL, 0u, &part, 1u, &line, &said), "a part is committed");
    cli_check(g_cli_git_cells.holds == held, "a commit gives its room back, made or refused");
    cli_check(
        git_do(root, "show", "HEAD:a.txt", NULL, NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "one\nTWO\nthree\nfour", .cap = CLI_CELLS_POOL})),
        "the commit holds the part");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = git_read(root, "a.txt"), .other = "one\nTWO\nthree\nFOUR\n", .cap = CLI_CELLS_POOL}),
              "the file is as it was");
    cli_check(git_do(root, "show", "--name-only", "--format=", "HEAD") &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "a.txt", .cap = CLI_CELLS_POOL})),
              "and the commit that file alone");
    // git's output comes trimmed, the first line's leading space with it
    cli_check(
        git_do(root, "status", "--porcelain", NULL, NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "M a.txt\nA  other.txt", .cap = CLI_CELLS_POOL})),
        "what was staged apart stays staged");
}

// the branch act `act` on `name`: 1, or 0
static int git_act(const char *root, const char *act, const char *name, const char *to)
{
    char why[1024];
    char out[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine line = {out, sizeof(out), 0ull};
    return cli_git_branch_act(root, act, name, to, &line, &said);
}

// a branch is made, switched to, renamed, merged and deleted
static void git_branching(void)
{
    char root[CLI_PATH_LONGEST];
    git_started("git_branches", root);
    git_written(root, "a.txt", "one");
    (void)git_do(root, "add", ".", NULL, NULL);
    (void)git_do(root, "commit", "-q", "-m", "first");
    cli_check(git_act(root, "create", "topic", ""), "a branch is made");
    git_written(root, "a.txt", "two");
    (void)git_do(root, "commit", "-q", "-am", "second");
    cli_git_text_given(&s_branches.text);
    cli_git_branches(root, &s_branches);
    int current = 0;
    int subject = 0;
    for (unsigned int at = 0u; at < s_branches.count; at += 1u)
    {
        const CliBranch *const branch = &s_branches.item[at];
        const int topic = cellul.eq(&(CatenaFinitaCfg){
            .src = cli_git_text_of(&s_branches.text, branch->name), .other = "topic", .cap = CLI_CELLS_POOL});
        current = current || (branch->current && topic);
        subject = subject ||
                  (topic && (cellul.eq(&(CatenaFinitaCfg){.src = cli_git_text_of(&s_branches.text, branch->subject),
                                                          .other = "second",
                                                          .cap = CLI_CELLS_POOL})));
    }
    cli_check(current && subject, "the tree is on it, its last commit's subject with it");
    cli_check(git_act(root, "rename", "topic", "feature") && git_act(root, "switch", "main", ""),
              "it is renamed and another switched to");
    cli_check(!git_act(root, "delete", "feature", ""), "a branch not merged is not deleted");
    cli_check(
        git_act(root, "merge", "feature", "") &&
            (cellul.eq(&(CatenaFinitaCfg){.src = git_read(root, "a.txt"), .other = "two", .cap = CLI_CELLS_POOL})),
        "it is merged");
    cli_check(git_act(root, "delete", "feature", ""), "and then deleted");
    cli_git_text_given(&s_branches.text);
    cli_git_branches(root, &s_branches);
    cli_check((s_branches.count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_git_text_of(&s_branches.text, s_branches.item[0].name),
                                                .other = "main",
                                                .cap = CLI_CELLS_POOL})),
              "main alone is left");
    cli_check(!git_act(root, "create", "-x", "") && !git_act(root, "create", "bad..name", ""),
              "a name git refuses is refused");
}

// a commit of the graph put with its parents
static void git_commit_put(const char *id, const char *first, const char *second)
{
    CliDrawn *const drawn = &s_graph.item[s_graph.count];
    memor.set(&(MemoriaCfg){.dst = drawn, .val = 0, .bytes = sizeof(*drawn)});
    drawn->id = cli_git_text_put(&s_graph.text, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    const char *const parents[2] = {first, second};
    for (unsigned int at = 0u; (at < 2u) && (parents[at] != NULL); at += 1u)
    {
        drawn->parent[at] = cli_git_text_put(&s_graph.text, parents[at],
                                             cellul.len(&(CatenaFinitaCfg){.src = parents[at], .cap = CLI_CELLS_POOL}));
        drawn->parents = at + 1u;
    }
    s_graph.count += 1u;
}

// the strokes of row `index`, each `from to half color` joined by `|`, into `out`, which holds `room`
static const char *git_strokes(unsigned int index, char *out, unsigned long long room)
{
    const CliDrawn *const drawn = &s_graph.item[index];
    CliLine line = {out, room, 0ull};
    for (unsigned int at = 0u; at < drawn->stroke_count; at += 1u)
    {
        const unsigned int *const stroke = s_graph.stroke[drawn->first_stroke + at];
        cli_line_text(&line, (at > 0u) ? "|" : "");
        for (unsigned int part = 0u; part < 4u; part += 1u)
        {
            cli_line_text(&line, (part > 0u) ? " " : "");
            cli_line_decimal(&line, stroke[part], 1u);
        }
    }
    (void)cli_line_closed(&line);
    return out;
}

// a merged branch takes a column of its own and joins where it began
static void git_laid_out(void)
{
    cli_git_text_given(&s_graph.text);
    cli_git_text_begun(&s_graph.text);
    s_graph.count = 0u;
    s_graph.strokes = 0u;
    git_commit_put("m", "a3", "b2");
    git_commit_put("a3", "a2", NULL);
    git_commit_put("b2", "b1", NULL);
    git_commit_put("b1", "a2", NULL);
    git_commit_put("a2", "a1", NULL);
    git_commit_put("a1", NULL, NULL);
    cli_git_laid_out(&s_graph);
    static const unsigned int s_lanes[6] = {0u, 0u, 1u, 1u, 0u, 0u};
    int lanes = 1;
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        lanes = lanes && (s_graph.item[at].lane == s_lanes[at]);
    }
    cli_check(lanes, "a merged branch takes a column of its own");
    char strokes[256];
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = git_strokes(0u, strokes, sizeof(strokes)), .other = "0 0 1 0|0 1 1 1", .cap = CLI_CELLS_POOL}),
              "the merge opens it");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = git_strokes(1u, strokes, sizeof(strokes)),
                                           .other = "0 0 0 0|1 1 0 1|0 0 1 0|1 1 1 1",
                                           .cap = CLI_CELLS_POOL}),
              "each column goes on past another's commit");
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = git_strokes(3u, strokes, sizeof(strokes)),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "1 0 1 0",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "and joins where it began");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = git_strokes(4u, strokes, sizeof(strokes)), .other = "0 0 0 0|0 0 1 0", .cap = CLI_CELLS_POOL}),
              "one column after");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = git_strokes(5u, strokes, sizeof(strokes)), .other = "0 0 0 0", .cap = CLI_CELLS_POOL}),
              "the first commit ends it");
    cli_check(s_graph.item[2].color == 1u, "the branch's line its own color");
}

// the repository of `repos`, `count` of them, that owns `path`, checked against `wanted`, NULL for none
static void git_owner_is(const char *const *repos, unsigned int count, const char *path, const char *wanted)
{
    const char *const owner =
        cli_git_owner(repos, count, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_check((wanted == NULL)
                  ? (owner == NULL)
                  : ((owner != NULL) &&
                     (cellul.eq(&(CatenaFinitaCfg){.src = owner, .other = wanted, .cap = CLI_CELLS_POOL}))),
              path);
}

int main(void)
{
    git_clone_is("https://github.com/dstroy0/orior.git", 1);
    git_clone_is("https://github.com/dstroy0/orior/", 1);
    git_clone_is("git@github.com:dstroy0/orior.git", 1);
    git_clone_is("git@host:orior", 1);
    git_clone_is("D:\\repos\\orior.git", 1);
    git_clone_is("https://", 0);
    git_clone_is("  ", 0);
    git_parts();
    git_branching();
    git_laid_out();
    static const char *const s_repos[3] = {"", "lib", "lib/inner"};
    git_owner_is(s_repos, 3u, "a.txt", "");
    git_owner_is(s_repos, 3u, "lib/a.txt", "lib");
    git_owner_is(s_repos, 3u, "lib/inner/b/c.txt", "lib/inner");
    git_owner_is(s_repos, 3u, "library/a.txt", "");
    git_owner_is(s_repos + 1, 2u, "a.txt", NULL);
    cli_check((cli_git_state_of('?', '?') == 'U') && (cli_git_state_of(' ', 'M') == 'M') &&
                  (cli_git_state_of('M', ' ') == 'M') && (cli_git_state_of('A', ' ') == 'A') &&
                  (cli_git_state_of(' ', 'D') == 'D') && (cli_git_state_of('R', ' ') == 'R') &&
                  (cli_git_state_of('U', 'U') == 'C') && (cli_git_state_of('A', 'A') == 'C'),
              "each status has one letter");
    cli_git_text_given(&s_branches.text);
    cli_git_text_given(&s_graph.text);
    cli_check((g_cli_git_cells.holds == 0ull) && (g_cli_git_cells.hw != 0ull), "each list's texts are given back");
    return cli_checked();
}
