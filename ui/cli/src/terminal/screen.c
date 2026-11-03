// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// screen.c: a terminal's screen read from a shell's output as an xterm reads it (screen.h). The output's plain runs
// are found a word at a time, each letter of a word with a control byte, an escape or a byte past ASCII in it read
// alone; a letter past ASCII is read from its UTF-8 bytes, an ill-formed one read as U+FFFD
#include "screen.h"
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"

// where the reading of the output stands: in plain text, after an escape, in a control sequence, in an
// operating-system command, in a device control string, after the escape that may end a string, or before the one
// byte a character set's choice takes
#define SCREEN_GROUND 0u
#define SCREEN_ESCAPE 1u
#define SCREEN_CONTROL 2u
#define SCREEN_COMMAND 3u
#define SCREEN_DEVICE 4u
#define SCREEN_STRING_END 5u
#define SCREEN_CHARSET 6u

// the most numbers of a control sequence read, and the number an empty one stands for
#define SCREEN_NUMBERS 32u
#define SCREEN_ABSENT 0xffffffffu

// the tab stops' spacing, and the letter an ill-formed UTF-8 run reads as
#define SCREEN_TAB 8u
#define SCREEN_REPLACEMENT 0xfffdu

// the scrollback's cells for each of its lines on the average
#define SCREEN_SCROLLBACK_WIDTH 96ull

typedef struct
{
    CliTerminalScreen *screen;
    const CliTerminalScreen *screen_read;
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
} ScreenCtx;

// the cells of row `y` of the screen drawn on
EMBED_INLINE CliTerminalCell *screen_row(const ScreenCtx *args)
{
    return args->screen->shown + ((unsigned long long)args->y * args->screen->columns);
}

// a blank cell in the background of the style of the moment, its letter a space
EMBED_INLINE CliTerminalCell screen_blank(const ScreenCtx *args)
{
    const CliTerminalCell blank = {(unsigned int)' ', CLI_TERMINAL_COLOR_DEFAULT, args->screen->cursor.style.background,
                                   0u};
    return blank;
}

// row `y` marked changed
EMBED_INLINE void screen_touch(const ScreenCtx *args)
{
    args->screen->changed[args->y] = 1u;
}

// the cells of row `y` from `from` up to, not including, `to` blank
EMBED_INLINE void screen_cells_erase(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    CliTerminalCell *const row = EMBED_CALL(screen_row, ScreenCtx, .screen = screen, .y = args->y);
    const CliTerminalCell blank = EMBED_CALL(screen_blank, ScreenCtx, .screen = screen);
    const unsigned int to = (args->to < screen->columns) ? args->to : screen->columns;
    for (unsigned int x = args->from; x < to; x += 1u)
    {
        row[x] = blank;
    }
    EMBED_CALL(screen_touch, ScreenCtx, .screen = screen, .y = args->y);
}

// the rows from `from` up to, not including, `to` blank
EMBED_INLINE void screen_lines_erase(const ScreenCtx *args)
{
    const unsigned int to = (args->to < args->screen->rows) ? args->to : args->screen->rows;
    for (unsigned int y = args->from; y < to; y += 1u)
    {
        EMBED_CALL(screen_cells_erase, ScreenCtx, .screen = args->screen, .y = y, .from = 0u, .to = SCREEN_ABSENT);
    }
}

// `columns` cells at `cells` put after the scrollback's last line, their blanks at the end left off; the oldest
// lines let go while the scrollback holds CLI_TERMINAL_SCROLLBACK_LINES or the new line's cells lie over theirs
EMBED_INLINE void screen_scrollback_put(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    unsigned int columns = args->columns;
    while ((columns > 0u) && (args->cells[columns - 1u].letter == (unsigned int)' ') &&
           (args->cells[columns - 1u].background == CLI_TERMINAL_COLOR_DEFAULT) &&
           (args->cells[columns - 1u].marks == 0u))
    {
        columns -= 1u;
    }
    const unsigned long long width =
        (columns < screen->scrollback_cell_count) ? columns : screen->scrollback_cell_count;
    const unsigned long long at =
        ((screen->scrollback_cell_used + width) <= screen->scrollback_cell_count) ? screen->scrollback_cell_used : 0ULL;
    while (screen->scrollback_count > 0u)
    {
        const CliTerminalLine *const oldest = &screen->scrollback_lines[screen->scrollback_first];
        const int lies_over = (width > 0ULL) && (oldest->at < (at + width)) && (at < (oldest->at + oldest->columns));
        if ((screen->scrollback_count < CLI_TERMINAL_SCROLLBACK_LINES) && !lies_over)
        {
            break;
        }
        screen->scrollback_first = (screen->scrollback_first + 1u) % CLI_TERMINAL_SCROLLBACK_LINES;
        screen->scrollback_count -= 1u;
    }
    for (unsigned long long cell = 0ULL; cell < width; cell += 1ULL)
    {
        screen->scrollback_cells[at + cell] = args->cells[cell];
    }
    const unsigned int last = (screen->scrollback_first + screen->scrollback_count) % CLI_TERMINAL_SCROLLBACK_LINES;
    // a line's width is the screen's at the most, far below what an unsigned int counts
    screen->scrollback_lines[last] = (CliTerminalLine){at, (unsigned int)width};
    screen->scrollback_count += 1u;
    screen->scrollback_cell_used = at + width;
}

