// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// screen.h: a terminal's screen, the grid of cells a shell's output draws, read as an xterm reads it. Printed letters
// land at the cursor in the colors and weight of the moment; the control letters and the escape sequences move the
// cursor, erase, insert and delete, scroll a region, set the colors from the 16, the 256 and any red, green and blue,
// switch to the second screen full-screen programs draw on, and ask where the cursor is and what the terminal is. What
// the screen does not read it reads past. The lines that scroll off the top of the main screen go to its scrollback,
// the last CLI_TERMINAL_SCROLLBACK_LINES of them; the second screen has none (screen.c)
#ifndef CLI_TERMINAL_SCREEN_H
#define CLI_TERMINAL_SCREEN_H
#include "embed_dispatch_layout.h"

typedef struct CliTerminalScreenSizeCfg CliTerminalScreenSizeCfg;
typedef struct CliTerminalScreenCfg CliTerminalScreenCfg;
typedef struct CliTerminalScreenBytesCfg CliTerminalScreenBytesCfg;
typedef struct CliTerminalScreenLineCfg CliTerminalScreenLineCfg;

// a cell's weight and marks, a bit each
#define CLI_TERMINAL_BOLD 0x01u
#define CLI_TERMINAL_DIM 0x02u
#define CLI_TERMINAL_ITALIC 0x04u
#define CLI_TERMINAL_UNDERLINE 0x08u
#define CLI_TERMINAL_INVERSE 0x10u
#define CLI_TERMINAL_STRIKE 0x20u

// a color: the default, one of the 256 by its index in the low byte, or red, green and blue in the low three bytes
#define CLI_TERMINAL_COLOR_DEFAULT 0u
#define CLI_TERMINAL_COLOR_INDEXED 0x01000000u
#define CLI_TERMINAL_COLOR_DIRECT 0x02000000u
#define CLI_TERMINAL_COLOR_KIND 0xff000000u

// the lines of the main screen's scrollback, the most bytes of an operating-system command read, the bytes the screen
// answers the shell between two reads of its answer, and the bytes of the place a shell reports
#define CLI_TERMINAL_SCROLLBACK_LINES 5000u
#define CLI_TERMINAL_COMMAND_MAX 4096u
#define CLI_TERMINAL_ANSWER_MAX 256u
#define CLI_TERMINAL_PLACE_MAX 4096u

// One cell: its letter, a code point, its colors and its marks
typedef struct
{
    unsigned int letter;
    unsigned int foreground;
    unsigned int background;
    unsigned int marks;
} CliTerminalCell;

// What is printed next is drawn in: its colors and its marks
typedef struct
{
    unsigned int foreground;
    unsigned int background;
    unsigned int marks;
} CliTerminalStyle;

// The cursor as the screen saves it: its column and row, its style, and whether the next letter wraps first
typedef struct
{
    unsigned int x;
    unsigned int y;
    CliTerminalStyle style;
    int wrap_pending;
} CliTerminalCursor;

// A line of the scrollback: where its cells start in the scrollback's cells and how many there are
typedef struct
{
    unsigned long long at;
    unsigned int columns;
} CliTerminalLine;

// A screen: its size; the one cell its main screen, its second screen, a mark for each row changed since the drawing
// last read it, the command being read, the answer for the shell and the place the shell reported lie in; which of the
// two screens is drawn on; the cursor, the region that scrolls and the cursor saved; the modes; where the reading of
// the output stands; and the scrollback in a cell of its own, its lines a ring and their cells a ring
typedef struct
{
    unsigned int columns;
    unsigned int rows;
    unsigned char *cell;
    CliTerminalCell *main;
    CliTerminalCell *second;
    CliTerminalCell *shown;
    unsigned char *changed;
    unsigned char *command;
    unsigned int command_size;
    unsigned char *answer;
    unsigned int answer_size;
    char *place;
    int place_reported;
    CliTerminalCursor cursor;
    CliTerminalCursor saved;
    unsigned int top;
    unsigned int bottom;
    int application_cursor;
    int wraps;
    int cursor_shown;
    int bracketed_paste;
    int inserts;
    unsigned int state;
    unsigned int ending;
    unsigned int letter;
    unsigned int letter_bytes_left;
    unsigned char *scrollback_cell;
    CliTerminalLine *scrollback_lines;
    CliTerminalCell *scrollback_cells;
    unsigned long long scrollback_cell_count;
    unsigned int scrollback_first;
    unsigned int scrollback_count;
    unsigned long long scrollback_cell_first;
    unsigned long long scrollback_cell_used;
} CliTerminalScreen;

