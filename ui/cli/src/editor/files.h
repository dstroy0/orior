// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files.h: the editor's reads and writes, every one inside the tree (files_seen.c, files_cells.c, files_list.c,
// files_fuzzy.c, files_ranked.c, files_search.c, files_hits.c, files_find.c, files_read.c, files_write.c,
// files_paste.c, files_fetch.c).
//
// A folder's list contains every entry in it but git's own folder, and marks the ones the tree's ignore files leave
// out: what a build writes, what a cache stores and what a tool stores for itself. A search finds only what git tracks
// or would track. A tree git cannot read is listed whole, less `.git` and `target`, and nothing in it is marked
#ifndef FILES_H
#define FILES_H
#include "../command_line/callback_cfg.h"
#include "../machine/state.h"
#include "../text/patterns.h"
#include "embed_dispatch_layout.h"

typedef struct CliFilesContextCfg CliFilesContextCfg;
typedef struct CliFilesCountCfg CliFilesCountCfg;
typedef struct CliFilesRootCfg CliFilesRootCfg;
typedef struct CliFilesCountOutCfg CliFilesCountOutCfg;
typedef struct CliFilesRootCountCfg CliFilesRootCountCfg;
typedef struct CliFilesRootRowsCfg CliFilesRootRowsCfg;
typedef struct CliFilesRootPathCfg CliFilesRootPathCfg;
typedef struct CliFilesQueryCfg CliFilesQueryCfg;
typedef struct CliFilesRootOutErrMsgCfg CliFilesRootOutErrMsgCfg;
typedef struct CliFilesRootEachCfg CliFilesRootEachCfg;
typedef struct CliFilesRootErrMsgCfg CliFilesRootErrMsgCfg;
typedef struct CliFilesRootErrMsgTextCfg CliFilesRootErrMsgTextCfg;
typedef struct CliFilesRootPathsCfg CliFilesRootPathsCfg;
typedef struct CliFilesTextCfg CliFilesTextCfg;
typedef struct CliFilesRootOutDirCfg CliFilesRootOutDirCfg;
typedef struct CliFilesRootEachContextCfg CliFilesRootEachContextCfg;
typedef struct CliFilesPathCfg CliFilesPathCfg;
typedef struct CliFilesPathErrMsgCfg CliFilesPathErrMsgCfg;
typedef struct CliFilesQueryLettersCfg CliFilesQueryLettersCfg;
typedef struct CliFilesCfg CliFilesCfg;
typedef struct CliFilesPathTypeCfg CliFilesPathTypeCfg;
typedef struct CliFilesGitCfg CliFilesGitCfg;

// the most paths the tree's list contains, and the most bytes of them
#define CLI_FILES_MAX 50000u

#define CLI_FILES_TEXT (1u << 23u)

// A list of paths: each path where it starts in the text, ended by a 0, `count` of them in capacity for `most`; and the
// bytes the text uses, in capacity for `text_max`. Its starts and text are cells of the cellblock, counted to the
// files, taken at the sizes known as it is made
typedef struct
{
    unsigned int *start;
    unsigned int count;
    unsigned int most;
    char *text;
    unsigned long long used;
    unsigned long long text_max;
} CliFiles;

