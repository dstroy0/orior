// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files.h: the editor's reads and writes, every one inside the tree (files_seen.c, files_cells.c, files_list.c,
// files_fuzzy.c, files_ranked.c, files_search.c, files_hits.c, files_find.c, files_read.c, files_write.c,
// files_paste.c, files_fetch.c).
//
// A folder's list holds every entry in it but git's own folder. It marks the ones the tree's ignore files leave out:
// what a build writes, what a cache stores and what a tool stores for itself. A search finds only what git tracks or
// would track. The list of a tree git cannot read is whole, less `.git` and `target`, and it marks nothing
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
typedef struct CliFilesRankedAtCfg CliFilesRankedAtCfg;
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
typedef struct CliFilesViewCfg CliFilesViewCfg;
typedef struct CliFilesPathTypeCfg CliFilesPathTypeCfg;
typedef struct CliFilesGitCfg CliFilesGitCfg;

// the max paths the tree's list contains, and the max bytes of them
#define CLI_FILES_MAX 50000u

#define CLI_FILES_TEXT (1u << 23u)

// A list of paths. It holds where each path starts in the text, a 0 after each, `count` of them in capacity for
// `max`. It holds the bytes the text uses, in capacity for `text_max`. Its starts and text are cells of the cellblock,
// counted to the files, taken at the sizes the make knows
typedef struct
{
    unsigned int *start;
    unsigned int count;
    unsigned int max;
    char *text;
    unsigned long long used;
    unsigned long long text_max;
} CliFiles;

// A sorted list of paths in one movable cell of the files module. Each path's place from `text_at` comes first, as
// many as `count`, and the paths follow, a 0 after each. A reader reads a path into a buffer of its own call
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned int count;
    unsigned long long text_at;
} CliPicked;

// What a view of the tree's files passes to its callback: `context`, and the files in the files module's movable
// cell, valid for the call alone
typedef struct
{
    void *context;
    const CliPicked *files;
} CliFilesEachCfg;