// a screen `columns` wide and `rows` tall, its cursor home and every cell blank: 1, or 0 where its cells are not had;
// the screen given another size, each line cut or padded and the lines a shorter main screen loses from its top while
// the cursor would fall off its foot sent to the scrollback: 1, or 0 where its cells are not had and it stands as it
// was; and its cells given back
int cli_terminal_screen_init(const CliTerminalScreenSizeCfg *args);
int cli_terminal_screen_resize(const CliTerminalScreenSizeCfg *args);
void cli_terminal_screen_free(const CliTerminalScreenCfg *args);
// `size` bytes of a shell's output read onto the screen, a letter or a sequence cut by the end read on with the next
void cli_terminal_screen_write(const CliTerminalScreenBytesCfg *args);
// the cells of the scrollback's line `line`, the oldest 0, and their count in `*columns`: NULL past the last
const CliTerminalCell *cli_terminal_screen_scrollback(const CliTerminalScreenLineCfg *args);
struct CliTerminalScreenSizeCfg
{
    CliTerminalScreen *screen;
    unsigned int columns;
    unsigned int rows;
};

struct CliTerminalScreenCfg
{
    CliTerminalScreen *screen;
};

struct CliTerminalScreenBytesCfg
{
    CliTerminalScreen *screen;
    const unsigned char *bytes;
    unsigned long long size;
};

struct CliTerminalScreenLineCfg
{
    const CliTerminalScreen *screen_read;
    unsigned int line;
    unsigned int *columns;
};

typedef struct
{
    int (*init)(const CliTerminalScreenSizeCfg *args);
    int (*resize)(const CliTerminalScreenSizeCfg *args);
} CliTerminalScreenSizeNs;
EMBED_TABLE_LAYOUT(CliTerminalScreenSizeNs, init, resize);
EMBED_TABLE_STORAGE CliTerminalScreenSizeNs cli_terminal_screen_size EMBED_UNUSED = {
    .init = cli_terminal_screen_init,
    .resize = cli_terminal_screen_resize,
};

typedef struct
{
    void (*free)(const CliTerminalScreenCfg *args);
} CliTerminalScreenNs;
EMBED_TABLE_LAYOUT(CliTerminalScreenNs, free);
EMBED_TABLE_STORAGE CliTerminalScreenNs cli_terminal_screen EMBED_UNUSED = {
    .free = cli_terminal_screen_free,
};

typedef struct
{
    void (*write)(const CliTerminalScreenBytesCfg *args);
} CliTerminalScreenBytesNs;
EMBED_TABLE_LAYOUT(CliTerminalScreenBytesNs, write);
EMBED_TABLE_STORAGE CliTerminalScreenBytesNs cli_terminal_screen_bytes EMBED_UNUSED = {
    .write = cli_terminal_screen_write,
};

typedef struct
{
    const CliTerminalCell *(*scrollback)(const CliTerminalScreenLineCfg *args);
} CliTerminalScreenLineNs;
EMBED_TABLE_LAYOUT(CliTerminalScreenLineNs, scrollback);
EMBED_TABLE_STORAGE CliTerminalScreenLineNs cli_terminal_screen_line EMBED_UNUSED = {
    .scrollback = cli_terminal_screen_scrollback,
};

#endif