// every file of the tree at `root`, at most CLI_FILES_MAX of them, sorted, laid flat in the temporary tier for the
// length of one call, until cli_files_let gives it back; or NULL where the cellblock cannot meet it (files_seen.c)
const CliFiles *cli_files_view(const CliFilesRootPathsCfg *args);
void cli_files_let(const CliFilesCfg *args);
// the files of the folder at `root`, less `.git` and `target`, walked from its top: each file's path from the top
// handed to `each`, at most CLI_FILES_MAX of them (files_seen.c)
void cli_files_walk(const CliFilesRootEachContextCfg *args);
// each path the tree lists off its trunk handed to `each`, git's where git reads the tree, its word into `*git`: 1
// where the listing is whole; and a folder's alone, by the same means. Whether a path runs through a folder a walk does
// not go into, `git` naming whether git decides the rest. Which of `paths`, `count` of them, git ignores, 1 into
// `ignored` for each (files_seen.c)
int cli_files_on_list(const CliFilesRootEachContextCfg *args);
int cli_files_folder_list(const CliFilesRootEachCfg *args);
int cli_files_path_skip(const CliFilesGitCfg *args);
void cli_files_ignored_read(const CliFilesRootPathsCfg *args);
// the name of every file of the tree, and every ending, `.lean` from a name's last dot, each once, sorted, laid flat
// for one call and given back by cli_files_let; or NULL as there (files_seen.c)
const CliFiles *cli_files_types_view(const CliFilesRootPathsCfg *args);
// the files' own report, `<pool's bytes> <their mark> 0`, the most the views and lists occupied of the cellblock at
// once, written into `out`; the path at `index`; and whether `path` is among the files, which are sorted
// (files_cells.c)
void cli_files_cells_usage_write(const CliFilesRootOutDirCfg *args);
const char *cli_files_at(const CliFilesCfg *args);
int cli_files_contains(const CliFilesPathTypeCfg *args);
// the place of `path` among the sorted files, their count where it is not among them
unsigned int cli_files_index(const CliFilesPathTypeCfg *args);
// the tree listed behind the window, as a tree opens; and whether git tracks the path or would: a file it lists, a
// file in a folder containing one it lists or at the tree's top, or a file in a folder the tree does not contain, as
// one just made, that git does not ignore. Every path is, in a tree git cannot read (files_seen.c, files_tracked.c)
void cli_files_warm(const CliFilesRootPathsCfg *args);
int cli_files_track(const CliFilesRootPathCfg *args);

// One entry of a folder: its name, its path from the tree's top, whether it is a folder, and whether git ignores it
typedef struct
{
    const char *name;
    const char *path;
    int dir;
    int ignored;
} CliFileEntry;
// A folder's entries in one cell
typedef struct
{
    unsigned int count;
    CliFileEntry *entry;
} CliFileEntries;
// the entries of the folder `dir` of `root`, the top folder where empty, less what the explorer's patterns hide, in a
// cell of `owner`: folders first, then by name, case aside; where a pattern includes only some names, a folder shows
// while it contains a file included or a pattern includes it. The cell, or NULL with why given (files_list.c)
typedef struct
{
    const char *root;
    const char *dir;
    const CellOwner *owner;
    VerbaLinea *err_msg;
} CliFilesEntriesCfg;
CliFileEntries *cli_files_entries_alloc(const CliFilesEntriesCfg *args);
typedef struct
{
    CliFileEntries *(*entries_alloc)(const CliFilesEntriesCfg *args);
} CliFilesListingNs;
EMBED_TABLE_LAYOUT(CliFilesListingNs, entries_alloc);
EMBED_TABLE_STORAGE CliFilesListingNs cli_files_listing EMBED_UNUSED = {
    .entries_alloc = cli_files_entries_alloc,
};

// the folders the explorer lists at the most
#define CLI_FILES_EXPLORER_FOLDERS 4096u

// One row of the explorer: the path from the tree's top and the name, standing as long as its folder's listing does;
// how deep it stands; whether it is a folder; whether git ignores it; and whether it is open
typedef struct
{
    const char *path;
    const char *name;
    unsigned int depth;
    int dir;
    int ignored;
    int open;
} CliExplorerRow;

// the explorer's top folder of the tree `root`, which stands until the explorer ends, listed and opened the first time;
// and every listing let go (files_list.c). The rows are each listed folder's entries in their order, an open folder's
// own after it a level deeper: the row `at` into `row` where `row` is not NULL: 1, or 0 past them; how many there are;
// and a press on the row `at`, a folder opened, listed the first time, or closed: 1, or 0 where the row is a file,
// which the caller opens, or past the rows
void cli_files_explorer_show(const char *root);
void cli_files_explorer_end(void);
int cli_files_explorer_row(unsigned int at, CliExplorerRow *row);
unsigned int cli_files_explorer_rows(void);
int cli_files_explorer_press(unsigned int at);
// the folder at `path` among the rows opened or closed as a press on its row does: 1, or 0 where no row is a folder at
// the path; every listed folder listed again, those open standing open; every folder but the top closed; and the menu
// of the row `at` built as the menus' own rows, a folder's Open or Close, a file's Open, then Copy path, and past the
// rows the pane's own, Refresh Explorer and Collapse Folders in Explorer: 1, or 0 where the tree is not listed
int cli_files_explorer_open(const char *path);
void cli_files_explorer_refresh(void);
void cli_files_explorer_collapse(void);
int cli_files_explorer_menu(unsigned int at);
// the explorer marked stale from the watch's thread, as a folder's entries come and go; and whether it was marked since
// this was last asked, read on the window's thread: 1 once for each run of marks
void cli_files_explorer_stale(void);
int cli_files_explorer_changed(void);

