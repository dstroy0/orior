// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// xterm.h: a shell's output read as an xterm reads it into the text a terminal's pane shows. The text, stored under
// `key`, is the only store: each of the screen's rows a line of it, the lines above the main screen's first the
// history. A paged text's lines are in its file and the pages, each edit written through them, so the pane scrolls
// the file; a text read whole is in the document. Printed characters are written into the row at the cursor from
// the output's bytes where they lie, in place of
// the characters they cover, the SGR of the style of the moment put before them where the line's style there is another
// and the style that was after them put back where characters follow; the xterm grammar plugin colors the text by those
// sequences. The control characters and the other escape sequences move the cursor, erase, insert and delete, scroll a
// region, switch to the second screen full-screen programs draw on, and ask where the cursor is and what the terminal
// is, each an edit of the text's lines or the cursor alone. A line that leaves the main screen's top as the whole
// screen scrolls joins the history where it is; the output's asking the history cleared takes its lines out. What
// the xterm does not read it reads past (xterm.c)
#ifndef CLI_TERMINAL_XTERM_H
#define CLI_TERMINAL_XTERM_H
#include "embed_dispatch_layout.h"

typedef struct CliTerminalXtermSizeCfg CliTerminalXtermSizeCfg;
typedef struct CliTerminalXtermKeepCfg CliTerminalXtermKeepCfg;
typedef struct CliTerminalXtermLineCfg CliTerminalXtermLineCfg;
typedef struct CliTerminalXtermCfg CliTerminalXtermCfg;
typedef struct CliTerminalXtermBytesCfg CliTerminalXtermBytesCfg;
typedef struct CliTerminalXtermSpansCfg CliTerminalXtermSpansCfg;

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
#define CLI_TERMINAL_COLOR_TYPE 0xff000000u

// the max bytes of an operating-system command read, and the bytes the xterm answers the shell between two reads of
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

// The cursor as the xterm saves it: its column and row, its style, and whether the next character wraps first
typedef struct
{
    unsigned int x;
    unsigned int y;
    CliTerminalStyle style;
    int wrap_pending;
} CliTerminalCursor;

// An xterm: its size; the key of the text it writes into, NULL before the text is stored, whether that text is paged,
// its edits since its file was last written through, the main screen's first line, the history the lines above it,
// and the first line of the screen drawn on, the main screen's or the second's after it; the command being read and
// the answer for the shell, each in its own bytes; the folder the path of a folder the shell reports is written into,
// NULL for none, and its bytes, whether a folder was reported since the terminal last read it and whether one ever
// was; the cursor, the region that scrolls and the cursor saved; the modes; where the reading of the output is and
// the bytes of a character cut across two writes; whether the text or the cursor moved since the drawing last read them;
// and whether the text refused an edit, after which nothing more is written into it
typedef struct
{
    unsigned int columns;
    unsigned int rows;
    const char *key;
    int paged;
    unsigned long long edits;
    unsigned long long history;
    unsigned long long base;
    int second;
    unsigned char command[CLI_TERMINAL_COMMAND_MAX];
    unsigned int command_size;
    unsigned char answer[CLI_TERMINAL_ANSWER_MAX];
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
    int cursor_displayed;
    int bracketed_paste;
    int inserts;
    unsigned int state;
    unsigned int ending;
    unsigned int character_bytes_left;
    unsigned int character_size;
    unsigned char character[4];
    int moved;
    int refused;
} CliTerminalXterm;

