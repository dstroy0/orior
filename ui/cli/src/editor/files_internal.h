// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_internal.h: what the files sources share: the cells and lists of paths they make
#ifndef FILES_INTERNAL_H
#define FILES_INTERNAL_H
#include "../command_line/callback_cfg.h"
#include "embed_dispatch_layout.h"
#include "files.h"

typedef struct CliFilesBytesRequestedCtx CliFilesBytesRequestedCtx;
typedef struct CliFilesRootCtx CliFilesRootCtx;
typedef struct CliFilesPathCtx CliFilesPathCtx;
typedef struct CliFilesContextCtx CliFilesContextCtx;
typedef struct CliFilesRootOutCtx CliFilesRootOutCtx;
typedef struct CliFilesContextErrMsgCtx CliFilesContextErrMsgCtx;
typedef struct CliFilesPickedCtx CliFilesPickedCtx;
typedef struct CliFilesOutCtx CliFilesOutCtx;
typedef struct CliFilesBytesCtx CliFilesBytesCtx;
typedef struct CliFilesPathLengthCtx CliFilesPathLengthCtx;
typedef struct CliFilesTextU32sCtx CliFilesTextU32sCtx;
typedef struct CliFilesFolderCtx CliFilesFolderCtx;
typedef struct CliFilesErrMsgCtx CliFilesErrMsgCtx;
typedef struct CliFilesPathNameCtx CliFilesPathNameCtx;
typedef struct CliFilesPathFolderCtx CliFilesPathFolderCtx;
typedef struct CliFilesPickedIndexCtx CliFilesPickedIndexCtx;
typedef struct CliFilesCtx CliFilesCtx;
typedef struct CliFilesLetterCtx CliFilesLetterCtx;

// The files a reader picks off the tree for one call: a list laid flat in the temporary tier, and how many
typedef struct
{
    CliFiles *files;
    unsigned int count;
} CliPicked;

// a cell of `bytes` of the cellblock counted to the files, or NULL where it cannot meet them, the files' report
// reported where their mark rose a step past the one last reported; and a cell given back, NULL given back as nothing.
// A list made for `count` paths of `bytes` together, each bounded by the list's limits, none made for no paths: 1, or 0
// where the cellblock cannot meet its cells, the list left empty; a path put after the files: 1, or 0 where they are
// full; the paths sorted by their bytes, each listed once, the sort's spare a cell taken and given back: 1, or 0 where
// it could not be taken; and a list's cells given back and the list emptied. A list made for work given back within
// its call, in the temporary tier, the same. The files of the tree at `root` that `filter` admits picked: 1, or 0 where
// the cellblock cannot meet them; the path of the one picked at `index`; and their cell given back (files_cells.c)
// a cell of the files' lists, which outlive the call that makes them; and one for work given back within its call, from
// the temporary tier: no short cell stands in the persistent tier to leave a hole under one laid above it. Each is
// given back the same way
void *cli_files_cell_alloc(const CliFilesBytesCtx *args);
void *cli_files_work_alloc(const CliFilesBytesCtx *args);
void cli_files_cell_free(const CliFilesCtx *args);
int cli_files_init(const CliFilesBytesCtx *args);
int cli_files_work_init(const CliFilesBytesCtx *args);
int cli_files_append(const CliFilesPathLengthCtx *args);
int cli_files_sort(const CliFilesBytesCtx *args);
void cli_files_clear(const CliFilesBytesCtx *args);
int cli_files_pick(const CliFilesPickedCtx *args);
const char *cli_picked_at(const CliFilesPickedIndexCtx *args);
void cli_picked_let(const CliFilesPickedCtx *args);

// the most lines a search returns, the most letters of a found line retained, and the most bytes they take
#define CLI_HITS_MAX 2000u
#define CLI_HIT_TEXT 240u
#define CLI_HIT_BYTES (CLI_HIT_TEXT * 4u)

// What a search looks for: the query and its bytes, whether it is all ASCII, its letters in lower case, and how it is
// read
typedef struct
{
    const char *query;
    unsigned long long length;
    int ascii;
    const unsigned int *lower;
    unsigned int lower_count;
    CliSearching how;
} CliNeedle;

// a line a search found handed on: its line and column counted from 1, and the bytes of its text a hit retains
typedef void (*CliHitAppend)(const CliHitAppendCfg *args);

