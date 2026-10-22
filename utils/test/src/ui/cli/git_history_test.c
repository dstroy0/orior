// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_history_test.c: git's history: a commit lists the files it changed and a search finds it; each line has the
// commit that last changed it; each repository of a tree keeps its own changes and commits; and changes are put aside
// and brought back, and a commit is picked from another branch
#include "../../../../../ui/cli/src/code/git.h"
#include "cli_check.h"

static CliTouchedList s_touched;
static CliGraph s_graph;
static CliLineHistory s_lines;
static CliRepositories s_repos;
static CliChangedList s_changed;
static CliCommits s_commits;
static CliStashes s_stashes;
static char s_out[1u << 16u];

// git run at `root` with the words given, NULL past the last: 1, its output trimmed into s_out, or 0
static int history_git(const char *root, const char *one, const char *two, const char *three, const char *four)
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

// a file of the tree `root` written
static void history_written(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// a repository made at `root` on `branch`, its user `user`, `file` holding `text` committed as its first
static void history_started(const char *root, const char *branch, const char *user, const char *file, const char *text)
{
    (void)query_folder_made(root);
    cli_check(history_git(root, "init", "-q", "-b", branch) && history_git(root, "config", "user.name", user, NULL) &&
                  history_git(root, "config", "user.email", "t@t", NULL),
              "a repository is made");
    if (file != NULL)
    {
        history_written(root, file, text);
        (void)history_git(root, "add", file, NULL, NULL);
        (void)history_git(root, "commit", "-q", "-m", "first");
    }
}

// the file `name` of the tree `root` is there
static int history_there(const char *root, const char *name)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    return query_is_file(path);
}

// a touched file `path state was` is among those found
static int history_touched(const char *wanted)
{
    for (unsigned int at = 0u; at < s_touched.count; at += 1u)
    {
        const CliTouched *const one = &s_touched.item[at];
        char held[CLI_PATH_LONGEST * 2u];
        CliLine line = {held, sizeof(held), 0ull};
        cli_line_text(&line, cli_git_text_of(&s_touched.text, one->path));
        cli_line_character(&line, ' ');
        cli_line_character(&line, one->state);
        if (one->has_was)
        {
            cli_line_character(&line, ' ');
            cli_line_text(&line, cli_git_text_of(&s_touched.text, one->was));
        }
        (void)cli_line_closed(&line);
        if (cellul.eq(&(CatenaFinitaCfg){.src = held, .other = wanted, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// a commit lists the files it changed and a search finds it
static void history_graph(void)
{
    char root[CLI_PATH_LONGEST];
    char first[CLI_NAME_LONGEST];
    char second[CLI_NAME_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("git_graph", root);
    history_started(root, "main", "Ada", NULL, NULL);
    history_written(root, "a.txt", "one\ntwo\nthree\nfour\n");
    history_written(root, "b.txt", "b\n");
    (void)history_git(root, "add", ".", NULL, NULL);
    (void)history_git(root, "commit", "-q", "-m", "first");
    (void)history_git(root, "mv", "a.txt", "c.txt", NULL);
    (void)history_git(root, "rm", "-q", "b.txt", NULL);
    history_written(root, "d.txt", "d\n");
    (void)history_git(root, "add", "-A", NULL, NULL);
    (void)history_git(root, "commit", "-q", "-m", "Second, renamed");
    (void)history_git(root, "rev-parse", "HEAD~1", NULL, NULL);
    memor.cpy(&(MemoriaCfg){
        .dst = first, .src = s_out, .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL}) + 1u});
    (void)history_git(root, "rev-parse", "HEAD", NULL, NULL);
    memor.cpy(&(MemoriaCfg){.dst = second,
                            .src = s_out,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL}) + 1u});
    cli_git_text_given(&s_touched.text);
    cli_check(cli_git_touched(root, second, &s_touched, &said) && (s_touched.count == 3u) &&
                  history_touched("b.txt D") && history_touched("c.txt R a.txt") && history_touched("d.txt A"),
              "a commit lists the files it changed, a rename with the name it had");
    cli_git_text_given(&s_touched.text);
    cli_check(cli_git_touched(root, first, &s_touched, &said) && (s_touched.count == 2u), "the first, from nothing");
    cli_git_text_given(&s_touched.text);
    cli_check(!cli_git_touched(root, "not-hex", &s_touched, &said), "an id that is none is refused");
    cli_git_text_given(&s_graph.text);
    cli_git_graph(root, "", 0, &s_graph);
    int main_ref = 0;
    for (unsigned int at = 0u; (s_graph.count > 0u) && (at < s_graph.item[0].refs); at += 1u)
    {
        main_ref =
            main_ref || (cellul.find(&(CatenaFinitaCfg){.src = cli_git_text_of(&s_graph.text, s_graph.item[0].ref[at]),
                                                        .cap = CLI_CELLS_POOL,
                                                        .other = "main",
                                                        .other_cap = CLI_CELLS_POOL}) != NULL);
    }
    cli_check(
        (s_graph.count == 2u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_git_text_of(&s_graph.text, s_graph.item[0].subject),
                                          .other = "Second, renamed",
                                          .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_git_text_of(&s_graph.text, s_graph.item[1].subject),
                                          .other = "first",
                                          .cap = CLI_CELLS_POOL})) &&
            main_ref &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_git_text_of(&s_graph.text, s_graph.item[0].author), .other = "Ada", .cap = CLI_CELLS_POOL})),
        "the graph holds every commit, its branch and its author");
    cli_git_text_given(&s_graph.text);
    cli_git_graph(root, "RENAMED ada", 1, &s_graph);
    cli_check(
        (s_graph.count == 1u) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_git_text_of(&s_graph.text, s_graph.item[0].id), .other = second, .cap = CLI_CELLS_POOL})),
        "a search finds it, each word in any case");
    cli_git_text_given(&s_graph.text);
    cli_git_graph(root, "RENAMED ada", 0, &s_graph);
    cli_check(s_graph.count == 0u, "an author is matched only where authors show");
    cli_git_text_given(&s_graph.text);
    cli_git_graph(root, "RENAMED", 0, &s_graph);
    cli_check(s_graph.count == 1u, "its subject alone");
    cli_git_text_given(&s_graph.text);
    cli_git_graph(root, "nowhere", 0, &s_graph);
    cli_check(s_graph.count == 0u, "and nothing where nothing holds it");
}

