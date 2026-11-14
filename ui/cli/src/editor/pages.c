// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages.c: a file opened paged, its index read behind the page, and its lines read from the disk and from the lines
// written over it, for the page and for the walk of its colors
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "pages_internal.h"

// how many bytes the index reads at a time, how many of a file's first bytes determine its end of line, and the bytes a
// reading's cell begins with for each line requested
#define CLI_PAGE_CHUNK (1ULL << 20u)
#define CLI_PAGE_HEAD (64ULL * 1024ULL)

CliPaged *g_cli_paged = NULL;
CliWake g_cli_index_wake = CLI_WAKE_INIT;

typedef struct
{
    CliPaged *one;
    unsigned long long size;
    const char *path;
    int *done;
    unsigned long long from;
    unsigned long long *start;
    unsigned long long *first_line;
    unsigned long long to;
    unsigned long long whole;
    CliPageLines *lines;
    VerbaLinea *err_msg;
    const CliPaged *one_read;
    const CliPagePiece *piece;
    unsigned long long skip;
    unsigned long long count;
    unsigned long long bytes;
    const char *text;
    unsigned long long cut;
    unsigned long long *units;
    unsigned long long one_u64;
    unsigned long long other;
    unsigned long long total;
    const CliPageLines *lines_read;
    unsigned long long at;
    unsigned long long width;
    unsigned long long pivot;
    unsigned long long *from_pointer;
    unsigned long long *to_pointer;
    int below;
    int moving;
    const CliPagesView *view;
    unsigned long long last;
    int follows;
} PagesCtx;

EMBED_INLINE int pages_cells_alloc(const PagesCtx *args)
{
    // a file of `size` bytes has no more than size + 1 lines, and the index contains no more than size / CLI_PAGE_STEP
    // starts recorded by their lines, size / CLI_PAGE_SPAN by their bytes, and the first
    const unsigned long long starts = (args->size / CLI_PAGE_STEP) + (args->size / CLI_PAGE_SPAN) + 2ULL;
    args->one->index.starts_max = (starts < CLI_PAGE_STARTS) ? starts : CLI_PAGE_STARTS;
    // the index's two lists and the one piece a text opens with, each in a movable cell sized to it; the pieces are
    // written anew sized to their count as an edit replaces them
    args->one->index.start = g_cli_guard->movable_buf_alloc(
        (embed_word)args->one->index.starts_max * sizeof(unsigned long long), g_cli_editor_owner);
    args->one->index.line = g_cli_guard->movable_buf_alloc(
        (embed_word)args->one->index.starts_max * sizeof(unsigned long long), g_cli_editor_owner);
    args->one->piece = g_cli_guard->movable_buf_alloc(sizeof(CliPagePiece), g_cli_editor_owner);
    const int reserved =
        (args->one->index.start != NULL) && (args->one->index.line != NULL) && (args->one->piece != NULL);
    if (!reserved)
    {
        (void)g_cli_guard->movable_buf_release(args->one->piece, g_cli_editor_owner);
        (void)g_cli_guard->movable_buf_release(args->one->index.start, g_cli_editor_owner);
        (void)g_cli_guard->movable_buf_release(args->one->index.line, g_cli_editor_owner);
        args->one->piece = NULL;
        args->one->index.start = NULL;
        args->one->index.line = NULL;
    }
    return reserved;
}

// a paged text let go: taken off the texts stored, its index stopped before its cells are given back since its reader
// writes into them, its shadow removed where it has one, and its own cell given back
EMBED_INLINE void pages_let_go(const PagesCtx *args)
{
    CliPaged **link = &g_cli_paged;
    while ((*link != NULL) && (*link != args->one))
    {
        link = &(*link)->next;
    }
    if (*link != NULL)
    {
        *link = args->one->next;
    }
    args->one->index.cancel = 1;
    while (args->one->index.read_state)
    {
        EMBED_CALL(cli_wake_internal.wake_wait, CliWakeCtx, .wake = &g_cli_index_wake);
    }
    (void)g_cli_guard->movable_buf_release(args->one->piece, g_cli_editor_owner);
    (void)g_cli_guard->movable_buf_release(args->one->index.start, g_cli_editor_owner);
    (void)g_cli_guard->movable_buf_release(args->one->index.line, g_cli_editor_owner);
    args->one->piece = NULL;
    args->one->index.start = NULL;
    args->one->index.line = NULL;
    if (args->one->shadow_used > 0ULL)
    {
        (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = args->one->shadow);
    }
    args->one->reserved = 0;
}

CliPaged *cli_paged_get(const CliPagesErrMsgCtx *args)
{
    for (CliPaged *one = g_cli_paged; one != NULL; one = one->next)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = args->key, .cap = CLI_POOL_BYTES}))
        {
            return one;
        }
    }
    mmgr_line_put(args->err_msg, "no paged text is stored under ");
    mmgr_line_put(args->err_msg, args->key);
    return NULL;
}

unsigned long long cli_page_index(const CliPagesOneCtx *args)
{
    const unsigned long long lines = args->one->index.lines;
    return lines;
}

unsigned long long cli_page_length(const CliPagesCtx *args)
{
    if (args->piece->written)
    {
        return args->piece->lines;
    }
    const unsigned long long to = args->piece->open ? args->indexed : args->piece->to;
    return (to > args->piece->from) ? (to - args->piece->from) : 0ULL;
}

