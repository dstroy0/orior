// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_scrollback.c: a terminal screen's scrollback on disk, as a large file is: every line that scrolls off the
// top of the screen, as the page drew it, one to a line of a temporary file, and where each line starts, eight bytes to
// a line of another. The page holds only the lines nearest the screen and reads the rest back from here as the reader
// scrolls to them; this file holds neither the lines nor where they start, only where each file ends. The system
// deletes both files as they are let go or the window ends, and a crash leaves nothing behind
#include "window_internal.h"

// One screen's scrollback: its number, 0 where the place is free; its lines and where each starts; the bytes of its
// lines; and how many it holds
typedef struct
{
    unsigned long long id;
    CliTemporaryFile lines;
    CliTemporaryFile starts;
    unsigned long long end;
    unsigned long long count;
} CliScrollback;
// The lines a save writes, a piece at a time: the scrollback, the bytes of the piece and of the starts gathered, and
// whether every write so far landed
typedef struct
{
    CliScrollback *scrollback;
    unsigned long long piece;
    unsigned int starts;
    int written;
} CliScrollbackSave;
// the most scrollbacks open at once, one a terminal screen; the most lines one save hands over; the bytes lines are
// written and read in, a piece at a time; and the starts of lines read or written at once
#define CLI_SCROLLBACKS 64u
#define CLI_SCROLLBACK_LINES 65536u
#define CLI_SCROLLBACK_PIECE 65536u
#define CLI_SCROLLBACK_STARTS 4096u

static CliScrollback s_scrollback[CLI_SCROLLBACKS];
static unsigned long long s_next_id;
static char s_piece[CLI_SCROLLBACK_PIECE];
static unsigned long long s_starts[CLI_SCROLLBACK_STARTS];

typedef struct
{
    unsigned long long id;
    VerbaLinea *err_msg;
    CliScrollback *scrollback;
    CliScrollbackSave *save;
    const char *line;
    const CliScrollback *scrollback_read;
    unsigned long long begin;
    unsigned long long finish;
    VerbaLinea *out;
} WindowCallScrollbackCtx;
// the scrollback numbered `id`, or NULL with why given where none is open
EMBED_INLINE CliScrollback *scrollback_get(const WindowCallScrollbackCtx *args)
{
    for (unsigned int at = 0u; (args->id != 0ull) && (at < CLI_SCROLLBACKS); at += 1u)
    {
        if (s_scrollback[at].id == args->id)
        {
            return &s_scrollback[at];
        }
    }
    mmgr_line_put(args->err_msg, "no scrollback ");
    mmgr_line_decimal(args->err_msg, args->id, 1u);
    return NULL;
}

// a scrollback let go, its files deleted
EMBED_INLINE void scrollback_let_go(const WindowCallScrollbackCtx *args)
{
    EMBED_CALL(cli_file_temporary_file_internal.temporary_file_let_go, CliFileTemporaryFileCtx,
               .file_temporary_file_pointer = &args->scrollback->lines);
    EMBED_CALL(cli_file_temporary_file_internal.temporary_file_let_go, CliFileTemporaryFileCtx,
               .file_temporary_file_pointer = &args->scrollback->starts);
    args->scrollback->id = 0ull;
}

// a scrollback made for a terminal screen: its number
static int scrollback_open(const CliServeAnswerCfg *args)
{
    CliScrollback *free_place = NULL;
    for (unsigned int at = 0u; (free_place == NULL) && (at < CLI_SCROLLBACKS); at += 1u)
    {
        free_place = (s_scrollback[at].id == 0ull) ? &s_scrollback[at] : NULL;
    }
    const int made =
        (free_place != NULL) && EMBED_CALL(cli_file_temporary_file_internal.temporary_file_init,
                                           CliFileTemporaryFileCtx, .file_temporary_file_pointer = &free_place->lines);
    const int whole = made && EMBED_CALL(cli_file_temporary_file_internal.temporary_file_init, CliFileTemporaryFileCtx,
                                         .file_temporary_file_pointer = &free_place->starts);
    if (whole)
    {
        s_next_id += 1ull;
        *free_place = (CliScrollback){s_next_id, free_place->lines, free_place->starts, 0ull, 0ull};
        mmgr_line_decimal(args->out, s_next_id, 1u);
    }
    else
    {
        if (made)
        {
            EMBED_CALL(cli_file_temporary_file_internal.temporary_file_let_go, CliFileTemporaryFileCtx,
                       .file_temporary_file_pointer = &free_place->lines);
        }
        mmgr_line_put(args->err_msg,
                      (free_place == NULL) ? "every scrollback is taken" : "a scrollback's file could not be made");
    }
    return whole;
}

