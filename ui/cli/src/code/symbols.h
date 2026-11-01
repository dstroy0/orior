// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols.h: what the files of a tree declare, for Go to Symbol in the tree and Search Everywhere: each file read off
// the leaves of the grammar of the plugin its extension opens it as (symbols_read.c). The tree stores each file's
// declarations with the time the file was last written and reads a file again only once that time moves. It is read
// on a thread of its own, and a search requested while it is read answers from what is read so far and reports so
// (symbols.c)
#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "../command_line/cli_internal.h"

// one declaration found: its name, `length` bytes, what it is, and its line counted from 0
typedef void (*CliDeclared)(void *context, const char *name, unsigned long long length, const char *kind,
                            unsigned int line);

// The languages texts are read in, the language each file opens as and each language's grammar, read once for the
// texts of a request (symbols_read.c)
typedef struct CliSymbolsLanguages CliSymbolsLanguages;
// the languages read in a cell taken from the cells, NULL where it is refused, and given back with the cell
CliSymbolsLanguages *cli_symbols_languages_read(void);
void cli_symbols_languages_given(CliSymbolsLanguages *read);

// whether the declarations of the file at `path` are read: whether its language is one read here
int cli_symbols_reads(const char *path);
// the declarations of `size` bytes of `text`, read in the language of the file at `path`, each handed to `each`
void cli_symbols_declared(const char *path, const char *text, unsigned long long size, CliDeclared each, void *context);
// How well `query` names `name`, higher the better, into `points`: the name itself first, then a name it begins, then
// one containing it, then one whose words it starts, as `gtf` starts `go_to_file` and `goToFile`, then one containing
// its letters in order. Case is set aside, and the shorter of two names that answer alike comes first. 1, or 0 where it
// does not name it
int cli_symbols_score(const char *query, const char *name, unsigned long long name_length, long long *points);
// every file forgotten, for a tree opened in place of the last
void cli_symbols_forget(void);
// the tree brought up to date on a thread of its own, unless it is being brought up to date already or was within
// four seconds; `paths`, `count` of them, the tree's files as the file listing gives them
void cli_symbols_refresh(const char *root, const char *const *paths, unsigned int count);
// how far the first reading of the tree's files has gone, `done` of `total`: 1 while it goes, else 0
int cli_symbols_progress(unsigned long long *done, unsigned long long *total);
// the reading going ended, what it has read retained
void cli_symbols_stop(void);
// whether the tree's files are being read for the first time
int cli_symbols_unread(void);
// the declarations whose names answer `query`, at most `most` of them, the best first, written as `{"symbols":
// [{"name", "kind", "path", "line", "text"}...], "reading"}`, `text` the declaration's own line trimmed, read from
// its file as the answer is written
void cli_symbols_find(const char *query, unsigned int most, VerbaLinea *out);
// the signature of the call the cursor stands in at `line`, `col` in UTF-16 units of `size` bytes of `text`, the lines
// the page hands from the file at `path`, the first starting in its grammar's `state`: written as `{"name", "active",
// "arguments": [{"text", "type"}], "declarations": [{"path", "line", "label", "params": [[from, to]], "types",
// "count", "variadic", "differs"}], "reading"}`, the declarations every one of the call's name in the tree in order of
// fit, or as `null` where the cursor stands in no call or the file's language is none read here: 1, or 0 with why
// given where the cells cannot meet it (symbols_signature.c)
int cli_symbols_signature(const char *path, const char *state, const char *text, unsigned long long size,
                          unsigned int line, unsigned int col, VerbaLinea *out, VerbaLinea *err_msg);
// the definition of `name` in effect at line `line` of the tree's file at `path`, from a walk of the file to that
// line: the defines the compilation database gives the file first, each include of a file of the tree walked where it
// stands, each branch once, each condition read off its leaves; the last define of the name met in an active region
// with no undefine after it, or for a name no define names, the last declaration of it the tree holds that the walk
// met in an active region. Written as `{"name", "path", "line", "text"}`, `line` null and `path` the node it is given
// to for a define the database gives, `text` a define's value or the declaration's line, or as `null` where none is in
// effect: 1, or 0 with why given where the cells cannot meet the walk (symbols_effect.c)
int cli_symbols_effect(const char *path, unsigned int line, const char *name, VerbaLinea *out, VerbaLinea *err_msg);
// the slice of the tree clangd is given for the lines `first` to `last` of the tree's file at `path`, its text the
// `size` bytes of `text` where they are given, the editor's, else the disk's, widened to the statements they cut: the
// lines `opener`, `openers` of them, opening each region the first line stands inside, outermost first, each to the
// bracket that opens its region; the locals declared above the first line in those regions that the lines name; the
// lines themselves; and before them the definitions in effect at the first line of the names they name, and of the
// names those name, with each include of no file of the tree, in the order a walk of the file meets them; the brackets
// the lines leave open closed after. Written as `{"lines": [{"path", "line", "text"}...]}`, each line's branch and
// line where it came from, both null for a bracket the slice closes: 1, or 0 with why given where the cells cannot
// meet it (symbols_slice.c)
int cli_symbols_slice(const char *path, const char *text, unsigned long long size, unsigned int first,
                      unsigned int last, const unsigned int *opener, unsigned int openers, VerbaLinea *out,
                      VerbaLinea *err_msg);
