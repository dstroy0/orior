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
typedef struct CliSymbolsCfg CliSymbolsCfg;

// one declaration found: its name, `length` bytes, what it is, and its line counted from 0
typedef void (*CliDeclared)(const CliDeclaredCfg *args);

// The languages texts are read in, the language each file opens as and each language's grammar, read once for the
// texts of a request (symbols_read.c)
typedef struct CliSymbolsLanguages CliSymbolsLanguages;
// the languages read in a cell taken from the cells, NULL where it is refused, and given back with the cell
CliSymbolsLanguages *cli_symbols_languages_read(void);
CliSymbolsLanguages *cli_symbols_languages_for_call(void);

// whether the declarations of the file at `path` are read: whether its language is one read here
int cli_symbols_reads(const CliSymbolsPathNameCfg *args);
// the declarations of `size` bytes of `text`, read in the language of the file at `path`, each handed to `each`
void cli_symbols_declare(const CliSymbolsPathTextSizeCfg *args);
// How well `query` names `name`, higher the better, into `points`: the name itself first, then a name it begins, then
// one containing it, then one whose words it starts, as `gtf` starts `go_to_file` and `goToFile`, then one containing
// its letters in order. Case is set aside, and the shorter of two names that answer alike comes first. 1, or 0 where it
// does not name it
int cli_symbols_score(const CliSymbolsQueryCfg *args);
// every branch forgotten, for a tree opened in place of the last
void cli_tree_forgotten(void);
// the tree at `root` listed, one time as it opens: every file git tracks or would track, or for a tree git cannot read
// every file less `.git` and `target`, a branch under its closest parent, at most CLI_BRANCHES_MAX of them. Standing
// already, it is left as it stands. Whether it is listed, without listing it; whether git listed it; and when it was
// last listed, in milliseconds
void cli_tree_list(const CliSymbolsContextCfg *args);
int cli_tree_is_list(const CliSymbolsRootCfg *args);
int cli_tree_git(void);
unsigned long long cli_tree_listing_at(void);
// the files of the tree standing, how many and the bytes of their paths with their ends; the next file at or past
// `*at`, from 0, its path into `path`, sized for CLI_PATH_MAX_LEN, `*at` moved past it: 1, or 0 past the last; and
// whether a branch of the listed tree stands at `path`, a file and a folder
void cli_tree_files_count(const CliSymbolsCountCfg *args);
int cli_tree_next_file(const CliSymbolsAtCfg *args);
int cli_tree_has_file(const CliSymbolsPathNameCfg *args);
int cli_tree_has_folder(const CliSymbolsPathNameCfg *args);
// a change the tree tells of: its path from the tree's top, and its kind, a CLI_CHANGE_ number
typedef void (*CliTreeChange)(const CliEachPathKindCfg *args);
// what the trunk holds now at each of `paths`, `count` of them, mirrored to its closest parent: a file git does not
// ignore joined, the file walked and not the tree; a folder's own files joined; what is not there let go, each folder
// left with nothing under it let go in turn
void cli_tree_mirror(const CliSymbolsRootCfg *args);
// the tree at `root` listed again, as a listing's rules change or changes were lost: each file now listed joined and
// each no longer listed let go, each handed to `emit` where it is given, every other branch left as it stands
void cli_tree_relist(const CliSymbolsContextCfg *args);
// each file of the tree stamped where nothing reports its changes, each stamp that moved handed to `emit`
void cli_tree_stamp(const CliSymbolsContextCfg *args);
// the tree's branches walked on a thread of its own, one time as it opens, and again only over a mount added or taken
void cli_tree_on_walk(const CliSymbolsRootCfg *args);
// the branches `paths`, `count` of them, each of a file that changed, walked in this call
void cli_tree_branches_walk(const CliSymbolsRootCfg *args);
// how far the first reading of the tree's files has gone, `done` of `total`: 1 while it goes, else 0
int cli_tree_walk_progress(const CliSymbolsDoneCfg *args);
// the reading going ended, what it has read retained
void cli_tree_walk_stop(void);
// whether the tree's files are being read for the first time
int cli_tree_unwalked(void);
// the declarations whose names answer `query`, at most `most` of them, the best first, written as `{"symbols":
// [{"name", "kind", "path", "line", "text"}...], "reading"}`, `text` the declaration's own line trimmed, read from
// its file as the answer is written
typedef struct
{
    const char *name;
    const char *kind;
    const char *path;
    unsigned int line;
    const char *text;
} CliFoundDeclaration;
// The declarations a query finds in one cell, the best first, and whether the tree is still being read
typedef struct
{
    unsigned int count;
    int reading;
    CliFoundDeclaration *found;
} CliFoundDeclarations;
CliFoundDeclarations *cli_tree_declarations_alloc(const CliSymbolsQueryMaxCfg *args);
// the signature of the call the cursor stands in at `line`, `col` in UTF-16 units of `size` bytes of `text`, the lines
// the page hands from the file at `path`, the first starting in its grammar's `state`: written as `{"name", "active",
// "arguments": [{"text", "type"}], "declarations": [{"path", "line", "label", "params": [[from, to]], "types",
// "count", "variadic", "differs"}], "reading"}`, the declarations every one of the call's name in the tree in order of
// fit, or as `null` where the cursor stands in no call or the file's language is none read here: 1, or 0 with why
// given where the cells cannot meet it (symbols_signature.c)
CliSignaturePanel *cli_symbols_signature_alloc(const CliSymbolsPathTextSizeOutCfg *args);
// the line of no branch, a bracket the slice closes; what each line of a slice is handed to: the path of the branch it
// came from, NULL for none, its line there, CLI_SLICE_NOWHERE for none, and its text, `length` bytes; and the same
// slice handed to `each` a line at a time in place of being written (symbols_slice.c)
#define CLI_SLICE_NOWHERE 0xffffffffu
typedef void (*CliSliceLine)(const CliSliceLineCfg *args);
int cli_symbols_slice_each(const CliSymbolsPathCfg *args);
// what each window of a branch's slices is handed to once its slice's lines are: its first and last lines and the lines
// opening the regions its first stands in, outermost first, and how many: 1 to cut the next, 0 to stop; and the branch
// at `path`, `size` bytes of `text`, cut in one walk of it into the slices of its windows, `height` lines each from its
// first line, each window that holds a leaf cut as the walk reaches it, its openers read off the brackets of the active
// lines above it, its slice handed to `each` a line at a time and then to `window`: 1, or 0 with why given where the
// cells cannot meet the walk (symbols_slice.c)
typedef int (*CliSliceWindow)(const CliSliceWindowCfg *args);
int cli_symbols_slices_each(const CliSymbolsPathTextSizeContextErrMsgCfg *args);