// the index of a file's lines read, a chunk at a time, each chunk's starts put in the index as it is read, until the
// file ends or the reading is to stop
static void cli_index_read(void *context)
{
    CliPageIndex *const index = (CliPageIndex *)context;
    unsigned long long size = 0ULL;
    const int sized =
        EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = index->path, .size_pointer = &size);
    unsigned long long lines = 1ULL;
    int stop = 0;
    // a chunk at a time stored in a cell, its newlines found where it lies; a line that runs over a chunk's end is
    // counted at its newline in the chunk after
    unsigned long long offset = 0ULL;
    while (sized && !stop && (offset < size))
    {
        unsigned long long len = 0ULL;
        const unsigned char *const chunk =
            EMBED_CALL(cli_path_internal.file_piece_alloc, CliPathCtx, .path = index->path, .from = offset,
                       .count_u64 = CLI_PAGE_CHUNK, .size_pointer = &len, .owner = g_cli_editor_owner);
        const unsigned long long end = offset + len;
        for (unsigned long long at = offset; (chunk != NULL) && (at < end);)
        {
            const unsigned char *const newline =
                memor.chr(&(MemoriaCfg){.src = chunk + (at - offset), .val = '\n', .bytes = end - at});
            if (newline == NULL)
            {
                break;
            }
            // a newline found lies in the chunk, at or past `at`
            at = offset + (unsigned long long)(newline - chunk) + 1ULL;
            // the line starting here recorded CLI_PAGE_STEP lines or CLI_PAGE_SPAN bytes after the last recorded
            const unsigned long long last = index->starts - 1ULL;
            unsigned long long last_line = 0ULL;
            unsigned long long last_start = 0ULL;
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = index->line,
                                                                    .offset = (embed_word)last * sizeof(last_line),
                                                                    .dst = &last_line,
                                                                    .bytes = sizeof(last_line)});
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = index->start,
                                                                    .offset = (embed_word)last * sizeof(last_start),
                                                                    .dst = &last_start,
                                                                    .bytes = sizeof(last_start)});
            const int far = ((lines - last_line) >= CLI_PAGE_STEP) || ((at - last_start) >= CLI_PAGE_SPAN);
            if (far && (index->starts < index->starts_max))
            {
                {
                    const unsigned long long start = at;
                    const unsigned long long line = lines;
                    (void)g_cli_guard->movable_buf_write(
                        &(CarcerMovableCfg){.cell = index->start,
                                            .offset = (embed_word)index->starts * sizeof(start),
                                            .src = &start,
                                            .bytes = sizeof(start)});
                    (void)g_cli_guard->movable_buf_write(
                        &(CarcerMovableCfg){.cell = index->line,
                                            .offset = (embed_word)index->starts * sizeof(line),
                                            .src = &line,
                                            .bytes = sizeof(line)});
                }
                index->starts += 1ULL;
            }
            lines += 1ULL;
        }
        index->lines = lines;
        stop = index->cancel || (chunk == NULL) || (len == 0ULL);
        g_cli_guard->persistent_buf_release((void *)(embed_word)chunk, (g_cli_editor_owner));
        offset += CLI_PAGE_CHUNK;
    }
    index->lines = lines;
    index->done = 1;
    index->read_state = 0;
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &g_cli_index_wake);
}

// the end of line a file's first line ends with: "\r\n" where it does, and "\n" otherwise
EMBED_INLINE const char *cli_eol_get(const PagesCtx *args)
{
    const char *path = args->path;
    unsigned long long size = 0ULL;
    const unsigned char *const bytes =
        EMBED_CALL(cli_path_internal.file_piece_alloc, CliPathCtx, .path = path, .from = 0ULL,
                   .count_u64 = CLI_PAGE_HEAD, .size_pointer = &size, .owner = g_cli_editor_owner);
    const unsigned char *const newline =
        (bytes != NULL) ? memor.chr(&(MemoriaCfg){.src = bytes, .val = '\n', .bytes = size}) : NULL;
    const int crlf = (newline != NULL) && (newline > bytes) && (newline[-1] == '\r');
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_editor_owner));
    return crlf ? "\r\n" : "\n";
}

// the text's lines in all as far as the index has read, and whether the index is whole into `done`
EMBED_INLINE unsigned long long pages_total(const PagesCtx *args)
{
    int *done = args->done;
    const unsigned long long indexed = args->one->index.lines;
    *done = args->one->index.done;
    unsigned long long lines = 0ULL;
    for (unsigned int at = 0u; at < args->one->pieces; at += 1u)
    {
        CliPagePiece piece = {0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = args->one->piece, .offset = (embed_word)at * sizeof(piece), .dst = &piece, .bytes = sizeof(piece)});
        lines += EMBED_CALL(cli_pages_internal.page_length, CliPagesCtx, .piece = &piece, .indexed = indexed);
    }
    return lines;
}

void cli_page_shape_write(const CliPagesOneCtx *args)
{
    if (args->out == NULL)
    {
        return;
    }
    int done = 0;
    const unsigned long long lines = EMBED_CALL(pages_total, PagesCtx, .one = args->one, .done = &done);
    mmgr_line_put(args->out, "{\"lines\":");
    mmgr_line_decimal(args->out, lines, 0u);
    mmgr_line_put(args->out, done ? ",\"indexed\":true,\"size\":" : ",\"indexed\":false,\"size\":");
    mmgr_line_decimal(args->out, args->one->size, 0u);
    mmgr_line_ch(args->out, '}');
}