// the most letters a text a match reads contains
#define CLI_FUZZY_LETTERS 4096u

// the places in `text` that match `query`, counted in letters, into `hits`, sized for CLI_FUZZY_LETTERS, their count
// into `count`, and the match's score into `score`: 1, or 0 where it does not match. The query's letters match in
// order anywhere in the text, case aside, in no more pieces than half its letters or three. A letter scores more at the
// start of a word or past `from`, where the text's name starts; the letters of `query` a match looks for, in lower
// case, its spaces left out; a text's letters as a match reads them, as they are and in lower case; and the match of
// letters already read, `required` in a text's `displayed` and `lower`, `count` letters each, `latest` sized for
// CLI_FUZZY_LETTERS for the match's own use (files_fuzzy.c)
int cli_files_fuzzy(const CliFilesQueryCfg *args);
unsigned int cli_files_want(const CliFilesQueryLettersCfg *args);
unsigned int cli_files_letters(const CliFilesTextCfg *args);
int cli_files_fuzzy_matches(const CliFilesCountCfg *args);

// the tree listed behind the window for the quick open, where it is not listed; and whether it is not listed yet
// (files_ranked.c)
void cli_files_read_behind(const CliFilesRootPathsCfg *args);
int cli_files_unread(const CliFilesRootPathsCfg *args);

// the most files a ranked find answers
#define CLI_FILES_RANKED_MOST 200u

// A file that answers a query: its path from the tree's top, its score, and the places of its path's letters that
// matched, counted in letters, `hit_count` of them from `hit_first` among the answer's places
typedef struct
{
    const char *path;
    double score;
    unsigned int hit_first;
    unsigned int hit_count;
} CliFileRanked;

// The files that answer a query, in one cell, NULL where none do: how many, the files best first, the places their
// letters matched, and whether the tree's files are still being read, so that more may answer
typedef struct
{
    void *cell;
    unsigned int count;
    const CliFileRanked *row;
    const unsigned int *hit;
    int reading;
} CliFilesRanked;

// the tree's files at `root` that answer `query`, at most `most` of them and no more than CLI_FILES_RANKED_MOST, the
// best first in one pass over the sorted files, into `rows`, its cell of `owner` released and taken anew sized to the
// answer: a file whose name equals the query scores CLI_FILES_RANKED_EXACT over the match, one whose name starts with
// it half that, and one among the `recents` paths of `recent` CLI_FILES_RANKED_LATELY over it; with `fuzzy` off a
// file answers only where its path contains the query's letters in a row, case aside; until the tree is listed the
// paths of `recent` alone answer and `reading` is 1. 1, or 0 where the cells cannot meet the answer, `rows` empty
#define CLI_FILES_RANKED_EXACT 100000.0
#define CLI_FILES_RANKED_LATELY 6.0
int cli_files_ranked_find(const CliFilesRootRowsCfg *args);
// How a search reads its query: case as given or not, whole words only or not, and a regular expression or the text
typedef struct
{
    int match_case;
    int word_text;
    int regex;
} CliSearching;

// A line a search found handed on: its file's path from the top of the tree, its line and column counted from 1, and
// the bytes of its text a hit retains
typedef struct
{
    void *context;
    const char *path;
    unsigned long long line;
    unsigned long long col;
    const unsigned char *text;
    unsigned long long size;
} CliEachHitCfg;

// every line in the tree's files that contains the query, at most 2000 of them, less the files the search's patterns
// hide, and only in the files `set` includes where it includes any, written as `[{"path", "line", "col", "text"}]`, or
// where `each` is not NULL each handed to it with `context` and nothing written: 1, or 0 with why in `err_msg`; and
// every listed file whose path contains each word of the query, case aside, at most 500 of them, less the files the
// explorer's patterns hide, written as a JSON array of paths (files_search.c, files_find.c)
int cli_files_search(const CliFilesRootOutErrMsgCfg *args);

