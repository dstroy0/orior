// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// xterm.h: a shell's output read as an xterm reads it onto the rows a terminal's pane shows at its foot. Printed
// letters land at the cursor in the colors and weight of the moment; the control letters and the escape sequences move
// the cursor, erase, insert and delete, scroll a region, set the colors from the 16, the 256 and any red, green and
// blue, switch to the second screen full-screen programs draw on, and ask where the cursor is and what the terminal is.
// What the xterm does not read it reads past. A line that leaves the top of the main screen is handed to `history`, and
// the history is marked cleared where the output asks; the second screen hands nothing on (xterm.c)
#ifndef CLI_TERMINAL_XTERM_H
#define CLI_TERMINAL_XTERM_H
#include "embed_dispatch_layout.h"

typedef struct CliTerminalXtermSizeCfg CliTerminalXtermSizeCfg;
typedef struct CliTerminalXtermCfg CliTerminalXtermCfg;
typedef struct CliTerminalXtermBytesCfg CliTerminalXtermBytesCfg;
typedef struct CliTerminalXtermLineCfg CliTerminalXtermLineCfg;

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

// the most bytes of an operating-system command read, the bytes the xterm answers the shell between two reads of its
// answer, and the bytes of the place a shell reports
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

// The cursor as the xterm saves it: its column and row, its style, and whether the next letter wraps first
typedef struct
{
    unsigned int x;
    unsigned int y;
    CliTerminalStyle style;
    int wrap_pending;
} CliTerminalCursor;

// What takes the lines that leave the top of the main screen: `put`, handed each line's cells, and its context
typedef struct
{
    void (*put)(const CliTerminalXtermLineCfg *args);
    void *context;
} CliTerminalXtermHistory;

// An xterm: its size; the one cell its main screen, its second screen, a mark for each row changed since the drawing
// last read it, the command being read, the answer for the shell and the place the shell reported lie in; which of the
// two screens is drawn on; the cursor, the region that scrolls and the cursor saved; the modes; where the reading of
// the output stands; what takes the lines that leave the main screen's top, and whether the output asked the history
// cleared since the window last read it
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
    CliTerminalXtermHistory history;
    int history_cleared;
} CliTerminalXterm;

// an xterm `columns` wide and `rows` tall, its cursor home and every cell blank: 1, or 0 where its cells are not had;
// the xterm resized, each row cut or padded and the rows a shorter main screen loses from its top while the cursor
// would fall off its foot handed to the history: 1, or 0 where its cells are not had and it stands as it was; and its
// cells released
int cli_terminal_xterm_init(const CliTerminalXtermSizeCfg *args);
int cli_terminal_xterm_resize(const CliTerminalXtermSizeCfg *args);
void cli_terminal_xterm_free(const CliTerminalXtermCfg *args);
// `size` bytes of a shell's output read onto the xterm, a letter or a sequence cut by the end read on with the next
void cli_terminal_xterm_write(const CliTerminalXtermBytesCfg *args);
struct CliTerminalXtermSizeCfg
{
    CliTerminalXterm *xterm;
    unsigned int columns;
    unsigned int rows;
};

struct CliTerminalXtermCfg
{
    CliTerminalXterm *xterm;
};

struct CliTerminalXtermBytesCfg
{
    CliTerminalXterm *xterm;
    const unsigned char *bytes;
    unsigned long long size;
};

// a line the main screen's top lets go: its `columns` cells and the history's context
struct CliTerminalXtermLineCfg
{
    void *context;
    const CliTerminalCell *cells;
    unsigned int columns;
};

typedef struct
{
    int (*init)(const CliTerminalXtermSizeCfg *args);
    int (*resize)(const CliTerminalXtermSizeCfg *args);
} CliTerminalXtermSizeNs;
EMBED_TABLE_LAYOUT(CliTerminalXtermSizeNs, init, resize);
EMBED_TABLE_STORAGE CliTerminalXtermSizeNs cli_terminal_xterm_size EMBED_UNUSED = {
    .init = cli_terminal_xterm_init,
    .resize = cli_terminal_xterm_resize,
};

typedef struct
{
    void (*free)(const CliTerminalXtermCfg *args);
} CliTerminalXtermNs;
EMBED_TABLE_LAYOUT(CliTerminalXtermNs, free);
EMBED_TABLE_STORAGE CliTerminalXtermNs cli_terminal_xterm EMBED_UNUSED = {
    .free = cli_terminal_xterm_free,
};

typedef struct
{
    void (*write)(const CliTerminalXtermBytesCfg *args);
} CliTerminalXtermBytesNs;
EMBED_TABLE_LAYOUT(CliTerminalXtermBytesNs, write);
EMBED_TABLE_STORAGE CliTerminalXtermBytesNs cli_terminal_xterm_bytes EMBED_UNUSED = {
    .write = cli_terminal_xterm_write,
};

#endif
