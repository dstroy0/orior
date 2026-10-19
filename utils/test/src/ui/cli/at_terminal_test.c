// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// at_terminal_test.c: the terminal's commands' check: a window command runs here only where the terminal can do it;
// branches are made, listed, switched, renamed and deleted and changes stashed and brought back; tests run here and say
// each failure and the counts; settings sent from one machine are taken in on another and the taking undone; a window
// command given with no window open runs here where it can and opens a window where not; and a branch with commits of
// its own is deleted only when forced and its commit picked first
#include "../../../../../ui/cli/src/command_line/at_terminal.h"
#include "../../../../../ui/cli/src/command_line/command_line.h"
#include "../../../../../ui/cli/src/machine/carry.h"
#include "cli_check.h"

static char s_out[1u << 16u];
static char s_said[1u << 16u];

// git run in `root` with its words, ended by NULL, checked to have ended well
static void terminal_git(const char *root, const char *const *words)
{
    CliRan *const ran = cli_ran_taken();
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_ran(words, root, NULL, 0u, ran, &said) && (ran->code == 0), words[1]);
    cli_ran_given(ran);
}

// a repository `name` with one commit of a.txt, its path into `root`, which holds CLI_PATH_LONGEST
static void terminal_repo(const char *name, char *root)
{
    // the repository's own settings, whatever the machine's ask: a commit here is not signed
    static const char *const s_setup[6][7] = {{"git", "init", "-q", "-b", "main", NULL},
                                              {"git", "config", "user.name", "t", NULL},
                                              {"git", "config", "user.email", "t@t", NULL},
                                              {"git", "config", "commit.gpgsign", "false", NULL},
                                              {"git", "add", ".", NULL},
                                              {"git", "commit", "-q", "-m", "first", NULL}};
    char path[CLI_PATH_LONGEST];
    cli_check_folder(name, root);
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        terminal_git(root, s_setup[at]);
    }
    cli_full(root, "a.txt", path);
    (void)query_file_written(path, "one\n", 4ull);
    terminal_git(root, s_setup[4]);
    terminal_git(root, s_setup[5]);
}

// `command` with `count` words run on the tree at `root`: 1 with what it said in s_out, or 0 with why in s_said, each
// ended by a 0
static int terminal_run(const char *root, const char *command, const char *const *args, unsigned int count)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    CliLine said = {s_said, sizeof(s_said), 0ull};
    const int ran = cli_at_terminal_run(root, command, args, count, &out, &said);
    (void)cli_line_closed(&out);
    (void)cli_line_closed(&said);
    return ran;
}

// the text of the file `name` of the tree, the empty text where it has none, into `out`, which holds 64
static const char *terminal_file(const char *root, const char *name, char *out)
{
    char path[CLI_PATH_LONGEST];
    cli_full(root, name, path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)out, 63u);
    out[(size == QRY_MEMBER_UNREAD) ? 0u : size] = '\0';
    return out;
}

// a window command runs here only where the terminal can do it
static void terminal_refused(void)
{
    cli_check(cli_at_terminal_runs("push") && cli_at_terminal_runs("branch-switch") &&
                  cli_at_terminal_runs("stash-drop") && cli_at_terminal_runs("test-run") &&
                  cli_at_terminal_runs("carry-undo"),
              "git's, the tests' and the carried settings' commands run here");
    cli_check(!cli_at_terminal_runs("word-wrap") && !cli_at_terminal_runs("preferences") &&
                  !cli_at_terminal_runs("debug"),
              "and the window's own do not");
    char root[CLI_PATH_LONGEST];
    terminal_repo("at_terminal_refused", root);
    cli_check(!terminal_run(root, "word-wrap", NULL, 0u) &&
                  (cellul.eq(
                      &(CatenaFinitaCfg){.src = s_said, .other = "word-wrap runs in a window", .cap = CLI_CELLS_POOL})),
              "word-wrap runs in a window");
    cli_check(!terminal_run(root, "branch-switch", NULL, 0u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_said, .other = "branch-switch takes a branch", .cap = CLI_CELLS_POOL})),
              "branch-switch takes a branch");
}