// the subject of the commit that last changed line `line` of s_lines, "-" for none
static const char *history_line_subject(unsigned int line)
{
    const int at = s_lines.line[line];
    return (at < 0) ? "-" : cli_git_text_of(&s_lines.text, s_lines.commit[at].subject);
}

// each line has the commit that last changed it
static void history_lines(void)
{
    char base[CLI_PATH_LONGEST];
    char root[CLI_PATH_LONGEST];
    char first[CLI_NAME_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("git_lines", base);
    cli_full(base, "sub", root);
    (void)query_folder_made(root);
    history_started(base, "main", "Ada", NULL, NULL);
    history_written(root, "a.txt", "one\ntwo\n");
    (void)history_git(base, "add", ".", NULL, NULL);
    (void)history_git(base, "commit", "-q", "-m", "first");
    history_written(root, "a.txt", "one\nsecond\n");
    (void)history_git(base, "commit", "-q", "-am", "second");
    (void)history_git(base, "rev-parse", "HEAD~1", NULL, NULL);
    memor.cpy(&(MemoriaCfg){
        .dst = first, .src = s_out, .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL}) + 1u});
    static const char s_text[] = "one\nsecond\nnew\n";
    cli_git_text_given(&s_lines.text);
    cli_check(
        cli_git_line_history(root, "a.txt", s_text, sizeof(s_text) - 1u, &s_lines, &said) && (s_lines.lines >= 3u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = history_line_subject(0u), .other = "first", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = history_line_subject(1u), .other = "second", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = history_line_subject(2u), .other = "-", .cap = CLI_CELLS_POOL})),
        "each line has the commit that last changed it, a new line none");
    const CliLineCommit *const second = &s_lines.commit[(s_lines.line[1] < 0) ? 0 : s_lines.line[1]];
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_git_text_of(&s_lines.text, second->author), .other = "Ada", .cap = CLI_CELLS_POOL})) &&
                  second->has_path &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_git_text_of(&s_lines.text, second->path), .other = "a.txt", .cap = CLI_CELLS_POOL})) &&
                  second->has_parent &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_git_text_of(&s_lines.text, second->parent), .other = first, .cap = CLI_CELLS_POOL})) &&
                  second->has_was &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_git_text_of(&s_lines.text, second->was), .other = "a.txt", .cap = CLI_CELLS_POOL})),
              "its author, its path, and the commit before it with the path there");
    cli_git_text_given(&s_lines.text);
    cli_check(!cli_git_line_history(root, "missing.txt", "", 0ull, &s_lines, &said), "a file git has not is refused");
}

// whether the changed files hold `path` in `state`
static int history_changed(const char *path, char state)
{
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        if ((cellul.eq(&(CatenaFinitaCfg){.src = cli_git_text_of(&s_changed.text, s_changed.item[at].path),
                                          .other = path,
                                          .cap = CLI_CELLS_POOL})) &&
            (s_changed.item[at].state == state))
        {
            return 1;
        }
    }
    return 0;
}

