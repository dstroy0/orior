// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_changes.c: the changes as the window reads them, and the next commit. Each read lists the tree's changed files
// through the repositories' status into the list read from git, whose texts stand in the persistent tier until the
// next read, and every row points into it, and reads how far the branch stands from the remote it follows. The next
// commit takes every file but those left out: whether each file of the list is left out stands as a byte a file in a
// cell sized to the list, and the paths left out stand as one text of their own, so the choice outlasts the list's
// next read. The message typed stands in a cell sized to it, and git's refusal of the last commit, pull, push or roll
// back in one sized to the refusal. The watch marks the changes stale from its thread, and the step reads them again
#include "git.h"

#include "../command_line/commands.h"
#include "../text/typed.h"
#include "embed_atomic.h"

// why nothing was committed, and the bytes git's own words of a commit, a pull or a push may take
#define CHANGES_EMPTY "The commit message is empty."
#define CHANGES_NONE "No file is taken."
#define CHANGES_OUT 4096ULL

// the list, standing between the window's frames until the next read
static CliChangedList s_changed;

// The next commit: its message, the paths left out as one text each with its end and the bytes of that text, a byte a
// file of the list, 1 where the file is left out, how many files are taken, git's refusal of the last commit, pull,
// push or roll back with its bytes, whether the list was read since the tree was opened, how far the branch stands from
// the remote it follows, 1 where it follows one, and the millisecond a roll back was armed at, 0 for not armed
typedef struct
{
    CliTyped message;
    char *left;
    unsigned long long left_bytes;
    unsigned char *out;
    unsigned int taken;
    char *refusal;
    unsigned long long refusal_length;
    int listed;
    int followed;
    unsigned int ahead;
    unsigned int behind;
    unsigned long long armed_ms;
} Changes;

static Changes s_changes;
static void (*s_woken)(void);
static embed_atomic_u32 s_stale;

typedef struct
{
    const char *path;
    const char *text;
    unsigned long long size;
    const char *root;
    int (*run)(const CliGitRootOutCountErrMsgCfg *args);
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

// the choice settled after the bytes a file changed: the files taken counted, and the paths left out written again
// from the files marked out, in a cell sized to them, none where none is
EMBED_INLINE void choice_settled(const ChangesCtx *args)
{
    (void)args;
    unsigned long long bytes = 0ULL;
    unsigned int taken = 0u;
    for (unsigned int at = 0u; (s_changes.out != NULL) && (at < s_changed.count); at += 1u)
    {
        const char *const path = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                            .text_read = &s_changed.text, .at = s_changed.item[at].path);
        bytes += s_changes.out[at] ? cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN}) + 1ULL : 0ULL;
        taken += s_changes.out[at] ? 0u : 1u;
    }
    s_changes.taken = taken;
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

// git `run` at `root`, pull or push: its words into a cell of this run alone, 1, or 0 with the refusal in a cell sized
// to why
EMBED_INLINE int remote_run(const ChangesCtx *args)
{
    g_cli_cells->persistent_buf_release(s_changes.refusal, &g_cli_git_cells);
    s_changes.refusal = NULL;
    s_changes.refusal_length = 0ULL;
    char *const scratch = g_cli_cells->temporary_buf_alloc(CHANGES_OUT + CLI_ERR_MSG_MAX_LEN, &g_cli_git_cells);
    if (scratch == NULL)
    {
        return 0;
    }
    VerbaLinea out = {scratch, CHANGES_OUT, 0ULL};
    VerbaLinea err_msg = {scratch + CHANGES_OUT, CLI_ERR_MSG_MAX_LEN, 0ULL};
    const int ran = args->run(&(CliGitRootOutCountErrMsgCfg){.root = args->root, .out = &out, .err_msg = &err_msg});
    const unsigned long long refused = (!ran && mmgr_line_ok(&err_msg)) ? err_msg.at : 0ULL;
    s_changes.refusal = (refused != 0ULL) ? g_cli_cells->persistent_buf_alloc(refused + 1ULL, &g_cli_git_cells) : NULL;
    s_changes.refusal_length = (s_changes.refusal != NULL) ? refused : 0ULL;
    if (s_changes.refusal != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_changes.refusal, .src = scratch + CHANGES_OUT, .bytes = refused});
        s_changes.refusal[refused] = '\0';
    }
    g_cli_cells->persistent_buf_release(scratch, &g_cli_git_cells);
    return ran;
}

