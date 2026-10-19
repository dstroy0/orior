// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_commit.c: chosen files committed, whole or in part, a commit to each repository that holds some; a file a merge
// left in conflict marked resolved; and a file put back as the last commit left it
#include "git_internal.h"

// the most files a commit is chosen, and the most bytes of its message
#define CLI_GIT_CHOSEN 4096u

#define CLI_GIT_MESSAGE (1u << 16u)

// What a commit is made of, held under the lock: each chosen path's repository, the paths as git is given them from
// the repository's folder, the words git is run with, and the message trimmed
typedef struct
{
    unsigned int owner[CLI_GIT_CHOSEN];
    int done[CLI_GIT_CHOSEN];
    char shown[CLI_GIT_CHOSEN][CLI_PATH_LONGEST];
    const char *words[CLI_GIT_CHOSEN + 8u];
    const char *own_whole[CLI_GIT_CHOSEN];
    CliGitPart own_parts[CLI_GIT_CHOSEN];
    char message[CLI_GIT_MESSAGE];
} CliCommitting;

static CliCommitting s_committing;
static CliRepositories s_repositories;
static const char *s_repos[CLI_GIT_REPOSITORIES];
static CliLock s_lock = CLI_LOCK_INIT;

// `message` trimmed into the message held: 1, or 0 with why said where nothing is left
static int cli_message(const char *message, CliLine *said)
{
    const char *from = message;
    const char *to = message + cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL});
    cli_git_trimmed(&from, &to);
    CliLine line = {s_committing.message, CLI_GIT_MESSAGE, 0ull};
    cli_line_counted(&line, from, (unsigned long long)(to - from));
    if ((from == to) || (cli_line_closed(&line) == 0ull))
    {
        cli_line_text(said, (from == to) ? "a commit needs a message" : "the commit's message is too long");
        return 0;
    }
    return 1;
}

// the line of what git said of a commit, the one that starts with `[`, else its first, into `out`
static void cli_commit_line(const char *said, unsigned long long length, CliLine *out)
{
    const char *first = NULL;
    unsigned long long first_length = 0ull;
    const char *line = said;
    const char *const end = said + length;
    while (line < end)
    {
        const char *stop = line;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const unsigned long long size =
            (unsigned long long)(stop - line) - (((stop > line) && (stop[-1] == '\r')) ? 1ull : 0ull);
        if (first == NULL)
        {
            first = line;
            first_length = size;
        }
        if ((size != 0ull) && (line[0] == '['))
        {
            cli_line_counted(out, line, size);
            return;
        }
        line = stop + 1;
    }
    if (first != NULL)
    {
        cli_line_counted(out, first, first_length);
    }
}

// `paths`, `count` of them, given as git is given them, `./` before each, put after `words` from `at`: the count after
static unsigned int cli_shown_words(const char *const *paths, unsigned int count, unsigned int first, unsigned int at)
{
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        char *const shown = s_committing.shown[first + index];
        CliLine line = {shown, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, "./");
        cli_line_text(&line, paths[index]);
        cli_line_closed(&line);
        s_committing.words[at] = shown;
        at += 1u;
    }
    return at;
}

// the files at `paths` committed with the message held, each as it stands, whether changed, new or gone, every other
// file left as it was, in the repository at `folder`: git's line for the commit into `out`, 1, or 0 with why said
static int cli_commit_in(const char *folder, const char *const *paths, unsigned int count, CliLine *out, CliLine *said)
{
    if (count == 0u)
    {
        cli_line_text(said, "no file is chosen to commit");
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char joined[CLI_PATH_LONGEST];
        if (!cli_inside(folder, paths[at], joined, said))
        {
            return 0;
        }
    }
    static char s_said[CLI_GIT_MESSAGE];
    CliLine ran = {s_said, sizeof(s_said), 0ull};
    s_committing.words[0] = "add";
    s_committing.words[1] = "-A";
    s_committing.words[2] = "--";
    unsigned int words = cli_shown_words(paths, count, 0u, 3u);
    if (!cli_git_run(folder, s_committing.words, words, &ran, said))
    {
        return 0;
    }
    ran.at = 0ull;
    s_committing.words[0] = "commit";
    s_committing.words[1] = "-m";
    s_committing.words[2] = s_committing.message;
    s_committing.words[3] = "--";
    words = cli_shown_words(paths, count, 0u, 4u);
    if (!cli_git_run(folder, s_committing.words, words, &ran, said))
    {
        return 0;
    }
    cli_commit_line(s_said, ran.at, out);
    return 1;
}