// the region scrolled up `count` lines; on the main screen, with the region at the screen's top, each line that leaves
// the top goes to the scrollback
EMBED_INLINE void screen_scroll_up(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const unsigned int height = screen->bottom - screen->top + 1u;
    const unsigned int count = (args->count < height) ? args->count : height;
    const unsigned long long columns = screen->columns;
    for (unsigned int step = 0u; step < count; step += 1u)
    {
        CliTerminalCell *const top = EMBED_CALL(screen_row, ScreenCtx, .screen = screen, .y = screen->top);
        if ((screen->shown == screen->main) && (screen->top == 0u))
        {
            EMBED_CALL(screen_scrollback_put, ScreenCtx, .screen = screen, .cells = top, .columns = screen->columns);
        }
        const unsigned long long moved = (unsigned long long)(screen->bottom - screen->top) * columns;
        for (unsigned long long cell = 0ULL; cell < moved; cell += 1ULL)
        {
            top[cell] = top[cell + columns];
        }
        EMBED_CALL(screen_cells_erase, ScreenCtx, .screen = screen, .y = screen->bottom, .from = 0u,
                   .to = SCREEN_ABSENT);
    }
    for (unsigned int y = screen->top; y <= screen->bottom; y += 1u)
    {
        screen->changed[y] = 1u;
    }
}

// the region scrolled down `count` lines, blank lines coming in at its top
EMBED_INLINE void screen_scroll_down(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const unsigned int height = screen->bottom - screen->top + 1u;
    const unsigned int count = (args->count < height) ? args->count : height;
    const unsigned long long columns = screen->columns;
    for (unsigned int step = 0u; step < count; step += 1u)
    {
        CliTerminalCell *const top = EMBED_CALL(screen_row, ScreenCtx, .screen = screen, .y = screen->top);
        for (unsigned long long cell = (unsigned long long)(screen->bottom - screen->top) * columns; cell > 0ULL;
             cell -= 1ULL)
        {
            top[cell - 1ULL + columns] = top[cell - 1ULL];
        }
        EMBED_CALL(screen_cells_erase, ScreenCtx, .screen = screen, .y = screen->top, .from = 0u, .to = SCREEN_ABSENT);
    }
    for (unsigned int y = screen->top; y <= screen->bottom; y += 1u)
    {
        screen->changed[y] = 1u;
    }
}

// the cursor moved to column `x` and row `y`, each bounded by the screen, the next letter no longer wrapping first
EMBED_INLINE void screen_move(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    EMBED_CALL(screen_touch, ScreenCtx, .screen = screen, .y = screen->cursor.y);
    screen->cursor.x = (args->x < screen->columns) ? args->x : (screen->columns - 1u);
    screen->cursor.y = (args->y < screen->rows) ? args->y : (screen->rows - 1u);
    screen->cursor.wrap_pending = 0;
    EMBED_CALL(screen_touch, ScreenCtx, .screen = screen, .y = screen->cursor.y);
}

// a line fed: the region scrolled up at its foot, else the cursor a row down
EMBED_INLINE void screen_line_feed(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    if (screen->cursor.y == screen->bottom)
    {
        EMBED_CALL(screen_scroll_up, ScreenCtx, .screen = screen, .count = 1u);
        return;
    }
    if ((screen->cursor.y + 1u) < screen->rows)
    {
        EMBED_CALL(screen_move, ScreenCtx, .screen = screen, .x = screen->cursor.x, .y = screen->cursor.y + 1u);
    }
}

// the cursor a row up: the region scrolled down at its top, else the cursor a row up
EMBED_INLINE void screen_reverse_index(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    if (screen->cursor.y == screen->top)
    {
        EMBED_CALL(screen_scroll_down, ScreenCtx, .screen = screen, .count = 1u);
        return;
    }
    if (screen->cursor.y > 0u)
    {
        EMBED_CALL(screen_move, ScreenCtx, .screen = screen, .x = screen->cursor.x, .y = screen->cursor.y - 1u);
    }
}

// the letter `letter` printed at the cursor in the style of the moment, a line fed first where the last printed stood
// at the last column; in insert mode the cells from the cursor on move a cell right first
EMBED_INLINE void screen_print(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    if (screen->cursor.wrap_pending && screen->wraps)
    {
        screen->cursor.wrap_pending = 0;
        screen->cursor.x = 0u;
        EMBED_CALL(screen_line_feed, ScreenCtx, .screen = screen);
    }
    CliTerminalCell *const row = EMBED_CALL(screen_row, ScreenCtx, .screen = screen, .y = screen->cursor.y);
    for (unsigned int x = screen->columns - 1u; screen->inserts && (x > screen->cursor.x); x -= 1u)
    {
        row[x] = row[x - 1u];
    }
    row[screen->cursor.x] = (CliTerminalCell){args->letter, screen->cursor.style.foreground,
                                              screen->cursor.style.background, screen->cursor.style.marks};
    EMBED_CALL(screen_touch, ScreenCtx, .screen = screen, .y = screen->cursor.y);
    screen->cursor.wrap_pending = (screen->cursor.x + 1u) == screen->columns;
    screen->cursor.x += screen->cursor.wrap_pending ? 0u : 1u;
}

// the cursor saved, or the one saved restored, a screen with none saved restoring the home and the plain style
EMBED_INLINE void screen_save(const ScreenCtx *args)
{
    args->screen->saved = args->screen->cursor;
}

EMBED_INLINE void screen_restore(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const CliTerminalCursor saved = screen->saved;
    EMBED_CALL(screen_move, ScreenCtx, .screen = screen, .x = saved.x, .y = saved.y);
    screen->cursor.style = saved.style;
    screen->cursor.wrap_pending = saved.wrap_pending;
}

// every row marked changed
EMBED_INLINE void screen_touch_all(const ScreenCtx *args)
{
    for (unsigned int y = 0u; y < args->screen->rows; y += 1u)
    {
        args->screen->changed[y] = 1u;
    }
}