// of `size` bytes of a line, the bytes a hit retains: those before its trailing whitespace, at most CLI_HIT_TEXT
// letters of them; whether a letter joins a word, for a search of whole words; and each line of a file's `size` bytes
// that contains the query, the first place on it the query stands, at most CLI_HITS_MAX of them, handed to `put`. Text
// in ASCII without case, and any text with case, is found by the sift over the bytes; text outside ASCII without case
// is found line by line in its lower case (files_hits.c)
unsigned long long cli_hit_cut(const CliFilesPathCtx *args);
int cli_hit_wordy(const CliFilesLetterCtx *args);
void cli_files_hits_in(const CliFilesContextCtx *args);

// what a search by git came to: git cannot search the tree, which is then searched for the query as text; the lines it
// found written; or why it could not read the expression given
#define CLI_GIT_SEARCH_NONE 0
#define CLI_GIT_SEARCH_FOUND 1
#define CLI_GIT_SEARCH_FAILED 2

// a found line written as the window reads it, `{"path", "line", "col", "text"}`, a comma before it where it is not the
// first; and a regular expression searched by git over the files `specs` name, less those `patterns` or `set` hide, at
// most CLI_HITS_MAX lines written as a JSON array (files_find.c)
void cli_hit_write(const CliFilesPathCtx *args);
int cli_search_by_git(const CliFilesRootCtx *args);

// a file or folder of a paste's walk handed on: where it is, where it goes, and whether it is a folder: 1, or 0 with
// why given
typedef int (*CliPasteEach)(const CliPasteEachCfg *args);

// the last part of `path`, its trailing slashes left off, into `name`, sized for CLI_PATH_MAX_LEN: 1, or 0 where it
// has none; whether `path` is `folder` or inside it, a slash either way; the path for `name` in `folder`, numbered
// where the folder contains that name already, `notes (2).txt` and on, into `path`, sized for CLI_PATH_MAX_LEN;
// `source` and all it contains walked, each handed to `each` with where it goes under `target`, a folder before what it
// contains and its entries in the order the system lists them: 1, or 0 with why given where `each` gave 0 or a folder
// could not be read; and a pair pasted written as `{"from", "to"}`, `from` null where it is not given (files_paste.c)
int cli_paste_name(const CliFilesPathNameCtx *args);
int cli_paste_under(const CliFilesPathFolderCtx *args);
// a paste or a bring begun: `into` inside the tree as `folder`, sized for CLI_PATH_MAX_LEN, a folder on the disk,
// and the answer's list opened on `out`: 1, or 0 with why given; and a source's name into `name`: 1, or 0 with the
// source given to have none
int cli_paste_begin(const CliFilesRootOutCtx *args);
int cli_paste_by_name(const CliFilesErrMsgCtx *args);
void cli_paste_free(const CliFilesFolderCtx *args);
int cli_paste_walk(const CliFilesContextErrMsgCtx *args);

// whether a letter is whitespace, as Unicode reads it; whether a word starts at `at` of `text`: the text's start, after
// a space, a slash, a separator in a name, a dot, a colon or an at, or an upper case letter after one that is not, a
// text with `bytes` 1 being the bytes of a path all ASCII, whose only spaces are a space and a tab; and the match of
// `required`, lower case, in `displayed` and its lower case `lower`, `count` letters, letter for letter, a letter past
// `from` scoring more, `latest` sized for CLI_FUZZY_LETTERS for its own use: 1 with its score and its places, or 0
// where it does not match (files_fuzzy.c)
int cli_fuzzy_white(const CliFilesLetterCtx *args);
int cli_fuzzy_starts_word(const CliFilesTextU32sCtx *args);
int cli_fuzzy_letters(const CliFilesBytesRequestedCtx *args);

struct CliFilesBytesRequestedCtx
{
    int bytes_i32;
    const unsigned int *required;
    unsigned int requested_count;
    const unsigned int *displayed;
    const unsigned int *lower;
    unsigned int count_u32;
    unsigned int from_u32;
    unsigned int *latest;
    double *score;
    unsigned int *hits;
    unsigned int *hit_count;
};

struct CliFilesRootCtx
{
    const char *root;
    VerbaLinea *out;
    const char *query;
    CliSearching how;
    const CliWords *specs;
    const CliPatterns *patterns;
    const CliPatterns *set;
    VerbaLinea *err_msg;
};

struct CliFilesPathCtx
{
    const char *path;
    const unsigned char *text;
    unsigned long long size;
    VerbaLinea *out;
    int first;
    unsigned long long line;
    unsigned long long col;
};

struct CliFilesContextCtx
{
    void *context;
    unsigned long long size;
    const unsigned char *bytes_bytes;
    const CliNeedle *needle;
    CliHitAppend put;
};

struct CliFilesRootOutCtx
{
    const char *root;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *into;
    char *folder_out;
};

