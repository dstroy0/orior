// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_lines.c: the commit that last changed each line of a file as the editor has it, read from git blame's porcelain
#include "git_internal.h"

// the places of the commits found, by their id, twice as many as the commits a history holds
#define CLI_GIT_PLACES (CLI_GIT_LINE_COMMITS * 2u)

#define CLI_GIT_NO_PLACE 0xffffffffu

// whether `length` bytes of `text` are all hex digits
static int cli_all_hex(const char *text, unsigned long long length)
{
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const char letter = text[at];
        if (!(((letter >= '0') && (letter <= '9')) || ((letter >= 'a') && (letter <= 'f')) ||
              ((letter >= 'A') && (letter <= 'F'))))
        {
            return 0;
        }
    }
    return 1;
}

// A path git gives from the repository's folder made the tree's: `strip` taken from its start and `add` put there
typedef struct
{
    const char *strip;
    unsigned long long strip_length;
    const char *add;
    unsigned long long add_length;
} CliUnder;

// a path git gives kept as the tree's: 1 with its place into `at`, or 0 where it does not start with what is stripped
static int cli_under(CliLineHistory *found, const CliUnder *under, const char *path, unsigned long long length,
                     unsigned int *at)
{
    if ((length < under->strip_length) ||
        (memor.cmp(&(MemoriaCfg){.src = path, .other = under->strip, .bytes = under->strip_length}) != 0))
    {
        return 0;
    }
    char joined[CLI_PATH_LONGEST];
    CliLine line = {joined, sizeof(joined), 0ull};
    cli_line_counted(&line, under->add, under->add_length);
    cli_line_counted(&line, path + under->strip_length, length - under->strip_length);
    const unsigned long long size = cli_line_closed(&line);
    *at = cli_git_text_put(&found->text, joined, size);
    return 1;
}

// `git blame --porcelain` read: a line of a commit's id and the line's numbers before each line of the file, the
// commit's own lines the first time it comes, and the line itself after a tab
static void cli_history_of(const char *out, unsigned long long size, const CliUnder *under, CliLineHistory *found)
{
    static unsigned int s_places[CLI_GIT_PLACES];
    for (unsigned int at = 0u; at < CLI_GIT_PLACES; at += 1u)
    {
        s_places[at] = CLI_GIT_NO_PLACE;
    }
    unsigned int at = 0u;
    const char *line = out;
    const char *const end = out + size;
    while (line < end)
    {
        const char *stop = line;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        const unsigned long long length = (unsigned long long)(stop - line);
        if ((length > 0ull) && (line[0] == '\t'))
        {
            int held = 0;
            if (at < found->commits)
            {
                const char *const id = cli_git_text_of(&found->text, found->commit[at].id);
                for (const char *digit = id; !held && (digit[0] != '\0'); digit += 1)
                {
                    held = digit[0] != '0';
                }
            }
            if (found->lines < CLI_GIT_LINES)
            {
                // a commit's place is below CLI_GIT_LINE_COMMITS, which an int holds
                found->line[found->lines] = held ? (int)at : -1;
                found->lines += 1u;
            }
            line = next;
            continue;
        }
        const char *space = line;
        while ((space < stop) && (space[0] != ' '))
        {
            space += 1;
        }
        const char *const key = line;
        const unsigned long long key_length = (unsigned long long)(space - line);
        const char *const value = (space < stop) ? (space + 1) : stop;
        const unsigned long long value_length = (unsigned long long)(stop - value);
        line = next;
        if ((key_length == 40ull) && cli_all_hex(key, 40ull))
        {
            // FNV-1a over the id, its slot in the places
            unsigned int hash = 2166136261u;
            for (unsigned int digit = 0u; digit < 40u; digit += 1u)
            {
                hash = (hash ^ (unsigned char)key[digit]) * 16777619u;
            }
            unsigned int slot = hash % CLI_GIT_PLACES;
            while ((s_places[slot] != CLI_GIT_NO_PLACE) &&
                   (memor.cmp(&(MemoriaCfg){.src = cli_git_text_of(&found->text, found->commit[s_places[slot]].id),
                                            .other = key,
                                            .bytes = 40u}) != 0))
            {
                slot = (slot + 1u) % CLI_GIT_PLACES;
            }
            if ((s_places[slot] == CLI_GIT_NO_PLACE) && (found->commits < CLI_GIT_LINE_COMMITS))
            {
                CliLineCommit *const commit = &found->commit[found->commits];
                memor.set(&(MemoriaCfg){.dst = commit, .val = 0, .bytes = sizeof(*commit)});
                commit->id = cli_git_text_put(&found->text, key, 40ull);
                s_places[slot] = found->commits;
                found->commits += 1u;
            }
            at = (s_places[slot] != CLI_GIT_NO_PLACE) ? s_places[slot] : found->commits;
            continue;
        }
        if (at >= found->commits)
        {
            continue;
        }
        CliLineCommit *const commit = &found->commit[at];
        if ((key_length == 6ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "author", .bytes = 6u}) == 0))
        {
            commit->author = cli_git_text_put(&found->text, value, value_length);
        }
        else if ((key_length == 11ull) &&
                 (memor.cmp(&(MemoriaCfg){.src = key, .other = "author-time", .bytes = 11u}) == 0))
        {
            commit->when = 0ll;
            cli_git_seconds(value, value_length, &commit->when);
        }
        else if ((key_length == 7ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "summary", .bytes = 7u}) == 0))
        {
            commit->subject = cli_git_text_put(&found->text, value, value_length);
        }
        else if ((key_length == 8ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "filename", .bytes = 8u}) == 0))
        {
            commit->has_path = cli_under(found, under, value, value_length, &commit->path);
        }
        else if ((key_length == 8ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "previous", .bytes = 8u}) == 0))
        {
            const char *gap = value;
            while ((gap < stop) && (gap[0] != ' '))
            {
                gap += 1;
            }
            commit->parent = cli_git_text_put(&found->text, value, (unsigned long long)(gap - value));
            commit->has_parent = 1;
            const char *const path = (gap < stop) ? (gap + 1) : stop;
            commit->has_was = cli_under(found, under, path, (unsigned long long)(stop - path), &commit->was);
        }
    }
}