void cli_changes_begin(const CliGitChangesCfg *args)
{
    s_woken = args->woken;
}

void cli_changes_stale(const CliGitChangesCfg *args)
{
    (void)args;
    EMBED_ATOMIC_STORE(&s_stale, 1u);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

int cli_changes_step(const CliGitChangesCfg *args)
{
    if (!args->on || (EMBED_ATOMIC_EXCHANGE_32(&s_stale, 0u) == 0u) || !s_changes.listed)
    {
        return 0;
    }
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
    return 1;
}

void cli_changes_read(const CliGitChangesCfg *args)
{
    g_cli_cells->persistent_buf_release(s_changed.text.bytes, &g_cli_git_cells);
    s_changed.text = (CliGitText){NULL, 0ULL, 0ULL, 0, 1};
    s_changed.count = 0u;
    g_cli_cells->persistent_buf_release(s_changes.out, &g_cli_git_cells);
    s_changes.out = NULL;
    s_changes.taken = 0u;
    s_changes.listed = args->root != NULL;
    if (args->root == NULL)
    {
        g_cli_cells->persistent_buf_release(s_changes.left, &g_cli_git_cells);
        s_changes.left = NULL;
        s_changes.left_bytes = 0ULL;
        s_changes.followed = 0;
        s_changes.ahead = 0u;
        s_changes.behind = 0u;
        return;
    }
    EMBED_CALL(cli_git_root_repos.change, CliGitRootReposCfg, .root = args->root, .named = 0,
               .found_changed_list_pointer = &s_changed);
    // a byte a file, 1 where the file was left out before this read; the paths left out that are no longer in the
    // list go with the settling
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
    EMBED_CALL(choice_settled, ChangesCtx, .size = 0ULL);
    s_changes.followed = EMBED_CALL(cli_git_root_ahead.ahead_behind, CliGitRootAheadCfg, .root = args->root,
                                    .ahead = &s_changes.ahead, .behind = &s_changes.behind);
    if (!s_changes.followed)
    {
        s_changes.ahead = 0u;
        s_changes.behind = 0u;
    }
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
    EMBED_CALL(choice_settled, ChangesCtx, .size = 0ULL);
}

unsigned int cli_changes_taken(const CliGitChangesCfg *args)
{
    (void)args;
    return s_changes.taken;
}

void cli_changes_take_all(const CliGitChangesCfg *args)
{
    if (s_changes.out == NULL)
    {
        return;
    }
    memor.set(&(MemoriaCfg){.dst = s_changes.out, .val = args->on ? 0 : 1, .bytes = s_changed.count});
    EMBED_CALL(choice_settled, ChangesCtx, .size = 0ULL);
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
    g_cli_cells->persistent_buf_release(s_changes.refusal, &g_cli_git_cells);
    s_changes.refusal = NULL;
    s_changes.refusal_length = 0ULL;
    const unsigned int taken = s_changes.taken;
    if ((s_changes.message.length == 0ULL) || (taken == 0u))
    {
        // why nothing is to commit, in a cell sized to the sentence with its end
        const char *const why = (s_changes.message.length == 0ULL) ? CHANGES_EMPTY : CHANGES_NONE;
        const unsigned long long size =
            (s_changes.message.length == 0ULL) ? sizeof(CHANGES_EMPTY) : sizeof(CHANGES_NONE);
        s_changes.refusal = g_cli_cells->persistent_buf_alloc(size, &g_cli_git_cells);
        s_changes.refusal_length = (s_changes.refusal != NULL) ? size - 1ULL : 0ULL;
        if (s_changes.refusal != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = s_changes.refusal, .src = why, .bytes = size});
        }
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
    const int committed = EMBED_CALL(cli_git_root_out_count.commit, CliGitRootOutCountCfg, .root = args->root,
                                     .out = &out, .count = taken, .message = s_changes.message.bytes, .paths = paths,
                                     .err_msg = &err_msg, .push = args->push);
    if (committed)
    {
        g_cli_cells->persistent_buf_release(s_changes.message.bytes, &g_cli_git_cells);
        s_changes.message = (CliTyped){NULL, 0ULL};
    }
    // git's refusal in a cell sized to it with its end
    const unsigned long long refused = (!committed && mmgr_line_ok(&err_msg)) ? err_msg.at : 0ULL;
    s_changes.refusal = (refused != 0ULL) ? g_cli_cells->persistent_buf_alloc(refused + 1ULL, &g_cli_git_cells) : NULL;
    s_changes.refusal_length = (s_changes.refusal != NULL) ? refused : 0ULL;
    if (s_changes.refusal != NULL)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_changes.refusal, .src = scratch + paths_bytes + CHANGES_OUT, .bytes = refused});
        s_changes.refusal[refused] = '\0';
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

