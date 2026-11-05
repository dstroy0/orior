// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_changes.c: the changes as the window reads them, and the next commit. Each read lists the tree's changed files
// through the repositories' status into the list read from git, whose texts stand in the persistent tier until the
// next read, and every row points into it. The next commit takes every file but those left out: whether each file of
// the list is left out stands as a byte a file in a cell sized to the list, and the paths left out stand as one text of
// their own, so the choice outlasts the list's next read. The message typed stands in a cell sized to it, and git's
// refusal of the last commit in one sized to the refusal
#include "git.h"

#include "../command_line/commands.h"
#include "../text/typed.h"

// why nothing was committed, and the bytes git's own words of a commit may take
#define CHANGES_EMPTY "The commit message is empty."
#define CHANGES_NONE "No file is taken."
#define CHANGES_OUT 4096ULL

// the list, standing between the window's frames until the next read
static CliChangedList s_changed;

// The next commit: its message, the paths left out as one text each with its end and the bytes of that text, a byte a
// file of the list, 1 where the file is left out, and git's refusal of the last commit with its bytes
typedef struct
{
    CliTyped message;
    char *left;
    unsigned long long left_bytes;
    unsigned char *out;
    char *refusal;
    unsigned long long refusal_length;
} Changes;

static Changes s_changes;

typedef struct
{
    const char *path;
    const char *text;
    unsigned long long size;
} ChangesCtx;

// whether `path` is among the paths left out
EMBED_INLINE int left_has(const ChangesCtx *args)
{
    for (unsigned long long at = 0ULL; at < s_changes.left_bytes;)
    {
        const char *const one = s_changes.left + at;
        if (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = args->path, .cap = CLI_PATH_MAX_LEN}))
        {
            return 1;
        }
        at += cellul.len(&(CatenaFinitaCfg){.src = one, .cap = CLI_PATH_MAX_LEN}) + 1ULL;
    }
    return 0;
}

// the paths left out written again from the list's files marked out, in a cell sized to them, none where none is
EMBED_INLINE void left_rebuild(const ChangesCtx *args)
{
    (void)args;
    unsigned long long bytes = 0ULL;
    for (unsigned int at = 0u; (s_changes.out != NULL) && (at < s_changed.count); at += 1u)
    {
        const char *const path = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                            .text_read = &s_changed.text, .at = s_changed.item[at].path);
        bytes += s_changes.out[at] ? cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL : 0ULL;
    }
    char *const left = (bytes != 0ULL) ? g_cli_cells->persistent_buf_alloc(bytes, &g_cli_git_cells) : NULL;
    unsigned long long used = 0ULL;
    for (unsigned int at = 0u; (left != NULL) && (at < s_changed.count); at += 1u)
    {
        if (!s_changes.out[at])
        {
            continue;
        }
        const char *const path = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                            .text_read = &s_changed.text, .at = s_changed.item[at].path);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        memor.cpy(&(MemoriaCfg){.dst = left + used, .src = path, .bytes = length + 1ULL});
        used += length + 1ULL;
    }
    g_cli_cells->persistent_buf_release(s_changes.left, &g_cli_git_cells);
    s_changes.left = left;
    s_changes.left_bytes = (left != NULL) ? bytes : 0ULL;
}

// `size` bytes of `text` as the refusal, in a cell sized to them; none where `size` is 0
EMBED_INLINE void refusal_set(const ChangesCtx *args)
{
    g_cli_cells->persistent_buf_release(s_changes.refusal, &g_cli_git_cells);
    s_changes.refusal =
        (args->size != 0ULL) ? g_cli_cells->persistent_buf_alloc(args->size + 1ULL, &g_cli_git_cells) : NULL;
    s_changes.refusal_length = (s_changes.refusal != NULL) ? args->size : 0ULL;
    if (s_changes.refusal != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_changes.refusal, .src = args->text, .bytes = args->size});
        s_changes.refusal[args->size] = '\0';
    }
}

void cli_changes_read(const CliGitChangesCfg *args)
{
    g_cli_cells->persistent_buf_release(s_changed.text.bytes, &g_cli_git_cells);
    s_changed.text = (CliGitText){NULL, 0ULL, 0ULL, 0, 1};
    s_changed.count = 0u;
    g_cli_cells->persistent_buf_release(s_changes.out, &g_cli_git_cells);
    s_changes.out = NULL;
    if (args->root == NULL)
    {
        g_cli_cells->persistent_buf_release(s_changes.left, &g_cli_git_cells);
        s_changes.left = NULL;
        s_changes.left_bytes = 0ULL;
        return;
    }
    EMBED_CALL(cli_git_root_repos.change, CliGitRootReposCfg, .root = args->root, .named = 0,
               .found_changed_list_pointer = &s_changed);
    // a byte a file, 1 where the file was left out before this read; the paths left out that are no longer in the
    // list go with the rebuilding
    s_changes.out =
        (s_changed.count != 0u) ? g_cli_cells->persistent_buf_alloc(s_changed.count, &g_cli_git_cells) : NULL;
    for (unsigned int at = 0u; (s_changes.out != NULL) && (at < s_changed.count); at += 1u)
    {
        // whether a file is left out is 0 or 1, which a byte carries
        s_changes.out[at] =
            (unsigned char)EMBED_CALL(left_has, ChangesCtx,
                                      .path = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                         .text_read = &s_changed.text, .at = s_changed.item[at].path));
    }
    EMBED_CALL(left_rebuild, ChangesCtx, .size = 0ULL);
}

