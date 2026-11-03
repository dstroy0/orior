// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// xterm.c: a shell's output read as an xterm reads it (xterm.h). The output's plain runs are found a word at a time,
// each letter of a word with a control byte, an escape or a byte past ASCII in it read alone; a letter past ASCII is
// read from its UTF-8 bytes, an ill-formed one read as U+FFFD
#include "xterm.h"
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"

// where the reading of the output stands: in plain text, after an escape, in a control sequence, in an
// operating-system command, in a device control string, after the escape that may end a string, or before the one
// byte a character set's choice takes
#define XTERM_GROUND 0u
#define XTERM_ESCAPE 1u
#define XTERM_CONTROL 2u
#define XTERM_COMMAND 3u
#define XTERM_DEVICE 4u
#define XTERM_STRING_END 5u
#define XTERM_CHARSET 6u

// the most numbers of a control sequence read, and the number an empty one stands for
#define XTERM_NUMBERS 32u
#define XTERM_ABSENT 0xffffffffu

// the tab stops' spacing, and the letter an ill-formed UTF-8 run reads as
#define XTERM_TAB 8u
#define XTERM_REPLACEMENT 0xfffdu

typedef struct
{
    CliTerminalXterm *xterm;
    const CliTerminalXterm *xterm_read;
    unsigned int x;
    unsigned int y;
    unsigned int from;
    unsigned int to;
    unsigned int count;
    unsigned int mode;
    int on;
    unsigned int letter;
    unsigned char byte;
    const unsigned char *bytes;
    unsigned long long size;
    const unsigned int *numbers;
    unsigned int *numbers_out;
    unsigned int number_count;
    const CliTerminalCell *cells;
    unsigned int columns;
    unsigned int rows;
    const char *text;
    unsigned long long value;
} XtermCtx;

// the cells of row `y` of the screen drawn on
EMBED_INLINE CliTerminalCell *xterm_row(const XtermCtx *args)
{
    return args->xterm->shown + ((unsigned long long)args->y * args->xterm->columns);
}

// a blank cell in the background of the style of the moment, its letter a space
EMBED_INLINE CliTerminalCell xterm_blank(const XtermCtx *args)
{
    const CliTerminalCell blank = {(unsigned int)' ', CLI_TERMINAL_COLOR_DEFAULT, args->xterm->cursor.style.background,
                                   0u};
    return blank;
}

// row `y` marked changed
EMBED_INLINE void xterm_touch(const XtermCtx *args)
{
    args->xterm->changed[args->y] = 1u;
}

// the cells of row `y` from `from` up to, not including, `to` blank
EMBED_INLINE void xterm_cells_erase(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    CliTerminalCell *const row = EMBED_CALL(xterm_row, XtermCtx, .xterm = xterm, .y = args->y);
    const CliTerminalCell blank = EMBED_CALL(xterm_blank, XtermCtx, .xterm = xterm);
    const unsigned int to = (args->to < xterm->columns) ? args->to : xterm->columns;
    for (unsigned int x = args->from; x < to; x += 1u)
    {
        row[x] = blank;
    }
    EMBED_CALL(xterm_touch, XtermCtx, .xterm = xterm, .y = args->y);
}

// the rows from `from` up to, not including, `to` blank
EMBED_INLINE void xterm_lines_erase(const XtermCtx *args)
{
    const unsigned int to = (args->to < args->xterm->rows) ? args->to : args->xterm->rows;
    for (unsigned int y = args->from; y < to; y += 1u)
    {
        EMBED_CALL(xterm_cells_erase, XtermCtx, .xterm = args->xterm, .y = y, .from = 0u, .to = XTERM_ABSENT);
    }
}

// `columns` cells at `cells`, a line the main screen's top lets go, handed to the history where one takes them
EMBED_INLINE void xterm_history_put(const XtermCtx *args)
{
    const CliTerminalXtermHistory *const history = &args->xterm->history;
    if (history->put != NULL)
    {
        history->put(
            &(CliTerminalXtermLineCfg){.context = history->context, .cells = args->cells, .columns = args->columns});
    }
}

// the region scrolled up `count` lines; on the main screen, with the region at the screen's top, each line that leaves
// the top goes to the history
EMBED_INLINE void xterm_scroll_up(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned int height = xterm->bottom - xterm->top + 1u;
    const unsigned int count = (args->count < height) ? args->count : height;
    const unsigned long long columns = xterm->columns;
    for (unsigned int step = 0u; step < count; step += 1u)
    {
        CliTerminalCell *const top = EMBED_CALL(xterm_row, XtermCtx, .xterm = xterm, .y = xterm->top);
        if ((xterm->shown == xterm->main) && (xterm->top == 0u))
        {
            EMBED_CALL(xterm_history_put, XtermCtx, .xterm = xterm, .cells = top, .columns = xterm->columns);
        }
        const unsigned long long moved = (unsigned long long)(xterm->bottom - xterm->top) * columns;
        for (unsigned long long cell = 0ULL; cell < moved; cell += 1ULL)
        {
            top[cell] = top[cell + columns];
        }
        EMBED_CALL(xterm_cells_erase, XtermCtx, .xterm = xterm, .y = xterm->bottom, .from = 0u, .to = XTERM_ABSENT);
    }
    for (unsigned int y = xterm->top; y <= xterm->bottom; y += 1u)
    {
        xterm->changed[y] = 1u;
    }
}

// the region scrolled down `count` lines, blank lines coming in at its top
EMBED_INLINE void xterm_scroll_down(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned int height = xterm->bottom - xterm->top + 1u;
    const unsigned int count = (args->count < height) ? args->count : height;
    const unsigned long long columns = xterm->columns;
    for (unsigned int step = 0u; step < count; step += 1u)
    {
        CliTerminalCell *const top = EMBED_CALL(xterm_row, XtermCtx, .xterm = xterm, .y = xterm->top);
        for (unsigned long long cell = (unsigned long long)(xterm->bottom - xterm->top) * columns; cell > 0ULL;
             cell -= 1ULL)
        {
            top[cell - 1ULL + columns] = top[cell - 1ULL];
        }
        EMBED_CALL(xterm_cells_erase, XtermCtx, .xterm = xterm, .y = xterm->top, .from = 0u, .to = XTERM_ABSENT);
    }
    for (unsigned int y = xterm->top; y <= xterm->bottom; y += 1u)
    {
        xterm->changed[y] = 1u;
    }
}