int cli_changes_sync(const CliGitChangesCfg *args)
{
    *args->ahead = s_changes.ahead;
    *args->behind = s_changes.behind;
    return s_changes.followed;
}

int cli_changes_pull(const CliGitChangesCfg *args)
{
    const int pulled =
        EMBED_CALL(remote_run, ChangesCtx, .root = args->root, .run = cli_git_root_out_count_err_msg.pull);
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
    return pulled;
}

int cli_changes_push(const CliGitChangesCfg *args)
{
    const int pushed =
        EMBED_CALL(remote_run, ChangesCtx, .root = args->root, .run = cli_git_root_out_count_err_msg.push);
    EMBED_CALL(cli_changes.read, CliGitChangesCfg, .root = args->root);
    return pushed;
}

int cli_changes_rollback(const CliGitChangesCfg *args)
{
    if (s_changes.taken == 0u)
    {
        return 0;
    }
    if ((s_changes.armed_ms == 0ULL) || (args->now_ms - s_changes.armed_ms > CLI_CHANGES_ROLLBACK_MS))
    {
        s_changes.armed_ms = args->now_ms;
        return 0;
    }
    s_changes.armed_ms = 0ULL;
    g_cli_cells->persistent_buf_release(s_changes.refusal, &g_cli_git_cells);
    s_changes.refusal = NULL;
    s_changes.refusal_length = 0ULL;
    for (unsigned int at = 0u; at < s_changed.count; at += 1u)
    {
        if ((s_changes.out != NULL) && s_changes.out[at])
        {
            continue;
        }
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        const int rolled = EMBED_CALL(cli_git_root_err_msg_path.rollback, CliGitRootErrMsgPathCfg, .root = args->root,
                                      .path = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                         .text_read = &s_changed.text, .at = s_changed.item[at].path),
                                      .err_msg = &err_msg);
        const unsigned long long refused = (!rolled && mmgr_line_ok(&err_msg)) ? err_msg.at : 0ULL;
        if (refused == 0ULL)
        {
            continue;
        }
        // the file's refusal put after the refusals before it on a line of its own, in a cell sized to them all
        const unsigned long long before = s_changes.refusal_length;
        const unsigned long long joined = before + ((before != 0ULL) ? 1ULL : 0ULL);
        char *const grown = g_cli_cells->persistent_buf_alloc(joined + refused + 1ULL, &g_cli_git_cells);
        if (grown == NULL)
        {
            continue;
        }
        if (before != 0ULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_changes.refusal, .bytes = before});
            grown[before] = '\n';
        }
        memor.cpy(&(MemoriaCfg){.dst = grown + joined, .src = why, .bytes = refused});
        grown[joined + refused] = '\0';
        g_cli_cells->persistent_buf_release(s_changes.refusal, &g_cli_git_cells);
        s_changes.refusal = grown;
        s_changes.refusal_length = joined + refused;
    }
    return 1;
}
