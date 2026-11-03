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

// The files of the tree's tests as a run of them is gathered, each once: their paths, each ended by a 0, on a line over
// a cell of the most their bytes are, the last path put, and how many are put
typedef struct
{
    VerbaLinea line;
    char last[CLI_PATH_MAX_LEN];
    unsigned int count;
} CliTerminalFiles;
// the most files of tests one run of the tree's tests is given, and the most bytes of their paths
#define CLI_AT_TERMINAL_FILES 65536u
#define CLI_AT_TERMINAL_FILES_TEXT (1u << 22u)

int cli_at_terminal_runs(const CliAtTerminalCommandCfg *args)
{
    static const char *const s_commands[] = {
        "push",          "pull",          "branches",      "new-branch", "branch-switch", "branch-merge",
        "branch-rebase", "branch-rename", "branch-delete", "stash",      "stashes",       "stash-apply",
        "stash-pop",     "stash-drop",    "cherry-pick",   "run-tests",  "cover-tests",   "test-run",
        "test-cover",    "carry-send",    "carry-take",    "carry-undo"};
    for (unsigned int at = 0u; at < sizeof(s_commands) / sizeof(s_commands[0]); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_commands[at], .other = args->command, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    const char *command;
    const char *const *request;
    unsigned int count;
    const char *what;
    VerbaLinea *err_msg;
    char *home;
    const char *root;
    VerbaLinea *out;
    CliTerminalFiles *found;
    int cover;
} AtTerminalCtx;
// the first word, NULL with what the command takes given where none is given
EMBED_INLINE const char *at_terminal_by_name(const AtTerminalCtx *args)
{
    if ((args->count == 0u) || (args->request[0][0] == '\0'))
    {
        mmgr_line_put(args->err_msg, args->command);
        mmgr_line_put(args->err_msg, " takes ");
        mmgr_line_put(args->err_msg, args->what);
        return NULL;
    }
    return args->request[0];
}

// orior's own folder into `home`, which sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
EMBED_INLINE int at_terminal_home(const AtTerminalCtx *args)
{
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = args->home))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own");
        return 0;
    }
    return 1;
}

// every branch, the one the tree is on marked, each with its last commit's subject, a line each
EMBED_INLINE void at_terminal_branches(const AtTerminalCtx *args)
{
    const char *root = args->root;
    CliBranches *const found = g_cli_cells->temporary_buf_alloc(sizeof(*found), &g_cli_git_cells);
    if (found == NULL)
    {
        return;
    }
    EMBED_CALL(cli_git_root_found_branches.branches, CliGitRootFoundBranchesCfg, .root = root,
               .found_branches_pointer = found);
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const CliBranch *const one = &found->item[at];
        const char *const subject =
            EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &found->text, .at = one->subject);
        mmgr_line_put(args->out, (at > 0u) ? "\n" : "");
        mmgr_line_put(args->out, one->current ? "* " : "  ");
        mmgr_line_put(args->out, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &found->text,
                                            .at = one->name));
        if (subject[0] != '\0')
        {
            mmgr_line_put(args->out, "  ");
            mmgr_line_put(args->out, subject);
        }
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_git_cells);
}

// every stash with its line, a line each
EMBED_INLINE void at_terminal_stashes(const AtTerminalCtx *args)
{
    const char *root = args->root;
    CliStashes *const found = g_cli_cells->temporary_buf_alloc(sizeof(*found), &g_cli_git_cells);
    if (found == NULL)
    {
        return;
    }
    EMBED_CALL(cli_git_root_found_stashes.stashes, CliGitRootFoundStashesCfg, .root = root,
               .found_stashes_pointer = found);
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? "\n" : "");
        mmgr_line_put(args->out, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &found->text,
                                            .at = found->item[at].name));
        mmgr_line_put(args->out, "  ");
        mmgr_line_put(args->out, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &found->text,
                                            .at = found->item[at].subject));
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_git_cells);
}