// what opening a file left: nothing, with why given; its text stored whole; its text paged; or a file that is not text,
// nothing stored
#define CLI_FILES_OPEN_REFUSED 0u
#define CLI_FILES_OPEN_WHOLE 1u
#define CLI_FILES_OPEN_PAGED 2u
#define CLI_FILES_OPEN_NOT_TEXT 3u
// the file at `file` of `root` opened under `key`: a text the window takes whole stored in the document store, a larger
// one opened in the pages, a file within an archive read out of it first; what that left (files_read.c)
typedef struct
{
    const char *root;
    const char *file;
    const char *key;
    VerbaLinea *err_msg;
} CliFilesOpenCfg;
unsigned int cli_files_open(const CliFilesOpenCfg *args);
// an empty file made at `file`, and a folder at `dir`, with each folder above that is not there yet, one there already
// left as it is; and `length` bytes of `text` written beside the file and moved over it, a failed write leaving it as
// it was, a file within an archive saved back into it. Each 1, or 0 with why given (files_write.c)
int cli_files_create_file(const CliFilesRootErrMsgTextCfg *args);
int cli_files_create_folder(const CliFilesRootOutDirCfg *args);
int cli_files_write(const CliFilesRootErrMsgTextCfg *args);
// the tree's way of writing a file, which every writer of one takes: the text is written beside the file at `path`,
// as `.name.saving` in its folder, the path into `adjacent`, sized for CLI_PATH_MAX_LEN, and moved over the file once
// whole, the file left as it was where the move fails and the text beside removed; `file` names the file in what is
// given. Each 1, or 0 with why given (files_write.c)
int cli_files_saving_adjacent(const CliFilesPathErrMsgCfg *args);
int cli_files_save(const CliFilesPathCfg *args);

// what is reported as a file is brought: the tree's path of it, the bytes copied and its size
typedef void (*CliProgressFn)(const CliProgressCfg *args);

// a call made of the tree's side on another machine: its name and its JSON arguments, the answer into `out`: 1, or 0
// with why given
typedef int (*CliCall)(const CliCallCfg *args);

// each of `from`, a file or a folder and all it contains, copied or moved into the folder `into` of the tree, a name
// the folder contains already numbered, `notes (2).txt` and on, written as `[{"from", "to"}]`; the same copied as a
// paste copies it, each file reported as it is copied; the tree's path for `name` in `into`, numbered where it contains
// it already; `size` bytes of `data` written at `at` of the file at the tree's `path` as it is brought a part at a
// time, the last moving it over the file; and each of `from`, on this machine, copied into `into` of a tree on another
// machine through `call`. Each 1, or 0 with why given (files_paste.c, files_fetch.c)
int cli_files_free_in(const CliFilesRootErrMsgCfg *args);
int cli_files_write_part(const CliFilesRootPathCfg *args);

struct CliFilesContextCfg
{
    void *context;
    unsigned int count;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *into;
    const char *const *from;
    CliProgressFn progress;
    CliCall call;
    void *call_context;
};

struct CliFilesCountCfg
{
    unsigned int count;
    double *score;
    unsigned int *hits;
    const unsigned int *required;
    unsigned int requested_count;
    const unsigned int *displayed_read;
    const unsigned int *lower_read;
    unsigned int *latest;
    unsigned int *hit_count;
};

struct CliFilesRootCfg
{
    const char *root;
    void *context;
    unsigned int count;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *into;
    const char *const *from;
    CliProgressFn progress;
};

struct CliFilesCountOutCfg
{
    unsigned int count;
    VerbaLinea *out;
    const char *query;
    const char *const *recent;
    unsigned int recents;
    unsigned int most;
    int fuzzy;
    const char *const *files_letters;
};

struct CliFilesRootCountCfg
{
    const char *root;
    unsigned int count;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *into;
    const char *const *from;
    int moving;
};

struct CliFilesRootRowsCfg
{
    const char *root;
    CliFilesRanked *rows;
    const char *query;
    const char *const *recent;
    unsigned int recents;
    unsigned int most;
    int fuzzy;
    const CellOwner *owner;
};

struct CliFilesRootPathCfg
{
    const char *root;
    const char *path;
    VerbaLinea *err_msg;
    unsigned long long at;
    const void *data;
    unsigned long long size;
    int last;
};

struct CliFilesQueryCfg
{
    const char *query;
    const char *text;
    unsigned long long from_u64;
    double *score;
    unsigned int *hits;
    unsigned int *count_pointer;
};