// the cursor moved to column `x` and row `y`, each bounded by the screen, the next letter no longer wrapping first
EMBED_INLINE void xterm_move(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    EMBED_CALL(xterm_touch, XtermCtx, .xterm = xterm, .y = xterm->cursor.y);
    xterm->cursor.x = (args->x < xterm->columns) ? args->x : (xterm->columns - 1u);
    xterm->cursor.y = (args->y < xterm->rows) ? args->y : (xterm->rows - 1u);
    xterm->cursor.wrap_pending = 0;
    EMBED_CALL(xterm_touch, XtermCtx, .xterm = xterm, .y = xterm->cursor.y);
}

// a line fed: the region scrolled up at its foot, else the cursor a row down
EMBED_INLINE void xterm_line_feed(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    if (xterm->cursor.y == xterm->bottom)
    {
        EMBED_CALL(xterm_scroll_up, XtermCtx, .xterm = xterm, .count = 1u);
        return;
    }
    if ((xterm->cursor.y + 1u) < xterm->rows)
    {
        EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = xterm->cursor.x, .y = xterm->cursor.y + 1u);
    }
}

// the cursor a row up: the region scrolled down at its top, else the cursor a row up
EMBED_INLINE void xterm_reverse_index(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    if (xterm->cursor.y == xterm->top)
    {
        EMBED_CALL(xterm_scroll_down, XtermCtx, .xterm = xterm, .count = 1u);
        return;
    }
    if (xterm->cursor.y > 0u)
    {
        EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = xterm->cursor.x, .y = xterm->cursor.y - 1u);
    }
}

// the letter `letter` printed at the cursor in the style of the moment, a line fed first where the last printed stood
// at the last column; in insert mode the cells from the cursor on move a cell right first
EMBED_INLINE void xterm_print(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    if (xterm->cursor.wrap_pending && xterm->wraps)
    {
        xterm->cursor.wrap_pending = 0;
        xterm->cursor.x = 0u;
        EMBED_CALL(xterm_line_feed, XtermCtx, .xterm = xterm);
    }
    CliTerminalCell *const row = EMBED_CALL(xterm_row, XtermCtx, .xterm = xterm, .y = xterm->cursor.y);
    for (unsigned int x = xterm->columns - 1u; xterm->inserts && (x > xterm->cursor.x); x -= 1u)
    {
        row[x] = row[x - 1u];
    }
    row[xterm->cursor.x] = (CliTerminalCell){args->letter, xterm->cursor.style.foreground,
                                             xterm->cursor.style.background, xterm->cursor.style.marks};
    EMBED_CALL(xterm_touch, XtermCtx, .xterm = xterm, .y = xterm->cursor.y);
    xterm->cursor.wrap_pending = (xterm->cursor.x + 1u) == xterm->columns;
    xterm->cursor.x += xterm->cursor.wrap_pending ? 0u : 1u;
}

// the cursor saved, or the one saved restored, a screen with none saved restoring the home and the plain style
EMBED_INLINE void xterm_save(const XtermCtx *args)
{
    args->xterm->saved = args->xterm->cursor;
}

EMBED_INLINE void xterm_restore(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const CliTerminalCursor saved = xterm->saved;
    EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = saved.x, .y = saved.y);
    xterm->cursor.style = saved.style;
    xterm->cursor.wrap_pending = saved.wrap_pending;
}

// every row marked changed
EMBED_INLINE void xterm_touch_all(const XtermCtx *args)
{
    for (unsigned int y = 0u; y < args->xterm->rows; y += 1u)
    {
        args->xterm->changed[y] = 1u;
    }
}

// the second screen drawn on, blank, the cursor saved and the region the whole screen; and the main screen drawn on
// again, the cursor restored
EMBED_INLINE void xterm_second_enter(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    if (xterm->shown != xterm->main)
    {
        return;
    }
    EMBED_CALL(xterm_save, XtermCtx, .xterm = xterm);
    const unsigned long long cells = (unsigned long long)xterm->columns * xterm->rows;
    const CliTerminalCell blank = {(unsigned int)' ', CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u};
    for (unsigned long long cell = 0ULL; cell < cells; cell += 1ULL)
    {
        xterm->second[cell] = blank;
    }
    xterm->shown = xterm->second;
    xterm->top = 0u;
    xterm->bottom = xterm->rows - 1u;
    EMBED_CALL(xterm_touch_all, XtermCtx, .xterm = xterm);
}

EMBED_INLINE void xterm_second_leave(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    if (xterm->shown == xterm->main)
    {
        return;
    }
    xterm->shown = xterm->main;
    xterm->top = 0u;
    xterm->bottom = xterm->rows - 1u;
    EMBED_CALL(xterm_touch_all, XtermCtx, .xterm = xterm);
    EMBED_CALL(xterm_restore, XtermCtx, .xterm = xterm);
}

// the cursor home, the style plain, the region the whole screen, nothing saved and every mode as a terminal starts
EMBED_INLINE void xterm_reset(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const CliTerminalCursor home = {0u, 0u, {CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u}, 0};
    xterm->cursor = home;
    xterm->saved = home;
    xterm->top = 0u;
    xterm->bottom = xterm->rows - 1u;
    xterm->application_cursor = 0;
    xterm->wraps = 1;
    xterm->cursor_shown = 1;
    xterm->bracketed_paste = 0;
    xterm->inserts = 0;
}