// whether `text` ends with `tail`
static int cli_ends_with(const char *text, const char *tail)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = tail, .cap = CLI_CELLS_POOL});
    return (length >= size) &&
           (memor.cmp(&(MemoriaCfg){.src = text + length - size, .other = tail, .bytes = size}) == 0);
}

int cli_git_line_history(const char *root, const char *file, const char *text, unsigned long long length,
                         CliLineHistory *found, CliLine *said)
{
    static const char *const s_prefix[2] = {"rev-parse", "--show-prefix"};
    static CliLock s_lock = CLI_LOCK_INIT;
    found->commits = 0u;
    found->lines = 0u;
    cli_git_text_begun(&found->text);
    char folder[CLI_PATH_LONGEST];
    char shown[CLI_PATH_LONGEST];
    if (!cli_git_beside(root, file, folder, shown, said))
    {
        return 0;
    }
    // git names a file from its repository's folder, the tree names it from its own: the folder's path from each ends
    // the same, and what goes before it in either path names the other's folder
    char from_repo[CLI_PATH_LONGEST];
    CliGitGave *const gave = cli_git_taken();
    const int prefixed = cli_git_asked(folder, s_prefix, 2u, gave);
    const char *from = gave->out;
    const char *to = gave->out + gave->out_size;
    cli_git_trimmed(&from, &to);
    CliLine repo_line = {from_repo, sizeof(from_repo), 0ull};
    cli_line_counted(&repo_line, from, prefixed ? (unsigned long long)(to - from) : 0ull);
    cli_line_closed(&repo_line);
    char from_tree[CLI_PATH_LONGEST];
    char relative[CLI_PATH_LONGEST];
    cli_relative(root, folder, relative);
    CliLine tree_line = {from_tree, sizeof(from_tree), 0ull};
    cli_line_text(&tree_line, relative);
    cli_line_text(&tree_line, (relative[0] != '\0') ? "/" : "");
    cli_line_closed(&tree_line);
    CliUnder under = {"", 0ull, "", 0ull};
    if (cli_ends_with(from_tree, from_repo))
    {
        under.add = from_tree;
        under.add_length = cellul.len(&(CatenaFinitaCfg){.src = from_tree, .cap = CLI_CELLS_POOL}) -
                           cellul.len(&(CatenaFinitaCfg){.src = from_repo, .cap = CLI_CELLS_POOL});
    }
    else if (cli_ends_with(from_repo, from_tree))
    {
        under.strip = from_repo;
        under.strip_length = cellul.len(&(CatenaFinitaCfg){.src = from_repo, .cap = CLI_CELLS_POOL}) -
                             cellul.len(&(CatenaFinitaCfg){.src = from_tree, .cap = CLI_CELLS_POOL});
    }
    else
    {
        under.strip = from_repo;
        under.strip_length = cellul.len(&(CatenaFinitaCfg){.src = from_repo, .cap = CLI_CELLS_POOL});
    }
    const char *const args[6] = {"blame", "--porcelain", "--contents", "-", "--", shown};
    char why[CLI_NAME_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    const int blamed = cli_git_gave(folder, args, 6u, CLI_GIT_LOCKS_OFF, NULL, text, length, gave, &unstarted) &&
                       gave->ok && gave->out_whole;
    if (blamed)
    {
        cli_lock_take(&s_lock);
        cli_history_of(gave->out, gave->out_size, &under, found);
        cli_lock_give(&s_lock);
    }
    cli_git_given(gave);
    if (!blamed)
    {
        cli_line_text(said, "git has no history of ");
        cli_line_text(said, file);
    }
    return blamed;
}
