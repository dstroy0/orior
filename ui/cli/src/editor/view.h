// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view.h: the editor's view of a text stored here, the window a viewport on it: the view holds where its top stands,
// its rows and columns, the cursor and the anchor the selection runs from, the folds it holds closed, and the steps
// that undo and redo the edits it made; the window sends it each key, text typed, press of the pointer, scroll and
// size, and draws the frame it answers, the rows in view with their runs, the folds among them, the cursor, the anchor
// and the bracket pair at the cursor. Keys are named as the page's editor names them, `Mod+Shift+Left` and the rest,
// and move, select and edit as it does, `Mod+Shift+[` and `Mod+Shift+]` closing and opening the fold at the cursor's
// line; the language's pairs and quotes close themselves as typed and a line breaks a step deeper after an opener, as
// its plugin names them. Find is the view's too: `Mod+F` opens its bar and takes the keys as its query, `Enter` and
// `Shift+Enter` select the next match and the one before, `F3` seeks the query again, `Alt+C`, `Alt+W` and `Alt+R`
// turn its case, whole words and pattern on and off, and `Escape` closes it. A view shows a text read whole or paged
// (view.c)
#ifndef VIEW_H
#define VIEW_H
typedef struct CliViewLineCfg CliViewLineCfg;
typedef struct CliViewRowCfg CliViewRowCfg;
typedef struct CliViewOutCfg CliViewOutCfg;
typedef struct CliViewFromCfg CliViewFromCfg;
typedef struct CliViewCursorCfg CliViewCursorCfg;
typedef struct CliViewInputCfg CliViewInputCfg;
typedef struct CliViewCfg CliViewCfg;
typedef struct CliViewPathKindCfg CliViewPathKindCfg;
typedef struct CliViewFullKindCfg CliViewFullKindCfg;
typedef struct CliViewFullCfg CliViewFullCfg;
#include "document.h"
#include "embed_dispatch_layout.h"

// what the window sends: a key, text typed, the pointer, a scroll, the view's size, and a completion accepted, whose
// `text` replaces the `column` units before the cursor, and a place, the cursor put at the line `row` and the unit
// `column`; the pointer's events; the tab's width in cells where nothing sets the indentation; and the milliseconds
// within which edits of one kind undo as one step
#define CLI_VIEW_KEY 0u
#define CLI_VIEW_TEXT 1u
#define CLI_VIEW_POINTER 2u
#define CLI_VIEW_SCROLL 3u
#define CLI_VIEW_SIZE 4u
#define CLI_VIEW_COMPLETE 5u
#define CLI_VIEW_PLACE 6u
#define CLI_VIEW_POINTER_DOWN 0u
#define CLI_VIEW_POINTER_MOVED 1u
#define CLI_VIEW_POINTER_UP 2u
#define CLI_VIEW_TAB 4u
#define CLI_VIEW_JOINED 1200ull
// the lines either side of the cursor the file's own words are taken from for a completion, and the most taken
#define CLI_VIEW_WORD_LINES 3000ull
#define CLI_VIEW_WORDS 4000u

// One input: its kind; a key's name; `length` bytes of text typed; the pointer's event, its row from the view's top and
// its column in cells, how many presses in a row it is and whether Shift is down; a scroll's top line; a size's rows
// and columns; and whether the pointer stands in the gutter, where a press toggles the fold at its row
typedef struct
{
    unsigned int kind;
    const char *name;
    const char *text;
    unsigned long long length;
    unsigned int pointer;
    unsigned long long row;
    unsigned long long column;
    unsigned int clicks;
    int shift;
    unsigned long long top;
    unsigned int rows;
    unsigned int columns;
    int gutter;
} CliViewInput;

