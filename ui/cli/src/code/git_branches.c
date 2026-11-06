// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_branches.c: the branches and what is done to them, the stashes and what is done to them, the commits of other
// branches and one applied here, and the branch pushed and pulled
#include "../machine/net.h"
#include "../text/text.h"
#include "git_internal.h"

// the most bytes of what git reports of what it was requested to do, stored to be reported
#define CLI_GIT_OUTPUT_MAX_LEN (1u << 16u)

void cli_git_branches(const CliGitRootFoundBranchesCfg *args)
{
    static const char *const s_args[5] = {
        "for-each-ref", "--sort=-committerdate",
        "--format=%(refname)%00%(refname:short)%00%(HEAD)%00%(upstream:short)%00%(committerdate:unix)%00%(subject)",
        "refs/heads", "refs/remotes"};
    args->found_branches_pointer->count = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &args->found_branches_pointer->text);
    CliGitResult *const result = cli_git_alloc();
    const int queried = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root,
                                   .request = s_args, .count = 5u, .result = result);
    const char *const end = result->out + result->out_size;
    const char *line = queried ? result->out : end;
    while ((line < end) && (args->found_branches_pointer->count < CLI_GIT_BRANCHES))
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        const char *at = line;
        unsigned long long lengths[6] = {0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL};
        const char *fields[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
        for (unsigned int field = 0u; field < 6u; field += 1u)
        {
            fields[field] = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &at, .end = stop, .stop = '\0',
                                       .length_pointer = &lengths[field]);
        }
        line = next;
        const int remote = (fields[0] != NULL) && (lengths[0] >= 13ULL) &&
                           (memor.cmp(&(MemoriaCfg){.src = fields[0], .other = "refs/remotes/", .bytes = 13u}) == 0);
        const int remote_head =
            remote && (lengths[0] >= 5ULL) &&
            (memor.cmp(&(MemoriaCfg){.src = fields[0] + lengths[0] - 5ULL, .other = "/HEAD", .bytes = 5u}) == 0);
        if ((fields[4] == NULL) || remote_head)
        {
            continue;
        }
        CliBranch *const one = &args->found_branches_pointer->item[args->found_branches_pointer->count];
        args->found_branches_pointer->count += 1u;
        one->name = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found_branches_pointer->text,
                               .from = fields[1], .length = lengths[1]);
        one->remote = remote;
        one->current = (lengths[2] == 1ULL) && (fields[2][0] == '*');
        one->upstream = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found_branches_pointer->text,
                                   .from = fields[3], .length = lengths[3]);
        one->when = 0LL;
        EMBED_CALL(cli_git_length_text_internal.seconds, CliGitLengthTextCtx, .text = fields[4], .length = lengths[4],
                   .value = &one->when);
        one->subject = (fields[5] != NULL) ? EMBED_CALL(cli_git_text.text_append, CliGitTextCfg,
                                                        .text = &args->found_branches_pointer->text, .from = fields[5],
                                                        .length = lengths[5])
                                           : 0u;
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
}

typedef struct
{
    const char *root;
    const char *const *request;
    unsigned int count;
    VerbaLinea *err_msg;
    char *address;
} GitBranchesCtx;
// git run with `args` at `root`, what it wrote and reported let go: 1, or 0 with what it reported went wrong
EMBED_INLINE int cli_run_quiet(const GitBranchesCtx *args)
{
    const char *root = args->root;
    const char *const *arguments = args->request;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    static char s_output_buf[CLI_GIT_OUTPUT_MAX_LEN];
    VerbaLinea out = {s_output_buf, sizeof(s_output_buf), 0ULL};
    const int ok = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = root,
                              .request = arguments, .count = count, .out = &out, .err_msg = err_msg);
    return ok;
}

