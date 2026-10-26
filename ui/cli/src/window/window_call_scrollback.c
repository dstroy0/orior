// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_scrollback.c: a terminal screen's scrollback on disk, as a large file is: every line that scrolls off the
// top of the screen, as the page drew it, one to a line of a temporary file, and where each line starts, eight bytes to
// a line of another. The page holds only the lines nearest the screen and reads the rest back from here as the reader
// scrolls to them; this file holds neither the lines nor where they start, only where each file ends. The system
// deletes both files as they are let go or the window ends, and a crash leaves nothing behind
#include "window_internal.h"

// the most scrollbacks open at once, one a terminal screen; the most lines one keep hands over; the bytes lines are
// written and read in, a piece at a time; and the starts of lines read or written at once
#define CLI_SCROLLBACKS 64u
#define CLI_SCROLLBACK_LINES 65536u
#define CLI_SCROLLBACK_PIECE 65536u
#define CLI_SCROLLBACK_STARTS 4096u

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

static CliScrollback s_scrollback[CLI_SCROLLBACKS];
static unsigned long long s_next_id;
static CliLock s_lock = CLI_LOCK_INIT;
static char s_piece[CLI_SCROLLBACK_PIECE];
static unsigned long long s_starts[CLI_SCROLLBACK_STARTS];

// the scrollback numbered `id`, or NULL with why given where none is open
static CliScrollback *scrollback_of(unsigned long long id, VerbaLinea *err_msg)
{
    for (unsigned int at = 0u; (id != 0ull) && (at < CLI_SCROLLBACKS); at += 1u)
    {
        if (s_scrollback[at].id == id)
        {
            return &s_scrollback[at];
        }
    }
    mmgr_line_put(err_msg, "no scrollback ");
    mmgr_line_decimal(err_msg, id, 1u);
    return NULL;
}

// a scrollback let go, its files deleted
static void scrollback_let_go(CliScrollback *scrollback)
{
    cli_temporary_file_let_go(&scrollback->lines);
    cli_temporary_file_let_go(&scrollback->starts);
    scrollback->id = 0ull;
}

// a scrollback made for a terminal screen: its number
static int scrollback_open(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    cli_lock_take(&s_lock);
    CliScrollback *free_place = NULL;
    for (unsigned int at = 0u; (free_place == NULL) && (at < CLI_SCROLLBACKS); at += 1u)
    {
        free_place = (s_scrollback[at].id == 0ull) ? &s_scrollback[at] : NULL;
    }
    const int made = (free_place != NULL) && cli_temporary_file_made(&free_place->lines);
    const int whole = made && cli_temporary_file_made(&free_place->starts);
    if (whole)
    {
        s_next_id += 1ull;
        *free_place = (CliScrollback){s_next_id, free_place->lines, free_place->starts, 0ull, 0ull};
        mmgr_line_decimal(out, s_next_id, 1u);
    }
    else
    {
        if (made)
        {
            cli_temporary_file_let_go(&free_place->lines);
        }
        mmgr_line_put(err_msg, (free_place == NULL) ? "every scrollback is taken" : "a scrollback's file could not be made");
    }
    cli_lock_give(&s_lock);
    return whole;
}

// The lines a keep writes, a piece at a time: the scrollback, the bytes of the piece and of the starts gathered, and
// whether every write so far landed
typedef struct
{
    CliScrollback *scrollback;
    unsigned long long piece;
    unsigned int starts;
    int written;
} CliScrollbackKeep;

// the piece and the starts gathered written at the ends of their files
static void scrollback_flushed(CliScrollbackKeep *keep)
{
    CliScrollback *const scrollback = keep->scrollback;
    keep->written = keep->written && cli_temporary_file_written(&scrollback->lines, scrollback->end, s_piece, keep->piece);
    scrollback->end += keep->piece;
    keep->piece = 0ull;
    const unsigned long long first = scrollback->count - keep->starts;
    keep->written = keep->written && cli_temporary_file_written(&scrollback->starts, first * sizeof(s_starts[0]),
                                                                s_starts, keep->starts * sizeof(s_starts[0]));
    keep->starts = 0u;
}

// `line` added after the lines kept, each newline in it a space and a newline after it
static void scrollback_line_kept(CliScrollbackKeep *keep, const char *line)
{
    if (keep->starts == CLI_SCROLLBACK_STARTS)
    {
        scrollback_flushed(keep);
    }
    s_starts[keep->starts] = keep->scrollback->end + keep->piece;
    keep->starts += 1u;
    keep->scrollback->count += 1u;
    for (const char *at = line;; at += 1)
    {
        if (keep->piece == CLI_SCROLLBACK_PIECE)
        {
            scrollback_flushed(keep);
        }
        s_piece[keep->piece] = ((*at == '\n') || (*at == '\0')) ? ((*at == '\0') ? '\n' : ' ') : *at;
        keep->piece += 1ull;
        if (*at == '\0')
        {
            return;
        }
    }
}