struct CliFilesContextErrMsgCtx
{
    void *context;
    VerbaLinea *err_msg;
    const char *source;
    const char *target;
    CliPasteEach each;
};

struct CliFilesPickedCtx
{
    CliPicked *picked;
    const char *root;
    int (*filter)(const CliEachPathCfg *args);
    void *context;
};

struct CliFilesOutCtx
{
    VerbaLinea *out;
    int first;
    const char *from;
    const char *to;
};

struct CliFilesBytesCtx
{
    unsigned long long bytes;
    CliFiles *files;
    unsigned long long count;
};

struct CliFilesPathLengthCtx
{
    CliFiles *files;
    const char *path;
    unsigned long long length;
};

struct CliFilesTextU32sCtx
{
    const unsigned int *text_u32s;
    unsigned int at;
    int bytes_i32;
};

struct CliFilesFolderCtx
{
    const char *folder;
    const char *name_read;
    char *path_out;
};

struct CliFilesErrMsgCtx
{
    VerbaLinea *err_msg;
    char *name;
    const char *source;
};

struct CliFilesPathNameCtx
{
    const char *path;
    char *name;
};

struct CliFilesPathFolderCtx
{
    const char *path;
    const char *folder;
};

struct CliFilesPickedIndexCtx
{
    const CliPicked *picked_read;
    unsigned int index;
};

struct CliFilesCtx
{
    void *cell;
};

struct CliFilesLetterCtx
{
    unsigned int letter;
};

typedef struct
{
    void *(*cell_alloc)(const CliFilesBytesCtx *args);
    void *(*work_alloc)(const CliFilesBytesCtx *args);
    int (*init)(const CliFilesBytesCtx *args);
    int (*work_init)(const CliFilesBytesCtx *args);
    int (*sort)(const CliFilesBytesCtx *args);
    void (*clear)(const CliFilesBytesCtx *args);
} CliFilesBytesNs;
EMBED_TABLE_LAYOUT(CliFilesBytesNs, cell_alloc, work_alloc, init, work_init, sort, clear);
EMBED_TABLE_STORAGE CliFilesBytesNs cli_files_bytes_internal EMBED_UNUSED = {
    .cell_alloc = cli_files_cell_alloc,
    .work_alloc = cli_files_work_alloc,
    .init = cli_files_init,
    .work_init = cli_files_work_init,
    .sort = cli_files_sort,
    .clear = cli_files_clear,
};

typedef struct
{
    void (*cell_free)(const CliFilesCtx *args);
} CliFilesNsTable;
EMBED_TABLE_LAYOUT(CliFilesNsTable, cell_free);
EMBED_TABLE_STORAGE CliFilesNsTable cli_files_internal EMBED_UNUSED = {
    .cell_free = cli_files_cell_free,
};

typedef struct
{
    int (*put)(const CliFilesPathLengthCtx *args);
} CliFilesPathLengthNs;
EMBED_TABLE_LAYOUT(CliFilesPathLengthNs, put);
EMBED_TABLE_STORAGE CliFilesPathLengthNs cli_files_path_length_internal EMBED_UNUSED = {
    .put = cli_files_append,
};

typedef struct
{
    int (*pick)(const CliFilesPickedCtx *args);
    void (*picked_let)(const CliFilesPickedCtx *args);
} CliFilesPickedNs;
EMBED_TABLE_LAYOUT(CliFilesPickedNs, pick, picked_let);
EMBED_TABLE_STORAGE CliFilesPickedNs cli_files_picked_internal EMBED_UNUSED = {
    .pick = cli_files_pick,
    .picked_let = cli_picked_let,
};

typedef struct
{
    const char *(*picked_at)(const CliFilesPickedIndexCtx *args);
} CliFilesPickedIndexNs;
EMBED_TABLE_LAYOUT(CliFilesPickedIndexNs, picked_at);
EMBED_TABLE_STORAGE CliFilesPickedIndexNs cli_files_picked_index_internal EMBED_UNUSED = {
    .picked_at = cli_picked_at,
};

typedef struct
{
    unsigned long long (*hit_cut)(const CliFilesPathCtx *args);
    void (*hit_write)(const CliFilesPathCtx *args);
} CliFilesPathNsTable;
EMBED_TABLE_LAYOUT(CliFilesPathNsTable, hit_cut, hit_write);
EMBED_TABLE_STORAGE CliFilesPathNsTable cli_files_path_internal EMBED_UNUSED = {
    .hit_cut = cli_hit_cut,
    .hit_write = cli_hit_write,
};

