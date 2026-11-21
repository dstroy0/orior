// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols.h: what the files of a tree declare, for Go to Symbol in the tree and Search Everywhere: each file read off
// the leaves of the grammar of the plugin its extension opens it as (symbols_read.c). The filesystem is the tree's
// trunk and its files are its branches, each stored under its closest parent with its declarations and its stamp. It is
// listed and walked one time as it opens, on a thread of its own, and a search requested meanwhile answers from what is
// walked so far and reports so. Each change after that joins, leaves or walks the one branch it names (symbols.c)
#ifndef SYMBOLS_H
#define SYMBOLS_H
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "../text/edits.h"
#include "embed_dispatch_layout.h"

typedef struct CliSymbolsPathCfg CliSymbolsPathCfg;
typedef struct CliSymbolsPathTextCfg CliSymbolsPathTextCfg;
typedef struct CliSymbolsPathContextCfg CliSymbolsPathContextCfg;
typedef struct CliSymbolsPathTextSizeCfg CliSymbolsPathTextSizeCfg;
typedef struct CliSymbolsPathTextSizeContextCfg CliSymbolsPathTextSizeContextCfg;
typedef struct CliSymbolsPathTextSizeOutCfg CliSymbolsPathTextSizeOutCfg;
typedef struct CliSymbolsPathTextSizeContextErrMsgCfg CliSymbolsPathTextSizeContextErrMsgCfg;
typedef struct CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionCfg
    CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionCfg;
typedef struct CliSymbolsPathContextLineCfg CliSymbolsPathContextLineCfg;
typedef struct CliSymbolsPathTextSizeStateCfg CliSymbolsPathTextSizeStateCfg;
typedef struct CliSymbolsPathNameCfg CliSymbolsPathNameCfg;
typedef struct CliSymbolsQueryCfg CliSymbolsQueryCfg;
typedef struct CliSymbolsRootCfg CliSymbolsRootCfg;
typedef struct CliSymbolsQueryMaxCfg CliSymbolsQueryMaxCfg;
typedef struct CliSymbolsContextCfg CliSymbolsContextCfg;
typedef struct CliSymbolsCountCfg CliSymbolsCountCfg;
typedef struct CliSymbolsAtCfg CliSymbolsAtCfg;
typedef struct CliSymbolsDoneCfg CliSymbolsDoneCfg;

// one declaration found: its name, `length` bytes, what it is, and its line counted from 0
typedef void (*CliDeclared)(const CliDeclaredCfg *args);