struct CliFilesRootOutErrMsgCfg
{
    const char *root;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *query;
    CliSearching how;
    const CliPatterns *set;
    void (*each)(const CliEachHitCfg *args);
    void *context;
    int standing;
};

struct CliFilesRootEachCfg
{
    const char *root;
    void (*each)(const CliEachPathLengthCfg *args);
    void *context;
    const char *folder;
    int git;
};

struct CliFilesRootErrMsgCfg
{
    const char *root;
    VerbaLinea *err_msg;
    const char *into;
    const char *name;
    char *path_out;
};

struct CliFilesRootErrMsgTextCfg
{
    const char *root;
    VerbaLinea *err_msg;
    const char *text;
    const char *file;
    unsigned long long length;
};

struct CliFilesRootPathsCfg
{
    const char *root;
    const char *const *paths;
    unsigned int count;
    unsigned char *ignored;
};

struct CliFilesTextCfg
{
    const char *text;
    unsigned int capacity;
    unsigned int *displayed;
    unsigned int *lower;
};

struct CliFilesRootOutDirCfg
{
    const char *root;
    VerbaLinea *out;
    const char *dir;
    VerbaLinea *err_msg;
};

struct CliFilesRootEachContextCfg
{
    const char *root;
    void (*each)(const CliEachPathLengthCfg *args);
    void *context;
    int *git_pointer;
};

struct CliFilesPathCfg
{
    const char *path;
    VerbaLinea *err_msg;
    const char *file;
    const char *adjacent_read;
};

struct CliFilesPathErrMsgCfg
{
    const char *path;
    VerbaLinea *err_msg;
    const char *file;
    char *adjacent;
};

struct CliFilesQueryLettersCfg
{
    const char *query;
    unsigned int *letters;
    unsigned int capacity;
};

struct CliFilesCfg
{
    const CliFiles *files;
    unsigned int index;
};

struct CliFilesPathTypeCfg
{
    const CliFiles *files;
    const char *path;
};

struct CliFilesGitCfg
{
    int git;
    const char *path;
};

typedef struct
{
    const CliFiles *(*view)(const CliFilesRootPathsCfg *args);
    void (*ignored_read)(const CliFilesRootPathsCfg *args);
    const CliFiles *(*types_view)(const CliFilesRootPathsCfg *args);
    void (*warm)(const CliFilesRootPathsCfg *args);
    void (*read_behind)(const CliFilesRootPathsCfg *args);
    int (*unread)(const CliFilesRootPathsCfg *args);
} CliFilesRootPathsNs;
EMBED_TABLE_LAYOUT(CliFilesRootPathsNs, view, ignored_read, types_view, warm, read_behind, unread);
EMBED_TABLE_STORAGE CliFilesRootPathsNs cli_files_root_paths EMBED_UNUSED = {
    .view = cli_files_view,
    .ignored_read = cli_files_ignored_read,
    .types_view = cli_files_types_view,
    .warm = cli_files_warm,
    .read_behind = cli_files_read_behind,
    .unread = cli_files_unread,
};

typedef struct
{
    int (*ranked_find)(const CliFilesRootRowsCfg *args);
} CliFilesRootRowsNs;
EMBED_TABLE_LAYOUT(CliFilesRootRowsNs, ranked_find);
EMBED_TABLE_STORAGE CliFilesRootRowsNs cli_files_root_rows EMBED_UNUSED = {
    .ranked_find = cli_files_ranked_find,
};

typedef struct
{
    void (*let)(const CliFilesCfg *args);
    const char *(*at)(const CliFilesCfg *args);
} CliFilesNs;
EMBED_TABLE_LAYOUT(CliFilesNs, let, at);
EMBED_TABLE_STORAGE CliFilesNs cli_files EMBED_UNUSED = {
    .let = cli_files_let,
    .at = cli_files_at,
};

typedef struct
{
    void (*walk)(const CliFilesRootEachContextCfg *args);
    int (*list)(const CliFilesRootEachContextCfg *args);
} CliFilesRootEachContextNs;
EMBED_TABLE_LAYOUT(CliFilesRootEachContextNs, walk, list);
EMBED_TABLE_STORAGE CliFilesRootEachContextNs cli_files_root_each_context EMBED_UNUSED = {
    .walk = cli_files_walk,
    .list = cli_files_on_list,
};

