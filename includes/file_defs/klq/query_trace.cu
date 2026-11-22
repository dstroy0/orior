// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_trace.cu: the trace's flags read from their table, and a word of them written back by name
#include "../../../src/platform/platform.h"
#include "../../parsers/tree_folder.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "memoria_operor/memoria_linea.h"
#include "query_trace.h"

int query_trace_path(char *path, size_t bytes)
{
    return tree_folder_from(__FILE__, 1u, "klq/query_trace.tsv", path, bytes);
}

// the `length` bytes at `cell` copied into `out`, `bytes` of it, terminated: 1 where they fit
static int query_trace_copied(const char *cell, size_t length, char *out, size_t bytes)
{
    VerbaLinea line = mmgr_line_over(out, bytes);
    (void)mmgr_line_put_n(&line, cell, length);
    return (mmgr_line_finish(&line) == length) && (mmgr_line_ok(&line) != 0);
}

// the place of `character` in the `length` bytes at `text`, or `length` where it is not there
static size_t query_trace_find(const char *text, size_t length, char character)
{
    const char *const found = mmgr_text_char(text, (uint8_t)character, length);
    // a byte found lies inside the text, at or past its start
    return ((found != NULL) && (found < (text + length))) ? (size_t)(found - text) : length;
}

// One line of the table, the `length` bytes at `line`, read onto the end of `trace`. 1, or 0 where it gives no flag
// of one bit, one two names share, or a name or gloss past its bytes
static int query_trace_line(const char *line, size_t length, unsigned long long *seen_bits, QueryTrace *trace)
{
    const size_t name_at = query_trace_find(line, length, '\t');
    const size_t name_length =
        (name_at < length) ? query_trace_find(&line[name_at + 1u], length - name_at - 1u, '\t') : 0u;
    const size_t gloss_at = name_at + 1u + name_length;
    char flag_text[32];
    QueryTraceFlag *const made = &trace->flags[(trace->count < QUERY_TRACE_FLAGS_MAX) ? trace->count : 0u];
    const int cut =
        (name_at < length) && (trace->count < QUERY_TRACE_FLAGS_MAX) &&
        query_trace_copied(line, name_at, flag_text, sizeof(flag_text)) &&
        query_trace_copied(&line[name_at + 1u], name_length, made->name, sizeof(made->name)) &&
        query_trace_copied((gloss_at < length) ? &line[gloss_at + 1u] : "",
                           (gloss_at < length) ? (length - gloss_at - 1u) : 0u, made->gloss, sizeof(made->gloss));
    const unsigned long long word = (cut != 0) ? (unsigned long long)mmgr_text_unsigned_in(flag_text, NULL, 16u) : 0ULL;
    const int fresh = (cut != 0) && (made->name[0] != '\0') && (query_trace_flag(trace, made->name) == 0ULL);
    if ((fresh == 0) || (word == 0ULL) || ((word & (word - 1ULL)) != 0ULL) || ((*seen_bits & word) != 0ULL))
    {
        return 0;
    }
    *seen_bits |= word;
    made->word = word;
    trace->count += 1u;
    return 1;
}

int query_trace_read(const char *path, QueryTrace *trace, VerbaLinea *error)
{
    trace->count = 0u;
    unsigned long long size = 0ULL;
    char *const cell = (platform_file_size(path, &size) != 0)
                           ? (char *)platform_cell_taken(size + 1ULL, platform_place_g_program_owner, 1)
                           : NULL;
    unsigned long long read = 0ULL;
    if ((cell == NULL) || (platform_file_read(path, cell, size, &read) == 0))
    {
        platform_cell_released(cell, platform_place_g_program_owner);
        mmgr_line_put(error, path);
        mmgr_line_put(error, " could not be opened");
        return 0;
    }
    cell[read] = '\0';
    unsigned int number = 0u;
    unsigned long long seen_bits = 0ULL;
    size_t at = 0u;
    int ok = 1;
    while ((ok != 0) && (at < read))
    {
        const size_t end = at + query_trace_find(&cell[at], (size_t)read - at, '\n');
        number += 1u;
        // a line's carriage return before its newline is not of the line
        const size_t length = ((end > at) && (cell[end - 1u] == '\r')) ? (end - at - 1u) : (end - at);
        const int skipped = (number == 1u) || (length == 0u) || (cell[at] == '#');
        ok = skipped || query_trace_line(&cell[at], length, &seen_bits, trace);
        at = end + 1u;
    }
    platform_cell_released(cell, platform_place_g_program_owner);
    if (ok == 0)
    {
        mmgr_line_put(error, path);
        mmgr_line_ch(error, ':');
        mmgr_line_decimal(error, number, 1u);
        mmgr_line_put(error, ": a flag of more than one bit, or one bit or one name twice");
        return 0;
    }
    return 1;
}

unsigned long long query_trace_flag(const QueryTrace *trace, const char *name)
{
    for (unsigned int at = 0u; at < trace->count; at += 1u)
    {
        if (mmgr_text_order(trace->flags[at].name, name, PLATFORM_TEXT_TO_TERMINATOR) == 0)
        {
            return trace->flags[at].word;
        }
    }
    return 0ULL;
}

void query_trace_names(const QueryTrace *trace, unsigned long long word, VerbaLinea *names)
{
    int first = 1;
    for (unsigned int at = 0u; at < trace->count; at += 1u)
    {
        if ((word & trace->flags[at].word) != 0ULL)
        {
            mmgr_line_put(names, (first != 0) ? "" : " ");
            mmgr_line_put(names, trace->flags[at].name);
            first = 0;
        }
    }
}