int cli_git_branch_act(const CliGitRootOutCfg *args)
{
    if ((args->name_read[0] == '\0') || (args->name_read[0] == '-') || (args->to[0] == '-'))
    {
        mmgr_line_put(args->err_msg, args->name_read);
        mmgr_line_put(args->err_msg, " is no branch's name");
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "create", .cap = CLI_POOL_BYTES}))
    {
        const char *const check[3] = {"check-ref-format", "--branch", args->name_read};
        const char *const arguments[3] = {"switch", "-c", args->name_read};
        return EMBED_CALL(cli_run_quiet, GitBranchesCtx, .root = args->root, .request = check, .count = 3u,
                          .err_msg = args->err_msg) &&
               EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                          .request = arguments, .count = 3u, .out = args->out, .err_msg = args->err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "switch", .cap = CLI_POOL_BYTES}))
    {
        char remote_ref[CLI_PATH_MAX_LEN];
        VerbaLinea line = {remote_ref, sizeof(remote_ref), 0ULL};
        mmgr_line_put(&line, "refs/remotes/");
        mmgr_line_put(&line, args->name_read);
        mmgr_line_finish(&line);
        const char *const displayed[4] = {"show-ref", "--verify", "--quiet", remote_ref};
        char err_buf[CLI_PATH_MAX_LEN];
        VerbaLinea ignored = {err_buf, sizeof(err_buf), 0ULL};
        const int remote = EMBED_CALL(cli_run_quiet, GitBranchesCtx, .root = args->root, .request = displayed,
                                      .count = 4u, .err_msg = &ignored);
        const char *const track[3] = {"switch", "--track", args->name_read};
        const char *const plain[2] = {"switch", args->name_read};
        return remote ? EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                                   .request = track, .count = 3u, .out = args->out, .err_msg = args->err_msg)
                      : EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                                   .request = plain, .count = 2u, .out = args->out, .err_msg = args->err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "rename", .cap = CLI_POOL_BYTES}))
    {
        const char *const check[3] = {"check-ref-format", "--branch", args->to};
        const char *const arguments[4] = {"branch", "-m", args->name_read, args->to};
        return EMBED_CALL(cli_run_quiet, GitBranchesCtx, .root = args->root, .request = check, .count = 3u,
                          .err_msg = args->err_msg) &&
               EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                          .request = arguments, .count = 4u, .out = args->out, .err_msg = args->err_msg);
    }
    static const char *const s_acts[4] = {"delete", "delete-unmerged", "merge", "rebase"};
    static const char *const s_words[4][2] = {
        {"branch", "-d"}, {"branch", "-D"}, {"merge", "--no-edit"}, {"rebase", NULL}};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = s_acts[at], .cap = CLI_POOL_BYTES}))
        {
            const char *const arguments[3] = {
                s_words[at][0], (s_words[at][1] != NULL) ? s_words[at][1] : args->name_read, args->name_read};
            return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                              .request = arguments, .count = (s_words[at][1] != NULL) ? 3u : 2u, .out = args->out,
                              .err_msg = args->err_msg);
        }
    }
    mmgr_line_put(args->err_msg, args->act);
    mmgr_line_put(args->err_msg, " is nothing a branch is requested to do");
    return 0;
}

void cli_git_stashes(const CliGitRootFoundStashesCfg *args)
{
    static const char *const s_args[3] = {"stash", "list", "--format=%gd%x1f%ct%x1f%gs"};
    args->found_stashes_pointer->count = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &args->found_stashes_pointer->text);
    CliGitResult *const result = cli_git_alloc();
    const int queried = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root,
                                   .request = s_args, .count = 3u, .result = result);
    const char *const end = result->out + result->out_size;
    const char *line = queried ? result->out : end;
    while ((line < end) && (args->found_stashes_pointer->count < CLI_GIT_STASHES))
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        const char *at = line;
        unsigned long long name_length = 0ULL;
        unsigned long long when_length = 0ULL;
        const char *const name = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &at, .end = stop,
                                            .stop = '\x1f', .length_pointer = &name_length);
        const char *const when = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &at, .end = stop,
                                            .stop = '\x1f', .length_pointer = &when_length);
        // the subject is the rest of the line, whatever it contains
        const char *const subject = (at <= stop) ? at : NULL;
        line = next;
        if (when == NULL)
        {
            continue;
        }
        CliStash *const one = &args->found_stashes_pointer->item[args->found_stashes_pointer->count];
        args->found_stashes_pointer->count += 1u;
        one->name = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found_stashes_pointer->text,
                               .from = name, .length = name_length);
        one->when = 0LL;
        EMBED_CALL(cli_git_length_text_internal.seconds, CliGitLengthTextCtx, .text = when, .length = when_length,
                   .value = &one->when);
        one->subject = (subject != NULL) ? EMBED_CALL(cli_git_text.text_append, CliGitTextCfg,
                                                      .text = &args->found_stashes_pointer->text, .from = subject,
                                                      .length = (unsigned long long)(stop - subject))
                                         : 0u;
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
}

int cli_git_stash_act(const CliGitRootOutMessageCfg *args)
{
    int named = cellul.starts(&(CatenaFinitaCfg){.src = args->name_read, .other = "stash@{", .cap = CLI_POOL_BYTES});
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name_read, .cap = CLI_POOL_BYTES});
    named = named && (length > 8ULL) && (args->name_read[length - 1ULL] == '}');
    unsigned long long at = 7ULL;
    while (named && (at < (length - 1ULL)))
    {
        named = (args->name_read[at] >= '0') && (args->name_read[at] <= '9');
        at += 1ULL;
    }
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "push", .cap = CLI_POOL_BYTES})) && !named)
    {
        mmgr_line_put(args->err_msg, args->name_read);
        mmgr_line_put(args->err_msg, " is no stash's name");
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "push", .cap = CLI_POOL_BYTES}))
    {
        const char *from = args->message;
        const char *to = args->message + cellul.len(&(CatenaFinitaCfg){.src = args->message, .cap = CLI_POOL_BYTES});
        EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
        char trimmed[CLI_GIT_OUTPUT_MAX_LEN];
        VerbaLinea line = {trimmed, sizeof(trimmed), 0ULL};
        mmgr_line_put_n(&line, from, (unsigned long long)(to - from));
        mmgr_line_finish(&line);
        const char *const arguments[5] = {"stash", "push", "--include-untracked", "-m", trimmed};
        return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                          .request = arguments, .count = (from == to) ? 3u : 5u, .out = args->out,
                          .err_msg = args->err_msg);
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "apply", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "pop", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "drop", .cap = CLI_POOL_BYTES})))
    {
        const char *const arguments[3] = {"stash", args->act, args->name_read};
        return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                          .request = arguments, .count = 3u, .out = args->out, .err_msg = args->err_msg);
    }
    mmgr_line_put(args->err_msg, args->act);
    mmgr_line_put(args->err_msg, " is nothing a stash is requested to do");
    return 0;
}