// `text`, a C string, put after the answer for the shell, where it fits whole
EMBED_INLINE void xterm_answer(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    VerbaLinea line = {(char *)xterm->answer + xterm->answer_size, CLI_TERMINAL_ANSWER_MAX - xterm->answer_size, 0ULL};
    mmgr_line_put(&line, args->text);
    // the answer is far below CLI_TERMINAL_ANSWER_MAX, which an unsigned int counts
    xterm->answer_size += mmgr_line_ok(&line) ? (unsigned int)line.at : 0u;
}

// the answer to where the cursor is: ESC [ row ; column R
EMBED_INLINE void xterm_answer_place(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    VerbaLinea line = {(char *)xterm->answer + xterm->answer_size, CLI_TERMINAL_ANSWER_MAX - xterm->answer_size, 0ULL};
    mmgr_line_put(&line, "\x1b[");
    mmgr_line_decimal(&line, (unsigned long long)xterm->cursor.y + 1ULL, 1u);
    mmgr_line_ch(&line, ';');
    mmgr_line_decimal(&line, (unsigned long long)xterm->cursor.x + 1ULL, 1u);
    mmgr_line_ch(&line, 'R');
    // the answer is far below CLI_TERMINAL_ANSWER_MAX, which an unsigned int counts
    xterm->answer_size += mmgr_line_ok(&line) ? (unsigned int)line.at : 0u;
}

// the number at `count` of the sequence's `number_count` numbers, else `value` where it is past them or empty
EMBED_INLINE unsigned int xterm_number(const XtermCtx *args)
{
    const int present = (args->count < args->number_count) && (args->numbers[args->count] != XTERM_ABSENT);
    // the number read in its place is an unsigned int's
    return present ? args->numbers[args->count] : (unsigned int)args->value;
}

// a red, a green and a blue each read as at most 255, put together as a direct color
EMBED_INLINE unsigned int xterm_direct(const XtermCtx *args)
{
    unsigned int color = CLI_TERMINAL_COLOR_DIRECT;
    for (unsigned int part = 0u; part < 3u; part += 1u)
    {
        const unsigned int value =
            EMBED_CALL(xterm_number, XtermCtx, .numbers = args->numbers, .number_count = args->number_count,
                       .count = args->from + part, .value = 0ULL);
        color |= ((value < 255u) ? value : 255u) << (16u - (8u * part));
    }
    return color;
}

// the style of the moment changed by a Select Graphic Rendition's numbers
EMBED_INLINE void xterm_rendition(const XtermCtx *args)
{
    CliTerminalStyle *const style = &args->xterm->cursor.style;
    const unsigned int *const numbers = args->numbers;
    const unsigned int count = args->number_count;
    static const unsigned int s_on[10] = {0u,
                                          CLI_TERMINAL_BOLD,
                                          CLI_TERMINAL_DIM,
                                          CLI_TERMINAL_ITALIC,
                                          CLI_TERMINAL_UNDERLINE,
                                          0u,
                                          0u,
                                          CLI_TERMINAL_INVERSE,
                                          0u,
                                          CLI_TERMINAL_STRIKE};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int value = (numbers[at] == XTERM_ABSENT) ? 0u : numbers[at];
        const CliTerminalStyle plain = {CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u};
        *style = (value == 0u) ? plain : *style;
        style->marks |= (value < 10u) ? s_on[value] : 0u;
        style->marks &= (value == 22u) ? ~(CLI_TERMINAL_BOLD | CLI_TERMINAL_DIM) : 0xffffffffu;
        // 23, 24, 27 and 29 take off what 3, 4, 7 and 9 put on
        style->marks &= ((value >= 23u) && (value <= 29u)) ? ~s_on[value - 20u] : 0xffffffffu;
        style->foreground = ((value >= 30u) && (value <= 37u))   ? (CLI_TERMINAL_COLOR_INDEXED | (value - 30u))
                            : ((value >= 90u) && (value <= 97u)) ? (CLI_TERMINAL_COLOR_INDEXED | (value - 82u))
                            : (value == 39u)                     ? CLI_TERMINAL_COLOR_DEFAULT
                                                                 : style->foreground;
        style->background = ((value >= 40u) && (value <= 47u))     ? (CLI_TERMINAL_COLOR_INDEXED | (value - 40u))
                            : ((value >= 100u) && (value <= 107u)) ? (CLI_TERMINAL_COLOR_INDEXED | (value - 92u))
                            : (value == 49u)                       ? CLI_TERMINAL_COLOR_DEFAULT
                                                                   : style->background;
        if ((value != 38u) && (value != 48u))
        {
            continue;
        }
        const unsigned int kind = EMBED_CALL(xterm_number, XtermCtx, .numbers = numbers, .number_count = count,
                                             .count = at + 1u, .value = 0ULL);
        unsigned int color = (value == 38u) ? style->foreground : style->background;
        if (kind == 5u)
        {
            color = CLI_TERMINAL_COLOR_INDEXED | (EMBED_CALL(xterm_number, XtermCtx, .numbers = numbers,
                                                             .number_count = count, .count = at + 2u, .value = 0ULL) &
                                                  0xffu);
            at += 2u;
        }
        if (kind == 2u)
        {
            // an empty color space may stand before the three parts, as 38:2::r:g:b
            const unsigned int from = (((at + 2u) < count) && (numbers[at + 2u] == XTERM_ABSENT)) ? at + 3u : at + 2u;
            color = EMBED_CALL(xterm_direct, XtermCtx, .numbers = numbers, .number_count = count, .from = from);
            at = from + 2u;
        }
        style->foreground = (value == 38u) ? color : style->foreground;
        style->background = (value == 48u) ? color : style->background;
    }
}

// a private mode set or reset: the cursor keys' application mode, wrapping, the cursor shown, the second screen and
// bracketed paste
EMBED_INLINE void xterm_private_mode(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    switch (args->mode)
    {
    case 1u:
        xterm->application_cursor = args->on;
        break;
    case 7u:
        xterm->wraps = args->on;
        break;
    case 25u:
        xterm->cursor_shown = args->on;
        EMBED_CALL(xterm_touch, XtermCtx, .xterm = xterm, .y = xterm->cursor.y);
        break;
    case 47u:
    case 1047u:
    case 1049u:
        if (!args->on)
        {
            EMBED_CALL(xterm_second_leave, XtermCtx, .xterm = xterm);
            break;
        }
        EMBED_CALL(xterm_second_enter, XtermCtx, .xterm = xterm);
        if (args->mode != 1049u)
        {
            EMBED_CALL(xterm_lines_erase, XtermCtx, .xterm = xterm, .from = 0u, .to = xterm->rows);
        }
        break;
    case 2004u:
        xterm->bracketed_paste = args->on;
        break;
    default:
        break;
    }
}