// a view opened on the text stored under `key`, its indentation the `.editorconfig` files above the file at `path`
// give it, in place of a view open on the key: 1, or 0 where no text is stored under the key or the views table is
// full; and the view closed, its steps freed
int cli_view_open(const CliViewCfg *args);
void cli_view_close(const CliViewCfg *args);
// `input` taken by the view under `key`: 1, or 0 with why given where no view is open under the key or an edit could
// not be made
int cli_view_input(const CliViewInputCfg *args);
// the frame of the view under `key` written into `out` as `{"key", "seq", "top", "lines", "from", "rows", "numbers",
// "folds", "cursor", "anchor", "pair", "find", "matches", "colors"}`: the rows the lines shown from `from`, the lines
// a closed fold hides left out, each a string, with the line of each in `numbers`; the folds that open on the rows'
// lines, each `[first, last, shown, closed, closer]` as the text folds them, `closed` 1 where the view holds it closed
// and `closer` the text of the line it closes on where it shows that line; the cursor and the anchor each `{"line",
// "col"}` with the col in UTF-16 units; the pair the bracket pair at the cursor as two such places, the bracket that
// ends at the cursor, else the one that starts at it, first, or null where no bracket of a pair stands there; find
// where its bar is open, `{"open", "focus", "query", "at", "regex", "case", "word", "count", "current"}`, `focus` 1
// where the keys are its own, `at` its cursor in the query's units, `count` the matches in the whole text and
// `current` the one the last step selected, or null, else null; the matches among the rows, each `[line, from, to]`;
// and the colors the rows' runs as the walk saves them; before find, the indentation, `{"tabs", "size"}`, tabs 1, 0
// for spaces or null where nothing set it, and the step; every frame counted in `seq`: 1, or 0 where no view is open
// under the key
int cli_view_frame_write(const CliViewOutCfg *args);
// the cursor and the anchor of the view under `key`, each where it is given: 1, or 0 where no view is open under it
int cli_view_cursor(const CliViewCursorCfg *args);
// the file's path of the view under `key` into `path`, sized for CLI_PATH_MAX_LEN: 1, or 0 where no view is open
// under it
int cli_view_path(const CliViewPathKindCfg *args);
// the view under `key` marked as served by its language server at the whole path `full`, or no longer where `full` is
// empty: 1, or 0 where no view is open under it; the whole path into `full`, sized for CLI_PATH_MAX_LEN: 1 where the
// view is served, else 0; whether any open view is served at the whole path `full`; and whether the view's text changed
// since this was last asked, 1 once for each change
int cli_view_served_set(const CliViewFullCfg *args);
int cli_view_serve(const CliViewFullKindCfg *args);
int cli_view_serving(const CliViewFullCfg *args);
int cli_view_changed_take(const CliViewCfg *args);
// the place of the cell `column` of the row `row` from the top of the view under `key`: its line into `line`, the
// lines a closed fold hides stepped over, and its column in units into `col`, the line's end where the cell is past
// it: 1, or 0 where no view is open under the key
int cli_view_place_at(const CliViewRowCfg *args);
// the word at `line`, `col` of the view under `key`, in units, the word the place touches, inside it or at either end:
// its first unit into `from`, the unit after it into `to` and its UTF-8 into `word`, sized for `room`: 1, or 0 where
// no word stands there or no view is open under the key; and the word before the cursor, the prefix a completion
// answers, its first unit into `from`, its text into `prefix`: 1 with an empty prefix where no word stands before the
// cursor, or 0 where no view is open under the key
int cli_view_word_at(const CliViewLineCfg *args);
int cli_view_prefix(const CliViewFromCfg *args);
// the words of the view under `key`'s text within CLI_VIEW_WORD_LINES of the cursor, each of two letters or more once,
// the word the cursor stands in left out, at most CLI_VIEW_WORDS of them, written into `out` as a JSON array of
// completion items `{"label", "kind": "word", "insert"}`: 1, or 0 where no view is open under the key
int cli_view_words_write(const CliViewOutCfg *args);
// the minimap's lines from `first` to `last` of the view under `key` written into `out` as `{"from", "bars"}`: for
// each line the bars its words make, each `[start, width, name]` in cells, a tab as many cells as the indentation's
// step leaves to its stop, every run of letters that are not blanks one bar in the name of the run it stands in, as
// the text's runs give them; a line the text does not hold is empty: 1, or 0 where no view is open under the key
int cli_view_map_write(const CliViewOutCfg *args);
struct CliViewLineCfg
{
    const char *key;
    unsigned long long line_value;
    unsigned long long col_value;
    unsigned long long *from;
    unsigned long long *to;
    char *word;
    unsigned long long room;
};

struct CliViewRowCfg
{
    const char *key;
    unsigned long long row;
    unsigned long long column;
    unsigned long long *line;
    unsigned long long *col;
};

struct CliViewOutCfg
{
    const char *key;
    VerbaLinea *out;
    unsigned long long first;
    unsigned long long last;
};

struct CliViewFromCfg
{
    const char *key;
    unsigned long long *from;
    unsigned long long room;
    char *prefix;
};