int cli_git_cherry_pick(const CliGitRootFileCfg *args)
{
    if (!EMBED_CALL(cli_git_internal.is_id, CliGitCtx, .id = args->id))
    {
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, " is not a commit");
        return 0;
    }
    const char *const arguments[2] = {"cherry-pick", args->id};
    return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                      .request = arguments, .count = 2u, .out = args->out, .err_msg = args->err_msg);
}

// the address of the remote the branch follows, or of origin, as git would reach it, into `address`, sized for
// CLI_PATH_MAX_LEN, empty where git cannot determine
EMBED_INLINE void cli_remote_address(const GitBranchesCtx *args)
{
    const char *root = args->root;
    static const char *const s_args[2] = {"ls-remote", "--get-url"};
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea ignored = {err_buf, sizeof(err_buf), 0ULL};
    VerbaLinea line = {args->address, CLI_PATH_MAX_LEN, 0ULL};
    if (!EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = root, .request = s_args,
                    .count = 2u, .out = &line, .err_msg = &ignored))
    {
        line.at = 0ULL;
    }
    mmgr_line_finish(&line);
}

int cli_git_push(const CliGitRootOutCountErrMsgCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    static const char *const s_follows[4] = {"rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"};
    static const char *const s_push[1] = {"push"};
    static const char *const s_push_new[4] = {"push", "-u", "origin", "HEAD"};
    char address[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_remote_address, GitBranchesCtx, .root = args->root, .address = address);
    if (!EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = address, .purpose = "git push",
                    .err_msg = err_msg))
    {
        return 0;
    }
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea ignored = {err_buf, sizeof(err_buf), 0ULL};
    return EMBED_CALL(cli_run_quiet, GitBranchesCtx, .root = args->root, .request = s_follows, .count = 4u,
                      .err_msg = &ignored)
               ? EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                            .request = s_push, .count = 1u, .out = args->out, .err_msg = err_msg)
               : EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                            .request = s_push_new, .count = 4u, .out = args->out, .err_msg = err_msg);
}

int cli_git_pull(const CliGitRootOutCountErrMsgCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    static const char *const s_pull[2] = {"pull", "--ff-only"};
    char address[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_remote_address, GitBranchesCtx, .root = args->root, .address = address);
    return EMBED_CALL(cli_net_address.reach, CliNetAddressCfg, .address = address, .purpose = "git pull",
                      .err_msg = err_msg) &&
           EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->root,
                      .request = s_pull, .count = 2u, .out = args->out, .err_msg = err_msg);
}

int cli_git_ahead_behind(const CliGitRootAheadCfg *args)
{
    unsigned int *ahead = args->ahead;
    unsigned int *behind = args->behind;
    static const char *const s_args[4] = {"rev-list", "--left-right", "--count", "HEAD...@{u}"};
    CliGitResult *const result = cli_git_alloc();
    int found = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root, .request = s_args,
                           .count = 4u, .result = result);
    unsigned int counts[2] = {0u, 0u};
    unsigned int read = 0u;
    const char *at = result->out;
    const char *const end = result->out + result->out_size;
    while (found && (read < 2u) && (at < end))
    {
        at += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                         .size = (unsigned long long)(end - at), .set = CLI_TEXT_WHITE);
        const char *const word_text = at;
        at += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                         .size = (unsigned long long)(end - at), .set = CLI_TEXT_WHITE);
        long long value = 0LL;
        if ((at > word_text) &&
            EMBED_CALL(cli_git_length_text_internal.seconds, CliGitLengthTextCtx, .text = word_text,
                       .length = (unsigned long long)(at - word_text), .value = &value) &&
            (value >= 0LL) && (value <= 0xffffffffLL))
        {
            // a count of commits fits an unsigned int, as checked above
            counts[read] = (unsigned int)value;
            read += 1u;
        }
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    found = found && (read == 2u);
    *ahead = counts[0];
    *behind = counts[1];
    return found;
}
