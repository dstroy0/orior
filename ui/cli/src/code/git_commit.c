// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_commit.c: chosen files committed, whole or in part, a commit to each repository that contains some; a file a
// merge left in conflict marked resolved; and a file put back as the last commit left it
#include "../text/text.h"
#include "git_internal.h"

// the most files a commit is chosen, and the most bytes of its message
#define CLI_GIT_CHOSEN 4096u

#define CLI_GIT_MESSAGE (1u << 16u)

// the bytes the line of what git reports of a commit is taken with
#define CLI_GIT_OUTPUT_CAPACITY (1ULL << 20u)

// What a commit is made of, guarded by the lock in one cell sized to the files chosen and the message: the files of one
// repository in part and whole, the words git is run with, the chosen paths, each chosen path's
// repository and whether it is committed yet, the paths as git is given them from the repository's folder, and the
// message trimmed, in a capacity of `message_capacity` bytes
typedef struct
{
    CliGitPart *own_parts;
    const char **words;
    const char **own_full_files;
    const char **paths;
    unsigned int *owner;
    int *done;
    char (*displayed)[CLI_PATH_MAX_LEN];
    char *message;
    unsigned long long message_capacity;
} CliCommitting;

static CliCommitting s_committing;
static CliRepositories s_repositories;
static const char *s_repos[CLI_GIT_REPOSITORIES];

typedef struct
{
    const char *message;
    unsigned int count;
    VerbaLinea *err_msg;
    const char *const *paths;
    unsigned int first;
    unsigned int at;
    const char *folder;
    VerbaLinea *out;
    const char *const *whole;
    unsigned int full_files_count;
    const CliGitPart *parts;
    unsigned int part_count;
    const char *root;
    unsigned int repo;
    char *folder_out;
    const char *path;
} GitCommitCtx;
// the commit's capacity taken for `count` chosen paths and `message` trimmed, the message's capacity CLI_GIT_MESSAGE at
// the most: 1, or 0 with why given where more files are chosen than a commit takes or the cellblock could not meet it
EMBED_INLINE int cli_committing_alloc(const GitCommitCtx *args)
{
    const char *message = args->message;
    if (args->count > CLI_GIT_CHOSEN)
    {
        mmgr_line_put(args->err_msg, "more files are chosen than one commit takes");
        return 0;
    }
    const char *from = message;
    const char *to = message + cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL});
    EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
    // the trimmed message's length, a difference of two places in it, `to` never before `from`
    const unsigned long long length = (unsigned long long)(to - from);
    const unsigned long long capacity = (length < CLI_GIT_MESSAGE) ? (length + 1ULL) : CLI_GIT_MESSAGE;
    const unsigned long long many = args->count;
    const unsigned long long parts = many * sizeof(CliGitPart);
    const unsigned long long pointers = ((many + 8ULL) + (many * 2ULL)) * sizeof(const char *);
    const unsigned long long marks = many * (sizeof(unsigned int) + sizeof(int));
    char *const cell = g_cli_cells->persistent_buf_alloc(
        parts + pointers + marks + (many * CLI_PATH_MAX_LEN) + capacity, &g_cli_git_cells);
    if (cell == NULL)
    {
        mmgr_line_put(args->err_msg, "a commit's work state could not be allocated");
        return 0;
    }
    // each list starts on a boundary its type requires: the cell's on MMGR_CARCER_ALIGN, the parts' and the pointers'
    // lengths multiples of a pointer's, and the marks' of an int's
    s_committing.own_parts = (CliGitPart *)(void *)cell;
    s_committing.words = (const char **)(void *)(cell + parts);
    s_committing.own_full_files = s_committing.words + (many + 8ULL);
    s_committing.paths = s_committing.own_full_files + many;
    s_committing.owner = (unsigned int *)(void *)(cell + parts + pointers);
    s_committing.done = (int *)(void *)(s_committing.owner + many);
    s_committing.displayed = (char (*)[CLI_PATH_MAX_LEN])(void *)(cell + parts + pointers + marks);
    s_committing.message = cell + parts + pointers + marks + (many * CLI_PATH_MAX_LEN);
    s_committing.message_capacity = capacity;
    return 1;
}

