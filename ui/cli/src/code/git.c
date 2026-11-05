// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git.c: the commit and the branch a tree stands at, and the files that differ from their last commits
#include "../text/text.h"
#include "git_internal.h"

// the state letter for a porcelain status's two columns
char cli_git_state_get(const CliGitIndexCtx *args)
{
    if ((args->index_value == '?') && (args->tree == '?'))
    {
        return 'U';
    }
    if ((args->index_value == 'U') || (args->tree == 'U') || ((args->index_value == 'A') && (args->tree == 'A')) ||
        ((args->index_value == 'D') && (args->tree == 'D')))
    {
        return 'C';
    }
    if ((args->index_value == 'D') || (args->tree == 'D'))
    {
        return 'D';
    }
    if (args->index_value == 'R')
    {
        return 'R';
    }
    if (args->index_value == 'A')
    {
        return 'A';
    }
    return 'M';
}

int cli_git_head(const CliGitRootCommitCfg *args)
{
    long long *when = args->when;
    int *clean = args->clean;
    static const char *const s_log[3] = {"log", "-1", "--format=%H %ct"};
    static const char *const s_status[3] = {"status", "--porcelain", "--untracked-files=no"};
    CliGitResult *const result = cli_git_alloc();
    int found = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root, .request = s_log,
                           .count = 3u, .result = result);
    const char *from = result->out;
    const char *to = result->out + result->out_size;
    EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
    const char *space = from;
    space += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = space, .at = 0ULL,
                        .size = (unsigned long long)(to - space), .set = " ");
    found = found && (space < to) && (space[1] >= '0') && (space[1] <= '9');
    long long seconds = 0LL;
    const char *digit = space + 1;
    while (found && (digit < to))
    {
        found = (digit[0] >= '0') && (digit[0] <= '9');
        seconds = (seconds * 10LL) + (long long)(digit[0] - '0');
        digit += 1;
    }
    if (found)
    {
        VerbaLinea line = {args->commit, CLI_NAME_MAX_LEN, 0ULL};
        mmgr_line_put_n(&line, from, (unsigned long long)(space - from));
        mmgr_line_finish(&line);
        *when = seconds;
        int all_white = 1;
        if (EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root, .request = s_status,
                       .count = 3u, .result = result))
        {
            for (unsigned long long at = 0ULL; all_white && (at < result->out_size); at += 1ULL)
            {
                const char letter = result->out[at];
                all_white = (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
            }
            *clean = all_white;
        }
        else
        {
            *clean = 0;
        }
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    return found;
}

typedef struct
{
    const char *root;
    const char *const *request;
    unsigned int count;
    char *out;
    unsigned long long capacity;
    const char *folder;
    const char *repo;
    const char *const *repos;
    CliChangedList *found;
} GitCtx;
// what git wrote, trimmed, into `out`, sized for `capacity`: 1, or 0 where git failed
EMBED_INLINE int cli_query_line(const GitCtx *args)
{
    const char *root = args->root;
    const char *const *arguments = args->request;
    unsigned int count = args->count;
    CliGitResult *const result = cli_git_alloc();
    const int found = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = root, .request = arguments,
                                 .count = count, .result = result);
    const char *from = result->out;
    const char *to = result->out + result->out_size;
    EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
    VerbaLinea line = {args->out, args->capacity, 0ULL};
    mmgr_line_put_n(&line, from, found ? (unsigned long long)(to - from) : 0ULL);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    return found;
}

int cli_git_branch(const CliGitCfg *args)
{
    static const char *const s_named[3] = {"rev-parse", "--abbrev-ref", "HEAD"};
    static const char *const s_short[3] = {"rev-parse", "--short", "HEAD"};
    if (!EMBED_CALL(cli_query_line, GitCtx, .root = args->root, .request = s_named, .count = 3u, .out = args->name,
                    .capacity = CLI_NAME_MAX_LEN))
    {
        return 0;
    }
    return (!cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "HEAD", .cap = CLI_CELLS_POOL})) ||
           EMBED_CALL(cli_query_line, GitCtx, .root = args->root, .request = s_short, .count = 3u, .out = args->name,
                      .capacity = CLI_NAME_MAX_LEN);
}