// branches are made, listed, switched, renamed and deleted and changes stashed and brought back
static void terminal_branches(void)
{
    static const char *const s_made[1] = {"made"};
    static const char *const s_main[1] = {"main"};
    static const char *const s_rename[2] = {"made", "named"};
    static const char *const s_named[1] = {"named"};
    static const char *const s_put[2] = {"put", "aside"};
    static const char *const s_stash[1] = {"stash@{0}"};
    char root[CLI_PATH_LONGEST];
    char text[64];
    char path[CLI_PATH_LONGEST];
    terminal_repo("at_terminal_git", root);
    (void)terminal_run(root, "new-branch", s_made, 1u);
    cli_check(
        terminal_run(root, "branches", NULL, 0u) &&
            ((cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "* made", .cap = CLI_CELLS_POOL})) ||
             (cellul.find(&(CatenaFinitaCfg){
                  .src = s_out, .cap = CLI_CELLS_POOL, .other = "\n* made", .other_cap = CLI_CELLS_POOL}) != NULL)),
        "a branch made is the one the tree is on");
    (void)terminal_run(root, "branch-switch", s_main, 1u);
    (void)terminal_run(root, "branch-rename", s_rename, 2u);
    cli_check(terminal_run(root, "branches", NULL, 0u) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_out, .cap = CLI_CELLS_POOL, .other = "named", .other_cap = CLI_CELLS_POOL}) != NULL),
              "a branch is renamed");
    (void)terminal_run(root, "branch-delete", s_named, 1u);
    cli_check(terminal_run(root, "branches", NULL, 0u) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_out, .cap = CLI_CELLS_POOL, .other = "named", .other_cap = CLI_CELLS_POOL}) == NULL),
              "and deleted");
    cli_full(root, "a.txt", path);
    (void)query_file_written(path, "two\n", 4ull);
    (void)terminal_run(root, "stash", s_put, 2u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = terminal_file(root, "a.txt", text), .other = "one\n", .cap = CLI_CELLS_POOL}),
              "a change is stashed");
    cli_check(terminal_run(root, "stashes", NULL, 0u) &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "stash@{0}", .cap = CLI_CELLS_POOL})),
              "and listed");
    (void)terminal_run(root, "stash-pop", s_stash, 1u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = terminal_file(root, "a.txt", text), .other = "two\n", .cap = CLI_CELLS_POOL}),
              "and brought back");
    cli_check(terminal_run(root, "stashes", NULL, 0u) && (s_out[0] == '\0'), "and gone from the stashes");
}

// tests run here and say each failure and the counts
static void terminal_tests(void)
{
    static const char *const s_here[1] = {"test_here.py"};
    static const char *const s_good[1] = {"test_good.py"};
    static const char *const s_none[1] = {"test_none.py"};
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    terminal_repo("at_terminal_tests", root);
    // cases of unittest's, which pytest and Python's own runner each take, whichever the machine has
    static const char s_test_here[] =
        "import unittest\n\n\nclass Here(unittest.TestCase):\n    def test_passes(self):\n"
        "        self.assertTrue(True)\n\n    def test_fails(self):\n"
        "        self.assertEqual(1, 2)\n";
    static const char s_test_good[] =
        "import unittest\n\n\nclass Good(unittest.TestCase):\n    def test_passes(self):\n"
        "        self.assertTrue(True)\n";
    cli_full(root, "test_here.py", path);
    (void)query_file_written(path, s_test_here, sizeof(s_test_here) - 1u);
    cli_full(root, "test_good.py", path);
    (void)query_file_written(path, s_test_good, sizeof(s_test_good) - 1u);
    cli_check(
        !terminal_run(root, "test-run", s_here, 1u) &&
            ((cellul.starts(&(CatenaFinitaCfg){.src = s_said, .other = "FAILED ", .cap = CLI_CELLS_POOL})) ||
             (cellul.find(&(CatenaFinitaCfg){
                  .src = s_said, .cap = CLI_CELLS_POOL, .other = "\nFAILED ", .other_cap = CLI_CELLS_POOL}) != NULL)) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_said, .cap = CLI_CELLS_POOL, .other = "test_fails", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_said, .cap = CLI_CELLS_POOL, .other = "1 passed, 1 failed", .other_cap = CLI_CELLS_POOL}) !=
             NULL),
        "a run says each failure and the counts");
    cli_check(
        terminal_run(root, "test-run", s_good, 1u) &&
            (cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "1 passed, 0 failed", .cap = CLI_CELLS_POOL})),
        "and a run with none failed says its counts");
    cli_full(root, "test_none.py", path);
    (void)query_file_written(path, "x = 1\n", 6ull);
    cli_check(
        !terminal_run(root, "test-run", s_none, 1u) &&
            (cellul.starts(&(CatenaFinitaCfg){.src = s_said, .other = "0 passed, 0 failed", .cap = CLI_CELLS_POOL})),
        "a run that ran no test fails");
}

