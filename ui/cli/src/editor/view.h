// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view.h: the editor's view of a text stored here, the window a viewport on it: the view holds where its top is,
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

// The pair answers on their way from the workers to the view thread, a ticket ring's entries as a power of two. Then
// the bytes they take, the pairs' part of the rings block
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
typedef struct CliViewWatcher CliViewWatcher;
typedef struct CliViewPathTypeCfg CliViewPathTypeCfg;
typedef struct CliViewFullTypeCfg CliViewFullTypeCfg;
typedef struct CliViewFullCfg CliViewFullCfg;
typedef struct CliViewSaveCfg CliViewSaveCfg;

// What the window sends: a key, text typed, the pointer, a scroll, the view's size, and a completion accepted. Its
// `text` replaces the `column` units before the cursor. A place puts the cursor at the line `row` and the unit
// `column`. A wheel turned moves the top by its rows from where it is. Then the pointer's events, and the tab's width
// in cells where nothing sets the indentation. Last, the milliseconds within which edits of one type undo as one step
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
// a table column's width set: the column at index `column` is `columns` cells wide, whatever its fields measure, and
// 0 cells gives the width back to its fields
#define CLI_VIEW_COLUMN 9u
// `length` bytes of `text` written over the span from `from` to `to`, in lines and UTF-16 units, as one step
#define CLI_VIEW_REPLACE 10u
// The rows a frame holds past the rows the view shows whole. A window that scrolls by pixels shows a part of a row at
// its top and its foot. Its rows thus run up to two past the whole ones
#define CLI_VIEW_ROWS_PAST 2u
// the sticky rows a frame carries at max
#define CLI_VIEW_STICKY_MAX 5u
#define CLI_VIEW_POINTER_DOWN 0u
#define CLI_VIEW_POINTER_MOVED 1u
#define CLI_VIEW_POINTER_UP 2u
#define CLI_VIEW_TAB 4u
#define CLI_VIEW_JOINED 1200ULL
// the lines either side of the cursor that give the file's own words for a completion: the edited line alone. The
// rest of the file's names come off its branch in the tree
#define CLI_VIEW_WORD_LINES 0ULL

// One input. It holds its type, a key's name and `length` bytes of text typed. Then the pointer's event, its row from
// the view's top, its column in cells, its count of presses in a row, and Shift. Then a
// scroll's top line, and the furthest line down a wheel takes the top to, CLI_VIEW_WHEEL_UNBOUND where the text's last
// line alone bounds it. Then a size's rows and columns, and whether the pointer is in the gutter, where a press toggles
// the fold at its row. Last, a wheel's rows, positive down the text from the top
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
    CliPosition from;
    CliPosition to;
} CliViewInput;

// A row of a frame: its line's number in the file, and where its UTF-8 starts among the frame's bytes. Last, its
// length, cut at the view's columns
typedef struct
{
    unsigned long long number;
    unsigned long long text;
    unsigned long long length;
} CliViewFrameRow;

// A fold that opens on a row: that line, the last it hides, and whether it shows the line it closes on. Then whether
// the view keeps it closed. Last, the text of the line it closes on, without its blanks, with where it starts among
// the frame's closers and its length
typedef struct
{
    unsigned long long first;
    unsigned long long last;
    int displayed;
    int closed;
    unsigned long long closer;
    unsigned long long closer_length;
} CliViewFrameFold;

// Find as a frame shows it: whether its bar is open and the keys are its own. Then its query, `query_length` bytes,
// and where its cursor is in it, in units. Then whether it seeks a pattern, with case and whole words. Last, how many
// matches there are and the current one, -1 for none
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