// whether the tree reads the declarations of the file at `path`: whether the tree reads the file's language
int cli_symbols_reads(const CliSymbolsPathNameCfg *args);
// the declarations of `size` bytes of `text`, read in the language of the file at `path`, each passed to `each`
void cli_symbols_declare(const CliSymbolsPathTextSizeCfg *args);
// How well `query` names `name`, higher the better, into `points`. The name itself ranks first, then a name it begins,
// then a name with it inside. Next comes a name whose words it starts, as `gtf` starts `go_to_file` and `goToFile`.
// Last comes a name with its characters in order. The rank ignores case, and of two names that answer alike the shorter
// comes first. 1, or 0 where it does not name it
int cli_symbols_score(const CliSymbolsQueryCfg *args);
// every branch forgotten, for a tree opened in place of the last
void cli_tree_forgotten(void);
// the tree at `root` listed, one time as it opens: every file git tracks or would track. For a tree git cannot read, it
// lists every file less `.git` and `target`. Each file is a branch under its closest parent, at max CLI_BRANCHES_MAX of
// them. A tree with a listing already keeps it. Then: whether the tree has a listing, without a new one; whether git
// gave the listing; and the time of the last listing, in milliseconds
void cli_tree_list(const CliSymbolsContextCfg *args);
int cli_tree_is_list(const CliSymbolsRootCfg *args);
int cli_tree_git(void);
unsigned long long cli_tree_listing_at(void);
// the files of the tree's listing: how many, and the bytes of their paths with their ends. Then the next file at or
// past `*at`, from 0: its path into `path`, sized for CLI_PATH_MAX_LEN, and `*at` moved past it. 1, or 0 past the last.
// Then whether a branch of the listed tree is at `path`, for a file and for a folder
void cli_tree_files_count(const CliSymbolsCountCfg *args);
int cli_tree_next_file(const CliSymbolsAtCfg *args);
int cli_tree_has_file(const CliSymbolsPathNameCfg *args);
int cli_tree_has_folder(const CliSymbolsPathNameCfg *args);
// a change the tree tells of: its path from the tree's top, and its type, a CLI_CHANGE_ number
typedef void (*CliTreeChange)(const CliEachPathTypeCfg *args);
// what the trunk keeps now at each of `paths`, `count` of them, mirrored to its closest parent. A file git does not
// ignore joins, and the walk reads that file and not the tree. A folder's own files join. A path with nothing there
// goes, and then each folder with nothing under it goes in turn
void cli_tree_mirror(const CliSymbolsRootCfg *args);
// the tree at `root` listed again, as a listing's rules change or the watch loses changes. Each file in the new listing
// joins, and each file out of it goes. Each goes to `emit` where the caller gives one, and no other branch changes
void cli_tree_relist(const CliSymbolsContextCfg *args);
// each file of the tree stamped where nothing reports its changes, each stamp that moved passed to `emit`
void cli_tree_stamp(const CliSymbolsContextCfg *args);
// the tree's branches walked on a thread of its own, one time as it opens, and again only over a mount added or taken
void cli_tree_on_walk(const CliSymbolsRootCfg *args);
// the branches `paths`, `count` of them, each of a file that changed, walked in this call
void cli_tree_branches_walk(const CliSymbolsRootCfg *args);
// the progress of the first read of the tree's files, `done` of `total`: 1 while the read runs, else 0
int cli_tree_walk_progress(const CliSymbolsDoneCfg *args);
// the read that runs stopped, with what it read kept
void cli_tree_walk_stop(void);
// whether the first read of the tree's files still runs
int cli_tree_unwalked(void);
// One declaration a query finds, passed to a caller with its `context`. It gives its name, `name_length` bytes, and
// what it is. It gives its file's tree path and its own line of text, `text_length` bytes. A zero ends each text, and
// the line counts from 0. The texts are the search's own for the one call
typedef struct
{
    void *context;
    const char *name;
    unsigned long long name_length;
    const char *type;
    const char *path;
    const char *text;
    unsigned long long text_length;
    unsigned int line;
} CliEachFoundCfg;
// the declarations whose names answer `query`, at max `max` of them, each passed to `each` with `context`, the best
// first. 1 where the first read of the tree still runs, else 0 (symbols_find.c)
int cli_tree_declarations_each(const CliSymbolsQueryMaxCfg *args);
// The names the tree holds for the file at `path`, as its last walk or save joined the file's branch. Each name goes to
// `each` with `context`. The read copies the branch's cell once and walks no text. Returns 1, or 0 where the tree holds
// no branch at `path` or the cells cannot meet the copy (symbols_find.c)
typedef struct
{
    const char *path;
    void (*each)(const CliEachTextLengthCfg *args);
    void *context;
} CliSymbolsPathEachCfg;
int cli_tree_names_each(const CliSymbolsPathEachCfg *args);
// the signature of the call the cursor is in at `line`, `col` in UTF-16 units of `size` bytes of `text`. The text is
// the lines the page passes from the file at `path`, and the first starts in its grammar's `state`. The call writes
// `{"name", "active", "arguments": [{"text", "type"}], "declarations": [{"path", "line", "label", "params":
// [[from, to]], "types", "count", "variadic", "differs"}], "reading"}`.
//
// The declarations are every declaration of the call's name in the tree, in order of fit. The call writes `null` where
// the cursor is in no call. It also writes
// `null` where the tree does not read the file's language. 1, or 0 with why where the cells cannot meet it.
//
// The call also writes the first declaration that fits the call into `store` as a panel. The panel marks the call's
// parameter and the parameters the arguments differ from. 1, or 0 with why where the cursor is in no call. It also
// gives 0 where the tree knows no declaration or the cells cannot meet the read (symbols_signature.c)
int cli_symbols_signature_read(const CliSymbolsPathTextSizeOutCfg *args);
// the line of no branch, for a bracket the slice closes. Then the call that takes each line of a slice: the path of the
// branch it came from, or NULL. Its line there, or CLI_SLICE_NOWHERE, and its text, `length` bytes. Last, the same
// slice passed to `each` a line at a time, in place of a write (symbols_slice.c)
#define CLI_SLICE_NOWHERE 0xffffffffu
typedef void (*CliSliceLine)(const CliSliceLineCfg *args);
int cli_symbols_slice_each(const CliSymbolsPathCfg *args);
// the call that takes each window of a branch's slices after the slice's lines. It gets the window's first and last
// lines. It also gets the lines that open the regions around the first, outermost first, and their count. It returns 1
// to cut the next window, 0 to stop.
//
// Then the branch at `path`, `size` bytes of `text`, cut in one walk into the slices of its windows. Each window is
// `height` lines, from the branch's first line. The walk cuts each window with a leaf as it reaches it. The brackets of
// the active lines above the window give its openers. Its slice goes to `each` a line at a time, then to `window`. 1,
// or 0 with why where the cells cannot meet the walk (symbols_slice.c)
typedef int (*CliSliceWindow)(const CliSliceWindowCfg *args);
int cli_symbols_slices_each(const CliSymbolsPathTextSizeContextErrMsgCfg *args);