// settings sent from one machine are taken in on another and the taking undone
static void terminal_carry(void)
{
    char base[CLI_PATH_LONGEST];
    char here[CLI_PATH_LONGEST];
    char there[CLI_PATH_LONGEST];
    char between[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    char text[64];
    cli_check_folder("at_terminal_carry", base);
    cli_full(base, "here", here);
    cli_full(base, "there", there);
    cli_full(base, "between", between);
    (void)query_folder_made(here);
    (void)query_folder_made(there);
    (void)query_folder_made(between);
    cli_full(here, "user.css", path);
    (void)query_file_written(path, "body { color: red; }\n", 21ull);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    CliLine said = {s_said, sizeof(s_said), 0ull};
    (void)cli_carry_send(here, between, &out, &said);
    out.at = 0ull;
    said.at = 0ull;
    cli_check(cli_at_terminal_take(there, between, NULL, 0u, &out, &said) && (cli_line_closed(&out) > 0ull) &&
                  (cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "Took 1 file", .cap = CLI_CELLS_POOL})),
              "settings sent are taken in");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = terminal_file(there, "user.css", text),
                                           .other = "body { color: red; }\n",
                                           .cap = CLI_CELLS_POOL}),
              "as they were sent");
    char id[128] = "";
    const char *const undo = cellul.find(
        &(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL, .other = "carry-undo ", .other_cap = CLI_CELLS_POOL});
    for (unsigned int at = 0u; (undo != NULL) && (at + 1u < sizeof(id)) && (undo[11u + at] > ' '); at += 1u)
    {
        id[at] = undo[11u + at];
        id[at + 1u] = '\0';
    }
    out.at = 0ull;
    char wanted[CLI_PATH_LONGEST + 64u];
    CliLine line = {wanted, sizeof(wanted), 0ull};
    cli_line_text(&line, between);
    cli_line_text(&line, " holds the settings this machine has.");
    (void)cli_line_closed(&line);
    cli_check(cli_at_terminal_take(there, between, NULL, 0u, &out, &said) && (cli_line_closed(&out) > 0ull) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_out, .other = wanted, .cap = CLI_CELLS_POOL})),
              "and taken again change nothing");
    out.at = 0ull;
    (void)cli_at_terminal_undo(there, id, &out, &said);
    cli_full(there, "user.css", path);
    cli_check(!query_is_file(path), "the taking undone");
}

// a window command given with no window open runs here where it can and opens a window where not
static void terminal_command_line(void)
{
    static CliOutcome s_outcome;
    char root[CLI_PATH_LONGEST];
    terminal_repo("at_terminal_command_line", root);
    const char *const branch[5] = {"--root", root, "git", "new-branch", "from-the-terminal"};
    cli_command_line_run(branch, 5u, &s_outcome);
    cli_check(!s_outcome.window && (s_outcome.code == 0), "a git command runs here");
    cli_check(
        terminal_run(root, "branches", NULL, 0u) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = s_out, .cap = CLI_CELLS_POOL, .other = "* from-the-terminal", .other_cap = CLI_CELLS_POOL}) !=
             NULL),
        "and makes its branch");
    const char *const wrap[5] = {"--root", root, "view", "word-wrap", "on"};
    cli_command_line_run(wrap, 5u, &s_outcome);
    cli_check(s_outcome.window, "the window's own command opens a window");
}

// a branch with commits of its own is deleted only when forced and its commit picked first
static void terminal_unmerged(void)
{
    static const char *const s_side[1] = {"side"};
    static const char *const s_main[1] = {"main"};
    static const char *const s_forced[2] = {"side", "--force"};
    static const char *const s_add[4] = {"git", "add", ".", NULL};
    static const char *const s_commit[6] = {"git", "commit", "-q", "-m", "on the side", NULL};
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    terminal_repo("at_terminal_unmerged", root);
    (void)terminal_run(root, "new-branch", s_side, 1u);
    cli_full(root, "b.txt", path);
    (void)query_file_written(path, "side\n", 5ull);
    terminal_git(root, s_add);
    terminal_git(root, s_commit);
    (void)terminal_run(root, "branch-switch", s_main, 1u);
    const unsigned long long listed_size = terminal_run(root, "cherry-pick", NULL, 0u)
                                               ? cellul.len(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL})
                                               : 0ull;
    cli_check((listed_size >= 11ull) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = s_out + listed_size - 11u, .other = "on the side", .cap = CLI_CELLS_POOL})),
              "the side's commit is listed to pick");
    char commit[64] = "";
    for (unsigned int at = 0u; (at + 1u < sizeof(commit)) && (s_out[at] > ' '); at += 1u)
    {
        commit[at] = s_out[at];
        commit[at + 1u] = '\0';
    }
    cli_check(!terminal_run(root, "branch-delete", s_side, 1u) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_said, .cap = CLI_CELLS_POOL, .other = "--force", .other_cap = CLI_CELLS_POOL}) != NULL),
              "a branch with commits of its own is deleted only when forced");
    const char *const pick[1] = {commit};
    (void)terminal_run(root, "cherry-pick", pick, 1u);
    cli_full(root, "b.txt", path);
    cli_check(query_is_file(path), "the commit picked onto main");
    (void)terminal_run(root, "branch-delete", s_forced, 2u);
    (void)terminal_run(root, "branches", NULL, 0u);
    cli_check((cellul.find(&(CatenaFinitaCfg){
                   .src = s_out, .cap = CLI_CELLS_POOL, .other = "  side\n", .other_cap = CLI_CELLS_POOL}) == NULL) &&
                  (!cellul.starts(&(CatenaFinitaCfg){.src = s_out, .other = "  side", .cap = CLI_CELLS_POOL})) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_out, .cap = CLI_CELLS_POOL, .other = "* side", .other_cap = CLI_CELLS_POOL}) == NULL),
              "side deleted");
}

int main(void)
{
    terminal_refused();
    terminal_branches();
    terminal_tests();
    terminal_carry();
    terminal_command_line();
    terminal_unmerged();
    return cli_checked();
}
