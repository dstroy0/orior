// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_lines.c: the commit that last changed each line of a file as the editor has it, read from git blame's porcelain
#include "../text/text.h"
#include "git_internal.h"

// A path git gives from the repository's folder made the tree's: `strip` taken from its start and `add` put there
typedef struct
{
    const char *strip;
    unsigned long long strip_length;
    const char *add;
    unsigned long long add_length;
} CliUnder;
// the places of the commits found, by their id, twice as many as the commits a history contains
#define CLI_GIT_POSITIONS (CLI_GIT_LINE_COMMITS * 2u)

#define CLI_GIT_NO_POSITION 0xffffffffu

typedef struct
{
    const char *text;
    unsigned long long length;
    CliLineHistory *found;
    const CliUnder *under;
    const char *path;
    unsigned int *at;
    const char *out;
    unsigned long long size;
    const char *tail;
} GitLinesCtx;
// whether `length` bytes of `text` are all hex digits
EMBED_INLINE int cli_all_hex(const GitLinesCtx *args)
{
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        const char letter = args->text[at];
        if (!(((letter >= '0') && (letter <= '9')) || ((letter >= 'a') && (letter <= 'f')) ||
              ((letter >= 'A') && (letter <= 'F'))))
        {
            return 0;
        }
    }
    return 1;
}

// a path git gives stored as the tree's: 1 with its place into `at`, or 0 where it does not start with what is stripped
EMBED_INLINE int cli_under(const GitLinesCtx *args)
{
    unsigned long long length = args->length;
    unsigned int *at = args->at;
    if ((length < args->under->strip_length) ||
        (memor.cmp(&(MemoriaCfg){.src = args->path, .other = args->under->strip, .bytes = args->under->strip_length}) !=
         0))
    {
        return 0;
    }
    char joined[CLI_PATH_MAX_LEN];
    VerbaLinea line = {joined, sizeof(joined), 0ull};
    mmgr_line_put_n(&line, args->under->add, args->under->add_length);
    mmgr_line_put_n(&line, args->path + args->under->strip_length, length - args->under->strip_length);
    const unsigned long long size = mmgr_line_finish(&line);
    *at =
        EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = joined, .length = size);
    return 1;
}

// `git blame --porcelain` read: a line of a commit's id and the line's numbers before each line of the file, the
// commit's own lines the first time it comes, and the line itself after a tab
EMBED_INLINE void cli_history_get(const GitLinesCtx *args)
{
    unsigned long long size = args->size;
    static unsigned int s_positions[CLI_GIT_POSITIONS];
    for (unsigned int at = 0u; at < CLI_GIT_POSITIONS; at += 1u)
    {
        s_positions[at] = CLI_GIT_NO_POSITION;
    }
    unsigned int at = 0u;
    const char *line = args->out;
    const char *const end = args->out + size;
    while (line < end)
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ull,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        const unsigned long long length = (unsigned long long)(stop - line);
        if ((length > 0ull) && (line[0] == '\t'))
        {
            int nonzero = 0;
            if (at < args->found->commits)
            {
                const char *const id = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                  .text_read = &args->found->text, .at = args->found->commit[at].id);
                nonzero =
                    id[EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = id, .at = 0ull,
                                  .size = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}),
                                  .set = "0")] != '\0';
            }
            if (args->found->lines < CLI_GIT_LINES)
            {
                // a commit's place is below CLI_GIT_LINE_COMMITS, which an int stores
                args->found->line[args->found->lines] = nonzero ? (int)at : -1;
                args->found->lines += 1u;
            }
            line = next;
            continue;
        }
        const char *space = line;
        space += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = space, .at = 0ull,
                            .size = (unsigned long long)(stop - space), .set = " ");
        const char *const key = line;
        const unsigned long long key_length = (unsigned long long)(space - line);
        const char *const value = (space < stop) ? (space + 1) : stop;
        const unsigned long long value_length = (unsigned long long)(stop - value);
        line = next;
        if ((key_length == 40ull) && EMBED_CALL(cli_all_hex, GitLinesCtx, .text = key, .length = 40ull))
        {
            // FNV-1a over the id, its entry in the places
            unsigned int hash = 2166136261u;
            for (unsigned int digit = 0u; digit < 40u; digit += 1u)
            {
                hash = (hash ^ (unsigned char)key[digit]) * 16777619u;
            }
            unsigned int entry = hash % CLI_GIT_POSITIONS;
            while ((s_positions[entry] != CLI_GIT_NO_POSITION) &&
                   (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                              .text_read = &args->found->text,
                                                              .at = args->found->commit[s_positions[entry]].id),
                                            .other = key,
                                            .bytes = 40u}) != 0))
            {
                entry = (entry + 1u) % CLI_GIT_POSITIONS;
            }
            if ((s_positions[entry] == CLI_GIT_NO_POSITION) && (args->found->commits < CLI_GIT_LINE_COMMITS))
            {
                CliLineCommit *const commit = &args->found->commit[args->found->commits];
                memor.set(&(MemoriaCfg){.dst = commit, .val = 0, .bytes = sizeof(*commit)});
                commit->id = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text,
                                        .from = key, .length = 40ull);
                s_positions[entry] = args->found->commits;
                args->found->commits += 1u;
            }
            at = (s_positions[entry] != CLI_GIT_NO_POSITION) ? s_positions[entry] : args->found->commits;
            continue;
        }
        if (at >= args->found->commits)
        {
            continue;
        }
        CliLineCommit *const commit = &args->found->commit[at];
        if ((key_length == 6ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "author", .bytes = 6u}) == 0))
        {
            commit->author = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text,
                                        .from = value, .length = value_length);
        }
        else if ((key_length == 11ull) &&
                 (memor.cmp(&(MemoriaCfg){.src = key, .other = "author-time", .bytes = 11u}) == 0))
        {
            commit->when = 0ll;
            EMBED_CALL(cli_git_length_text_internal.seconds, CliGitLengthTextCtx, .text = value, .length = value_length,
                       .value = &commit->when);
        }
        else if ((key_length == 7ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "summary", .bytes = 7u}) == 0))
        {
            commit->subject = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text,
                                         .from = value, .length = value_length);
        }
        else if ((key_length == 8ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "filename", .bytes = 8u}) == 0))
        {
            commit->has_path = EMBED_CALL(cli_under, GitLinesCtx, .found = args->found, .under = args->under,
                                          .path = value, .length = value_length, .at = &commit->path);
        }
        else if ((key_length == 8ull) && (memor.cmp(&(MemoriaCfg){.src = key, .other = "previous", .bytes = 8u}) == 0))
        {
            const char *gap = value;
            gap += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = gap, .at = 0ull,
                              .size = (unsigned long long)(stop - gap), .set = " ");
            commit->parent = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text,
                                        .from = value, .length = (unsigned long long)(gap - value));
            commit->has_parent = 1;
            const char *const path = (gap < stop) ? (gap + 1) : stop;
            commit->has_was = EMBED_CALL(cli_under, GitLinesCtx, .found = args->found, .under = args->under,
                                         .path = path, .length = (unsigned long long)(stop - path), .at = &commit->was);
        }
    }
}