// the line of no branch, a bracket the slice closes; what each line of a slice is handed to: the path of the branch it
// came from, NULL for none, its line there, CLI_SLICE_NOWHERE for none, and its text, `length` bytes; and the same
// slice handed to `each` a line at a time in place of being written (symbols_slice.c)
#define CLI_SLICE_NOWHERE 0xffffffffu
typedef void (*CliSliceLine)(void *context, const char *path, unsigned int line, const char *text,
                             unsigned long long length);
int cli_symbols_slice_each(const char *path, const char *text, unsigned long long size, unsigned int first,
                           unsigned int last, const unsigned int *opener, unsigned int openers, CliSliceLine each,
                           void *context, VerbaLinea *err_msg);
// what each window of a branch's slices is handed to once its slice's lines are: its first and last lines and the lines
// opening the regions its first stands in, outermost first, and how many: 1 to cut the next, 0 to stop; and the branch
// at `path`, `size` bytes of `text`, cut in one walk of it into the slices of its windows, `height` lines each from its
// first line, each window that holds a leaf cut as the walk reaches it, its openers read off the brackets of the active
// lines above it, its slice handed to `each` a line at a time and then to `window`: 1, or 0 with why given where the
// cells cannot meet the walk (symbols_slice.c)
typedef int (*CliSliceWindow)(void *context, unsigned int first, unsigned int last, const unsigned int *opener,
                              unsigned int openers);
int cli_symbols_slices_each(const char *path, const char *text, unsigned long long size, unsigned int height,
                            CliSliceLine each, CliSliceWindow window, void *context, VerbaLinea *err_msg);

// one usage of a name: the tree's file it stands in, by its path in the tree, its line counted from 0, its span from
// `from` to `to` in UTF-16 units, and its line's text, `length` bytes; and the text the editor holds of the tree's file
// at `path`, in a cell the caller gives back with g_cli_cells->persistent_buf_release, its size into `size`, NULL where
// the editor holds none
typedef void (*CliUsage)(void *context, const char *path, unsigned int line, unsigned int from, unsigned int to,
                         const char *text, unsigned long long length);
typedef char *(*CliOpenText)(void *context, const char *path, unsigned long long *size);
// the usages of the name at `line`, `col` in UTF-16 units of the tree's file at `path`, each handed to `each`, the
// place's own among them, each file's text the one `open_text` gives, else the disk's: a local's or a parameter's, the
// leaves of its name in the region of the function that declares it; any other name's, the leaves of it whose
// definition in effect is the one at the place, in the file at `path` alone where `across` is 0, else in every file of
// the tree. 1, or 0 where the place names no name or the tree knows no definition of it, nothing handed on, or -1 with
// why given where the cells cannot meet it (symbols_usages.c)
int cli_symbols_usages(const char *path, unsigned int line, unsigned int col, int across, CliOpenText open_text,
                       void *open_text_context, CliUsage each, void *context, VerbaLinea *err_msg);
// the definition in effect of the name at `line`, `col` in UTF-16 units of the tree's file at `path`, handed to `each`
// as a usage: a local's or a parameter's first leaf in the region of the function that declares it, any other name's
// leaf on the line of the define in effect or of the first declaration the walk met, in that declaration's file, each
// file's text the one `open_text` gives. 1, or 0 where the place names no name or the tree knows no definition of it,
// as for a define the compile database gives, nothing handed on, or -1 with why given (symbols_usages.c)
int cli_symbols_definition(const char *path, unsigned int line, unsigned int col, CliOpenText open_text,
                           void *open_text_context, CliUsage each, void *context, VerbaLinea *err_msg);
// the names that complete the word that ends at `line`, `col` in UTF-16 units of `size` bytes of `text`, the editor's
// text of the tree's file at `path`, each handed to `each` once with what it is and the line it is declared on, in its
// own file: every define in effect at the line, then every declaration a walk of the file to the line met in an active
// region, then the parameters and the locals above the place of the function it stands in, each that begins with the
// word, the whole of each where no word ends there; a define the compilation database gives on line 0xffffffff. 1, or
// 0 with why given where the cells cannot meet the walk (symbols_complete.c)
int cli_symbols_complete(const char *path, const char *text, unsigned long long size, unsigned int line,
                         unsigned int col, CliDeclared each, void *context, VerbaLinea *err_msg);