// Every file of the tree at `root`, at max CLI_FILES_MAX of them, sorted, in a movable cell of the files module. The
// files go to `each` with `context`, and the module gives the cell back before it returns. Returns 1, or 0 where the
// cellblock cannot meet the view and the files do not go to `each` (files_seen.c)
int cli_files_each(const CliFilesRootPathsCfg *args);
// The path at `index` of the view `view` read into `path_out`, CLI_PATH_MAX_LEN bytes: `path_out`, empty past the
// view. Whether `path` is among the files of the view (files_cells.c)
const char *cli_files_view_at(const CliFilesViewCfg *args);
int cli_files_view_contains(const CliFilesViewCfg *args);
// The files of the folder at `root`, less `.git` and `target`, walked from its top. Each file's path from the top
// goes to `each`, at max CLI_FILES_MAX of them (files_seen.c)
void cli_files_walk(const CliFilesRootEachContextCfg *args);
// Each path the tree lists off its trunk passed to `each`, git's where git reads the tree, its word into `*git`.
// Returns 1 where the listing is whole. A folder's list alone works the same way. Then whether a path runs through a
// folder a walk does not go into, `git` naming whether git decides the rest. Last, which of `paths`, `count` of them,
// git ignores, 1 into `ignored` for each (files_seen.c)
int cli_files_on_list(const CliFilesRootEachContextCfg *args);
int cli_files_folder_list(const CliFilesRootEachCfg *args);
int cli_files_path_skip(const CliFilesGitCfg *args);
void cli_files_ignored_read(const CliFilesRootPathsCfg *args);
// The name of every file of the tree, and every ending, `.lean` from a name's last dot, each once, sorted. The view
// goes to `each` as cli_files_each passes its files, and returns as it returns (files_seen.c)
int cli_files_types_each(const CliFilesRootPathsCfg *args);
// The files' own report, `<pool's bytes> <their mark> 0`, written into `out`. The mark is the max of the cellblock
// the views and lists occupied at once. Then the path at `index`, and whether `path` is among the files, which are in
// order (files_cells.c)
void cli_files_cells_usage_write(const CliFilesRootOutDirCfg *args);
const char *cli_files_at(const CliFilesCfg *args);
int cli_files_contains(const CliFilesPathTypeCfg *args);
// the place of `path` among the sorted files, their count where it is not among them
unsigned int cli_files_index(const CliFilesPathTypeCfg *args);
// The tree listed behind the window, as a tree opens. Then whether git tracks the path or would. It does for a file
// it lists, and for a file in a folder that holds one it lists or at the tree's top. It does too for a file in a
// folder the tree does not hold, as one just made, that git does not ignore. In a tree git cannot read, it does for
// every path (files_seen.c, files_tracked.c)
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
// One entry of a folder in a movable cell of the files module. It holds the places of its name and its path among
// the cell's text, whether it is a folder, and whether git ignores it
typedef struct
{
    unsigned long long name_at;
    unsigned long long path_at;
    int dir;
    int ignored;
} CliFileListed;
// A folder's entries in one movable cell of the files module, NULL where there are none. The `count` entries come
// first, and their text starts at `text_at`, a 0 after each name and path
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned int count;
    unsigned long long text_at;
} CliFilesListed;
// What a folder's listing passes to its callback: `context`, and the entries, valid for the call alone
typedef struct
{
    void *context;
    const CliFilesListed *listed;
} CliFilesListedEachCfg;
// The entries of the folder `dir` of `root`, the top folder where empty, less what the explorer's patterns hide. They
// go in a cell of `owner`, folders first, then by name, case aside. Where a pattern includes only some names, a folder
// shows while it holds a file included or a pattern includes it. Returns the cell, or NULL with why given
// (files_list.c).
//
// The same entries in a movable cell of the files module go to `each` with `context`, and the module gives the cell
// back before it returns. Returns 1, or 0 with why given, and nothing goes to `each`.
//
// The entry `index` of `listed` read into `entry_out`, its name into `name_out` and its path into `path_out`, each
// CLI_PATH_MAX_LEN bytes. The entry's name and path point at those buffers. Returns 1, or 0 past the entries
typedef struct
{
    const char *root;
    const char *dir;
    const CellOwner *owner;
    void (*each)(const CliFilesListedEachCfg *args);
    void *context;
    VerbaLinea *err_msg;
} CliFilesEntriesCfg;
typedef struct
{
    const CliFilesListed *listed;
    unsigned int index;
    CliFileEntry *entry_out;
    char *name_out;
    char *path_out;
} CliFilesListedAtCfg;
CliFileEntries *cli_files_entries_alloc(const CliFilesEntriesCfg *args);
int cli_files_entries_each(const CliFilesEntriesCfg *args);
int cli_files_listed_at(const CliFilesListedAtCfg *args);
typedef struct
{
    CliFileEntries *(*entries_alloc)(const CliFilesEntriesCfg *args);
    int (*entries_each)(const CliFilesEntriesCfg *args);
} CliFilesListingNs;
EMBED_TABLE_LAYOUT(CliFilesListingNs, entries_alloc, entries_each);
EMBED_TABLE_STORAGE CliFilesListingNs cli_files_listing EMBED_UNUSED = {
    .entries_alloc = cli_files_entries_alloc,
    .entries_each = cli_files_entries_each,
};
typedef struct
{
    int (*at)(const CliFilesListedAtCfg *args);
} CliFilesListedNs;
EMBED_TABLE_LAYOUT(CliFilesListedNs, at);
EMBED_TABLE_STORAGE CliFilesListedNs cli_files_listed EMBED_UNUSED = {
    .at = cli_files_listed_at,
};

// One row of the explorer. It holds the path from the tree's top and the name, valid while its folder's listing is.
// Then how deep it is, whether it is a folder, whether git ignores it, and whether it is open
typedef struct
{
    const char *path;
    const char *name;
    unsigned int depth;
    int dir;
    int ignored;
    int open;
} CliExplorerRow;

