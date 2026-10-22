// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// at_terminal.c: the window's menu commands run at the terminal: git's pushes and pulls; the branches listed, made,
// switched to, merged, rebased onto, renamed and deleted, a branch whose commits no other branch holds deleted only
// with --force, as the window asks first; the stashes made, listed, applied, popped and dropped; the commits of the
// other branches the branch open does not hold listed, and one picked; the tree's tests run; and the settings sent,
// taken in and the taking undone
#include "at_terminal.h"
#include "../code/git.h"
#include "../machine/carry.h"
#include "../tests/testing.h"
#include "../text/pieces.h"
#include "../text/text.h"

// the most files of tests one run of the tree's tests is given, and the most bytes of their paths
#define CLI_AT_TERMINAL_FILES 65536u
#define CLI_AT_TERMINAL_FILES_TEXT (1u << 22u)

int cli_at_terminal_runs(const char *command)
{
    static const char *const s_commands[] = {
        "push",          "pull",          "branches",      "new-branch", "branch-switch", "branch-merge",
        "branch-rebase", "branch-rename", "branch-delete", "stash",      "stashes",       "stash-apply",
        "stash-pop",     "stash-drop",    "cherry-pick",   "run-tests",  "cover-tests",   "test-run",
        "test-cover",    "carry-send",    "carry-take",    "carry-undo"};
    for (unsigned int at = 0u; at < sizeof(s_commands) / sizeof(s_commands[0]); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_commands[at], .other = command, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

// the first word, NULL with what the command takes said where none is given
static const char *at_terminal_named(const char *command, const char *const *args, unsigned int count, const char *what,
                                     CliLine *said)
{
    if ((count == 0u) || (args[0][0] == '\0'))
    {
        cli_line_text(said, command);
        cli_line_text(said, " takes ");
        cli_line_text(said, what);
        return NULL;
    }
    return args[0];
}

// orior's own folder into `home`, which holds CLI_PATH_LONGEST: 1, or 0 with why said
static int at_terminal_home(char *home, CliLine *said)
{
    if (!cli_home_folder(home))
    {
        cli_line_text(said, "orior_cli has no folder of its own");
        return 0;
    }
    return 1;
}

// every branch, the one the tree is on marked, each with its last commit's subject, a line each
static void at_terminal_branches(const char *root, CliLine *out)
{
    CliBranches *const found = cli_cell_taken_by(&g_cli_git_cells, sizeof(*found), "a tree's branches");
    if (found == NULL)
    {
        return;
    }
    cli_git_branches(root, found);
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliBranch *const one = &found->item[at];
        const char *const subject = cli_git_text_of(&found->text, one->subject);
        cli_line_text(out, (at > 0u) ? "\n" : "");
        cli_line_text(out, one->current ? "* " : "  ");
        cli_line_text(out, cli_git_text_of(&found->text, one->name));
        if (subject[0] != '\0')
        {
            cli_line_text(out, "  ");
            cli_line_text(out, subject);
        }
    }
    cli_git_text_given(&found->text);
    cli_cell_given_by(&g_cli_git_cells, found);
}

// every stash with its line, a line each
static void at_terminal_stashes(const char *root, CliLine *out)
{
    CliStashes *const found = cli_cell_taken_by(&g_cli_git_cells, sizeof(*found), "a tree's stashes");
    if (found == NULL)
    {
        return;
    }
    cli_git_stashes(root, found);
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "\n" : "");
        cli_line_text(out, cli_git_text_of(&found->text, found->item[at].name));
        cli_line_text(out, "  ");
        cli_line_text(out, cli_git_text_of(&found->text, found->item[at].subject));
    }
    cli_git_text_given(&found->text);
    cli_cell_given_by(&g_cli_git_cells, found);
}

// the commits of the other branches the branch open does not hold, each by its short id with its subject, a line each
static void at_terminal_elsewhere(const char *root, CliLine *out)
{
    CliCommits *const found = cli_cell_taken_by(&g_cli_git_cells, sizeof(*found), "a tree's commits elsewhere");
    if (found == NULL)
    {
        return;
    }
    cli_git_elsewhere(root, found);
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const char *const id = cli_git_text_of(&found->text, found->item[at].id);
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL});
        cli_line_text(out, (at > 0u) ? "\n" : "");
        cli_line_counted(out, id, (length < 7u) ? length : 7u);
        cli_line_text(out, "  ");
        cli_line_text(out, cli_git_text_of(&found->text, found->item[at].subject));
    }
    cli_git_text_given(&found->text);
    cli_cell_given_by(&g_cli_git_cells, found);
}