// whether `text` ends with `tail`
EMBED_INLINE int cli_ends_with(const GitLinesCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->tail, .cap = CLI_CELLS_POOL});
    return (length >= size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->text + length - size, .other = args->tail, .bytes = size}) == 0);
}

int cli_git_line_history(const CliGitLengthCfg *args)
{
    static const char *const s_prefix[2] = {"rev-parse", "--show-prefix"};
    args->found_line_history_pointer->commits = 0u;
    args->found_line_history_pointer->lines = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &args->found_line_history_pointer->text);
    char folder[CLI_PATH_MAX_LEN];
    char displayed[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_git_root_err_msg_internal.adjacent, CliGitRootErrMsgCtx, .root = args->root, .file = args->file,
                    .folder = folder, .displayed = displayed, .err_msg = args->err_msg))
    {
        return 0;
    }
    // git names a file from its repository's folder, the tree names it from its own: the folder's path from each ends
    // the same, and what goes before it in either path names the other's folder
    char from_repo[CLI_PATH_MAX_LEN];
    CliGitResult *const result = cli_git_alloc();
    const int prefixed = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = folder, .request = s_prefix,
                                    .count = 2u, .result = result);
    const char *from = result->out;
    const char *to = result->out + result->out_size;
    EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
    VerbaLinea repo_line = {from_repo, sizeof(from_repo), 0ull};
    mmgr_line_put_n(&repo_line, from, prefixed ? (unsigned long long)(to - from) : 0ull);
    mmgr_line_finish(&repo_line);
    char from_tree[CLI_PATH_MAX_LEN];
    char relative[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = folder,
               .relative_out = relative);
    VerbaLinea tree_line = {from_tree, sizeof(from_tree), 0ull};
    mmgr_line_put(&tree_line, relative);
    mmgr_line_put(&tree_line, (relative[0] != '\0') ? "/" : "");
    mmgr_line_finish(&tree_line);
    CliUnder under = {"", 0ull, "", 0ull};
    if (EMBED_CALL(cli_ends_with, GitLinesCtx, .text = from_tree, .tail = from_repo))
    {
        under.add = from_tree;
        under.add_length = cellul.len(&(CatenaFinitaCfg){.src = from_tree, .cap = CLI_CELLS_POOL}) -
                           cellul.len(&(CatenaFinitaCfg){.src = from_repo, .cap = CLI_CELLS_POOL});
    }
    else if (EMBED_CALL(cli_ends_with, GitLinesCtx, .text = from_repo, .tail = from_tree))
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
    const char *const arguments[6] = {"blame", "--porcelain", "--contents", "-", "--", displayed};
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    const int blamed = EMBED_CALL(cli_git_result_internal.result, CliGitResultCtx, .root = folder, .request = arguments,
                                  .count = 6u, .how = CLI_GIT_LOCKS_OFF, .index = NULL, .input = args->text_text,
                                  .length = args->length, .result = result, .err_msg = &unstarted) &&
                       result->ok && result->out_untruncated;
    if (blamed)
    {
        EMBED_CALL(cli_history_get, GitLinesCtx, .out = result->out, .size = result->out_size, .under = &under,
                   .found = args->found_line_history_pointer);
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    if (!blamed)
    {
        mmgr_line_put(args->err_msg, "git has no history of ");
        mmgr_line_put(args->err_msg, args->file);
    }
    return blamed;
}