// the files of `whole` as each stands and the files of `parts` with the text each is given committed with the message
// held, everything else left as the last commit had it, in what is staged as well: the commit is made from an index of
// its own, read from the last commit; after it, the tree's index takes the commit's text for each file committed, and a
// file committed in part keeps the rest of its changes unstaged. git's line for the commit into `out`, 1, or 0 with why
// said
static int cli_commit_parts_in(const char *folder, const char *const *whole, unsigned int whole_count,
                               const CliGitPart *parts, unsigned int part_count, CliLine *out, CliLine *said)
{
    if ((whole_count == 0u) && (part_count == 0u))
    {
        cli_line_text(said, "no file is chosen to commit");
        return 0;
    }
    for (unsigned int at = 0u; at < (whole_count + part_count); at += 1u)
    {
        char joined[CLI_PATH_LONGEST];
        if (!cli_inside(folder, (at < whole_count) ? whole[at] : parts[at - whole_count].path, joined, said))
        {
            return 0;
        }
    }
    static const char *const s_where[3] = {"rev-parse", "--git-path", "orior-commit-index"};
    char named[CLI_PATH_LONGEST];
    CliLine where = {named, sizeof(named), 0ull};
    if (!cli_git_run(folder, s_where, 3u, &where, said) || (cli_line_closed(&where) == 0ull))
    {
        return 0;
    }
    char index[CLI_PATH_LONGEST];
    const int absolute = (named[0] == '/') || (named[0] == '\\') || ((named[0] != '\0') && (named[1] == ':'));
    CliLine joined = {index, sizeof(index), 0ull};
    cli_line_text(&joined, absolute ? "" : folder);
    cli_line_text(&joined, absolute ? "" : "/");
    cli_line_text(&joined, named);
    cli_line_closed(&joined);
    static char s_said[CLI_GIT_MESSAGE];
    CliLine ran = {s_said, sizeof(s_said), 0ull};
    static const char *const s_head[2] = {"read-tree", "HEAD"};
    static const char *const s_empty[2] = {"read-tree", "--empty"};
    char unsaid[CLI_PATH_LONGEST];
    CliLine ignored = {unsaid, sizeof(unsaid), 0ull};
    int made = cli_git_ran(folder, s_head, 2u, index, NULL, 0ull, &ran, &ignored) ||
               cli_git_ran(folder, s_empty, 2u, index, NULL, 0ull, &ran, said);
    if (made && (whole_count != 0u))
    {
        ran.at = 0ull;
        s_committing.words[0] = "add";
        s_committing.words[1] = "-A";
        s_committing.words[2] = "--";
        const unsigned int words = cli_shown_words(whole, whole_count, 0u, 3u);
        made = cli_git_ran(folder, s_committing.words, words, index, NULL, 0ull, &ran, said);
    }
    for (unsigned int at = 0u; made && (at < part_count); at += 1u)
    {
        static const char *const s_hash[3] = {"hash-object", "-w", "--stdin"};
        char blob[CLI_NAME_LONGEST];
        CliLine hashed = {blob, sizeof(blob), 0ull};
        made = cli_git_ran(folder, s_hash, 3u, index, parts[at].text, parts[at].length, &hashed, said) &&
               (cli_line_closed(&hashed) != 0ull);
        char listed[CLI_PATH_LONGEST * 2u];
        CliLine listing = {listed, sizeof(listed), 0ull};
        const char *const ls[4] = {"ls-files", "-s", "--", parts[at].path};
        made = made && cli_git_ran(folder, ls, 4u, index, NULL, 0ull, &listing, said);
        cli_line_closed(&listing);
        // the mode the index gives the file, its first word, else a plain file's
        unsigned long long mode_length = 0ull;
        while ((listed[mode_length] != '\0') && (listed[mode_length] != ' ') && (listed[mode_length] != '\t') &&
               (listed[mode_length] != '\n'))
        {
            mode_length += 1ull;
        }
        char info[CLI_PATH_LONGEST + CLI_NAME_LONGEST];
        CliLine cacheinfo = {info, sizeof(info), 0ull};
        if (mode_length != 0ull)
        {
            cli_line_counted(&cacheinfo, listed, mode_length);
        }
        else
        {
            cli_line_text(&cacheinfo, "100644");
        }
        cli_line_character(&cacheinfo, ',');
        cli_line_text(&cacheinfo, blob);
        cli_line_character(&cacheinfo, ',');
        cli_line_text(&cacheinfo, parts[at].path);
        const char *const update[4] = {"update-index", "--add", "--cacheinfo", info};
        made = made && (cli_line_closed(&cacheinfo) != 0ull) &&
               cli_git_ran(folder, update, 4u, index, NULL, 0ull, &ran, said);
    }
    if (made)
    {
        ran.at = 0ull;
        const char *const commit[3] = {"commit", "-m", s_committing.message};
        made = cli_git_ran(folder, commit, 3u, index, NULL, 0ull, &ran, said);
    }
    query_removed(index);
    if (!made)
    {
        return 0;
    }
    const unsigned long long told = ran.at;
    s_committing.words[0] = "reset";
    s_committing.words[1] = "-q";
    s_committing.words[2] = "--";
    unsigned int words = cli_shown_words(whole, whole_count, 0u, 3u);
    for (unsigned int at = 0u; at < part_count; at += 1u)
    {
        const char *const path = parts[at].path;
        words = cli_shown_words(&path, 1u, whole_count + at, words);
    }
    char reset_said[CLI_PATH_LONGEST];
    CliLine reset = {reset_said, sizeof(reset_said), 0ull};
    if (!cli_git_run(folder, s_committing.words, words, &reset, said))
    {
        return 0;
    }
    cli_commit_line(s_said, told, out);
    return 1;
}