// one usage of a name: the tree's file it is in, by its path in the tree, and its line counted from 0. Its span from
// `from` to `to` in UTF-16 units, and its line's text, `length` bytes. Then the editor's text of the tree's file at
// `path`, with its size into `size`. The caller gives the cell back with g_cli_guard->persistent_buf_release. NULL
// where the editor has no text of the file
typedef void (*CliUsage)(const CliUsageCfg *args);
typedef char *(*CliOpenText)(const CliOpenTextCfg *args);
// the usages of the name at `line`, `col` in UTF-16 units of the tree's file at `path`, each passed to `each`. The
// place's own usage goes too. Each file's text comes from `open_text`, else from the disk. For a local or a parameter,
// the usages are the leaves of its name in the function that declares it. For any other name, they are its leaves with
// the same definition in effect as the place. Where `across` is 0 they come from the file at `path` alone, else from
// every file of the tree.
//
// 1, or 0 with nothing passed on where the place names no name. 0 also where the tree knows no definition of it. -1
// with why where the cells cannot meet it (symbols_usages.c)
int cli_symbols_usages(const CliSymbolsPathContextCfg *args);
// the definition in effect of the name at `line`, `col` in UTF-16 units of the tree's file at `path`, passed to `each`
// as a usage. For a local or a parameter, it is the first leaf in the function that declares it. For any other name, it
// is the leaf on the line of the define in effect, or of the first declaration the walk met. That leaf is in the
// declaration's own file. Each file's text comes from `open_text`.
//
// 1, or 0 with nothing passed on where the place names no name or the tree knows no definition of it. A define from the
// compile database has no definition here. -1 with why where the cells cannot meet it (symbols_usages.c)
int cli_symbols_definition(const CliSymbolsPathContextCfg *args);
// the names that complete the word that ends at `line`, `col` in UTF-16 units of `size` bytes of `text`. The text is
// the editor's text of the tree's file at `path`. Each name goes to `each` once, with what it is and its line. First
// come the names of the file's branch in the tree, as its last walk or save joined them. Then come the names on the
// line under edit, a parameter or a local by its leaf.
//
// Each name begins with the word, and all names go where no word ends there. The call reads no text past the line under
// edit. 1, or 0 with why where the cells cannot meet the names (symbols_complete.c)
int cli_symbols_complete(const CliSymbolsPathTextSizeCfg *args);
// one hint: the line and the column in UTF-16 units it comes before. Then the name of the parameter that takes the
// argument there, `length` bytes
typedef void (*CliHinted)(const CliHintedCfg *args);
// the hints on the lines `from` to `to` of `size` bytes of `text`, the editor's text of the tree's file at `path`. At
// each call there, a hint comes before each argument. It names the parameter of the call's definition in effect that
// takes the argument. A hint needs a parameter with a name, and an argument other than that name alone. Each hint goes
// to `each` in the order of the text. 1, or 0 with why where the cells cannot meet it (symbols_hints.c)
int cli_symbols_hints(const CliSymbolsPathTextSizeContextCfg *args);
// a place in a file: its line counted from 0 and its column in UTF-16 units
typedef struct
{
    unsigned int line;
    unsigned int col;
} CliSymbolPosition;
typedef struct
{
    void *context;
    const char *path;
    unsigned int line;
    unsigned int from;
    unsigned int to;
    const char *name;
    unsigned long long length;
    const CliSymbolPosition *sites;
    unsigned int count;
} CliCallReachedCfg;