// The explorer's top folder of the tree `root`, which lasts until the explorer ends, listed and opened the first
// time. Then every listing let go (files_list.c).
//
// The rows are each listed folder's entries in their order, with an open folder's own after it a level deeper. The row
// `at` goes into `row` where `row` is not NULL: 1, or 0 past them. Then how many there are. Last, a press on the row
// `at`, which opens a folder, lists it the first time, or closes it. It returns 1, or 0 where the row is a file, which
// the caller opens, or past the rows.
//
// The explorer: the tree `root` shown, the row `at` into `row`, the row `at` pressed or its menu opened, and the
// file at `path` opened
typedef struct
{
    const char *root;
    unsigned int at;
    CliExplorerRow *row;
    const char *path;
} CliFilesExplorerCfg;
void cli_files_explorer_show(const CliFilesExplorerCfg *args);
void cli_files_explorer_end(void);
int cli_files_explorer_row(const CliFilesExplorerCfg *args);
unsigned int cli_files_explorer_rows(void);
int cli_files_explorer_press(const CliFilesExplorerCfg *args);
// The folder at `path` among the rows opened or closed as a press on its row does it. Returns 1, or 0 where no row is
// a folder at the path. Then every listed folder listed again, and those open stay open. Then every folder but the top
// closed.
//
// Last, the menu of the row `at`, built as the menus' own rows: a folder's Open or Close, or a file's Open, then Copy
// path. Past the rows it is the pane's own, Refresh Explorer and Collapse Folders in Explorer. It returns 1, or 0
// where the tree has no listing
int cli_files_explorer_open(const CliFilesExplorerCfg *args);
void cli_files_explorer_refresh(void);
void cli_files_explorer_collapse(void);
int cli_files_explorer_menu(const CliFilesExplorerCfg *args);
// The explorer marked stale from the watch's thread, as a folder's entries come and go. Then whether a mark came
// since the last ask, read on the window's thread: 1 once for each run of marks
void cli_files_explorer_stale(void);
int cli_files_explorer_changed(void);

// the max characters a text a match reads contains
#define CLI_FUZZY_CHARACTERS 4096u

// The places in `text` that match `query`, counted in characters, into `hits`, sized for the query's bytes. Their
// count goes into `count`, and the match's score into `score`. The match's own lists are one take of `owner`. Returns
// 1, or 0 where it does not match. The query's characters match in order anywhere in the text, case aside, in no more
// pieces than half its characters or three. A character scores more at the start of a word or past `from`, where the
// text's name starts.
//
// Then the characters of `query` a match looks for, in lower case, without its spaces, into `characters`, sized for
// `capacity`. Then a text's characters as a match reads them, as they are and in lower case. Last, the match of
// characters read already, `required` in a text's `displayed` and `lower`, `count` characters each. `latest`, sized
// for CLI_FUZZY_CHARACTERS, is for the match's own use (files_fuzzy.c)
int cli_files_fuzzy(const CliFilesQueryCfg *args);
unsigned int cli_files_want(const CliFilesQueryLettersCfg *args);
unsigned int cli_files_characters(const CliFilesTextCfg *args);
int cli_files_fuzzy_matches(const CliFilesCountCfg *args);

// The tree listed behind the window for the quick open, where it has no listing yet. Then whether it has no listing
// yet (files_ranked.c)
void cli_files_read_behind(const CliFilesRootPathsCfg *args);
int cli_files_unread(const CliFilesRootPathsCfg *args);

// the max files a ranked find answers
#define CLI_FILES_RANKED_MAX 200u

// A file that answers a query, as a reader reads it into a record of its own call. It holds the place of its path
// among the answer's paths, and its score. It also holds the places of its path's characters that matched, counted
// in characters. There are `hit_count` of them from `hit_first` among the answer's places
typedef struct
{
    unsigned long long path_at;
    double score;
    unsigned int hit_first;
    unsigned int hit_count;
} CliFileRanked;

// The files that answer a query, in one movable cell of the files module, NULL where none do. The rows come first,
// `count` of them best first. The places their characters matched start at `hit_at`, and the paths at `text_at`, a 0
// after each. Last, whether the read of the tree's files runs still, and more can answer
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned int count;
    unsigned long long hit_at;
    unsigned long long text_at;
    int reading;
} CliFilesRanked;

