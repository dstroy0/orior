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
typedef struct CliFilesTopCtx CliFilesTopCtx;

// A place in a bounded list ranked by score: its score and its place in the list the search found it in
typedef struct
{
    double score;
    unsigned int at;
} CliTopPlace;

// A cell of `bytes` for work given back within its call, from the temporary tier, or NULL where the cellblock cannot
// meet it. The files' report goes out where their mark rose a step past the last report. The caller gives the cell
// back with g_cli_guard->persistent_buf_release, and NULL gives back nothing.
//
// A list made for work for `count` paths of `bytes` together, each bounded by the list's limits, and none for no
// paths. Returns 1, or 0 where the cellblock cannot meet its cells and the list stays empty. A path put after the files
// returns 1, or 0 where they are full. The sort puts the paths in the order of their bytes, each once, with a spare
// cell it takes and gives back. It returns 1, or 0 where the pool has no spare cell. Last, a list's cells given back
// and the list emptied.
//
// The files of the tree at `root` that `filter` admits, picked and sorted into a movable cell of the picked. Returns 1,
// or 0 where the cellblock cannot meet them. The path of the file picked at `index` goes into `path_out`, sized
// CLI_PATH_MAX_LEN, and the call returns `path_out`. The place of `path` among the picked comes next, their count where
// it is none of them. Last, their cell given back (files_cells.c)
void *cli_files_work_alloc(const CliFilesBytesCtx *args);
int cli_files_work_init(const CliFilesBytesCtx *args);
int cli_files_append(const CliFilesPathLengthCtx *args);
int cli_files_sort(const CliFilesBytesCtx *args);
void cli_files_clear(const CliFilesBytesCtx *args);
int cli_files_pick(const CliFilesPickedCtx *args);
const char *cli_picked_at(const CliFilesPickedIndexCtx *args);
unsigned int cli_picked_index(const CliFilesPickedIndexCtx *args);
void cli_picked_let(const CliFilesPickedCtx *args);
// the sorted list `files_read` written whole into a movable cell of the files module, set in `picked`: 1, or 0
// where the cellblock cannot meet it (files_cells.c)
int cli_files_picked_put(const CliFilesPickedCtx *args);

// the max lines a search returns, the max characters of a found line retained, and the max bytes they take
#define CLI_HITS_MAX 2000u
#define CLI_HIT_TEXT 240u
#define CLI_HIT_BYTES (CLI_HIT_TEXT * 4u)

// What a search looks for: the query and its bytes, and whether it is all ASCII. Then its characters in lower case,
// and the way the search reads it
typedef struct
{
    const char *query;
    unsigned long long length;
    int ascii;
    const unsigned int *lower;
    unsigned int lower_count;
    CliSearching how;
} CliNeedle;

// a line a search found passed on: its line and column counted from 1, and the bytes of its text a hit retains
typedef void (*CliHitAppend)(const CliHitAppendCfg *args);

// Of `size` bytes of a line, the bytes a hit keeps: those before its whitespace at the end, at max CLI_HIT_TEXT
// characters of them. Then whether a character joins a word, for a search of whole words. Each line of a file's `size`
// bytes that contains the query goes to `put`, at max CLI_HITS_MAX of them. Each goes with the first place of the
// query on it. The sift over the bytes finds text in ASCII without case, and any text with case. A search line by line
// in lower case finds text outside ASCII without case (files_hits.c)
unsigned long long cli_hit_cut(const CliFilesPathCtx *args);
int cli_hit_wordy(const CliFilesLetterCtx *args);
void cli_files_hits_in(const CliFilesContextCtx *args);

// What a search by git came to. Where git cannot search the tree, the search runs over it for the query as text.
// Else it writes the lines it found, or why it could not read the expression
#define CLI_GIT_SEARCH_NONE 0
#define CLI_GIT_SEARCH_FOUND 1
#define CLI_GIT_SEARCH_FAILED 2