// one function a call hierarchy reaches: the tree's file that declares it, by its path in the tree. Its name's line,
// and its span there from `from` to `to` in UTF-16 units. Its name, `length` bytes, and the places of the calls that
// lead there, `count` of them
typedef void (*CliCallReached)(const CliCallReachedCfg *args);
// the call hierarchy over the tree's files. Each file's text comes from `open_text`, else from the disk. The first call
// passes to `each` the function of the file at `path` with its name or body on line `line`, with no calls. The second
// passes each function that calls the function named on that line, with the places of its calls in its file. A call is
// a usage of the name with a bracket `(` after it.
//
// The third passes each function it calls, with the places of those calls. Each of those is the definition in effect of
// a call in its body. A function is a region that a bracket `{` opens at no depth. Its head has a bracket `(` after a
// leaf that declares its name. Each returns 1, 0 where no function is there, or -1 with why (symbols_calls.c)
int cli_symbols_function_at(const CliSymbolsPathContextLineCfg *args);
int cli_symbols_callers(const CliSymbolsPathContextLineCfg *args);
int cli_symbols_callees(const CliSymbolsPathContextLineCfg *args);
typedef struct
{
    void *context;
    CliSymbolPosition from;
    CliSymbolPosition to;
} CliSpannedCfg;

// one span Expand Selection steps through, from `from` to `to`
typedef void (*CliSpanned)(const CliSpannedCfg *args);
// the spans of `size` bytes of `text`, the editor's text of the tree's file at `path`, that contain `from` to `to` and
// are more than it. Each goes to `each` once, the least first. First is the leaf the place is in. Then each bracket
// pair around it, inside its brackets and with them. Then the statement it is in among a region's, and each region with
// its head. Last is the whole text.
//
// The roles the grammar names give each span. 1, or 0 with why where the cells cannot meet them (symbols_spans.c)
int cli_symbols_spans(const CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionCfg *args);
// the bracket pair at `at` in `size` bytes of `text`, the editor's text of the tree's file at `path`. The first line
// starts in its grammar's `state`. The bracket that ends at the place goes into `pair[0]`, else the bracket that starts
// there. Its other bracket goes into `pair[1]`.
//
// Each is a leaf that opens or closes by the roles its grammar names. A string's leaf or a comment's leaf is no
// bracket. The pairs are the pairs of the spans. 1, or 0 where no bracket of a pair is there. A mark of no characters
// closes no pair. 0 also where the cells cannot meet the leaves (symbols_spans.c)
int cli_symbols_pair(const CliSymbolsPathTextSizeStateCfg *args);
// the roles a token's name gives it in a walk, read off its segments as for a leaf. It opens a region, or it closes
// one. It closes a bracket pair, ends a statement, or divides one. It is a bare name and has no other role. It declares
// a local, or a parameter.
//
// It ends a statement, or it is a directive's. It is an operator on memory that binds no two values, and the name after
// it comes through it. It opens a pair whose depth it counts, or it closes one. It names a thing, with any other role
// too. It is a string's or a comment's, and then it has no other role (symbols_leaves.c)
#define CLI_ROLE_REGION_OPENS 0x01u
#define CLI_ROLE_REGION_CLOSES 0x02u
#define CLI_ROLE_CLOSING 0x04u
#define CLI_ROLE_NAME 0x08u
#define CLI_ROLE_LOCAL 0x10u
#define CLI_ROLE_PARAMETER 0x20u
#define CLI_ROLE_ENDS 0x40u
#define CLI_ROLE_REACHES 0x80u
#define CLI_ROLE_DEPTH_OPENS 0x100u
#define CLI_ROLE_DEPTH_CLOSES 0x200u
#define CLI_ROLE_WORD 0x400u
#define CLI_ROLE_QUOTED 0x800u
// the roles that count the depth of a pair
#define CLI_ROLE_PAIR (CLI_ROLE_DEPTH_OPENS | CLI_ROLE_DEPTH_CLOSES)
unsigned int cli_symbols_name_roles(const CliSymbolsQueryCfg *args);