void cli_git_name(const CliGitCfg *args)
{
    static const char *const s_remote[3] = {"config", "--get", "remote.origin.url"};
    char remote[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_query_line, GitCtx, .root = args->root, .request = s_remote, .count = 3u, .out = remote,
                    .capacity = sizeof(remote)))
    {
        remote[0] = '\0';
    }
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = remote, .cap = CLI_CELLS_POOL});
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = remote, .at = length,
                        .set = "/");
    unsigned long long start = length;
    start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = remote, .at = start,
                       .set = "/:\\");
    if (((length - start) >= 4ULL) &&
        (memor.cmp(&(MemoriaCfg){.src = remote + length - 4ULL, .other = ".git", .bytes = 4u}) == 0))
    {
        length -= 4ULL;
    }
    if (length > start)
    {
        VerbaLinea line = {args->name, CLI_NAME_MAX_LEN, 0ULL};
        mmgr_line_put_n(&line, remote + start, length - start);
        mmgr_line_finish(&line);
        return;
    }
    // the folder's own name, the last part of its path
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL});
    end = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->root, .at = end,
                     .set = "/\\");
    unsigned long long first = end;
    first = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->root, .at = first,
                       .set = "/\\");
    const int is_drive = ((end - first) == 2ULL) && (args->root[first + 1ULL] == ':');
    VerbaLinea line = {args->name, CLI_NAME_MAX_LEN, 0ULL};
    mmgr_line_put_n(&line, args->root + first, is_drive ? 0ULL : (end - first));
    mmgr_line_finish(&line);
}

int cli_git_stage_text(const CliGitRootFileStageCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char displayed[CLI_PATH_MAX_LEN];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_git_root_err_msg_internal.adjacent, CliGitRootErrMsgCtx, .root = args->root, .file = args->file,
                    .folder = folder, .displayed = displayed, .err_msg = &err_msg))
    {
        return 0;
    }
    char named[CLI_PATH_MAX_LEN + 8u];
    VerbaLinea line = {named, sizeof(named), 0ULL};
    mmgr_line_ch(&line, ':');
    mmgr_line_decimal(&line, args->stage, 1u);
    mmgr_line_ch(&line, ':');
    mmgr_line_put(&line, displayed);
    mmgr_line_finish(&line);
    const char *const arguments[2] = {"show", named};
    CliGitResult *const result = cli_git_alloc();
    const int found = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = folder, .request = arguments,
                                 .count = 2u, .result = result);
    if (found)
    {
        mmgr_line_put_n(args->out, result->out, result->out_size);
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    return found;
}

// the repository of `repos` that contains `path`, both paths in the tree: the deepest whose folder contains it, or NULL
const char *cli_git_owner(const CliGitCountCtx *args)
{
    const char *owner = NULL;
    unsigned long long longest = 0ULL;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *const repo = args->repos[at];
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = repo, .cap = CLI_CELLS_POOL});
        const int contains = (size == 0ULL) ||
                             ((size == args->length) &&
                              (memor.cmp(&(MemoriaCfg){.src = args->path, .other = repo, .bytes = size}) == 0)) ||
                             ((size < args->length) &&
                              (memor.cmp(&(MemoriaCfg){.src = args->path, .other = repo, .bytes = size}) == 0) &&
                              (args->path[size] == '/'));
        if (contains && ((owner == NULL) || (size >= longest)))
        {
            owner = repo;
            longest = size;
        }
    }
    return owner;
}