typedef struct
{
    int (*folder_list)(const CliFilesRootEachCfg *args);
} CliFilesRootEachNs;
EMBED_TABLE_LAYOUT(CliFilesRootEachNs, folder_list);
EMBED_TABLE_STORAGE CliFilesRootEachNs cli_files_root_each EMBED_UNUSED = {
    .folder_list = cli_files_folder_list,
};

typedef struct
{
    int (*path_skip)(const CliFilesGitCfg *args);
} CliFilesGitNs;
EMBED_TABLE_LAYOUT(CliFilesGitNs, path_skip);
EMBED_TABLE_STORAGE CliFilesGitNs cli_files_git EMBED_UNUSED = {
    .path_skip = cli_files_path_skip,
};

typedef struct
{
    void (*cells_usage_write)(const CliFilesRootOutDirCfg *args);
    int (*create_folder)(const CliFilesRootOutDirCfg *args);
} CliFilesRootOutDirNs;
EMBED_TABLE_LAYOUT(CliFilesRootOutDirNs, cells_usage_write, create_folder);
EMBED_TABLE_STORAGE CliFilesRootOutDirNs cli_files_root_out_dir EMBED_UNUSED = {
    .cells_usage_write = cli_files_cells_usage_write,
    .create_folder = cli_files_create_folder,
};

typedef struct
{
    int (*contains)(const CliFilesPathTypeCfg *args);
    unsigned int (*index)(const CliFilesPathTypeCfg *args);
} CliFilesPathTypeNs;
EMBED_TABLE_LAYOUT(CliFilesPathTypeNs, contains, index);
EMBED_TABLE_STORAGE CliFilesPathTypeNs cli_files_path_type EMBED_UNUSED = {
    .contains = cli_files_contains,
    .index = cli_files_index,
};

typedef struct
{
    int (*track)(const CliFilesRootPathCfg *args);
    int (*write_part)(const CliFilesRootPathCfg *args);
} CliFilesRootPathNs;
EMBED_TABLE_LAYOUT(CliFilesRootPathNs, track, write_part);
EMBED_TABLE_STORAGE CliFilesRootPathNs cli_files_root_path EMBED_UNUSED = {
    .track = cli_files_track,
    .write_part = cli_files_write_part,
};

typedef struct
{
    int (*fuzzy)(const CliFilesQueryCfg *args);
} CliFilesQueryNs;
EMBED_TABLE_LAYOUT(CliFilesQueryNs, fuzzy);
EMBED_TABLE_STORAGE CliFilesQueryNs cli_files_query EMBED_UNUSED = {
    .fuzzy = cli_files_fuzzy,
};

typedef struct
{
    unsigned int (*want)(const CliFilesQueryLettersCfg *args);
} CliFilesQueryLettersNs;
EMBED_TABLE_LAYOUT(CliFilesQueryLettersNs, want);
EMBED_TABLE_STORAGE CliFilesQueryLettersNs cli_files_query_letters EMBED_UNUSED = {
    .want = cli_files_want,
};

typedef struct
{
    unsigned int (*letters)(const CliFilesTextCfg *args);
} CliFilesTextNs;
EMBED_TABLE_LAYOUT(CliFilesTextNs, letters);
EMBED_TABLE_STORAGE CliFilesTextNs cli_files_text EMBED_UNUSED = {
    .letters = cli_files_letters,
};

typedef struct
{
    int (*fuzzy_matches)(const CliFilesCountCfg *args);
} CliFilesCountNs;
EMBED_TABLE_LAYOUT(CliFilesCountNs, fuzzy_matches);
EMBED_TABLE_STORAGE CliFilesCountNs cli_files_count EMBED_UNUSED = {
    .fuzzy_matches = cli_files_fuzzy_matches,
};

typedef struct
{
    int (*search)(const CliFilesRootOutErrMsgCfg *args);
} CliFilesRootOutErrMsgNs;
EMBED_TABLE_LAYOUT(CliFilesRootOutErrMsgNs, search);
EMBED_TABLE_STORAGE CliFilesRootOutErrMsgNs cli_files_root_out_err_msg EMBED_UNUSED = {
    .search = cli_files_search,
};

typedef struct
{
    unsigned int (*open)(const CliFilesOpenCfg *args);
} CliFilesOpenNs;
EMBED_TABLE_LAYOUT(CliFilesOpenNs, open);
EMBED_TABLE_STORAGE CliFilesOpenNs cli_files_opening EMBED_UNUSED = {
    .open = cli_files_open,
};