struct CliSymbolsPathCfg
{
    const char *path;
    const char *text;
    unsigned long long size;
    void *context;
    VerbaLinea *err_msg;
    unsigned int first;
    unsigned int last;
    const unsigned int *opener;
    unsigned int openers;
    CliSliceLine each_slice_line;
};

struct CliSymbolsPathTextCfg
{
    const char *path;
    const char *text;
    unsigned long long size;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned int first;
    unsigned int last;
    const unsigned int *opener;
    unsigned int openers;
};

struct CliSymbolsPathContextCfg
{
    const char *path;
    void *context;
    unsigned int line;
    unsigned int col;
    VerbaLinea *err_msg;
    int across;
    CliOpenText open_text;
    void *open_text_context;
    CliUsage each_usage;
};

struct CliSymbolsPathTextSizeCfg
{
    const char *path;
    const char *text;
    unsigned long long size;
    CliDeclared each_declared;
    void *context;
    unsigned int line;
    unsigned int col;
    VerbaLinea *err_msg;
};

struct CliSymbolsPathTextSizeContextCfg
{
    const char *path;
    const char *text;
    unsigned long long size;
    void *context;
    VerbaLinea *err_msg;
    unsigned int from;
    unsigned int to;
    CliHinted each_hinted;
};

struct CliSymbolsPathTextSizeOutCfg
{
    const char *path;
    const char *text;
    unsigned long long size;
    const CellOwner *owner;
    const char *state;
    unsigned int line;
    unsigned int col;
    VerbaLinea *err_msg;
    CliSignatureStore *store;
};

struct CliSymbolsPathTextSizeContextErrMsgCfg
{
    const char *path;
    const char *text;
    unsigned long long size;
    void *context;
    VerbaLinea *err_msg;
    CliSliceLine each_slice_line;
    unsigned int height;
    CliSliceWindow window;
};

struct CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionCfg
{
    const char *path;
    const char *text;
    unsigned long long size;
    void *context;
    VerbaLinea *err_msg;
    CliSymbolPosition from_symbol_position;
    CliSymbolPosition to_symbol_position;
    CliSpanned each_spanned;
};

struct CliSymbolsPathContextLineCfg
{
    const char *path;
    void *context;
    unsigned int line;
    VerbaLinea *err_msg;
    CliOpenText open_text;
    void *open_text_context;
    CliCallReached each;
};

