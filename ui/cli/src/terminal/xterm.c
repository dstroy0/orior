// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// xterm.c: a shell's output read into the text as an xterm reads it (xterm.h). The output's runs of letters, ASCII a
// word at a time and whole UTF-8 letters among them, are written into the row at the cursor as one edit each, a run
// past the row's columns cut where it wraps; a letter cut across two writes is put together on the stack, an
// ill-formed one written as U+FFFD. A column's place in a row counts its letters, a sequence among them counting none
#include "xterm.h"
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "../editor/document.h"

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

// the tab stops' spacing; the most bytes an SGR the xterm writes takes; and the spaces and line ends one edit writes
#define XTERM_TAB 8u
#define XTERM_SGR_MAX 64u
#define XTERM_RUN 64u

// the replacement letter's UTF-8, which an ill-formed run of bytes is written as
static const char s_replacement[3] = {(char)0xefu, (char)0xbfu, (char)0xbdu};
// XTERM_RUN spaces and XTERM_RUN line ends, which blanks and empty lines are written from
static const char s_spaces[XTERM_RUN + 1u] = "                                                                ";
static const char s_line_ends[XTERM_RUN + 1u] = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
                                                "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

_Static_assert(sizeof(s_spaces) == (XTERM_RUN + 1u), "s_spaces is XTERM_RUN spaces and its end");
_Static_assert(sizeof(s_line_ends) == (XTERM_RUN + 1u), "s_line_ends is XTERM_RUN line ends and its end");

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
    unsigned char byte;
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long letters;
    const unsigned int *numbers;
    unsigned int *numbers_out;
    unsigned int number_count;
    const unsigned short *units;
    unsigned long long units_count;
    unsigned long long at;
    unsigned long long *letters_out;
    unsigned long long line;
    unsigned long long lines;
    CliPosition place;
    CliPosition past;
    const char *text;
    unsigned long long value;
    CliTerminalStyle *style;
    const CliTerminalStyle *style_read;
    char *out;
} XtermCtx;

// the number of the text's lines
EMBED_INLINE unsigned long long xterm_text_lines(const XtermCtx *args)
{
    CliShape shape = {0ULL, 0ULL};
    const int stored =
        EMBED_CALL(cli_document_text.shape, CliDocumentTextCfg, .key = args->xterm_read->key, .shape = &shape);
    return stored ? shape.lines : 0ULL;
}

// the units of the text's line `line`, where they stand, their count into `letters_out`: NULL where it has none
EMBED_INLINE const unsigned short *xterm_line_units(const XtermCtx *args)
{
    return (args->xterm_read->key != NULL)
               ? EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = args->xterm_read->key,
                            .line = args->line, .count = args->letters_out)
               : NULL;
}