// A found line written as the window reads it, `{"path", "line", "col", "text"}`, with a comma before it where it is
// not the first. Where `each` is not NULL, the line goes to `each` with `context`, its text cut where a hit cuts it.
// Then a regular expression git searches for over the files `specs` name, less those `patterns` or `set` hide. At max
// CLI_HITS_MAX lines go out as a JSON array, or each goes to `each` (files_find.c)
void cli_hit_write(const CliFilesPathCtx *args);
int cli_search_by_git(const CliFilesRootCtx *args);

// A file or folder of a paste's walk passed on: where it is, where it goes, and whether it is a folder. Returns 1, or
// 0 with why given
typedef int (*CliPasteEach)(const CliPasteEachCfg *args);

// The last part of `path`, without its slashes at the end, into `name`, sized for CLI_PATH_MAX_LEN. Returns 1, or 0
// where it has none. Then whether `path` is `folder` or inside it, a slash either way. Then the path for `name` in
// `folder`, with a number where the folder holds that name already, `notes (2).txt` and on. It goes into `path`, sized
// for CLI_PATH_MAX_LEN.
//
// `source` and all it contains walked, each passed to `each` with where it goes under `target`. A folder comes before
// what it contains, and its entries come in the order the system lists them. Returns 1, or 0 with why given where
// `each` gave 0 or the read of a folder failed. Last, a pair pasted written as `{"from", "to"}`, `from` null where the
// call gives none (files_paste.c)
int cli_paste_name(const CliFilesPathNameCtx *args);
int cli_paste_under(const CliFilesPathFolderCtx *args);
// A paste or a bring begun: `into` inside the tree as `folder`, sized for CLI_PATH_MAX_LEN, a folder on the disk.
// The answer's list opens on `out`. Returns 1, or 0 with why given. Then a source's name into `name`: 1, or 0 where
// the source has none
int cli_paste_begin(const CliFilesRootOutCtx *args);
int cli_paste_by_name(const CliFilesErrMsgCtx *args);
void cli_paste_free(const CliFilesFolderCtx *args);
int cli_paste_walk(const CliFilesContextErrMsgCtx *args);

// Whether a character is whitespace, as Unicode reads it. Then whether a word starts at `at` of `text`. A word starts
// at the text's start, and after a space, a slash, a separator in a name, a dot, a colon or an at. It also starts at
// an upper case character after one that is not. A text with `bytes` 1 is the bytes of a path all in ASCII, whose only
// spaces are a space and a tab.
//
// Last, the match of `required`, lower case, in `displayed` and its lower case `lower`, `count` characters, character
// for character. A character past `from` scores more, and `latest`, sized for CLI_FUZZY_CHARACTERS, is for its own
// use. Returns 1 with its score and its places, or 0 where it does not match (files_fuzzy.c)
int cli_fuzzy_white(const CliFilesLetterCtx *args);
// The place `at` with `score` put in the bounded list `top` of `count` places, at max `max`. It goes after every
// place that scores as much or more, at a place a binary search finds. The last goes where the list is full. Returns
// the list's count (files_fuzzy.c)
unsigned int cli_top_put(const CliFilesTopCtx *args);
int cli_fuzzy_starts_word(const CliFilesTextU32sCtx *args);
int cli_fuzzy_characters(const CliFilesBytesRequestedCtx *args);

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
    void (*each)(const CliEachHitCfg *args);
    void *context;
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
    void (*each)(const CliEachHitCfg *args);
    void *context;
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
    const CliFiles *files_read;
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
    char *path_out;
    const char *path;
};

struct CliFilesCtx
{
    void *cell;
};

struct CliFilesLetterCtx
{
    unsigned int character;
};

struct CliFilesTopCtx
{
    CliTopPlace *top;
    unsigned int count;
    unsigned int max;
    unsigned int at;
    double score;
};