// the commit's capacity given back
static void cli_committing_free(void)
{
    g_cli_cells->persistent_buf_release(s_committing.own_parts, &g_cli_git_cells);
    s_committing = (CliCommitting){0};
}

// `message` trimmed into the message stored: 1, or 0 with why given where nothing is left
EMBED_INLINE int cli_message(const GitCommitCtx *args)
{
    const char *from = args->message;
    const char *to = args->message + cellul.len(&(CatenaFinitaCfg){.src = args->message, .cap = CLI_CELLS_POOL});
    EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
    VerbaLinea line = {s_committing.message, s_committing.message_capacity, 0ULL};
    mmgr_line_put_n(&line, from, (unsigned long long)(to - from));
    if ((from == to) || (mmgr_line_finish(&line) == 0ULL))
    {
        mmgr_line_put(args->err_msg, (from == to) ? "a commit needs a message" : "the commit's message is too long");
        return 0;
    }
    return 1;
}

// `paths`, `count` of them, given as git is given them, `./` before each, put after `words` from `at`: the count after
EMBED_INLINE unsigned int cli_displayed_words(const GitCommitCtx *args)
{
    unsigned int at = args->at;
    for (unsigned int index = 0u; index < args->count; index += 1u)
    {
        char *const displayed = s_committing.displayed[args->first + index];
        VerbaLinea line = {displayed, CLI_PATH_MAX_LEN, 0ULL};
        mmgr_line_put(&line, "./");
        mmgr_line_put(&line, args->paths[index]);
        mmgr_line_finish(&line);
        s_committing.words[at] = displayed;
        at += 1u;
    }
    return at;
}

// the files at `paths` committed with the message stored, each as it stands, whether changed, new or gone, every other
// file left as it was, in the repository at `folder`: git's line for the commit into `out`, 1, or 0 with why given
EMBED_INLINE int cli_commit_in(const GitCommitCtx *args)
{
    unsigned int count = args->count;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    if (count == 0u)
    {
        mmgr_line_put(err_msg, "no file is chosen to commit");
        return 0;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char joined[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->folder, .relative = args->paths[at],
                        .joined = joined, .err_msg = err_msg))
        {
            return 0;
        }
    }
    char *const run_result_cell = g_cli_cells->temporary_buf_alloc(CLI_GIT_OUTPUT_CAPACITY, &g_cli_git_cells);
    VerbaLinea run_result = mmgr_line_over(run_result_cell, CLI_GIT_OUTPUT_CAPACITY);
    if (run_result.out == NULL)
    {
        mmgr_line_put(err_msg, "a commit's work state could not be allocated");
        return 0;
    }
    s_committing.words[0] = "add";
    s_committing.words[1] = "-A";
    s_committing.words[2] = "--";
    unsigned int words =
        EMBED_CALL(cli_displayed_words, GitCommitCtx, .paths = args->paths, .count = count, .first = 0u, .at = 3u);
    int made = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->folder,
                          .request = s_committing.words, .count = words, .out = &run_result, .err_msg = err_msg);
    if (made)
    {
        run_result.at = 0ULL;
        s_committing.words[0] = "commit";
        s_committing.words[1] = "-m";
        s_committing.words[2] = s_committing.message;
        s_committing.words[3] = "--";
        words =
            EMBED_CALL(cli_displayed_words, GitCommitCtx, .paths = args->paths, .count = count, .first = 0u, .at = 4u);
        made = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->folder,
                          .request = s_committing.words, .count = words, .out = &run_result, .err_msg = err_msg);
    }
    if (made)
    {
        EMBED_CALL(cli_git_length_out_internal.commit_line, CliGitLengthOutCtx, .output = run_result.out,
                   .length = run_result.at, .out = out);
    }
    g_cli_cells->persistent_buf_release(run_result.out, &g_cli_git_cells);
    return made;
}