// the piece and the starts gathered written at the ends of their files
EMBED_INLINE void scrollback_flush(const WindowCallScrollbackCtx *args)
{
    CliScrollback *const scrollback = args->save->scrollback;
    args->save->written =
        args->save->written && EMBED_CALL(cli_bytes_internal.temporary_file_write, CliBytesCtx,
                                          .file_temporary_file_pointer_other = &scrollback->lines,
                                          .at = scrollback->end, .bytes = s_piece, .size = args->save->piece);
    scrollback->end += args->save->piece;
    args->save->piece = 0ull;
    const unsigned long long first = scrollback->count - args->save->starts;
    args->save->written = args->save->written && EMBED_CALL(cli_bytes_internal.temporary_file_write, CliBytesCtx,
                                                            .file_temporary_file_pointer_other = &scrollback->starts,
                                                            .at = first * sizeof(s_starts[0]), .bytes = s_starts,
                                                            .size = args->save->starts * sizeof(s_starts[0]));
    args->save->starts = 0u;
}

// `line` added after the lines saved, each newline in it a space and a newline after it
EMBED_INLINE void scrollback_line_save(const WindowCallScrollbackCtx *args)
{
    if (args->save->starts == CLI_SCROLLBACK_STARTS)
    {
        scrollback_flush(args);
    }
    s_starts[args->save->starts] = args->save->scrollback->end + args->save->piece;
    args->save->starts += 1u;
    args->save->scrollback->count += 1u;
    for (const char *at = args->line;; at += 1)
    {
        if (args->save->piece == CLI_SCROLLBACK_PIECE)
        {
            scrollback_flush(args);
        }
        s_piece[args->save->piece] = ((*at == '\n') || (*at == '\0')) ? ((*at == '\0') ? '\n' : ' ') : *at;
        args->save->piece += 1ull;
        if (*at == '\0')
        {
            return;
        }
    }
}

// the lines `lines` added at the end of the scrollback `id`: how many lines it holds
static int scrollback_save(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long id = 0ull;
    unsigned int count = 0u;
    const char **lines = NULL;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "id",
                    .value = &id, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.strings_take, CliServeErrMsgCtx, .request = arguments, .key = "lines",
                    .most = CLI_SCROLLBACK_LINES, .items_pointer = &lines, .count = &count, .err_msg = err_msg))
    {
        EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = lines);
        return 0;
    }
    CliScrollback *const scrollback = EMBED_CALL(scrollback_get, WindowCallScrollbackCtx, .id = id, .err_msg = err_msg);
    CliScrollbackSave save = {scrollback, 0ull, 0u, 1};
    for (unsigned int at = 0u; (scrollback != NULL) && (at < count); at += 1u)
    {
        EMBED_CALL(scrollback_line_save, WindowCallScrollbackCtx, .save = &save, .line = lines[at]);
    }
    if (scrollback != NULL)
    {
        EMBED_CALL(scrollback_flush, WindowCallScrollbackCtx, .save = &save);
        if (save.written)
        {
            mmgr_line_decimal(args->out, scrollback->count, 1u);
        }
        else
        {
            mmgr_line_put(err_msg, "the scrollback's file could not be written");
        }
    }
    EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = lines);
    return (scrollback != NULL) && save.written;
}

// the bytes of the scrollback's lines from `begin` to `finish` written as a JSON string, read a piece at a time, each
// piece cut where a letter starts: 1, or 0 where they could not be read
EMBED_INLINE int scrollback_line_write(const WindowCallScrollbackCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '"');
    for (unsigned long long at = args->begin; at < args->finish;)
    {
        unsigned long long size =
            ((args->finish - at) < CLI_SCROLLBACK_PIECE) ? (args->finish - at) : CLI_SCROLLBACK_PIECE;
        if (!EMBED_CALL(cli_size_internal.temporary_file_read, CliSizeCtx,
                        .file_temporary_file_pointer_other = &args->scrollback_read->lines, .at = at,
                        .bytes_out = s_piece, .size = size))
        {
            return 0;
        }
        // a piece that is not the line's last ends before its last letter where the piece's end cuts that letter
        unsigned long long lead = size - 1ull;
        while ((lead > 0ull) && ((((unsigned char)s_piece[lead]) & 0xc0u) == 0x80u))
        {
            lead -= 1ull;
        }
        const unsigned char first_byte = (unsigned char)s_piece[lead];
        const unsigned long long letter =
            (first_byte >= 0xf0u) ? 4ull : ((first_byte >= 0xe0u) ? 3ull : ((first_byte >= 0xc0u) ? 2ull : 1ull));
        const int cut = (at + size < args->finish) && (lead > 0ull) && (lead + letter > size);
        size = cut ? lead : size;
        EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)s_piece, .length = size);
        at += size;
    }
    mmgr_line_ch(out, '"');
    return 1;
}