int cli_page_open_read_state(const CliPagesErrMsgCtx *args)
{
    unsigned long long size = 0ULL;
    if (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = args->path, .size_pointer = &size))
    {
        mmgr_line_put(args->err_msg, args->path);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    // a text stored under the key already is let go first, its index stopped and its shadow removed
    CliPaged *one = g_cli_paged;
    while ((one != NULL) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = args->key, .cap = CLI_POOL_BYTES})))
    {
        one = one->next;
    }
    if (one != NULL)
    {
        EMBED_CALL(pages_let_go, PagesCtx, .one = one);
    }
    // a paged text's record carries the index its reader thread writes, so the records are fixed outside the pool,
    // one place for each paged text open at once, where they punch no hole in the persistent tier
    static CliPaged s_places[CLI_PAGED_MAX];
    one = NULL;
    for (unsigned int at = 0u; (one == NULL) && (at < CLI_PAGED_MAX); at += 1u)
    {
        one = (s_places[at].reserved == 0) ? &s_places[at] : NULL;
    }
    if (one == NULL)
    {
        mmgr_line_put(args->err_msg, "the paged text's cells could not be taken");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = one, .val = 0, .bytes = sizeof(*one)});
    one->reserved = 1;
    if (!EMBED_CALL(pages_cells_alloc, PagesCtx, .one = one, .size = size))
    {
        one->reserved = 0;
        mmgr_line_put(args->err_msg, "the paged text's cells could not be taken");
        return 0;
    }
    {
        const unsigned long long start = 0ULL;
        const unsigned long long line = 0ULL;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = (&one->index)->start,
                                                                 .offset = (embed_word)0ULL * sizeof(start),
                                                                 .src = &start,
                                                                 .bytes = sizeof(start)});
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = (&one->index)->line,
                                                                 .offset = (embed_word)0ULL * sizeof(line),
                                                                 .src = &line,
                                                                 .bytes = sizeof(line)});
    }
    one->index.starts = 1ULL;
    one->index.lines = 1ULL;
    one->index.read_state = 1;
    memor.cpy(&(MemoriaCfg){.dst = one->index.path,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = one->key,
                            .src = args->key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = one->path,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    EMBED_CALL(cli_pages_path_internal.page_adjacent, CliPagesPathCtx, .path = args->path, .ending = CLI_PAGE_SHADOW,
               .adjacent = one->shadow);
    one->size = size;
    one->eol = EMBED_CALL(cli_eol_get, PagesCtx, .path = args->path);
    const CliPagePiece whole_piece = {0, 0ULL, 0ULL, 1, 0ULL};
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = one->piece, .src = &whole_piece, .bytes = sizeof(whole_piece)});
    one->pieces = 1u;
    one->next = g_cli_paged;
    g_cli_paged = one;
    one->index.reader = (CliThread){cli_index_read, &one->index};
    if (!args->behind || !EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &one->index.reader))
    {
        cli_index_read(&one->index);
    }
    EMBED_CALL(cli_pages_one_internal.page_shape_write, CliPagesOneCtx, .one = one, .out = args->out);
    return 1;
}

int cli_pages_open(const CliPagesPathCfg *args)
{
    const int opened = EMBED_CALL(cli_pages_err_msg_internal.page_open_read_state, CliPagesErrMsgCtx, .key = args->key,
                                  .path = args->path, .behind = 1, .out = args->out, .err_msg = args->err_msg);
    return opened;
}

int cli_pages_shape(const CliPagesPathCfg *args)
{
    CliPaged *const one =
        EMBED_CALL(cli_pages_err_msg_internal.paged_get, CliPagesErrMsgCtx, .key = args->key, .err_msg = args->err_msg);
    if (one != NULL)
    {
        EMBED_CALL(cli_pages_one_internal.page_shape_write, CliPagesOneCtx, .one = one, .out = args->out);
    }
    return one != NULL;
}

// the last start the index records at or before `from` and the line it starts: 1, or 0 where it records none
EMBED_INLINE int cli_index_adjacent(const PagesCtx *args)
{
    unsigned long long *start = args->start;
    unsigned long long *first_line = args->first_line;
    const CliPageIndex *const index = &args->one->index;
    unsigned long long low = 0ULL;
    unsigned long long high = index->starts;
    while ((high - low) > 1ULL)
    {
        const unsigned long long middle = low + ((high - low) / 2ULL);
        unsigned long long line_3 = 0ULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = index->line,
                                                                .offset = (embed_word)middle * sizeof(line_3),
                                                                .dst = &line_3,
                                                                .bytes = sizeof(line_3)});

        if (line_3 <= args->from)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    const int indexed = index->starts != 0ULL;
    unsigned long long start_1 = 0ULL;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = index->start, .offset = (embed_word)low * sizeof(start_1), .dst = &start_1, .bytes = sizeof(start_1)});

    *start = indexed ? start_1 : 0ULL;
    unsigned long long line_4 = 0ULL;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = index->line, .offset = (embed_word)low * sizeof(line_4), .dst = &line_4, .bytes = sizeof(line_4)});

    *first_line = indexed ? line_4 : 0ULL;
    return indexed;
}

// the byte the lines before `to` end by, in a file of `whole` bytes: the start of the first line the index records at
// or past `to`; past the last it records, the max bytes it lets pass between two while it is read, and the file's end
// once it has read the file or has no more places
EMBED_INLINE unsigned long long cli_index_end(const PagesCtx *args)
{
    const CliPageIndex *const index = &args->one->index;
    unsigned long long low = 0ULL;
    unsigned long long high = index->starts;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ULL);
        unsigned long long line_5 = 0ULL;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = index->line,
                                                                .offset = (embed_word)middle * sizeof(line_5),
                                                                .dst = &line_5,
                                                                .bytes = sizeof(line_5)});

        if (line_5 < args->to)
        {
            low = middle + 1ULL;
        }
        else
        {
            high = middle;
        }
    }
    unsigned long long end = args->whole;
    if (low < index->starts)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = index->start, .offset = (embed_word)low * sizeof(end), .dst = &end, .bytes = sizeof(end)});
    }
    else if (!index->done && (index->starts != 0ULL) && (index->starts < index->starts_max))
    {
        unsigned long long start_2 = 0ULL;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = index->start,
                                .offset = (embed_word)(index->starts - 1ULL) * sizeof(start_2),
                                .dst = &start_2,
                                .bytes = sizeof(start_2)});

        end = start_2 + CLI_PAGE_SPAN;
    }
    return (end < args->whole) ? end : args->whole;
}