// the second screen drawn on, blank, the cursor saved and the region the whole screen; and the main screen drawn on
// again, the cursor restored
EMBED_INLINE void screen_second_enter(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    if (screen->shown != screen->main)
    {
        return;
    }
    EMBED_CALL(screen_save, ScreenCtx, .screen = screen);
    const unsigned long long cells = (unsigned long long)screen->columns * screen->rows;
    const CliTerminalCell blank = {(unsigned int)' ', CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u};
    for (unsigned long long cell = 0ULL; cell < cells; cell += 1ULL)
    {
        screen->second[cell] = blank;
    }
    screen->shown = screen->second;
    screen->top = 0u;
    screen->bottom = screen->rows - 1u;
    EMBED_CALL(screen_touch_all, ScreenCtx, .screen = screen);
}

EMBED_INLINE void screen_second_leave(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    if (screen->shown == screen->main)
    {
        return;
    }
    screen->shown = screen->main;
    screen->top = 0u;
    screen->bottom = screen->rows - 1u;
    EMBED_CALL(screen_touch_all, ScreenCtx, .screen = screen);
    EMBED_CALL(screen_restore, ScreenCtx, .screen = screen);
}

// the cursor home, the style plain, the region the whole screen, nothing saved and every mode as a terminal starts
EMBED_INLINE void screen_reset(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const CliTerminalCursor home = {0u, 0u, {CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u}, 0};
    screen->cursor = home;
    screen->saved = home;
    screen->top = 0u;
    screen->bottom = screen->rows - 1u;
    screen->application_cursor = 0;
    screen->wraps = 1;
    screen->cursor_shown = 1;
    screen->bracketed_paste = 0;
    screen->inserts = 0;
}

// `text`, a C string, put after the answer for the shell, where it fits whole
EMBED_INLINE void screen_answer(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    VerbaLinea line = {(char *)screen->answer + screen->answer_size, CLI_TERMINAL_ANSWER_MAX - screen->answer_size,
                       0ULL};
    mmgr_line_put(&line, args->text);
    // the answer is far below CLI_TERMINAL_ANSWER_MAX, which an unsigned int counts
    screen->answer_size += mmgr_line_ok(&line) ? (unsigned int)line.at : 0u;
}

// the answer to where the cursor is: ESC [ row ; column R
EMBED_INLINE void screen_answer_place(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    VerbaLinea line = {(char *)screen->answer + screen->answer_size, CLI_TERMINAL_ANSWER_MAX - screen->answer_size,
                       0ULL};
    mmgr_line_put(&line, "\x1b[");
    mmgr_line_decimal(&line, (unsigned long long)screen->cursor.y + 1ULL, 1u);
    mmgr_line_ch(&line, ';');
    mmgr_line_decimal(&line, (unsigned long long)screen->cursor.x + 1ULL, 1u);
    mmgr_line_ch(&line, 'R');
    // the answer is far below CLI_TERMINAL_ANSWER_MAX, which an unsigned int counts
    screen->answer_size += mmgr_line_ok(&line) ? (unsigned int)line.at : 0u;
}

// the number at `count` of the sequence's `number_count` numbers, else `value` where it is past them or empty
EMBED_INLINE unsigned int screen_number(const ScreenCtx *args)
{
    const int present = (args->count < args->number_count) && (args->numbers[args->count] != SCREEN_ABSENT);
    // the number read in its place is an unsigned int's
    return present ? args->numbers[args->count] : (unsigned int)args->value;
}

// a red, a green and a blue each read as at most 255, put together as a direct color
EMBED_INLINE unsigned int screen_direct(const ScreenCtx *args)
{
    unsigned int color = CLI_TERMINAL_COLOR_DIRECT;
    for (unsigned int part = 0u; part < 3u; part += 1u)
    {
        const unsigned int value =
            EMBED_CALL(screen_number, ScreenCtx, .numbers = args->numbers, .number_count = args->number_count,
                       .count = args->from + part, .value = 0ULL);
        color |= ((value < 255u) ? value : 255u) << (16u - (8u * part));
    }
    return color;
}

// the style of the moment changed by a Select Graphic Rendition's numbers
EMBED_INLINE void screen_rendition(const ScreenCtx *args)
{
    CliTerminalStyle *const style = &args->screen->cursor.style;
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
        const unsigned int value = (numbers[at] == SCREEN_ABSENT) ? 0u : numbers[at];
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
        const unsigned int kind = EMBED_CALL(screen_number, ScreenCtx, .numbers = numbers, .number_count = count,
                                             .count = at + 1u, .value = 0ULL);
        unsigned int color = (value == 38u) ? style->foreground : style->background;
        if (kind == 5u)
        {
            color = CLI_TERMINAL_COLOR_INDEXED | (EMBED_CALL(screen_number, ScreenCtx, .numbers = numbers,
                                                             .number_count = count, .count = at + 2u, .value = 0ULL) &
                                                  0xffu);
            at += 2u;
        }
        if (kind == 2u)
        {
            // an empty color space may stand before the three parts, as 38:2::r:g:b
            const unsigned int from = (((at + 2u) < count) && (numbers[at + 2u] == SCREEN_ABSENT)) ? at + 3u : at + 2u;
            color = EMBED_CALL(screen_direct, ScreenCtx, .numbers = numbers, .number_count = count, .from = from);
            at = from + 2u;
        }
        style->foreground = (value == 38u) ? color : style->foreground;
        style->background = (value == 48u) ? color : style->background;
    }
}

// a private mode set or reset: the cursor keys' application mode, wrapping, the cursor shown, the second screen and
// bracketed paste
EMBED_INLINE void screen_private_mode(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    switch (args->mode)
    {
    case 1u:
        screen->application_cursor = args->on;
        break;
    case 7u:
        screen->wraps = args->on;
        break;
    case 25u:
        screen->cursor_shown = args->on;
        EMBED_CALL(screen_touch, ScreenCtx, .screen = screen, .y = screen->cursor.y);
        break;
    case 47u:
    case 1047u:
    case 1049u:
        if (!args->on)
        {
            EMBED_CALL(screen_second_leave, ScreenCtx, .screen = screen);
            break;
        }
        EMBED_CALL(screen_second_enter, ScreenCtx, .screen = screen);
        if (args->mode != 1049u)
        {
            EMBED_CALL(screen_lines_erase, ScreenCtx, .screen = screen, .from = 0u, .to = screen->rows);
        }
        break;
    case 2004u:
        screen->bracketed_paste = args->on;
        break;
    default:
        break;
    }
}