// An xterm `columns` wide and `rows` tall, its cursor home, writing into the text under `key`, paged where `paged`,
// whose main screen starts at the line `history`, the text holding that many lines and the screen's rows after them,
// and the path of a folder the shell reports into the `folder_size` bytes at `folder`: 1, or 0 where its cell is not
// had. The xterm resized, the text's rows with it: the rows a shorter main screen loses from its top while the cursor
// would fall off its foot joining the history, the rest let go from its foot, and a taller screen's rows put at its
// foot: 1, or 0 where the text refused. And its cell released
int cli_terminal_xterm_init(const CliTerminalXtermSizeCfg *args);
int cli_terminal_xterm_resize(const CliTerminalXtermSizeCfg *args);
// the history's lines beyond its last `retain` taken out of the text from its first line, the screen located where it
// was among the rest: how many were taken out
unsigned long long cli_terminal_xterm_trim(const CliTerminalXtermKeepCfg *args);
// the units of the text's line `line`, a paged text's read through the pages, in a cell of the temporary tier given
// back with persistent_buf_release, their count into `count`: the cell, or NULL with the count 0 where the line has no
// units or the cells cannot meet them
unsigned short *cli_terminal_xterm_line_units_alloc(const CliTerminalXtermLineCfg *args);
// `size` bytes of a shell's output read into the text, a character or a sequence cut by the end read on with the next
void cli_terminal_xterm_write(const CliTerminalXtermBytesCfg *args);
// the place among a row's `count` units at `units` where the character of column `column` starts, the sequences before it
// passed over: `count` where the row's characters end first
typedef struct
{
    const unsigned short *units;
    unsigned long long count;
    unsigned int column;
} CliTerminalXtermUnitsCfg;
unsigned long long cli_terminal_xterm_column_unit(const CliTerminalXtermUnitsCfg *args);
// the `size` bytes at `bytes` with the escape sequences among them taken out in place, the characters after each moved up:
// the count of the bytes left
typedef struct
{
    unsigned char *bytes;
    unsigned long long size;
} CliTerminalXtermPlainCfg;
unsigned long long cli_terminal_xterm_plain(const CliTerminalXtermPlainCfg *args);
// A span of a line's plain text in one style: the place of its first byte among the plain bytes and its bytes, its
// color, the index of one of the sixteen or CLI_TERMINAL_SPAN_PLAIN for the default and every other, and whether it is
// bold
#define CLI_TERMINAL_SPAN_PLAIN 256u
typedef struct
{
    unsigned int offset;
    unsigned int length;
    unsigned int color;
    int bold;
} CliTerminalXtermSpan;
// the `size` bytes of one line at `text` read through the SGR walk the screen's write reads them with: the bytes with
// the escape sequences taken out written into `plain_out`, `plain_capacity` at max, their count into
// `plain_size_out`, and a span each run of them whose color or weight is not the plain style's into `spans_out`,
// `spans_capacity` at max, a run the spans cannot meet left plain: the count of the spans
unsigned int cli_terminal_xterm_spans(const CliTerminalXtermSpansCfg *args);

struct CliTerminalXtermSizeCfg
{
    CliTerminalXterm *xterm;
    unsigned int columns;
    unsigned int rows;
    const char *key;
    int paged;
    unsigned long long history;
    char *folder;
    unsigned long long folder_size;
};

struct CliTerminalXtermKeepCfg
{
    CliTerminalXterm *xterm;
    unsigned long long retain;
};

struct CliTerminalXtermLineCfg
{
    const CliTerminalXterm *xterm_read;
    unsigned long long line;
    unsigned long long *count;
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

struct CliTerminalXtermSpansCfg
{
    const char *text;
    unsigned long long size;
    char *plain_out;
    unsigned long long plain_capacity;
    unsigned long long *plain_size_out;
    CliTerminalXtermSpan *spans_out;
    unsigned int spans_capacity;
};

typedef struct
{
    unsigned short *(*units_alloc)(const CliTerminalXtermLineCfg *args);
} CliTerminalXtermLineNs;
EMBED_TABLE_LAYOUT(CliTerminalXtermLineNs, units_alloc);
EMBED_TABLE_STORAGE CliTerminalXtermLineNs cli_terminal_xterm_line EMBED_UNUSED = {
    .units_alloc = cli_terminal_xterm_line_units_alloc,
};

typedef struct
{
    unsigned long long (*trim)(const CliTerminalXtermKeepCfg *args);
} CliTerminalXtermKeepNs;
EMBED_TABLE_LAYOUT(CliTerminalXtermKeepNs, trim);
EMBED_TABLE_STORAGE CliTerminalXtermKeepNs cli_terminal_xterm_retain EMBED_UNUSED = {
    .trim = cli_terminal_xterm_trim,
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
    void (*write)(const CliTerminalXtermBytesCfg *args);
} CliTerminalXtermBytesNs;
EMBED_TABLE_LAYOUT(CliTerminalXtermBytesNs, write);
EMBED_TABLE_STORAGE CliTerminalXtermBytesNs cli_terminal_xterm_bytes EMBED_UNUSED = {
    .write = cli_terminal_xterm_write,
};

typedef struct
{
    unsigned int (*spans)(const CliTerminalXtermSpansCfg *args);
} CliTerminalXtermSpansNs;
EMBED_TABLE_LAYOUT(CliTerminalXtermSpansNs, spans);
EMBED_TABLE_STORAGE CliTerminalXtermSpansNs cli_terminal_xterm_spans_table EMBED_UNUSED = {
    .spans = cli_terminal_xterm_spans,
};

#endif