// the commits of the other branches the branch open does not contain, each by its short id with its subject, a line
// each
EMBED_INLINE void at_terminal_elsewhere(const AtTerminalCtx *args)
{
    const char *root = args->root;
    CliCommits *const found = g_cli_cells->temporary_buf_alloc(sizeof(*found), &g_cli_git_cells);
    if (found == NULL)
    {
        return;
    }
    EMBED_CALL(cli_git_root_file_err_msg.elsewhere, CliGitRootFileErrMsgCfg, .root = root, .found = found);
    for (unsigned int at = 0u; at < found->count; at += 1u)
    {
        const char *const id = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &found->text,
                                          .at = found->item[at].id);
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL});
        mmgr_line_put(args->out, (at > 0u) ? "\n" : "");
        mmgr_line_put_n(args->out, id, (length < 7u) ? length : 7u);
        mmgr_line_put(args->out, "  ");
        mmgr_line_put(args->out, EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &found->text,
                                            .at = found->item[at].subject));
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &found->text);
    g_cli_cells->persistent_buf_release(found, &g_cli_git_cells);
}

// a branch deleted, with --force where it is given, a branch whose commits no other branch contains refused with how to
// delete it
EMBED_INLINE int at_terminal_branch_delete(const AtTerminalCtx *args)
{
    const char *root = args->root;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    int force = 0;
    const char *name = NULL;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const int flag =
            (cellul.eq(&(CatenaFinitaCfg){.src = args->request[at], .other = "--force", .cap = CLI_CELLS_POOL}));
        force |= flag;
        name = ((name == NULL) && !flag) ? args->request[at] : name;
    }
    name = (name != NULL) ? name : "";
    char why[4096];
    VerbaLinea refused = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_git_root_out.branch_act, CliGitRootOutCfg, .root = root,
                   .act = force ? "delete-unmerged" : "delete", .name_read = name, .to = "", .out = out,
                   .err_msg = &refused))
    {
        return 1;
    }
    unsigned long long at = 0ULL;
    if (EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = why, .length_value = refused.at,
                   .required = "not fully merged", .size = 16ULL, .at_pointer = &at))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " has commits no other branch contains: branch-delete ");
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " --force deletes it and them");
        return 0;
    }
    mmgr_line_put_n(err_msg, why, mmgr_line_ok(&refused) ? refused.at : 0ULL);
    return 0;
}

// a file of tests put among the files, after the last where it is not that one, up to the most files and their bytes
static void at_terminal_test_file(const CliTestEachCfg *args)
{
    CliTerminalFiles *const found = args->context;
    const size_t size = cellul.len(&(CatenaFinitaCfg){.src = args->test->file, .cap = CLI_CELLS_POOL}) + 1u;
    if (((found->count > 0u) &&
         (cellul.eq(&(CatenaFinitaCfg){.src = found->last, .other = args->test->file, .cap = CLI_CELLS_POOL}))) ||
        (found->count == CLI_AT_TERMINAL_FILES) || (found->line.at + size > CLI_AT_TERMINAL_FILES_TEXT))
    {
        return;
    }
    mmgr_line_put_n(&found->line, args->test->file, size);
    found->last[0] = '\0';
    if (size <= sizeof(found->last))
    {
        memor.cpy(&(MemoriaCfg){.dst = found->last, .src = args->test->file, .bytes = size});
    }
    found->count += 1u;
}