// the display erased: from the cursor to its end, from its start to the cursor, whole, or the scrollback emptied
EMBED_INLINE void screen_display_erase(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const CliTerminalCursor at = screen->cursor;
    switch (args->mode)
    {
    case 0u:
        EMBED_CALL(screen_cells_erase, ScreenCtx, .screen = screen, .y = at.y, .from = at.x, .to = SCREEN_ABSENT);
        EMBED_CALL(screen_lines_erase, ScreenCtx, .screen = screen, .from = at.y + 1u, .to = screen->rows);
        break;
    case 1u:
        EMBED_CALL(screen_lines_erase, ScreenCtx, .screen = screen, .from = 0u, .to = at.y);
        EMBED_CALL(screen_cells_erase, ScreenCtx, .screen = screen, .y = at.y, .from = 0u, .to = at.x + 1u);
        break;
    case 2u:
        EMBED_CALL(screen_lines_erase, ScreenCtx, .screen = screen, .from = 0u, .to = screen->rows);
        break;
    case 3u:
        screen->scrollback_first = 0u;
        screen->scrollback_count = 0u;
        screen->scrollback_cell_used = 0ULL;
        break;
    default:
        break;
    }
}

// `count` lines inserted at the cursor's row, or deleted from it, inside the region, the cursor to the row's start
EMBED_INLINE void screen_lines_insert(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    if ((screen->cursor.y < screen->top) || (screen->cursor.y > screen->bottom))
    {
        return;
    }
    const unsigned int height = screen->bottom - screen->cursor.y + 1u;
    const unsigned int count = (args->count < height) ? args->count : height;
    const unsigned long long columns = screen->columns;
    CliTerminalCell *const first = EMBED_CALL(screen_row, ScreenCtx, .screen = screen, .y = screen->cursor.y);
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
    const unsigned int blank_from = args->on ? screen->cursor.y : (screen->bottom + 1u - count);
    EMBED_CALL(screen_lines_erase, ScreenCtx, .screen = screen, .from = blank_from, .to = blank_from + count);
    for (unsigned int y = screen->cursor.y; y <= screen->bottom; y += 1u)
    {
        screen->changed[y] = 1u;
    }
    EMBED_CALL(screen_move, ScreenCtx, .screen = screen, .x = 0u, .y = screen->cursor.y);
}

// `count` cells at the cursor: blanks inserted, the rest moving right, or cells deleted, the rest moving left
EMBED_INLINE void screen_cells_insert(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    CliTerminalCell *const row = EMBED_CALL(screen_row, ScreenCtx, .screen = screen, .y = screen->cursor.y);
    const unsigned int left = screen->columns - screen->cursor.x;
    const unsigned int count = (args->count < left) ? args->count : left;
    const CliTerminalCell blank = EMBED_CALL(screen_blank, ScreenCtx, .screen = screen);
    if (args->on)
    {
        for (unsigned int x = screen->columns; x > (screen->cursor.x + count); x -= 1u)
        {
            row[x - 1u] = row[x - 1u - count];
        }
        for (unsigned int x = screen->cursor.x; x < (screen->cursor.x + count); x += 1u)
        {
            row[x] = blank;
        }
    }
    else
    {
        for (unsigned int x = screen->cursor.x; (x + count) < screen->columns; x += 1u)
        {
            row[x] = row[x + count];
        }
        for (unsigned int x = screen->columns - count; x < screen->columns; x += 1u)
        {
            row[x] = blank;
        }
    }
    EMBED_CALL(screen_touch, ScreenCtx, .screen = screen, .y = screen->cursor.y);
}

// the region set from its two numbers, the whole screen where its bottom is not below its top, the cursor home
EMBED_INLINE void screen_region(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const unsigned int top = EMBED_CALL(screen_number, ScreenCtx, .numbers = args->numbers,
                                        .number_count = args->number_count, .count = 0u, .value = 1ULL);
    const unsigned int bottom = EMBED_CALL(screen_number, ScreenCtx, .numbers = args->numbers,
                                           .number_count = args->number_count, .count = 1u, .value = screen->rows);
    screen->top = (top > 0u) ? top - 1u : 0u;
    screen->bottom = ((bottom > 0u) && (bottom < screen->rows)) ? bottom - 1u : screen->rows - 1u;
    if (screen->bottom <= screen->top)
    {
        screen->top = 0u;
        screen->bottom = screen->rows - 1u;
    }
    EMBED_CALL(screen_move, ScreenCtx, .screen = screen, .x = 0u, .y = 0u);
}

// a control sequence's numbers read from the `size` bytes of its body at `bytes`, past a leading mark, into
// `numbers`, each empty one SCREEN_ABSENT: their count
EMBED_INLINE unsigned int screen_numbers_read(const ScreenCtx *args)
{
    unsigned int *const numbers = args->numbers_out;
    unsigned int count = 1u;
    numbers[0] = SCREEN_ABSENT;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        const unsigned char byte = args->bytes[at];
        const int digit = (byte >= (unsigned char)'0') && (byte <= (unsigned char)'9');
        const int cut = ((byte == (unsigned char)';') || (byte == (unsigned char)':')) && (count < SCREEN_NUMBERS);
        const unsigned int was = (numbers[count - 1u] == SCREEN_ABSENT) ? 0u : numbers[count - 1u];
        if (digit)
        {
            // a number of more than nine digits stands at its first nine, far below what an unsigned int counts
            numbers[count - 1u] = (was < 100000000u) ? ((was * 10u) + (unsigned int)(byte - (unsigned char)'0')) : was;
        }
        else if (cut)
        {
            numbers[count] = SCREEN_ABSENT;
            count += 1u;
        }
    }
    return count;
}