// the display erased: from the cursor to its end, from its start to the cursor, whole, or the history marked cleared
EMBED_INLINE void xterm_display_erase(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const CliTerminalCursor at = xterm->cursor;
    switch (args->mode)
    {
    case 0u:
        EMBED_CALL(xterm_cells_erase, XtermCtx, .xterm = xterm, .y = at.y, .from = at.x, .to = XTERM_ABSENT);
        EMBED_CALL(xterm_lines_erase, XtermCtx, .xterm = xterm, .from = at.y + 1u, .to = xterm->rows);
        break;
    case 1u:
        EMBED_CALL(xterm_lines_erase, XtermCtx, .xterm = xterm, .from = 0u, .to = at.y);
        EMBED_CALL(xterm_cells_erase, XtermCtx, .xterm = xterm, .y = at.y, .from = 0u, .to = at.x + 1u);
        break;
    case 2u:
        EMBED_CALL(xterm_lines_erase, XtermCtx, .xterm = xterm, .from = 0u, .to = xterm->rows);
        break;
    case 3u:
        xterm->history_cleared = 1;
        break;
    default:
        break;
    }
}

// `count` lines inserted at the cursor's row, or deleted from it, inside the region, the cursor to the row's start
EMBED_INLINE void xterm_lines_insert(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    if ((xterm->cursor.y < xterm->top) || (xterm->cursor.y > xterm->bottom))
    {
        return;
    }
    const unsigned int height = xterm->bottom - xterm->cursor.y + 1u;
    const unsigned int count = (args->count < height) ? args->count : height;
    const unsigned long long columns = xterm->columns;
    CliTerminalCell *const first = EMBED_CALL(xterm_row, XtermCtx, .xterm = xterm, .y = xterm->cursor.y);
    const unsigned long long moved = (unsigned long long)(height - count) * columns;
    const unsigned long long shift = (unsigned long long)count * columns;
    // inserted, the rows from the cursor's move down, the last first; deleted, the rows below move up
    for (unsigned long long cell = 0ULL; args->on && (cell < moved); cell += 1ULL)
    {
        first[moved - 1ULL - cell + shift] = first[moved - 1ULL - cell];
    }
    for (unsigned long long cell = 0ULL; !args->on && (cell < moved); cell += 1ULL)
    {
        first[cell] = first[cell + shift];
    }
    const unsigned int blank_from = args->on ? xterm->cursor.y : (xterm->bottom + 1u - count);
    EMBED_CALL(xterm_lines_erase, XtermCtx, .xterm = xterm, .from = blank_from, .to = blank_from + count);
    for (unsigned int y = xterm->cursor.y; y <= xterm->bottom; y += 1u)
    {
        xterm->changed[y] = 1u;
    }
    EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = 0u, .y = xterm->cursor.y);
}

// `count` cells at the cursor: blanks inserted, the rest moving right, or cells deleted, the rest moving left
EMBED_INLINE void xterm_cells_insert(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    CliTerminalCell *const row = EMBED_CALL(xterm_row, XtermCtx, .xterm = xterm, .y = xterm->cursor.y);
    const unsigned int left = xterm->columns - xterm->cursor.x;
    const unsigned int count = (args->count < left) ? args->count : left;
    const CliTerminalCell blank = EMBED_CALL(xterm_blank, XtermCtx, .xterm = xterm);
    if (args->on)
    {
        for (unsigned int x = xterm->columns; x > (xterm->cursor.x + count); x -= 1u)
        {
            row[x - 1u] = row[x - 1u - count];
        }
        for (unsigned int x = xterm->cursor.x; x < (xterm->cursor.x + count); x += 1u)
        {
            row[x] = blank;
        }
    }
    else
    {
        for (unsigned int x = xterm->cursor.x; (x + count) < xterm->columns; x += 1u)
        {
            row[x] = row[x + count];
        }
        for (unsigned int x = xterm->columns - count; x < xterm->columns; x += 1u)
        {
            row[x] = blank;
        }
    }
    EMBED_CALL(xterm_touch, XtermCtx, .xterm = xterm, .y = xterm->cursor.y);
}

// the region set from its two numbers, the whole screen where its bottom is not below its top, the cursor home
EMBED_INLINE void xterm_region(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned int top = EMBED_CALL(xterm_number, XtermCtx, .numbers = args->numbers,
                                        .number_count = args->number_count, .count = 0u, .value = 1ULL);
    const unsigned int bottom = EMBED_CALL(xterm_number, XtermCtx, .numbers = args->numbers,
                                           .number_count = args->number_count, .count = 1u, .value = xterm->rows);
    xterm->top = (top > 0u) ? top - 1u : 0u;
    xterm->bottom = ((bottom > 0u) && (bottom < xterm->rows)) ? bottom - 1u : xterm->rows - 1u;
    if (xterm->bottom <= xterm->top)
    {
        xterm->top = 0u;
        xterm->bottom = xterm->rows - 1u;
    }
    EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = 0u, .y = 0u);
}

