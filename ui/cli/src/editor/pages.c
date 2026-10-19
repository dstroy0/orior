// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages.c: a file opened paged, its index read behind the page, and its lines read from the disk and from the lines
// written over it
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "pages_internal.h"

// how many bytes the index reads at a time, and how many of a file's first bytes tell its end of line
#define CLI_PAGE_CHUNK (1ull << 20u)
#define CLI_PAGE_HEAD (64ull * 1024ull)

CliPaged *g_cli_paged = NULL;
CliLock g_cli_pages_lock = CLI_LOCK_INIT;
CliLock g_cli_index_lock = CLI_LOCK_INIT;
CliWake g_cli_index_wake = CLI_WAKE_INIT;

// a paged text's cells given back, those it has; and taken for a file of `size` bytes: 1, or 0 with none kept where
// the cells cannot meet them
static void pages_cells_given(CliPaged *one)
{
    cli_cell_given(one->piece);
    cli_cell_given(one->index.start);
    one->piece = NULL;
    one->index.start = NULL;
}

static int pages_cells_taken(CliPaged *one, unsigned long long size)
{
    // a file of `size` bytes has no more than size + 1 lines, so the index holds no more than size / CLI_PAGE_STEP + 2
    // starts
    const unsigned long long starts = (size / CLI_PAGE_STEP) + 2ull;
    one->index.starts_most = (starts < CLI_PAGE_STARTS) ? starts : CLI_PAGE_STARTS;
    one->index.start = cli_cell_taken(one->index.starts_most * sizeof(one->index.start[0]), "a paged text's index");
    one->piece = cli_cell_taken(CLI_PAGE_PIECES * sizeof(one->piece[0]), "a paged text's pieces");
    const int taken = (one->index.start != NULL) && (one->piece != NULL);
    if (!taken)
    {
        pages_cells_given(one);
    }
    return taken;
}

// a paged text let go: taken off the texts held, its index stopped before its cells are given back since its reader
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
    cli_cell_given(one);
}

CliPaged *cli_paged_of(const char *key, CliLine *said)
{
    for (CliPaged *one = g_cli_paged; one != NULL; one = one->next)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = key, .cap = CLI_CELLS_POOL}))
        {
            return one;
        }
    }
    cli_line_text(said, "no paged text is held under ");
    cli_line_text(said, key);
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
    const unsigned char *const bytes = qry_member_file_mapped(index->path, &size);
    unsigned long long lines = 1ull;
    int stop = 0;
    for (unsigned long long offset = 0ull; (bytes != NULL) && !stop && (offset < size); offset += CLI_PAGE_CHUNK)
    {
        const unsigned long long end = ((size - offset) < CLI_PAGE_CHUNK) ? size : (offset + CLI_PAGE_CHUNK);
        cli_lock_take(&g_cli_index_lock);
        for (unsigned long long at = offset; at < end;)
        {
            const unsigned char *const newline =
                memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = end - at});
            if (newline == NULL)
            {
                break;
            }
            // a newline found lies in the chunk, at or past `at`
            at = (unsigned long long)(newline - bytes) + 1ull;
            if (((lines % CLI_PAGE_STEP) == 0ull) && (index->starts < index->starts_most))
            {
                index->start[index->starts] = at;
                index->starts += 1ull;
            }
            lines += 1ull;
        }
        index->lines = lines;
        stop = index->cancel;
        cli_lock_give(&g_cli_index_lock);
    }
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
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
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    const unsigned long long head = (size < CLI_PAGE_HEAD) ? size : CLI_PAGE_HEAD;
    const unsigned char *const newline =
        (bytes != NULL) ? memor.chr(&(MemoriaCfg){.src = bytes, .val = '\n', .bytes = head}) : NULL;
    const int crlf = (newline != NULL) && (newline > bytes) && (newline[-1] == '\r');
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
    }
    return crlf ? "\r\n" : "\n";
}

void cli_page_shape_written(CliPaged *one, CliLine *out)
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
    cli_line_text(out, "{\"lines\":");
    cli_line_decimal(out, lines, 0u);
    cli_line_text(out, done ? ",\"indexed\":true,\"size\":" : ",\"indexed\":false,\"size\":");
    cli_line_decimal(out, one->size, 0u);
    cli_line_character(out, '}');
}

int cli_page_open_reading(const char *key, const char *path, int behind, CliLine *out, CliLine *said)
{
    unsigned long long size = 0ull;
    if (!cli_file_size(path, &size))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    // a text held under the key already is let go first, its index stopped and its shadow removed
    CliPaged *one = g_cli_paged;
    while ((one != NULL) && (!cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = key, .cap = CLI_CELLS_POOL})))
    {
        one = one->next;
    }
    if (one != NULL)
    {
        pages_let_go(one);
    }
    one = cli_cell_taken(sizeof(*one), "a paged text");
    if (one == NULL)
    {
        cli_line_text(said, "the paged text's cells could not be taken");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
    if (!pages_cells_taken(one, size))
    {
        cli_cell_given(one);
        cli_line_text(said, "the paged text's cells could not be taken");
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

int cli_pages_open(const char *key, const char *path, CliLine *out, CliLine *said)
{
    cli_lock_take(&g_cli_pages_lock);
    const int opened = cli_page_open_reading(key, path, 1, out, said);
    cli_lock_give(&g_cli_pages_lock);
    return opened;
}

int cli_pages_shape(const char *key, CliLine *out, CliLine *said)
{
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, said);
    if (one != NULL)
    {
        cli_page_shape_written(one, out);
    }
    cli_lock_give(&g_cli_pages_lock);
    return one != NULL;
}

// a line put after the lines read
static void cli_line_put(CliPageLines *lines, const char *bytes, unsigned long long size)
{
    if ((lines->count >= CLI_PAGE_READ_LINES) || ((lines->used + size) > CLI_PAGE_READ_TEXT))
    {
        lines->full = 1;
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = lines->text + lines->used, .src = bytes, .bytes = size});
    lines->start[lines->count] = lines->used;
    lines->length[lines->count] = size;
    lines->used += size;
    lines->count += 1ull;
}

