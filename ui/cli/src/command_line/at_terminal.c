// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// at_terminal.c: the window's menu commands run at the terminal: git's pushes and pulls; the branches listed, made,
// switched to, merged, rebased onto, renamed and deleted, a branch whose commits no other branch contains deleted only
// with --force, as the window confirms first; the stashes made, listed, applied, popped and dropped; the commits of the
// other branches the branch open does not contain listed, and one picked; the tree's tests run; and the settings sent,
// taken in and the taking undone
#include "at_terminal.h"
#include "../code/git.h"
#include "../machine/carry.h"
#include "../tests/testing.h"
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

// the first word, NULL with what the command takes given where none is given
static const char *at_terminal_named(const char *command, const char *const *args, unsigned int count, const char *what,
                                     VerbaLinea *err_msg)
{
    if ((count == 0u) || (args[0][0] == '\0'))
    {
        mmgr_line_put(err_msg, command);
        mmgr_line_put(err_msg, " takes ");
        mmgr_line_put(err_msg, what);
        return NULL;
    }
    return args[0];
}

// orior's own folder into `home`, which sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
static int at_terminal_home(char *home, VerbaLinea *err_msg)
{
    if (!cli_home_folder(home))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own");
        return 0;
    }
    return 1;
}

// every branch, the one the tree is on marked, each with its last commit's subject, a line each
static void at_terminal_branches(const char *root, VerbaLinea *out)
{
    CliBranches *const found = g_cli_cells->temporary_buf_alloc(sizeof(*found), &g_cli_git_cells);
    if (found == NULL)
    {
        return;
    }
    cli_git_branches(root, found);
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliBranch *const one = &found->item[at];
        const char *const subject = cli_git_text_of(&found->text, one->subject);
        mmgr_line_put(out, (at > 0u) ? "\n" : "");
        mmgr_line_put(out, one->current ? "* " : "  ");
        mmgr_line_put(out, cli_git_text_of(&found->text, one->name));
        if (subject[0] != '\0')
        {
            mmgr_line_put(out, "  ");
            mmgr_line_put(out, subject);
        }
    }
    cli_git_text_given(&found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_git_cells);
}

// every stash with its line, a line each
static void at_terminal_stashes(const char *root, VerbaLinea *out)
{
    CliStashes *const found = g_cli_cells->temporary_buf_alloc(sizeof(*found), &g_cli_git_cells);
    if (found == NULL)
    {
        return;
    }
    cli_git_stashes(root, found);
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "\n" : "");
        mmgr_line_put(out, cli_git_text_of(&found->text, found->item[at].name));
        mmgr_line_put(out, "  ");
        mmgr_line_put(out, cli_git_text_of(&found->text, found->item[at].subject));
    }
    cli_git_text_given(&found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_git_cells);
}

// the commits of the other branches the branch open does not contain, each by its short id with its subject, a line
// each
static void at_terminal_elsewhere(const char *root, VerbaLinea *out)
{
    CliCommits *const found = g_cli_cells->temporary_buf_alloc(sizeof(*found), &g_cli_git_cells);
    if (found == NULL)
    {
        return;
    }
    cli_git_elsewhere(root, found);
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const char *const id = cli_git_text_of(&found->text, found->item[at].id);
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL});
        mmgr_line_put(out, (at > 0u) ? "\n" : "");
        mmgr_line_put_n(out, id, (length < 7u) ? length : 7u);
        mmgr_line_put(out, "  ");
        mmgr_line_put(out, cli_git_text_of(&found->text, found->item[at].subject));
    }
    cli_git_text_given(&found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_git_cells);
}

// a branch deleted, with --force where it is given, a branch whose commits no other branch contains refused with how to
// delete it
static int at_terminal_branch_delete(const char *root, const char *const *args, unsigned int count, VerbaLinea *out,
                                     VerbaLinea *err_msg)
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
    VerbaLinea refused = {why, sizeof(why), 0ull};
    if (cli_git_branch_act(root, force ? "delete-unmerged" : "delete", name, "", out, &refused))
    {
        return 1;
    }
    unsigned long long at = 0ull;
    if (cli_text_find(why, refused.at, "not fully merged", 16ull, &at))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " has commits no other branch contains: branch-delete ");
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " --force deletes it and them");
        return 0;
    }
    mmgr_line_put_n(err_msg, why, mmgr_line_ok(&refused) ? refused.at : 0ull);
    return 0;
}

// The files of the tree's tests as a run of them is gathered, each once: their paths, each ended by a 0, on a line over
// a cell of the most their bytes are, the last path put, and how many are put
typedef struct
{
    VerbaLinea line;
    char last[CLI_PATH_MAX_LEN];
    unsigned int count;
} CliTerminalFiles;

// a file of tests put among the files, after the last where it is not that one, up to the most files and their bytes
static void at_terminal_test_file(void *context, const CliTest *test)
{
    CliTerminalFiles *const found = context;
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = test->file, .cap = CLI_CELLS_POOL}) + 1u;
    if (((found->count > 0u) &&
         (cellul.eq(&(CatenaFinitaCfg){.src = found->last, .other = test->file, .cap = CLI_CELLS_POOL}))) ||
        (found->count == CLI_AT_TERMINAL_FILES) || (found->line.at + size > CLI_AT_TERMINAL_FILES_TEXT))
    {
        return;
    }
    mmgr_line_put_n(&found->line, test->file, size);
    found->last[0] = '\0';
    if (size <= sizeof(found->last))
    {
        memor.cpy(&(MemoriaCfg){.dst = found->last, .src = test->file, .bytes = size});
    }
    found->count += 1u;
}