// A view's frame: every count and pointer the window draws from, which stays as it is once read. The rows are the
// lines on display from `from`, without the lines a closed fold hides. Each row's runs come from the document under
// the text's `key` by the row's line. The closers are among `bytes` from `closers`. Each match is a line, a from and a
// to column. `indent_tabs` is 1 for tabs, 0 for spaces and -1 where nothing set it.
//
// The file is a grid of lines by cells and the view a window on it: `top` is its first line and `left` its first cell
// column. Each row holds its line from its start through the view's last column. Thus every column stays the file's
// own, and the window draws the row moved left by `left` cells. `widest` is the max cells of any line the view showed,
// the grid's width as the view knows it.
//
// After the `count` rows on display come `stickies` sticky rows, the head lines of the regions the first line under
// them is inside, outermost first. Each has its text and runs as the rows on display have theirs. The window pins them
// over the rows at the view's top. A region that opens on a line of its brace alone has the line before as its head.
//
// A table file has the characters its plugin defines for the file extension. `table` is the delimiter character that
// separates a line into fields, and `table_quote` the quote character that encloses a field, 0 for none. There are
// `column_count` column offsets and one past them. The field at column index `k` begins at the cell offset
// `column_offset[k]`. That is the sum of the previous offset, the previous column's width and one cell for the
// delimiter. `column_offset[column_count]` is the cell after the last column.
//
// A column's width is the max width of its fields, or the width a CLI_VIEW_COLUMN input set. A file that is not a
// table has `table` 0 and a column count of 0. `colors_serial` is the text's colors serial, read before the rows'
// runs. A frame with the same serial as an earlier one has the same colors
typedef struct
{
    unsigned int colors_serial;
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
    unsigned int table;
    unsigned int table_quote;
    unsigned int column_count;
    CliViewFrameRow *row;
    CliViewFrameFold *fold;
    unsigned long long (*match)[3];
    unsigned long long *column_offset;
    char *bytes;
    char key[CLI_NAME_MAX_LEN];
} CliViewFrame;

// The view under `key` builds its frame, sized to its rows and columns, and keeps it in a movable cell of its own. The
// frame stays until the view builds its next frame or closes. Returns the frame's bytes, or 0 where no view is open
// under the key or the cells cannot meet it. The bytes of the frame the view keeps come next, 0 for none.
//
// The read copies the kept frame into `frame_out`, `capacity` bytes, a buffer of the caller's own call. The frame's
// lists point into that buffer. Returns the frame, or NULL where the view keeps none or the capacity is short.
//
// The views' table: the view `key`'s frame and its lines `first` to `last`, and the watcher. Then the view `at`, its
// key into `key_out` and its path into `path_out`, shown, its close `closing`, and its file closed. Last, the sticky
// lines `on`, the fill `columns`, and the session saved for the tree `root`
typedef struct
{
    const char *key;
    unsigned long long first;
    unsigned long long last;
    const CliViewWatcher *watcher;
    unsigned int at;
    char *key_out;
    char *path_out;
    int closing;
    int on;
    unsigned long long columns;
    const char *root;
    void *frame_out;
    unsigned long long capacity;
} CliViewAtCfg;
unsigned long long cli_view_frame_build(const CliViewAtCfg *args);
unsigned long long cli_view_frame_bytes(const CliViewAtCfg *args);
const CliViewFrame *cli_view_frame_read(const CliViewAtCfg *args);

// The minimap's map the views keep. The place puts the lines from `first` to `last` of the view under `key` as bars,
// as cli_document_map_alloc puts them, in place of the map before. A text that indents by tabs takes a tab's step of
// cells a unit of the gap that opens a line. The place returns 1, or 0 where no view is open under the key or the
// cells cannot meet the map.
//
// The bars count of `line`, 0 where the map holds no line `line`. The bar `at` of `line` goes into `bar_out`: 1, or 0
// where the map holds no such bar. The lines writes the map's first line into `from_out` and its count into
// `count_out`: 1, or 0 where the views hold no map. The release gives the map back
typedef struct
{
    const char *key;
    unsigned long long first;
    unsigned long long last;
    unsigned long long line;
    unsigned int at;
    CliDocumentMapBar *bar_out;
    unsigned long long *from_out;
    unsigned int *count_out;
} CliViewMapCfg;
int cli_view_map_place(const CliViewMapCfg *args);
unsigned int cli_view_map_bars(const CliViewMapCfg *args);
int cli_view_map_bar(const CliViewMapCfg *args);
int cli_view_map_lines(const CliViewMapCfg *args);
void cli_view_map_release(const CliViewMapCfg *args);