// the files of `whole` as each stands and the files of `parts` with the text each is given committed with the message
// stored, everything else left as the last commit had it, in what is staged as well: the commit is made from an index
// of its own, read from the last commit; after it, git's index takes the commit's text for each file committed,
// and a file committed in part leaves the rest of its changes unstaged. git's line for the commit into `out`, 1, or 0
// with why given
EMBED_INLINE int cli_commit_parts_in(const GitCommitCtx *args)
{
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    if ((args->full_files_count == 0u) && (args->part_count == 0u))
    {
        mmgr_line_put(err_msg, "no file is chosen to commit");
        return 0;
    }
    for (unsigned int at = 0u; at < (args->full_files_count + args->part_count); at += 1u)
    {
        char joined[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->folder,
                        .relative = (at < args->full_files_count) ? args->whole[at]
                                                                  : args->parts[at - args->full_files_count].path,
                        .joined = joined, .err_msg = err_msg))
        {
            return 0;
        }
    }
    static const char *const s_where[3] = {"rev-parse", "--git-path", "orior-commit-index"};
    char named[CLI_PATH_MAX_LEN];
    VerbaLinea where = {named, sizeof(named), 0ULL};
    if (!EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->folder,
                    .request = s_where, .count = 3u, .out = &where, .err_msg = err_msg) ||
        (mmgr_line_finish(&where) == 0ULL))
    {
        return 0;
    }
    char index[CLI_PATH_MAX_LEN];
    const int absolute = (named[0] == '/') || (named[0] == '\\') || ((named[0] != '\0') && (named[1] == ':'));
    VerbaLinea joined = {index, sizeof(index), 0ULL};
    mmgr_line_put(&joined, absolute ? "" : args->folder);
    mmgr_line_put(&joined, absolute ? "" : "/");
    mmgr_line_put(&joined, named);
    mmgr_line_finish(&joined);
    char *const run_result_cell = g_cli_cells->temporary_buf_alloc(CLI_GIT_OUTPUT_CAPACITY, &g_cli_git_cells);
    VerbaLinea run_result = mmgr_line_over(run_result_cell, CLI_GIT_OUTPUT_CAPACITY);
    if (run_result.out == NULL)
    {
        mmgr_line_put(err_msg, "a commit's work state could not be allocated");
        return 0;
    }
    static const char *const s_head[2] = {"read-tree", "HEAD"};
    static const char *const s_empty[2] = {"read-tree", "--empty"};
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea ignored = {err_buf, sizeof(err_buf), 0ULL};
    int made =
        EMBED_CALL(cli_git_root_internal.run, CliGitRootCtx, .root = args->folder, .request = s_head, .count = 2u,
                   .index = index, .input = NULL, .length = 0ULL, .out = &run_result, .err_msg = &ignored) ||
        EMBED_CALL(cli_git_root_internal.run, CliGitRootCtx, .root = args->folder, .request = s_empty, .count = 2u,
                   .index = index, .input = NULL, .length = 0ULL, .out = &run_result, .err_msg = err_msg);
    if (made && (args->full_files_count != 0u))
    {
        run_result.at = 0ULL;
        s_committing.words[0] = "add";
        s_committing.words[1] = "-A";
        s_committing.words[2] = "--";
        const unsigned int words = EMBED_CALL(cli_displayed_words, GitCommitCtx, .paths = args->whole,
                                              .count = args->full_files_count, .first = 0u, .at = 3u);
        made = EMBED_CALL(cli_git_root_internal.run, CliGitRootCtx, .root = args->folder, .request = s_committing.words,
                          .count = words, .index = index, .input = NULL, .length = 0ULL, .out = &run_result,
                          .err_msg = err_msg);
    }
    for (unsigned int at = 0u; made && (at < args->part_count); at += 1u)
    {
        static const char *const s_hash[3] = {"hash-object", "-w", "--stdin"};
        char blob[CLI_NAME_MAX_LEN];
        VerbaLinea hashed = {blob, sizeof(blob), 0ULL};
        made = EMBED_CALL(cli_git_root_internal.run, CliGitRootCtx, .root = args->folder, .request = s_hash,
                          .count = 3u, .index = index, .input = args->parts[at].text, .length = args->parts[at].length,
                          .out = &hashed, .err_msg = err_msg) &&
               (mmgr_line_finish(&hashed) != 0ULL);
        char list_entries[CLI_PATH_MAX_LEN * 2u];
        VerbaLinea listing = {list_entries, sizeof(list_entries), 0ULL};
        const char *const ls[4] = {"ls-files", "-s", "--", args->parts[at].path};
        made = made &&
               EMBED_CALL(cli_git_root_internal.run, CliGitRootCtx, .root = args->folder, .request = ls, .count = 4u,
                          .index = index, .input = NULL, .length = 0ULL, .out = &listing, .err_msg = err_msg);
        mmgr_line_finish(&listing);
        // the mode the index gives the file, its first word, else a plain file's
        unsigned long long mode_length = 0ULL;
        mode_length = EMBED_CALL(
            cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = list_entries, .at = mode_length,
            .size =
                mode_length + cellul.len(&(CatenaFinitaCfg){.src = list_entries + mode_length, .cap = CLI_CELLS_POOL}),
            .set = " \t\n");
        char info[CLI_PATH_MAX_LEN + CLI_NAME_MAX_LEN];
        VerbaLinea cacheinfo = {info, sizeof(info), 0ULL};
        if (mode_length != 0ULL)
        {
            mmgr_line_put_n(&cacheinfo, list_entries, mode_length);
        }
        else
        {
            mmgr_line_put(&cacheinfo, "100644");
        }
        mmgr_line_ch(&cacheinfo, ',');
        mmgr_line_put(&cacheinfo, blob);
        mmgr_line_ch(&cacheinfo, ',');
        mmgr_line_put(&cacheinfo, args->parts[at].path);
        const char *const update[4] = {"update-index", "--add", "--cacheinfo", info};
        made =
            made && (mmgr_line_finish(&cacheinfo) != 0ULL) &&
            EMBED_CALL(cli_git_root_internal.run, CliGitRootCtx, .root = args->folder, .request = update, .count = 4u,
                       .index = index, .input = NULL, .length = 0ULL, .out = &run_result, .err_msg = err_msg);
    }
    if (made)
    {
        run_result.at = 0ULL;
        const char *const commit[3] = {"commit", "-m", s_committing.message};
        made =
            EMBED_CALL(cli_git_root_internal.run, CliGitRootCtx, .root = args->folder, .request = commit, .count = 3u,
                       .index = index, .input = NULL, .length = 0ULL, .out = &run_result, .err_msg = err_msg);
    }
    EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = index);
    if (made)
    {
        s_committing.words[0] = "reset";
        s_committing.words[1] = "-q";
        s_committing.words[2] = "--";
        unsigned int words = EMBED_CALL(cli_displayed_words, GitCommitCtx, .paths = args->whole,
                                        .count = args->full_files_count, .first = 0u, .at = 3u);
        for (unsigned int at = 0u; at < args->part_count; at += 1u)
        {
            const char *const path = args->parts[at].path;
            words = EMBED_CALL(cli_displayed_words, GitCommitCtx, .paths = &path, .count = 1u,
                               .first = args->full_files_count + at, .at = words);
        }
        char reset_buf[CLI_PATH_MAX_LEN];
        VerbaLinea reset = {reset_buf, sizeof(reset_buf), 0ULL};
        made = EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = args->folder,
                          .request = s_committing.words, .count = words, .out = &reset, .err_msg = err_msg);
    }
    if (made)
    {
        EMBED_CALL(cli_git_length_out_internal.commit_line, CliGitLengthOutCtx, .output = run_result.out,
                   .length = run_result.at, .out = out);
    }
    g_cli_cells->persistent_buf_release(run_result.out, &g_cli_git_cells);
    return made;
}