// each repository of a tree keeps its own changes and commits
static void history_repositories(void)
{
    char root[CLI_PATH_LONGEST];
    char inner[CLI_PATH_LONGEST];
    char why[1024];
    char out[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine line = {out, sizeof(out), 0ull};
    cli_check_folder("git_repos", root);
    history_started(root, "main", "t", "a.txt", "one\n");
    cli_full(root, "libs/inner", inner);
    history_started(inner, "trunk", "t", "b.txt", "two\n");
    cli_git_text_given(&s_repos.text);
    cli_git_repositories(root, &s_repos);
    for (unsigned int at = 0u; at < s_repos.count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, cli_git_text_of(&s_repos.text, s_repos.item[at].path));
        cli_line_character(&line, ':');
        cli_line_text(&line,
                      s_repos.item[at].has_branch ? cli_git_text_of(&s_repos.text, s_repos.item[at].branch) : "-");
    }
    cli_check_text(out, line.at, ":main|libs/inner:trunk", "the tree's repositories and their branches");
    history_written(root, "a.txt", "one more\n");
    history_written(inner, "b.txt", "two more\n");
    cli_git_text_given(&s_changed.text);
    cli_git_changed(root, NULL, 0u, 0, &s_changed);
    cli_check((s_changed.count == 2u) && history_changed("a.txt", 'M') && history_changed("libs/inner/b.txt", 'M'),
              "each repository's changes by their paths in the tree");
    static const char *const s_paths[2] = {"a.txt", "libs/inner/b.txt"};
    line.at = 0ull;
    cli_check(cli_git_commit(root, "both", s_paths, 2u, &line, &said), "the two are committed");
    cli_check(history_git(root, "log", "-1", "--format=%s", NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "both", .cap = CLI_CELLS_POOL})) &&
                  history_git(inner, "log", "-1", "--format=%s", NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "both", .cap = CLI_CELLS_POOL})),
              "a commit to each repository");
    cli_git_text_given(&s_changed.text);
    cli_git_changed(root, NULL, 0u, 0, &s_changed);
    cli_check(s_changed.count == 0u, "and nothing is left changed");
    line.at = 0ull;
    cli_check(cli_git_head_text(root, "libs/inner/b.txt", &line), "a file's text as the last commit left it");
    cli_check_text(out, line.at, "two more\n", "read from its own repository");
    cli_git_text_given(&s_commits.text);
    cli_check(cli_git_commits(root, "libs/inner/b.txt", &s_commits, &said) && (s_commits.count == 2u),
              "the commits that touched it");
    line.at = 0ull;
    cli_check(
        cli_git_text_at(root, "libs/inner/b.txt", cli_git_text_of(&s_commits.text, s_commits.item[1].id), &line, &said),
        "its text as a commit left it");
    cli_check_text(out, line.at, "two\n", "the first");
    cli_git_text_given(&s_lines.text);
    cli_check(cli_git_line_history(root, "libs/inner/b.txt", "two more\n", 9ull, &s_lines, &said) &&
                  (s_lines.commits > 0u) && s_lines.commit[0].has_path &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_git_text_of(&s_lines.text, s_lines.commit[0].path),
                                                .other = "libs/inner/b.txt",
                                                .cap = CLI_CELLS_POOL})),
              "and its lines' commits by its path in the tree");
}