// a control sequence's numbers read from the `size` bytes of its body at `bytes`, past a leading mark, into
// `numbers`, each empty one XTERM_ABSENT: their count
EMBED_INLINE unsigned int xterm_numbers_read(const XtermCtx *args)
{
    unsigned int *const numbers = args->numbers_out;
    unsigned int count = 1u;
    numbers[0] = XTERM_ABSENT;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        const unsigned char byte = args->bytes[at];
        const int digit = (byte >= (unsigned char)'0') && (byte <= (unsigned char)'9');
        const int cut = ((byte == (unsigned char)';') || (byte == (unsigned char)':')) && (count < XTERM_NUMBERS);
        const unsigned int was = (numbers[count - 1u] == XTERM_ABSENT) ? 0u : numbers[count - 1u];
        if (digit)
        {
            // a number of more than nine digits stands at its first nine, far below what an unsigned int counts
            numbers[count - 1u] = (was < 100000000u) ? ((was * 10u) + (unsigned int)(byte - (unsigned char)'0')) : was;
        }
        else if (cut)
        {
            numbers[count] = XTERM_ABSENT;
            count += 1u;
        }
    }
    return count;
}

// a control sequence's cursor moves, `final` its last byte in `byte` and its first number `count`, at least 1
EMBED_INLINE int xterm_control_move(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const CliTerminalCursor at = xterm->cursor;
    const unsigned int count = args->count;
    unsigned int x = at.x;
    unsigned int y = at.y;
    const unsigned int floor = (at.y >= xterm->top) ? xterm->top : 0u;
    const unsigned int ceiling = (at.y <= xterm->bottom) ? xterm->bottom : (xterm->rows - 1u);
    switch (args->byte)
    {
    case 'A':
        y = ((at.y > count) && ((at.y - count) > floor)) ? (at.y - count) : floor;
        break;
    case 'B':
        y = ((at.y + count) < ceiling) ? (at.y + count) : ceiling;
        break;
    case 'C':
        x = at.x + count;
        break;
    case 'D':
        x = (at.x > count) ? (at.x - count) : 0u;
        break;
    case 'E':
        x = 0u;
        y = at.y + count;
        break;
    case 'F':
        x = 0u;
        y = (at.y > count) ? (at.y - count) : 0u;
        break;
    case 'G':
    case '`':
        x = count - 1u;
        break;
    case 'H':
    case 'f': {
        const unsigned int column = EMBED_CALL(xterm_number, XtermCtx, .numbers = args->numbers,
                                               .number_count = args->number_count, .count = 1u, .value = 1ULL);
        x = ((column > 0u) ? column : 1u) - 1u;
        y = count - 1u;
        break;
    }
    case 'd':
        y = count - 1u;
        break;
    default:
        return 0;
    }
    EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = x, .y = y);
    return 1;
}

// a control sequence read, its body the command read and its last byte `byte`; one with bytes between its numbers and
// its last byte is read past
EMBED_INLINE void xterm_control(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned char *const body = xterm->command;
    const unsigned int size = xterm->command_size;
    const unsigned char lead = (size > 0u) ? body[0] : 0u;
    const int marked = (lead == (unsigned char)'?') || (lead == (unsigned char)'>') || (lead == (unsigned char)'=') ||
                       (lead == (unsigned char)'!');
    int between = 0;
    for (unsigned int at = marked ? 1u : 0u; at < size; at += 1u)
    {
        between |= !(((body[at] >= (unsigned char)'0') && (body[at] <= (unsigned char)'9')) ||
                     (body[at] == (unsigned char)';') || (body[at] == (unsigned char)':'));
    }
    unsigned int numbers[XTERM_NUMBERS];
    const unsigned int count = EMBED_CALL(xterm_numbers_read, XtermCtx, .numbers_out = numbers,
                                          .bytes = body + (marked ? 1u : 0u), .size = size - (marked ? 1u : 0u));
    const unsigned int first =
        EMBED_CALL(xterm_number, XtermCtx, .numbers = numbers, .number_count = count, .count = 0u, .value = 1ULL);
    const unsigned int times = (first > 0u) ? first : 1u;
    const unsigned int how =
        EMBED_CALL(xterm_number, XtermCtx, .numbers = numbers, .number_count = count, .count = 0u, .value = 0ULL);
    if (lead == (unsigned char)'?')
    {
        for (unsigned int at = 0u; ((args->byte == 'h') || (args->byte == 'l')) && (at < count); at += 1u)
        {
            EMBED_CALL(xterm_private_mode, XtermCtx, .xterm = xterm, .mode = numbers[at], .on = args->byte == 'h');
        }
        return;
    }
    if ((lead == (unsigned char)'>') || (lead == (unsigned char)'='))
    {
        if (args->byte == 'c')
        {
            EMBED_CALL(xterm_answer, XtermCtx, .xterm = xterm, .text = "\x1b[>0;0;0c");
        }
        return;
    }
    if (between || EMBED_CALL(xterm_control_move, XtermCtx, .xterm = xterm, .byte = args->byte, .count = times,
                              .numbers = numbers, .number_count = count))
    {
        return;
    }
    const CliTerminalCursor at = xterm->cursor;
    switch (args->byte)
    {
    case '@':
    case 'P':
        EMBED_CALL(xterm_cells_insert, XtermCtx, .xterm = xterm, .count = times, .on = args->byte == '@');
        break;
    case 'J':
        EMBED_CALL(xterm_display_erase, XtermCtx, .xterm = xterm, .mode = how);
        break;
    case 'K':
        EMBED_CALL(xterm_cells_erase, XtermCtx, .xterm = xterm, .y = at.y, .from = (how == 0u) ? at.x : 0u,
                   .to = (how == 1u) ? (at.x + 1u) : XTERM_ABSENT);
        break;
    case 'L':
    case 'M':
        EMBED_CALL(xterm_lines_insert, XtermCtx, .xterm = xterm, .count = times, .on = args->byte == 'L');
        break;
    case 'X':
        EMBED_CALL(xterm_cells_erase, XtermCtx, .xterm = xterm, .y = at.y, .from = at.x, .to = at.x + times);
        break;
    case 'S':
        EMBED_CALL(xterm_scroll_up, XtermCtx, .xterm = xterm, .count = times);
        break;
    case 'T':
        EMBED_CALL(xterm_scroll_down, XtermCtx, .xterm = xterm, .count = times);
        break;
    case 'm':
        EMBED_CALL(xterm_rendition, XtermCtx, .xterm = xterm, .numbers = numbers, .number_count = count);
        break;
    case 'r':
        EMBED_CALL(xterm_region, XtermCtx, .xterm = xterm, .numbers = numbers, .number_count = count);
        break;
    case 's':
        EMBED_CALL(xterm_save, XtermCtx, .xterm = xterm);
        break;
    case 'u':
        EMBED_CALL(xterm_restore, XtermCtx, .xterm = xterm);
        break;
    case 'h':
    case 'l':
        xterm->inserts = (how == 4u) ? (args->byte == 'h') : xterm->inserts;
        break;
    case 'n':
        if (how == 6u)
        {
            EMBED_CALL(xterm_answer_place, XtermCtx, .xterm = xterm);
        }
        if (how == 5u)
        {
            EMBED_CALL(xterm_answer, XtermCtx, .xterm = xterm, .text = "\x1b[0n");
        }
        break;
    case 'c':
        EMBED_CALL(xterm_answer, XtermCtx, .xterm = xterm, .text = "\x1b[?1;2c");
        break;
    default:
        break;
    }
}

