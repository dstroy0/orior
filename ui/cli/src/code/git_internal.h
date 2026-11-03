// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_internal.h: what the git sources share: git started and what it gave read (git_run.c); and what the git check
// reaches: a status's letter and the graph laid out
#ifndef GIT_INTERNAL_H
#define GIT_INTERNAL_H
#include "embed_dispatch_layout.h"
#include "git.h"

typedef struct CliGitResultCtx CliGitResultCtx;
typedef struct CliGitRootCtx CliGitRootCtx;
typedef struct CliGitRootErrMsgCtx CliGitRootErrMsgCtx;
typedef struct CliGitLengthCtx CliGitLengthCtx;
typedef struct CliGitAtCtx CliGitAtCtx;
typedef struct CliGitCountCtx CliGitCountCtx;
typedef struct CliGitLengthOutCtx CliGitLengthOutCtx;
typedef struct CliGitLengthTextCtx CliGitLengthTextCtx;
typedef struct CliGitIndexCtx CliGitIndexCtx;
typedef struct CliGitFromCtx CliGitFromCtx;
typedef struct CliGitCtx CliGitCtx;
typedef struct CliGitFoundCtx CliGitFoundCtx;

// the most bytes of what git writes that are stored, the bytes each read past them takes and lets go, and the most
// bytes of what it reports went wrong
#define CLI_GIT_OUT (1u << 24u)
#define CLI_GIT_READ 4096ull
#define CLI_GIT_ERR (1u << 16u)

// how git is started: with no optional lock taken, as a question that changes nothing runs it; with no terminal prompt
// to answer
#define CLI_GIT_LOCKS_OFF 1u

#define CLI_GIT_PROMPT_OFF 2u

// What git gave: what it wrote, `out_size` bytes of CLI_GIT_OUT, ended by a 0, and 0 in `out_untruncated` where more
// came than is stored; what it reported went wrong, the same of CLI_GIT_ERR; and whether it ended with success
typedef struct
{
    char *out;
    unsigned long long out_size;
    int out_untruncated;
    char *err;
    unsigned long long err_size;
    int ok;
} CliGitResult;

// what one run of git gave, in a cell taken for the run, and given back; where the cells cannot meet a run, an empty
// one every run of git refuses
CliGitResult *cli_git_alloc(void);
void cli_git_free(const CliGitResultCtx *args);
// git at `root` started with the words trust gives, then `args`, `count` of them, as `how` specifies, with
// GIT_INDEX_FILE set to `index` where it is given, and `length` bytes of `input` on its standard input where it is
// given, else nothing: what it gave into `result`, 1, or 0 with why it did not start given
int cli_git_result(const CliGitResultCtx *args);
// git at `root` sent a question that changes nothing, `args`, `count` of them, with no lock taken: what it wrote into
// `result`, 1, or 0 where it failed or wrote more than is stored
int cli_git_query(const CliGitResultCtx *args);
// git at `root` run with `args`, with nothing to answer a prompt with, an index of its own where `index` is given and
// `input` on its standard input where it is given: what it wrote and reported, trimmed, into `out`, 1, or 0 with what
// it reported went wrong, trimmed, or that it failed where it reported nothing
int cli_git_on_run(const CliGitRootCtx *args);
// the bytes from `*from` to `*to` with the whitespace at either end left off
void cli_git_trim(const CliGitFromCtx *args);
// the repository of the `count` at `repos` that contains the first `length` bytes of `path`, both paths in the tree:
// the deepest whose folder contains it, or NULL where none does (git.c)
const char *cli_git_owner(const CliGitCountCtx *args);
// the field from `*at` to the next `stop` or `end`: its start, its length into `length`, and `*at` moved past the
// stop; NULL where the fields have ended. And a whole number as written, a sign before it or none: 1 with its value
// into `value`, or 0 where it is no number
const char *cli_git_field(const CliGitAtCtx *args);
int cli_git_seconds(const CliGitLengthTextCtx *args);
// whether `id` is a commit's id as written: not empty and only hex digits
int cli_git_is_id(const CliGitCtx *args);
// the folder git is queried about a file from into `folder`, and the file's path from there, `./` before it, into
// `displayed`, each sized for CLI_PATH_MAX_LEN: the file's own folder, or the nearest one above it that is there where
// that one is gone. 1, or 0 with why given where the file is not inside the tree
int cli_git_adjacent(const CliGitRootErrMsgCtx *args);
// the state letter for a porcelain status's two columns (git.c)
char cli_git_state_get(const CliGitIndexCtx *args);
// each commit of the graph given its column, its color and the strokes of its row (git_graph.c)
void cli_git_layout_out(const CliGitFoundCtx *args);
// the line of what git reported of a commit, `length` bytes, the one that starts with `[`, else its first, into `out`;
// and git's lines of the commits made so far, those of `out` from `from`, and `error` after them, trimmed, into
// `err_msg` (git_commit_output.c)
void cli_git_commit_line(const CliGitLengthOutCtx *args);
void cli_git_failed_after(const CliGitLengthCtx *args);

struct CliGitResultCtx
{
    CliGitResult *result;
    const char *root;
    const char *const *request;
    unsigned int count;
    unsigned int how;
    const char *index;
    const char *input;
    unsigned long long length;
    VerbaLinea *err_msg;
};

struct CliGitRootCtx
{
    const char *root;
    const char *const *request;
    unsigned int count;
    const char *index;
    const char *input;
    unsigned long long length;
    VerbaLinea *err_msg;
    VerbaLinea *out;
};

