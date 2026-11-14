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
#include "../command_line/rings.h"
#include "../machine/state.h"
#include "document.h"
#include "embed_dispatch_layout.h"
#include "embed_ticket_ring.h"

// the pair answers on their way from the workers to the view thread, a ticket ring's slots as a two's power, and the
// bytes they take, the pairs' part of the rings block
#define CLI_VIEW_PAIR_SHIFT 3u
#define CLI_VIEW_PAIR_SLOTS (1u << CLI_VIEW_PAIR_SHIFT)
#define CLI_VIEW_PAIR_RING_BYTES ((unsigned long long)CLI_VIEW_PAIR_SLOTS * sizeof(EmbedTicketRingSlot))

typedef struct CliViewLineCfg CliViewLineCfg;
typedef struct CliViewRowCfg CliViewRowCfg;
typedef struct CliViewOutCfg CliViewOutCfg;
typedef struct CliViewFromCfg CliViewFromCfg;
typedef struct CliViewCursorCfg CliViewCursorCfg;
typedef struct CliViewInputCfg CliViewInputCfg;
typedef struct CliViewCfg CliViewCfg;
typedef struct CliViewPathTypeCfg CliViewPathTypeCfg;
typedef struct CliViewFullTypeCfg CliViewFullTypeCfg;
typedef struct CliViewFullCfg CliViewFullCfg;
typedef struct CliViewSaveCfg CliViewSaveCfg;

// what the window sends: a key, text typed, the pointer, a scroll, the view's size, and a completion accepted, whose
// `text` replaces the `column` units before the cursor, a place, the cursor put at the line `row` and the unit
// `column`, and a wheel turned, the top moved by its rows from where it stands; the pointer's events; the tab's width
// in cells where nothing sets the indentation; and the milliseconds within which edits of one type undo as one step
#define CLI_VIEW_KEY 0u
#define CLI_VIEW_TEXT 1u
#define CLI_VIEW_POINTER 2u
#define CLI_VIEW_SCROLL 3u
#define CLI_VIEW_SIZE 4u
#define CLI_VIEW_COMPLETE 5u
#define CLI_VIEW_POSITION 6u
#define CLI_VIEW_WHEEL 7u
// the view slid across the file's grid, its first cell column `column`, no further than the widest line lets it
#define CLI_VIEW_SLIDE 8u
// the rows a frame holds past the rows the view shows whole: a window that scrolls by pixels shows a part of a row at
// its top and its foot, so the rows it draws run up to two past the whole ones
#define CLI_VIEW_ROWS_PAST 2u
// the sticky rows a frame carries at the most
#define CLI_VIEW_STICKY_MOST 5u
#define CLI_VIEW_POINTER_DOWN 0u
#define CLI_VIEW_POINTER_MOVED 1u
#define CLI_VIEW_POINTER_UP 2u
#define CLI_VIEW_TAB 4u
#define CLI_VIEW_JOINED 1200ull
// the lines either side of the cursor the file's own words are taken from for a completion
#define CLI_VIEW_WORD_LINES 3000ull

// One input: its type; a key's name; `length` bytes of text typed; the pointer's event, its row from the view's top and
// its column in cells, how many presses in a row it is and whether Shift is down; a scroll's top line, and the furthest
// line down a wheel takes the top to, CLI_VIEW_WHEEL_UNBOUND where the text's last line alone bounds it; a size's rows
// and columns; whether the pointer stands in the gutter, where a press toggles the fold at its row; and a wheel's rows,
// down the text from the top positive
#define CLI_VIEW_WHEEL_UNBOUND (~0ULL)
typedef struct
{
    unsigned int type;
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
    long long wheel_rows;
} CliViewInput;

// A row of a frame: its line's number in the file, and where its UTF-8 starts among the frame's bytes and its length,
// cut at the view's columns
typedef struct
{
    unsigned long long number;
    unsigned long long text;
    unsigned long long length;
} CliViewFrameRow;

// A fold that opens on a row: that line, the last it hides, whether it shows the line it closes on and whether the view
// holds it closed, and that line's text, its blanks left off, where it starts among the frame's closers and its length
typedef struct
{
    unsigned long long first;
    unsigned long long last;
    int displayed;
    int closed;
    unsigned long long closer;
    unsigned long long closer_length;
} CliViewFrameFold;