unsigned int cli_changes_rows(const CliGitChangesCfg *args)
{
    (void)args;
    return s_changed.count;
}

int cli_changes_row(const CliGitChangesCfg *args)
{
    if (args->at >= s_changed.count)
    {
        return 0;
    }
    const CliChanged *const changed = &s_changed.item[args->at];
    const char *const path =
        EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_changed.text, .at = changed->path);
    *args->row = (CliChangedRow){path, path + changed->name, (changed->name > 0u) ? changed->name - 1u : 0u,
                                 changed->state, (s_changes.out == NULL) || !s_changes.out[args->at]};
    return 1;
}

void cli_changes_toggle(const CliGitChangesCfg *args)
{
    if ((args->at >= s_changed.count) || (s_changes.out == NULL))
    {
        return;
    }
    s_changes.out[args->at] ^= 1u;
    EMBED_CALL(left_rebuild, ChangesCtx, .size = 0ULL);
}

int cli_changes_menu(const CliGitChangesCfg *args)
{
    CliChangedRow read;
    if (!EMBED_CALL(cli_changes.row, CliGitChangesCfg, .at = args->at, .row = &read))
    {
        return 0;
    }
    // the rows run with the file's path, which stands in the list while the menu is open
    cli_menus_own_begin();
    cli_menus_own_row(
        &(CliItem){"Open File", "file-open", "", read.path, (read.state == 'D') ? "never" : "", "", 0, 0, 0, 0, 0u});
    cli_menus_own_line();
    cli_menus_own_row(&(CliItem){"Copy Path", "file-copy-path", "", read.path, "", "", 0, 0, 0, 0, 0u});
    return 1;
}

void cli_changes_type(const CliGitChangesCfg *args)
{
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &s_changes.message, .text = args->text, .size = args->size,
               .most = CLI_CHANGES_MESSAGE_MAX, .owner = &g_cli_git_cells);
}

int cli_changes_key(const CliGitChangesCfg *args)
{
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 16ULL}))
    {
        return 0;
    }
    EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &s_changes.message, .owner = &g_cli_git_cells);
    return 1;
}

const char *cli_changes_message(const CliGitChangesCfg *args)
{
    *args->length_out = s_changes.message.length;
    return (s_changes.message.bytes != NULL) ? s_changes.message.bytes : "";
}

int cli_changes_commit(const CliGitChangesCfg *args)
{
    EMBED_CALL(refusal_set, ChangesCtx, .size = 0ULL);
    if (s_changes.message.length == 0ULL)
    {
        EMBED_CALL(refusal_set, ChangesCtx, .text = CHANGES_EMPTY, .size = sizeof(CHANGES_EMPTY) - 1ULL);
        return 0;
    }
    unsigned int taken = 0u;
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        taken += ((s_changes.out == NULL) || !s_changes.out[at]) ? 1u : 0u;
    }
    if (taken == 0u)
    {
        EMBED_CALL(refusal_set, ChangesCtx, .text = CHANGES_NONE, .size = sizeof(CHANGES_NONE) - 1ULL);
        return 0;
    }
    // the paths taken, git's own words and the error line, for this commit alone
    const unsigned long long paths_bytes = (unsigned long long)taken * sizeof(const char *);
    char *const scratch =
        g_cli_cells->temporary_buf_alloc(paths_bytes + CHANGES_OUT + CLI_ERR_MSG_MAX_LEN, &g_cli_git_cells);
    if (scratch == NULL)
    {
        return 0;
    }
    const char **const paths = (const char **)(void *)scratch;
    VerbaLinea out = {scratch + paths_bytes, CHANGES_OUT, 0ULL};
    VerbaLinea err_msg = {scratch + paths_bytes + CHANGES_OUT, CLI_ERR_MSG_MAX_LEN, 0ULL};
    unsigned int put = 0u;
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        if ((s_changes.out == NULL) || !s_changes.out[at])
        {
            paths[put] = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_changed.text,
                                    .at = s_changed.item[at].path);
            put += 1u;
        }
    }
    const int committed =
        EMBED_CALL(cli_git_root_out_count.commit, CliGitRootOutCountCfg, .root = args->root, .out = &out,
                   .count = taken, .message = s_changes.message.bytes, .paths = paths, .err_msg = &err_msg);
    if (committed)
    {
        g_cli_cells->persistent_buf_release(s_changes.message.bytes, &g_cli_git_cells);
        s_changes.message = (CliTyped){NULL, 0ULL};
    }
    else
    {
        EMBED_CALL(refusal_set, ChangesCtx, .text = scratch + paths_bytes + CHANGES_OUT,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
    }
    g_cli_cells->persistent_buf_release(scratch, &g_cli_git_cells);
    if (committed)
    {
        EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
    }
    return committed;
}

const char *cli_changes_refusal(const CliGitChangesCfg *args)
{
    *args->length_out = s_changes.refusal_length;
    return (s_changes.refusal != NULL) ? s_changes.refusal : "";
}