// a branch deleted, with --force where it is given, a branch whose commits no other branch holds refused with how to
// delete it
static int at_terminal_branch_delete(const char *root, const char *const *args, unsigned int count, CliLine *out,
                                     CliLine *said)
{
    int force = 0;
    const char *name = NULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const int flag = (cellul.eq(&(CatenaFinitaCfg){.src = args[at], .other = "--force", .cap = CLI_CELLS_POOL}));
        force |= flag;
        name = ((name == NULL) && !flag) ? args[at] : name;
    }
    name = (name != NULL) ? name : "";
    char why[4096];
    CliLine refused = {why, sizeof(why), 0ull};
    if (cli_git_branch_act(root, force ? "delete-unmerged" : "delete", name, "", out, &refused))
    {
        return 1;
    }
    unsigned long long at = 0ull;
    if (cli_text_find(why, refused.at, "not fully merged", 16ull, &at))
    {
        cli_line_text(said, name);
        cli_line_text(said, " has commits no other branch holds: branch-delete ");
        cli_line_text(said, name);
        cli_line_text(said, " --force deletes it and them");
        return 0;
    }
    cli_line_counted(said, why, cli_line_fits(&refused) ? refused.at : 0ull);
    return 0;
}

// The files of the tree's tests as a run of them is gathered, each once: their paths, each ended by a 0, written
// through pieces, the bytes a piece is held in before it goes to a cell, the last path put, and how many are put
typedef struct
{
    CliPieces pieces;
    CliLine line;
    char piece[4096];
    char last[CLI_PATH_LONGEST];
    unsigned int count;
} CliTerminalFiles;

// a file of tests put among the files, after the last where it is not that one, up to the most files and their bytes
static void at_terminal_test_file(void *context, const CliTest *test)
{
    CliTerminalFiles *const found = context;
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = test->file, .cap = CLI_CELLS_POOL}) + 1u;
    if (((found->count > 0u) &&
         (cellul.eq(&(CatenaFinitaCfg){.src = found->last, .other = test->file, .cap = CLI_CELLS_POOL}))) ||
        (found->count == CLI_AT_TERMINAL_FILES) ||
        (found->pieces.size + found->line.at + size > CLI_AT_TERMINAL_FILES_TEXT))
    {
        return;
    }
    cli_line_counted(&found->line, test->file, size);
    found->last[0] = '\0';
    if (size <= sizeof(found->last))
    {
        memor.cpy(&(MemoriaCfg){.dst = found->last, .src = test->file, .bytes = size});
    }
    found->count += 1u;
}

// the tree's tests run over the files `found` gathered: their paths joined into a cell and listed in another, both of
// their size and given back once the run is done
static int at_terminal_files_run(const char *root, CliTerminalFiles *found, int cover, CliLine *out, CliLine *said)
{
    unsigned long long size = 0ull;
    char *const joined = cli_pieces_joined(&found->pieces, &found->line, "the files of the tree's tests", &size);
    const char **const files =
        (joined != NULL) ? cli_cell_taken((found->count + 1ull) * sizeof(const char *), "the tests' files listed")
                         : NULL;
    if (files == NULL)
    {
        cli_cell_given(joined);
        cli_line_text(said, "the cells the tests' files are held in could not be taken");
        return 0;
    }
    unsigned long long at = 0ull;
    for (unsigned int one = 0u; one < found->count; one += 1u)
    {
        files[one] = joined + at;
        at += cellul.len(&(CatenaFinitaCfg){.src = joined + at, .cap = CLI_CELLS_POOL}) + 1u;
    }
    const int good = cli_at_terminal_tests(root, files, found->count, cover, out, said);
    cli_cell_given(files);
    cli_cell_given(joined);
    return good;
}

