// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages.c: a file opened paged, its index read behind the page, and its lines read from the disk and from the lines
// written over it
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "pages_internal.h"

// how many bytes the index reads at a time, how many of a file's first bytes determine its end of line, and the bytes a
// reading's cell begins with for each line requested
#define CLI_PAGE_CHUNK (1ull << 20u)
#define CLI_PAGE_HEAD (64ull * 1024ull)
#define CLI_PAGE_LINE_GUESS 64ull

CliPaged *g_cli_paged = NULL;
CliLock g_cli_pages_lock = CLI_LOCK_INIT;
CliLock g_cli_index_lock = CLI_LOCK_INIT;
CliWake g_cli_index_wake = CLI_WAKE_INIT;

// a paged text's cells given back, those it has; and taken for a file of `size` bytes: 1, or 0 with none retained where
// the cells cannot meet them
static void pages_cells_given(CliPaged *one)
{
    g_cli_cells->persistent_buf_release(one->piece, NULL);
    g_cli_cells->persistent_buf_release(one->index.start, NULL);
    one->piece = NULL;
    one->index.start = NULL;
}

static int pages_cells_taken(CliPaged *one, unsigned long long size)
{
    // a file of `size` bytes has no more than size + 1 lines, so the index contains no more than size / CLI_PAGE_STEP +
    // 2 starts
    const unsigned long long starts = (size / CLI_PAGE_STEP) + 2ull;
    one->index.starts_most = (starts < CLI_PAGE_STARTS) ? starts : CLI_PAGE_STARTS;
    one->index.start = g_cli_cells->persistent_buf_alloc(one->index.starts_most * sizeof(one->index.start[0]), NULL);
    one->piece = g_cli_cells->persistent_buf_alloc(CLI_PAGE_PIECES * sizeof(one->piece[0]), NULL);
    const int taken = (one->index.start != NULL) && (one->piece != NULL);
    if (!taken)
    {
        pages_cells_given(one);
    }
    return taken;
}

// a paged text let go: taken off the texts stored, its index stopped before its cells are given back since its reader
// writes into them, its shadow removed where it has one, and its own cell given back
static void pages_let_go(CliPaged *one)
{
    CliPaged **link = &g_cli_paged;
    while ((*link != NULL) && (*link != one))
    {
        link = &(*link)->next;
    }
    if (*link == one)
    {
        *link = one->next;
    }
    cli_lock_take(&g_cli_index_lock);
    one->index.cancel = 1;
    while (one->index.reading)
    {
        cli_wake_wait(&g_cli_index_wake, &g_cli_index_lock);
    }
    cli_lock_give(&g_cli_index_lock);
    pages_cells_given(one);
    if (one->shadow_used > 0ull)
    {
        (void)query_removed(one->shadow);
    }
    g_cli_cells->persistent_buf_release(one, NULL);
}

CliPaged *cli_paged_of(const char *key, VerbaLinea *err_msg)
{
    for (CliPaged *one = g_cli_paged; one != NULL; one = one->next)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = key, .cap = CLI_CELLS_POOL}))
        {
            return one;
        }
    }
    mmgr_line_put(err_msg, "no paged text is stored under ");
    mmgr_line_put(err_msg, key);
    return NULL;
}

unsigned long long cli_page_indexed(CliPaged *one)
{
    cli_lock_take(&g_cli_index_lock);
    const unsigned long long lines = one->index.lines;
    cli_lock_give(&g_cli_index_lock);
    return lines;
}

unsigned long long cli_page_length(const CliPagePiece *piece, unsigned long long indexed)
{
    if (piece->written)
    {
        return piece->lines;
    }
    const unsigned long long to = piece->open ? indexed : piece->to;
    return (to > piece->from) ? (to - piece->from) : 0ull;
}

