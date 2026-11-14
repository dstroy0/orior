// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_lines.c: the commit that last changed each line of a file as the editor has it, read from git blame's porcelain
#include "../text/text.h"
#include "git_internal.h"

// A path git gives from the repository's folder made the tree's: `strip` taken from its start and `add` put there
typedef struct
{
    const char *strip;
    unsigned long long side_menu_length;
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
    unsigned int *positions;
    const char *key;
    unsigned long long key_length;
    const char *word_text;
    CliLineCommit *commit;
} GitLinesCtx;
// whether `length` bytes of `text` are all hex digits
EMBED_INLINE int cli_all_hex(const GitLinesCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
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
    if ((length < args->under->side_menu_length) ||
        (memor.cmp(&(MemoriaCfg){
             .src = args->path, .other = args->under->strip, .bytes = args->under->side_menu_length}) != 0))
    {
        return 0;
    }
    char joined[CLI_PATH_MAX_LEN];
    VerbaLinea line = {joined, sizeof(joined), 0ULL};
    mmgr_line_put_n(&line, args->under->add, args->under->add_length);
    mmgr_line_put_n(&line, args->path + args->under->side_menu_length, length - args->under->side_menu_length);
    const unsigned long long size = mmgr_line_finish(&line);
    *at =
        EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = joined, .length = size);
    return 1;
}

// the commit at `at` of `found` put as the one that last changed the file's next line: -1 where it is none, or where
// its id is all zeros, the id git gives a line not committed yet
EMBED_INLINE void cli_line_blamed(const GitLinesCtx *args)
{
    CliLineHistory *const found = args->found;
    const unsigned int at = *args->at;
    int nonzero = 0;
    if (at < found->commits)
    {
        const char *const id = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &found->text,
                                          .at = found->commit[at].id);
        nonzero =
            id[EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = id, .at = 0ULL,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_POOL_BYTES}), .set = "0")] !=
            '\0';
    }
    if (found->lines < CLI_GIT_LINES)
    {
        // a commit's place is below CLI_GIT_LINE_COMMITS, which an int stores
        found->line[found->lines] = nonzero ? (int)at : -1;
        found->lines += 1u;
    }
}

// the place among `found`'s commits of the commit whose 40-digit id is `key`, put there the first time it comes where
// one fits, found through `positions` by FNV-1a over the id; `found`'s count of commits where none fits
EMBED_INLINE unsigned int cli_commit_placed(const GitLinesCtx *args)
{
    CliLineHistory *const found = args->found;
    unsigned int *const positions = args->positions;
    const char *const key = args->key;
    unsigned int hash = 2166136261u;
    for (unsigned int digit = 0u; digit < 40u; digit += 1u)
    {
        hash = (hash ^ (unsigned char)key[digit]) * 16777619u;
    }
    unsigned int entry = hash % CLI_GIT_POSITIONS;
    while (
        (positions[entry] != CLI_GIT_NO_POSITION) &&
        (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                   .text_read = &found->text, .at = found->commit[positions[entry]].id),
                                 .other = key,
                                 .bytes = 40u}) != 0))
    {
        entry = (entry + 1u) % CLI_GIT_POSITIONS;
    }
    if ((positions[entry] == CLI_GIT_NO_POSITION) && (found->commits < CLI_GIT_LINE_COMMITS))
    {
        CliLineCommit *const commit = &found->commit[found->commits];
        memor.set(&(MemoriaCfg){.dst = commit, .val = 0, .bytes = sizeof(*commit)});
        commit->id =
            EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &found->text, .from = key, .length = 40ULL);
        positions[entry] = found->commits;
        found->commits += 1u;
    }
    return (positions[entry] != CLI_GIT_NO_POSITION) ? positions[entry] : found->commits;
}

// whether the `key_length` bytes of `key` are `word`
EMBED_INLINE int cli_key_is(const GitLinesCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_POOL_BYTES});
    return (args->key_length == size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->key, .other = args->word_text, .bytes = size}) == 0);
}

