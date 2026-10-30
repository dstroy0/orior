// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages.c: a file opened paged, its index read behind the page, and its lines read from the disk and from the lines
// written over it, for the page and for the walk of its colors
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
    g_cli_cells->persistent_buf_release(one->piece, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(one->index.start, &g_cli_editor_cells);
    g_cli_cells->persistent_buf_release(one->index.line, &g_cli_editor_cells);
    one->piece = NULL;
    one->index.start = NULL;
    one->index.line = NULL;
}

static int pages_cells_taken(CliPaged *one, unsigned long long size)
{
    // a file of `size` bytes has no more than size + 1 lines, and the index contains no more than size / CLI_PAGE_STEP
    // starts recorded by their lines, size / CLI_PAGE_SPAN by their bytes, and the first
    const unsigned long long starts = (size / CLI_PAGE_STEP) + (size / CLI_PAGE_SPAN) + 2ull;
    one->index.starts_most = (starts < CLI_PAGE_STARTS) ? starts : CLI_PAGE_STARTS;
    one->index.start =
        g_cli_cells->persistent_buf_alloc(one->index.starts_most * sizeof(one->index.start[0]), &g_cli_editor_cells);
    one->index.line =
        g_cli_cells->persistent_buf_alloc(one->index.starts_most * sizeof(one->index.line[0]), &g_cli_editor_cells);
    one->piece = g_cli_cells->persistent_buf_alloc(CLI_PAGE_PIECES * sizeof(one->piece[0]), &g_cli_editor_cells);
    const int taken = (one->index.start != NULL) && (one->index.line != NULL) && (one->piece != NULL);
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
    g_cli_cells->persistent_buf_release(one, &g_cli_editor_cells);
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
            // the line starting here recorded CLI_PAGE_STEP lines or CLI_PAGE_SPAN bytes after the last recorded
            const unsigned long long last = index->starts - 1ull;
            const int far =
                ((lines - index->line[last]) >= CLI_PAGE_STEP) || ((at - index->start[last]) >= CLI_PAGE_SPAN);
            if (far && (index->starts < index->starts_most))
            {
                index->start[index->starts] = at;
                index->line[index->starts] = lines;
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

// the text's lines in all as far as the index has read, and whether the index is whole into `done`
static unsigned long long pages_total(CliPaged *one, int *done)
{
    cli_lock_take(&g_cli_index_lock);
    const unsigned long long indexed = one->index.lines;
    *done = one->index.done;
    cli_lock_give(&g_cli_index_lock);
    unsigned long long lines = 0ull;
    for (unsigned int at = 0u; at < one->pieces; at += 1u)
    {
        lines += cli_page_length(&one->piece[at], indexed);
    }
    return lines;
}

void cli_page_shape_written(CliPaged *one, VerbaLinea *out)
{
    int done = 0;
    const unsigned long long lines = pages_total(one, &done);
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
    one = g_cli_cells->persistent_buf_alloc(sizeof(*one), &g_cli_editor_cells);
    if (one == NULL)
    {
        mmgr_line_put(err_msg, "the paged text's cells could not be taken");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
    if (!pages_cells_taken(one, size))
    {
        g_cli_cells->persistent_buf_release(one, &g_cli_editor_cells);
        mmgr_line_put(err_msg, "the paged text's cells could not be taken");
        return 0;
    }
    one->index.start[0] = 0ull;
    one->index.line[0] = 0ull;
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
    // the last start the index records at or before `from`, and the bytes a line takes there, as far as the next start
    // recorded says
    cli_lock_take(&g_cli_index_lock);
    const CliPageIndex *const index = &one->index;
    unsigned long long low = 0ull;
    unsigned long long high = index->starts;
    while ((high - low) > 1ull)
    {
        const unsigned long long middle = low + ((high - low) / 2ull);
        if (index->line[middle] <= from)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    const int indexed = index->starts != 0ull;
    const unsigned long long start = indexed ? index->start[low] : 0ull;
    const unsigned long long first_line = indexed ? index->line[low] : 0ull;
    const unsigned long long each = ((low + 1ull) < index->starts)
                                        ? ((index->start[low + 1ull] - start) / (index->line[low + 1ull] - first_line))
                                        : 0ull;
    cli_lock_give(&g_cli_index_lock);
    if (!indexed)
    {
        mmgr_line_put(err_msg, "the index has not read that far yet");
        return 0;
    }
    // The lines from the start stored in a cell: a window of the file from there, as many bytes as the lines requested
    // take where the index says how long its lines run and CLI_PAGE_CHUNK where it does not, doubled while it contains
    // neither the newlines the lines requested end at nor the file's end
    unsigned long long whole = 0ull;
    if (!cli_file_size(one->path, &whole))
    {
        mmgr_line_put(err_msg, "it could not be read");
        return 0;
    }
    const unsigned long long begin = (start < whole) ? start : whole;
    const unsigned long long wanted = to - first_line;
    const unsigned long long guessed = (each != 0ull) ? (((wanted + 1ull) * each) + CLI_PAGE_HEAD) : CLI_PAGE_CHUNK;
    unsigned long long window = (guessed > CLI_PAGE_HEAD) ? guessed : CLI_PAGE_HEAD;
    unsigned long long size = 0ull;
    const unsigned char *bytes = cli_file_piece_read(one->path, begin, window, &size);
    for (;;)
    {
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
        // the window doubled where its cell stands, the bytes past those read put after them
        window *= 2ull;
        bytes = cli_file_piece_grown(one->path, bytes, begin, window, &size);
    }
    // the lines from the start, those from `from` on put after the lines read
    const unsigned long long first = lines->count;
    unsigned long long at = 0ull;
    for (unsigned long long line = first_line; line < to; line += 1ull)
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

// the bytes of the first `cut` letters of the `size` bytes of UTF-8 at `text`, all of them where `cut` is 0 or they
// hold no more; and the length of all of them in UTF-16's units into `units`
static unsigned long long pages_cut_bytes(const char *text, unsigned long long size, unsigned long long cut,
                                          unsigned long long *units)
{
    unsigned long long letters = 0ull;
    unsigned long long bytes = size;
    unsigned long long counted = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        const unsigned char byte = (unsigned char)text[at];
        // a byte that continues a letter starts none, and a letter of four bytes is two of UTF-16's units
        if ((byte & 0xc0u) != 0x80u)
        {
            bytes = ((cut != 0ull) && (letters == cut) && (bytes == size)) ? at : bytes;
            letters += 1ull;
            counted += (byte >= 0xf0u) ? 2ull : 1ull;
        }
    }
    *units = counted;
    return bytes;
}

int cli_pages_read(const char *key, unsigned long long from, unsigned long long to, unsigned long long *cut,
                   VerbaLinea *out, VerbaLinea *lengths, VerbaLinea *err_msg)
{
    const unsigned long long letters = (cut != NULL) ? *cut : 0ull;
    int any = 0;
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
        if (lengths != NULL)
        {
            mmgr_line_ch(lengths, '[');
        }
        for (unsigned long long at = 0ull; at < lines.count; at += 1ull)
        {
            unsigned long long units = 0ull;
            const char *const text = lines.text + lines.start[at];
            const unsigned long long bytes = pages_cut_bytes(text, lines.length[at], letters, &units);
            any = any || (bytes < lines.length[at]);
            mmgr_line_put(out, (at != 0ull) ? "," : "");
            cli_json_quoted(out, text, bytes);
            if (lengths != NULL)
            {
                mmgr_line_put(lengths, (at != 0ull) ? "," : "");
                mmgr_line_decimal(lengths, units, 1u);
            }
        }
        mmgr_line_ch(out, ']');
        if (lengths != NULL)
        {
            mmgr_line_ch(lengths, ']');
        }
        if (cut != NULL)
        {
            *cut = any ? letters : 0ull;
        }
    }
    cli_lock_give(&g_cli_pages_lock);
    cli_page_lines_given(&lines);
    return read;
}

int cli_pages_counted(const char *key, unsigned long long *lines, int *whole)
{
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, &err_msg);
    if (one != NULL)
    {
        *lines = pages_total(one, whole);
    }
    cli_lock_give(&g_cli_pages_lock);
    return one != NULL;
}

char *cli_pages_lines_taken(const char *key, unsigned long long from, unsigned long long to, const char **line,
                            unsigned long long *count, CellOwner *owner)
{
    *count = 0ull;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned long long wanted = (to > from) ? (to - from) : 1ull;
    CliPageLines lines;
    if (!cli_page_lines_reserved(&lines, wanted, wanted * CLI_PAGE_LINE_GUESS, &err_msg))
    {
        return NULL;
    }
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, &err_msg);
    const int read = (one != NULL) && cli_page_lines_of(one, from, to, &lines, &err_msg) && !lines.full;
    cli_lock_give(&g_cli_pages_lock);
    // each line's bytes and its NUL
    char *const text = read ? g_cli_cells->persistent_buf_alloc(lines.used + lines.count + 1ull, owner) : NULL;
    const unsigned long long taken = (lines.count < wanted) ? lines.count : wanted;
    unsigned long long at = 0ull;
    for (unsigned long long each = 0ull; (text != NULL) && (each < taken); each += 1ull)
    {
        line[each] = text + at;
        memor.cpy(&(MemoriaCfg){.dst = text + at, .src = lines.text + lines.start[each], .bytes = lines.length[each]});
        at += lines.length[each];
        text[at] = '\0';
        at += 1ull;
    }
    *count = (text != NULL) ? taken : 0ull;
    cli_page_lines_given(&lines);
    return text;
}

// the lesser of two lines
static unsigned long long pages_least(unsigned long long one, unsigned long long other)
{
    return (one < other) ? one : other;
}

// the greater of two lines
static unsigned long long pages_most(unsigned long long one, unsigned long long other)
{
    return (one > other) ? one : other;
}

// the lines a step of `one`, a text of `total` lines, reads: as many as REST_BYTES reach at the text's bytes a line,
// its size over its lines, a line cut after `cut` letters counting no more than those, from REST_LEAST to half of KEPT
static unsigned long long pages_step(const CliPaged *one, unsigned long long total, unsigned long long cut)
{
    const unsigned long long whole = (total != 0ull) ? ((one->size / total) + 1ull) : 1ull;
    const unsigned long long each = ((cut != 0ull) && ((cut + 1ull) < whole)) ? (cut + 1ull) : whole;
    return pages_least(pages_most(CLI_PAGE_REST_BYTES / each, CLI_PAGE_REST_LEAST), CLI_PAGE_KEPT / 2ull);
}

// the rows the line `at` of `lines` takes in a view that wraps at `width` letters: as many as its UTF-16 units fill,
// one at the least
static unsigned long long pages_rows_of(const CliPageLines *lines, unsigned long long at, unsigned long long width)
{
    unsigned long long units = 0ull;
    (void)pages_cut_bytes(lines->text + lines->start[at], lines->length[at], 0ull, &units);
    return (units > width) ? ((units + width - 1ull) / width) : 1ull;
}

// the lines of `one` from `from` to before `to` read into `lines`, reserved for them first: 1 where every one was read
// whole, else 0, `lines` to be given back either way; under the pages' lock
static int pages_placed_read(CliPaged *one, unsigned long long from, unsigned long long to, CliPageLines *lines)
{
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const unsigned long long wanted = to - from;
    lines->cell = NULL;
    return cli_page_lines_reserved(lines, wanted, wanted * CLI_PAGE_LINE_GUESS, &err_msg) &&
           cli_page_lines_of(one, from, to, lines, &err_msg) && !lines->full && (lines->count == wanted);
}

// the lines of `one` from `*from` to before `*to`, placed about the line `pivot` for a view that wraps at `width`
// letters, trimmed to those its rows take: as many on either side of `pivot` as fill the rows placed on that side, a
// line as many rows as its letters fill. As placed where the view does not wrap, or the lines cannot be read whole;
// under the pages' lock
static void pages_rows_trimmed(CliPaged *one, unsigned long long width, unsigned long long pivot,
                               unsigned long long *from, unsigned long long *to)
{
    const int about = (width != 0ull) && (pivot >= *from) && (pivot < *to);
    CliPageLines lines;
    if (about && pages_placed_read(one, *from, *to, &lines))
    {
        unsigned long long first = pivot;
        for (unsigned long long rows = 0ull; (first > *from) && (rows < (pivot - *from)); first -= 1ull)
        {
            rows += pages_rows_of(&lines, first - 1ull - *from, width);
        }
        unsigned long long end = pivot;
        for (unsigned long long rows = 0ull; (end < *to) && (rows < (*to - pivot)); end += 1ull)
        {
            rows += pages_rows_of(&lines, end - *from, width);
        }
        *from = first;
        *to = end;
    }
    if (about)
    {
        cli_page_lines_given(&lines);
    }
}

// the lines of `one` from `*from` to before `*to`, a step toward the ends of the page's lines below them where `below`
// is 1 and above where 0, trimmed to those from the page's side that CLI_PAGE_REST_BYTES of their bytes reach, a line
// cut after `cut` letters counting no more than those, and where the view moves and wraps at `width` letters, that fill
// the rows placed, a line as many rows as its letters fill; one line at the least. As placed where the lines cannot be
// read whole; under the pages' lock
static void pages_step_trimmed(CliPaged *one, int below, int moving, unsigned long long cut, unsigned long long width,
                               unsigned long long *from, unsigned long long *to)
{
    CliPageLines lines;
    const unsigned long long wanted = *to - *from;
    if ((wanted != 0ull) && pages_placed_read(one, *from, *to, &lines))
    {
        const int by_rows = moving && (width != 0ull);
        unsigned long long taken = 0ull;
        unsigned long long bytes = 0ull;
        unsigned long long rows = 0ull;
        while ((taken < wanted) &&
               ((taken == 0ull) || ((bytes < CLI_PAGE_REST_BYTES) && (!by_rows || (rows < wanted)))))
        {
            const unsigned long long at = below ? taken : (wanted - 1ull - taken);
            bytes += ((cut != 0ull) && (cut < lines.length[at])) ? cut : lines.length[at];
            rows += by_rows ? pages_rows_of(&lines, at, width) : 1ull;
            taken += 1ull;
        }
        *to = below ? (*from + taken) : *to;
        *from = below ? *from : (*to - taken);
    }
    if (wanted != 0ull)
    {
        cli_page_lines_given(&lines);
    }
}

// a moving view of `one`, reported at `last` before, placed in a text of `total` lines, cut at its reach. Where the
// page's lines meet the rows it needs, its rows where it stands and where it will stand a report later as far on the
// way it went as it went since `last` where the report follows it, EDGE and a SPREADth of that step about them, no more
// than the lines a step reads on either side: a step of lines past the page's on the side it goes once those rows come
// within NEAR lines of their end, or past their end; else nothing. Where they do not meet, as a view that jumped away,
// those rows alone, only those of where it will stand where it went farther than its rows. It keeps half of KEPT lines
// on either side of its rows and every line it is answered. A view that wraps is answered the lines its rows take of
// those about them, and a step as many lines as fill its rows and CLI_PAGE_REST_BYTES of their bytes reach
static CliPagesPlaced pages_move_placed(CliPaged *one, const CliPagesView *view, unsigned long long last, int follows,
                                        unsigned long long total)
{
    const unsigned long long top = pages_least(view->top, (total != 0ull) ? (total - 1ull) : 0ull);
    const unsigned long long down = (follows && (top > last)) ? (top - last) : 0ull;
    const unsigned long long up = (follows && (top < last)) ? (last - top) : 0ull;
    const unsigned long long ahead = (down != 0ull) ? (top + down) : ((top > up) ? (top - up) : 0ull);
    const int far = (down + up) > view->rows;
    const unsigned long long low = (far || (ahead < top)) ? ahead : top;
    const unsigned long long high = (far || (ahead > top)) ? ahead : top;
    const unsigned long long step = pages_step(one, total, view->columns);
    const unsigned long long spread = CLI_PAGE_EDGE + pages_least((down + up) / CLI_PAGE_SPREAD, step);
    const unsigned long long first = pages_least((low > spread) ? (low - spread) : 0ull, total);
    const unsigned long long end = pages_least(total, high + view->rows + spread);
    const int reaches = (view->held_cut == 0ull) || (view->held_cut >= view->columns);
    const int meets =
        reaches && (view->held_to > view->held_from) && (view->held_from <= end) && (view->held_to >= first);
    const unsigned long long kept_from = (top > (CLI_PAGE_KEPT / 2ull)) ? (top - (CLI_PAGE_KEPT / 2ull)) : 0ull;
    const unsigned long long kept_to = pages_least(total, top + view->rows + (CLI_PAGE_KEPT / 2ull));
    CliPagesPlaced placed = {first, first, view->columns, kept_from, kept_to};
    const int below = (up == 0ull) && (view->held_to < total) && (view->held_to < (end + CLI_PAGE_NEAR));
    const int above = (up != 0ull) && (view->held_from > 0ull) && ((view->held_from + CLI_PAGE_NEAR) > first);
    if (!meets)
    {
        placed.to = end;
        placed.keep_from = first;
        placed.keep_to = end;
        pages_rows_trimmed(one, view->width, low, &placed.from, &placed.to);
    }
    else if (below || (view->held_to < end))
    {
        placed.from = view->held_to;
        placed.to = pages_least(total, pages_most(end, view->held_to + step));
        placed.keep_to = pages_most(kept_to, placed.to);
        pages_step_trimmed(one, 1, 1, view->columns, view->width, &placed.from, &placed.to);
    }
    else if (above || (view->held_from > first))
    {
        placed.from = pages_least(first, (view->held_from > step) ? (view->held_from - step) : 0ull);
        placed.to = view->held_from;
        placed.keep_from = pages_least(kept_from, placed.from);
        pages_step_trimmed(one, 0, 1, view->columns, view->width, &placed.from, &placed.to);
    }
    return placed;
}

// a resting view of `one`, a text of `total` lines, placed: its rows whole, EDGE about them, where the page does not
// hold them whole; then the lines it wants below them, then above, a step at a time; nothing where it holds them all.
// It wants NEAR lines about its rows and the minimap's lines, within half of KEPT on either side of its rows, and keeps
// those and every line within half of KEPT. Its rows, where it wraps, are the lines they take of those about them; its
// rows whole come from their first a step at a time, and each step is as many lines as CLI_PAGE_REST_BYTES of their
// bytes reach
static CliPagesPlaced pages_rest_placed(CliPaged *one, const CliPagesView *view, unsigned long long total)
{
    const unsigned long long top = pages_least(view->top, (total != 0ull) ? (total - 1ull) : 0ull);
    const unsigned long long rows_end = pages_least(total, top + view->rows);
    const unsigned long long kept_from = (top > (CLI_PAGE_KEPT / 2ull)) ? (top - (CLI_PAGE_KEPT / 2ull)) : 0ull;
    const unsigned long long kept_to = pages_least(total, rows_end + (CLI_PAGE_KEPT / 2ull));
    const unsigned long long near_from = (top > CLI_PAGE_NEAR) ? (top - CLI_PAGE_NEAR) : 0ull;
    const unsigned long long want_from = pages_most(pages_least(near_from, view->map_from), kept_from);
    const unsigned long long want_to = pages_least(pages_most(rows_end + CLI_PAGE_NEAR, view->map_to), kept_to);
    unsigned long long first = (top > CLI_PAGE_EDGE) ? (top - CLI_PAGE_EDGE) : 0ull;
    unsigned long long end = pages_least(total, rows_end + CLI_PAGE_EDGE);
    pages_rows_trimmed(one, view->width, top, &first, &end);
    const unsigned long long step = pages_step(one, total, 0ull);
    CliPagesPlaced placed = {first, first, 0ull, kept_from, kept_to};
    if ((view->held_cut != 0ull) || (view->held_from > first) || (view->held_to <= first))
    {
        placed.to = end;
        pages_step_trimmed(one, 1, 0, 0ull, 0ull, &placed.from, &placed.to);
    }
    else if (view->held_to < end)
    {
        placed.from = view->held_to;
        placed.to = end;
        pages_step_trimmed(one, 1, 0, 0ull, 0ull, &placed.from, &placed.to);
    }
    else if (view->held_to < want_to)
    {
        placed.from = view->held_to;
        placed.to = pages_least(want_to, view->held_to + step);
        pages_step_trimmed(one, 1, 0, 0ull, view->width, &placed.from, &placed.to);
    }
    else if (view->held_from > want_from)
    {
        placed.from = ((view->held_from - want_from) > step) ? (view->held_from - step) : want_from;
        placed.to = view->held_from;
        pages_step_trimmed(one, 0, 0, 0ull, view->width, &placed.from, &placed.to);
    }
    return placed;
}

int cli_pages_viewed(const char *key, const CliPagesView *view, CliPagesPlaced *placed)
{
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, &err_msg);
    if (one != NULL)
    {
        int done = 0;
        const unsigned long long total = pages_total(one, &done);
        const unsigned long long now = cli_clock_milliseconds();
        const int follows = one->viewed && ((now - one->view_at) <= CLI_PAGE_STILL);
        const int rests = !follows || (view->top == one->view_top);
        *placed =
            rests ? pages_rest_placed(one, view, total) : pages_move_placed(one, view, one->view_top, follows, total);
        one->viewed = 1;
        one->view_top = view->top;
        one->view_at = now;
    }
    cli_lock_give(&g_cli_pages_lock);
    return one != NULL;
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