// a control sequence's cursor moves, `final` its last byte in `byte` and its first number `count`, at least 1
EMBED_INLINE int screen_control_move(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const CliTerminalCursor at = screen->cursor;
    const unsigned int count = args->count;
    unsigned int x = at.x;
    unsigned int y = at.y;
    const unsigned int floor = (at.y >= screen->top) ? screen->top : 0u;
    const unsigned int ceiling = (at.y <= screen->bottom) ? screen->bottom : (screen->rows - 1u);
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
        const unsigned int column = EMBED_CALL(screen_number, ScreenCtx, .numbers = args->numbers,
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
    EMBED_CALL(screen_move, ScreenCtx, .screen = screen, .x = x, .y = y);
    return 1;
}

// a control sequence read, its body the command read and its last byte `byte`; one with bytes between its numbers and
// its last byte is read past
EMBED_INLINE void screen_control(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const unsigned char *const body = screen->command;
    const unsigned int size = screen->command_size;
    const unsigned char lead = (size > 0u) ? body[0] : 0u;
    const int marked = (lead == (unsigned char)'?') || (lead == (unsigned char)'>') || (lead == (unsigned char)'=') ||
                       (lead == (unsigned char)'!');
    int between = 0;
    for (unsigned int at = marked ? 1u : 0u; at < size; at += 1u)
    {
        between |= !(((body[at] >= (unsigned char)'0') && (body[at] <= (unsigned char)'9')) ||
                     (body[at] == (unsigned char)';') || (body[at] == (unsigned char)':'));
    }
    unsigned int numbers[SCREEN_NUMBERS];
    const unsigned int count = EMBED_CALL(screen_numbers_read, ScreenCtx, .numbers_out = numbers,
                                          .bytes = body + (marked ? 1u : 0u), .size = size - (marked ? 1u : 0u));
    const unsigned int first =
        EMBED_CALL(screen_number, ScreenCtx, .numbers = numbers, .number_count = count, .count = 0u, .value = 1ULL);
    const unsigned int times = (first > 0u) ? first : 1u;
    const unsigned int how =
        EMBED_CALL(screen_number, ScreenCtx, .numbers = numbers, .number_count = count, .count = 0u, .value = 0ULL);
    if (lead == (unsigned char)'?')
    {
        for (unsigned int at = 0u; ((args->byte == 'h') || (args->byte == 'l')) && (at < count); at += 1u)
        {
            EMBED_CALL(screen_private_mode, ScreenCtx, .screen = screen, .mode = numbers[at], .on = args->byte == 'h');
        }
        return;
    }
    if ((lead == (unsigned char)'>') || (lead == (unsigned char)'='))
    {
        if (args->byte == 'c')
        {
            EMBED_CALL(screen_answer, ScreenCtx, .screen = screen, .text = "\x1b[>0;0;0c");
        }
        return;
    }
    if (between || EMBED_CALL(screen_control_move, ScreenCtx, .screen = screen, .byte = args->byte, .count = times,
                              .numbers = numbers, .number_count = count))
    {
        return;
    }
    const CliTerminalCursor at = screen->cursor;
    switch (args->byte)
    {
    case '@':
    case 'P':
        EMBED_CALL(screen_cells_insert, ScreenCtx, .screen = screen, .count = times, .on = args->byte == '@');
        break;
    case 'J':
        EMBED_CALL(screen_display_erase, ScreenCtx, .screen = screen, .mode = how);
        break;
    case 'K':
        EMBED_CALL(screen_cells_erase, ScreenCtx, .screen = screen, .y = at.y, .from = (how == 0u) ? at.x : 0u,
                   .to = (how == 1u) ? (at.x + 1u) : SCREEN_ABSENT);
        break;
    case 'L':
    case 'M':
        EMBED_CALL(screen_lines_insert, ScreenCtx, .screen = screen, .count = times, .on = args->byte == 'L');
        break;
    case 'X':
        EMBED_CALL(screen_cells_erase, ScreenCtx, .screen = screen, .y = at.y, .from = at.x, .to = at.x + times);
        break;
    case 'S':
        EMBED_CALL(screen_scroll_up, ScreenCtx, .screen = screen, .count = times);
        break;
    case 'T':
        EMBED_CALL(screen_scroll_down, ScreenCtx, .screen = screen, .count = times);
        break;
    case 'm':
        EMBED_CALL(screen_rendition, ScreenCtx, .screen = screen, .numbers = numbers, .number_count = count);
        break;
    case 'r':
        EMBED_CALL(screen_region, ScreenCtx, .screen = screen, .numbers = numbers, .number_count = count);
        break;
    case 's':
        EMBED_CALL(screen_save, ScreenCtx, .screen = screen);
        break;
    case 'u':
        EMBED_CALL(screen_restore, ScreenCtx, .screen = screen);
        break;
    case 'h':
    case 'l':
        screen->inserts = (how == 4u) ? (args->byte == 'h') : screen->inserts;
        break;
    case 'n':
        if (how == 6u)
        {
            EMBED_CALL(screen_answer_place, ScreenCtx, .screen = screen);
        }
        if (how == 5u)
        {
            EMBED_CALL(screen_answer, ScreenCtx, .screen = screen, .text = "\x1b[0n");
        }
        break;
    case 'c':
        EMBED_CALL(screen_answer, ScreenCtx, .screen = screen, .text = "\x1b[?1;2c");
        break;
    default:
        break;
    }
}

// an operating-system command read: OSC 7, the folder the shell stands in as a file URL, becomes the place reported
EMBED_INLINE void screen_command(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const unsigned char *const body = screen->command;
    if ((screen->command_size < 2u) || (body[0] != (unsigned char)'7') || (body[1] != (unsigned char)';'))
    {
        return;
    }
    const unsigned int size = screen->command_size - 2u;
    const unsigned int taken = (size < (CLI_TERMINAL_PLACE_MAX - 1u)) ? size : (CLI_TERMINAL_PLACE_MAX - 1u);
    memor.cpy(&(MemoriaCfg){.dst = screen->place, .src = body + 2, .bytes = taken});
    screen->place[taken] = '\0';
    screen->place_reported = 1;
}

// a control letter: the bell read past, backspace, a tab to the next stop, a line fed, and a carriage return
EMBED_INLINE void screen_control_letter(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const CliTerminalCursor at = screen->cursor;
    switch (args->byte)
    {
    case 0x08u:
        EMBED_CALL(screen_move, ScreenCtx, .screen = screen, .x = (at.x > 0u) ? (at.x - 1u) : 0u, .y = at.y);
        break;
    case 0x09u:
        EMBED_CALL(screen_move, ScreenCtx, .screen = screen, .x = ((at.x / SCREEN_TAB) + 1u) * SCREEN_TAB, .y = at.y);
        break;
    case 0x0au:
    case 0x0bu:
    case 0x0cu:
        EMBED_CALL(screen_line_feed, ScreenCtx, .screen = screen);
        break;
    case 0x0du:
        EMBED_CALL(screen_move, ScreenCtx, .screen = screen, .x = 0u, .y = at.y);
        break;
    default:
        break;
    }
}

// the byte after an escape: a control sequence, an operating-system command, a device control string or a character
// set's choice begun; the cursor saved or restored; an index, a next line, a reverse index or a reset
EMBED_INLINE void screen_escape(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    screen->state = SCREEN_GROUND;
    screen->command_size = 0u;
    switch (args->byte)
    {
    case '[':
        screen->state = SCREEN_CONTROL;
        break;
    case ']':
        screen->state = SCREEN_COMMAND;
        break;
    case 'P':
        screen->state = SCREEN_DEVICE;
        break;
    case '(':
    case ')':
    case '*':
    case '+':
        screen->state = SCREEN_CHARSET;
        break;
    case '7':
        EMBED_CALL(screen_save, ScreenCtx, .screen = screen);
        break;
    case '8':
        EMBED_CALL(screen_restore, ScreenCtx, .screen = screen);
        break;
    case 'D':
        EMBED_CALL(screen_line_feed, ScreenCtx, .screen = screen);
        break;
    case 'E':
        EMBED_CALL(screen_move, ScreenCtx, .screen = screen, .x = 0u, .y = screen->cursor.y);
        EMBED_CALL(screen_line_feed, ScreenCtx, .screen = screen);
        break;
    case 'M':
        EMBED_CALL(screen_reverse_index, ScreenCtx, .screen = screen);
        break;
    case 'c':
        EMBED_CALL(screen_second_leave, ScreenCtx, .screen = screen);
        EMBED_CALL(screen_reset, ScreenCtx, .screen = screen);
        EMBED_CALL(screen_lines_erase, ScreenCtx, .screen = screen, .from = 0u, .to = screen->rows);
        break;
    default:
        break;
    }
}

// a byte put after the command being read, where there is space for it
EMBED_INLINE void screen_command_put(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    if (screen->command_size < CLI_TERMINAL_COMMAND_MAX)
    {
        screen->command[screen->command_size] = args->byte;
        screen->command_size += 1u;
    }
}

// a byte read in plain text: an escape begun, a control letter, or a letter's byte, a letter printed once whole
EMBED_INLINE void screen_ground(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const unsigned char byte = args->byte;
    const int continues = (byte & 0xc0u) == 0x80u;
    if ((screen->letter_bytes_left > 0u) && !continues)
    {
        // a letter cut before its end reads as the replacement, and the byte after it is read afresh
        screen->letter_bytes_left = 0u;
        EMBED_CALL(screen_print, ScreenCtx, .screen = screen, .letter = SCREEN_REPLACEMENT);
    }
    if (screen->letter_bytes_left > 0u)
    {
        screen->letter = (screen->letter << 6u) | (byte & 0x3fu);
        screen->letter_bytes_left -= 1u;
        if (screen->letter_bytes_left == 0u)
        {
            EMBED_CALL(screen_print, ScreenCtx, .screen = screen, .letter = screen->letter);
        }
        return;
    }
    if (byte == 0x1bu)
    {
        screen->state = SCREEN_ESCAPE;
        return;
    }
    if ((byte < 0x20u) || (byte == 0x7fu))
    {
        EMBED_CALL(screen_control_letter, ScreenCtx, .screen = screen, .byte = byte);
        return;
    }
    const unsigned int left = ((byte & 0xe0u) == 0xc0u) ? 1u : (((byte & 0xf0u) == 0xe0u) ? 2u : 3u);
    const int leads = (byte >= 0xc2u) && (byte <= 0xf4u);
    screen->letter = byte & ((left == 1u) ? 0x1fu : ((left == 2u) ? 0x0fu : 0x07u));
    screen->letter_bytes_left = leads ? left : 0u;
    if (byte < 0x80u)
    {
        EMBED_CALL(screen_print, ScreenCtx, .screen = screen, .letter = byte);
    }
    else if (!leads)
    {
        EMBED_CALL(screen_print, ScreenCtx, .screen = screen, .letter = SCREEN_REPLACEMENT);
    }
}

// a byte read in a control sequence, a string or after an escape
EMBED_INLINE void screen_sequence(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const unsigned char byte = args->byte;
    switch (screen->state)
    {
    case SCREEN_ESCAPE:
        EMBED_CALL(screen_escape, ScreenCtx, .screen = screen, .byte = byte);
        break;
    case SCREEN_CONTROL:
        screen->state = ((byte >= 0x40u) && (byte <= 0x7eu)) ? SCREEN_GROUND
                        : (byte == 0x1bu)                    ? SCREEN_ESCAPE
                                                             : SCREEN_CONTROL;
        if ((byte >= 0x40u) && (byte <= 0x7eu))
        {
            EMBED_CALL(screen_control, ScreenCtx, .screen = screen, .byte = byte);
        }
        else if (byte >= 0x20u)
        {
            EMBED_CALL(screen_command_put, ScreenCtx, .screen = screen, .byte = byte);
        }
        else if (byte != 0x1bu)
        {
            EMBED_CALL(screen_control_letter, ScreenCtx, .screen = screen, .byte = byte);
        }
        break;
    case SCREEN_COMMAND:
    case SCREEN_DEVICE:
        if ((byte == 0x07u) && (screen->state == SCREEN_COMMAND))
        {
            screen->state = SCREEN_GROUND;
            EMBED_CALL(screen_command, ScreenCtx, .screen = screen);
        }
        else if (byte == 0x1bu)
        {
            screen->ending = screen->state;
            screen->state = SCREEN_STRING_END;
        }
        else if (screen->state == SCREEN_COMMAND)
        {
            EMBED_CALL(screen_command_put, ScreenCtx, .screen = screen, .byte = byte);
        }
        break;
    case SCREEN_STRING_END:
        screen->state = SCREEN_GROUND;
        if ((byte == (unsigned char)'\\') && (screen->ending == SCREEN_COMMAND))
        {
            EMBED_CALL(screen_command, ScreenCtx, .screen = screen);
        }
        if (byte != (unsigned char)'\\')
        {
            EMBED_CALL(screen_escape, ScreenCtx, .screen = screen, .byte = byte);
        }
        screen->ending = SCREEN_GROUND;
        break;
    default:
        screen->state = SCREEN_GROUND;
        break;
    }
}

// how many of the `size` bytes at `bytes` from the start are printable ASCII, a word of them at a time, the bytes of
// a word that holds another read one by one
EMBED_INLINE unsigned long long screen_plain_run(const ScreenCtx *args)
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

void cli_terminal_screen_write(const CliTerminalScreenBytesCfg *args)
{
    CliTerminalScreen *const screen = args->screen;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const int plain = (screen->state == SCREEN_GROUND) && (screen->letter_bytes_left == 0u);
        const unsigned long long run =
            plain ? EMBED_CALL(screen_plain_run, ScreenCtx, .bytes = args->bytes + at, .size = args->size - at) : 0ULL;
        for (unsigned long long letter = 0ULL; letter < run; letter += 1ULL)
        {
            EMBED_CALL(screen_print, ScreenCtx, .screen = screen, .letter = args->bytes[at + letter]);
        }
        at += run;
        if (at >= args->size)
        {
            break;
        }
        if (screen->state == SCREEN_GROUND)
        {
            EMBED_CALL(screen_ground, ScreenCtx, .screen = screen, .byte = args->bytes[at]);
        }
        else if (screen->state == SCREEN_CHARSET)
        {
            screen->state = SCREEN_GROUND;
        }
        else
        {
            EMBED_CALL(screen_sequence, ScreenCtx, .screen = screen, .byte = args->bytes[at]);
        }
        at += 1ULL;
    }
}