int cli_at_terminal_run(const char *root, const char *command, const char *const *args, unsigned int count,
                        CliLine *out, CliLine *said)
{
    const char *const first = (count > 0u) ? args[0] : "";
    static char s_words[4096];
    CliLine words = {s_words, sizeof(s_words), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&words, (at > 0u) ? " " : "");
        cli_line_text(&words, args[at]);
    }
    (void)cli_line_closed(&words);
    const char *name = NULL;
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "push", .cap = CLI_CELLS_POOL}))
    {
        return cli_git_push(root, out, said);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "pull", .cap = CLI_CELLS_POOL}))
    {
        return cli_git_pull(root, out, said);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "branches", .cap = CLI_CELLS_POOL}))
    {
        at_terminal_branches(root, out);
        return 1;
    }
    static const char *const s_acts[4][2] = {
        {"new-branch", "create"}, {"branch-switch", "switch"}, {"branch-merge", "merge"}, {"branch-rebase", "rebase"}};
    for (unsigned int act = 0u; act < 4u; act += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = s_acts[act][0], .cap = CLI_CELLS_POOL}))
        {
            name = at_terminal_named(command, args, count, (act == 0u) ? "the new branch's name" : "a branch", said);
            return (name != NULL) && cli_git_branch_act(root, s_acts[act][1], name, "", out, said);
        }
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "branch-rename", .cap = CLI_CELLS_POOL}))
    {
        name = at_terminal_named(command, args, count, "a branch", said);
        if ((name != NULL) && (count < 2u))
        {
            cli_line_text(said, "branch-rename takes a branch and its new name");
            return 0;
        }
        return (name != NULL) && cli_git_branch_act(root, "rename", name, args[1], out, said);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "branch-delete", .cap = CLI_CELLS_POOL}))
    {
        return at_terminal_branch_delete(root, args, count, out, said);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "stash", .cap = CLI_CELLS_POOL}))
    {
        return cli_git_stash_act(root, "push", "", s_words, out, said);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "stashes", .cap = CLI_CELLS_POOL}))
    {
        at_terminal_stashes(root, out);
        return 1;
    }
    static const char *const s_stashes[3][2] = {{"stash-apply", "apply"}, {"stash-pop", "pop"}, {"stash-drop", "drop"}};
    for (unsigned int act = 0u; act < 3u; act += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = s_stashes[act][0], .cap = CLI_CELLS_POOL}))
        {
            name = at_terminal_named(command, args, count, "a stash", said);
            return (name != NULL) && cli_git_stash_act(root, s_stashes[act][1], name, "", out, said);
        }
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "cherry-pick", .cap = CLI_CELLS_POOL}))
    {
        // with no commit named, the commits of the other branches the branch open does not hold, to name one of
        if (first[0] == '\0')
        {
            at_terminal_elsewhere(root, out);
            return 1;
        }
        return cli_git_cherry_pick(root, first, out, said);
    }
    const int cover = (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "cover-tests", .cap = CLI_CELLS_POOL})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "test-cover", .cap = CLI_CELLS_POOL}));
    if ((cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "run-tests", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "cover-tests", .cap = CLI_CELLS_POOL})))
    {
        static CliTerminalFiles s_found;
        static CliLock s_found_lock = CLI_LOCK_INIT;
        cli_lock_take(&s_found_lock);
        cli_pieces_begun(&s_found.pieces, &s_found.line, s_found.piece, sizeof(s_found.piece));
        s_found.last[0] = '\0';
        s_found.count = 0u;
        cli_tests_found(root, at_terminal_test_file, &s_found);
        const int good = at_terminal_files_run(root, &s_found, cover, out, said);
        cli_lock_give(&s_found_lock);
        return good;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "test-run", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "test-cover", .cap = CLI_CELLS_POOL})))
    {
        if (count == 0u)
        {
            cli_line_text(said, command);
            cli_line_text(said, " takes the tests or files of tests to run");
            return 0;
        }
        return cli_at_terminal_tests(root, args, count, cover, out, said);
    }
    char home[CLI_PATH_LONGEST];
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "carry-send", .cap = CLI_CELLS_POOL}))
    {
        name = at_terminal_named(command, args, count, "the file or folder the settings go to", said);
        return (name != NULL) && at_terminal_home(home, said) && cli_carry_send(home, name, out, said);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "carry-take", .cap = CLI_CELLS_POOL}))
    {
        name = at_terminal_named(command, args, count, "the file or folder the settings come from", said);
        return (name != NULL) && at_terminal_home(home, said) &&
               cli_at_terminal_take(home, name, args + 1, count - 1u, out, said);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "carry-undo", .cap = CLI_CELLS_POOL}))
    {
        name = at_terminal_named(command, args, count, "a taking", said);
        return (name != NULL) && at_terminal_home(home, said) && cli_at_terminal_undo(home, name, out, said);
    }
    cli_line_text(said, command);
    cli_line_text(said, " runs in a window");
    return 0;
}