// the files under the repository at `folder`, whose path in the tree is `repo`, that differ from its last commit, put
// after the changes found where the repository of `repos` that contains each is `repo`
EMBED_INLINE void cli_changed_at(const GitCtx *args)
{
    const char *const *repos = args->repos;
    unsigned int count = args->count;
    static const char *const s_prefix[2] = {"rev-parse", "--show-prefix"};
    static const char *const s_status[5] = {"status", "--porcelain=v1", "-z", "--untracked-files=all", "--"};
    char prefix[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_query_line, GitCtx, .root = args->folder, .request = s_prefix, .count = 2u, .out = prefix,
                    .capacity = sizeof(prefix)))
    {
        prefix[0] = '\0';
    }
    const char *const arguments[6] = {s_status[0], s_status[1], s_status[2], s_status[3], s_status[4], "."};
    CliGitResult *const result = cli_git_alloc();
    if (!EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->folder, .request = arguments,
                    .count = 6u, .result = result))
    {
        EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
        return;
    }
    const unsigned long long prefix_length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL});
    const unsigned long long repo_length = cellul.len(&(CatenaFinitaCfg){.src = args->repo, .cap = CLI_CELLS_POOL});
    const char *record = result->out;
    const char *const end = result->out + result->out_size;
    int skip = 0;
    while (record < end)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = record, .cap = CLI_CELLS_POOL});
        const char *const next = record + length + 1;
        if (length == 0ULL)
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
        if (length < 4ULL)
        {
            record = next;
            continue;
        }
        const char index = record[0];
        const char tree = record[1];
        // a rename or a copy is followed by the path it came from
        skip = (index == 'R') || (index == 'C');
        if (EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = record + 3, .other_pointer = prefix,
                       .size = prefix_length) &&
            (args->found->count < CLI_GIT_CHANGED))
        {
            char path[CLI_PATH_MAX_LEN];
            VerbaLinea line = {path, sizeof(path), 0ULL};
            mmgr_line_put(&line, args->repo);
            mmgr_line_put(&line, (repo_length != 0ULL) ? "/" : "");
            mmgr_line_put(&line, record + 3 + prefix_length);
            unsigned long long size = mmgr_line_finish(&line);
            // a repository under this one shows in its changes as a folder of new files
            unsigned long long trimmed = size;
            trimmed = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = path,
                                 .at = trimmed, .set = "/");
            const char *const owner = EMBED_CALL(cli_git_count_internal.owner, CliGitCountCtx, .repos = repos,
                                                 .count = count, .path = path, .length = trimmed);
            if ((size != 0ULL) && (owner != NULL) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = owner, .other = args->repo, .cap = CLI_CELLS_POOL})))
            {
                args->found->item[args->found->count].path = EMBED_CALL(
                    cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = path, .length = size);
                args->found->item[args->found->count].state =
                    EMBED_CALL(cli_git_index_internal.state_get, CliGitIndexCtx, .index_value = index, .tree = tree);
                // the name's place is below CLI_PATH_MAX_LEN, which an unsigned int counts
                args->found->item[args->found->count].name = (unsigned int)EMBED_CALL(
                    cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = path, .at = size, .set = "/");
                args->found->count += 1u;
            }
        }
        record = next;
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
}

void cli_git_change(const CliGitRootReposCfg *args)
{
    const char *const *repos = args->repos;
    unsigned int count = args->count;
    static CliRepositories s_repositories;
    static const char *s_paths[CLI_GIT_REPOSITORIES];
    args->found_changed_list_pointer->count = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &args->found_changed_list_pointer->text);
    if (!args->named)
    {
        EMBED_CALL(cli_git_root_found_repositories.repositories, CliGitRootFoundRepositoriesCfg, .root = args->root,
                   .found_repositories_pointer = &s_repositories);
        for (unsigned int at = 0u; at < s_repositories.count; at += 1u)
        {
            s_paths[at] = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_repositories.text,
                                     .at = s_repositories.item[at].path);
        }
        repos = s_paths;
        count = s_repositories.count;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        char folder[CLI_PATH_MAX_LEN];
        VerbaLinea line = {folder, sizeof(folder), 0ULL};
        mmgr_line_put(&line, args->root);
        if (repos[at][0] != '\0')
        {
            mmgr_line_ch(&line, '/');
            mmgr_line_put(&line, repos[at]);
        }
        if (mmgr_line_finish(&line) != 0ULL)
        {
            EMBED_CALL(cli_changed_at, GitCtx, .folder = folder, .repo = repos[at], .repos = repos, .count = count,
                       .found = args->found_changed_list_pointer);
        }
    }
    g_cli_cells->persistent_buf_release(s_repositories.text.bytes, &g_cli_git_cells);
    s_repositories.text.bytes = NULL;
    s_repositories.text.used = 0ULL;
    s_repositories.text.capacity = 0ULL;
}
