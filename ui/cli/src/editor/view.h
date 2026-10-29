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
#include "document.h"

// what the window sends: a key, text typed, the pointer, a scroll, and the view's size; the pointer's events; the
// tab's width in cells where nothing sets the indentation; and the milliseconds within which edits of one kind undo as
// one step
#define CLI_VIEW_KEY 0u
#define CLI_VIEW_TEXT 1u
#define CLI_VIEW_POINTER 2u
#define CLI_VIEW_SCROLL 3u
#define CLI_VIEW_SIZE 4u
#define CLI_VIEW_POINTER_DOWN 0u
#define CLI_VIEW_POINTER_MOVED 1u
#define CLI_VIEW_POINTER_UP 2u
#define CLI_VIEW_TAB 4u
#define CLI_VIEW_JOINED 1200ull

// One input: its kind; a key's name; `length` bytes of text typed; the pointer's event, its row from the view's top and
// its column in cells, how many presses in a row it is and whether Shift is held; a scroll's top line; a size's rows
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
// give it, in place of a view open on the key: 1, or 0 where no text is stored under the key or no more views can be
// held; and the view closed, its steps given back
int cli_view_opened(const char *key, const char *path);
void cli_view_closed(const char *key);
// `input` taken by the view under `key`: 1, or 0 with why given where no view is open under the key or an edit could
// not be made
int cli_view_input(const char *key, const CliViewInput *input, VerbaLinea *err_msg);
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
// and the colors the rows' runs as the walk keeps them; before find, the indentation, `{"tabs", "size"}`, tabs 1, 0
// for spaces or null where nothing set it, and the step; every frame counted in `seq`: 1, or 0 where no view is open
// under the key
int cli_view_frame_written(const char *key, VerbaLinea *out);
// the cursor and the anchor of the view under `key`, each where it is given: 1, or 0 where no view is open under it
int cli_view_cursor(const char *key, CliPlace *cursor, CliPlace *anchor);
#endif