// an operating-system command read: OSC 7, the folder the shell stands in as a file URL, becomes the place reported
EMBED_INLINE void xterm_command(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned char *const body = xterm->command;
    if ((xterm->command_size < 2u) || (body[0] != (unsigned char)'7') || (body[1] != (unsigned char)';'))
    {
        return;
    }
    const unsigned int size = xterm->command_size - 2u;
    const unsigned int taken = (size < (CLI_TERMINAL_PLACE_MAX - 1u)) ? size : (CLI_TERMINAL_PLACE_MAX - 1u);
    memor.cpy(&(MemoriaCfg){.dst = xterm->place, .src = body + 2, .bytes = taken});
    xterm->place[taken] = '\0';
    xterm->place_reported = 1;
}

// a control letter: the bell read past, backspace, a tab to the next stop, a line fed, and a carriage return
EMBED_INLINE void xterm_control_letter(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const CliTerminalCursor at = xterm->cursor;
    switch (args->byte)
    {
    case 0x08u:
        EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = (at.x > 0u) ? (at.x - 1u) : 0u, .y = at.y);
        break;
    case 0x09u:
        EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = ((at.x / XTERM_TAB) + 1u) * XTERM_TAB, .y = at.y);
        break;
    case 0x0au:
    case 0x0bu:
    case 0x0cu:
        EMBED_CALL(xterm_line_feed, XtermCtx, .xterm = xterm);
        break;
    case 0x0du:
        EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = 0u, .y = at.y);
        break;
    default:
        break;
    }
}

// the byte after an escape: a control sequence, an operating-system command, a device control string or a character
// set's choice begun; the cursor saved or restored; an index, a next line, a reverse index or a reset
EMBED_INLINE void xterm_escape(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    xterm->state = XTERM_GROUND;
    xterm->command_size = 0u;
    switch (args->byte)
    {
    case '[':
        xterm->state = XTERM_CONTROL;
        break;
    case ']':
        xterm->state = XTERM_COMMAND;
        break;
    case 'P':
        xterm->state = XTERM_DEVICE;
        break;
    case '(':
    case ')':
    case '*':
    case '+':
        xterm->state = XTERM_CHARSET;
        break;
    case '7':
        EMBED_CALL(xterm_save, XtermCtx, .xterm = xterm);
        break;
    case '8':
        EMBED_CALL(xterm_restore, XtermCtx, .xterm = xterm);
        break;
    case 'D':
        EMBED_CALL(xterm_line_feed, XtermCtx, .xterm = xterm);
        break;
    case 'E':
        EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = 0u, .y = xterm->cursor.y);
        EMBED_CALL(xterm_line_feed, XtermCtx, .xterm = xterm);
        break;
    case 'M':
        EMBED_CALL(xterm_reverse_index, XtermCtx, .xterm = xterm);
        break;
    case 'c':
        EMBED_CALL(xterm_second_leave, XtermCtx, .xterm = xterm);
        EMBED_CALL(xterm_reset, XtermCtx, .xterm = xterm);
        EMBED_CALL(xterm_lines_erase, XtermCtx, .xterm = xterm, .from = 0u, .to = xterm->rows);
        break;
    default:
        break;
    }
}

// a byte put after the command being read, where there is space for it
EMBED_INLINE void xterm_command_put(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    if (xterm->command_size < CLI_TERMINAL_COMMAND_MAX)
    {
        xterm->command[xterm->command_size] = args->byte;
        xterm->command_size += 1u;
    }
}

// a byte read in plain text: an escape begun, a control letter, or a letter's byte, a letter printed once whole
EMBED_INLINE void xterm_ground(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned char byte = args->byte;
    const int continues = (byte & 0xc0u) == 0x80u;
    if ((xterm->letter_bytes_left > 0u) && !continues)
    {
        // a letter cut before its end reads as the replacement, and the byte after it is read afresh
        xterm->letter_bytes_left = 0u;
        EMBED_CALL(xterm_print, XtermCtx, .xterm = xterm, .letter = XTERM_REPLACEMENT);
    }
    if (xterm->letter_bytes_left > 0u)
    {
        xterm->letter = (xterm->letter << 6u) | (byte & 0x3fu);
        xterm->letter_bytes_left -= 1u;
        if (xterm->letter_bytes_left == 0u)
        {
            EMBED_CALL(xterm_print, XtermCtx, .xterm = xterm, .letter = xterm->letter);
        }
        return;
    }
    if (byte == 0x1bu)
    {
        xterm->state = XTERM_ESCAPE;
        return;
    }
    if ((byte < 0x20u) || (byte == 0x7fu))
    {
        EMBED_CALL(xterm_control_letter, XtermCtx, .xterm = xterm, .byte = byte);
        return;
    }
    const unsigned int left = ((byte & 0xe0u) == 0xc0u) ? 1u : (((byte & 0xf0u) == 0xe0u) ? 2u : 3u);
    const int leads = (byte >= 0xc2u) && (byte <= 0xf4u);
    xterm->letter = byte & ((left == 1u) ? 0x1fu : ((left == 2u) ? 0x0fu : 0x07u));
    xterm->letter_bytes_left = leads ? left : 0u;
    if (byte < 0x80u)
    {
        EMBED_CALL(xterm_print, XtermCtx, .xterm = xterm, .letter = byte);
    }
    else if (!leads)
    {
        EMBED_CALL(xterm_print, XtermCtx, .xterm = xterm, .letter = XTERM_REPLACEMENT);
    }
}