// git's lines of the commits made so far and `error` after them, trimmed, into `said`
static void cli_failed_after(const CliLine *out, unsigned long long from, const char *error, unsigned long long length,
                             CliLine *said)
{
    static char s_joined[CLI_GIT_MESSAGE * 2u];
    CliLine joined = {s_joined, sizeof(s_joined), 0ull};
    cli_line_counted(&joined, out->out + from, out->at - from);
    cli_line_character(&joined, '\n');
    cli_line_counted(&joined, error, length);
    const char *start = s_joined;
    const char *stop = s_joined + joined.at;
    cli_git_trimmed(&start, &stop);
    cli_line_counted(said, start, (unsigned long long)(stop - start));
}

// the chosen paths put each with the repository of the tree that holds it, and the repositories read: 1, or 0 with
// why said where no repository holds one
static int cli_owned(const char *root, const char *const *paths, unsigned int count, CliLine *said)
{
    if (count > CLI_GIT_CHOSEN)
    {
        cli_line_text(said, "more files are chosen than one commit takes");
        return 0;
    }
    cli_git_repositories(root, &s_repositories);
    for (unsigned int at = 0u; at < s_repositories.count; at += 1u)
    {
        s_repos[at] = cli_git_text_of(&s_repositories.text, s_repositories.item[at].path);
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const owner =
            cli_git_owner(s_repos, s_repositories.count, paths[at],
                          cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}));
        if (owner == NULL)
        {
            cli_line_text(said, "no repository holds ");
            cli_line_text(said, paths[at]);
            return 0;
        }
        for (unsigned int repo = 0u; repo < s_repositories.count; repo += 1u)
        {
            s_committing.owner[at] = (s_repos[repo] == owner) ? repo : s_committing.owner[at];
        }
        s_committing.done[at] = 0;
    }
    return 1;
}

// the folder of repository `repo` of the tree into `folder`, which holds CLI_PATH_LONGEST
static void cli_repo_folder(const char *root, unsigned int repo, char *folder)
{
    CliLine line = {folder, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, root);
    if (s_repos[repo][0] != '\0')
    {
        cli_line_character(&line, '/');
        cli_line_text(&line, s_repos[repo]);
    }
    cli_line_closed(&line);
}

// a path of the tree as its repository's folder gives it
static const char *cli_short(const char *path, unsigned int repo)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_repos[repo], .cap = CLI_CELLS_POOL});
    return (length == 0ull) ? path : (path + length + 1ull);
}

int cli_git_commit(const char *root, const char *message, const char *const *paths, unsigned int count, CliLine *out,
                   CliLine *said)
{
    if (count == 0u)
    {
        cli_line_text(said, "no file is chosen to commit");
        return 0;
    }
    cli_lock_take(&s_lock);
    int ok = cli_owned(root, paths, count, said);
    const unsigned long long from = out->at;
    for (unsigned int first = 0u; ok && (first < count); first += 1u)
    {
        if (s_committing.done[first])
        {
            continue;
        }
        const unsigned int repo = s_committing.owner[first];
        unsigned int group = 0u;
        for (unsigned int at = first; at < count; at += 1u)
        {
            if (!s_committing.done[at] && (s_committing.owner[at] == repo))
            {
                s_committing.own_whole[group] = cli_short(paths[at], repo);
                s_committing.done[at] = 1;
                group += 1u;
            }
        }
        char folder[CLI_PATH_LONGEST];
        cli_repo_folder(root, repo, folder);
        char error[CLI_PATH_LONGEST];
        CliLine why = {error, sizeof(error), 0ull};
        cli_line_text(out, (out->at != from) ? "\n" : "");
        const unsigned long long line_at = out->at;
        if (!cli_message(message, &why) || !cli_commit_in(folder, s_committing.own_whole, group, out, &why))
        {
            out->at = (line_at > from) ? (line_at - 1ull) : line_at;
            cli_failed_after(out, from, error, why.at, said);
            ok = 0;
        }
    }
    cli_lock_give(&s_lock);
    return ok;
}