// What a ranked find passes to its callback: `context`, and the answer in the files module's movable cell, valid for
// the call alone
typedef struct
{
    void *context;
    const CliFilesRanked *ranked;
} CliFilesRankedEachCfg;

// The tree's files at `root` that answer `query`, the best first, at max `max` of them and CLI_FILES_RANKED_MAX at
// most. One pass over the files in order puts them into a movable cell of the files module, at the answer's size. A
// file whose name equals the query scores CLI_FILES_RANKED_EXACT over the match, and one whose name starts with it
// half that. One among the `recents` paths of `recent` scores CLI_FILES_RANKED_LATELY over it. With `fuzzy` off, a
// file answers only where its path holds the query's characters in a row, case aside.
//
// Until the tree has its listing, the paths of `recent` alone answer and `reading` is 1. The answer goes to `each`
// with `context`, and the module gives the cell back before it returns. Returns 1, or 0 where the cells cannot meet
// the answer and nothing goes to `each`.
//
// The row at `index` of `ranked` read into `row_out`, its path into `path_out`, CLI_PATH_MAX_LEN bytes. Its places go
// into `hits_out`, at max `hits_max` of them, and `row_out`'s `hit_count` tells how many. Returns 1, or 0 past the
// rows
#define CLI_FILES_RANKED_EXACT 100000.0
#define CLI_FILES_RANKED_LATELY 6.0
int cli_files_ranked_find(const CliFilesRootRowsCfg *args);
int cli_files_ranked_at(const CliFilesRankedAtCfg *args);
// How a search reads its query: case as given or not, whole words only or not, and a regular expression or the text
typedef struct
{
    int match_case;
    int word_text;
    int regex;
} CliSearching;

// A line a search found, passed on. It holds its file's path from the tree's top, its line and column counted from
// 1, and the bytes of its text a hit keeps
typedef struct
{
    void *context;
    const char *path;
    unsigned long long line;
    unsigned long long col;
    const unsigned char *text;
    unsigned long long size;
} CliEachHitCfg;

// Every line in the tree's files that contains the query, at max 2000 of them, less the files the search's patterns
// hide. Where `set` includes any files, only those count. The lines go out as `[{"path", "line", "col", "text"}]`.
// Where `each` is not NULL, each goes to it with `context`, and nothing goes out. Returns 1, or 0 with why in
// `err_msg`.
//
// Last, every listed file whose path contains each word of the query, case aside, at max 500 of them. The list leaves
// out the files the explorer's patterns hide, and goes out as a JSON array of paths (files_search.c, files_find.c)
int cli_files_search(const CliFilesRootOutErrMsgCfg *args);

// what opening a file left: nothing, with why given; its text stored whole; its text paged; or a file that is not text,
// nothing stored
#define CLI_FILES_OPEN_REFUSED 0u
#define CLI_FILES_OPEN_WHOLE 1u
#define CLI_FILES_OPEN_PAGED 2u
#define CLI_FILES_OPEN_NOT_TEXT 3u
// The file at `file` of `root` opened under `key`. A text the window takes whole goes into the document store, and a
// larger one opens in the pages. A file within an archive comes out of it first. Returns what that left (files_read.c)
typedef struct
{
    const char *root;
    const char *file;
    const char *key;
    VerbaLinea *err_msg;
} CliFilesOpenCfg;
unsigned int cli_files_open(const CliFilesOpenCfg *args);
// An empty file made at `file`, and a folder at `dir`, with each folder above that is not there yet. One there
// already stays as it is. Then `length` bytes of `text` written beside the file and moved over it. A failed write
// leaves it as it was, and a file within an archive goes back into it. Each returns 1, or 0 with why given
// (files_write.c)
int cli_files_create_file(const CliFilesRootErrMsgTextCfg *args);
int cli_files_create_folder(const CliFilesRootOutDirCfg *args);
int cli_files_write(const CliFilesRootErrMsgTextCfg *args);
// The tree's way to write a file, which every writer of one takes. The text goes beside the file at `path`, as
// `.name.saving` in its folder, the path into `adjacent`, sized for CLI_PATH_MAX_LEN. Once whole, it moves over the
// file. Where the move fails, the file stays as it was and the text beside goes. `file` names the file in what the
// call gives. Each returns 1, or 0 with why given (files_write.c)
int cli_files_saving_adjacent(const CliFilesPathErrMsgCfg *args);
int cli_files_save(const CliFilesPathCfg *args);