// a stash act: 1, or 0
static int history_stash(const char *root, const char *act, const char *name, const char *message)
{
    char why[1024];
    char out[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine line = {out, sizeof(out), 0ull};
    return cli_git_stash_act(root, act, name, message, &line, &said);
}

// changes are put aside and brought back, and a commit is picked from another branch
static void history_stashing(void)
{
    char root[CLI_PATH_LONGEST];
    char picked[CLI_NAME_LONGEST];
    char path[CLI_PATH_LONGEST];
    char why[1024];
    char out[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine line = {out, sizeof(out), 0ull};
    cli_check_folder("git_stash", root);
    history_started(root, "main", "t", "a.txt", "one\n");
    history_written(root, "a.txt", "changed\n");
    history_written(root, "b.txt", "new\n");
    cli_check(history_stash(root, "push", "", "half done") && history_git(root, "status", "--porcelain", NULL, NULL) &&
                  (s_out[0] == '\0'),
              "every change is put aside, new files with them");
    cli_git_text_given(&s_stashes.text);
    cli_git_stashes(root, &s_stashes);
    const char *const subject =
        (s_stashes.count > 0u) ? cli_git_text_of(&s_stashes.text, s_stashes.item[0].subject) : "";
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = subject, .cap = CLI_CELLS_POOL});
    cli_check(
        (s_stashes.count == 1u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_git_text_of(&s_stashes.text, s_stashes.item[0].name),
                                          .other = "stash@{0}",
                                          .cap = CLI_CELLS_POOL})) &&
            (length >= 9ull) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = subject + length - 9u, .other = "half done", .cap = CLI_CELLS_POOL})),
        "a stash, its message with it");
    cli_check(history_stash(root, "apply", "stash@{0}", ""), "it is brought back");
    cli_full(root, "a.txt", path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_out, sizeof(s_out) - 1u);
    cli_check_text(s_out, (size != QRY_MEMBER_UNREAD) ? size : 0ull, "changed\n", "its changes");
    cli_git_text_given(&s_stashes.text);
    cli_git_stashes(root, &s_stashes);
    cli_check(history_there(root, "b.txt") && (s_stashes.count == 1u), "its new files, and kept");
    (void)history_git(root, "checkout", "--", "a.txt", NULL);
    cli_full(root, "b.txt", path);
    (void)query_removed(path);
    const int popped = history_stash(root, "pop", "stash@{0}", "");
    cli_git_text_given(&s_stashes.text);
    cli_git_stashes(root, &s_stashes);
    cli_check(popped && (s_stashes.count == 0u), "a pop drops it");
    cli_check(history_stash(root, "push", "", "") && history_stash(root, "drop", "stash@{0}", ""),
              "a stash is thrown away");
    cli_git_text_given(&s_stashes.text);
    cli_git_stashes(root, &s_stashes);
    cli_check(s_stashes.count == 0u, "and gone");
    cli_check(!history_stash(root, "apply", "HEAD", "") && !history_stash(root, "clear", "stash@{0}", ""),
              "what is not a stash, and an act not asked for, are refused");
    (void)history_git(root, "switch", "-q", "-c", "topic");
    history_written(root, "c.txt", "picked\n");
    (void)history_git(root, "add", ".", NULL, NULL);
    (void)history_git(root, "commit", "-q", "-m", "on topic");
    (void)history_git(root, "rev-parse", "HEAD", NULL, NULL);
    memor.cpy(&(MemoriaCfg){.dst = picked,
                            .src = s_out,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL}) + 1u});
    (void)history_git(root, "switch", "-q", "main", NULL);
    cli_git_text_given(&s_commits.text);
    cli_git_elsewhere(root, &s_commits);
    cli_check((s_commits.count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_git_text_of(&s_commits.text, s_commits.item[0].id),
                                                .other = picked,
                                                .cap = CLI_CELLS_POOL})),
              "a commit of another branch");
    cli_check(cli_git_cherry_pick(root, picked, &line, &said) && history_there(root, "c.txt") &&
                  history_git(root, "log", "-1", "--format=%s", NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "on topic", .cap = CLI_CELLS_POOL})),
              "is picked onto this one");
    cli_git_text_given(&s_commits.text);
    cli_git_elsewhere(root, &s_commits);
    cli_check(s_commits.count == 0u, "and is no longer elsewhere");
    line.at = 0ull;
    cli_check(!cli_git_cherry_pick(root, "--abort", &line, &said), "a word that is no commit is refused");
}

// a repository just made inside the tree, with no commit yet, takes the first commit of its own file named by its
// path in the tree
static void history_fresh(void)
{
    char root[CLI_PATH_LONGEST];
    char inner[CLI_PATH_LONGEST];
    char why[1024];
    char out[1024];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine line = {out, sizeof(out), 0ull};
    cli_check_folder("git_fresh", root);
    history_started(root, "main", "t", "a.txt", "one\n");
    cli_full(root, "held", inner);
    history_started(inner, "main", "t", NULL, NULL);
    history_written(inner, "held.py", "x = 1\n");
    const char *const paths[1] = {"held/held.py"};
    const int committed = cli_git_commit(root, "held", paths, 1u, &line, &said);
    (void)cli_line_closed(&said);
    cli_check(committed && history_git(inner, "log", "-1", "--format=%s", NULL) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = "held", .cap = CLI_CELLS_POOL}),
              committed ? "a fresh inner repository takes its own file's first commit" : why);
}

int main(void)
{
    history_graph();
    history_lines();
    history_repositories();
    history_fresh();
    history_stashing();
    cli_git_text_given(&s_touched.text);
    cli_git_text_given(&s_graph.text);
    cli_git_text_given(&s_lines.text);
    cli_git_text_given(&s_repos.text);
    cli_git_text_given(&s_changed.text);
    cli_git_text_given(&s_commits.text);
    cli_git_text_given(&s_stashes.text);
    cli_check((g_cli_git_cells.holds == 0ull) && (g_cli_git_cells.hw != 0ull),
              "each list's texts are kept in a cell grown as they are put, and given back");
    return cli_checked();
}