struct CliGitRootErrMsgCtx
{
    const char *root;
    VerbaLinea *err_msg;
    const char *file;
    char *folder;
    char *displayed;
};

struct CliGitLengthCtx
{
    unsigned long long length;
    VerbaLinea *err_msg;
    const VerbaLinea *out_read;
    unsigned long long from_u64;
    const char *error;
};

struct CliGitAtCtx
{
    const char **at;
    const char *end;
    char stop;
    unsigned long long *length_pointer;
};

struct CliGitCountCtx
{
    unsigned int count;
    unsigned long long length;
    const char *const *repos;
    const char *path;
};

struct CliGitLengthOutCtx
{
    unsigned long long length;
    VerbaLinea *out;
    const char *output;
};

struct CliGitLengthTextCtx
{
    unsigned long long length;
    const char *text;
    long long *value;
};

struct CliGitIndexCtx
{
    char index_value;
    char tree;
};

struct CliGitFromCtx
{
    const char **from;
    const char **to;
};

struct CliGitCtx
{
    const char *id;
};

struct CliGitFoundCtx
{
    CliGraph *found;
};

typedef struct
{
    void (*free)(const CliGitResultCtx *args);
    int (*result)(const CliGitResultCtx *args);
    int (*query)(const CliGitResultCtx *args);
} CliGitResultNs;
EMBED_TABLE_LAYOUT(CliGitResultNs, free, result, query);
EMBED_TABLE_STORAGE CliGitResultNs cli_git_result_internal EMBED_UNUSED = {
    .free = cli_git_free,
    .result = cli_git_result,
    .query = cli_git_query,
};

typedef struct
{
    int (*run)(const CliGitRootCtx *args);
} CliGitRootNsTable;
EMBED_TABLE_LAYOUT(CliGitRootNsTable, run);
EMBED_TABLE_STORAGE CliGitRootNsTable cli_git_root_internal EMBED_UNUSED = {
    .run = cli_git_on_run,
};

typedef struct
{
    void (*trim)(const CliGitFromCtx *args);
} CliGitFromNs;
EMBED_TABLE_LAYOUT(CliGitFromNs, trim);
EMBED_TABLE_STORAGE CliGitFromNs cli_git_from_internal EMBED_UNUSED = {
    .trim = cli_git_trim,
};

typedef struct
{
    const char *(*owner)(const CliGitCountCtx *args);
} CliGitCountNs;
EMBED_TABLE_LAYOUT(CliGitCountNs, owner);
EMBED_TABLE_STORAGE CliGitCountNs cli_git_count_internal EMBED_UNUSED = {
    .owner = cli_git_owner,
};

typedef struct
{
    const char *(*field)(const CliGitAtCtx *args);
} CliGitAtNs;
EMBED_TABLE_LAYOUT(CliGitAtNs, field);
EMBED_TABLE_STORAGE CliGitAtNs cli_git_at_internal EMBED_UNUSED = {
    .field = cli_git_field,
};

typedef struct
{
    int (*seconds)(const CliGitLengthTextCtx *args);
} CliGitLengthTextNs;
EMBED_TABLE_LAYOUT(CliGitLengthTextNs, seconds);
EMBED_TABLE_STORAGE CliGitLengthTextNs cli_git_length_text_internal EMBED_UNUSED = {
    .seconds = cli_git_seconds,
};

typedef struct
{
    int (*is_id)(const CliGitCtx *args);
} CliGitNsTable;
EMBED_TABLE_LAYOUT(CliGitNsTable, is_id);
EMBED_TABLE_STORAGE CliGitNsTable cli_git_internal EMBED_UNUSED = {
    .is_id = cli_git_is_id,
};

typedef struct
{
    int (*adjacent)(const CliGitRootErrMsgCtx *args);
} CliGitRootErrMsgNsTable;
EMBED_TABLE_LAYOUT(CliGitRootErrMsgNsTable, adjacent);
EMBED_TABLE_STORAGE CliGitRootErrMsgNsTable cli_git_root_err_msg_internal EMBED_UNUSED = {
    .adjacent = cli_git_adjacent,
};

typedef struct
{
    char (*state_get)(const CliGitIndexCtx *args);
} CliGitIndexNs;
EMBED_TABLE_LAYOUT(CliGitIndexNs, state_get);
EMBED_TABLE_STORAGE CliGitIndexNs cli_git_index_internal EMBED_UNUSED = {
    .state_get = cli_git_state_get,
};

typedef struct
{
    void (*layout_out)(const CliGitFoundCtx *args);
} CliGitFoundNs;
EMBED_TABLE_LAYOUT(CliGitFoundNs, layout_out);
EMBED_TABLE_STORAGE CliGitFoundNs cli_git_found_internal EMBED_UNUSED = {
    .layout_out = cli_git_layout_out,
};

typedef struct
{
    void (*commit_line)(const CliGitLengthOutCtx *args);
} CliGitLengthOutNs;
EMBED_TABLE_LAYOUT(CliGitLengthOutNs, commit_line);
EMBED_TABLE_STORAGE CliGitLengthOutNs cli_git_length_out_internal EMBED_UNUSED = {
    .commit_line = cli_git_commit_line,
};

typedef struct
{
    void (*failed_after)(const CliGitLengthCtx *args);
} CliGitLengthNsTable;
EMBED_TABLE_LAYOUT(CliGitLengthNsTable, failed_after);
EMBED_TABLE_STORAGE CliGitLengthNsTable cli_git_length_internal EMBED_UNUSED = {
    .failed_after = cli_git_failed_after,
};

#endif