typedef struct
{
    int (*create_file)(const CliFilesRootErrMsgTextCfg *args);
    int (*write)(const CliFilesRootErrMsgTextCfg *args);
} CliFilesRootErrMsgTextNs;
EMBED_TABLE_LAYOUT(CliFilesRootErrMsgTextNs, create_file, write);
EMBED_TABLE_STORAGE CliFilesRootErrMsgTextNs cli_files_root_err_msg_text EMBED_UNUSED = {
    .create_file = cli_files_create_file,
    .write = cli_files_write,
};

typedef struct
{
    int (*saving_adjacent)(const CliFilesPathErrMsgCfg *args);
} CliFilesPathErrMsgNs;
EMBED_TABLE_LAYOUT(CliFilesPathErrMsgNs, saving_adjacent);
EMBED_TABLE_STORAGE CliFilesPathErrMsgNs cli_files_path_err_msg EMBED_UNUSED = {
    .saving_adjacent = cli_files_saving_adjacent,
};

typedef struct
{
    int (*save)(const CliFilesPathCfg *args);
} CliFilesPathNs;
EMBED_TABLE_LAYOUT(CliFilesPathNs, save);
EMBED_TABLE_STORAGE CliFilesPathNs cli_files_path EMBED_UNUSED = {
    .save = cli_files_save,
};

typedef struct
{
    int (*free_in)(const CliFilesRootErrMsgCfg *args);
} CliFilesRootErrMsgNs;
EMBED_TABLE_LAYOUT(CliFilesRootErrMsgNs, free_in);
EMBED_TABLE_STORAGE CliFilesRootErrMsgNs cli_files_root_err_msg EMBED_UNUSED = {
    .free_in = cli_files_free_in,
};

// The Search pane as the window lists it (files_search_pane.c): every line of the tree's files that contains what is
// typed, a row a file with its count and under it a row a line, the match marked. Case, whole words and regular
// expressions each turn on and off beside the field, and under it the search is set to the whole tree or to a set of
// files Preferences lists under `search.sets`, `<name>:` on a line and its patterns on the lines after, the set chosen
// saved under `search.set`. Typing rests CLI_SEARCH_REST_MS before the tree is searched; a choice flipped searches at
// once; what stands found is found again as files git sees change. A file's row closes and opens its lines, which
// stays with the file across searches, and a line's row opens the file at the match
// the most bytes a query takes with its end, the rest typing takes before the search, the most bytes of the report
// with its end, the most sets Preferences lists and the most bytes of a set's name with its end
#define CLI_SEARCH_QUERY_MAX 512u
#define CLI_SEARCH_REST_MS 300ULL
#define CLI_SEARCH_REPORT 128u
#define CLI_SEARCH_SETS 32u
#define CLI_SEARCH_NAME_MAX 64u
// the rows: a file, and a line found under it
#define CLI_SEARCH_ROW_FILE 0u
#define CLI_SEARCH_ROW_HIT 1u
// the head's actions: the field, which takes the keys; the case, whole word and regular expression choices; and the
// set, which moves to the next
#define CLI_SEARCH_FIELD 1u
#define CLI_SEARCH_CASE 2u
#define CLI_SEARCH_WORD 3u
#define CLI_SEARCH_REGEX 4u
#define CLI_SEARCH_SET 5u

// the most bytes of a line row's text with its end
#define CLI_SEARCH_ROW_TEXT 1024u

// A row of the pane: its type; the file's path from the top of the tree; a file row's name with its bytes and the
// bytes of its folder before the name, its count of lines found and whether its lines show; a line row's line and
// column counted from 1, its text with the text's bytes, and the match's place and bytes in the text; the text and
// the path copied into the row's own bytes
typedef struct
{
    unsigned int type;
    const char *path;
    const char *name;
    unsigned long long name_length;
    unsigned long long folder_length;
    unsigned int count;
    int open;
    unsigned long long line;
    unsigned long long col;
    const char *text;
    unsigned long long text_length;
    unsigned long long match_at;
    unsigned long long match_length;
    char text_copied[CLI_SEARCH_ROW_TEXT];
    char path_copied[CLI_PATH_MAX_LEN];
} CliSearchRow;

typedef struct CliSearchPaneCfg CliSearchPaneCfg;