// the index of a file's lines read, a chunk at a time, each chunk's starts put in the index as it is read, until the
// file ends or the reading is to stop
static void cli_index_read(void *context)
{
    CliPageIndex *const index = (CliPageIndex *)context;
    unsigned long long size = 0ull;
    const int sized = cli_file_size(index->path, &size);
    unsigned long long lines = 1ull;
    int stop = 0;
    // a chunk at a time stored in a cell, its newlines found where it lies; a line that runs over a chunk's end is
    // counted at its newline in the chunk after
    for (unsigned long long offset = 0ull; sized && !stop && (offset < size); offset += CLI_PAGE_CHUNK)
    {
        unsigned long long len = 0ull;
        const unsigned char *const chunk = cli_file_piece_read(index->path, offset, CLI_PAGE_CHUNK, &len);
        const unsigned long long end = offset + len;
        cli_lock_take(&g_cli_index_lock);
        for (unsigned long long at = offset; (chunk != NULL) && (at < end);)
        {
            const unsigned char *const newline =
                memor.chr(&(MemoriaCfg){.src = chunk + (at - offset), .val = '\n', .bytes = end - at});
            if (newline == NULL)
            {
                break;
            }
            // a newline found lies in the chunk, at or past `at`
            at = offset + (unsigned long long)(newline - chunk) + 1ull;
            if (((lines % CLI_PAGE_STEP) == 0ull) && (index->starts < index->starts_most))
            {
                index->start[index->starts] = at;
                index->starts += 1ull;
            }
            lines += 1ull;
        }
        index->lines = lines;
        stop = index->cancel || (chunk == NULL) || (len == 0ull);
        cli_lock_give(&g_cli_index_lock);
        cli_file_let_go(chunk, len);
    }
    cli_lock_take(&g_cli_index_lock);
    index->lines = lines;
    index->done = 1;
    index->reading = 0;
    cli_wake_all(&g_cli_index_wake);
    cli_lock_give(&g_cli_index_lock);
}

// the end of line a file's first line ends with: "\r\n" where it does, and "\n" otherwise
static const char *cli_eol_of(const char *path)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_piece_read(path, 0ull, CLI_PAGE_HEAD, &size);
    const unsigned char *const newline =
        (bytes != NULL) ? memor.chr(&(MemoriaCfg){.src = bytes, .val = '\n', .bytes = size}) : NULL;
    const int crlf = (newline != NULL) && (newline > bytes) && (newline[-1] == '\r');
    cli_file_let_go(bytes, size);
    return crlf ? "\r\n" : "\n";
}

void cli_page_shape_written(CliPaged *one, VerbaLinea *out)
{
    cli_lock_take(&g_cli_index_lock);
    const unsigned long long indexed = one->index.lines;
    const int done = one->index.done;
    cli_lock_give(&g_cli_index_lock);
    unsigned long long lines = 0ull;
    for (unsigned int at = 0u; at < one->pieces; at += 1u)
    {
        lines += cli_page_length(&one->piece[at], indexed);
    }
    mmgr_line_put(out, "{\"lines\":");
    mmgr_line_decimal(out, lines, 0u);
    mmgr_line_put(out, done ? ",\"indexed\":true,\"size\":" : ",\"indexed\":false,\"size\":");
    mmgr_line_decimal(out, one->size, 0u);
    mmgr_line_ch(out, '}');
}

int cli_page_open_reading(const char *key, const char *path, int behind, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned long long size = 0ull;
    if (!cli_file_size(path, &size))
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    // a text stored under the key already is let go first, its index stopped and its shadow removed
    CliPaged *one = g_cli_paged;
    while ((one != NULL) && (!cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = key, .cap = CLI_CELLS_POOL})))
    {
        one = one->next;
    }
    if (one != NULL)
    {
        pages_let_go(one);
    }
    one = g_cli_cells->persistent_buf_alloc(sizeof(*one), NULL);
    if (one == NULL)
    {
        mmgr_line_put(err_msg, "the paged text's cells could not be taken");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
    if (!pages_cells_taken(one, size))
    {
        g_cli_cells->persistent_buf_release(one, NULL);
        mmgr_line_put(err_msg, "the paged text's cells could not be taken");
        return 0;
    }
    one->index.start[0] = 0ull;
    one->index.starts = 1ull;
    one->index.lines = 1ull;
    one->index.reading = 1;
    memor.cpy(&(MemoriaCfg){.dst = one->index.path,
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = one->key, .src = key, .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = one->path,
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    cli_page_beside(path, CLI_PAGE_SHADOW, one->shadow);
    one->size = size;
    one->eol = cli_eol_of(path);
    one->piece[0] = (CliPagePiece){0, 0ull, 0ull, 1, 0ull};
    one->pieces = 1u;
    one->next = g_cli_paged;
    g_cli_paged = one;
    one->index.reader = (CliThread){cli_index_read, &one->index};
    if (!behind || !cli_thread_started(&one->index.reader))
    {
        cli_index_read(&one->index);
    }
    cli_page_shape_written(one, out);
    return 1;
}

int cli_pages_open(const char *key, const char *path, VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_pages_lock);
    const int opened = cli_page_open_reading(key, path, 1, out, err_msg);
    cli_lock_give(&g_cli_pages_lock);
    return opened;
}

int cli_pages_shape(const char *key, VerbaLinea *out, VerbaLinea *err_msg)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, err_msg);
    if (one != NULL)
    {
        cli_page_shape_written(one, out);
    }
    cli_lock_give(&g_cli_pages_lock);
    return one != NULL;
}