// The names the walk found within CLI_VIEW_WORD_LINES of the cursor of the view under `key`. Each goes to `each` with
// `context` as cli_document_words_each passes them, before the call returns. No view open under the key passes none
typedef struct
{
    const char *key;
    void (*each)(const CliEachTextLengthCfg *args);
    void *context;
} CliViewWordsCfg;
void cli_view_words_each(const CliViewWordsCfg *args);

// A view opened on the text in the store under `key`, in place of a view open on the key. Its indentation is what the
// `.editorconfig` files above the file at `path` give it. Returns 1, or 0 where the store has no text under the key or
// the views table is full. Then the view closed, its steps freed
int cli_view_open(const CliViewCfg *args);
void cli_view_close(const CliViewCfg *args);

// A frame positioned of the view under `key`: its cursor, its top line, its lines and its rows
typedef struct
{
    const char *key;
    CliPosition cursor;
    unsigned long long top;
    unsigned long long lines;
    unsigned int rows;
} CliViewDrawn;

// What watches the views. It gets each frame a view lays out, and each view opened under `key` on the file at `path`
// from the tree's top, empty for none. It also gets each view about to close, while it still is open
typedef struct CliViewWatcher
{
    void (*drawn)(const CliViewDrawn *drawn);
    void (*opened)(const char *key, const char *path);
    void (*closing)(const char *key);
} CliViewWatcher;

// the views watched by `watcher` from here on, NULL for none
void cli_view_watch(const CliViewAtCfg *args);
// The views open in the order they opened, which are the files open as the window's tabs. A view opened with no path
// is a pane's own and in no tab. First, how many there are. Then the key of the one at `at` into `key_out`, sized for
// CLI_NAME_MAX_LEN. Its file's path from the tree's top goes into `path_out`, sized for CLI_PATH_MAX_LEN. Each returns
// 1, or 0 past the views.
//
// Then its file's name, the last part of that path, inside `path_out` once the path is there, NULL past the views.
// Then the place of the one on display and of the one on display before it, each -1 for none. Then the one at `at` on
// display, with the one on display before it kept. Last, whether the close of the one at `at` waits on a second
// press, set as `closing` asks
unsigned int cli_view_count(void);
int cli_view_key(const CliViewAtCfg *args);
int cli_view_path_at(const CliViewAtCfg *args);
const char *cli_view_name_at(const CliViewAtCfg *args);
int cli_view_displayed(void);
int cli_view_last(void);
void cli_view_show(const CliViewAtCfg *args);
int cli_view_closing(const CliViewAtCfg *args);
void cli_view_closing_set(const CliViewAtCfg *args);
// whether the frames carry sticky rows, 1 as the views start; and that switched `on` or off for every view
int cli_view_sticky(void);
void cli_view_sticky_set(const CliViewAtCfg *args);
// the width Fill Paragraph positions words within, in cells, 0 for the view's own columns; and that width set
unsigned long long cli_view_fill(void);
void cli_view_fill_set(const CliViewAtCfg *args);
// The files open as views (view_files.c). The file at `path`, a whole path inside the tree `root` or one from its top,
// opens as a view where none is open on it. Its text reads whole or paged under the key `window:` and its path from
// the tree's top. Its grammar applies, and the view opens at `rows` and `columns`.
//
// The view goes on display, and the one on display before it stays kept. Its cursor goes to `place` where it is not
// NULL. Else it goes to the line and the column the path ends in, each a colon and digits counted from 1. A line alone
// goes to its start. Returns the view's place, or -1 where the window refused the file, why written into `err_msg`.
//
// Then the view at `at` closed and its text let go. Last, the session: the views' paths in order, a line each, the
// one on display first. It goes into orior's folder under a key of the tree `root`'s own. The views it names open
// again from `entries`
int cli_view_file_open(const char *root, const char *path, unsigned int rows, unsigned int columns,
                       const CliPosition *place, VerbaLinea *err_msg);