// a line of a commit's own read into `commit`, `key` its first word and the `length` bytes of `text` after it: the
// commit's author, its time, its summary, the file it names and the commit before it with the file's name there
EMBED_INLINE void cli_commit_read(const GitLinesCtx *args)
{
    CliLineCommit *const commit = args->commit;
    CliLineHistory *const found = args->found;
    const char *const value = args->text;
    const unsigned long long value_length = args->length;
    const char *const stop = value + value_length;
    if (EMBED_CALL(cli_key_is, GitLinesCtx, .key = args->key, .key_length = args->key_length, .word_text = "author"))
    {
        commit->author = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &found->text, .from = value,
                                    .length = value_length);
    }
    else if (EMBED_CALL(cli_key_is, GitLinesCtx, .key = args->key, .key_length = args->key_length,
                        .word_text = "author-time"))
    {
        commit->when = 0LL;
        EMBED_CALL(cli_git_length_text_internal.seconds, CliGitLengthTextCtx, .text = value, .length = value_length,
                   .value = &commit->when);
    }
    else if (EMBED_CALL(cli_key_is, GitLinesCtx, .key = args->key, .key_length = args->key_length,
                        .word_text = "summary"))
    {
        commit->subject = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &found->text, .from = value,
                                     .length = value_length);
    }
    else if (EMBED_CALL(cli_key_is, GitLinesCtx, .key = args->key, .key_length = args->key_length,
                        .word_text = "filename"))
    {
        commit->has_path = EMBED_CALL(cli_under, GitLinesCtx, .found = found, .under = args->under, .path = value,
                                      .length = value_length, .at = &commit->path);
    }
    else if (EMBED_CALL(cli_key_is, GitLinesCtx, .key = args->key, .key_length = args->key_length,
                        .word_text = "previous"))
    {
        const char *gap = value;
        gap += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = gap, .at = 0ULL,
                          .size = (unsigned long long)(stop - gap), .set = " ");
        commit->parent = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &found->text, .from = value,
                                    .length = (unsigned long long)(gap - value));
        commit->has_parent = 1;
        const char *const path = (gap < stop) ? (gap + 1) : stop;
        commit->has_was = EMBED_CALL(cli_under, GitLinesCtx, .found = found, .under = args->under, .path = path,
                                     .length = (unsigned long long)(stop - path), .at = &commit->was);
    }
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
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        const unsigned long long length = (unsigned long long)(stop - line);
        if ((length > 0ULL) && (line[0] == '\t'))
        {
            EMBED_CALL(cli_line_blamed, GitLinesCtx, .found = args->found, .at = &at);
            line = next;
            continue;
        }
        const char *space = line;
        space += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = space, .at = 0ULL,
                            .size = (unsigned long long)(stop - space), .set = " ");
        const char *const key = line;
        const unsigned long long key_length = (unsigned long long)(space - line);
        const char *const value = (space < stop) ? (space + 1) : stop;
        line = next;
        if ((key_length == 40ULL) && EMBED_CALL(cli_all_hex, GitLinesCtx, .text = key, .length = 40ULL))
        {
            at = EMBED_CALL(cli_commit_placed, GitLinesCtx, .found = args->found, .positions = s_positions, .key = key);
        }
        else if (at < args->found->commits)
        {
            EMBED_CALL(cli_commit_read, GitLinesCtx, .commit = &args->found->commit[at], .found = args->found,
                       .under = args->under, .key = key, .key_length = key_length, .text = value,
                       .length = (unsigned long long)(stop - value));
        }
    }
}

// whether `text` ends with `tail`
EMBED_INLINE int cli_ends_with(const GitLinesCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES});
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->tail, .cap = CLI_POOL_BYTES});
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
    VerbaLinea repo_line = {from_repo, sizeof(from_repo), 0ULL};
    mmgr_line_put_n(&repo_line, from, prefixed ? (unsigned long long)(to - from) : 0ULL);
    mmgr_line_finish(&repo_line);
    char from_tree[CLI_PATH_MAX_LEN];
    char relative[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = args->root, .path = folder,
               .relative_out = relative);
    VerbaLinea tree_line = {from_tree, sizeof(from_tree), 0ULL};
    mmgr_line_put(&tree_line, relative);
    mmgr_line_put(&tree_line, (relative[0] != '\0') ? "/" : "");
    mmgr_line_finish(&tree_line);
    CliUnder under = {"", 0ULL, "", 0ULL};
    if (EMBED_CALL(cli_ends_with, GitLinesCtx, .text = from_tree, .tail = from_repo))
    {
        under.add = from_tree;
        under.add_length = cellul.len(&(CatenaFinitaCfg){.src = from_tree, .cap = CLI_POOL_BYTES}) -
                           cellul.len(&(CatenaFinitaCfg){.src = from_repo, .cap = CLI_POOL_BYTES});
    }
    else if (EMBED_CALL(cli_ends_with, GitLinesCtx, .text = from_repo, .tail = from_tree))
    {
        under.strip = from_repo;
        under.side_menu_length = cellul.len(&(CatenaFinitaCfg){.src = from_repo, .cap = CLI_POOL_BYTES}) -
                                 cellul.len(&(CatenaFinitaCfg){.src = from_tree, .cap = CLI_POOL_BYTES});
    }
    else
    {
        under.strip = from_repo;
        under.side_menu_length = cellul.len(&(CatenaFinitaCfg){.src = from_repo, .cap = CLI_POOL_BYTES});
    }
    const char *const arguments[6] = {"blame", "--porcelain", "--contents", "-", "--", displayed};
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
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
