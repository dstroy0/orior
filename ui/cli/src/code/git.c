// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git.c: the commit and the branch a tree stands at, and the files that differ from their last commits
#include "../text/text.h"
#include "git_internal.h"

// the state letter for a porcelain status's two columns
char cli_git_state_of(char index, char tree)
{
    if ((index == '?') && (tree == '?'))
    {
        return 'U';
    }
    if ((index == 'U') || (tree == 'U') || ((index == 'A') && (tree == 'A')) || ((index == 'D') && (tree == 'D')))
    {
        return 'C';
    }
    if ((index == 'D') || (tree == 'D'))
    {
        return 'D';
    }
    return (index == 'R') ? 'R' : (index == 'A') ? 'A' : 'M';
}

int cli_git_head(const char *root, char *commit, long long *when, int *clean)
{
    static const char *const s_log[3] = {"log", "-1", "--format=%H %ct"};
    static const char *const s_status[3] = {"status", "--porcelain", "--untracked-files=no"};
    CliGitGave *const gave = cli_git_taken();
    int found = cli_git_asked(root, s_log, 3u, gave);
    const char *from = gave->out;
    const char *to = gave->out + gave->out_size;
    cli_git_trimmed(&from, &to);
    const char *space = from;
    while ((space < to) && (space[0] != ' '))
    {
        space += 1;
    }
    found = found && (space < to) && (space[1] >= '0') && (space[1] <= '9');
    long long seconds = 0ll;
    for (const char *digit = space + 1; found && (digit < to); digit += 1)
    {
        found = (digit[0] >= '0') && (digit[0] <= '9');
        seconds = (seconds * 10ll) + (long long)(digit[0] - '0');
    }
    if (found)
    {
        CliLine line = {commit, CLI_NAME_LONGEST, 0ull};
        cli_line_counted(&line, from, (unsigned long long)(space - from));
        cli_line_closed(&line);
        *when = seconds;
        int all_white = 1;
        if (cli_git_asked(root, s_status, 3u, gave))
        {
            for (unsigned long long at = 0ull; all_white && (at < gave->out_size); at += 1ull)
            {
                const char letter = gave->out[at];
                all_white = (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
            }
            *clean = all_white;
        }
        else
        {
            *clean = 0;
        }
    }
    cli_git_given(gave);
    return found;
}

// what git wrote, trimmed, into `out`, which holds `room`: 1, or 0 where git failed
static int cli_asked_line(const char *root, const char *const *args, unsigned int count, char *out,
                          unsigned long long room)
{
    CliGitGave *const gave = cli_git_taken();
    const int found = cli_git_asked(root, args, count, gave);
    const char *from = gave->out;
    const char *to = gave->out + gave->out_size;
    cli_git_trimmed(&from, &to);
    CliLine line = {out, room, 0ull};
    cli_line_counted(&line, from, found ? (unsigned long long)(to - from) : 0ull);
    cli_line_closed(&line);
    cli_git_given(gave);
    return found;
}

int cli_git_branch(const char *root, char *name)
{
    static const char *const s_named[3] = {"rev-parse", "--abbrev-ref", "HEAD"};
    static const char *const s_short[3] = {"rev-parse", "--short", "HEAD"};
    if (!cli_asked_line(root, s_named, 3u, name, CLI_NAME_LONGEST))
    {
        return 0;
    }
    return (!cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "HEAD", .cap = CLI_CELLS_POOL})) ||
           cli_asked_line(root, s_short, 3u, name, CLI_NAME_LONGEST);
}

void cli_git_name(const char *root, char *name)
{
    static const char *const s_remote[3] = {"config", "--get", "remote.origin.url"};
    char remote[CLI_PATH_LONGEST];
    if (!cli_asked_line(root, s_remote, 3u, remote, sizeof(remote)))
    {
        remote[0] = '\0';
    }
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = remote, .cap = CLI_CELLS_POOL});
    while ((length > 0ull) && (remote[length - 1ull] == '/'))
    {
        length -= 1ull;
    }
    unsigned long long start = length;
    while ((start > 0ull) && (remote[start - 1ull] != '/') && (remote[start - 1ull] != ':') &&
           (remote[start - 1ull] != '\\'))
    {
        start -= 1ull;
    }
    if (((length - start) >= 4ull) &&
        (memor.cmp(&(MemoriaCfg){.src = remote + length - 4ull, .other = ".git", .bytes = 4u}) == 0))
    {
        length -= 4ull;
    }
    if (length > start)
    {
        CliLine line = {name, CLI_NAME_LONGEST, 0ull};
        cli_line_counted(&line, remote + start, length - start);
        cli_line_closed(&line);
        return;
    }
    // the folder's own name, the last part of its path
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    while ((end > 0ull) && ((root[end - 1ull] == '/') || (root[end - 1ull] == '\\')))
    {
        end -= 1ull;
    }
    unsigned long long first = end;
    while ((first > 0ull) && (root[first - 1ull] != '/') && (root[first - 1ull] != '\\'))
    {
        first -= 1ull;
    }
    const int is_drive = ((end - first) == 2ull) && (root[first + 1ull] == ':');
    CliLine line = {name, CLI_NAME_LONGEST, 0ull};
    cli_line_counted(&line, root + first, is_drive ? 0ull : (end - first));
    cli_line_closed(&line);
}