// Find as a frame shows it: whether its bar is open and the keys are its own; its query, `query_length` bytes, and
// where its cursor stands in it, in units; whether it seeks a pattern, with case and whole words; how many matches
// there are and the current one, -1 for none
typedef struct
{
    int open;
    int focus;
    unsigned long long query_length;
    unsigned long long at;
    int regex;
    int match_case;
    int word_text;
    unsigned long long count;
    long long current;
    char *query;
} CliViewFind;

// A view's frame: every count and pointer the window draws from, in cells of the views' owner, immutable once read. The
// rows are the lines shown from `from`, the lines a closed fold hides left out, each row's runs at `runs->line[row]`;
// the closers stand among `bytes` from `closers`; each match is a line, a from and a to column; `indent_tabs` is 1 for
// tabs, 0 for spaces and -1 where nothing set it. The file is a grid of lines by cells and the view a window on it:
// `top` is its first line and `left` its first cell column. Each row holds its line from its start through the view's
// last column, so every column stays the file's own and the window draws the row moved left by `left` cells. `widest`
// is the most cells of any line the view has shown, the grid's width as the view knows it. After the `count` rows
// shown stand `stickies` sticky rows, the head lines of the regions the first line under them stands inside,
// outermost first, each with its text and runs as the rows shown have theirs, for the window to pin over the rows
// at the view's top; a region opened on a line of its brace alone has the line before as its head
typedef struct
{
    unsigned long long seq;
    unsigned long long top;
    unsigned long long left;
    unsigned long long widest;
    unsigned long long lines;
    unsigned long long from;
    unsigned long long closers;
    unsigned long long matches;
    unsigned int count;
    unsigned int stickies;
    unsigned int folds;
    CliPosition cursor;
    CliPosition anchor;
    int has_pair;
    CliPosition pair[2];
    CliViewFind find;
    int indent_tabs;
    unsigned int indent_size;
    CliViewFrameRow *row;
    CliViewFrameFold *fold;
    unsigned long long (*match)[3];
    char *bytes;
    CliDocumentRuns *runs;
} CliViewFrame;

// the frame of the view under `key` in cells taken for it, sized to its rows and columns: the frame, or NULL where no
// view is open under the key or the cells cannot meet it; and a frame's cells released
CliViewFrame *cli_view_frame_alloc(const char *key);
void cli_view_frame_release(CliViewFrame *frame);

// the lines from `first` to `last` of the view under `key` laid as bars in a cell of the views' owner, as
// cli_document_map_alloc lays them, a text that indents by tabs a tab's step of cells a unit of the gap that opens a
// line: the cell, or NULL where no view is open under the key or the cells cannot meet it; and that cell released
CliDocumentMap *cli_view_map_alloc(const char *key, unsigned long long first, unsigned long long last);
void cli_view_map_release(CliDocumentMap *map);

// the names the walk has found within CLI_VIEW_WORD_LINES of the cursor of the view under `key` in a cell of the views'
// owner, as cli_document_words_alloc reads them: the cell, or NULL where no view is open under the key or the cells
// cannot meet it; and that cell released
CliDocumentWords *cli_view_words_alloc(const char *key);
void cli_view_words_release(CliDocumentWords *words);

// a view opened on the text stored under `key`, its indentation the `.editorconfig` files above the file at `path`
// give it, in place of a view open on the key: 1, or 0 where no text is stored under the key or the views table is
// full; and the view closed, its steps freed
int cli_view_open(const CliViewCfg *args);
void cli_view_close(const CliViewCfg *args);

// A frame laid of the view under `key`: its cursor, its top line, its lines and its rows
typedef struct
{
    const char *key;
    CliPosition cursor;
    unsigned long long top;
    unsigned long long lines;
    unsigned int rows;
} CliViewDrawn;

// What watches the views: handed each frame laid of a view, each view opened, under `key` on the file at `path` from
// the tree's top, empty for none, and each view about to close, while it still stands open
typedef struct
{
    void (*drawn)(const CliViewDrawn *drawn);
    void (*opened)(const char *key, const char *path);
    void (*closing)(const char *key);
} CliViewWatcher;