struct CliSymbolsPathTextSizeStateCfg
{
    const char *path;
    const char *text;
    unsigned long long size;
    const char *state;
    CliSymbolPosition at_symbol_position;
    CliSymbolPosition *pair;
    int *bracket_pointer; /**< Set to whether a bracket is at the position, where the caller gives one. */
};

struct CliSymbolsPathNameCfg
{
    const char *path;
    const char *name;
    VerbaLinea *out;
    unsigned int line;
    VerbaLinea *err_msg;
};

struct CliSymbolsQueryCfg
{
    const char *query;
    const char *name;
    unsigned long long name_length;
    long long *points;
};

struct CliSymbolsRootCfg
{
    const char *root;
    const char *const *paths;
    unsigned int count;
};

struct CliSymbolsQueryMaxCfg
{
    const char *query;
    unsigned int max;
    void (*each)(const CliEachFoundCfg *args);
    void *context;
};

struct CliSymbolsContextCfg
{
    void *context;
    const char *root;
    CliTreeChange emit;
};

struct CliSymbolsCountCfg
{
    unsigned int *count_pointer;
    unsigned long long *bytes;
};

struct CliSymbolsAtCfg
{
    unsigned int *at;
    char *path_out;
};

struct CliSymbolsDoneCfg
{
    unsigned long long *done;
    unsigned long long *total;
};

typedef struct
{
    int (*reads)(const CliSymbolsPathNameCfg *args);
    int (*tree_has_file)(const CliSymbolsPathNameCfg *args);
    int (*tree_has_folder)(const CliSymbolsPathNameCfg *args);
} CliSymbolsPathNameNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathNameNs, reads, tree_has_file, tree_has_folder);
EMBED_TABLE_STORAGE CliSymbolsPathNameNs cli_symbols_path_name EMBED_UNUSED = {
    .reads = cli_symbols_reads,
    .tree_has_file = cli_tree_has_file,
    .tree_has_folder = cli_tree_has_folder,
};

typedef struct
{
    void (*declare)(const CliSymbolsPathTextSizeCfg *args);
    int (*complete)(const CliSymbolsPathTextSizeCfg *args);
} CliSymbolsPathTextSizeNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathTextSizeNs, declare, complete);
EMBED_TABLE_STORAGE CliSymbolsPathTextSizeNs cli_symbols_path_text_size EMBED_UNUSED = {
    .declare = cli_symbols_declare,
    .complete = cli_symbols_complete,
};

typedef struct
{
    int (*score)(const CliSymbolsQueryCfg *args);
    unsigned int (*name_roles)(const CliSymbolsQueryCfg *args);
} CliSymbolsQueryNs;
EMBED_TABLE_LAYOUT(CliSymbolsQueryNs, score, name_roles);
EMBED_TABLE_STORAGE CliSymbolsQueryNs cli_symbols_query EMBED_UNUSED = {
    .score = cli_symbols_score,
    .name_roles = cli_symbols_name_roles,
};

typedef struct
{
    void (*tree_list)(const CliSymbolsContextCfg *args);
    void (*tree_relist)(const CliSymbolsContextCfg *args);
    void (*tree_stamp)(const CliSymbolsContextCfg *args);
} CliSymbolsContextNs;
EMBED_TABLE_LAYOUT(CliSymbolsContextNs, tree_list, tree_relist, tree_stamp);
EMBED_TABLE_STORAGE CliSymbolsContextNs cli_symbols_context EMBED_UNUSED = {
    .tree_list = cli_tree_list,
    .tree_relist = cli_tree_relist,
    .tree_stamp = cli_tree_stamp,
};