// the file's lines from `from` to before `to`, read from the disk from the nearest line the index holds before
// `from`, put after the lines read, as many as asked, empty past the file's end: 1, or 0 with why said
static int cli_file_lines(CliPaged *one, unsigned long long from, unsigned long long to, CliPageLines *lines,
                          CliLine *said)
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
        cli_line_text(said, "the index has not read that far yet");
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(one->path, &size);
    if (bytes == NULL)
    {
        cli_line_text(said, "it could not be read");
        return 0;
    }
    const unsigned long long first = lines->count;
    unsigned long long at = (start < size) ? start : size;
    for (unsigned long long line = step * CLI_PAGE_STEP; line < to; line += 1ull)
    {
        const unsigned char *const newline =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = size - at}) : NULL;
        // a newline found lies in the bytes, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
        unsigned long long length = end - at;
        length -= ((newline != NULL) && (length > 0ull) && (bytes[end - 1ull] == '\r')) ? 1ull : 0ull;
        const unsigned long long read = (newline != NULL) ? (end - at + 1ull) : (size - at);
        if ((line >= from) && (lines->count < CLI_PAGE_READ_LINES))
        {
            // the line read as UTF-8, each byte that starts no whole letter as U+FFFD, straight after the lines read
            CliLine lossy = {lines->text + lines->used, CLI_PAGE_READ_TEXT - lines->used, 0ull};
            cli_lossy(&lossy, bytes + at, length);
            lines->full = lines->full || !cli_line_fits(&lossy);
            lines->start[lines->count] = lines->used;
            lines->length[lines->count] = cli_line_fits(&lossy) ? lossy.at : 0ull;
            lines->used += lines->length[lines->count];
            lines->count += 1ull;
        }
        at = (newline != NULL) ? (end + 1ull) : size;
        if (read == 0ull)
        {
            break;
        }
    }
    qry_member_file_unmapped(bytes, size);
    while ((lines->count - first) < (to - from))
    {
        cli_line_put(lines, "", 0ull);
        if (lines->full)
        {
            break;
        }
    }
    return 1;
}

// `count` lines of a written piece from its line `skip`, read from the shadow and put after the lines read: 1, or 0
// with why said
static int cli_written_lines(const CliPaged *one, const CliPagePiece *piece, unsigned long long skip,
                             unsigned long long count, CliPageLines *lines, CliLine *said)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(one->shadow, &size);
    if ((bytes == NULL) || (piece->to > size))
    {
        if (bytes != NULL)
        {
            qry_member_file_unmapped(bytes, size);
        }
        cli_line_text(said, one->shadow);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    unsigned long long at = piece->from;
    for (unsigned long long line = 0ull; line < (skip + count); line += 1ull)
    {
        const unsigned char *const newline =
            (at < piece->to) ? memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = piece->to - at}) : NULL;
        // a newline found lies in the piece, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : piece->to;
        if (line >= skip)
        {
            cli_line_put(lines, (const char *)bytes + at, end - at);
        }
        at = (end < piece->to) ? (end + 1ull) : piece->to;
    }
    qry_member_file_unmapped(bytes, size);
    return 1;
}

int cli_page_lines_of(CliPaged *one, unsigned long long from, unsigned long long to, CliPageLines *lines, CliLine *said)
{
    lines->used = 0ull;
    lines->count = 0ull;
    lines->full = 0;
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
                if (!cli_file_lines(one, piece->from + start - at, piece->from + end - at, lines, said))
                {
                    return 0;
                }
            }
            else if (!cli_written_lines(one, piece, start - at, end - start, lines, said))
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

int cli_pages_read(const char *key, unsigned long long from, unsigned long long to, CliLine *out, CliLine *said)
{
    // the index read as far as `to`, or whole, the text found again each turn since it may be let go meanwhile
    for (;;)
    {
        cli_lock_take(&g_cli_pages_lock);
        CliPaged *const one = cli_paged_of(key, said);
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
    CliPageLines *const lines = cli_cell_taken(sizeof(*lines), "the lines a reading gives");
    if (lines == NULL)
    {
        cli_line_text(said, "the lines a reading gives could not be taken");
        return 0;
    }
    cli_lock_take(&g_cli_pages_lock);
    CliPaged *const one = cli_paged_of(key, said);
    const int read = (one != NULL) && cli_page_lines_of(one, from, to, lines, said);
    if (read)
    {
        cli_line_character(out, '[');
        for (unsigned long long at = 0ull; at < lines->count; at += 1ull)
        {
            cli_line_text(out, (at != 0ull) ? "," : "");
            cli_json_quoted(out, lines->text + lines->start[at], lines->length[at]);
        }
        cli_line_character(out, ']');
    }
    cli_lock_give(&g_cli_pages_lock);
    cli_cell_given(lines);
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