// the chosen paths put each with the repository of the tree that contains it, and the repositories read: 1, or 0 with
// why given where no repository contains one
EMBED_INLINE int cli_own(const GitCommitCtx *args)
{
    const char *root = args->root;
    unsigned int count = args->count;
    // the repositories read for this commit, given back as it ends
    EMBED_CALL(cli_git_root_found_repositories.repositories, CliGitRootFoundRepositoriesCfg, .root = root,
               .found_repositories_pointer = &s_repositories);
    for (unsigned int at = 0u; at < s_repositories.count; at += 1u)
    {
        s_repos[at] = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_repositories.text,
                                 .at = s_repositories.item[at].path);
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const owner =
            EMBED_CALL(cli_git_count_internal.owner, CliGitCountCtx, .repos = s_repos, .count = s_repositories.count,
                       .path = args->paths[at],
                       .length = cellul.len(&(CatenaFinitaCfg){.src = args->paths[at], .cap = CLI_CELLS_POOL}));
        if (owner == NULL)
        {
            mmgr_line_put(args->err_msg, "no repository contains ");
            mmgr_line_put(args->err_msg, args->paths[at]);
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

// the folder of repository `repo` of the tree into `folder`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void cli_repo_folder(const GitCommitCtx *args)
{
    VerbaLinea line = {args->folder_out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->root);
    if (s_repos[args->repo][0] != '\0')
    {
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, s_repos[args->repo]);
    }
    mmgr_line_finish(&line);
}

// a path of the tree as its repository's folder gives it
EMBED_INLINE const char *cli_short(const GitCommitCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_repos[args->repo], .cap = CLI_CELLS_POOL});
    return (length == 0ULL) ? args->path : (args->path + length + 1ULL);
}