// the text from `place` to `past` replaced by the `size` bytes at `text`: 1, or 0 where the text refused, after which
// nothing more is written into it
EMBED_INLINE int xterm_edit(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    if ((xterm->key == NULL) || xterm->refused)
    {
        return 0;
    }
    const CliEdit edit = {args->place, args->past, args->text, args->size};
    const CliEdit *const change = &edit;
    const unsigned int edits = 1u;
    CliShape shape = {0ULL, 0ULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const int done = EMBED_CALL(cli_document_err_msg.edit, CliDocumentErrMsgCfg, .err_msg = &err_msg, .key = xterm->key,
                                .shape = &shape, .changes = &change, .edits = &edits, .count_u32 = 1u);
    xterm->refused = !done;
    xterm->moved = 1;
    return done;
}

// the unit at `at` of the text's units, or of the bytes where no units are passed
EMBED_INLINE unsigned int xterm_unit(const XtermCtx *args)
{
    return (args->units != NULL) ? args->units[args->at] : args->bytes[args->at];
}

// the units an escape sequence at `at` takes: its escape, its opening, its numbers and its last letter, the rest of the
// units where it runs past them, one for an escape alone; 0 where no escape stands at `at`. The units are the text's,
// or the bytes where no units are passed
EMBED_INLINE unsigned long long xterm_sequence_units(const XtermCtx *args)
{
    const unsigned long long count = args->units_count;
    const unsigned long long at = args->at;
    if ((at >= count) ||
        (EMBED_CALL(xterm_unit, XtermCtx, .units = args->units, .bytes = args->bytes, .at = at) != 0x1bu))
    {
        return 0ULL;
    }
    if (((at + 1ULL) >= count) || (EMBED_CALL(xterm_unit, XtermCtx, .units = args->units, .bytes = args->bytes,
                                              .at = at + 1ULL) != (unsigned int)'['))
    {
        return 1ULL;
    }
    unsigned long long end = at + 2ULL;
    unsigned int unit =
        (end < count) ? EMBED_CALL(xterm_unit, XtermCtx, .units = args->units, .bytes = args->bytes, .at = end) : 0u;
    while ((end < count) && (unit >= 0x20u) && (unit <= 0x3fu))
    {
        end += 1ULL;
        unit = (end < count) ? EMBED_CALL(xterm_unit, XtermCtx, .units = args->units, .bytes = args->bytes, .at = end)
                             : 0u;
    }
    return ((end < count) && (unit >= 0x40u) && (unit <= 0x7eu)) ? (end + 1ULL - at) : (count - at);
}

// the units the letter at `at` takes: two for a surrogate pair, else one; one byte where no units are passed
EMBED_INLINE unsigned long long xterm_letter_units(const XtermCtx *args)
{
    if (args->units == NULL)
    {
        return 1ULL;
    }
    const unsigned short unit = args->units[args->at];
    const int high = (unit >= 0xd800u) && (unit <= 0xdbffu);
    const int paired = high && ((args->at + 1ULL) < args->units_count) && (args->units[args->at + 1ULL] >= 0xdc00u) &&
                       (args->units[args->at + 1ULL] <= 0xdfffu);
    return paired ? 2ULL : 1ULL;
}

// the place where the letter of column `x` starts among a row's units, the sequences before it passed over: the units'
// count where the row's letters end first, the letters standing before the place into `letters_out`
EMBED_INLINE unsigned long long xterm_column_start(const XtermCtx *args)
{
    unsigned long long at = 0ULL;
    unsigned long long letters = 0ULL;
    for (;;)
    {
        unsigned long long sequence = EMBED_CALL(xterm_sequence_units, XtermCtx, .units = args->units,
                                                 .units_count = args->units_count, .at = at);
        while (sequence != 0ULL)
        {
            at += sequence;
            sequence = EMBED_CALL(xterm_sequence_units, XtermCtx, .units = args->units,
                                  .units_count = args->units_count, .at = at);
        }
        if ((at >= args->units_count) || (letters == args->x))
        {
            break;
        }
        at +=
            EMBED_CALL(xterm_letter_units, XtermCtx, .units = args->units, .units_count = args->units_count, .at = at);
        letters += 1ULL;
    }
    if (args->letters_out != NULL)
    {
        *args->letters_out = letters;
    }
    return at;
}

// the place just past the letter of column `x` among a row's units, no sequence after it passed over: the units' count
// where the row's letters end first
EMBED_INLINE unsigned long long xterm_column_end(const XtermCtx *args)
{
    const unsigned long long start = EMBED_CALL(xterm_column_start, XtermCtx, .units = args->units,
                                                .units_count = args->units_count, .x = args->x, .letters_out = NULL);
    return (start >= args->units_count) ? args->units_count
                                        : (start + EMBED_CALL(xterm_letter_units, XtermCtx, .units = args->units,
                                                              .units_count = args->units_count, .at = start));
}

// whether an escape sequence stands at `at` among a row's units: every sequence the xterm writes into the text sets the
// whole style from the plain one, so one standing just past letters written sets the style of those after it itself
EMBED_INLINE int xterm_sequence_at(const XtermCtx *args)
{
    return EMBED_CALL(xterm_sequence_units, XtermCtx, .units = args->units, .units_count = args->units_count,
                      .at = args->at) != 0ULL;
}

// whether a letter stands at or past `at` among a row's units
EMBED_INLINE int xterm_letters_follow(const XtermCtx *args)
{
    unsigned long long at = args->at;
    while (at < args->units_count)
    {
        const unsigned long long sequence = EMBED_CALL(xterm_sequence_units, XtermCtx, .units = args->units,
                                                       .units_count = args->units_count, .at = at);
        if (sequence == 0ULL)
        {
            return 1;
        }
        at += sequence;
    }
    return 0;
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

// the style `style` changed by a Select Graphic Rendition's numbers
EMBED_INLINE void xterm_rendition(const XtermCtx *args)
{
    CliTerminalStyle *const style = args->style;
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

// the style `style` changed by the escape sequence of `size` units at `at` where it is an SGR, the units the text's
// or the bytes where no units are passed: its numbers read, XTERM_NUMBERS at most, and the rendition applied
EMBED_INLINE void xterm_sgr_apply(const XtermCtx *args)
{
    const unsigned long long place = args->at;
    const unsigned long long sequence = args->size;
    if ((sequence < 3ULL) || (EMBED_CALL(xterm_unit, XtermCtx, .units = args->units, .bytes = args->bytes,
                                         .at = place + sequence - 1ULL) != (unsigned int)'m'))
    {
        return;
    }
    unsigned int numbers[XTERM_NUMBERS];
    unsigned int count = 1u;
    numbers[0] = XTERM_ABSENT;
    for (unsigned long long at = place + 2ULL; at < (place + sequence - 1ULL); at += 1ULL)
    {
        const unsigned int unit =
            EMBED_CALL(xterm_unit, XtermCtx, .units = args->units, .bytes = args->bytes, .at = at);
        const unsigned int was = (numbers[count - 1u] == XTERM_ABSENT) ? 0u : numbers[count - 1u];
        if ((unit >= (unsigned int)'0') && (unit <= (unsigned int)'9'))
        {
            // a number of more than nine digits stands at its first nine, far below what an unsigned int counts
            numbers[count - 1u] = (was < 100000000u) ? ((was * 10u) + (unit - (unsigned int)'0')) : was;
        }
        else if (((unit == (unsigned int)';') || (unit == (unsigned int)':')) && (count < XTERM_NUMBERS))
        {
            numbers[count] = XTERM_ABSENT;
            count += 1u;
        }
    }
    EMBED_CALL(xterm_rendition, XtermCtx, .style = args->style, .numbers = numbers, .number_count = count);
}

// the style `style` changed by the SGR sequences of a row's units before `at`, the units the text's or the bytes where
// no units are passed
EMBED_INLINE void xterm_style_apply(const XtermCtx *args)
{
    unsigned long long place = 0ULL;
    while ((place < args->at) && (place < args->units_count))
    {
        const unsigned long long sequence =
            EMBED_CALL(xterm_sequence_units, XtermCtx, .units = args->units, .bytes = args->bytes,
                       .units_count = args->units_count, .at = place);
        if (sequence != 0ULL)
        {
            EMBED_CALL(xterm_sgr_apply, XtermCtx, .units = args->units, .bytes = args->bytes, .at = place,
                       .size = sequence, .style = args->style);
        }
        place += (sequence != 0ULL) ? sequence
                                    : EMBED_CALL(xterm_letter_units, XtermCtx, .units = args->units,
                                                 .units_count = args->units_count, .at = place);
    }
}

// the style the SGR sequences of a row's units before `at` set, from the plain style
EMBED_INLINE CliTerminalStyle xterm_style_at(const XtermCtx *args)
{
    CliTerminalStyle style = {CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u};
    EMBED_CALL(xterm_style_apply, XtermCtx, .units = args->units, .units_count = args->units_count, .at = args->at,
               .style = &style);
    return style;
}

// whether two styles are one
EMBED_INLINE int xterm_style_same(const XtermCtx *args)
{
    return (args->style_read->foreground == args->style->foreground) &&
           (args->style_read->background == args->style->background) && (args->style_read->marks == args->style->marks);
}

// a color put after the SGR at `line`: the eight's own number from `base`, the bright eight's from `bright`, any other
// of the 256 as `wide` and 5, and a direct one as `wide` and 2 and its parts
EMBED_INLINE void xterm_sgr_color(VerbaLinea *line, unsigned int color, unsigned int base, unsigned int bright,
                                  unsigned int wide)
{
    const unsigned int kind = color & CLI_TERMINAL_COLOR_KIND;
    const unsigned int index = color & 0xffu;
    if (kind == CLI_TERMINAL_COLOR_INDEXED)
    {
        mmgr_line_ch(line, ';');
        mmgr_line_decimal(line, (index < 8u) ? (base + index) : ((index < 16u) ? (bright + index - 8u) : wide), 1u);
        if (index >= 16u)
        {
            mmgr_line_put(line, ";5;");
            mmgr_line_decimal(line, index, 1u);
        }
    }
    else if (kind == CLI_TERMINAL_COLOR_DIRECT)
    {
        mmgr_line_ch(line, ';');
        mmgr_line_decimal(line, wide, 1u);
        mmgr_line_put(line, ";2;");
        mmgr_line_decimal(line, (color >> 16u) & 0xffu, 1u);
        mmgr_line_ch(line, ';');
        mmgr_line_decimal(line, (color >> 8u) & 0xffu, 1u);
        mmgr_line_ch(line, ';');
        mmgr_line_decimal(line, color & 0xffu, 1u);
    }
}

// the SGR that sets the style `style_read` from the plain style, written into `out`, sized for XTERM_SGR_MAX: its bytes
EMBED_INLINE unsigned long long xterm_sgr_write(const XtermCtx *args)
{
    static const unsigned int s_marks[6] = {CLI_TERMINAL_BOLD,      CLI_TERMINAL_DIM,     CLI_TERMINAL_ITALIC,
                                            CLI_TERMINAL_UNDERLINE, CLI_TERMINAL_INVERSE, CLI_TERMINAL_STRIKE};
    static const char *const s_numbers[6] = {";1", ";2", ";3", ";4", ";7", ";9"};
    VerbaLinea line = {args->out, XTERM_SGR_MAX, 0ULL};
    mmgr_line_put(&line, "\x1b[0");
    for (unsigned int mark = 0u; mark < 6u; mark += 1u)
    {
        mmgr_line_put(&line, ((args->style_read->marks & s_marks[mark]) != 0u) ? s_numbers[mark] : "");
    }
    xterm_sgr_color(&line, args->style_read->foreground, 30u, 90u, 38u);
    xterm_sgr_color(&line, args->style_read->background, 40u, 100u, 48u);
    mmgr_line_ch(&line, 'm');
    return mmgr_line_ok(&line) ? line.at : 0ULL;
}

// the number of the text's lines before its last, which a line put past it is put after, and the units of its last
EMBED_INLINE CliPosition xterm_text_end(const XtermCtx *args)
{
    const unsigned long long lines = EMBED_CALL(xterm_text_lines, XtermCtx, .xterm_read = args->xterm_read);
    const unsigned long long last = (lines != 0ULL) ? (lines - 1ULL) : 0ULL;
    unsigned long long units = 0ULL;
    (void)EMBED_CALL(xterm_line_units, XtermCtx, .xterm_read = args->xterm_read, .line = last, .letters_out = &units);
    return (CliPosition){last, units};
}

// `lines` empty lines put before the text's line `line`, past its last where `line` is past it
EMBED_INLINE void xterm_lines_put(const XtermCtx *args)
{
    unsigned long long left = args->lines;
    while (left > 0ULL)
    {
        const unsigned long long run = (left < XTERM_RUN) ? left : XTERM_RUN;
        const unsigned long long lines = EMBED_CALL(xterm_text_lines, XtermCtx, .xterm_read = args->xterm);
        const CliPosition place = (args->line < lines)
                                      ? (CliPosition){args->line, 0ULL}
                                      : EMBED_CALL(xterm_text_end, XtermCtx, .xterm_read = args->xterm);
        if (!EMBED_CALL(xterm_edit, XtermCtx, .xterm = args->xterm, .place = place, .past = place, .text = s_line_ends,
                        .size = run))
        {
            return;
        }
        left -= run;
    }
}

// `lines` of the text's lines from `line` on taken out, the text keeping one line at the least
EMBED_INLINE void xterm_lines_take(const XtermCtx *args)
{
    const unsigned long long lines = EMBED_CALL(xterm_text_lines, XtermCtx, .xterm_read = args->xterm);
    if ((args->lines == 0ULL) || (args->line >= lines))
    {
        return;
    }
    const unsigned long long past = ((args->line + args->lines) < lines) ? (args->line + args->lines) : lines;
    if (past < lines)
    {
        (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = args->xterm, .place = (CliPosition){args->line, 0ULL},
                         .past = (CliPosition){past, 0ULL}, .text = "", .size = 0ULL);
        return;
    }
    const CliPosition end = EMBED_CALL(xterm_text_end, XtermCtx, .xterm_read = args->xterm);
    CliPosition place = {0ULL, 0ULL};
    if (args->line > 0ULL)
    {
        unsigned long long units = 0ULL;
        (void)EMBED_CALL(xterm_line_units, XtermCtx, .xterm_read = args->xterm, .line = args->line - 1ULL,
                         .letters_out = &units);
        place = (CliPosition){args->line - 1ULL, units};
    }
    (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = args->xterm, .place = place, .past = end, .text = "", .size = 0ULL);
}

// the screen's row `y` left empty
EMBED_INLINE void xterm_row_clear(const XtermCtx *args)
{
    unsigned long long count = 0ULL;
    const unsigned long long line = args->xterm->base + args->y;
    (void)EMBED_CALL(xterm_line_units, XtermCtx, .xterm_read = args->xterm, .line = line, .letters_out = &count);
    if (count != 0ULL)
    {
        (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = args->xterm, .place = (CliPosition){line, 0ULL},
                         .past = (CliPosition){line, count}, .text = "", .size = 0ULL);
    }
}

// the rows from `from` up to, not including, `to` left empty
EMBED_INLINE void xterm_rows_clear(const XtermCtx *args)
{
    const unsigned int to = (args->to < args->xterm->rows) ? args->to : args->xterm->rows;
    for (unsigned int y = args->from; y < to; y += 1u)
    {
        EMBED_CALL(xterm_row_clear, XtermCtx, .xterm = args->xterm, .y = y);
    }
}

// the letters of row `y` from column `from` up to, not including, column `to` erased: to the row's end taken out, else
// written as spaces, the style that stood after them put back where it differs from the one before them
EMBED_INLINE void xterm_cells_erase(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned long long line = xterm->base + args->y;
    unsigned long long count = 0ULL;
    const unsigned short *const units =
        EMBED_CALL(xterm_line_units, XtermCtx, .xterm_read = xterm, .line = line, .letters_out = &count);
    const unsigned long long start =
        EMBED_CALL(xterm_column_start, XtermCtx, .units = units, .units_count = count, .x = args->from);
    if (start >= count)
    {
        return;
    }
    const unsigned long long end =
        ((args->to == XTERM_ABSENT) || (args->to <= args->from))
            ? count
            : EMBED_CALL(xterm_column_end, XtermCtx, .units = units, .units_count = count, .x = args->to - 1u);
    const int follows = EMBED_CALL(xterm_letters_follow, XtermCtx, .units = units, .units_count = count, .at = end);
    if (!follows)
    {
        (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = xterm, .place = (CliPosition){line, start},
                         .past = (CliPosition){line, count}, .text = "", .size = 0ULL);
        return;
    }
    const CliTerminalStyle before =
        EMBED_CALL(xterm_style_at, XtermCtx, .units = units, .units_count = count, .at = start);
    CliTerminalStyle after = EMBED_CALL(xterm_style_at, XtermCtx, .units = units, .units_count = count, .at = end);
    char put_back[XTERM_SGR_MAX];
    const unsigned long long put_back_size =
        (EMBED_CALL(xterm_sequence_at, XtermCtx, .units = units, .units_count = count, .at = end) ||
         EMBED_CALL(xterm_style_same, XtermCtx, .style_read = &before, .style = &after))
            ? 0ULL
            : EMBED_CALL(xterm_sgr_write, XtermCtx, .style_read = &after, .out = put_back);
    unsigned long long blanks = (unsigned long long)args->to - args->from;
    // the edits from the right, so each place stands as it was read
    if (put_back_size != 0ULL)
    {
        (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = xterm, .place = (CliPosition){line, end},
                         .past = (CliPosition){line, end}, .text = put_back, .size = put_back_size);
    }
    const unsigned long long first = (blanks < XTERM_RUN) ? blanks : XTERM_RUN;
    (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = xterm, .place = (CliPosition){line, start},
                     .past = (CliPosition){line, end}, .text = s_spaces, .size = first);
    blanks -= first;
    while (blanks > 0ULL)
    {
        const unsigned long long run = (blanks < XTERM_RUN) ? blanks : XTERM_RUN;
        (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = xterm, .place = (CliPosition){line, start},
                         .past = (CliPosition){line, start}, .text = s_spaces, .size = run);
        blanks -= run;
    }
}

// the region scrolled up `count` lines: on the main screen with the region the whole screen, the lines leaving its top
// join the history where they stand and empty lines come in at its foot; else the region's top lines taken out and
// empty lines put after its foot
EMBED_INLINE void xterm_scroll_up(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned int height = xterm->bottom - xterm->top + 1u;
    const unsigned int count = (args->count < height) ? args->count : height;
    if (!xterm->second && (xterm->top == 0u) && (xterm->bottom == (xterm->rows - 1u)))
    {
        xterm->history += count;
        xterm->base += count;
        EMBED_CALL(xterm_lines_put, XtermCtx, .xterm = xterm, .line = xterm->base + xterm->rows - count,
                   .lines = count);
        return;
    }
    EMBED_CALL(xterm_lines_take, XtermCtx, .xterm = xterm, .line = xterm->base + xterm->top, .lines = count);
    EMBED_CALL(xterm_lines_put, XtermCtx, .xterm = xterm, .line = xterm->base + xterm->bottom + 1u - count,
               .lines = count);
}

// the region scrolled down `count` lines, empty lines coming in at its top
EMBED_INLINE void xterm_scroll_down(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned int height = xterm->bottom - xterm->top + 1u;
    const unsigned int count = (args->count < height) ? args->count : height;
    EMBED_CALL(xterm_lines_take, XtermCtx, .xterm = xterm, .line = xterm->base + xterm->bottom + 1u - count,
               .lines = count);
    EMBED_CALL(xterm_lines_put, XtermCtx, .xterm = xterm, .line = xterm->base + xterm->top, .lines = count);
}

// the cursor moved to column `x` and row `y`, each bounded by the screen, the next letter no longer wrapping first
EMBED_INLINE void xterm_move(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    xterm->cursor.x = (args->x < xterm->columns) ? args->x : (xterm->columns - 1u);
    xterm->cursor.y = (args->y < xterm->rows) ? args->y : (xterm->rows - 1u);
    xterm->cursor.wrap_pending = 0;
    xterm->moved = 1;
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

// the bytes of the first `letters` letters of the `size` bytes of UTF-8 at `bytes`, each letter whole
EMBED_INLINE unsigned long long xterm_letters_bytes(const XtermCtx *args)
{
    unsigned long long at = 0ULL;
    for (unsigned long long letter = 0ULL; (letter < args->letters) && (at < args->size); letter += 1ULL)
    {
        const unsigned char lead = args->bytes[at];
        at += (lead < 0x80u) ? 1ULL : (((lead & 0xe0u) == 0xc0u) ? 2ULL : (((lead & 0xf0u) == 0xe0u) ? 3ULL : 4ULL));
    }
    return (at < args->size) ? at : args->size;
}

// `letters` letters, the `size` bytes of UTF-8 at `bytes`, written at column `x` of row `y` in the style of the
// moment, in place of the letters they cover and the sequences between the letter before the column and the column's
// own, or before the column's letter in insert mode, the row cut at its last column after: the SGR of the style of the
// moment before them where the line's style past the letter before is another, the style that stood after them put back
// where letters follow, and spaces up to the column first where the row ends before it. The edits go from the right, so
// each place stands as the row was read
EMBED_INLINE void xterm_row_put(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned long long line = xterm->base + args->y;
    unsigned long long count = 0ULL;
    const unsigned short *const units =
        EMBED_CALL(xterm_line_units, XtermCtx, .xterm_read = xterm, .line = line, .letters_out = &count);
    unsigned long long before = 0ULL;
    const unsigned long long column = EMBED_CALL(xterm_column_start, XtermCtx, .units = units, .units_count = count,
                                                 .x = args->x, .letters_out = &before);
    // where the row's letters reach the column, the letters start just past the letter before it, the sequences
    // between written over and the style they set put back after; the first column's start at the row's start
    const unsigned long long start = (before < args->x) ? column
                                     : (args->x == 0u)  ? 0ULL
                                                        : EMBED_CALL(xterm_column_end, XtermCtx, .units = units,
                                                                     .units_count = count, .x = args->x - 1u);
    unsigned long long pad = (before < args->x) ? (args->x - before) : 0ULL;
    // the letters covered end at the last the run reaches; a run past the row's letters covers to the row's end
    const unsigned long long end = (xterm->inserts || (column >= count))
                                       ? column
                                       : EMBED_CALL(xterm_column_end, XtermCtx, .units = units, .units_count = count,
                                                    .x = args->x + (unsigned int)args->letters - 1u);
    const CliTerminalStyle lined =
        EMBED_CALL(xterm_style_at, XtermCtx, .units = units, .units_count = count, .at = start);
    const CliTerminalStyle after =
        EMBED_CALL(xterm_style_at, XtermCtx, .units = units, .units_count = count, .at = end);
    const int follows = EMBED_CALL(xterm_letters_follow, XtermCtx, .units = units, .units_count = count, .at = end);
    char lead[XTERM_SGR_MAX];
    char put_back[XTERM_SGR_MAX];
    const unsigned long long lead_size =
        EMBED_CALL(xterm_style_same, XtermCtx, .style_read = &lined, .style = &xterm->cursor.style)
            ? 0ULL
            : EMBED_CALL(xterm_sgr_write, XtermCtx, .style_read = &xterm->cursor.style, .out = lead);
    const unsigned long long put_back_size =
        (!follows || EMBED_CALL(xterm_sequence_at, XtermCtx, .units = units, .units_count = count, .at = end) ||
         EMBED_CALL(xterm_style_same, XtermCtx, .style_read = &after, .style = &xterm->cursor.style))
            ? 0ULL
            : EMBED_CALL(xterm_sgr_write, XtermCtx, .style_read = &after, .out = put_back);
    if (put_back_size != 0ULL)
    {
        (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = xterm, .place = (CliPosition){line, end},
                         .past = (CliPosition){line, end}, .text = put_back, .size = put_back_size);
    }
    // the output's bytes, where they lie, are the letters written
    (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = xterm, .place = (CliPosition){line, start},
                     .past = (CliPosition){line, end}, .text = (const char *)args->bytes, .size = args->size);
    if (lead_size != 0ULL)
    {
        (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = xterm, .place = (CliPosition){line, start},
                         .past = (CliPosition){line, start}, .text = lead, .size = lead_size);
    }
    while (pad > 0ULL)
    {
        const unsigned long long run = (pad < XTERM_RUN) ? pad : XTERM_RUN;
        (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = xterm, .place = (CliPosition){line, start},
                         .past = (CliPosition){line, start}, .text = s_spaces, .size = run);
        pad -= run;
    }
    if (xterm->inserts)
    {
        unsigned long long now = 0ULL;
        const unsigned short *const row =
            EMBED_CALL(xterm_line_units, XtermCtx, .xterm_read = xterm, .line = line, .letters_out = &now);
        const unsigned long long cut =
            EMBED_CALL(xterm_column_end, XtermCtx, .units = row, .units_count = now, .x = xterm->columns - 1u);
        if (cut < now)
        {
            (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = xterm, .place = (CliPosition){line, cut},
                             .past = (CliPosition){line, now}, .text = "", .size = 0ULL);
        }
    }
}

// `letters` letters, the `size` bytes of UTF-8 at `bytes`, printed at the cursor, a line fed first where the last
// printed stood at the last column, each row taking as many as its columns left take and the rest wrapping
EMBED_INLINE void xterm_letters_put(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned char *bytes = args->bytes;
    unsigned long long size = args->size;
    unsigned long long letters = args->letters;
    while (letters > 0ULL)
    {
        if (xterm->cursor.wrap_pending && xterm->wraps)
        {
            xterm->cursor.wrap_pending = 0;
            xterm->cursor.x = 0u;
            EMBED_CALL(xterm_line_feed, XtermCtx, .xterm = xterm);
        }
        const unsigned int x = xterm->cursor.x;
        const unsigned long long fits = (unsigned long long)xterm->columns - x;
        const unsigned long long taken = (letters < fits) ? letters : fits;
        const unsigned long long part =
            EMBED_CALL(xterm_letters_bytes, XtermCtx, .bytes = bytes, .size = size, .letters = taken);
        EMBED_CALL(xterm_row_put, XtermCtx, .xterm = xterm, .x = x, .y = xterm->cursor.y, .bytes = bytes, .size = part,
                   .letters = taken);
        bytes += part;
        size -= part;
        letters -= taken;
        // a row's columns are far inside what an unsigned int counts
        const unsigned int reached = x + (unsigned int)taken;
        xterm->cursor.wrap_pending = reached >= xterm->columns;
        xterm->cursor.x = xterm->cursor.wrap_pending ? (xterm->columns - 1u) : reached;
        xterm->moved = 1;
        // with wrapping off the letters past the last column are let go
        letters = xterm->wraps ? letters : 0ULL;
    }
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

// the second screen drawn on, its rows put after the main screen's, the cursor saved and the region the whole screen;
// and the main screen drawn on again, the second's rows taken out, the cursor restored
EMBED_INLINE void xterm_second_enter(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    if (xterm->second)
    {
        return;
    }
    EMBED_CALL(xterm_save, XtermCtx, .xterm = xterm);
    EMBED_CALL(xterm_lines_put, XtermCtx, .xterm = xterm, .line = xterm->history + xterm->rows, .lines = xterm->rows);
    xterm->second = 1;
    xterm->base = xterm->history + xterm->rows;
    xterm->top = 0u;
    xterm->bottom = xterm->rows - 1u;
    xterm->moved = 1;
}

EMBED_INLINE void xterm_second_leave(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    if (!xterm->second)
    {
        return;
    }
    EMBED_CALL(xterm_lines_take, XtermCtx, .xterm = xterm, .line = xterm->base, .lines = xterm->rows);
    xterm->second = 0;
    xterm->base = xterm->history;
    xterm->top = 0u;
    xterm->bottom = xterm->rows - 1u;
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
    xterm->moved = 1;
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
        xterm->moved = 1;
        break;
    case 47u:
    case 1047u:
    case 1049u:
        if (args->on)
        {
            EMBED_CALL(xterm_second_enter, XtermCtx, .xterm = xterm);
        }
        else
        {
            EMBED_CALL(xterm_second_leave, XtermCtx, .xterm = xterm);
        }
        break;
    case 2004u:
        xterm->bracketed_paste = args->on;
        break;
    default:
        break;
    }
}

// the history taken out of the text, where the output asks it cleared
EMBED_INLINE void xterm_history_clear(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    EMBED_CALL(xterm_lines_take, XtermCtx, .xterm = xterm, .line = 0ULL, .lines = xterm->history);
    xterm->base -= xterm->history;
    xterm->history = 0ULL;
}

// the display erased: from the cursor to its end, from its start to the cursor, whole, or the history taken out
EMBED_INLINE void xterm_display_erase(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const CliTerminalCursor at = xterm->cursor;
    switch (args->mode)
    {
    case 0u:
        EMBED_CALL(xterm_cells_erase, XtermCtx, .xterm = xterm, .y = at.y, .from = at.x, .to = XTERM_ABSENT);
        EMBED_CALL(xterm_rows_clear, XtermCtx, .xterm = xterm, .from = at.y + 1u, .to = xterm->rows);
        break;
    case 1u:
        EMBED_CALL(xterm_rows_clear, XtermCtx, .xterm = xterm, .from = 0u, .to = at.y);
        EMBED_CALL(xterm_cells_erase, XtermCtx, .xterm = xterm, .y = at.y, .from = 0u, .to = at.x + 1u);
        break;
    case 2u:
        EMBED_CALL(xterm_rows_clear, XtermCtx, .xterm = xterm, .from = 0u, .to = xterm->rows);
        break;
    case 3u:
        EMBED_CALL(xterm_history_clear, XtermCtx, .xterm = xterm);
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
    if (args->on)
    {
        EMBED_CALL(xterm_lines_take, XtermCtx, .xterm = xterm, .line = xterm->base + xterm->bottom + 1u - count,
                   .lines = count);
        EMBED_CALL(xterm_lines_put, XtermCtx, .xterm = xterm, .line = xterm->base + xterm->cursor.y, .lines = count);
    }
    else
    {
        EMBED_CALL(xterm_lines_take, XtermCtx, .xterm = xterm, .line = xterm->base + xterm->cursor.y, .lines = count);
        EMBED_CALL(xterm_lines_put, XtermCtx, .xterm = xterm, .line = xterm->base + xterm->bottom + 1u - count,
                   .lines = count);
    }
    EMBED_CALL(xterm_move, XtermCtx, .xterm = xterm, .x = 0u, .y = xterm->cursor.y);
}

// `count` columns at the cursor: blanks inserted, the row cut at its last column after, or letters deleted with the
// sequences between the letter before the column and the column's own, the style that stood after them put back where
// it differs from the one before them
EMBED_INLINE void xterm_cells_insert(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned long long line = xterm->base + xterm->cursor.y;
    unsigned long long count = 0ULL;
    const unsigned short *const units =
        EMBED_CALL(xterm_line_units, XtermCtx, .xterm_read = xterm, .line = line, .letters_out = &count);
    const unsigned int left = xterm->columns - xterm->cursor.x;
    const unsigned int many = (args->count < left) ? args->count : left;
    const unsigned long long start =
        EMBED_CALL(xterm_column_start, XtermCtx, .units = units, .units_count = count, .x = xterm->cursor.x);
    if (start >= count)
    {
        return;
    }
    if (args->on)
    {
        const int inserted = xterm->inserts;
        xterm->inserts = 1;
        unsigned int blanks = many;
        while (blanks > 0u)
        {
            const unsigned int run = (blanks < XTERM_RUN) ? blanks : XTERM_RUN;
            const CliTerminalStyle style = xterm->cursor.style;
            // blanks carry the line's style where they go
            xterm->cursor.style =
                EMBED_CALL(xterm_style_at, XtermCtx, .units = units, .units_count = count, .at = start);
            EMBED_CALL(xterm_row_put, XtermCtx, .xterm = xterm, .x = xterm->cursor.x, .y = xterm->cursor.y,
                       .bytes = (const unsigned char *)s_spaces, .size = run, .letters = run);
            xterm->cursor.style = style;
            blanks -= run;
        }
        xterm->inserts = inserted;
        return;
    }
    const unsigned long long end =
        EMBED_CALL(xterm_column_end, XtermCtx, .units = units, .units_count = count, .x = xterm->cursor.x + many - 1u);
    const unsigned long long from =
        (xterm->cursor.x == 0u)
            ? 0ULL
            : EMBED_CALL(xterm_column_end, XtermCtx, .units = units, .units_count = count, .x = xterm->cursor.x - 1u);
    const CliTerminalStyle before =
        EMBED_CALL(xterm_style_at, XtermCtx, .units = units, .units_count = count, .at = from);
    CliTerminalStyle after = EMBED_CALL(xterm_style_at, XtermCtx, .units = units, .units_count = count, .at = end);
    const int follows = EMBED_CALL(xterm_letters_follow, XtermCtx, .units = units, .units_count = count, .at = end);
    char put_back[XTERM_SGR_MAX];
    const unsigned long long put_back_size =
        (!follows || EMBED_CALL(xterm_sequence_at, XtermCtx, .units = units, .units_count = count, .at = end) ||
         EMBED_CALL(xterm_style_same, XtermCtx, .style_read = &before, .style = &after))
            ? 0ULL
            : EMBED_CALL(xterm_sgr_write, XtermCtx, .style_read = &after, .out = put_back);
    (void)EMBED_CALL(xterm_edit, XtermCtx, .xterm = xterm, .place = (CliPosition){line, from},
                     .past = (CliPosition){line, end}, .text = put_back, .size = put_back_size);
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

// a control sequence's cursor moves, its last byte in `byte` and its first number `count`, at least 1
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
        EMBED_CALL(xterm_rendition, XtermCtx, .style = &xterm->cursor.style, .numbers = numbers, .number_count = count);
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

// a hexadecimal digit's value, or 16 where `byte` is none
EMBED_INLINE unsigned int xterm_hex(const XtermCtx *args)
{
    const unsigned int digit = args->byte;
    return ((digit >= (unsigned int)'0') && (digit <= (unsigned int)'9'))   ? (digit - (unsigned int)'0')
           : ((digit >= (unsigned int)'a') && (digit <= (unsigned int)'f')) ? (digit - (unsigned int)'a' + 10u)
           : ((digit >= (unsigned int)'A') && (digit <= (unsigned int)'F')) ? (digit - (unsigned int)'A' + 10u)
                                                                            : 16u;
}

// an operating-system command read: OSC 7, the folder the shell stands in as a file URL, `file://host/path`, its path
// written into the xterm's folder with its %XX escapes read, the folder left empty where the path is empty or does not
// fit
EMBED_INLINE void xterm_command(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    const unsigned char *const body = xterm->command;
    const unsigned int size = xterm->command_size;
    if ((xterm->folder == NULL) || (size < 2u) || (body[0] != (unsigned char)'7') || (body[1] != (unsigned char)';'))
    {
        return;
    }
    // the path starts past the host where the place is a file URL, else at the place's start
    static const char s_file_scheme[] = "file://";
    unsigned int from = 2u;
    unsigned int matched = 0u;
    while ((matched < (sizeof(s_file_scheme) - 1u)) && ((from + matched) < size) &&
           (body[from + matched] == (unsigned char)s_file_scheme[matched]))
    {
        matched += 1u;
    }
    if (matched == (sizeof(s_file_scheme) - 1u))
    {
        from += matched;
        while ((from < size) && (body[from] != (unsigned char)'/'))
        {
            from += 1u;
        }
    }
    VerbaLinea folder = {xterm->folder, xterm->folder_size, 0ULL};
    for (unsigned int at = from; at < size; at += 1u)
    {
        const unsigned int high = ((at + 2u) < size) ? EMBED_CALL(xterm_hex, XtermCtx, .byte = body[at + 1u]) : 16u;
        const unsigned int low = (high < 16u) ? EMBED_CALL(xterm_hex, XtermCtx, .byte = body[at + 2u]) : 16u;
        const int escaped = (body[at] == (unsigned char)'%') && (low < 16u);
        // two hexadecimal digits are one byte, and a byte read as a char is the same byte
        mmgr_line_ch(&folder, escaped ? (char)((high * 16u) + low) : (char)body[at]);
        at += escaped ? 2u : 0u;
    }
    const int written = mmgr_line_ok(&folder) && (mmgr_line_finish(&folder) != 0ULL);
    xterm->folder[0] = written ? xterm->folder[0] : '\0';
    xterm->folder_reported = 1;
    xterm->folder_known = 1;
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
        EMBED_CALL(xterm_rows_clear, XtermCtx, .xterm = xterm, .from = 0u, .to = xterm->rows);
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

// a letter whose bytes were cut across two writes, put together, printed whole
EMBED_INLINE void xterm_letter_ended(const XtermCtx *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    EMBED_CALL(xterm_letters_put, XtermCtx, .xterm = xterm, .bytes = xterm->letter, .size = xterm->letter_size,
               .letters = 1ULL);
    xterm->letter_size = 0u;
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
        xterm->letter_size = 0u;
        EMBED_CALL(xterm_letters_put, XtermCtx, .xterm = xterm, .bytes = (const unsigned char *)s_replacement,
                   .size = sizeof(s_replacement), .letters = 1ULL);
    }
    if (xterm->letter_bytes_left > 0u)
    {
        xterm->letter[xterm->letter_size] = byte;
        xterm->letter_size += 1u;
        xterm->letter_bytes_left -= 1u;
        if (xterm->letter_bytes_left == 0u)
        {
            EMBED_CALL(xterm_letter_ended, XtermCtx, .xterm = xterm);
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
    if (byte < 0x80u)
    {
        EMBED_CALL(xterm_letters_put, XtermCtx, .xterm = xterm, .bytes = &args->byte, .size = 1ULL, .letters = 1ULL);
    }
    else if (!leads)
    {
        EMBED_CALL(xterm_letters_put, XtermCtx, .xterm = xterm, .bytes = (const unsigned char *)s_replacement,
                   .size = sizeof(s_replacement), .letters = 1ULL);
    }
    else
    {
        xterm->letter[0] = byte;
        xterm->letter_size = 1u;
        xterm->letter_bytes_left = left;
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

// how many of the `size` bytes at `bytes` from the start are letters printed as they lie: printable ASCII, a word at a
// time, and whole UTF-8 letters past it, their count into `letters_out`
EMBED_INLINE unsigned long long xterm_text_run(const XtermCtx *args)
{
    const unsigned long long ones = 0x0101010101010101ULL;
    const unsigned long long highs = 0x8080808080808080ULL;
    unsigned long long at = 0ULL;
    unsigned long long letters = 0ULL;
    for (;;)
    {
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
            letters += 8ULL;
        }
        if (at >= args->size)
        {
            break;
        }
        const unsigned char lead = args->bytes[at];
        if ((lead >= 0x20u) && (lead < 0x7fu))
        {
            at += 1ULL;
            letters += 1ULL;
            continue;
        }
        const unsigned long long length = ((lead >= 0xc2u) && (lead <= 0xdfu))   ? 2ULL
                                          : ((lead >= 0xe0u) && (lead <= 0xefu)) ? 3ULL
                                          : ((lead >= 0xf0u) && (lead <= 0xf4u)) ? 4ULL
                                                                                 : 0ULL;
        int whole = (length != 0ULL) && ((at + length) <= args->size);
        for (unsigned long long next = 1ULL; whole && (next < length); next += 1ULL)
        {
            whole = (args->bytes[at + next] & 0xc0u) == 0x80u;
        }
        if (!whole)
        {
            break;
        }
        at += length;
        letters += 1ULL;
    }
    *args->letters_out = letters;
    return at;
}

void cli_terminal_xterm_write(const CliTerminalXtermBytesCfg *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const int plain = (xterm->state == XTERM_GROUND) && (xterm->letter_bytes_left == 0u);
        unsigned long long letters = 0ULL;
        const unsigned long long run = plain ? EMBED_CALL(xterm_text_run, XtermCtx, .bytes = args->bytes + at,
                                                          .size = args->size - at, .letters_out = &letters)
                                             : 0ULL;
        if (run != 0ULL)
        {
            EMBED_CALL(xterm_letters_put, XtermCtx, .xterm = xterm, .bytes = args->bytes + at, .size = run,
                       .letters = letters);
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

int cli_terminal_xterm_init(const CliTerminalXtermSizeCfg *args)
{
    CliTerminalXterm *const xterm = args->xterm;
    memor.set(&(MemoriaCfg){.dst = xterm, .val = 0, .bytes = sizeof(*xterm)});
    unsigned char *const cell = g_cli_guard->persistent_buf_alloc(
        (unsigned long long)CLI_TERMINAL_COMMAND_MAX + CLI_TERMINAL_ANSWER_MAX, g_cli_terminal_owner);
    if (cell == NULL)
    {
        return 0;
    }
    xterm->cell = cell;
    xterm->command = cell;
    xterm->answer = cell + CLI_TERMINAL_COMMAND_MAX;
    xterm->folder = args->folder;
    xterm->folder_size = args->folder_size;
    xterm->columns = (args->columns > 1u) ? args->columns : 2u;
    xterm->rows = (args->rows > 1u) ? args->rows : 2u;
    xterm->key = args->key;
    xterm->history = args->history;
    xterm->base = args->history;
    EMBED_CALL(xterm_reset, XtermCtx, .xterm = xterm);
    return 1;
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
    const unsigned int old = xterm->rows;
    // the second screen's rows, at the text's foot, cut or put at their foot
    if (xterm->second && (rows < old))
    {
        EMBED_CALL(xterm_lines_take, XtermCtx, .xterm = xterm, .line = xterm->base + rows, .lines = old - rows);
    }
    if (xterm->second && (rows > old))
    {
        EMBED_CALL(xterm_lines_put, XtermCtx, .xterm = xterm, .line = xterm->base + old, .lines = rows - old);
    }
    // the main screen's: those its top loses join the history while the cursor drawn on it would fall off its foot
    unsigned int dropped = 0u;
    while (!xterm->second && ((old - dropped) > rows) && ((xterm->cursor.y - dropped) >= rows))
    {
        dropped += 1u;
    }
    xterm->history += dropped;
    if (rows < old)
    {
        EMBED_CALL(xterm_lines_take, XtermCtx, .xterm = xterm, .line = xterm->history + rows,
                   .lines = old - rows - dropped);
    }
    if (rows > old)
    {
        EMBED_CALL(xterm_lines_put, XtermCtx, .xterm = xterm, .line = xterm->history + old, .lines = rows - old);
    }
    xterm->cursor.y -= dropped;
    xterm->columns = columns;
    xterm->rows = rows;
    xterm->base = xterm->second ? (xterm->history + rows) : xterm->history;
    xterm->top = 0u;
    xterm->bottom = rows - 1u;
    xterm->cursor.x = (xterm->cursor.x < columns) ? xterm->cursor.x : (columns - 1u);
    xterm->cursor.y = (xterm->cursor.y < rows) ? xterm->cursor.y : (rows - 1u);
    xterm->cursor.wrap_pending = 0;
    xterm->moved = 1;
    return !xterm->refused;
}

unsigned long long cli_terminal_xterm_column_unit(const CliTerminalXtermUnitsCfg *args)
{
    return EMBED_CALL(xterm_column_start, XtermCtx, .units = args->units, .units_count = args->count,
                      .x = args->column);
}

unsigned long long cli_terminal_xterm_plain(const CliTerminalXtermPlainCfg *args)
{
    unsigned char *const bytes = args->bytes;
    unsigned long long plain_count = 0ULL;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const unsigned long long sequence =
            EMBED_CALL(xterm_sequence_units, XtermCtx, .bytes = bytes, .units_count = args->size, .at = at);
        if (sequence != 0ULL)
        {
            at += sequence;
            continue;
        }
        bytes[plain_count] = bytes[at];
        plain_count += 1ULL;
        at += 1ULL;
    }
    return plain_count;
}

unsigned int cli_terminal_xterm_spans(const CliTerminalXtermSpansCfg *args)
{
    // the line's bytes read as the xterm reads every byte, unsigned
    const unsigned char *const bytes = (const unsigned char *)args->text;
    CliTerminalStyle style = {CLI_TERMINAL_COLOR_DEFAULT, CLI_TERMINAL_COLOR_DEFAULT, 0u};
    unsigned long long plain_count = 0ULL;
    unsigned int spans = 0u;
    unsigned long long at = 0ULL;
    while ((at < args->size) && (plain_count < args->plain_capacity))
    {
        const unsigned long long sequence =
            EMBED_CALL(xterm_sequence_units, XtermCtx, .bytes = bytes, .units_count = args->size, .at = at);
        if (sequence != 0ULL)
        {
            EMBED_CALL(xterm_sgr_apply, XtermCtx, .bytes = bytes, .at = at, .size = sequence, .style = &style);
            at += sequence;
            continue;
        }
        const unsigned int index = style.foreground & 0xffu;
        const unsigned int color =
            (((style.foreground & CLI_TERMINAL_COLOR_KIND) == CLI_TERMINAL_COLOR_INDEXED) && (index < 16u))
                ? index
                : CLI_TERMINAL_SPAN_PLAIN;
        const int bold = (style.marks & CLI_TERMINAL_BOLD) != 0u;
        CliTerminalXtermSpan *const last = (spans != 0u) ? &args->spans_out[spans - 1u] : NULL;
        // a line's plain bytes are far fewer than an unsigned int counts, bounded by the capacity handed in
        const unsigned int offset = (unsigned int)plain_count;
        if ((last != NULL) && ((last->offset + last->length) == offset) && (last->color == color) &&
            (last->bold == bold))
        {
            last->length += 1u;
        }
        else if (((color != CLI_TERMINAL_SPAN_PLAIN) || bold) && (spans < args->spans_capacity))
        {
            args->spans_out[spans] = (CliTerminalXtermSpan){offset, 1u, color, bold};
            spans += 1u;
        }
        // the byte goes back as the char it was read from
        args->plain_out[plain_count] = (char)bytes[at];
        plain_count += 1ULL;
        at += 1ULL;
    }
    *args->plain_size_out = plain_count;
    return spans;
}
