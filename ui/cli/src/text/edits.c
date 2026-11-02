// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// edits.c: edits written as the page reads them, and written over a text: each place found as a byte by one walk of
// the text's lines, its places taken in the order of their lines, and the spans then written in the order of their
// bytes, those of one span in the order they were given
#include "edits.h"
#include "../command_line/cells.h"
#include "json_tree.h"

#include <stdlib.h>

// the places of the edits being written, each edit's from and then its to; their bytes; the order they are taken in,
// one cell of twice the edits' count; and the lock they are guarded by, which the sorts read them through
static CliTextPlace *s_place;
static unsigned long long *s_byte;
static unsigned int *s_order;
static CliLock s_edits_lock = CLI_LOCK_INIT;

void cli_text_place_write(const CliEditsPlaceCfg *args)
{
    mmgr_line_put(args->out, "{\"line\":");
    mmgr_line_decimal(args->out, args->place.line, 1u);
    mmgr_line_put(args->out, ",\"col\":");
    mmgr_line_decimal(args->out, args->place.col, 1u);
    mmgr_line_ch(args->out, '}');
}

void cli_text_edit_write(const CliEditsCfg *args)
{
    mmgr_line_put(args->out, "{\"from\":");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = args->edit->from, .out = args->out);
    mmgr_line_put(args->out, ",\"to\":");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = args->edit->to, .out = args->out);
    mmgr_line_put(args->out, ",\"text\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->edit->text, .length = args->edit->size);
    mmgr_line_ch(args->out, '}');
}

unsigned long long cli_text_byte_get(const CliEditsLineCfg *args)
{
    unsigned long long units = 0ull;
    for (unsigned long long at = 0ull; at < args->size;)
    {
        if (units >= args->col)
        {
            return at;
        }
        const unsigned char lead = (unsigned char)args->line[at];
        const unsigned long long letter = (lead < 0xc0u) ? 1u : (lead < 0xe0u) ? 2u : (lead < 0xf0u) ? 3u : 4u;
        units += (letter == 4u) ? 2u : 1u;
        at += letter;
    }
    return args->size;
}

// two places in the order of their lines
static int edits_by_line(const void *one, const void *other)
{
    const unsigned int left = s_place[*(const unsigned int *)one].line;
    const unsigned int right = s_place[*(const unsigned int *)other].line;
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

// two edits in the order of their spans' bytes, then of their places among the edits
static int edits_by_span(const void *one, const void *other)
{
    const unsigned int left = *(const unsigned int *)one;
    const unsigned int right = *(const unsigned int *)other;
    const unsigned long long left_from = s_byte[2u * left];
    const unsigned long long right_from = s_byte[2u * right];
    if (left_from != right_from)
    {
        return (left_from < right_from) ? -1 : 1;
    }
    const unsigned long long left_to = s_byte[(2u * left) + 1u];
    const unsigned long long right_to = s_byte[(2u * right) + 1u];
    if (left_to != right_to)
    {
        return (left_to < right_to) ? -1 : 1;
    }
    return (left < right) ? -1 : (left > right) ? 1 : 0;
}

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned int count;
} EditsCtx;
// each place's byte in `size` bytes of `text`, `count` places
EMBED_INLINE void edits_bytes(const EditsCtx *args)
{
    unsigned long long size = args->size;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        s_order[at] = at;
    }
    qsort(s_order, args->count, sizeof(s_order[0]), edits_by_line);
    unsigned int line = 0u;
    unsigned long long start = 0ull;
    int beyond = 0;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const CliTextPlace place = s_place[s_order[at]];
        while (!beyond && (line < place.line))
        {
            const char *const newline =
                (start < size)
                    ? memor.chr(&(MemoriaCfg){.src = args->text + start, .val = '\n', .bytes = (size_t)(size - start)})
                    : NULL;
            beyond = (newline == NULL);
            start = beyond ? size : (unsigned long long)(newline - args->text) + 1u;
            line += 1u;
        }
        if (beyond)
        {
            s_byte[s_order[at]] = size;
            continue;
        }
        const char *const newline =
            (start < size)
                ? memor.chr(&(MemoriaCfg){.src = args->text + start, .val = '\n', .bytes = (size_t)(size - start)})
                : NULL;
        unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - args->text) : size;
        if ((end > start) && (args->text[end - 1u] == '\r'))
        {
            end -= 1u;
        }
        s_byte[s_order[at]] = start + EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg,
                                                 .line = args->text + start, .size = end - start, .col = place.col);
    }
}

int cli_text_apply(const CliEditsOutCfg *args)
{
    if (args->count > CLI_TEXT_EDITS)
    {
        return 0;
    }
    // the bytes first on the cell's boundary, then the places, then the order
    const unsigned long long places = 2ull * args->count;
    char *const cell = g_cli_cells->temporary_buf_alloc(
        (places + 1ull) * (sizeof(*s_byte) + sizeof(*s_place) + sizeof(*s_order)), &g_cli_text_cells);
    if (cell == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_edits_lock);
    s_byte = (unsigned long long *)(void *)cell;
    s_place = (CliTextPlace *)(void *)(s_byte + places + 1ull);
    s_order = (unsigned int *)(void *)(s_place + places + 1ull);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        s_place[2u * at] = args->edits[at].from;
        s_place[(2u * at) + 1u] = args->edits[at].to;
    }
    EMBED_CALL(edits_bytes, EditsCtx, .text = args->text, .size = args->size, .count = 2u * args->count);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        s_order[at] = at;
    }
    qsort(s_order, args->count, sizeof(s_order[0]), edits_by_span);
    unsigned long long cursor = 0ull;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const unsigned int edit = s_order[at];
        const unsigned long long from = (s_byte[2u * edit] > cursor) ? s_byte[2u * edit] : cursor;
        const unsigned long long to = s_byte[(2u * edit) + 1u];
        mmgr_line_put_n(args->out, args->text + cursor, from - cursor);
        mmgr_line_put_n(args->out, args->edits[edit].text, args->edits[edit].size);
        cursor = (to > from) ? to : from;
    }
    mmgr_line_put_n(args->out, args->text + cursor, args->size - cursor);
    s_byte = NULL;
    s_place = NULL;
    s_order = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_edits_lock);
    g_cli_cells->persistent_buf_release(cell, &g_cli_text_cells);
    return 1;
}