// the bytes of a screen's cell for `columns` by `rows`: the two screens' cells, then the marks of the rows changed,
// the command read, the answer and the place
EMBED_INLINE unsigned long long screen_cell_bytes(const ScreenCtx *args)
{
    const unsigned long long cells = (unsigned long long)args->columns * args->rows;
    return (2ULL * cells * sizeof(CliTerminalCell)) + args->rows + CLI_TERMINAL_COMMAND_MAX + CLI_TERMINAL_ANSWER_MAX +
           CLI_TERMINAL_PLACE_MAX;
}

// the screen's parts laid out in `cell`, sized for `columns` by `rows`
EMBED_INLINE void screen_lay(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    unsigned char *const cell = (unsigned char *)args->bytes;
    const unsigned long long cells = (unsigned long long)args->columns * args->rows;
    const int second_shown = (screen->shown != NULL) && (screen->shown == screen->second);
    screen->cell = cell;
    screen->main = (CliTerminalCell *)(void *)cell;
    screen->second = screen->main + cells;
    screen->shown = second_shown ? screen->second : screen->main;
    screen->changed = (unsigned char *)(void *)(screen->second + cells);
    screen->command = screen->changed + args->rows;
    screen->answer = screen->command + CLI_TERMINAL_COMMAND_MAX;
    screen->place = (char *)(screen->answer + CLI_TERMINAL_ANSWER_MAX);
    screen->columns = args->columns;
    screen->rows = args->rows;
}