// one hint: the line and the column in UTF-16 units it stands before, and the name of the parameter the argument there
// is given to, `length` bytes
typedef void (*CliHinted)(void *context, unsigned int line, unsigned int col, const char *name,
                          unsigned long long length);
// the hints on the lines `from` to `to` of `size` bytes of `text`, the editor's text of the tree's file at `path`: at
// each call there, before each argument, the name of the parameter of the call's definition in effect it is given to,
// where that parameter has a name and the argument is not that name alone, each handed to `each` in the order of the
// text. 1, or 0 with why given where the cells cannot meet it (symbols_hints.c)
int cli_symbols_hints(const char *path, const char *text, unsigned long long size, unsigned int from, unsigned int to,
                      CliHinted each, void *context, VerbaLinea *err_msg);
// a place in a file: its line counted from 0 and its column in UTF-16 units
typedef struct
{
    unsigned int line;
    unsigned int col;
} CliSymbolPlace;
// one function a call hierarchy reaches: the tree's file it is declared in, by its path in the tree, its name's line
// and its span there from `from` to `to` in UTF-16 units, its name, `length` bytes, and the places of the calls that
// lead there, `count` of them
typedef void (*CliCallReached)(void *context, const char *path, unsigned int line, unsigned int from, unsigned int to,
                               const char *name, unsigned long long length, const CliSymbolPlace *sites,
                               unsigned int count);
// the call hierarchy over the tree's files, each file's text the one `open_text` gives, else the disk's: the function
// of the tree's file at `path` whose name or body stands on line `line`, handed to `each` with no calls; the functions
// that call the function whose name stands on that line, each with the places of its calls of it in its file, a call a
// usage of the name a bracket `(` follows; and the functions it calls, each the definition in effect of a call in its
// body, with the places of those calls in it. A function is a region its bracket `{` opens at no depth whose head
// holds a bracket `(` after a leaf that declares its name. Each 1, 0 where no function stands there, or -1 with why
// given (symbols_calls.c)
int cli_symbols_function_at(const char *path, unsigned int line, CliOpenText open_text, void *open_text_context,
                            CliCallReached each, void *context, VerbaLinea *err_msg);
int cli_symbols_callers(const char *path, unsigned int line, CliOpenText open_text, void *open_text_context,
                        CliCallReached each, void *context, VerbaLinea *err_msg);
int cli_symbols_callees(const char *path, unsigned int line, CliOpenText open_text, void *open_text_context,
                        CliCallReached each, void *context, VerbaLinea *err_msg);
// one span Expand Selection steps through, from `from` to `to`
typedef void (*CliSpanned)(void *context, CliSymbolPlace from, CliSymbolPlace to);
// the spans of `size` bytes of `text`, the editor's text of the tree's file at `path`, that hold `from` to `to` and are
// more than it, each handed to `each` once, the least first: the leaf the place stands in, each bracket pair around
// it, inside its brackets and with them, the statement it stands in among a region's, each region with its head, and
// the whole text; each read by the roles its grammar names. 1, or 0 with why given where the cells cannot meet them
// (symbols_spans.c)
int cli_symbols_spans(const char *path, const char *text, unsigned long long size, CliSymbolPlace from,
                      CliSymbolPlace to, CliSpanned each, void *context, VerbaLinea *err_msg);
// the bracket pair at `at` in `size` bytes of `text`, the editor's text of the tree's file at `path`, the first line
// starting in its grammar's `state`: the bracket that ends at the place, else the one that starts at it, into `pair[0]`
// and the bracket it pairs with into `pair[1]`, each a leaf that opens or closes by the roles its grammar names and
// neither a string's nor a comment's, paired as the spans pair them: 1, or 0 where no bracket of a pair stands there,
// a pair a mark of no letters closes none, or the cells cannot meet the text's leaves (symbols_spans.c)
int cli_symbols_pair(const char *path, const char *state, const char *text, unsigned long long size, CliSymbolPlace at,
                     CliSymbolPlace *pair);
// the roles a token's name gives it as a text is walked, read off its segments as a leaf's are: it opens a region, it
// closes one, or it closes a bracket pair, ends a statement or parts one; it is a bare name, nothing more; it declares
// a local, or a parameter; it ends a statement or is a directive's; it is an operator on memory that binds no two
// values, the name after it one reached through it; and it opens a pair whose depth it counts, or closes one; none for
// a string's or a comment's (symbols_leaves.c)
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
unsigned int cli_symbols_name_roles(const char *name);

#endif
