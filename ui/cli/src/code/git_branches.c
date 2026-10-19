// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_branches.c: the branches and what is done to them, the stashes and what is done to them, the commits of other
// branches and one applied here, and the branch pushed and pulled
#include "../machine/net.h"
#include "git_internal.h"

// the most bytes of what git says of what it was asked to do kept to be told
#define CLI_GIT_TOLD (1u << 16u)

void cli_git_branches(const char *root, CliBranches *found)
{
    static const char *const s_args[5] = {
        "for-each-ref", "--sort=-committerdate",
        "--format=%(refname)%00%(refname:short)%00%(HEAD)%00%(upstream:short)%00%(committerdate:unix)%00%(subject)",
        "refs/heads", "refs/remotes"};
    found->count = 0u;
    cli_git_text_begun(&found->text);
    CliGitGave *const gave = cli_git_taken();
    const int asked = cli_git_asked(root, s_args, 5u, gave);
    const char *const end = gave->out + gave->out_size;
    const char *line = asked ? gave->out : end;
    while ((line < end) && (found->count < CLI_GIT_BRANCHES))
    {
        const char *stop = line;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        const char *at = line;
        unsigned long long lengths[6] = {0ull, 0ull, 0ull, 0ull, 0ull, 0ull};
        const char *fields[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
        for (unsigned int field = 0u; field < 6u; field += 1u)
        {
            fields[field] = cli_git_field(&at, stop, '\0', &lengths[field]);
        }
        line = next;
        const int remote = (fields[0] != NULL) && (lengths[0] >= 13ull) &&
                           (memor.cmp(&(MemoriaCfg){.src = fields[0], .other = "refs/remotes/", .bytes = 13u}) == 0);
        const int remote_head =
            remote && (lengths[0] >= 5ull) &&
            (memor.cmp(&(MemoriaCfg){.src = fields[0] + lengths[0] - 5ull, .other = "/HEAD", .bytes = 5u}) == 0);
        if ((fields[4] == NULL) || remote_head)
        {
            continue;
        }
        CliBranch *const one = &found->item[found->count];
        found->count += 1u;
        one->name = cli_git_text_put(&found->text, fields[1], lengths[1]);
        one->remote = remote;
        one->current = (lengths[2] == 1ull) && (fields[2][0] == '*');
        one->upstream = cli_git_text_put(&found->text, fields[3], lengths[3]);
        one->when = 0ll;
        cli_git_seconds(fields[4], lengths[4], &one->when);
        one->subject = (fields[5] != NULL) ? cli_git_text_put(&found->text, fields[5], lengths[5]) : 0u;
    }
    cli_git_given(gave);
}

// git run with `args` at `root`, what it wrote and said let go: 1, or 0 with what it said went wrong
static int cli_run_quiet(const char *root, const char *const *args, unsigned int count, CliLine *said)
{
    static char s_told[CLI_GIT_TOLD];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    CliLine out = {s_told, sizeof(s_told), 0ull};
    const int ok = cli_git_run(root, args, count, &out, said);
    cli_lock_give(&s_lock);
    return ok;
}

int cli_git_branch_act(const char *root, const char *act, const char *name, const char *to, CliLine *out, CliLine *said)
{
    if ((name[0] == '\0') || (name[0] == '-') || (to[0] == '-'))
    {
        cli_line_text(said, name);
        cli_line_text(said, " is no branch's name");
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "create", .cap = CLI_CELLS_POOL}))
    {
        const char *const check[3] = {"check-ref-format", "--branch", name};
        const char *const args[3] = {"switch", "-c", name};
        return cli_run_quiet(root, check, 3u, said) && cli_git_run(root, args, 3u, out, said);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "switch", .cap = CLI_CELLS_POOL}))
    {
        char remote_ref[CLI_PATH_LONGEST];
        CliLine line = {remote_ref, sizeof(remote_ref), 0ull};
        cli_line_text(&line, "refs/remotes/");
        cli_line_text(&line, name);
        cli_line_closed(&line);
        const char *const shown[4] = {"show-ref", "--verify", "--quiet", remote_ref};
        char unsaid[CLI_PATH_LONGEST];
        CliLine ignored = {unsaid, sizeof(unsaid), 0ull};
        const int remote = cli_run_quiet(root, shown, 4u, &ignored);
        const char *const track[3] = {"switch", "--track", name};
        const char *const plain[2] = {"switch", name};
        return remote ? cli_git_run(root, track, 3u, out, said) : cli_git_run(root, plain, 2u, out, said);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "rename", .cap = CLI_CELLS_POOL}))
    {
        const char *const check[3] = {"check-ref-format", "--branch", to};
        const char *const args[4] = {"branch", "-m", name, to};
        return cli_run_quiet(root, check, 3u, said) && cli_git_run(root, args, 4u, out, said);
    }
    static const char *const s_acts[4] = {"delete", "delete-unmerged", "merge", "rebase"};
    static const char *const s_words[4][2] = {
        {"branch", "-d"}, {"branch", "-D"}, {"merge", "--no-edit"}, {"rebase", NULL}};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = act, .other = s_acts[at], .cap = CLI_CELLS_POOL}))
        {
            const char *const args[3] = {s_words[at][0], (s_words[at][1] != NULL) ? s_words[at][1] : name, name};
            return cli_git_run(root, args, (s_words[at][1] != NULL) ? 3u : 2u, out, said);
        }
    }
    cli_line_text(said, act);
    cli_line_text(said, " is nothing a branch is asked to do");
    return 0;
}