typedef struct
{
    int (*hit_wordy)(const CliFilesLetterCtx *args);
    int (*fuzzy_white)(const CliFilesLetterCtx *args);
} CliFilesLetterNs;
EMBED_TABLE_LAYOUT(CliFilesLetterNs, hit_wordy, fuzzy_white);
EMBED_TABLE_STORAGE CliFilesLetterNs cli_files_letter_internal EMBED_UNUSED = {
    .hit_wordy = cli_hit_wordy,
    .fuzzy_white = cli_fuzzy_white,
};

typedef struct
{
    void (*hits_in)(const CliFilesContextCtx *args);
} CliFilesContextNsTable;
EMBED_TABLE_LAYOUT(CliFilesContextNsTable, hits_in);
EMBED_TABLE_STORAGE CliFilesContextNsTable cli_files_context_internal EMBED_UNUSED = {
    .hits_in = cli_files_hits_in,
};

typedef struct
{
    int (*search_by_git)(const CliFilesRootCtx *args);
} CliFilesRootNsTable;
EMBED_TABLE_LAYOUT(CliFilesRootNsTable, search_by_git);
EMBED_TABLE_STORAGE CliFilesRootNsTable cli_files_root_internal EMBED_UNUSED = {
    .search_by_git = cli_search_by_git,
};

typedef struct
{
    int (*paste_name)(const CliFilesPathNameCtx *args);
} CliFilesPathNameNs;
EMBED_TABLE_LAYOUT(CliFilesPathNameNs, paste_name);
EMBED_TABLE_STORAGE CliFilesPathNameNs cli_files_path_name_internal EMBED_UNUSED = {
    .paste_name = cli_paste_name,
};

typedef struct
{
    int (*paste_under)(const CliFilesPathFolderCtx *args);
} CliFilesPathFolderNs;
EMBED_TABLE_LAYOUT(CliFilesPathFolderNs, paste_under);
EMBED_TABLE_STORAGE CliFilesPathFolderNs cli_files_path_folder_internal EMBED_UNUSED = {
    .paste_under = cli_paste_under,
};

typedef struct
{
    int (*paste_begin)(const CliFilesRootOutCtx *args);
} CliFilesRootOutNsTable;
EMBED_TABLE_LAYOUT(CliFilesRootOutNsTable, paste_begin);
EMBED_TABLE_STORAGE CliFilesRootOutNsTable cli_files_root_out_internal EMBED_UNUSED = {
    .paste_begin = cli_paste_begin,
};

typedef struct
{
    int (*paste_by_name)(const CliFilesErrMsgCtx *args);
} CliFilesErrMsgNs;
EMBED_TABLE_LAYOUT(CliFilesErrMsgNs, paste_by_name);
EMBED_TABLE_STORAGE CliFilesErrMsgNs cli_files_err_msg_internal EMBED_UNUSED = {
    .paste_by_name = cli_paste_by_name,
};

typedef struct
{
    void (*paste_free)(const CliFilesFolderCtx *args);
} CliFilesFolderNs;
EMBED_TABLE_LAYOUT(CliFilesFolderNs, paste_free);
EMBED_TABLE_STORAGE CliFilesFolderNs cli_files_folder_internal EMBED_UNUSED = {
    .paste_free = cli_paste_free,
};

typedef struct
{
    int (*paste_walk)(const CliFilesContextErrMsgCtx *args);
} CliFilesContextErrMsgNs;
EMBED_TABLE_LAYOUT(CliFilesContextErrMsgNs, paste_walk);
EMBED_TABLE_STORAGE CliFilesContextErrMsgNs cli_files_context_err_msg_internal EMBED_UNUSED = {
    .paste_walk = cli_paste_walk,
};

typedef struct
{
    int (*fuzzy_starts_word)(const CliFilesTextU32sCtx *args);
} CliFilesTextU32sNs;
EMBED_TABLE_LAYOUT(CliFilesTextU32sNs, fuzzy_starts_word);
EMBED_TABLE_STORAGE CliFilesTextU32sNs cli_files_text_u_32s_internal EMBED_UNUSED = {
    .fuzzy_starts_word = cli_fuzzy_starts_word,
};

typedef struct
{
    int (*fuzzy_letters)(const CliFilesBytesRequestedCtx *args);
} CliFilesBytesRequestedNs;
EMBED_TABLE_LAYOUT(CliFilesBytesRequestedNs, fuzzy_letters);
EMBED_TABLE_STORAGE CliFilesBytesRequestedNs cli_files_bytes_requested_internal EMBED_UNUSED = {
    .fuzzy_letters = cli_fuzzy_letters,
};

#endif