// what wakes the window as the watch marks the search stale, `woken`, from the watch's thread; and the set chosen
// restored from the saved `entries`, which the sets are read from after
void cli_search_pane_begin(const CliSearchPaneCfg *args);
// `size` bytes of `text` typed at the query's end at `now_ms`; the key `name` pressed in the field at `now_ms`:
// Backspace erases the query's last letter and Enter searches at the next step, 1 where the key was one of those; and
// the choice `which` flipped, or the set moved to the next, at `now_ms`, searched at the next step
void cli_search_pane_type(const CliSearchPaneCfg *args);
int cli_search_pane_key(const CliSearchPaneCfg *args);
void cli_search_pane_option(const CliSearchPaneCfg *args);
// whether the choice `which` is on; the query with its bytes into `length_out`; and the set chosen's name, Whole tree
// where none is, with its bytes into `length_out` and how many sets Preferences lists into `count_out`
int cli_search_pane_on(const CliSearchPaneCfg *args);
unsigned long long cli_search_pane_query(const CliSearchPaneCfg *args);
const char *cli_search_pane_set(const CliSearchPaneCfg *args);
// one step of the window at `now_ms` for the tree at `root`: the tree searched where a change waited its rest, where a
// key asked for it, or where the watch marked the search stale: 1 where the rows changed; when the next search falls,
// 0 where none waits; and the search marked stale, from the watch's thread
int cli_search_pane_step(const CliSearchPaneCfg *args);
unsigned long long cli_search_pane_deadline_ms(const CliSearchPaneCfg *args);
void cli_search_pane_stale(const CliSearchPaneCfg *args);
// how many rows there are; the row `at` into `row`: 1, or 0 past the last; the file row `at` closed or opened; and the
// report with its bytes into `length_out`: the results and files counted, that none were found, or why the search
// failed, empty where the query is
unsigned int cli_search_pane_rows(const CliSearchPaneCfg *args);
int cli_search_pane_row(const CliSearchPaneCfg *args);
void cli_search_pane_toggle(const CliSearchPaneCfg *args);
const char *cli_search_pane_report(const CliSearchPaneCfg *args);
struct CliSearchPaneCfg
{
    void (*woken)(void);
    const CliStateEntries *entries;
    const char *text;
    unsigned long long size;
    const char *name;
    unsigned int which;
    const char *root;
    unsigned long long now_ms;
    unsigned int at;
    CliSearchRow *row;
    unsigned long long *length_out;
    unsigned int *count_out;
    char *text_out;
};

typedef struct
{
    void (*begin)(const CliSearchPaneCfg *args);
    void (*type)(const CliSearchPaneCfg *args);
    int (*key)(const CliSearchPaneCfg *args);
    void (*option)(const CliSearchPaneCfg *args);
    int (*on)(const CliSearchPaneCfg *args);
    unsigned long long (*query)(const CliSearchPaneCfg *args);
    const char *(*set)(const CliSearchPaneCfg *args);
    int (*step)(const CliSearchPaneCfg *args);
    unsigned long long (*deadline_ms)(const CliSearchPaneCfg *args);
    void (*stale)(const CliSearchPaneCfg *args);
    unsigned int (*rows)(const CliSearchPaneCfg *args);
    int (*row)(const CliSearchPaneCfg *args);
    void (*toggle)(const CliSearchPaneCfg *args);
    const char *(*report)(const CliSearchPaneCfg *args);
} CliSearchPaneNs;
EMBED_TABLE_LAYOUT(CliSearchPaneNs, begin, type, key, option, on, query, set, step, deadline_ms, stale, rows, row,
                   toggle, report);
EMBED_TABLE_STORAGE CliSearchPaneNs cli_search_pane EMBED_UNUSED = {
    .begin = cli_search_pane_begin,
    .type = cli_search_pane_type,
    .key = cli_search_pane_key,
    .option = cli_search_pane_option,
    .on = cli_search_pane_on,
    .query = cli_search_pane_query,
    .set = cli_search_pane_set,
    .step = cli_search_pane_step,
    .deadline_ms = cli_search_pane_deadline_ms,
    .stale = cli_search_pane_stale,
    .rows = cli_search_pane_rows,
    .row = cli_search_pane_row,
    .toggle = cli_search_pane_toggle,
    .report = cli_search_pane_report,
};

#endif