typedef struct
{
    int (*tree_is_list)(const CliSymbolsRootCfg *args);
    void (*tree_mirror)(const CliSymbolsRootCfg *args);
    void (*tree_walk)(const CliSymbolsRootCfg *args);
    void (*tree_branches_walk)(const CliSymbolsRootCfg *args);
} CliSymbolsRootNs;
EMBED_TABLE_LAYOUT(CliSymbolsRootNs, tree_is_list, tree_mirror, tree_walk, tree_branches_walk);
EMBED_TABLE_STORAGE CliSymbolsRootNs cli_symbols_root EMBED_UNUSED = {
    .tree_is_list = cli_tree_is_list,
    .tree_mirror = cli_tree_mirror,
    .tree_walk = cli_tree_on_walk,
    .tree_branches_walk = cli_tree_branches_walk,
};

typedef struct
{
    void (*tree_files_count)(const CliSymbolsCountCfg *args);
} CliSymbolsCountNs;
EMBED_TABLE_LAYOUT(CliSymbolsCountNs, tree_files_count);
EMBED_TABLE_STORAGE CliSymbolsCountNs cli_symbols_count EMBED_UNUSED = {
    .tree_files_count = cli_tree_files_count,
};

typedef struct
{
    int (*tree_next_file)(const CliSymbolsAtCfg *args);
} CliSymbolsAtNs;
EMBED_TABLE_LAYOUT(CliSymbolsAtNs, tree_next_file);
EMBED_TABLE_STORAGE CliSymbolsAtNs cli_symbols_at EMBED_UNUSED = {
    .tree_next_file = cli_tree_next_file,
};

typedef struct
{
    int (*tree_walk_progress)(const CliSymbolsDoneCfg *args);
} CliSymbolsDoneNs;
EMBED_TABLE_LAYOUT(CliSymbolsDoneNs, tree_walk_progress);
EMBED_TABLE_STORAGE CliSymbolsDoneNs cli_symbols_done EMBED_UNUSED = {
    .tree_walk_progress = cli_tree_walk_progress,
};

typedef struct
{
    int (*tree_declarations_each)(const CliSymbolsQueryMaxCfg *args);
} CliSymbolsQueryMaxNs;
EMBED_TABLE_LAYOUT(CliSymbolsQueryMaxNs, tree_declarations_each);
EMBED_TABLE_STORAGE CliSymbolsQueryMaxNs cli_symbols_query_max EMBED_UNUSED = {
    .tree_declarations_each = cli_tree_declarations_each,
};

typedef struct
{
    int (*names_each)(const CliSymbolsPathEachCfg *args);
} CliSymbolsPathEachNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathEachNs, names_each);
EMBED_TABLE_STORAGE CliSymbolsPathEachNs cli_symbols_path_each EMBED_UNUSED = {
    .names_each = cli_tree_names_each,
};

typedef struct
{
    int (*signature_read)(const CliSymbolsPathTextSizeOutCfg *args);
} CliSymbolsPathTextSizeOutNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathTextSizeOutNs, signature_read);
EMBED_TABLE_STORAGE CliSymbolsPathTextSizeOutNs cli_symbols_path_text_size_out EMBED_UNUSED = {
    .signature_read = cli_symbols_signature_read,
};

typedef struct
{
    int (*slice_each)(const CliSymbolsPathCfg *args);
} CliSymbolsPathNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathNs, slice_each);
EMBED_TABLE_STORAGE CliSymbolsPathNs cli_symbols_path EMBED_UNUSED = {
    .slice_each = cli_symbols_slice_each,
};

typedef struct
{
    int (*slices_each)(const CliSymbolsPathTextSizeContextErrMsgCfg *args);
} CliSymbolsPathTextSizeContextErrMsgNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathTextSizeContextErrMsgNs, slices_each);
EMBED_TABLE_STORAGE CliSymbolsPathTextSizeContextErrMsgNs cli_symbols_path_text_size_context_err_msg EMBED_UNUSED = {
    .slices_each = cli_symbols_slices_each,
};

typedef struct
{
    int (*usages)(const CliSymbolsPathContextCfg *args);
    int (*definition)(const CliSymbolsPathContextCfg *args);
} CliSymbolsPathContextNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathContextNs, usages, definition);
EMBED_TABLE_STORAGE CliSymbolsPathContextNs cli_symbols_path_context EMBED_UNUSED = {
    .usages = cli_symbols_usages,
    .definition = cli_symbols_definition,
};

