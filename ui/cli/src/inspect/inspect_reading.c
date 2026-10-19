// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_reading.c: the reading of one text: its tokens, lines, comments, columns and names counted from its letters,
// and every list the sources of its language read it into carved from one cell sized to those counts, taken with the
// inspections' lock and given back before it is
#include "../command_line/cells.h"
#include "inspect_internal.h"

CliInspectMost g_cli_inspect_most;

// the cell of the reading held, NULL where none is
static void *s_reading;

void *cli_inspect_carved(CliInspectCarve *carve, unsigned long long bytes)
{
    void *const part = (carve->at != NULL) ? (void *)(carve->at + carve->bytes) : NULL;
    carve->bytes += (bytes + 7ull) & ~7ull;
    return part;
}

// whether a byte goes on a name: a letter, a digit, `_` or `$`, or a byte of a letter past ASCII
static int reading_name_byte(unsigned char byte)
{
    return ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) || ((byte >= '0') && (byte <= '9')) ||
           (byte == '_') || (byte == '$') || (byte >= 0x80u);
}

// the reading's counts from `size` bytes of `text`: every token starts on a name's first byte, a number's, or another
// byte that is no space, and a reading may put one more on each line
static void reading_counted(const char *text, unsigned long long size)
{
    unsigned long long lines = 1ull;
    unsigned long long names = 0ull;
    unsigned long long starts = 0ull;
    unsigned long long opens = 0ull;
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned char byte = (unsigned char)text[at];
        if (reading_name_byte(byte))
        {
            names += 1ull;
            starts += 1ull;
            while ((at < size) && reading_name_byte((unsigned char)text[at]))
            {
                starts += ((unsigned char)text[at] >= 0x80u) ? 1ull : 0ull;
                at += 1ull;
            }
            continue;
        }
        lines += (byte == '\n') ? 1ull : 0ull;
        starts += ((byte != ' ') && (byte != '\t') && (byte != '\n') && (byte != '\r')) ? 1ull : 0ull;
        opens += ((byte == '#') || (byte == '/')) ? 1ull : 0ull;
        at += 1ull;
    }
    // a count is an unsigned int's, and a text past one's reach is read as one that fills
    const unsigned long long reach = 0x3fffffffull;
    const unsigned long long tokens = starts + lines + 16ull;
    g_cli_inspect_most.tokens = (unsigned int)((tokens < reach) ? tokens : reach);
    g_cli_inspect_most.lines = (unsigned int)((lines + 1ull < reach) ? (lines + 1ull) : reach);
    g_cli_inspect_most.comments = (unsigned int)((opens + 1ull < reach) ? (opens + 1ull) : reach);
    g_cli_inspect_most.columns = size + lines + 1ull;
    g_cli_inspect_most.names = (unsigned int)((names + 16ull < reach) ? (names + 16ull) : reach);
}

// every list of a reading in `language` carved, or measured where the carving has no cell
static void reading_laid(const char *language, CliInspectCarve *carve)
{
    cli_inspect_scan_laid(carve);
    cli_inspect_paint_laid(carve);
    cli_inspect_sort_laid(carve);
    if (cellul.eq(&(CatenaFinitaCfg){.src = language, .other = "python", .cap = CLI_CELLS_POOL}))
    {
        cli_inspect_py_lex_laid(carve);
        cli_inspect_py_parse_laid(carve);
        cli_inspect_py_sort_laid(carve);
        cli_inspect_py_unread_laid(carve);
    }
    else
    {
        cli_inspect_js_lex_laid(carve);
        cli_inspect_js_parse_laid(carve);
        cli_inspect_js_write_laid(carve);
        cli_inspect_js_facts_laid(carve);
    }
}

int cli_inspect_reading_held(const char *language, const char *text, unsigned long long size)
{
    reading_counted(text, size);
    CliInspectCarve measured = {NULL, 0ull};
    reading_laid(language, &measured);
    s_reading = cli_cell_taken(measured.bytes + 8ull, "a reading of a text");
    if (s_reading == NULL)
    {
        return 0;
    }
    CliInspectCarve carve = {(unsigned char *)s_reading, 0ull};
    reading_laid(language, &carve);
    return 1;
}

void cli_inspect_reading_given(void)
{
    cli_cell_given(s_reading);
    s_reading = NULL;
}