struct CliViewCursorCfg
{
    const char *key;
    CliPlace *cursor;
    CliPlace *anchor;
};

struct CliViewInputCfg
{
    const char *key;
    const CliViewInput *input;
    VerbaLinea *err_msg;
};

struct CliViewCfg
{
    const char *key;
    const char *path;
};

struct CliViewPathKindCfg
{
    const char *key;
    char *path_out;
};

struct CliViewFullKindCfg
{
    const char *key;
    char *full_out;
};

struct CliViewFullCfg
{
    const char *key;
    const char *full;
};

typedef struct
{
    int (*open)(const CliViewCfg *args);
    void (*close)(const CliViewCfg *args);
    int (*changed_take)(const CliViewCfg *args);
} CliViewNs;
EMBED_TABLE_LAYOUT(CliViewNs, open, close, changed_take);
EMBED_TABLE_STORAGE CliViewNs cli_view_table EMBED_UNUSED = {
    .open = cli_view_open,
    .close = cli_view_close,
    .changed_take = cli_view_changed_take,
};

typedef struct
{
    int (*input)(const CliViewInputCfg *args);
} CliViewInputNs;
EMBED_TABLE_LAYOUT(CliViewInputNs, input);
EMBED_TABLE_STORAGE CliViewInputNs cli_view_input_table EMBED_UNUSED = {
    .input = cli_view_input,
};

typedef struct
{
    int (*frame_write)(const CliViewOutCfg *args);
    int (*words_write)(const CliViewOutCfg *args);
    int (*map_write)(const CliViewOutCfg *args);
} CliViewOutNs;
EMBED_TABLE_LAYOUT(CliViewOutNs, frame_write, words_write, map_write);
EMBED_TABLE_STORAGE CliViewOutNs cli_view_out EMBED_UNUSED = {
    .frame_write = cli_view_frame_write,
    .words_write = cli_view_words_write,
    .map_write = cli_view_map_write,
};

typedef struct
{
    int (*cursor)(const CliViewCursorCfg *args);
} CliViewCursorNs;
EMBED_TABLE_LAYOUT(CliViewCursorNs, cursor);
EMBED_TABLE_STORAGE CliViewCursorNs cli_view_cursor_table EMBED_UNUSED = {
    .cursor = cli_view_cursor,
};

typedef struct
{
    int (*path)(const CliViewPathKindCfg *args);
} CliViewPathKindNs;
EMBED_TABLE_LAYOUT(CliViewPathKindNs, path);
EMBED_TABLE_STORAGE CliViewPathKindNs cli_view_path_kind EMBED_UNUSED = {
    .path = cli_view_path,
};

typedef struct
{
    int (*served_set)(const CliViewFullCfg *args);
    int (*serving)(const CliViewFullCfg *args);
} CliViewFullNs;
EMBED_TABLE_LAYOUT(CliViewFullNs, served_set, serving);
EMBED_TABLE_STORAGE CliViewFullNs cli_view_full EMBED_UNUSED = {
    .served_set = cli_view_served_set,
    .serving = cli_view_serving,
};

typedef struct
{
    int (*serve)(const CliViewFullKindCfg *args);
} CliViewFullKindNs;
EMBED_TABLE_LAYOUT(CliViewFullKindNs, serve);
EMBED_TABLE_STORAGE CliViewFullKindNs cli_view_full_kind EMBED_UNUSED = {
    .serve = cli_view_serve,
};

typedef struct
{
    int (*place_at)(const CliViewRowCfg *args);
} CliViewRowNs;
EMBED_TABLE_LAYOUT(CliViewRowNs, place_at);
EMBED_TABLE_STORAGE CliViewRowNs cli_view_row EMBED_UNUSED = {
    .place_at = cli_view_place_at,
};

typedef struct
{
    int (*word_at)(const CliViewLineCfg *args);
} CliViewLineNs;
EMBED_TABLE_LAYOUT(CliViewLineNs, word_at);
EMBED_TABLE_STORAGE CliViewLineNs cli_view_line EMBED_UNUSED = {
    .word_at = cli_view_word_at,
};

typedef struct
{
    int (*prefix)(const CliViewFromCfg *args);
} CliViewFromNs;
EMBED_TABLE_LAYOUT(CliViewFromNs, prefix);
EMBED_TABLE_STORAGE CliViewFromNs cli_view_from EMBED_UNUSED = {
    .prefix = cli_view_prefix,
};

#endif