// a byte read in a control sequence, a string or after an escape
EMBED_INLINE void xterm_sequence(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned char byte = args->byte;
    switch (xterm->state)
    {
    case XTERM_ESCAPE:
        EMBED_CALL(xterm_escape, XtermCtx, .xterm = xterm, .byte = byte);
        break;
    case XTERM_CONTROL:
        xterm->state = ((byte >= 0x40u) && (byte <= 0x7eu)) ? XTERM_GROUND
                       : (byte == 0x1bu)                    ? XTERM_ESCAPE
                                                            : XTERM_CONTROL;
        if ((byte >= 0x40u) && (byte <= 0x7eu))
        {
            EMBED_CALL(xterm_control, XtermCtx, .xterm = xterm, .byte = byte);
        }
        else if (byte >= 0x20u)
        {
            EMBED_CALL(xterm_command_put, XtermCtx, .xterm = xterm, .byte = byte);
        }
        else if (byte != 0x1bu)
        {
            EMBED_CALL(xterm_control_letter, XtermCtx, .xterm = xterm, .byte = byte);
        }
        break;
    case XTERM_COMMAND:
    case XTERM_DEVICE:
        if ((byte == 0x07u) && (xterm->state == XTERM_COMMAND))
        {
            xterm->state = XTERM_GROUND;
            EMBED_CALL(xterm_command, XtermCtx, .xterm = xterm);
        }
        else if (byte == 0x1bu)
        {
            xterm->ending = xterm->state;
            xterm->state = XTERM_STRING_END;
        }
        else if (xterm->state == XTERM_COMMAND)
        {
            EMBED_CALL(xterm_command_put, XtermCtx, .xterm = xterm, .byte = byte);
        }
        break;
    case XTERM_STRING_END:
        xterm->state = XTERM_GROUND;
        if ((byte == (unsigned char)'\\') && (xterm->ending == XTERM_COMMAND))
        {
            EMBED_CALL(xterm_command, XtermCtx, .xterm = xterm);
        }
        if (byte != (unsigned char)'\\')
        {
            EMBED_CALL(xterm_escape, XtermCtx, .xterm = xterm, .byte = byte);
        }
        xterm->ending = XTERM_GROUND;
        break;
    default:
        xterm->state = XTERM_GROUND;
        break;
    }
}

// how many of the `size` bytes at `bytes` from the start are printable ASCII, a word of them at a time, the bytes of
// a word that holds another read one by one
EMBED_INLINE unsigned long long xterm_plain_run(const XtermCtx *args)
{
    const unsigned long long ones = 0x0101010101010101ULL;
    const unsigned long long highs = 0x8080808080808080ULL;
    unsigned long long at = 0ULL;
    while ((at + 8ULL) <= args->size)
    {
        unsigned long long word = 0ULL;
        memor.cpy(&(MemoriaCfg){.dst = &word, .src = args->bytes + at, .bytes = 8u});
        const unsigned long long rubout = word ^ (ones * 0x7fULL);
        const unsigned long long flagged =
            ((word - (ones * 0x20ULL)) & ~word & highs) | ((rubout - ones) & ~rubout & highs) | (word & highs);
        if (flagged != 0ULL)
        {
            break;
        }
        at += 8ULL;
    }
    while ((at < args->size) && (args->bytes[at] >= 0x20u) && (args->bytes[at] < 0x7fu))
    {
        at += 1ULL;
    }
    return at;
}

void cli_terminal_xterm_write(const CliTerminalXtermBytesCfg *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const int plain = (xterm->state == XTERM_GROUND) && (xterm->letter_bytes_left == 0u);
        const unsigned long long run =
            plain ? EMBED_CALL(xterm_plain_run, XtermCtx, .bytes = args->bytes + at, .size = args->size - at) : 0ULL;
        for (unsigned long long letter = 0ULL; letter < run; letter += 1ULL)
        {
            EMBED_CALL(xterm_print, XtermCtx, .xterm = xterm, .letter = args->bytes[at + letter]);
        }
        at += run;
        if (at >= args->size)
        {
            break;
        }
        if (xterm->state == XTERM_GROUND)
        {
            EMBED_CALL(xterm_ground, XtermCtx, .xterm = xterm, .byte = args->bytes[at]);
        }
        else if (xterm->state == XTERM_CHARSET)
        {
            xterm->state = XTERM_GROUND;
        }
        else
        {
            EMBED_CALL(xterm_sequence, XtermCtx, .xterm = xterm, .byte = args->bytes[at]);
        }
        at += 1ULL;
    }
}

// the bytes of an xterm's cell for `columns` by `rows`: the two screens' cells, then the marks of the rows changed,
// the command read, the answer and the place
EMBED_INLINE unsigned long long xterm_cell_bytes(const XtermCtx *args)
{
    const unsigned long long cells = (unsigned long long)args->columns * args->rows;
    return (2ULL * cells * sizeof(CliTerminalCell)) + args->rows + CLI_TERMINAL_COMMAND_MAX + CLI_TERMINAL_ANSWER_MAX +
           CLI_TERMINAL_PLACE_MAX;
}