// the file's lines from `from` to before `to`, read from the disk from the nearest line the index records before
// `from`, put after the lines read, as many as requested, empty past the file's end: 1, or 0 with why given
static int cli_file_lines(CliPaged *one, unsigned long long from, unsigned long long to, CliPageLines *lines,
                          VerbaLinea *err_msg)
{
    if (from >= to)
    {
        return 1;
    }
    cli_lock_take(&g_cli_index_lock);
    const unsigned long long step = from / CLI_PAGE_STEP;
    const int indexed = step < one->index.starts;
    const unsigned long long start = indexed ? one->index.start[step] : 0ull;
    cli_lock_give(&g_cli_index_lock);
    if (!indexed)
    {
        mmgr_line_put(err_msg, "the index has not read that far yet");
        return 0;
    }
    // The lines from the step's start stored in a cell: a window of the file from there, doubled while it contains
    // neither the newlines the lines requested end at nor the file's end
    unsigned long long whole = 0ull;
    if (!cli_file_size(one->path, &whole))
    {
        mmgr_line_put(err_msg, "it could not be read");
        return 0;
    }
    const unsigned long long begin = (start < whole) ? start : whole;
    const unsigned long long wanted = to - (step * CLI_PAGE_STEP);
    unsigned long long window = CLI_PAGE_CHUNK;
    unsigned long long size = 0ull;
    const unsigned char *bytes = NULL;
    for (;;)
    {
        bytes = cli_file_piece_read(one->path, begin, window, &size);
        if (bytes == NULL)
        {
            mmgr_line_put(err_msg, "it could not be read");
            return 0;
        }
        unsigned long long newlines = 0ull;
        for (unsigned long long at = 0ull; (at < size) && (newlines < wanted);)
        {
            const unsigned char *const newline =
                memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = size - at});
            if (newline == NULL)
            {
                break;
            }
            // a newline found lies in the window, at or past `at`
            at = (unsigned long long)(newline - bytes) + 1ull;
            newlines += 1ull;
        }
        if (((begin + size) >= whole) || (newlines >= wanted))
        {
            break;
        }
        cli_file_let_go(bytes, size);
        window *= 2ull;
    }
    // the lines from the step's start, those from `from` on put after the lines read
    const unsigned long long first = lines->count;
    unsigned long long at = 0ull;
    for (unsigned long long line = step * CLI_PAGE_STEP; line < to; line += 1ull)
    {
        const unsigned char *const newline =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = size - at}) : NULL;
        // a newline found lies in the window, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
        unsigned long long length = end - at;
        length -= ((newline != NULL) && (length > 0ull) && (bytes[end - 1ull] == '\r')) ? 1ull : 0ull;
        const unsigned long long read = (newline != NULL) ? (end - at + 1ull) : (size - at);
        // the line read as UTF-8 straight after the lines read, with room for its own bytes; a byte that starts no
        // whole letter is written as U+FFFD, three bytes, and a line those do not fit is written again with three
        // bytes of room for each of its own
        if ((line >= from) && !lines->full && cli_page_lines_room(lines, 1ull, length))
        {
            VerbaLinea lossy = {lines->text + lines->used, lines->text_most - lines->used, 0ull};
            cli_lossy(&lossy, bytes + at, length);
            if (!mmgr_line_ok(&lossy) && cli_page_lines_room(lines, 1ull, 3ull * length))
            {
                lossy = (VerbaLinea){lines->text + lines->used, lines->text_most - lines->used, 0ull};
                cli_lossy(&lossy, bytes + at, length);
            }
            lines->full = lines->full || !mmgr_line_ok(&lossy);
            lines->start[lines->count] = lines->used;
            lines->length[lines->count] = mmgr_line_ok(&lossy) ? lossy.at : 0ull;
            lines->used += lines->length[lines->count];
            lines->count += 1ull;
        }
        at = (newline != NULL) ? (end + 1ull) : size;
        if (read == 0ull)
        {
            break;
        }
    }
    cli_file_let_go(bytes, size);
    while (((lines->count - first) < (to - from)) && !lines->full)
    {
        cli_page_lines_put(lines, "", 0ull);
    }
    return 1;
}