// the lines of the scrollback `id` from line `from`, as many as `count` and as many as it holds
static int scrollback_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long id = 0ull;
    unsigned long long from = 0ull;
    unsigned long long count = 0ull;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "id",
                    .value = &id, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "from",
                    .value = &from, .err_msg = err_msg) ||
        !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "count",
                    .value = &count, .err_msg = err_msg))
    {
        return 0;
    }
    const CliScrollback *const scrollback =
        EMBED_CALL(scrollback_get, WindowCallScrollbackCtx, .id = id, .err_msg = err_msg);
    int read = scrollback != NULL;
    const unsigned long long stored = read ? scrollback->count : 0ull;
    const unsigned long long first = (from < stored) ? from : stored;
    const unsigned long long last = ((count < stored - first) ? first + count : stored);
    mmgr_line_ch(out, '[');
    for (unsigned long long batch = first; read && (batch < last);)
    {
        // the starts of the batch's lines and of the line after, the end of the lines where none is after
        const unsigned long long lines =
            ((last - batch) < (CLI_SCROLLBACK_STARTS - 1u)) ? (last - batch) : (CLI_SCROLLBACK_STARTS - 1u);
        const unsigned long long known = (batch + lines < stored) ? lines + 1ull : lines;
        read = EMBED_CALL(cli_size_internal.temporary_file_read, CliSizeCtx,
                          .file_temporary_file_pointer_other = &scrollback->starts, .at = batch * sizeof(s_starts[0]),
                          .bytes_out = s_starts, .size = known * sizeof(s_starts[0]));
        s_starts[lines] = (known > lines) ? s_starts[lines] : scrollback->end;
        for (unsigned long long line = 0ull; read && (line < lines); line += 1ull)
        {
            mmgr_line_put(out, ((batch + line) > first) ? "," : "");
            read = EMBED_CALL(scrollback_line_write, WindowCallScrollbackCtx, .scrollback_read = scrollback,
                              .begin = s_starts[line], .finish = s_starts[line + 1ull] - 1ull, .out = out);
        }
        batch += lines;
    }
    mmgr_line_ch(out, ']');
    if ((scrollback != NULL) && !read)
    {
        mmgr_line_put(err_msg, "the scrollback's file could not be read");
    }
    return read;
}

// the scrollback `id` let go, which deletes its files
static int scrollback_close(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    char why[128];
    VerbaLinea unknown = {why, sizeof(why), 0ull};
    unsigned long long id = 0ull;
    if (!EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments, .key = "id",
                    .value = &id, .err_msg = err_msg))
    {
        return 0;
    }
    CliScrollback *const scrollback =
        EMBED_CALL(scrollback_get, WindowCallScrollbackCtx, .id = id, .err_msg = &unknown);
    if (scrollback != NULL)
    {
        EMBED_CALL(scrollback_let_go, WindowCallScrollbackCtx, .scrollback = scrollback);
    }
    return 1;
}

// every scrollback let go, as a page loaded again asks: the screens they were for are gone
static int scrollback_reset(const CliServeAnswerCfg *args)
{
    (void)args;
    for (unsigned int at = 0u; at < CLI_SCROLLBACKS; at += 1u)
    {
        if (s_scrollback[at].id != 0ull)
        {
            EMBED_CALL(scrollback_let_go, WindowCallScrollbackCtx, .scrollback = &s_scrollback[at]);
        }
    }
    return 1;
}

const CliServeCall g_cli_window_scrollback_calls[] = {
    {"scrollback_open", scrollback_open},   {"scrollback_keep", scrollback_save},
    {"scrollback_read", scrollback_read},   {"scrollback_close", scrollback_close},
    {"scrollback_reset", scrollback_reset},
};
const unsigned int g_cli_window_scrollback_count =
    sizeof(g_cli_window_scrollback_calls) / sizeof(g_cli_window_scrollback_calls[0]);