// the file's lines from `from` to before `to`, read from the disk from the nearest line the index records before
// `from`, put after the lines read, as many as requested, empty past the file's end: 1, or 0 with why given
EMBED_INLINE int cli_file_lines(const PagesCtx *args)
{
    unsigned long long from = args->from;
    CliPageLines *lines = args->lines;
    if (from >= args->to)
    {
        return 1;
    }
    unsigned long long start = 0ULL;
    unsigned long long first_line = 0ULL;
    if (!EMBED_CALL(cli_index_adjacent, PagesCtx, .one = args->one, .from = from, .start = &start,
                    .first_line = &first_line))
    {
        mmgr_line_put(args->err_msg, "the index has not read that far yet");
        return 0;
    }
    // The lines from the start stored in a cell of a window of the file from there to the byte they end by
    unsigned long long whole = 0ULL;
    if (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = args->one->path, .size_pointer = &whole))
    {
        mmgr_line_put(args->err_msg, "it could not be read");
        return 0;
    }
    const unsigned long long begin = (start < whole) ? start : whole;
    const unsigned long long end_at =
        EMBED_CALL(cli_index_end, PagesCtx, .one = args->one, .to = args->to, .whole = whole);
    const unsigned long long window = (end_at > begin) ? (end_at - begin) : 0ULL;
    unsigned long long size = 0ULL;
    const unsigned char *const bytes =
        EMBED_CALL(cli_path_internal.file_piece_alloc, CliPathCtx, .path = args->one->path, .from = begin,
                   .count_u64 = window, .size_pointer = &size, .owner = g_cli_editor_owner);
    if (bytes == NULL)
    {
        mmgr_line_put(args->err_msg, "it could not be read");
        return 0;
    }
    // the lines from the start, those from `from` on put after the lines read
    const unsigned long long first = lines->count;
    unsigned long long at = 0ULL;
    for (unsigned long long line = first_line; line < args->to; line += 1ULL)
    {
        const unsigned char *const newline =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = size - at}) : NULL;
        // a newline found lies in the window, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
        unsigned long long length = end - at;
        length -= ((newline != NULL) && (length > 0ULL) && (bytes[end - 1ULL] == '\r')) ? 1ULL : 0ULL;
        const unsigned long long read = (newline != NULL) ? (end - at + 1ULL) : (size - at);
        // the line read as UTF-8 straight after the lines read, a byte that starts no whole character written as U+FFFD,
        // three bytes
        if ((line >= from) && !lines->full &&
            EMBED_CALL(cli_pages_lines_lines_more_internal.page_lines_capacity, CliPagesLinesLinesMoreCtx,
                       .lines = lines, .lines_more = 1ULL, .text_more = 3ULL * length))
        {
            VerbaLinea lossy = {lines->text + lines->used, lines->text_max - lines->used, 0ULL};
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = &lossy, .text_bytes = bytes + at,
                       .length = length);
            lines->full = lines->full || !mmgr_line_ok(&lossy);
            lines->start[lines->count] = lines->used;
            lines->length[lines->count] = mmgr_line_ok(&lossy) ? lossy.at : 0ULL;
            lines->used += lines->length[lines->count];
            lines->count += 1ULL;
        }
        at = (newline != NULL) ? (end + 1ULL) : size;
        if (read == 0ULL)
        {
            break;
        }
    }
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_editor_owner));
    while (((lines->count - first) < (args->to - from)) && !lines->full)
    {
        EMBED_CALL(cli_pages_lines_bytes_internal.page_lines_append, CliPagesLinesBytesCtx, .lines = lines, .bytes = "",
                   .size = 0ULL);
    }
    return 1;
}

// `count` lines of a written piece from its line `skip`, read from the shadow and put after the lines read: 1, or 0
// with why given
EMBED_INLINE int cli_write_lines(const PagesCtx *args)
{
    unsigned long long count = args->count;
    CliPageLines *lines = args->lines;
    // the piece stored in a cell, from its start, its bytes counted from there
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(
        cli_path_internal.file_piece_alloc, CliPathCtx, .path = args->one_read->shadow, .from = args->piece->from,
        .count_u64 = args->piece->to - args->piece->from, .size_pointer = &size, .owner = g_cli_editor_owner);
    if ((bytes == NULL) || (size < (args->piece->to - args->piece->from)))
    {
        g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_editor_owner));
        mmgr_line_put(args->err_msg, args->one_read->shadow);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    unsigned long long at = 0ULL;
    for (unsigned long long line = 0ULL; line < (args->skip + count); line += 1ULL)
    {
        const unsigned char *const newline =
            (at < size) ? memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = size - at}) : NULL;
        // a newline found lies in the piece, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
        if (line >= args->skip)
        {
            EMBED_CALL(cli_pages_lines_bytes_internal.page_lines_append, CliPagesLinesBytesCtx, .lines = lines,
                       .bytes = (const char *)bytes + at, .size = end - at);
        }
        at = (end < size) ? (end + 1ULL) : size;
    }
    g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_editor_owner));
    return 1;
}

// the bytes the pieces of `one` that hold the lines from `from` to before `to` are known to read: a written piece's
// own, and the window of the file a piece of it is read through
EMBED_INLINE unsigned long long pages_reach(const PagesCtx *args)
{
    CliPaged *one = args->one;
    const unsigned long long indexed = EMBED_CALL(cli_pages_one_internal.page_index, CliPagesOneCtx, .one = one);
    unsigned long long whole = 0ULL;
    (void)EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = one->path, .size_pointer = &whole);
    unsigned long long bytes = 0ULL;
    unsigned long long at = 0ULL;
    for (unsigned int index = 0u; (index < one->pieces) && (at < args->to); index += 1u)
    {
        CliPagePiece read = {0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = one->piece, .offset = (embed_word)index * sizeof(read), .dst = &read, .bytes = sizeof(read)});
        const CliPagePiece *const piece = &read;
        const unsigned long long count =
            EMBED_CALL(cli_pages_internal.page_length, CliPagesCtx, .piece = piece, .indexed = indexed);
        const unsigned long long start = (args->from > at) ? args->from : at;
        const unsigned long long end = (args->to < (at + count)) ? args->to : (at + count);
        if ((start < end) && piece->written)
        {
            bytes += piece->to - piece->from;
        }
        else if (start < end)
        {
            unsigned long long adjacent = 0ULL;
            unsigned long long first_line = 0ULL;
            (void)EMBED_CALL(cli_index_adjacent, PagesCtx, .one = one, .from = piece->from + start - at,
                             .start = &adjacent, .first_line = &first_line);
            bytes += EMBED_CALL(cli_index_end, PagesCtx, .one = one, .to = piece->from + end - at, .whole = whole) -
                     adjacent;
        }
        at += count;
    }
    return bytes;
}

