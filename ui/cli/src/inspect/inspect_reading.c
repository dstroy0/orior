// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_reading.c: the reading of one text: its tokens, lines, comments, columns and names counted from its letters,
// and every list the sources of its language read it into carved from one cell sized to those counts, taken with the
// inspections' lock and given back before it is
#include "../command_line/cells.h"
#include "inspect_internal.h"

CliInspectMost g_cli_inspect_most;

// the cell of the reading in use, NULL where none is
static void *s_reading;

void *cli_inspect_carve(const CliInspectCarveCtx *args)
{
    unsigned long long bytes = args->bytes;
    void *const part = (args->carve->at != NULL) ? (void *)(args->carve->at + args->carve->bytes) : NULL;
    args->carve->bytes += (bytes + 7ull) & ~7ull;
    return part;
}

typedef struct
{
    unsigned char byte;
    const char *text;
    unsigned long long size;
    const char *language;
    CliInspectCarve *carve;
} InspectReadingCtx;
// whether a byte goes on a name: a letter, a digit, `_` or `$`, or a byte of a letter past ASCII
EMBED_INLINE int reading_name_byte(const InspectReadingCtx *args)
{
    return ((args->byte >= 'a') && (args->byte <= 'z')) || ((args->byte >= 'A') && (args->byte <= 'Z')) ||
           ((args->byte >= '0') && (args->byte <= '9')) || (args->byte == '_') || (args->byte == '$') ||
           (args->byte >= 0x80u);
}

// the reading's counts from `size` bytes of `text`: every token starts on a name's first byte, a number's, or another
// byte that is no space, and a reading may put one more on each line
EMBED_INLINE void reading_count(const InspectReadingCtx *args)
{
    unsigned long long lines = 1ull;
    unsigned long long names = 0ull;
    unsigned long long starts = 0ull;
    unsigned long long opens = 0ull;
    for (unsigned long long at = 0ull; at < args->size;)
    {
        const unsigned char byte = (unsigned char)args->text[at];
        if (EMBED_CALL(reading_name_byte, InspectReadingCtx, .byte = byte))
        {
            names += 1ull;
            starts += 1ull;
            while ((at < args->size) &&
                   EMBED_CALL(reading_name_byte, InspectReadingCtx, .byte = (unsigned char)args->text[at]))
            {
                starts += ((unsigned char)args->text[at] >= 0x80u) ? 1ull : 0ull;
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
    g_cli_inspect_most.columns = args->size + lines + 1ull;
    g_cli_inspect_most.names = (unsigned int)((names + 16ull < reach) ? (names + 16ull) : reach);
}

// every list of a reading in `language` carved, or measured where the carving has no cell
EMBED_INLINE void reading_layout(const InspectReadingCtx *args)
{
    CliInspectCarve *carve = args->carve;
    EMBED_CALL(cli_inspect_carve_internal.scan_layout, CliInspectCarveCtx, .carve = carve);
    EMBED_CALL(cli_inspect_carve_internal.paint_layout, CliInspectCarveCtx, .carve = carve);
    EMBED_CALL(cli_inspect_carve_internal.sort_layout, CliInspectCarveCtx, .carve = carve);
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "python", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(cli_inspect_carve_internal.py_lex_layout, CliInspectCarveCtx, .carve = carve);
        EMBED_CALL(cli_inspect_carve_internal.py_parse_layout, CliInspectCarveCtx, .carve = carve);
        EMBED_CALL(cli_inspect_carve_internal.py_sort_layout, CliInspectCarveCtx, .carve = carve);
        EMBED_CALL(cli_inspect_carve_internal.py_unread_layout, CliInspectCarveCtx, .carve = carve);
    }
    else
    {
        EMBED_CALL(cli_inspect_carve_internal.js_lex_layout, CliInspectCarveCtx, .carve = carve);
        EMBED_CALL(cli_inspect_carve_internal.js_parse_layout, CliInspectCarveCtx, .carve = carve);
        EMBED_CALL(cli_inspect_carve_internal.js_write_layout, CliInspectCarveCtx, .carve = carve);
        EMBED_CALL(cli_inspect_carve_internal.js_facts_layout, CliInspectCarveCtx, .carve = carve);
    }
}

int cli_inspect_reading_reserve(const CliInspectLanguageCtx *args)
{
    EMBED_CALL(reading_count, InspectReadingCtx, .text = args->text_text, .size = args->size);
    CliInspectCarve measured = {NULL, 0ull};
    EMBED_CALL(reading_layout, InspectReadingCtx, .language = args->language, .carve = &measured);
    s_reading = g_cli_cells->persistent_buf_alloc(measured.bytes + 8ull, &g_cli_inspect_cells);
    if (s_reading == NULL)
    {
        return 0;
    }
    CliInspectCarve carve = {(unsigned char *)s_reading, 0ull};
    EMBED_CALL(reading_layout, InspectReadingCtx, .language = args->language, .carve = &carve);
    return 1;
}

void cli_inspect_reading_free(void)
{
    g_cli_cells->persistent_buf_release(s_reading, &g_cli_inspect_cells);
    s_reading = NULL;
}