// the views watched by `watcher` from here on, NULL for none
void cli_view_watch(const CliViewWatcher *watcher);
// the views open in the order they were opened, which are the files open as the window's tabs, a view opened with no
// path a pane's own and among no tab: how many; the key of the
// one at `at` copied into `key_out`, sized for CLI_NAME_MAX_LEN, and its file's path from the tree's top into
// `path_out`, sized for CLI_PATH_MAX_LEN, each 1, or 0 past the views; its file's name, the last part of that path,
// inside `path_out` once the path is copied there, NULL past the views; the place of the one shown and of the one
// shown before it, each -1 for none; the one at `at` shown, the one shown before it saved; and whether the close of the
// one at `at` waits on a second press, set as `closing` asks
unsigned int cli_view_count(void);
int cli_view_key(unsigned int at, char *key_out);
int cli_view_path_at(unsigned int at, char *path_out);
const char *cli_view_name_at(unsigned int at, char *path_out);
int cli_view_shown(void);
int cli_view_last(void);
void cli_view_show(unsigned int at);
int cli_view_closing(unsigned int at);
void cli_view_closing_set(unsigned int at, int closing);
// whether the frames carry sticky rows, 1 as the views start; and that switched `on` or off for every view
int cli_view_sticky(void);
void cli_view_sticky_set(int on);
// the width Fill Paragraph lays words within, in cells, 0 for the view's own columns; and that width set
unsigned long long cli_view_fill(void);
void cli_view_fill_set(unsigned long long columns);
// the files open as views (view_files.c): the file at `path`, a whole path inside the tree `root` or one from its top,
// opened as a view where none is open on it: its text read whole or paged under the key `window:` and its path from the
// tree's top, its grammar applied, the view opened at `rows` and `columns`; the view shown, the one shown before it
// saved, its cursor put at `place` where it is not NULL, and else at the line and the column the path ends in, each a
// colon and digits counted from 1, a line alone at its start: the view's place, or -1 where the file was refused, why
// written into `err_msg`. The view at `at` closed and its text let go. And the session, the views' paths in order, a
// line each, the shown one's first, saved in orior's folder under a key of the tree `root`'s own, and the views it
// names opened again from `entries`
int cli_view_file_open(const char *root, const char *path, unsigned int rows, unsigned int columns,
                       const CliPosition *place, VerbaLinea *err_msg);
void cli_view_file_close(unsigned int at);
void cli_view_session_save(const char *root);
void cli_view_session_restore(const char *root, const CliStateEntries *entries, unsigned int rows,
                              unsigned int columns);
// `input` taken by the view under `key`: 1, or 0 with why given where no view is open under the key or an edit could
// not be made
int cli_view_input(const CliViewInputCfg *args);
// the line at the top of the view under `key`, 0 where no view is open under it
unsigned long long cli_view_top(const char *key);
// the cursor and the anchor of the view under `key`, each where it is given: 1, or 0 where no view is open under it
int cli_view_cursor(const CliViewCursorCfg *args);
// the file's path of the view under `key` into `path`, sized for CLI_PATH_MAX_LEN: 1, or 0 where no view is open
// under it
int cli_view_path(const CliViewPathTypeCfg *args);
// the view under `key` marked as served by its language server at the whole path `full`, or no longer where `full` is
// empty: 1, or 0 where no view is open under it; the whole path into `full`, sized for CLI_PATH_MAX_LEN: 1 where the
// view is served, else 0; whether any open view is served at the whole path `full`; and whether the view's text changed
// since this was last asked, 1 once for each change
int cli_view_served_set(const CliViewFullCfg *args);
int cli_view_serve(const CliViewFullTypeCfg *args);
int cli_view_serving(const CliViewFullCfg *args);
int cli_view_changed_alloc(const CliViewCfg *args);
// the save point of the view under `key`, the place in its steps its file was last written at, as Scintilla keeps
// one: put where the view stands now; lost, so the text reads modified until it is put again; and whether the text is
// modified, the view standing anywhere but its save point or the steps it marked let go: 1, else 0, and 0 where no
// view is open under the key. A view opens at its save point
void cli_view_saved(const CliViewCfg *args);
void cli_view_unsaved(const CliViewCfg *args);
int cli_view_modified(const CliViewCfg *args);
// the text of the view under `key` written to its file, the whole path `full`, as the tree writes every file, a paged
// text through its pages: 1 with the view's save point put where it stands, or 0 with the save point lost and why
// written into `err_msg`
int cli_view_save(const CliViewSaveCfg *args);
// the place of the cell `column` of the row `row` from the top of the view under `key`: its line into `line`, the
// lines a closed fold hides stepped over, and its column in units into `col`, the line's end where the cell is past
// it: 1, or 0 where no view is open under the key
int cli_view_position_at(const CliViewRowCfg *args);
// the word at `line`, `col` of the view under `key`, in units, the word the place touches, inside it or at either end:
// its first unit into `from`, the unit after it into `to` and its UTF-8 into `word`, sized for `capacity`: 1, or 0
// where no word stands there or no view is open under the key; and the word before the cursor, the prefix a completion
// answers, its first unit into `from`, its text into `prefix`: 1 with an empty prefix where no word stands before the
// cursor, or 0 where no view is open under the key
int cli_view_word_at(const CliViewLineCfg *args);
int cli_view_prefix(const CliViewFromCfg *args);
// the minimap's lines from `first` to `last` of the view under `key` written into `out` as `{"from", "bars"}`: for
// each line the bars its words make, each `[start, width, name]` in cells, a tab as many cells as the indentation's
// step leaves to its stop, every run of letters that are not blanks one bar in the name of the run it stands in, as
// the text's runs give them; a line the text does not hold is empty: 1, or 0 where no view is open under the key
struct CliViewLineCfg
{
    const char *key;
    unsigned long long line_value;
    unsigned long long col_value;
    unsigned long long *from;
    unsigned long long *to;
    char *word_text;
    unsigned long long capacity;
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
    unsigned long long capacity;
    char *prefix;
};