// the pieces' lines from `from` to before `to` read into `lines`: 1, or 0 with why given
EMBED_INLINE int pages_pieces_read(const PagesCtx *args)
{
    CliPaged *one = args->one;
    CliPageLines *lines = args->lines;
    EMBED_CALL(cli_pages_lines_lines_first_internal.page_lines_clear, CliPagesLinesLinesFirstCtx, .lines = lines);
    const unsigned long long indexed = EMBED_CALL(cli_pages_one_internal.page_index, CliPagesOneCtx, .one = one);
    unsigned long long at = 0ULL;
    for (unsigned int index = 0u; index < one->pieces; index += 1u)
    {
        CliPagePiece read = {0};
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = one->piece, .offset = (embed_word)index * sizeof(read), .dst = &read, .bytes = sizeof(read)});
        const CliPagePiece *const piece = &read;
        const unsigned long long count =
            EMBED_CALL(cli_pages_internal.page_length, CliPagesCtx, .piece = piece, .indexed = indexed);
        const unsigned long long start = (args->from > at) ? args->from : at;
        const unsigned long long end = (args->to < (at + count)) ? args->to : (at + count);
        if ((start < end) && !piece->written &&
            !EMBED_CALL(cli_file_lines, PagesCtx, .one = one, .from = piece->from + start - at,
                        .to = piece->from + end - at, .lines = lines, .err_msg = args->err_msg))
        {
            return 0;
        }
        if ((start < end) && piece->written &&
            !EMBED_CALL(cli_write_lines, PagesCtx, .one_read = one, .piece = piece, .skip = start - at,
                        .count = end - start, .lines = lines, .err_msg = args->err_msg))
        {
            return 0;
        }
        at += count;
        if (at >= args->to)
        {
            break;
        }
    }
    return 1;
}

// `lines` positioned over one cell of `count` lines and `bytes` of text, the one before given back: 1, or 0 with why given
EMBED_INLINE int pages_lines_resize(const PagesCtx *args)
{
    CliPageLines *lines = args->lines;
    VerbaLinea *err_msg = args->err_msg;
    EMBED_CALL(cli_pages_lines_lines_first_internal.page_lines_free, CliPagesLinesLinesFirstCtx, .lines = lines);
    return EMBED_CALL(cli_pages_lines_lines_first_internal.page_lines_reserve, CliPagesLinesLinesFirstCtx,
                      .lines = lines, .lines_first = args->count, .text_first = args->bytes, .err_msg = err_msg);
}