int cli_git_commit(const CliGitRootOutCountCfg *args)
{
    if (args->count == 0u)
    {
        mmgr_line_put(args->err_msg, "no file is chosen to commit");
        return 0;
    }
    int ok = EMBED_CALL(cli_committing_alloc, GitCommitCtx, .message = args->message, .count = args->count,
                        .err_msg = args->err_msg) &&
             EMBED_CALL(cli_own, GitCommitCtx, .root = args->root, .paths = args->paths, .count = args->count,
                        .err_msg = args->err_msg);
    const unsigned long long from = args->out->at;
    for (unsigned int first = 0u; ok && (first < args->count); first += 1u)
    {
        if (s_committing.done[first])
        {
            continue;
        }
        const unsigned int repo = s_committing.owner[first];
        unsigned int group = 0u;
        for (unsigned int at = first; at < args->count; at += 1u)
        {
            if (!s_committing.done[at] && (s_committing.owner[at] == repo))
            {
                s_committing.own_full_files[group] =
                    EMBED_CALL(cli_short, GitCommitCtx, .path = args->paths[at], .repo = repo);
                s_committing.done[at] = 1;
                group += 1u;
            }
        }
        char folder[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_repo_folder, GitCommitCtx, .root = args->root, .repo = repo, .folder_out = folder);
        char error[CLI_PATH_MAX_LEN];
        VerbaLinea why = {error, sizeof(error), 0ULL};
        mmgr_line_put(args->out, (args->out->at != from) ? "\n" : "");
        const unsigned long long line_at = args->out->at;
        if (!EMBED_CALL(cli_message, GitCommitCtx, .message = args->message, .err_msg = &why) ||
            !EMBED_CALL(cli_commit_in, GitCommitCtx, .folder = folder, .paths = s_committing.own_full_files,
                        .count = group, .out = args->out, .err_msg = &why))
        {
            args->out->at = (line_at > from) ? (line_at - 1ULL) : line_at;
            EMBED_CALL(cli_git_length_internal.failed_after, CliGitLengthCtx, .out_read = args->out, .from_u64 = from,
                       .error = error, .length = why.at, .err_msg = args->err_msg);
            ok = 0;
        }
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_repositories.text);
    cli_committing_free();
    return ok;
}