// the tree's tests run over the files `found` gathered: their paths listed in a cell of their count, given back once
// the run is done
static int at_terminal_files_run(const char *root, CliTerminalFiles *found, int cover, VerbaLinea *out,
                                 VerbaLinea *err_msg)
{
    const char *const joined = found->line.out;
    const char **const files =
        mmgr_line_ok(&found->line)
            ? g_cli_cells->temporary_buf_alloc((found->count + 1ull) * sizeof(const char *), &g_cli_git_cells)
            : NULL;
    if (files == NULL)
    {
        mmgr_line_put(err_msg, "the cells the tests' files are allocated in could not be taken");
        return 0;
    }
    unsigned long long at = 0ull;
    for (unsigned int one = 0u; one < found->count; one += 1u)
    {
        files[one] = joined + at;
        at += cellul.len(&(CatenaFinitaCfg){.src = joined + at, .cap = CLI_CELLS_POOL}) + 1u;
    }
    const int good = cli_at_terminal_tests(root, files, found->count, cover, out, err_msg);
    g_cli_cells->persistent_buf_release(files, &g_cli_git_cells);
    return good;
}

int cli_at_terminal_run(const char *root, const char *command, const char *const *args, unsigned int count,
                        VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const first = (count > 0u) ? args[0] : "";
    static char s_words[4096];
    VerbaLinea words = {s_words, sizeof(s_words), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&words, (at > 0u) ? " " : "");
        mmgr_line_put(&words, args[at]);
    }
    (void)mmgr_line_finish(&words);
    const char *name = NULL;
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "push", .cap = CLI_CELLS_POOL}))
    {
        return cli_git_push(root, out, err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "pull", .cap = CLI_CELLS_POOL}))
    {
        return cli_git_pull(root, out, err_msg);
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
            name = at_terminal_named(command, args, count, (act == 0u) ? "the new branch's name" : "a branch", err_msg);
            return (name != NULL) && cli_git_branch_act(root, s_acts[act][1], name, "", out, err_msg);
        }
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "branch-rename", .cap = CLI_CELLS_POOL}))
    {
        name = at_terminal_named(command, args, count, "a branch", err_msg);
        if ((name != NULL) && (count < 2u))
        {
            mmgr_line_put(err_msg, "branch-rename takes a branch and its new name");
            return 0;
        }
        return (name != NULL) && cli_git_branch_act(root, "rename", name, args[1], out, err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "branch-delete", .cap = CLI_CELLS_POOL}))
    {
        return at_terminal_branch_delete(root, args, count, out, err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "stash", .cap = CLI_CELLS_POOL}))
    {
        return cli_git_stash_act(root, "push", "", s_words, out, err_msg);
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
            name = at_terminal_named(command, args, count, "a stash", err_msg);
            return (name != NULL) && cli_git_stash_act(root, s_stashes[act][1], name, "", out, err_msg);
        }
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "cherry-pick", .cap = CLI_CELLS_POOL}))
    {
        // with no commit named, the commits of the other branches the branch open does not contain, to name one of
        if (first[0] == '\0')
        {
            at_terminal_elsewhere(root, out);
            return 1;
        }
        return cli_git_cherry_pick(root, first, out, err_msg);
    }
    const int cover = (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "cover-tests", .cap = CLI_CELLS_POOL})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "test-cover", .cap = CLI_CELLS_POOL}));
    if ((cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "run-tests", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "cover-tests", .cap = CLI_CELLS_POOL})))
    {
        static CliTerminalFiles s_found;
        static CliLock s_found_lock = CLI_LOCK_INIT;
        cli_lock_take(&s_found_lock);
        const unsigned long long room = CLI_AT_TERMINAL_FILES_TEXT + 1ull;
        char *const found_text = g_cli_cells->temporary_buf_alloc(room, &g_cli_git_cells);
        s_found.line = mmgr_line_over(found_text, room);
        s_found.last[0] = '\0';
        s_found.count = 0u;
        cli_tests_found(root, at_terminal_test_file, &s_found);
        const int good = at_terminal_files_run(root, &s_found, cover, out, err_msg);
        g_cli_cells->persistent_buf_release(found_text, &g_cli_git_cells);
        cli_lock_give(&s_found_lock);
        return good;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "test-run", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "test-cover", .cap = CLI_CELLS_POOL})))
    {
        if (count == 0u)
        {
            mmgr_line_put(err_msg, command);
            mmgr_line_put(err_msg, " takes the tests or files of tests to run");
            return 0;
        }
        return cli_at_terminal_tests(root, args, count, cover, out, err_msg);
    }
    char home[CLI_PATH_MAX_LEN];
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "carry-send", .cap = CLI_CELLS_POOL}))
    {
        name = at_terminal_named(command, args, count, "the file or folder the settings go to", err_msg);
        return (name != NULL) && at_terminal_home(home, err_msg) && cli_carry_send(home, name, out, err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "carry-take", .cap = CLI_CELLS_POOL}))
    {
        name = at_terminal_named(command, args, count, "the file or folder the settings come from", err_msg);
        return (name != NULL) && at_terminal_home(home, err_msg) &&
               cli_at_terminal_take(home, name, args + 1, count - 1u, out, err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "carry-undo", .cap = CLI_CELLS_POOL}))
    {
        name = at_terminal_named(command, args, count, "a taking", err_msg);
        return (name != NULL) && at_terminal_home(home, err_msg) && cli_at_terminal_undo(home, name, out, err_msg);
    }
    mmgr_line_put(err_msg, command);
    mmgr_line_put(err_msg, " runs in a window");
    return 0;
}