struct CliViewCursorCfg
{
    const char *key;
    CliPosition *cursor;
    CliPosition *anchor;
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

struct CliViewPathTypeCfg
{
    const char *key;
    char *path_out;
};

struct CliViewFullTypeCfg
{
    const char *key;
    char *full_out;
};

struct CliViewFullCfg
{
    const char *key;
    const char *full;
};

struct CliViewSaveCfg
{
    const char *key;
    const char *full;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*save)(const CliViewSaveCfg *args);
} CliViewSaveNs;
EMBED_TABLE_LAYOUT(CliViewSaveNs, save);
EMBED_TABLE_STORAGE CliViewSaveNs cli_view_save_table EMBED_UNUSED = {
    .save = cli_view_save,
};

typedef struct
{
    int (*open)(const CliViewCfg *args);
    void (*close)(const CliViewCfg *args);
    int (*changed_alloc)(const CliViewCfg *args);
    void (*saved)(const CliViewCfg *args);
    void (*unsaved)(const CliViewCfg *args);
    int (*modified)(const CliViewCfg *args);
} CliViewNs;
EMBED_TABLE_LAYOUT(CliViewNs, open, close, changed_alloc, saved, unsaved, modified);
EMBED_TABLE_STORAGE CliViewNs cli_view_table EMBED_UNUSED = {
    .open = cli_view_open,
    .close = cli_view_close,
    .changed_alloc = cli_view_changed_alloc,
    .saved = cli_view_saved,
    .unsaved = cli_view_unsaved,
    .modified = cli_view_modified,
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
    int (*cursor)(const CliViewCursorCfg *args);
} CliViewCursorNs;
EMBED_TABLE_LAYOUT(CliViewCursorNs, cursor);
EMBED_TABLE_STORAGE CliViewCursorNs cli_view_cursor_table EMBED_UNUSED = {
    .cursor = cli_view_cursor,
};

typedef struct
{
    int (*path)(const CliViewPathTypeCfg *args);
} CliViewPathTypeNs;
EMBED_TABLE_LAYOUT(CliViewPathTypeNs, path);
EMBED_TABLE_STORAGE CliViewPathTypeNs cli_view_path_type EMBED_UNUSED = {
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
    int (*serve)(const CliViewFullTypeCfg *args);
} CliViewFullTypeNs;
EMBED_TABLE_LAYOUT(CliViewFullTypeNs, serve);
EMBED_TABLE_STORAGE CliViewFullTypeNs cli_view_full_type EMBED_UNUSED = {
    .serve = cli_view_serve,
};

typedef struct
{
    int (*position_at)(const CliViewRowCfg *args);
} CliViewRowNs;
EMBED_TABLE_LAYOUT(CliViewRowNs, position_at);
EMBED_TABLE_STORAGE CliViewRowNs cli_view_row EMBED_UNUSED = {
    .position_at = cli_view_position_at,
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