int cli_git_commit_parts(const CliGitRootCfg *args)
{
    if ((args->full_files_count == 0u) && (args->part_count == 0u))
    {
        mmgr_line_put(args->err_msg, "no file is chosen to commit");
        return 0;
    }
    const unsigned int count = args->full_files_count + args->part_count;
    int ok = EMBED_CALL(cli_committing_alloc, GitCommitCtx, .message = args->message, .count = count,
                        .err_msg = args->err_msg);
    for (unsigned int at = 0u; ok && (at < count); at += 1u)
    {
        s_committing.paths[at] =
            (at < args->full_files_count) ? args->whole[at] : args->parts[at - args->full_files_count].path;
    }
    ok = ok && EMBED_CALL(cli_own, GitCommitCtx, .root = args->root, .paths = s_committing.paths, .count = count,
                          .err_msg = args->err_msg);
    const unsigned long long from = args->out->at;
    for (unsigned int first = 0u; ok && (first < count); first += 1u)
    {
        if (s_committing.done[first])
        {
            continue;
        }
        const unsigned int repo = s_committing.owner[first];
        unsigned int own_full_files = 0u;
        unsigned int own_parts = 0u;
        for (unsigned int at = first; at < count; at += 1u)
        {
            if (s_committing.done[at] || (s_committing.owner[at] != repo))
            {
                continue;
            }
            s_committing.done[at] = 1;
            // a path of `whole` that is a part's as well is committed as the part
            unsigned int part = args->part_count;
            for (unsigned int each = 0u; (part == args->part_count) && (each < args->part_count); each += 1u)
            {
                part = (cellul.eq(&(CatenaFinitaCfg){
                           .src = args->parts[each].path, .other = s_committing.paths[at], .cap = CLI_CELLS_POOL}))
                           ? each
                           : part;
            }
            if (part < args->part_count)
            {
                s_committing.own_parts[own_parts] =
                    (CliGitPart){EMBED_CALL(cli_short, GitCommitCtx, .path = s_committing.paths[at], .repo = repo),
                                 args->parts[part].text, args->parts[part].length};
                own_parts += 1u;
            }
            else
            {
                s_committing.own_full_files[own_full_files] =
                    EMBED_CALL(cli_short, GitCommitCtx, .path = s_committing.paths[at], .repo = repo);
                own_full_files += 1u;
            }
        }
        char folder[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_repo_folder, GitCommitCtx, .root = args->root, .repo = repo, .folder_out = folder);
        char error[CLI_PATH_MAX_LEN];
        VerbaLinea why = {error, sizeof(error), 0ULL};
        mmgr_line_put(args->out, (args->out->at != from) ? "\n" : "");
        const unsigned long long line_at = args->out->at;
        if (!EMBED_CALL(cli_message, GitCommitCtx, .message = args->message, .err_msg = &why) ||
            !EMBED_CALL(cli_commit_parts_in, GitCommitCtx, .folder = folder, .whole = s_committing.own_full_files,
                        .full_files_count = own_full_files, .parts = s_committing.own_parts, .part_count = own_parts,
                        .out = args->out, .err_msg = &why))
        {
            args->out->at = (line_at > from) ? (line_at - 1ULL) : line_at;
            EMBED_CALL(cli_git_length_internal.failed_after, CliGitLengthCtx, .out_read = args->out, .from_u64 = from,
                       .error = error, .length = why.at, .err_msg = args->err_msg);
            ok = 0;
        }
    }
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_repositories.text);
    cli_committing_free();
    return ok;
}

int cli_git_resolve(const CliGitRootErrMsgPathCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char displayed[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_git_root_err_msg_internal.adjacent, CliGitRootErrMsgCtx, .root = args->root, .file = args->path,
                    .folder = folder, .displayed = displayed, .err_msg = args->err_msg))
    {
        return 0;
    }
    const char *const arguments[3] = {"add", "--", displayed};
    char out_buf[CLI_PATH_MAX_LEN];
    VerbaLinea out = {out_buf, sizeof(out_buf), 0ULL};
    return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = folder,
                      .request = arguments, .count = 3u, .out = &out, .err_msg = args->err_msg);
}

int cli_git_rollback(const CliGitRootErrMsgPathCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char displayed[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_git_root_err_msg_internal.adjacent, CliGitRootErrMsgCtx, .root = args->root, .file = args->path,
                    .folder = folder, .displayed = displayed, .err_msg = args->err_msg))
    {
        return 0;
    }
    char head[CLI_PATH_MAX_LEN + 8u];
    VerbaLinea named = {head, sizeof(head), 0ULL};
    mmgr_line_put(&named, "HEAD:");
    mmgr_line_put(&named, displayed);
    mmgr_line_finish(&named);
    const char *const exists_args[3] = {"cat-file", "-e", head};
    CliGitResult *const result = cli_git_alloc();
    const int exists = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = folder,
                                  .request = exists_args, .count = 3u, .result = result);
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    if (!exists)
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, " is not in the last commit: delete it from the tree to take it back");
        return 0;
    }
    const char *const arguments[6] = {"restore", "--source=HEAD", "--staged", "--worktree", "--", displayed};
    char out_buf[CLI_PATH_MAX_LEN];
    VerbaLinea out = {out_buf, sizeof(out_buf), 0ULL};
    return EMBED_CALL(cli_git_root_out_count_err_msg.run, CliGitRootOutCountErrMsgCfg, .root = folder,
                      .request = arguments, .count = 6u, .out = &out, .err_msg = args->err_msg);
}