int cli_git_stage_text(const char *root, const char *file, unsigned int stage, CliLine *out)
{
    char folder[CLI_PATH_LONGEST];
    char shown[CLI_PATH_LONGEST];
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_git_beside(root, file, folder, shown, &said))
    {
        return 0;
    }
    char named[CLI_PATH_LONGEST + 8u];
    CliLine line = {named, sizeof(named), 0ull};
    cli_line_character(&line, ':');
    cli_line_decimal(&line, stage, 1u);
    cli_line_character(&line, ':');
    cli_line_text(&line, shown);
    cli_line_closed(&line);
    const char *const args[2] = {"show", named};
    CliGitGave *const gave = cli_git_taken();
    const int found = cli_git_asked(folder, args, 2u, gave);
    if (found)
    {
        cli_line_counted(out, gave->out, gave->out_size);
    }
    cli_git_given(gave);
    return found;
}

// the repository of `repos` that holds `path`, both paths in the tree: the deepest whose folder holds it, or NULL
const char *cli_git_owner(const char *const *repos, unsigned int count, const char *path, unsigned long long length)
{
    const char *owner = NULL;
    unsigned long long longest = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const repo = repos[at];
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = repo, .cap = CLI_CELLS_POOL});
        const int holds =
            (size == 0ull) ||
            ((size == length) && (memor.cmp(&(MemoriaCfg){.src = path, .other = repo, .bytes = size}) == 0)) ||
            ((size < length) && (memor.cmp(&(MemoriaCfg){.src = path, .other = repo, .bytes = size}) == 0) &&
             (path[size] == '/'));
        if (holds && ((owner == NULL) || (size >= longest)))
        {
            owner = repo;
            longest = size;
        }
    }
    return owner;
}

// the files under the repository at `folder`, whose path in the tree is `repo`, that differ from its last commit, put
// after the changes found where the repository of `repos` that holds each is `repo`
static void cli_changed_at(const char *folder, const char *repo, const char *const *repos, unsigned int count,
                           CliChangedList *found)
{
    static const char *const s_prefix[2] = {"rev-parse", "--show-prefix"};
    static const char *const s_status[5] = {"status", "--porcelain=v1", "-z", "--untracked-files=all", "--"};
    char prefix[CLI_PATH_LONGEST];
    if (!cli_asked_line(folder, s_prefix, 2u, prefix, sizeof(prefix)))
    {
        prefix[0] = '\0';
    }
    const char *const args[6] = {s_status[0], s_status[1], s_status[2], s_status[3], s_status[4], "."};
    CliGitGave *const gave = cli_git_taken();
    if (!cli_git_asked(folder, args, 6u, gave))
    {
        cli_git_given(gave);
        return;
    }
    const unsigned long long prefix_length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL});
    const unsigned long long repo_length = cellul.len(&(CatenaFinitaCfg){.src = repo, .cap = CLI_CELLS_POOL});
    const char *record = gave->out;
    const char *const end = gave->out + gave->out_size;
    int skip = 0;
    while (record < end)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = record, .cap = CLI_CELLS_POOL});
        const char *const next = record + length + 1;
        if (length == 0ull)
        {
            record = next;
            continue;
        }
        if (skip)
        {
            skip = 0;
            record = next;
            continue;
        }
        if (length < 4ull)
        {
            record = next;
            continue;
        }
        const char index = record[0];
        const char tree = record[1];
        // a rename or a copy is followed by the path it came from
        skip = (index == 'R') || (index == 'C');
        if (cli_text_same(record + 3, prefix, prefix_length) && (found->count < CLI_GIT_CHANGED))
        {
            char path[CLI_PATH_LONGEST];
            CliLine line = {path, sizeof(path), 0ull};
            cli_line_text(&line, repo);
            cli_line_text(&line, (repo_length != 0ull) ? "/" : "");
            cli_line_text(&line, record + 3 + prefix_length);
            unsigned long long size = cli_line_closed(&line);
            // a repository under this one shows in its changes as a folder of new files
            unsigned long long trimmed = size;
            while ((trimmed > 0ull) && (path[trimmed - 1ull] == '/'))
            {
                trimmed -= 1ull;
            }
            const char *const owner = cli_git_owner(repos, count, path, trimmed);
            if ((size != 0ull) && (owner != NULL) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = owner, .other = repo, .cap = CLI_CELLS_POOL})))
            {
                found->item[found->count].path = cli_git_text_put(&found->text, path, size);
                found->item[found->count].state = cli_git_state_of(index, tree);
                found->count += 1u;
            }
        }
        record = next;
    }
    cli_git_given(gave);
}

void cli_git_changed(const char *root, const char *const *repos, unsigned int count, int named, CliChangedList *found)
{
    static CliRepositories s_repositories;
    static const char *s_paths[CLI_GIT_REPOSITORIES];
    static CliLock s_lock = CLI_LOCK_INIT;
    found->count = 0u;
    cli_git_text_begun(&found->text);
    cli_lock_take(&s_lock);
    if (!named)
    {
        cli_git_repositories(root, &s_repositories);
        for (unsigned int at = 0u; at < s_repositories.count; at += 1u)
        {
            s_paths[at] = cli_git_text_of(&s_repositories.text, s_repositories.item[at].path);
        }
        repos = s_paths;
        count = s_repositories.count;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char folder[CLI_PATH_LONGEST];
        CliLine line = {folder, sizeof(folder), 0ull};
        cli_line_text(&line, root);
        if (repos[at][0] != '\0')
        {
            cli_line_character(&line, '/');
            cli_line_text(&line, repos[at]);
        }
        if (cli_line_closed(&line) != 0ull)
        {
            cli_changed_at(folder, repos[at], repos, count, found);
        }
    }
    cli_git_text_given(&s_repositories.text);
    cli_lock_give(&s_lock);
}