int cli_terminal_screen_init(const CliTerminalScreenSizeCfg *args)
{
    CliTerminalScreen *const screen = args->screen;
    const unsigned int columns = (args->columns > 1u) ? args->columns : 2u;
    const unsigned int rows = (args->rows > 1u) ? args->rows : 2u;
    memor.set(&(MemoriaCfg){.dst = screen, .val = 0, .bytes = sizeof(*screen)});
    const unsigned long long bytes = EMBED_CALL(screen_cell_bytes, ScreenCtx, .columns = columns, .rows = rows);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_terminal_cells);
    const unsigned long long scrollback_cells = CLI_TERMINAL_SCROLLBACK_LINES * SCREEN_SCROLLBACK_WIDTH;
    unsigned char *const scrollback =
        (cell != NULL) ? g_cli_cells->persistent_buf_alloc((CLI_TERMINAL_SCROLLBACK_LINES * sizeof(CliTerminalLine)) +
                                                               (scrollback_cells * sizeof(CliTerminalCell)),
                                                           &g_cli_terminal_cells)
                       : NULL;
    if (scrollback == NULL)
    {
        g_cli_cells->persistent_buf_release(cell, &g_cli_terminal_cells);
        return 0;
    }
    EMBED_CALL(screen_lay, ScreenCtx, .screen = screen, .bytes = cell, .columns = columns, .rows = rows);
    screen->scrollback_cell = scrollback;
    screen->scrollback_lines = (CliTerminalLine *)(void *)scrollback;
    screen->scrollback_cells =
        (CliTerminalCell *)(void *)(scrollback + (CLI_TERMINAL_SCROLLBACK_LINES * sizeof(CliTerminalLine)));
    screen->scrollback_cell_count = scrollback_cells;
    EMBED_CALL(screen_reset, ScreenCtx, .screen = screen);
    EMBED_CALL(screen_lines_erase, ScreenCtx, .screen = screen, .from = 0u, .to = rows);
    screen->shown = screen->second;
    EMBED_CALL(screen_lines_erase, ScreenCtx, .screen = screen, .from = 0u, .to = rows);
    screen->shown = screen->main;
    return 1;
}