// what a report says as a file comes in: the tree's path of it, the bytes copied and its size
typedef void (*CliProgressFn)(const CliProgressCfg *args);

// a call made of the tree's side on another machine: its name and its JSON arguments, the answer into `out`: 1, or 0
// with why given
typedef int (*CliCall)(const CliCallCfg *args);

// Each of `from`, a file or a folder and all it contains, copied or moved into the folder `into` of the tree. A name
// the folder holds already takes a number, `notes (2).txt` and on, and the pairs go out as `[{"from", "to"}]`. Then
// the same copy as a paste makes, with a report for each file as it copies. Then the tree's path for `name` in `into`,
// with a number where it holds it already.
//
// Then `size` bytes of `data` written at `at` of the file at the tree's `path` as it comes a part at a time. The last
// part moves it over the file. Last, each of `from`, on this machine, copied into `into` of a tree on another machine
// through `call`. Each returns 1, or 0 with why given (files_paste.c, files_fetch.c)
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
    unsigned int max;
    int fuzzy;
    const char *const *files_characters;
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
    const char *query;
    const char *const *recent;
    unsigned int recents;
    unsigned int max;
    int fuzzy;
    void (*each)(const CliFilesRankedEachCfg *args);
    void *context;
};

struct CliFilesRankedAtCfg
{
    const CliFilesRanked *ranked;
    unsigned int index;
    CliFileRanked *row_out;
    char *path_out;
    unsigned int *hits_out;
    unsigned int hits_max;
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
    const CellOwner *owner;
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
    int located;
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
    void (*each)(const CliFilesEachCfg *args);
    void *context;
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
    unsigned int *characters;
    unsigned int capacity;
};

struct CliFilesCfg
{
    const CliFiles *files;
    unsigned int index;
};

struct CliFilesViewCfg
{
    const CliPicked *view;
    unsigned int index;
    char *path_out;
    const char *path;
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
    int (*each)(const CliFilesRootPathsCfg *args);
    void (*ignored_read)(const CliFilesRootPathsCfg *args);
    int (*types_each)(const CliFilesRootPathsCfg *args);
    void (*warm)(const CliFilesRootPathsCfg *args);
    void (*read_behind)(const CliFilesRootPathsCfg *args);
    int (*unread)(const CliFilesRootPathsCfg *args);
} CliFilesRootPathsNs;
EMBED_TABLE_LAYOUT(CliFilesRootPathsNs, each, ignored_read, types_each, warm, read_behind, unread);
EMBED_TABLE_STORAGE CliFilesRootPathsNs cli_files_root_paths EMBED_UNUSED = {
    .each = cli_files_each,
    .ignored_read = cli_files_ignored_read,
    .types_each = cli_files_types_each,
    .warm = cli_files_warm,
    .read_behind = cli_files_read_behind,
    .unread = cli_files_unread,
};

typedef struct
{
    int (*ranked_find)(const CliFilesRootRowsCfg *args);
    int (*ranked_at)(const CliFilesRankedAtCfg *args);
} CliFilesRootRowsNs;
EMBED_TABLE_LAYOUT(CliFilesRootRowsNs, ranked_find, ranked_at);
EMBED_TABLE_STORAGE CliFilesRootRowsNs cli_files_root_rows EMBED_UNUSED = {
    .ranked_find = cli_files_ranked_find,
    .ranked_at = cli_files_ranked_at,
};

typedef struct
{
    const char *(*at)(const CliFilesViewCfg *args);
    int (*contains)(const CliFilesViewCfg *args);
} CliFilesViewReadNs;
EMBED_TABLE_LAYOUT(CliFilesViewReadNs, at, contains);
EMBED_TABLE_STORAGE CliFilesViewReadNs cli_files_view_read EMBED_UNUSED = {
    .at = cli_files_view_at,
    .contains = cli_files_view_contains,
};