int cli_page_lines_get(const CliPagesLinesErrMsgCtx *args)
{
    // a byte read is written as at max 3 bytes of UTF-8
    const unsigned long long count = (args->to_value > args->from) ? (args->to_value - args->from) : 1ULL;
    const unsigned long long bytes =
        (3ULL * EMBED_CALL(pages_reach, PagesCtx, .one = args->one, .from = args->from, .to = args->to_value)) + 1ULL;
    if (((args->lines->lines_max < count) || (args->lines->text_max < bytes)) &&
        !EMBED_CALL(pages_lines_resize, PagesCtx, .lines = args->lines, .count = count, .bytes = bytes,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    return EMBED_CALL(pages_pieces_read, PagesCtx, .one = args->one, .from = args->from, .to = args->to_value,
                      .lines = args->lines, .err_msg = args->err_msg);
}

// the bytes of the first `cut` characters of the `size` bytes of UTF-8 at `text`, all of them where `cut` is 0 or they
// hold no more; and the length of all of them in UTF-16's units into `units`
EMBED_INLINE unsigned long long pages_cut_bytes(const PagesCtx *args)
{
    unsigned long long *units = args->units;
    unsigned long long characters = 0ULL;
    unsigned long long bytes = args->size;
    unsigned long long counted = 0ULL;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        const unsigned char byte = (unsigned char)args->text[at];
        // a byte that continues a character starts none, and a character of four bytes is two of UTF-16's units
        if ((byte & 0xc0u) != 0x80u)
        {
            bytes = ((args->cut != 0ULL) && (characters == args->cut) && (bytes == args->size)) ? at : bytes;
            characters += 1ULL;
            counted += (byte >= 0xf0u) ? 2ULL : 1ULL;
        }
    }
    *units = counted;
    return bytes;
}

int cli_pages_count(const CliPagesCfg *args)
{
    unsigned long long *lines = args->lines;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliPaged *const one =
        EMBED_CALL(cli_pages_err_msg_internal.paged_get, CliPagesErrMsgCtx, .key = args->key, .err_msg = &err_msg);
    if (one != NULL)
    {
        *lines = EMBED_CALL(pages_total, PagesCtx, .one = one, .done = args->whole);
    }
    return one != NULL;
}

char *cli_pages_lines_alloc(const CliPagesFromCfg *args)
{
    unsigned long long *count = args->count;
    *count = 0ULL;
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned long long required = (args->to > args->from) ? (args->to - args->from) : 1ULL;
    CliPageLines lines = {0};
    CliPaged *const one =
        EMBED_CALL(cli_pages_err_msg_internal.paged_get, CliPagesErrMsgCtx, .key = args->key, .err_msg = &err_msg);
    const int read = (one != NULL) &&
                     EMBED_CALL(cli_pages_lines_err_msg_internal.page_lines_get, CliPagesLinesErrMsgCtx, .one = one,
                                .from = args->from, .to_value = args->to, .lines = &lines, .err_msg = &err_msg) &&
                     !lines.full;
    // each line's bytes and its NUL, in a cell of the caller's call, which gives it back as it ends
    char *const text = read ? g_cli_guard->temporary_buf_alloc(lines.used + lines.count + 1ULL, args->owner) : NULL;
    const unsigned long long reserved = (lines.count < required) ? lines.count : required;
    unsigned long long at = 0ULL;
    for (unsigned long long each = 0ULL; (text != NULL) && (each < reserved); each += 1ULL)
    {
        args->line[each] = text + at;
        memor.cpy(&(MemoriaCfg){.dst = text + at, .src = lines.text + lines.start[each], .bytes = lines.length[each]});
        at += lines.length[each];
        text[at] = '\0';
        at += 1ULL;
    }
    *count = (text != NULL) ? reserved : 0ULL;
    EMBED_CALL(cli_pages_lines_lines_first_internal.page_lines_free, CliPagesLinesLinesFirstCtx, .lines = &lines);
    return text;
}

// the lesser of two lines
EMBED_INLINE unsigned long long pages_min(const PagesCtx *args)
{
    return (args->one_u64 < args->other) ? args->one_u64 : args->other;
}

// the greater of two lines
EMBED_INLINE unsigned long long pages_max(const PagesCtx *args)
{
    return (args->one_u64 > args->other) ? args->one_u64 : args->other;
}

// the lines a step of `one`, a text of `total` lines, reads: as many as REST_BYTES reach at the text's bytes a line,
// its size over its lines, a line cut after `cut` characters counting no more than those, from REST_LEAST to half of SAVED
EMBED_INLINE unsigned long long pages_step(const PagesCtx *args)
{
    const unsigned long long whole = (args->total != 0ULL) ? ((args->one_read->size / args->total) + 1ULL) : 1ULL;
    const unsigned long long each = ((args->cut != 0ULL) && ((args->cut + 1ULL) < whole)) ? (args->cut + 1ULL) : whole;
    const unsigned long long reached = (each != 0ULL) ? (CLI_PAGE_REMAINDER_BYTES / each) : CLI_PAGE_REMAINDER_BYTES;
    return EMBED_CALL(pages_min, PagesCtx,
                      .one_u64 = EMBED_CALL(pages_max, PagesCtx, .one_u64 = reached, .other = CLI_PAGE_REMAINDER_MIN),
                      .other = CLI_PAGE_SAVED / 2ULL);
}

// the rows the line `at` of `lines` takes in a view that wraps at `width` characters: as many as its UTF-16 units fill,
// one at the least
EMBED_INLINE unsigned long long pages_rows_get(const PagesCtx *args)
{
    unsigned long long units = 0ULL;
    (void)EMBED_CALL(pages_cut_bytes, PagesCtx, .text = args->lines_read->text + args->lines_read->start[args->at],
                     .size = args->lines_read->length[args->at], .cut = 0ULL, .units = &units);
    return ((args->width != 0ULL) && (units > args->width)) ? ((units + args->width - 1ULL) / args->width) : 1ULL;
}

// the lines of `one` from `from` to before `to` read into `lines`, reserved for them first: 1 where every one was read
// whole, else 0, `lines` to be given back either way; under the pages' lock
EMBED_INLINE int pages_positioned_read(const PagesCtx *args)
{
    CliPaged *one = args->one;
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    CliPageLines *lines = args->lines;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned long long required = to - from;
    *lines = (CliPageLines){0};
    return EMBED_CALL(cli_pages_lines_err_msg_internal.page_lines_get, CliPagesLinesErrMsgCtx, .one = one, .from = from,
                      .to_value = to, .lines = lines, .err_msg = &err_msg) &&
           !lines->full && (lines->count == required);
}

// the lines of `one` from `*from` to before `*to`, placed about the line `pivot` for a view that wraps at `width`
// characters, trimmed to those its rows take: as many on either side of `pivot` as fill the rows placed on that side, a
// line as many rows as its characters fill. As placed where the view does not wrap, or the lines cannot be read whole;
// under the pages' lock
EMBED_INLINE void pages_rows_trim(const PagesCtx *args)
{
    unsigned long long *from = args->from_pointer;
    unsigned long long *to = args->to_pointer;
    const int about = (args->width != 0ULL) && (args->pivot >= *from) && (args->pivot < *to);
    CliPageLines lines;
    if (about &&
        EMBED_CALL(pages_positioned_read, PagesCtx, .one = args->one, .from = *from, .to = *to, .lines = &lines))
    {
        unsigned long long first = args->pivot;
        for (unsigned long long rows = 0ULL; (first > *from) && (rows < (args->pivot - *from)); first -= 1ULL)
        {
            rows += EMBED_CALL(pages_rows_get, PagesCtx, .lines_read = &lines, .at = first - 1ULL - *from,
                               .width = args->width);
        }
        unsigned long long end = args->pivot;
        for (unsigned long long rows = 0ULL; (end < *to) && (rows < (*to - args->pivot)); end += 1ULL)
        {
            rows += EMBED_CALL(pages_rows_get, PagesCtx, .lines_read = &lines, .at = end - *from, .width = args->width);
        }
        *from = first;
        *to = end;
    }
    if (about)
    {
        EMBED_CALL(cli_pages_lines_lines_first_internal.page_lines_free, CliPagesLinesLinesFirstCtx, .lines = &lines);
    }
}

// the lines of `one` from `*from` to before `*to`, a step toward the ends of the page's lines below them where `below`
// is 1 and above where 0, trimmed to those from the page's side that CLI_PAGE_REMAINDER_BYTES of their bytes reach, a
// line cut after `cut` characters counting no more than those, and where the view moves and wraps at `width` characters, that
// fill the rows placed, a line as many rows as its characters fill; one line at the least. As placed where the lines
// cannot be read whole; under the pages' lock
EMBED_INLINE void pages_step_trim(const PagesCtx *args)
{
    unsigned long long *from = args->from_pointer;
    unsigned long long *to = args->to_pointer;
    CliPageLines lines;
    const unsigned long long required = *to - *from;
    if ((required != 0ULL) &&
        EMBED_CALL(pages_positioned_read, PagesCtx, .one = args->one, .from = *from, .to = *to, .lines = &lines))
    {
        const int by_rows = args->moving && (args->width != 0ULL);
        unsigned long long reserved = 0ULL;
        unsigned long long bytes = 0ULL;
        unsigned long long rows = 0ULL;
        while ((reserved < required) &&
               ((reserved == 0ULL) || ((bytes < CLI_PAGE_REMAINDER_BYTES) && (!by_rows || (rows < required)))))
        {
            const unsigned long long at = args->below ? reserved : (required - 1ULL - reserved);
            bytes += ((args->cut != 0ULL) && (args->cut < lines.length[at])) ? args->cut : lines.length[at];
            rows += by_rows ? EMBED_CALL(pages_rows_get, PagesCtx, .lines_read = &lines, .at = at, .width = args->width)
                            : 1ULL;
            reserved += 1ULL;
        }
        *to = args->below ? (*from + reserved) : *to;
        *from = args->below ? *from : (*to - reserved);
    }
    if (required != 0ULL)
    {
        EMBED_CALL(cli_pages_lines_lines_first_internal.page_lines_free, CliPagesLinesLinesFirstCtx, .lines = &lines);
    }
}

// a moving view of `one`, reported at `last` before, placed in a text of `total` lines, cut at its reach. Where the
// page's lines meet the rows it needs, its rows where it is and where it will are a report later as far on the
// way it went as it went since `last` where the report follows it, EDGE and a SPREADth of that step about them, no more
// than the lines a step reads on either side: a step of lines past the page's on the side it goes once those rows come
// within ADJACENT lines of their end, or past their end; else nothing. Where they do not meet, as a view that jumped
// away, those rows alone, only those of where it will are where it went farther than its rows. It saves half of SAVED
// lines on either side of its rows and every line it is answered. A view that wraps is answered the lines its rows take
// of those about them, and a step as many lines as fill its rows and CLI_PAGE_REMAINDER_BYTES of their bytes reach
EMBED_INLINE CliPagesPositioned pages_move_position(const PagesCtx *args)
{
    const unsigned long long top = EMBED_CALL(pages_min, PagesCtx, .one_u64 = args->view->top,
                                              .other = (args->total != 0ULL) ? (args->total - 1ULL) : 0ULL);
    const unsigned long long down = (args->follows && (top > args->last)) ? (top - args->last) : 0ULL;
    const unsigned long long up = (args->follows && (top < args->last)) ? (args->last - top) : 0ULL;
    unsigned long long ahead;
    if (down != 0ULL)
    {
        ahead = top + down;
    }
    else if (top > up)
    {
        ahead = top - up;
    }
    else
    {
        ahead = 0ULL;
    }
    const int far = (down + up) > args->view->rows;
    const unsigned long long low = (far || (ahead < top)) ? ahead : top;
    const unsigned long long high = (far || (ahead > top)) ? ahead : top;
    const unsigned long long step =
        EMBED_CALL(pages_step, PagesCtx, .one_read = args->one, .total = args->total, .cut = args->view->columns);
    const unsigned long long spread =
        CLI_PAGE_EDGE + EMBED_CALL(pages_min, PagesCtx, .one_u64 = (down + up) / CLI_PAGE_SPREAD, .other = step);
    const unsigned long long first =
        EMBED_CALL(pages_min, PagesCtx, .one_u64 = (low > spread) ? (low - spread) : 0ULL, .other = args->total);
    const unsigned long long end =
        EMBED_CALL(pages_min, PagesCtx, .one_u64 = args->total, .other = high + args->view->rows + spread);
    const int reaches = (args->view->cached_cut == 0ULL) || (args->view->cached_cut >= args->view->columns);
    const int meets = reaches && (args->view->cached_to > args->view->cached_from) &&
                      (args->view->cached_from <= end) && (args->view->cached_to >= first);
    const unsigned long long saved_from = (top > (CLI_PAGE_SAVED / 2ULL)) ? (top - (CLI_PAGE_SAVED / 2ULL)) : 0ULL;
    const unsigned long long saved_to = EMBED_CALL(pages_min, PagesCtx, .one_u64 = args->total,
                                                   .other = top + args->view->rows + (CLI_PAGE_SAVED / 2ULL));
    CliPagesPositioned positioned = {first, first, args->view->columns, saved_from, saved_to};
    const int below =
        (up == 0ULL) && (args->view->cached_to < args->total) && (args->view->cached_to < (end + CLI_PAGE_ADJACENT));
    const int above =
        (up != 0ULL) && (args->view->cached_from > 0ULL) && ((args->view->cached_from + CLI_PAGE_ADJACENT) > first);
    if (!meets)
    {
        positioned.to = end;
        positioned.save_from = first;
        positioned.save_to = end;
        EMBED_CALL(pages_rows_trim, PagesCtx, .one = args->one, .width = args->view->width, .pivot = low,
                   .from_pointer = &positioned.from, .to_pointer = &positioned.to);
    }
    else if (below || (args->view->cached_to < end))
    {
        positioned.from = args->view->cached_to;
        positioned.to =
            EMBED_CALL(pages_min, PagesCtx, .one_u64 = args->total,
                       .other = EMBED_CALL(pages_max, PagesCtx, .one_u64 = end, .other = args->view->cached_to + step));
        positioned.save_to = EMBED_CALL(pages_max, PagesCtx, .one_u64 = saved_to, .other = positioned.to);
        EMBED_CALL(pages_step_trim, PagesCtx, .one = args->one, .below = 1, .moving = 1, .cut = args->view->columns,
                   .width = args->view->width, .from_pointer = &positioned.from, .to_pointer = &positioned.to);
    }
    else if (above || (args->view->cached_from > first))
    {
        positioned.from =
            EMBED_CALL(pages_min, PagesCtx, .one_u64 = first,
                       .other = (args->view->cached_from > step) ? (args->view->cached_from - step) : 0ULL);
        positioned.to = args->view->cached_from;
        positioned.save_from = EMBED_CALL(pages_min, PagesCtx, .one_u64 = saved_from, .other = positioned.from);
        EMBED_CALL(pages_step_trim, PagesCtx, .one = args->one, .below = 0, .moving = 1, .cut = args->view->columns,
                   .width = args->view->width, .from_pointer = &positioned.from, .to_pointer = &positioned.to);
    }
    return positioned;
}

// a resting view of `one`, a text of `total` lines, placed: its rows whole, EDGE about them, where the page does not
// hold them whole; then the lines it wants below them, then above, a step at a time; nothing where it holds them all.
// It wants ADJACENT lines about its rows and the minimap's lines, within half of SAVED on either side of its rows, and
// saves those and every line within half of SAVED. Its rows, where it wraps, are the lines they take of those about
// them; its rows whole come from their first a step at a time, and each step is as many lines as
// CLI_PAGE_REMAINDER_BYTES of their bytes reach
EMBED_INLINE CliPagesPositioned pages_remainder_position(const PagesCtx *args)
{
    const unsigned long long top = EMBED_CALL(pages_min, PagesCtx, .one_u64 = args->view->top,
                                              .other = (args->total != 0ULL) ? (args->total - 1ULL) : 0ULL);
    const unsigned long long rows_end =
        EMBED_CALL(pages_min, PagesCtx, .one_u64 = args->total, .other = top + args->view->rows);
    const unsigned long long saved_from = (top > (CLI_PAGE_SAVED / 2ULL)) ? (top - (CLI_PAGE_SAVED / 2ULL)) : 0ULL;
    const unsigned long long saved_to =
        EMBED_CALL(pages_min, PagesCtx, .one_u64 = args->total, .other = rows_end + (CLI_PAGE_SAVED / 2ULL));
    const unsigned long long adjacent_from = (top > CLI_PAGE_ADJACENT) ? (top - CLI_PAGE_ADJACENT) : 0ULL;
    const unsigned long long want_from =
        EMBED_CALL(pages_max, PagesCtx,
                   .one_u64 = EMBED_CALL(pages_min, PagesCtx, .one_u64 = adjacent_from, .other = args->view->map_from),
                   .other = saved_from);
    const unsigned long long want_to =
        EMBED_CALL(pages_min, PagesCtx,
                   .one_u64 = EMBED_CALL(pages_max, PagesCtx, .one_u64 = rows_end + CLI_PAGE_ADJACENT,
                                         .other = args->view->map_to),
                   .other = saved_to);
    unsigned long long first = (top > CLI_PAGE_EDGE) ? (top - CLI_PAGE_EDGE) : 0ULL;
    unsigned long long end = EMBED_CALL(pages_min, PagesCtx, .one_u64 = args->total, .other = rows_end + CLI_PAGE_EDGE);
    EMBED_CALL(pages_rows_trim, PagesCtx, .one = args->one, .width = args->view->width, .pivot = top,
               .from_pointer = &first, .to_pointer = &end);
    const unsigned long long step =
        EMBED_CALL(pages_step, PagesCtx, .one_read = args->one, .total = args->total, .cut = 0ULL);
    CliPagesPositioned positioned = {first, first, 0ULL, saved_from, saved_to};
    if ((args->view->cached_cut != 0ULL) || (args->view->cached_from > first) || (args->view->cached_to <= first))
    {
        positioned.to = end;
        EMBED_CALL(pages_step_trim, PagesCtx, .one = args->one, .below = 1, .moving = 0, .cut = 0ULL, .width = 0ULL,
                   .from_pointer = &positioned.from, .to_pointer = &positioned.to);
    }
    else if (args->view->cached_to < end)
    {
        positioned.from = args->view->cached_to;
        positioned.to = end;
        EMBED_CALL(pages_step_trim, PagesCtx, .one = args->one, .below = 1, .moving = 0, .cut = 0ULL, .width = 0ULL,
                   .from_pointer = &positioned.from, .to_pointer = &positioned.to);
    }
    else if (args->view->cached_to < want_to)
    {
        positioned.from = args->view->cached_to;
        positioned.to = EMBED_CALL(pages_min, PagesCtx, .one_u64 = want_to, .other = args->view->cached_to + step);
        EMBED_CALL(pages_step_trim, PagesCtx, .one = args->one, .below = 1, .moving = 0, .cut = 0ULL,
                   .width = args->view->width, .from_pointer = &positioned.from, .to_pointer = &positioned.to);
    }
    else if (args->view->cached_from > want_from)
    {
        positioned.from = ((args->view->cached_from - want_from) > step) ? (args->view->cached_from - step) : want_from;
        positioned.to = args->view->cached_from;
        EMBED_CALL(pages_step_trim, PagesCtx, .one = args->one, .below = 0, .moving = 0, .cut = 0ULL,
                   .width = args->view->width, .from_pointer = &positioned.from, .to_pointer = &positioned.to);
    }
    return positioned;
}

int cli_pages_on_view(const CliPagesViewCfg *args)
{
    CliPagesPositioned *positioned = args->positioned;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    CliPaged *const one =
        EMBED_CALL(cli_pages_err_msg_internal.paged_get, CliPagesErrMsgCtx, .key = args->key, .err_msg = &err_msg);
    if (one != NULL)
    {
        int done = 0;
        const unsigned long long total = EMBED_CALL(pages_total, PagesCtx, .one = one, .done = &done);
        const unsigned long long now = cli_clock_milliseconds();
        const int follows = one->viewed && ((now - one->view_at) <= CLI_PAGE_STILL);
        const int rests = !follows || (args->view->top == one->view_top);
        *positioned =
            rests ? EMBED_CALL(pages_remainder_position, PagesCtx, .one = one, .view = args->view, .total = total)
                  : EMBED_CALL(pages_move_position, PagesCtx, .one = one, .view = args->view, .last = one->view_top,
                               .follows = follows, .total = total);
        one->viewed = 1;
        one->view_top = args->view->top;
        one->view_at = now;
    }
    return one != NULL;
}

void cli_pages_release(const CliPagesCfg *args)
{
    CliPaged *one = g_cli_paged;
    while ((one != NULL) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = one->key, .other = args->key, .cap = CLI_POOL_BYTES})))
    {
        one = one->next;
    }
    if (one != NULL)
    {
        EMBED_CALL(pages_let_go, PagesCtx, .one = one);
    }
}