void cli_view_file_close(const CliViewAtCfg *args);
void cli_view_session_save(const CliViewAtCfg *args);
void cli_view_session_restore(const char *root, const CliStateEntries *entries, unsigned int rows,
                              unsigned int columns);
// `input` taken by the view under `key`: 1, or 0 with why given where no view is open under the key or an edit fails
int cli_view_input(const CliViewInputCfg *args);
// the line at the top of the view under `key`, 0 where no view is open under it
unsigned long long cli_view_top(const CliViewAtCfg *args);
// the cursor and the anchor of the view under `key`, each where the call gives a place: 1, or 0 where no view is open
// under it
int cli_view_cursor(const CliViewCursorCfg *args);
// the file's path of the view under `key` into `path`, sized for CLI_PATH_MAX_LEN: 1, or 0 where no view is open
// under it
int cli_view_path(const CliViewPathTypeCfg *args);
// The view under `key` marked as one its language server serves at the whole path `full`, or no more where `full` is
// empty. Returns 1, or 0 where no view is open under it. Then the whole path into `full`, sized for
// CLI_PATH_MAX_LEN: 1 where a server serves the view, else 0. Then whether a server serves any open view at the whole
// path `full`. Last, whether the view's text changed since the last ask, 1 once for each change
int cli_view_served_set(const CliViewFullCfg *args);
int cli_view_serve(const CliViewFullTypeCfg *args);
int cli_view_serving(const CliViewFullCfg *args);
int cli_view_changed_clear(const CliViewCfg *args);
// The save point of the view under `key`, the place in its steps of the last write of its file, as Scintilla keeps
// one. It goes where the view is now. Or it goes away, and the text reads as changed until it comes back. Then
// whether the text changed: the view is anywhere but its save point, or the steps it marked went. Returns 1, else 0,
// and 0 where no view is open under the key. A view opens at its save point
void cli_view_saved(const CliViewCfg *args);
void cli_view_unsaved(const CliViewCfg *args);
int cli_view_modified(const CliViewCfg *args);
// The text of the view under `key` written to its file, the whole path `full`, as the tree writes every file. A paged
// text goes through its pages. Returns 1 with the view's save point where it is, or 0 with no save point and why
// written into `err_msg`
int cli_view_save(const CliViewSaveCfg *args);
// The place of the cell `column` of the row `row` from the top of the view under `key`. Its line goes into `line`,
// past the lines a closed fold hides. Its column in units goes into `col`, the line's end where the cell is past it.
// Returns 1, or 0 where no view is open under the key
int cli_view_position_at(const CliViewRowCfg *args);
// The word at `line`, `col` of the view under `key`, in units: the word the place touches, inside it or at either
// end. Its first unit goes into `from`, the unit after it into `to`, and its UTF-8 into `word`, sized for `capacity`.
// Returns 1, or 0 where no word is there or no view is open under the key.
//
// Then the word before the cursor, the prefix a completion answers. Its first unit goes into `from`, and its text
// into `prefix`. Returns 1 with an empty prefix where no word is before the cursor, or 0 where no view is open under
// the key
int cli_view_word_at(const CliViewLineCfg *args);
int cli_view_prefix(const CliViewFromCfg *args);
// The minimap's lines from `first` to `last` of the view under `key` written into `out` as `{"from", "bars"}`. Each
// line gets the bars its words make, each `[start, width, name]` in cells. A tab is as many cells as the indentation's
// step leaves to its stop. Every run of characters that are not blanks is one bar, named for the run it is in. A line
// the text does not hold is empty. Returns 1, or 0 where no view is open under
// the key
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
    int (*changed_clear)(const CliViewCfg *args);
    void (*saved)(const CliViewCfg *args);
    void (*unsaved)(const CliViewCfg *args);
    int (*modified)(const CliViewCfg *args);
} CliViewNs;
EMBED_TABLE_LAYOUT(CliViewNs, open, close, changed_clear, saved, unsaved, modified);
EMBED_TABLE_STORAGE CliViewNs cli_view_table EMBED_UNUSED = {
    .open = cli_view_open,
    .close = cli_view_close,
    .changed_clear = cli_view_changed_clear,
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