typedef struct
{
    const char *(*at)(const CliFilesCfg *args);
} CliFilesNs;
EMBED_TABLE_LAYOUT(CliFilesNs, at);
EMBED_TABLE_STORAGE CliFilesNs cli_files EMBED_UNUSED = {
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
EMBED_TABLE_STORAGE CliFilesQueryLettersNs cli_files_query_characters EMBED_UNUSED = {
    .want = cli_files_want,
};

typedef struct
{
    unsigned int (*characters)(const CliFilesTextCfg *args);
} CliFilesTextNs;
EMBED_TABLE_LAYOUT(CliFilesTextNs, characters);
EMBED_TABLE_STORAGE CliFilesTextNs cli_files_text EMBED_UNUSED = {
    .characters = cli_files_characters,
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

// The Search pane as the window lists it (files_search_pane.c). It lists every line of the tree's files that contains
// the typed text. A file has a row with its count, and each line a row under it, the match marked. Case, whole words
// and regular expressions each turn on and off beside the field. Under it, the search covers the whole tree or a set
// of files Preferences lists under `search.sets`. A set is `<name>:` on a line and its patterns on the lines after,
// and the choice goes in `search.set`.
//
// The search waits CLI_SEARCH_REST_MS after the last key before it runs over the tree. A choice flipped searches at
// once, and the search runs again as files git sees change. A file's row closes and opens its lines, and that stays
// with the file across searches. A line's row opens the file at the match.
//
// The max bytes a query takes with its end, and the rest after a key before the search. Then the max bytes of the
// report with its end, and the max sets Preferences lists. Last, the max bytes of a set's name with its end
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

// the max bytes of a line row's text with its end
#define CLI_SEARCH_ROW_TEXT 1024u

// A row of the pane. It holds its type and the file's path from the tree's top. A file row holds its name with its
// bytes and the bytes of its folder before the name. It also holds its count of lines found, and whether its lines
// show. A line row
// holds its line and column counted from 1, its text with the text's bytes, and the match's place and bytes. The text
// and the path are copies in the row's own bytes
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

// What wakes the window as the watch marks the search stale, `woken`, from the watch's thread. Then the set chosen,
// restored from the saved `entries`, which the read of the sets uses after
void cli_search_pane_begin(const CliSearchPaneCfg *args);
// `size` bytes of `text` typed at the query's end at `now_ms`. Then the key `name` pressed in the field at `now_ms`.
// Backspace erases the query's last character, and Enter searches at the next step: 1 where the key was one of
// those. Last, the choice `which` flipped, or the set moved to the next, at `now_ms`, which searches at the next step
void cli_search_pane_type(const CliSearchPaneCfg *args);
int cli_search_pane_key(const CliSearchPaneCfg *args);
void cli_search_pane_option(const CliSearchPaneCfg *args);
// Whether the choice `which` is on, and the query with its bytes into `length_out`. Then the set chosen's name, Whole
// tree where none is, with its bytes into `length_out` and how many sets Preferences lists into `count_out`
int cli_search_pane_on(const CliSearchPaneCfg *args);
unsigned long long cli_search_pane_query(const CliSearchPaneCfg *args);
const char *cli_search_pane_set(const CliSearchPaneCfg *args);
// One step of the window at `now_ms` for the tree at `root`. The search runs where a change waited its rest, where a
// key asked for it, or where the watch marked the search stale. Returns 1 where the rows changed. Then when the next
// search falls, 0 where none waits. Last, the search marked stale, from the watch's thread
int cli_search_pane_step(const CliSearchPaneCfg *args);
unsigned long long cli_search_pane_deadline_ms(const CliSearchPaneCfg *args);
void cli_search_pane_stale(const CliSearchPaneCfg *args);
// How many rows there are, and the row `at` into `row`: 1, or 0 past the last. Then the file row `at` closed or
// opened. Last, the report with its bytes into `length_out`: the results and files counted, that the search found
// none, or why it failed. It is empty where the query is
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