// the xterm's parts laid out in `cell`, sized for `columns` by `rows`
EMBED_INLINE void xterm_lay(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    unsigned char *const cell = (unsigned char *)args->bytes;
    const unsigned long long cells = (unsigned long long)args->columns * args->rows;
    const int second_shown = (xterm->shown != NULL) && (xterm->shown == xterm->second);
    xterm->cell = cell;
    xterm->main = (CliTerminalCell *)(void *)cell;
    xterm->second = xterm->main + cells;
    xterm->shown = second_shown ? xterm->second : xterm->main;
    xterm->changed = (unsigned char *)(void *)(xterm->second + cells);
    xterm->command = xterm->changed + args->rows;
    xterm->answer = xterm->command + CLI_TERMINAL_COMMAND_MAX;
    xterm->place = (char *)(xterm->answer + CLI_TERMINAL_ANSWER_MAX);
    xterm->columns = args->columns;
    xterm->rows = args->rows;
}

int cli_terminal_xterm_init(const CliTerminalXtermSizeCfg *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned int columns = (args->columns > 1u) ? args->columns : 2u;
    const unsigned int rows = (args->rows > 1u) ? args->rows : 2u;
    // what takes the history stays as the caller put it
    const CliTerminalXtermHistory history = xterm->history;
    memor.set(&(MemoriaCfg){.dst = xterm, .val = 0, .bytes = sizeof(*xterm)});
    xterm->history = history;
    const unsigned long long bytes = EMBED_CALL(xterm_cell_bytes, XtermCtx, .columns = columns, .rows = rows);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_terminal_cells);
    if (cell == NULL)
    {
        return 0;
    }
    EMBED_CALL(xterm_lay, XtermCtx, .xterm = xterm, .bytes = cell, .columns = columns, .rows = rows);
    EMBED_CALL(xterm_reset, XtermCtx, .xterm = xterm);
    EMBED_CALL(xterm_lines_erase, XtermCtx, .xterm = xterm, .from = 0u, .to = rows);
    xterm->shown = xterm->second;
    EMBED_CALL(xterm_lines_erase, XtermCtx, .xterm = xterm, .from = 0u, .to = rows);
    xterm->shown = xterm->main;
    return 1;
}

// the rows of one of the xterm's two screens, `from` the old cells, copied into `to` the new, each row cut or padded
// to the new width; the main screen's lines from its top go to the history while the cursor stands below its new foot,
// where it is the one drawn on, and the rest from its foot are let go
EMBED_INLINE void xterm_copy(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const CliTerminalCell *const from = args->cells;
    CliTerminalCell *const to = (CliTerminalCell *)args->bytes;
    const unsigned int old_columns = xterm->columns;
    const unsigned int old_rows = xterm->rows;
    const int drawn = (from == xterm->shown);
    unsigned int dropped = 0u;
    while (drawn && ((old_rows - dropped) > args->rows) && ((xterm->cursor.y - dropped) >= args->rows))
    {
        if (from == xterm->main)
        {
            EMBED_CALL(xterm_history_put, XtermCtx, .xterm = xterm,
                       .cells = from + ((unsigned long long)dropped * old_columns), .columns = old_columns);
        }
        dropped += 1u;
    }
    const CliTerminalCell blank = {(unsigned int)' ', CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u};
    for (unsigned int y = 0u; y < args->rows; y += 1u)
    {
        const unsigned int old_y = y + dropped;
        for (unsigned int x = 0u; x < args->columns; x += 1u)
        {
            const int present = (old_y < old_rows) && (x < old_columns);
            to[((unsigned long long)y * args->columns) + x] =
                present ? from[((unsigned long long)old_y * old_columns) + x] : blank;
        }
    }
    xterm->cursor.y -= drawn ? dropped : 0u;
}

int cli_terminal_xterm_resize(const CliTerminalXtermSizeCfg *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned int columns = (args->columns > 1u) ? args->columns : 2u;
    const unsigned int rows = (args->rows > 1u) ? args->rows : 2u;
    if ((columns == xterm->columns) && (rows == xterm->rows))
    {
        return 1;
    }
    const unsigned long long bytes = EMBED_CALL(xterm_cell_bytes, XtermCtx, .columns = columns, .rows = rows);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_terminal_cells);
    if (cell == NULL)
    {
        return 0;
    }
    const unsigned long long cells = (unsigned long long)columns * rows;
    CliTerminalCell *const main = (CliTerminalCell *)(void *)cell;
    EMBED_CALL(xterm_copy, XtermCtx, .xterm = xterm, .cells = xterm->main, .bytes = (const unsigned char *)main,
               .columns = columns, .rows = rows);
    EMBED_CALL(xterm_copy, XtermCtx, .xterm = xterm, .cells = xterm->second,
               .bytes = (const unsigned char *)(main + cells), .columns = columns, .rows = rows);
    unsigned char *const old = xterm->cell;
    const unsigned int command_size = xterm->command_size;
    const unsigned int answer_size = xterm->answer_size;
    const unsigned char *const old_command = xterm->command;
    const unsigned char *const old_answer = xterm->answer;
    const char *const old_place = xterm->place;
    EMBED_CALL(xterm_lay, XtermCtx, .xterm = xterm, .bytes = cell, .columns = columns, .rows = rows);
    memor.cpy(&(MemoriaCfg){.dst = xterm->command, .src = old_command, .bytes = command_size});
    memor.cpy(&(MemoriaCfg){.dst = xterm->answer, .src = old_answer, .bytes = answer_size});
    memor.cpy(&(MemoriaCfg){.dst = xterm->place, .src = old_place, .bytes = CLI_TERMINAL_PLACE_MAX});
    g_cli_cells->persistent_buf_release(old, &g_cli_terminal_cells);
    xterm->top = 0u;
    xterm->bottom = rows - 1u;
    xterm->cursor.x = (xterm->cursor.x < columns) ? xterm->cursor.x : (columns - 1u);
    xterm->cursor.y = (xterm->cursor.y < rows) ? xterm->cursor.y : (rows - 1u);
    xterm->cursor.wrap_pending = 0;
    EMBED_CALL(xterm_touch_all, XtermCtx, .xterm = xterm);
    return 1;
}

void cli_terminal_xterm_free(const CliTerminalXtermCfg *args)
{
    g_cli_cells->persistent_buf_release(args->xterm->cell, &g_cli_terminal_cells);
    args->xterm->cell = NULL;
}
