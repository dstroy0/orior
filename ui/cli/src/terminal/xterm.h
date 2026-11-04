// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// xterm.h: a shell's output read as an xterm reads it into the text a terminal's pane shows. The text, stored under
// `key`, is the only store: each of the screen's rows a line of it, the lines above the main screen's first the
// history. Printed letters are written into the row at the cursor from the output's bytes where they lie, in place of
// the letters they cover, the SGR of the style of the moment put before them where the line's style there is another
// and the style that stood after them put back where letters follow; the xterm grammar plugin colors the text by those
// sequences. The control letters and the other escape sequences move the cursor, erase, insert and delete, scroll a
// region, switch to the second screen full-screen programs draw on, and ask where the cursor is and what the terminal
// is, each an edit of the text's lines or the cursor alone. A line that leaves the main screen's top as the whole
// screen scrolls joins the history where it stands; the output's asking the history cleared takes its lines out. What
// the xterm does not read it reads past (xterm.c)
#ifndef CLI_TERMINAL_XTERM_H
#define CLI_TERMINAL_XTERM_H
#include "embed_dispatch_layout.h"

typedef struct CliTerminalXtermSizeCfg CliTerminalXtermSizeCfg;
typedef struct CliTerminalXtermCfg CliTerminalXtermCfg;
typedef struct CliTerminalXtermBytesCfg CliTerminalXtermBytesCfg;

// a style's weight and marks, a bit each
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

// the most bytes of an operating-system command read, and the bytes the xterm answers the shell between two reads of
// its answer
#define CLI_TERMINAL_COMMAND_MAX 4096u
#define CLI_TERMINAL_ANSWER_MAX 256u

// A style: its colors and its marks
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

// An xterm: its size; the key of the text it writes into, NULL before the text is stored, the main screen's first line,
// the history the lines above it, and the first line of the screen drawn on, the main screen's or the second's after
// it; the one cell the command being read and the answer for the shell lie in; the folder the path of a folder the
// shell reports is written into, NULL for none, and its bytes, whether a folder was reported since the terminal last
// read it and whether one ever was; the cursor, the region that scrolls and the cursor saved; the modes; where the
// reading of the output stands and the bytes of a letter cut across two writes; whether the text or the cursor moved
// since the drawing last read them; and whether the text refused an edit, after which nothing more is written into it
typedef struct
{
    unsigned int columns;
    unsigned int rows;
    const char *key;
    unsigned long long history;
    unsigned long long base;
    int second;
    unsigned char *cell;
    unsigned char *command;
    unsigned int command_size;
    unsigned char *answer;
    unsigned int answer_size;
    char *folder;
    unsigned long long folder_size;
    int folder_reported;
    int folder_known;
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
    unsigned int letter_bytes_left;
    unsigned int letter_size;
    unsigned char letter[4];
    int moved;
    int refused;
} CliTerminalXterm;

// An xterm `columns` wide and `rows` tall, its cursor home, writing into the text under `key` whose main screen starts
// at the line `history`, the text holding that many lines and the screen's rows after them, and the path of a folder
// the shell reports into the `folder_size` bytes at `folder`: 1, or 0 where its cell is not had. The xterm resized, the
// text's rows with it: the rows a shorter main screen loses from its top while the cursor would fall off its foot
// joining the history, the rest let go from its foot, and a taller screen's rows put at its foot: 1, or 0 where the
// text refused. And its cell released
int cli_terminal_xterm_init(const CliTerminalXtermSizeCfg *args);
int cli_terminal_xterm_resize(const CliTerminalXtermSizeCfg *args);
void cli_terminal_xterm_free(const CliTerminalXtermCfg *args);
// `size` bytes of a shell's output read into the text, a letter or a sequence cut by the end read on with the next
void cli_terminal_xterm_write(const CliTerminalXtermBytesCfg *args);
// the place among a row's `count` units at `units` where the letter of column `column` starts, the sequences before it
// passed over: `count` where the row's letters end first
typedef struct
{
    const unsigned short *units;
    unsigned long long count;
    unsigned int column;
} CliTerminalXtermUnitsCfg;
unsigned long long cli_terminal_xterm_column_unit(const CliTerminalXtermUnitsCfg *args);
// the style `style` changed by the SGR sequences among the `size` bytes at `bytes`, read as the xterm reads its output
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    CliTerminalStyle *style;
} CliTerminalXtermStyleCfg;
void cli_terminal_xterm_style_read(const CliTerminalXtermStyleCfg *args);
// the `size` bytes at `bytes` with the escape sequences among them taken out in place, the letters after each moved up:
// the count of the bytes left
typedef struct
{
    unsigned char *bytes;
    unsigned long long size;
} CliTerminalXtermPlainCfg;
unsigned long long cli_terminal_xterm_plain(const CliTerminalXtermPlainCfg *args);

struct CliTerminalXtermSizeCfg
{
    CliTerminalXterm *xterm;
    unsigned int columns;
    unsigned int rows;
    const char *key;
    unsigned long long history;
    char *folder;
    unsigned long long folder_size;
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