typedef struct
{
    void *(*work_alloc)(const CliFilesBytesCtx *args);
    int (*work_init)(const CliFilesBytesCtx *args);
    int (*sort)(const CliFilesBytesCtx *args);
    void (*clear)(const CliFilesBytesCtx *args);
} CliFilesBytesNs;
EMBED_TABLE_LAYOUT(CliFilesBytesNs, work_alloc, work_init, sort, clear);
EMBED_TABLE_STORAGE CliFilesBytesNs cli_files_bytes_internal EMBED_UNUSED = {
    .work_alloc = cli_files_work_alloc,
    .work_init = cli_files_work_init,
    .sort = cli_files_sort,
    .clear = cli_files_clear,
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
    int (*picked_put)(const CliFilesPickedCtx *args);
} CliFilesPickedNs;
EMBED_TABLE_LAYOUT(CliFilesPickedNs, pick, picked_let, picked_put);
EMBED_TABLE_STORAGE CliFilesPickedNs cli_files_picked_internal EMBED_UNUSED = {
    .pick = cli_files_pick,
    .picked_let = cli_picked_let,
    .picked_put = cli_files_picked_put,
};

typedef struct
{
    const char *(*picked_at)(const CliFilesPickedIndexCtx *args);
    unsigned int (*picked_index)(const CliFilesPickedIndexCtx *args);
} CliFilesPickedIndexNs;
EMBED_TABLE_LAYOUT(CliFilesPickedIndexNs, picked_at, picked_index);
EMBED_TABLE_STORAGE CliFilesPickedIndexNs cli_files_picked_index_internal EMBED_UNUSED = {
    .picked_at = cli_picked_at,
    .picked_index = cli_picked_index,
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
EMBED_TABLE_STORAGE CliFilesLetterNs cli_files_character_internal EMBED_UNUSED = {
    .hit_wordy = cli_hit_wordy,
    .fuzzy_white = cli_fuzzy_white,
};

typedef struct
{
    unsigned int (*put)(const CliFilesTopCtx *args);
} CliFilesTopNs;
EMBED_TABLE_LAYOUT(CliFilesTopNs, put);
EMBED_TABLE_STORAGE CliFilesTopNs cli_files_top_internal EMBED_UNUSED = {
    .put = cli_top_put,
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
    int (*fuzzy_characters)(const CliFilesBytesRequestedCtx *args);
} CliFilesBytesRequestedNs;
EMBED_TABLE_LAYOUT(CliFilesBytesRequestedNs, fuzzy_characters);
EMBED_TABLE_STORAGE CliFilesBytesRequestedNs cli_files_bytes_requested_internal EMBED_UNUSED = {
    .fuzzy_characters = cli_fuzzy_characters,
};

// The views the files sources take for their own work. A view is every file of the tree at `root`, at max
// CLI_FILES_MAX, sorted, positioned flat in a temporary cell. The types view is each name and each ending once, sorted.
// Each returns NULL where the cellblock cannot meet it. The source that takes a view gives it back with the let before
// it returns (files_seen.c)
const CliFiles *cli_files_view(const CliFilesRootPathsCfg *args);
const CliFiles *cli_files_types_view(const CliFilesRootPathsCfg *args);
void cli_files_let(const CliFilesCfg *args);

typedef struct
{
    const CliFiles *(*view)(const CliFilesRootPathsCfg *args);
    const CliFiles *(*types_view)(const CliFilesRootPathsCfg *args);
} CliFilesViewNs;
EMBED_TABLE_LAYOUT(CliFilesViewNs, view, types_view);
EMBED_TABLE_STORAGE CliFilesViewNs cli_files_view_internal EMBED_UNUSED = {
    .view = cli_files_view,
    .types_view = cli_files_types_view,
};

typedef struct
{
    void (*let)(const CliFilesCfg *args);
} CliFilesLetNs;
EMBED_TABLE_LAYOUT(CliFilesLetNs, let);
EMBED_TABLE_STORAGE CliFilesLetNs cli_files_let_internal EMBED_UNUSED = {
    .let = cli_files_let,
};

#endif