void cli_git_stashes(const char *root, CliStashes *found)
{
    static const char *const s_args[3] = {"stash", "list", "--format=%gd%x1f%ct%x1f%gs"};
    found->count = 0u;
    cli_git_text_begun(&found->text);
    CliGitGave *const gave = cli_git_taken();
    const int asked = cli_git_asked(root, s_args, 3u, gave);
    const char *const end = gave->out + gave->out_size;
    const char *line = asked ? gave->out : end;
    while ((line < end) && (found->count < CLI_GIT_STASHES))
    {
        const char *stop = line;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        const char *at = line;
        unsigned long long name_length = 0ull;
        unsigned long long when_length = 0ull;
        const char *const name = cli_git_field(&at, stop, '\x1f', &name_length);
        const char *const when = cli_git_field(&at, stop, '\x1f', &when_length);
        // the subject is the rest of the line, whatever it holds
        const char *const subject = (at <= stop) ? at : NULL;
        line = next;
        if (when == NULL)
        {
            continue;
        }
        CliStash *const one = &found->item[found->count];
        found->count += 1u;
        one->name = cli_git_text_put(&found->text, name, name_length);
        one->when = 0ll;
        cli_git_seconds(when, when_length, &one->when);
        one->subject =
            (subject != NULL) ? cli_git_text_put(&found->text, subject, (unsigned long long)(stop - subject)) : 0u;
    }
    cli_git_given(gave);
}

int cli_git_stash_act(const char *root, const char *act, const char *name, const char *message, CliLine *out,
                      CliLine *said)
{
    int named = cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "stash@{", .cap = CLI_CELLS_POOL});
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    named = named && (length > 8ull) && (name[length - 1ull] == '}');
    for (unsigned long long at = 7ull; named && (at < (length - 1ull)); at += 1ull)
    {
        named = (name[at] >= '0') && (name[at] <= '9');
    }
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "push", .cap = CLI_CELLS_POOL})) && !named)
    {
        cli_line_text(said, name);
        cli_line_text(said, " is no stash's name");
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "push", .cap = CLI_CELLS_POOL}))
    {
        const char *from = message;
        const char *to = message + cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL});
        cli_git_trimmed(&from, &to);
        char trimmed[CLI_GIT_TOLD];
        CliLine line = {trimmed, sizeof(trimmed), 0ull};
        cli_line_counted(&line, from, (unsigned long long)(to - from));
        cli_line_closed(&line);
        const char *const args[5] = {"stash", "push", "--include-untracked", "-m", trimmed};
        return cli_git_run(root, args, (from == to) ? 3u : 5u, out, said);
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "apply", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "pop", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "drop", .cap = CLI_CELLS_POOL})))
    {
        const char *const args[3] = {"stash", act, name};
        return cli_git_run(root, args, 3u, out, said);
    }
    cli_line_text(said, act);
    cli_line_text(said, " is nothing a stash is asked to do");
    return 0;
}

