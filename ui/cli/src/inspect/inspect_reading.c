// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_read_state.c: the reading of one text: its tokens, lines, comments, columns and names counted from its
// letters, and every list the sources of its language read it into carved from one cell sized to those counts, taken
// with the inspections' lock and given back before it is
#include "../command_line/pool.h"
#include "inspect_internal.h"

CliInspectMax g_cli_inspect_max;

// the cell of the reading in use, NULL where none is
static void *s_read_state;

void *cli_inspect_partition(const CliInspectPartitionCtx *args)
{
    unsigned long long bytes = args->bytes;
    void *const part = (args->partition->at != NULL) ? (void *)(args->partition->at + args->partition->bytes) : NULL;
    args->partition->bytes += (bytes + 7ULL) & ~7ULL;
    return part;
}

typedef struct
{
    unsigned char byte;
    const char *text;
    unsigned long long size;
    const char *language;
    CliInspectPartition *partition;
} InspectReadingCtx;
// whether a byte goes on a name: a letter, a digit, `_` or `$`, or a byte of a letter past ASCII
EMBED_INLINE int read_state_name_byte(const InspectReadingCtx *args)
{
    return ((args->byte >= 'a') && (args->byte <= 'z')) || ((args->byte >= 'A') && (args->byte <= 'Z')) ||
           ((args->byte >= '0') && (args->byte <= '9')) || (args->byte == '_') || (args->byte == '$') ||
           (args->byte >= 0x80u);
}

// the reading's counts from `size` bytes of `text`: every token starts on a name's first byte, a number's, or another
// byte that is no space, and a reading may put one more on each line
EMBED_INLINE void read_state_count(const InspectReadingCtx *args)
{
    unsigned long long lines = 1ULL;
    unsigned long long names = 0ULL;
    unsigned long long starts = 0ULL;
    unsigned long long opens = 0ULL;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const unsigned char byte = (unsigned char)args->text[at];
        if (EMBED_CALL(read_state_name_byte, InspectReadingCtx, .byte = byte))
        {
            names += 1ULL;
            starts += 1ULL;
            while ((at < args->size) &&
                   EMBED_CALL(read_state_name_byte, InspectReadingCtx, .byte = (unsigned char)args->text[at]))
            {
                starts += ((unsigned char)args->text[at] >= 0x80u) ? 1ULL : 0ULL;
                at += 1ULL;
            }
            continue;
        }
        lines += (byte == '\n') ? 1ULL : 0ULL;
        starts += ((byte != ' ') && (byte != '\t') && (byte != '\n') && (byte != '\r')) ? 1ULL : 0ULL;
        opens += ((byte == '#') || (byte == '/')) ? 1ULL : 0ULL;
        at += 1ULL;
    }
    // a count is an unsigned int's, and a text past one's reach is read as one that fills
    const unsigned long long reach = 0x3fffffffULL;
    const unsigned long long tokens = starts + lines + 16ULL;
    g_cli_inspect_max.tokens = (unsigned int)((tokens < reach) ? tokens : reach);
    g_cli_inspect_max.lines = (unsigned int)((lines + 1ULL < reach) ? (lines + 1ULL) : reach);
    g_cli_inspect_max.comments = (unsigned int)((opens + 1ULL < reach) ? (opens + 1ULL) : reach);
    g_cli_inspect_max.columns = args->size + lines + 1ULL;
    g_cli_inspect_max.names = (unsigned int)((names + 16ULL < reach) ? (names + 16ULL) : reach);
}

// every list of a reading in `language` carved, or measured where the carving has no cell
EMBED_INLINE void read_state_layout(const InspectReadingCtx *args)
{
    CliInspectPartition *partition = args->partition;
    EMBED_CALL(cli_inspect_partition_internal.scan_layout, CliInspectPartitionCtx, .partition = partition);
    EMBED_CALL(cli_inspect_partition_internal.paint_layout, CliInspectPartitionCtx, .partition = partition);
    EMBED_CALL(cli_inspect_partition_internal.sort_layout, CliInspectPartitionCtx, .partition = partition);
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "python", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(cli_inspect_partition_internal.py_lex_layout, CliInspectPartitionCtx, .partition = partition);
        EMBED_CALL(cli_inspect_partition_internal.py_parse_layout, CliInspectPartitionCtx, .partition = partition);
        EMBED_CALL(cli_inspect_partition_internal.py_sort_layout, CliInspectPartitionCtx, .partition = partition);
        EMBED_CALL(cli_inspect_partition_internal.py_unread_layout, CliInspectPartitionCtx, .partition = partition);
    }
    else
    {
        EMBED_CALL(cli_inspect_partition_internal.js_lex_layout, CliInspectPartitionCtx, .partition = partition);
        EMBED_CALL(cli_inspect_partition_internal.js_parse_layout, CliInspectPartitionCtx, .partition = partition);
        EMBED_CALL(cli_inspect_partition_internal.js_write_layout, CliInspectPartitionCtx, .partition = partition);
        EMBED_CALL(cli_inspect_partition_internal.js_facts_layout, CliInspectPartitionCtx, .partition = partition);
    }
}

int cli_inspect_read_state_reserve(const CliInspectLanguageCtx *args)
{
    EMBED_CALL(read_state_count, InspectReadingCtx, .text = args->text_text, .size = args->size);
    CliInspectPartition measured = {NULL, 0ULL};
    EMBED_CALL(read_state_layout, InspectReadingCtx, .language = args->language, .partition = &measured);
    s_read_state = g_cli_guard->persistent_buf_alloc(measured.bytes + 8ULL, g_cli_inspect_owner);
    if (s_read_state == NULL)
    {
        return 0;
    }
    CliInspectPartition partition = {(unsigned char *)s_read_state, 0ULL};
    EMBED_CALL(read_state_layout, InspectReadingCtx, .language = args->language, .partition = &partition);
    return 1;
}

void cli_inspect_read_state_free(void)
{
    g_cli_guard->persistent_buf_release(s_read_state, g_cli_inspect_owner);
    s_read_state = NULL;
}