// one usage of a name: the tree's file it stands in, by its path in the tree, its line counted from 0, its span from
// `from` to `to` in UTF-16 units, and its line's text, `length` bytes; and the text the editor holds of the tree's file
// at `path`, in a cell the caller gives back with g_cli_guard->persistent_buf_release, its size into `size`, NULL where
// the editor holds none
typedef void (*CliUsage)(const CliUsageCfg *args);
typedef char *(*CliOpenText)(const CliOpenTextCfg *args);
// the usages of the name at `line`, `col` in UTF-16 units of the tree's file at `path`, each handed to `each`, the
// place's own among them, each file's text the one `open_text` gives, else the disk's: a local's or a parameter's, the
// leaves of its name in the region of the function that declares it; any other name's, the leaves of it whose
// definition in effect is the one at the place, in the file at `path` alone where `across` is 0, else in every file of
// the tree. 1, or 0 where the place names no name or the tree knows no definition of it, nothing handed on, or -1 with
// why given where the cells cannot meet it (symbols_usages.c)
int cli_symbols_usages(const CliSymbolsPathContextCfg *args);
// the definition in effect of the name at `line`, `col` in UTF-16 units of the tree's file at `path`, handed to `each`
// as a usage: a local's or a parameter's first leaf in the region of the function that declares it, any other name's
// leaf on the line of the define in effect or of the first declaration the walk met, in that declaration's file, each
// file's text the one `open_text` gives. 1, or 0 where the place names no name or the tree knows no definition of it,
// as for a define the compile database gives, nothing handed on, or -1 with why given (symbols_usages.c)
int cli_symbols_definition(const CliSymbolsPathContextCfg *args);
// the names that complete the word that ends at `line`, `col` in UTF-16 units of `size` bytes of `text`, the editor's
// text of the tree's file at `path`, each handed to `each` once with what it is and the line it is declared on, in its
// own file: every define in effect at the line, then every declaration a walk of the file to the line met in an active
// region, then the parameters and the locals above the place of the function it stands in, each that begins with the
// word, the whole of each where no word ends there; a define the compilation database gives on line 0xffffffff. 1, or
// 0 with why given where the cells cannot meet the walk (symbols_complete.c)
int cli_symbols_complete(const CliSymbolsPathTextSizeCfg *args);
// one hint: the line and the column in UTF-16 units it stands before, and the name of the parameter the argument there
// is given to, `length` bytes
typedef void (*CliHinted)(const CliHintedCfg *args);
// the hints on the lines `from` to `to` of `size` bytes of `text`, the editor's text of the tree's file at `path`: at
// each call there, before each argument, the name of the parameter of the call's definition in effect it is given to,
// where that parameter has a name and the argument is not that name alone, each handed to `each` in the order of the
// text. 1, or 0 with why given where the cells cannot meet it (symbols_hints.c)
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