int cli_git_cherry_pick(const char *root, const char *id, CliLine *out, CliLine *said)
{
    if (!cli_git_is_id(id))
    {
        cli_line_text(said, id);
        cli_line_text(said, " is not a commit");
        return 0;
    }
    const char *const args[2] = {"cherry-pick", id};
    return cli_git_run(root, args, 2u, out, said);
}

// the address of the remote the branch follows, or of origin, as git would reach it, into `address`, which holds
// CLI_PATH_LONGEST, empty where git cannot say
static void cli_remote_address(const char *root, char *address)
{
    static const char *const s_args[2] = {"ls-remote", "--get-url"};
    char unsaid[CLI_PATH_LONGEST];
    CliLine ignored = {unsaid, sizeof(unsaid), 0ull};
    CliLine line = {address, CLI_PATH_LONGEST, 0ull};
    if (!cli_git_run(root, s_args, 2u, &line, &ignored))
    {
        line.at = 0ull;
    }
    cli_line_closed(&line);
}

int cli_git_push(const char *root, CliLine *out, CliLine *said)
{
    static const char *const s_follows[4] = {"rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{u}"};
    static const char *const s_push[1] = {"push"};
    static const char *const s_push_new[4] = {"push", "-u", "origin", "HEAD"};
    char address[CLI_PATH_LONGEST];
    cli_remote_address(root, address);
    if (!cli_net_reach(address, "git push", said))
    {
        return 0;
    }
    char unsaid[CLI_PATH_LONGEST];
    CliLine ignored = {unsaid, sizeof(unsaid), 0ull};
    return cli_run_quiet(root, s_follows, 4u, &ignored) ? cli_git_run(root, s_push, 1u, out, said)
                                                        : cli_git_run(root, s_push_new, 4u, out, said);
}

int cli_git_pull(const char *root, CliLine *out, CliLine *said)
{
    static const char *const s_pull[2] = {"pull", "--ff-only"};
    char address[CLI_PATH_LONGEST];
    cli_remote_address(root, address);
    return cli_net_reach(address, "git pull", said) && cli_git_run(root, s_pull, 2u, out, said);
}

int cli_git_ahead_behind(const char *root, unsigned int *ahead, unsigned int *behind)
{
    static const char *const s_args[4] = {"rev-list", "--left-right", "--count", "HEAD...@{u}"};
    CliGitGave *const gave = cli_git_taken();
    int found = cli_git_asked(root, s_args, 4u, gave);
    unsigned int counts[2] = {0u, 0u};
    unsigned int read = 0u;
    const char *at = gave->out;
    const char *const end = gave->out + gave->out_size;
    while (found && (read < 2u) && (at < end))
    {
        while ((at < end) && ((at[0] == ' ') || ((at[0] >= '\t') && (at[0] <= '\r'))))
        {
            at += 1;
        }
        const char *const word = at;
        while ((at < end) && (at[0] != ' ') && !((at[0] >= '\t') && (at[0] <= '\r')))
        {
            at += 1;
        }
        long long value = 0ll;
        if ((at > word) && cli_git_seconds(word, (unsigned long long)(at - word), &value) && (value >= 0ll) &&
            (value <= 0xffffffffll))
        {
            // a count of commits fits an unsigned int, as checked above
            counts[read] = (unsigned int)value;
            read += 1u;
        }
    }
    cli_git_given(gave);
    found = found && (read == 2u);
    *ahead = counts[0];
    *behind = counts[1];
    return found;
}