typedef struct
{
    int (*hints)(const CliSymbolsPathTextSizeContextCfg *args);
} CliSymbolsPathTextSizeContextNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathTextSizeContextNs, hints);
EMBED_TABLE_STORAGE CliSymbolsPathTextSizeContextNs cli_symbols_path_text_size_context EMBED_UNUSED = {
    .hints = cli_symbols_hints,
};

typedef struct
{
    int (*function_at)(const CliSymbolsPathContextLineCfg *args);
    int (*callers)(const CliSymbolsPathContextLineCfg *args);
    int (*callees)(const CliSymbolsPathContextLineCfg *args);
} CliSymbolsPathContextLineNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathContextLineNs, function_at, callers, callees);
EMBED_TABLE_STORAGE CliSymbolsPathContextLineNs cli_symbols_path_context_line EMBED_UNUSED = {
    .function_at = cli_symbols_function_at,
    .callers = cli_symbols_callers,
    .callees = cli_symbols_callees,
};

typedef struct
{
    int (*spans)(const CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionCfg *args);
} CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionNs, spans);
EMBED_TABLE_STORAGE CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionNs
    cli_symbols_path_text_size_context_err_msg_from_symbol_position EMBED_UNUSED = {
        .spans = cli_symbols_spans,
};

typedef struct
{
    int (*pair)(const CliSymbolsPathTextSizeStateCfg *args);
} CliSymbolsPathTextSizeStateNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathTextSizeStateNs, pair);
EMBED_TABLE_STORAGE CliSymbolsPathTextSizeStateNs cli_symbols_path_text_size_state EMBED_UNUSED = {
    .pair = cli_symbols_pair,
};

// The outline, the declarations of the view displayed as rows (symbols_outline.c). A row is a declaration's name,
// `length` bytes, what it is, and its line counted from 0. It also gives its depth: its line's indent in steps of the
// text's smallest indent, to CLI_OUTLINE_DEEPEST. A text longer than CLI_OUTLINE_LINES_MAX lines has no rows, and the
// read stops at CLI_OUTLINE_ROWS
#define CLI_OUTLINE_DEEPEST 5u
#define CLI_OUTLINE_ROWS 4096u
#define CLI_OUTLINE_LINES_MAX 50000ULL
typedef struct
{
    char name[CLI_PATH_MAX_LEN];
    unsigned long long length;
    char type[CLI_NAME_MAX_LEN];
    unsigned long long line;
    unsigned int depth;
} CliOutlineRow;
typedef struct CliOutlineCfg CliOutlineCfg;

// The outline of the text under `key`, the file at `path`, in the language of the path, its rows in the order of their
// lines. 1 where the call reads it anew. 0 where the outline in place is of this text at its serial, and it stays. 0
// also where `key` is NULL or has no whole text under it, and then no outline stays
int cli_outline_read(const CliOutlineCfg *args);
// how many rows the outline has; and the row `at` into `row`: 1, or 0 past the last
unsigned int cli_outline_rows(const CliOutlineCfg *args);
int cli_outline_row(const CliOutlineCfg *args);
struct CliOutlineCfg
{
    const char *key;
    const char *path;
    unsigned int at;
    CliOutlineRow *row;
};

typedef struct
{
    int (*read)(const CliOutlineCfg *args);
    unsigned int (*rows)(const CliOutlineCfg *args);
    int (*row)(const CliOutlineCfg *args);
} CliOutlineNs;
EMBED_TABLE_LAYOUT(CliOutlineNs, read, rows, row);
EMBED_TABLE_STORAGE CliOutlineNs cli_outline EMBED_UNUSED = {
    .read = cli_outline_read,
    .rows = cli_outline_rows,
    .row = cli_outline_row,
};

#endif