// the lines `lines` added at the end of the scrollback `id`: how many lines it holds
static int scrollback_keep(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned long long id = 0ull;
    unsigned int count = 0u;
    const char **lines = NULL;
    if (!cli_serve_whole(args, "id", &id, err_msg) ||
        !cli_serve_strings_taken(args, "lines", CLI_SCROLLBACK_LINES, &lines, &count, err_msg))
    {
        cli_serve_strings_given(lines);
        return 0;
    }
    cli_lock_take(&s_lock);
    CliScrollback *const scrollback = scrollback_of(id, err_msg);
    CliScrollbackKeep keep = {scrollback, 0ull, 0u, 1};
    for (unsigned int at = 0u; (scrollback != NULL) && (at < count); at += 1u)
    {
        scrollback_line_kept(&keep, lines[at]);
    }
    if (scrollback != NULL)
    {
        scrollback_flushed(&keep);
        if (keep.written)
        {
            mmgr_line_decimal(out, scrollback->count, 1u);
        }
        else
        {
            mmgr_line_put(err_msg, "the scrollback's file could not be written");
        }
    }
    cli_lock_give(&s_lock);
    cli_serve_strings_given(lines);
    return (scrollback != NULL) && keep.written;
}

// the bytes of the scrollback's lines from `begin` to `finish` written as a JSON string, read a piece at a time, each
// piece cut where a letter starts: 1, or 0 where they could not be read
static int scrollback_line_written(const CliScrollback *scrollback, unsigned long long begin, unsigned long long finish,
                                   VerbaLinea *out)
{
    mmgr_line_ch(out, '"');
    for (unsigned long long at = begin; at < finish;)
    {
        unsigned long long size = ((finish - at) < CLI_SCROLLBACK_PIECE) ? (finish - at) : CLI_SCROLLBACK_PIECE;
        if (!cli_temporary_file_read(&scrollback->lines, at, s_piece, size))
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
        const int cut = (at + size < finish) && (lead > 0ull) && (lead + letter > size);
        size = cut ? lead : size;
        cli_json_escaped_lossy(out, (const unsigned char *)s_piece, size);
        at += size;
    }
    mmgr_line_ch(out, '"');
    return 1;
}

// the lines of the scrollback `id` from line `from`, as many as `count` and as many as it holds
static int scrollback_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned long long id = 0ull;
    unsigned long long from = 0ull;
    unsigned long long count = 0ull;
    if (!cli_serve_whole(args, "id", &id, err_msg) || !cli_serve_whole(args, "from", &from, err_msg) ||
        !cli_serve_whole(args, "count", &count, err_msg))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    const CliScrollback *const scrollback = scrollback_of(id, err_msg);
    int read = scrollback != NULL;
    const unsigned long long held = read ? scrollback->count : 0ull;
    const unsigned long long first = (from < held) ? from : held;
    const unsigned long long last = ((count < held - first) ? first + count : held);
    mmgr_line_ch(out, '[');
    for (unsigned long long batch = first; read && (batch < last);)
    {
        // the starts of the batch's lines and of the line after, the end of the lines where none is after
        const unsigned long long lines =
            ((last - batch) < (CLI_SCROLLBACK_STARTS - 1u)) ? (last - batch) : (CLI_SCROLLBACK_STARTS - 1u);
        const unsigned long long known = (batch + lines < held) ? lines + 1ull : lines;
        read = cli_temporary_file_read(&scrollback->starts, batch * sizeof(s_starts[0]), s_starts,
                                       known * sizeof(s_starts[0]));
        s_starts[lines] = (known > lines) ? s_starts[lines] : scrollback->end;
        for (unsigned long long line = 0ull; read && (line < lines); line += 1ull)
        {
            mmgr_line_put(out, ((batch + line) > first) ? "," : "");
            read = scrollback_line_written(scrollback, s_starts[line], s_starts[line + 1ull] - 1ull, out);
        }
        batch += lines;
    }
    mmgr_line_ch(out, ']');
    if ((scrollback != NULL) && !read)
    {
        mmgr_line_put(err_msg, "the scrollback's file could not be read");
    }
    cli_lock_give(&s_lock);
    return read;
}

// the scrollback `id` let go, which deletes its files
static int scrollback_close(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    char why[128];
    VerbaLinea unknown = {why, sizeof(why), 0ull};
    unsigned long long id = 0ull;
    if (!cli_serve_whole(args, "id", &id, err_msg))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    CliScrollback *const scrollback = scrollback_of(id, &unknown);
    if (scrollback != NULL)
    {
        scrollback_let_go(scrollback);
    }
    cli_lock_give(&s_lock);
    return 1;
}

// every scrollback let go, as a page loaded again asks: the screens they were for are gone
static int scrollback_reset(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)out;
    (void)err_msg;
    cli_lock_take(&s_lock);
    for (unsigned int at = 0u; at < CLI_SCROLLBACKS; at += 1u)
    {
        if (s_scrollback[at].id != 0ull)
        {
            scrollback_let_go(&s_scrollback[at]);
        }
    }
    cli_lock_give(&s_lock);
    return 1;
}

const CliServeCall g_cli_window_scrollback_calls[] = {
    {"scrollback_open", scrollback_open},   {"scrollback_keep", scrollback_keep},
    {"scrollback_read", scrollback_read},   {"scrollback_close", scrollback_close},
    {"scrollback_reset", scrollback_reset},
};
const unsigned int g_cli_window_scrollback_count =
    sizeof(g_cli_window_scrollback_calls) / sizeof(g_cli_window_scrollback_calls[0]);