// the tree's tests run over the files `found` gathered: their paths listed in a cell of their count, given back once
// the run is done
EMBED_INLINE int at_terminal_files_run(const AtTerminalCtx *args)
{
    const char *root = args->root;
    int cover = args->cover;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const joined = args->found->line.out;
    const char **const files =
        mmgr_line_ok(&args->found->line)
            ? g_cli_cells->temporary_buf_alloc((args->found->count + 1ULL) * sizeof(const char *), &g_cli_git_cells)
            : NULL;
    if (files == NULL)
    {
        mmgr_line_put(err_msg, "the cells the tests' files are allocated in could not be taken");
        return 0;
    }
    unsigned long long at = 0ULL;
    for (unsigned int one = 0u; one < args->found->count; one += 1u)
    {
        files[one] = joined + at;
        at += cellul.len(&(CatenaFinitaCfg){.src = joined + at, .cap = CLI_CELLS_POOL}) + 1u;
    }
    const int good = EMBED_CALL(cli_at_terminal_root.tests, CliAtTerminalRootCfg, .root = root, .supplied = files,
                                .count = args->found->count, .cover = cover, .out = out, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(files, &g_cli_git_cells);
    return good;
}

int cli_at_terminal_run(const CliAtTerminalCommandCfg *args)
{
    const char *root = args->root;
    unsigned int count = args->count;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const first = (count > 0u) ? args->request[0] : "";
    static char s_words[4096];
    VerbaLinea words = {s_words, sizeof(s_words), 0ULL};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&words, (at > 0u) ? " " : "");
        mmgr_line_put(&words, args->request[at]);
    }
    (void)mmgr_line_finish(&words);
    const char *name = NULL;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "push", .cap = CLI_CELLS_POOL}))
    {
        return EMBED_CALL(cli_git_root_out_count_err_msg.push, CliGitRootOutCountErrMsgCfg, .root = root, .out = out,
                          .err_msg = err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "pull", .cap = CLI_CELLS_POOL}))
    {
        return EMBED_CALL(cli_git_root_out_count_err_msg.pull, CliGitRootOutCountErrMsgCfg, .root = root, .out = out,
                          .err_msg = err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "branches", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(at_terminal_branches, AtTerminalCtx, .root = root, .out = out);
        return 1;
    }
    static const char *const s_acts[4][2] = {
        {"new-branch", "create"}, {"branch-switch", "switch"}, {"branch-merge", "merge"}, {"branch-rebase", "rebase"}};
    for (unsigned int act = 0u; act < 4u; act += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = s_acts[act][0], .cap = CLI_CELLS_POOL}))
        {
            name = EMBED_CALL(at_terminal_by_name, AtTerminalCtx, .command = args->command, .request = args->request,
                              .count = count, .what = (act == 0u) ? "the new branch's name" : "a branch",
                              .err_msg = err_msg);
            return (name != NULL) &&
                   EMBED_CALL(cli_git_root_out.branch_act, CliGitRootOutCfg, .root = root, .act = s_acts[act][1],
                              .name_read = name, .to = "", .out = out, .err_msg = err_msg);
        }
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "branch-rename", .cap = CLI_CELLS_POOL}))
    {
        name = EMBED_CALL(at_terminal_by_name, AtTerminalCtx, .command = args->command, .request = args->request,
                          .count = count, .what = "a branch", .err_msg = err_msg);
        if ((name != NULL) && (count < 2u))
        {
            mmgr_line_put(err_msg, "branch-rename takes a branch and its new name");
            return 0;
        }
        return (name != NULL) &&
               EMBED_CALL(cli_git_root_out.branch_act, CliGitRootOutCfg, .root = root, .act = "rename",
                          .name_read = name, .to = args->request[1], .out = out, .err_msg = err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "branch-delete", .cap = CLI_CELLS_POOL}))
    {
        return EMBED_CALL(at_terminal_branch_delete, AtTerminalCtx, .root = root, .request = args->request,
                          .count = count, .out = out, .err_msg = err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "stash", .cap = CLI_CELLS_POOL}))
    {
        return EMBED_CALL(cli_git_root_out_message.stash_act, CliGitRootOutMessageCfg, .root = root, .act = "push",
                          .name_read = "", .message = s_words, .out = out, .err_msg = err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "stashes", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(at_terminal_stashes, AtTerminalCtx, .root = root, .out = out);
        return 1;
    }
    static const char *const s_stashes[3][2] = {{"stash-apply", "apply"}, {"stash-pop", "pop"}, {"stash-drop", "drop"}};
    for (unsigned int act = 0u; act < 3u; act += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = s_stashes[act][0], .cap = CLI_CELLS_POOL}))
        {
            name = EMBED_CALL(at_terminal_by_name, AtTerminalCtx, .command = args->command, .request = args->request,
                              .count = count, .what = "a stash", .err_msg = err_msg);
            return (name != NULL) && EMBED_CALL(cli_git_root_out_message.stash_act, CliGitRootOutMessageCfg,
                                                .root = root, .act = s_stashes[act][1], .name_read = name,
                                                .message = "", .out = out, .err_msg = err_msg);
        }
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "cherry-pick", .cap = CLI_CELLS_POOL}))
    {
        // with no commit named, the commits of the other branches the branch open does not contain, to name one of
        if (first[0] == '\0')
        {
            EMBED_CALL(at_terminal_elsewhere, AtTerminalCtx, .root = root, .out = out);
            return 1;
        }
        return EMBED_CALL(cli_git_root_file.cherry_pick, CliGitRootFileCfg, .root = root, .id = first, .out = out,
                          .err_msg = err_msg);
    }
    const int cover =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "cover-tests", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "test-cover", .cap = CLI_CELLS_POOL}));
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "run-tests", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "cover-tests", .cap = CLI_CELLS_POOL})))
    {
        static CliTerminalFiles s_found;
        const unsigned long long capacity = CLI_AT_TERMINAL_FILES_TEXT + 1ULL;
        char *const found_text = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_git_cells);
        s_found.line = mmgr_line_over(found_text, capacity);
        s_found.last[0] = '\0';
        s_found.count = 0u;
        EMBED_CALL(cli_testing_root.tests_find, CliTestingRootCfg, .root = root, .each = at_terminal_test_file,
                   .context = &s_found);
        const int good = EMBED_CALL(at_terminal_files_run, AtTerminalCtx, .root = root, .found = &s_found,
                                    .cover = cover, .out = out, .err_msg = err_msg);
        g_cli_cells->persistent_buf_release(found_text, &g_cli_git_cells);
        return good;
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "test-run", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "test-cover", .cap = CLI_CELLS_POOL})))
    {
        if (count == 0u)
        {
            mmgr_line_put(err_msg, args->command);
            mmgr_line_put(err_msg, " takes the tests or files of tests to run");
            return 0;
        }
        return EMBED_CALL(cli_at_terminal_root.tests, CliAtTerminalRootCfg, .root = root, .supplied = args->request,
                          .count = count, .cover = cover, .out = out, .err_msg = err_msg);
    }
    char home[CLI_PATH_MAX_LEN];
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "carry-send", .cap = CLI_CELLS_POOL}))
    {
        name = EMBED_CALL(at_terminal_by_name, AtTerminalCtx, .command = args->command, .request = args->request,
                          .count = count, .what = "the file or folder the settings go to", .err_msg = err_msg);
        return (name != NULL) && EMBED_CALL(at_terminal_home, AtTerminalCtx, .home = home, .err_msg = err_msg) &&
               EMBED_CALL(cli_machine_transfer_home_to.send, CliMachineTransferHomeToCfg, .home = home, .to = name,
                          .out = out, .err_msg = err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "carry-take", .cap = CLI_CELLS_POOL}))
    {
        name = EMBED_CALL(at_terminal_by_name, AtTerminalCtx, .command = args->command, .request = args->request,
                          .count = count, .what = "the file or folder the settings come from", .err_msg = err_msg);
        return (name != NULL) && EMBED_CALL(at_terminal_home, AtTerminalCtx, .home = home, .err_msg = err_msg) &&
               EMBED_CALL(cli_at_terminal_count.take, CliAtTerminalCountCfg, .home = home, .from = name,
                          .chosen = args->request + 1, .count = count - 1u, .out = out, .err_msg = err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "carry-undo", .cap = CLI_CELLS_POOL}))
    {
        name = EMBED_CALL(at_terminal_by_name, AtTerminalCtx, .command = args->command, .request = args->request,
                          .count = count, .what = "a taking", .err_msg = err_msg);
        return (name != NULL) && EMBED_CALL(at_terminal_home, AtTerminalCtx, .home = home, .err_msg = err_msg) &&
               EMBED_CALL(cli_at_terminal.undo, CliAtTerminalCfg, .home = home, .id = name, .out = out,
                          .err_msg = err_msg);
    }
    mmgr_line_put(err_msg, args->command);
    mmgr_line_put(err_msg, " runs in a window");
    return 0;
}