int cli_git_commit_parts(const char *root, const char *message, const char *const *whole, unsigned int whole_count,
                         const CliGitPart *parts, unsigned int part_count, CliLine *out, CliLine *said)
{
    if ((whole_count == 0u) && (part_count == 0u))
    {
        cli_line_text(said, "no file is chosen to commit");
        return 0;
    }
    static const char *s_paths[CLI_GIT_CHOSEN];
    cli_lock_take(&s_lock);
    const unsigned int count = whole_count + part_count;
    for (unsigned int at = 0u; (at < count) && (at < CLI_GIT_CHOSEN); at += 1u)
    {
        s_paths[at] = (at < whole_count) ? whole[at] : parts[at - whole_count].path;
    }
    int ok = cli_owned(root, s_paths, count, said);
    const unsigned long long from = out->at;
    for (unsigned int first = 0u; ok && (first < count); first += 1u)
    {
        if (s_committing.done[first])
        {
            continue;
        }
        const unsigned int repo = s_committing.owner[first];
        unsigned int own_whole = 0u;
        unsigned int own_parts = 0u;
        for (unsigned int at = first; at < count; at += 1u)
        {
            if (s_committing.done[at] || (s_committing.owner[at] != repo))
            {
                continue;
            }
            s_committing.done[at] = 1;
            // a path of `whole` that is a part's as well is committed as the part
            unsigned int part = part_count;
            for (unsigned int each = 0u; (part == part_count) && (each < part_count); each += 1u)
            {
                part = (cellul.eq(
                           &(CatenaFinitaCfg){.src = parts[each].path, .other = s_paths[at], .cap = CLI_CELLS_POOL}))
                           ? each
                           : part;
            }
            if (part < part_count)
            {
                s_committing.own_parts[own_parts] =
                    (CliGitPart){cli_short(s_paths[at], repo), parts[part].text, parts[part].length};
                own_parts += 1u;
            }
            else
            {
                s_committing.own_whole[own_whole] = cli_short(s_paths[at], repo);
                own_whole += 1u;
            }
        }
        char folder[CLI_PATH_LONGEST];
        cli_repo_folder(root, repo, folder);
        char error[CLI_PATH_LONGEST];
        CliLine why = {error, sizeof(error), 0ull};
        cli_line_text(out, (out->at != from) ? "\n" : "");
        const unsigned long long line_at = out->at;
        if (!cli_message(message, &why) || !cli_commit_parts_in(folder, s_committing.own_whole, own_whole,
                                                                s_committing.own_parts, own_parts, out, &why))
        {
            out->at = (line_at > from) ? (line_at - 1ull) : line_at;
            cli_failed_after(out, from, error, why.at, said);
            ok = 0;
        }
    }
    cli_lock_give(&s_lock);
    return ok;
}

int cli_git_resolve(const char *root, const char *path, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    char shown[CLI_PATH_LONGEST];
    if (!cli_git_beside(root, path, folder, shown, said))
    {
        return 0;
    }
    const char *const args[3] = {"add", "--", shown};
    char told[CLI_PATH_LONGEST];
    CliLine out = {told, sizeof(told), 0ull};
    return cli_git_run(folder, args, 3u, &out, said);
}

int cli_git_rollback(const char *root, const char *path, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    char shown[CLI_PATH_LONGEST];
    if (!cli_git_beside(root, path, folder, shown, said))
    {
        return 0;
    }
    char head[CLI_PATH_LONGEST + 8u];
    CliLine named = {head, sizeof(head), 0ull};
    cli_line_text(&named, "HEAD:");
    cli_line_text(&named, shown);
    cli_line_closed(&named);
    const char *const held[3] = {"cat-file", "-e", head};
    CliGitGave *const gave = cli_git_taken();
    const int holds = cli_git_asked(folder, held, 3u, gave);
    cli_git_given(gave);
    if (!holds)
    {
        cli_line_text(said, path);
        cli_line_text(said, " is not in the last commit: delete it from the tree to take it back");
        return 0;
    }
    const char *const args[6] = {"restore", "--source=HEAD", "--staged", "--worktree", "--", shown};
    char told[CLI_PATH_LONGEST];
    CliLine out = {told, sizeof(told), 0ull};
    return cli_git_run(folder, args, 6u, &out, said);
}