// one function a call hierarchy reaches: the tree's file it is declared in, by its path in the tree, its name's line
// and its span there from `from` to `to` in UTF-16 units, its name, `length` bytes, and the places of the calls that
// lead there, `count` of them
typedef void (*CliCallReached)(const CliCallReachedCfg *args);
// the call hierarchy over the tree's files, each file's text the one `open_text` gives, else the disk's: the function
// of the tree's file at `path` whose name or body stands on line `line`, handed to `each` with no calls; the functions
// that call the function whose name stands on that line, each with the places of its calls of it in its file, a call a
// usage of the name a bracket `(` follows; and the functions it calls, each the definition in effect of a call in its
// body, with the places of those calls in it. A function is a region its bracket `{` opens at no depth whose head
// holds a bracket `(` after a leaf that declares its name. Each 1, 0 where no function stands there, or -1 with why
// given (symbols_calls.c)
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
// the spans of `size` bytes of `text`, the editor's text of the tree's file at `path`, that hold `from` to `to` and are
// more than it, each handed to `each` once, the least first: the leaf the place stands in, each bracket pair around
// it, inside its brackets and with them, the statement it stands in among a region's, each region with its head, and
// the whole text; each read by the roles its grammar names. 1, or 0 with why given where the cells cannot meet them
// (symbols_spans.c)
int cli_symbols_spans(const CliSymbolsPathTextSizeContextErrMsgFromSymbolPositionCfg *args);
// the bracket pair at `at` in `size` bytes of `text`, the editor's text of the tree's file at `path`, the first line
// starting in its grammar's `state`: the bracket that ends at the place, else the one that starts at it, into `pair[0]`
// and the bracket it pairs with into `pair[1]`, each a leaf that opens or closes by the roles its grammar names and
// neither a string's nor a comment's, paired as the spans pair them: 1, or 0 where no bracket of a pair stands there,
// a pair a mark of no letters closes none, or the cells cannot meet the text's leaves (symbols_spans.c)
int cli_symbols_pair(const CliSymbolsPathTextSizeStateCfg *args);
// the roles a token's name gives it as a text is walked, read off its segments as a leaf's are: it opens a region, it
// closes one, or it closes a bracket pair, ends a statement or parts one; it is a bare name, nothing more; it declares
// a local, or a parameter; it ends a statement or is a directive's; it is an operator on memory that binds no two
// values, the name after it one reached through it; it opens a pair whose depth it counts, or closes one; and it names
// something, whatever else it does; none for a string's or a comment's (symbols_leaves.c)
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
    unsigned int most;
    const CellOwner *owner;
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

struct CliSymbolsCfg
{
    CliSymbolsLanguages *read;
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
    CliFoundDeclarations *(*tree_declarations_alloc)(const CliSymbolsQueryMaxCfg *args);
} CliSymbolsQueryMaxNs;
EMBED_TABLE_LAYOUT(CliSymbolsQueryMaxNs, tree_declarations_alloc);
EMBED_TABLE_STORAGE CliSymbolsQueryMaxNs cli_symbols_query_max EMBED_UNUSED = {
    .tree_declarations_alloc = cli_tree_declarations_alloc,
};

typedef struct
{
    CliSignaturePanel *(*signature_alloc)(const CliSymbolsPathTextSizeOutCfg *args);
} CliSymbolsPathTextSizeOutNs;
EMBED_TABLE_LAYOUT(CliSymbolsPathTextSizeOutNs, signature_alloc);
EMBED_TABLE_STORAGE CliSymbolsPathTextSizeOutNs cli_symbols_path_text_size_out EMBED_UNUSED = {
    .signature_alloc = cli_symbols_signature_alloc,
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

// The outline, the declarations of the view shown as rows (symbols_outline.c): a row is a declaration's name, `length`
// bytes, what it is, its line counted from 0, and how deep it stands, its line's indent in steps of the text's smallest
// indent, no deeper than CLI_OUTLINE_DEEPEST. A text longer than CLI_OUTLINE_LINES_MOST lines has no rows, and at most
// CLI_OUTLINE_ROWS are read
#define CLI_OUTLINE_DEEPEST 5u
#define CLI_OUTLINE_ROWS 4096u
#define CLI_OUTLINE_LINES_MOST 50000ULL
typedef struct
{
    const char *name;
    unsigned long long length;
    const char *kind;
    unsigned long long line;
    unsigned int depth;
} CliOutlineRow;
typedef struct CliOutlineCfg CliOutlineCfg;

// the outline read anew of the text under `key`, the file at `path`, in the language of the path, its rows in the order
// of their lines; none where `key` is NULL or no text read whole stands under it
void cli_outline_read(const CliOutlineCfg *args);
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
    void (*read)(const CliOutlineCfg *args);
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
