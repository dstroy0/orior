// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_commit.c: chosen files committed, whole or in part, a commit to each repository that holds some; a file a merge
// left in conflict marked resolved; and a file put back as the last commit left it
#include "git_internal.h"

// the most files a commit is chosen, and the most bytes of its message
#define CLI_GIT_CHOSEN 4096u

#define CLI_GIT_MESSAGE (1u << 16u)

// the bytes the line of what git says of a commit is begun with, doubling as it says more
#define CLI_GIT_SAID_FIRST 4096u

// What a commit is made of, held under the lock in one cell sized to the files chosen and the message: the files of one
// repository in part and whole, the words git is run with, the chosen paths, each chosen path's
// repository and whether it is committed yet, the paths as git is given them from the repository's folder, and the
// message trimmed, in a room of `message_room` bytes
typedef struct
{
    CliGitPart *own_parts;
    const char **words;
    const char **own_whole;
    const char **paths;
    unsigned int *owner;
    int *done;
    char (*shown)[CLI_PATH_LONGEST];
    char *message;
    unsigned long long message_room;
} CliCommitting;

static CliCommitting s_committing;
static CliRepositories s_repositories;
static const char *s_repos[CLI_GIT_REPOSITORIES];
static CliLock s_lock = CLI_LOCK_INIT;

// the commit's room taken for `count` chosen paths and `message` trimmed, the message's room CLI_GIT_MESSAGE at the
// most: 1, or 0 with why said where more files are chosen than a commit takes or the cellblock could not meet it
static int cli_committing_taken(const char *message, unsigned int count, CliLine *said)
{
    if (count > CLI_GIT_CHOSEN)
    {
        cli_line_text(said, "more files are chosen than one commit takes");
        return 0;
    }
    const char *from = message;
    const char *to = message + cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL});
    cli_git_trimmed(&from, &to);
    // the trimmed message's length, a difference of two places in it, `to` never before `from`
    const unsigned long long length = (unsigned long long)(to - from);
    const unsigned long long room = (length < CLI_GIT_MESSAGE) ? (length + 1ull) : CLI_GIT_MESSAGE;
    const unsigned long long many = count;
    const unsigned long long parts = many * sizeof(CliGitPart);
    const unsigned long long pointers = ((many + 8ull) + (many * 2ull)) * sizeof(const char *);
    const unsigned long long marks = many * (sizeof(unsigned int) + sizeof(int));
    char *const cell =
        cli_cell_taken_by(&g_cli_git_cells, parts + pointers + marks + (many * CLI_PATH_LONGEST) + room, "a commit");
    if (cell == NULL)
    {
        cli_line_text(said, "a commit's room could not be taken");
        return 0;
    }
    // each list starts on a boundary its type keeps: the cell's on CLI_CELL_ALIGN, the parts' and the pointers'
    // lengths multiples of a pointer's, and the marks' of an int's
    s_committing.own_parts = (CliGitPart *)(void *)cell;
    s_committing.words = (const char **)(void *)(cell + parts);
    s_committing.own_whole = s_committing.words + (many + 8ull);
    s_committing.paths = s_committing.own_whole + many;
    s_committing.owner = (unsigned int *)(void *)(cell + parts + pointers);
    s_committing.done = (int *)(void *)(s_committing.owner + many);
    s_committing.shown = (char (*)[CLI_PATH_LONGEST])(void *)(cell + parts + pointers + marks);
    s_committing.message = cell + parts + pointers + marks + (many * CLI_PATH_LONGEST);
    s_committing.message_room = room;
    return 1;
}

// the commit's room given back
static void cli_committing_given(void)
{
    cli_cell_given_by(&g_cli_git_cells, s_committing.own_parts);
    s_committing = (CliCommitting){0};
}

// `message` trimmed into the message held: 1, or 0 with why said where nothing is left
static int cli_message(const char *message, CliLine *said)
{
    const char *from = message;
    const char *to = message + cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL});
    cli_git_trimmed(&from, &to);
    CliLine line = {s_committing.message, s_committing.message_room, 0ull};
    cli_line_counted(&line, from, (unsigned long long)(to - from));
    if ((from == to) || (cli_line_closed(&line) == 0ull))
    {
        cli_line_text(said, (from == to) ? "a commit needs a message" : "the commit's message is too long");
        return 0;
    }
    return 1;
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
    CliLine ran;
    if (!cli_line_growing(&ran, CLI_GIT_SAID_FIRST, "what git says of a commit"))
    {
        cli_line_text(said, "a commit's room could not be taken");
        return 0;
    }
    s_committing.words[0] = "add";
    s_committing.words[1] = "-A";
    s_committing.words[2] = "--";
    unsigned int words = cli_shown_words(paths, count, 0u, 3u);
    int made = cli_git_run(folder, s_committing.words, words, &ran, said);
    if (made)
    {
        ran.at = 0ull;
        s_committing.words[0] = "commit";
        s_committing.words[1] = "-m";
        s_committing.words[2] = s_committing.message;
        s_committing.words[3] = "--";
        words = cli_shown_words(paths, count, 0u, 4u);
        made = cli_git_run(folder, s_committing.words, words, &ran, said);
    }
    if (made)
    {
        cli_git_commit_line(ran.out, ran.at, out);
    }
    cli_cell_given(ran.out);
    return made;
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
    CliLine ran;
    if (!cli_line_growing(&ran, CLI_GIT_SAID_FIRST, "what git says of a commit"))
    {
        cli_line_text(said, "a commit's room could not be taken");
        return 0;
    }
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
    if (made)
    {
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
        made = cli_git_run(folder, s_committing.words, words, &reset, said);
    }
    if (made)
    {
        cli_git_commit_line(ran.out, ran.at, out);
    }
    cli_cell_given(ran.out);
    return made;
}

// the chosen paths put each with the repository of the tree that holds it, and the repositories read: 1, or 0 with
// why said where no repository holds one
static int cli_owned(const char *root, const char *const *paths, unsigned int count, CliLine *said)
{
    // the repositories read for this commit, given back as it ends
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
    int ok = cli_committing_taken(message, count, said) && cli_owned(root, paths, count, said);
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
            cli_git_failed_after(out, from, error, why.at, said);
            ok = 0;
        }
    }
    cli_git_text_given(&s_repositories.text);
    cli_committing_given();
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
    cli_lock_take(&s_lock);
    const unsigned int count = whole_count + part_count;
    int ok = cli_committing_taken(message, count, said);
    for (unsigned int at = 0u; ok && (at < count); at += 1u)
    {
        s_committing.paths[at] = (at < whole_count) ? whole[at] : parts[at - whole_count].path;
    }
    ok = ok && cli_owned(root, s_committing.paths, count, said);
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
                part = (cellul.eq(&(CatenaFinitaCfg){
                           .src = parts[each].path, .other = s_committing.paths[at], .cap = CLI_CELLS_POOL}))
                           ? each
                           : part;
            }
            if (part < part_count)
            {
                s_committing.own_parts[own_parts] =
                    (CliGitPart){cli_short(s_committing.paths[at], repo), parts[part].text, parts[part].length};
                own_parts += 1u;
            }
            else
            {
                s_committing.own_whole[own_whole] = cli_short(s_committing.paths[at], repo);
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
            cli_git_failed_after(out, from, error, why.at, said);
            ok = 0;
        }
    }
    cli_git_text_given(&s_repositories.text);
    cli_committing_given();
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