// the rows of one of the screen's two screens, `from` the old cells, copied into `to` the new, each row cut or padded
// to the new width; the main screen's lines from its top go to the scrollback while the cursor stands below its new
// foot, where it is the one drawn on, and the rest from its foot are let go
EMBED_INLINE void screen_copy(const ScreenCtx *args)
{
    CliTerminalScreen *const screen = args->screen;
    const CliTerminalCell *const from = args->cells;
    CliTerminalCell *const to = (CliTerminalCell *)args->bytes;
    const unsigned int old_columns = screen->columns;
    const unsigned int old_rows = screen->rows;
    const int drawn = (from == screen->shown);
    unsigned int dropped = 0u;
    while (drawn && ((old_rows - dropped) > args->rows) && ((screen->cursor.y - dropped) >= args->rows))
    {
        if (from == screen->main)
        {
            EMBED_CALL(screen_scrollback_put, ScreenCtx, .screen = screen,
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
    screen->cursor.y -= drawn ? dropped : 0u;
}

int cli_terminal_screen_resize(const CliTerminalScreenSizeCfg *args)
{
    CliTerminalScreen *const screen = args->screen;
    const unsigned int columns = (args->columns > 1u) ? args->columns : 2u;
    const unsigned int rows = (args->rows > 1u) ? args->rows : 2u;
    if ((columns == screen->columns) && (rows == screen->rows))
    {
        return 1;
    }
    const unsigned long long bytes = EMBED_CALL(screen_cell_bytes, ScreenCtx, .columns = columns, .rows = rows);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(bytes, &g_cli_terminal_cells);
    if (cell == NULL)
    {
        return 0;
    }
    const unsigned long long cells = (unsigned long long)columns * rows;
    CliTerminalCell *const main = (CliTerminalCell *)(void *)cell;
    EMBED_CALL(screen_copy, ScreenCtx, .screen = screen, .cells = screen->main, .bytes = (const unsigned char *)main,
               .columns = columns, .rows = rows);
    EMBED_CALL(screen_copy, ScreenCtx, .screen = screen, .cells = screen->second,
               .bytes = (const unsigned char *)(main + cells), .columns = columns, .rows = rows);
    unsigned char *const old = screen->cell;
    const unsigned int command_size = screen->command_size;
    const unsigned int answer_size = screen->answer_size;
    const unsigned char *const old_command = screen->command;
    const unsigned char *const old_answer = screen->answer;
    const char *const old_place = screen->place;
    EMBED_CALL(screen_lay, ScreenCtx, .screen = screen, .bytes = cell, .columns = columns, .rows = rows);
    memor.cpy(&(MemoriaCfg){.dst = screen->command, .src = old_command, .bytes = command_size});
    memor.cpy(&(MemoriaCfg){.dst = screen->answer, .src = old_answer, .bytes = answer_size});
    memor.cpy(&(MemoriaCfg){.dst = screen->place, .src = old_place, .bytes = CLI_TERMINAL_PLACE_MAX});
    g_cli_cells->persistent_buf_release(old, &g_cli_terminal_cells);
    screen->top = 0u;
    screen->bottom = rows - 1u;
    screen->cursor.x = (screen->cursor.x < columns) ? screen->cursor.x : (columns - 1u);
    screen->cursor.y = (screen->cursor.y < rows) ? screen->cursor.y : (rows - 1u);
    screen->cursor.wrap_pending = 0;
    EMBED_CALL(screen_touch_all, ScreenCtx, .screen = screen);
    return 1;
}

void cli_terminal_screen_free(const CliTerminalScreenCfg *args)
{
    g_cli_cells->persistent_buf_release(args->screen->cell, &g_cli_terminal_cells);
    g_cli_cells->persistent_buf_release(args->screen->scrollback_cell, &g_cli_terminal_cells);
    args->screen->cell = NULL;
    args->screen->scrollback_cell = NULL;
}

const CliTerminalCell *cli_terminal_screen_scrollback(const CliTerminalScreenLineCfg *args)
{
    const CliTerminalScreen *const screen = args->screen_read;
    if (args->line >= screen->scrollback_count)
    {
        *args->columns = 0u;
        return NULL;
    }
    const CliTerminalLine *const line =
        &screen->scrollback_lines[(screen->scrollback_first + args->line) % CLI_TERMINAL_SCROLLBACK_LINES];
    *args->columns = line->columns;
    return screen->scrollback_cells + line->at;
}