// `count` lines of a written piece from its line `skip`, read from the shadow and put after the lines read: 1, or 0
// with why given
static int cli_written_lines(const CliPaged *one, const CliPagePiece *piece, unsigned long long skip,
                             unsigned long long count, CliPageLines *lines, VerbaLinea *err_msg)
{
    // the piece stored in a cell, from its start, its bytes counted from there
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_piece_read(one->shadow, piece->from, piece->to - piece->from, &size);
    if ((bytes == NULL) || (size < (piece->to - piece->from)))
    {
        cli_file_let_go(bytes, size);
        mmgr_line_put(err_msg, one->shadow);
        mmgr_line_put(err_msg, ": it could not be read");
        return 0;
    }
    unsigned long long at = 0ull;
    for (unsigned long long line = 0ull; line < (skip + count); line += 1ull)
    {
        const unsigned char *const newline =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = size - at}) : NULL;
        // a newline found lies in the piece, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
        if (line >= skip)
        {
            cli_page_lines_put(lines, (const char *)bytes + at, end - at);
        }
        at = (end < size) ? (end + 1ull) : size;
    }
    cli_file_let_go(bytes, size);
    return 1;
}

int cli_page_lines_of(CliPaged *one, unsigned long long from, unsigned long long to, CliPageLines *lines,
                      VerbaLinea *err_msg)
{
    cli_page_lines_emptied(lines);
    const unsigned long long indexed = cli_page_indexed(one);
    unsigned long long at = 0ull;
    for (unsigned int index = 0u; index < one->pieces; index += 1u)
    {
        const CliPagePiece *const piece = &one->piece[index];
        const unsigned long long count = cli_page_length(piece, indexed);
        const unsigned long long start = (from > at) ? from : at;
        const unsigned long long end = (to < (at + count)) ? to : (at + count);
        if (start < end)
        {
            if (!piece->written)
            {
                if (!cli_file_lines(one, piece->from + start - at, piece->from + end - at, lines, err_msg))
                {
                    return 0;
                }
            }
            else if (!cli_written_lines(one, piece, start - at, end - start, lines, err_msg))
            {
                return 0;
            }
        }
        at += count;
        if (at >= to)
        {
            break;
        }
    }
    return 1;
}

int cli_pages_read(const char *key, unsigned long long from, unsigned long long to, VerbaLinea *out,
                   VerbaLinea *err_msg)
{
    // the index read as far as `to`, or whole, the text found again each turn since it may be let go meanwhile
    for (;;)
    {
        cli_lock_take(&g_cli_pages_lock);
        CliPaged *const one = cli_paged_of(key, err_msg);
        int far = 0;
        if (one != NULL)
        {
            cli_lock_take(&g_cli_index_lock);
            far = (one->index.lines >= to) || one->index.done;
            cli_lock_give(&g_cli_index_lock);
        }
        cli_lock_give(&g_cli_pages_lock);
        if (one == NULL)
        {
            return 0;
        }
        if (far)
        {
            break;
        }
        query_paused(10u);
    }
    // the lines' cell begins at the lines requested and CLI_PAGE_LINE_GUESS bytes for each, and grows to what is read
    const unsigned long long wanted = (to > from) ? (to - from) : 1ull;
    CliPageLines lines;
    if (!cli_page_lines_reserved(&lines, wanted, wanted * CLI_PAGE_LINE_GUESS, err_msg))
    {
        return 0;
    }
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, err_msg);
    const int read = (one != NULL) && cli_page_lines_of(one, from, to, &lines, err_msg);
    if (read)
    {
        mmgr_line_ch(out, '[');
        for (unsigned long long at = 0ull; at < lines.count; at += 1ull)
        {
            mmgr_line_put(out, (at != 0ull) ? "," : "");
            cli_json_quoted(out, lines.text + lines.start[at], lines.length[at]);
        }
        mmgr_line_ch(out, ']');
    }
    cli_lock_give(&g_cli_pages_lock);
    cli_page_lines_given(&lines);
    return read;
}

void cli_pages_release(const char *key)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *one = g_cli_paged;
    while ((one != NULL) && (!cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = key, .cap = CLI_CELLS_POOL})))
    {
        one = one->next;
    }
    if (one != NULL)
    {
        pages_let_go(one);
    }
    cli_lock_give(&g_cli_pages_lock);
}
