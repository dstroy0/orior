// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_read.c: a file read for the editor: its text where it is text, its first bytes where it is not, and a larger
// text read a window of whole lines at a time
#include "../text/json_tree.h"
#include "../text/pieces.h"
#include "archive.h"
#include "files.h"

// the size past which a text file opens as a window, the most bytes of a file that is not text shown, and the bytes
// looked through for a 0 that marks a file as no text
#define CLI_WINDOW_FROM (1024ull * 1024ull)
#define CLI_SHOWN_BYTES (64ull * 1024ull)
#define CLI_PROBE_BYTES 8192ull

// whether a letter the end of `size` bytes cuts short starts at them: a lead byte and as many of the bytes after it as
// there are, each as UTF-8 takes it, too few
static int cli_cut_short(const unsigned char *rest, unsigned long long size)
{
    const unsigned char lead = rest[0];
    const unsigned long long need = ((lead >= 0xc2u) && (lead <= 0xdfu))   ? 2ull
                                    : ((lead >= 0xe0u) && (lead <= 0xefu)) ? 3ull
                                    : ((lead >= 0xf0u) && (lead <= 0xf4u)) ? 4ull
                                                                           : 0ull;
    if ((need == 0ull) || (size >= need))
    {
        return 0;
    }
    for (unsigned long long at = 1ull; at < size; at += 1ull)
    {
        if ((rest[at] & 0xc0u) != 0x80u)
        {
            return 0;
        }
    }
    const unsigned char second = (size > 1ull) ? rest[1] : 0x80u;
    return !((lead == 0xe0u) && (second < 0xa0u)) && !((lead == 0xedu) && (second >= 0xa0u)) &&
           !((lead == 0xf0u) && (second < 0x90u)) && !((lead == 0xf4u) && (second >= 0x90u));
}

// whether `size` bytes are UTF-8 whole, or where `cut` is 1, UTF-8 but for a letter their end cuts short
static int cli_utf8(const unsigned char *bytes, unsigned long long size, int cut)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter(bytes, size, at, &letter);
        if ((step == 1ull) && (bytes[at] >= 0x80u))
        {
            return cut && cli_cut_short(bytes + at, size - at);
        }
        at += step;
    }
    return 1;
}

// whether the first bytes contain a 0
static int cli_probe_zero(const unsigned char *bytes, unsigned long long size)
{
    const unsigned long long look = (size < CLI_PROBE_BYTES) ? size : CLI_PROBE_BYTES;
    return (look != 0ull) && (memor.chr(&(MemoriaCfg){.src = bytes, .val = 0, .bytes = look}) != NULL);
}

// a file read written as the window reads it: its path and size, its text or its first bytes, and whether it opens as
// a window
static void cli_opened_written(VerbaLinea *out, const char *path, unsigned long long size, const unsigned char *text,
                               const unsigned char *bytes, unsigned long long length, int windowed)
{
    mmgr_line_put(out, "{\"path\":");
    cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"size\":");
    mmgr_line_decimal(out, size, 0u);
    mmgr_line_put(out, ",\"text\":");
    if (text != NULL)
    {
        cli_json_quoted(out, (const char *)text, length);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"bytes\":");
    if (bytes != NULL)
    {
        mmgr_line_ch(out, '[');
        for (unsigned long long at = 0ull; at < length; at += 1ull)
        {
            if (at != 0ull)
            {
                mmgr_line_ch(out, ',');
            }
            mmgr_line_decimal(out, bytes[at], 0u);
        }
        mmgr_line_ch(out, ']');
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, windowed ? ",\"windowed\":true}" : ",\"windowed\":false}");
}

// a file within an archive read out of it whole, through pieces into a cell of its size given back once it is
// written as a file read
static int cli_archived_read(const char *root, const char *file, VerbaLinea *out, VerbaLinea *err_msg)
{
    char piece[4096];
    CliPieces pieces;
    VerbaLinea read;
    cli_pieces_begun(&pieces, &read, piece, sizeof(piece));
    if (!cli_archive_read(root, file, &read, err_msg))
    {
        cli_pieces_given(&pieces);
        return 0;
    }
    unsigned long long size = 0ull;
    char *const joined = cli_pieces_joined(&pieces, &read, "a file read out of an archive", &size);
    if (joined == NULL)
    {
        mmgr_line_put(err_msg, "the file read out of the archive could not be stored");
        return 0;
    }
    const unsigned char *const bytes = (const unsigned char *)joined;
    // the path with the slashes at either end left off
    const char *from = file;
    while (from[0] == '/')
    {
        from += 1;
    }
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
    while ((length > 0ull) && (from[length - 1ull] == '/'))
    {
        length -= 1ull;
    }
    char path[CLI_PATH_MAX_LEN];
    const unsigned long long copied_len = (length < CLI_PATH_MAX_LEN) ? length : (CLI_PATH_MAX_LEN - 1ull);
    memor.cpy(&(MemoriaCfg){.dst = path, .src = from, .bytes = copied_len});
    path[copied_len] = '\0';
    if (!cli_probe_zero(bytes, size) && cli_utf8(bytes, size, 0))
    {
        cli_opened_written(out, path, size, bytes, NULL, size, 0);
    }
    else
    {
        cli_opened_written(out, path, size, NULL, bytes, (size < CLI_SHOWN_BYTES) ? size : CLI_SHOWN_BYTES, 0);
    }
    cli_cell_release(joined, NULL);
    return 1;
}

// that a file could not be read, reported
static void cli_unread(const char *file, VerbaLinea *err_msg)
{
    mmgr_line_put(err_msg, file);
    mmgr_line_put(err_msg, ": it could not be read");
}

// the file at `file` inside the tree mapped whole, its full path into `path`: its bytes, or NULL with why given
static const unsigned char *cli_mapped(const char *root, const char *file, char *path, unsigned long long *size,
                                       VerbaLinea *err_msg)
{
    if (!cli_inside(root, file, path, err_msg))
    {
        return NULL;
    }
    const unsigned char *const bytes = cli_file_read(path, size);
    if (bytes == NULL)
    {
        cli_unread(file, err_msg);
    }
    return bytes;
}

// `count` bytes of the file `file` of `root` from `from`, stored in a cell, the file's whole size into `whole`: the
// bytes, `got` of them, or NULL with why given. A file larger than the window pages is never stored whole: its shape is
// read off its first bytes and a slice off the bytes about it, each read at its offset
static const unsigned char *cli_piece(const char *root, const char *file, char *path, unsigned long long from,
                                      unsigned long long count, unsigned long long *got, unsigned long long *whole,
                                      VerbaLinea *err_msg)
{
    if (!cli_inside(root, file, path, err_msg))
    {
        return NULL;
    }
    if (!cli_file_size(path, whole))
    {
        cli_unread(file, err_msg);
        return NULL;
    }
    const unsigned long long room = (from < *whole) ? *whole - from : 0ull;
    const unsigned char *const bytes = cli_file_piece_read(path, from, (count < room) ? count : room, got);
    if (bytes == NULL)
    {
        cli_unread(file, err_msg);
    }
    return bytes;
}

int cli_files_read(const char *root, const char *file, VerbaLinea *out, VerbaLinea *err_msg)
{
    char archive[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    if (cli_archive_split(root, file, archive, inner) && (inner[0] != '\0'))
    {
        return cli_archived_read(root, file, out, err_msg);
    }
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    if (!cli_inside(root, file, path, err_msg))
    {
        return 0;
    }
    if (!cli_file_size(path, &size))
    {
        cli_unread(file, err_msg);
        return 0;
    }
    char shown_path[CLI_PATH_MAX_LEN];
    cli_relative(root, path, shown_path);
    if (size <= CLI_WINDOW_FROM)
    {
        // a file the window takes whole is stored whole
        const unsigned char *const text = cli_mapped(root, file, path, &size, err_msg);
        if (text == NULL)
        {
            return 0;
        }
        const unsigned long long shown = (size < CLI_SHOWN_BYTES) ? size : CLI_SHOWN_BYTES;
        if (!cli_probe_zero(text, size) && cli_utf8(text, size, 0))
        {
            cli_opened_written(out, shown_path, size, text, NULL, size, 0);
        }
        else
        {
            cli_opened_written(out, shown_path, size, NULL, text, shown, 0);
        }
        cli_file_let_go(text, size);
        return 1;
    }
    // a file the window pages: its first bytes alone, read at their offset; text where they contain no 0 and read as
    // UTF-8, a letter the cut splits aside
    unsigned long long got = 0ull;
    const unsigned char *const bytes = cli_piece(root, file, path, 0ull, CLI_SHOWN_BYTES, &got, &size, err_msg);
    if (bytes == NULL)
    {
        return 0;
    }
    const unsigned long long probe = (got < CLI_PROBE_BYTES) ? got : CLI_PROBE_BYTES;
    const int text = !cli_probe_zero(bytes, got) && cli_utf8(bytes, probe, 1);
    cli_opened_written(out, shown_path, size, NULL, text ? NULL : bytes, text ? 0ull : got, text);
    cli_file_let_go(bytes, got);
    return 1;
}

// the first line start at or after `at`: `at` itself where a line ends just before it, or else just past the next
// newline, or the end of the file
static unsigned long long cli_line_start(const unsigned char *bytes, unsigned long long at, unsigned long long size)
{
    if ((at == 0ull) || (at >= size))
    {
        return (at < size) ? at : size;
    }
    const unsigned char *const newline =
        memor.chr(&(MemoriaCfg){.src = bytes + (at - 1ull), .val = '\n', .bytes = size - (at - 1ull)});
    // a pointer found in the bytes is at or past their start
    return (newline != NULL) ? ((unsigned long long)(newline - bytes) + 1ull) : size;
}

// the newlines from `from` to `to`
static unsigned long long cli_newlines(const unsigned char *bytes, unsigned long long from, unsigned long long to)
{
    unsigned long long count = 0ull;
    for (unsigned long long at = from; at < to; at += 1ull)
    {
        count += (bytes[at] == '\n') ? 1ull : 0ull;
    }
    return count;
}

// whole lines written as the window reads them: the bytes from `start` to `end` of `bytes`, which contain the file from
// its byte `base`, the file's size, the number of the first line, and the lines in the whole file
static void cli_slice_written(VerbaLinea *out, const unsigned char *bytes, unsigned long long base,
                              unsigned long long start, unsigned long long end, unsigned long long size,
                              unsigned long long line, unsigned long long lines)
{
    mmgr_line_put(out, "{\"start\":");
    mmgr_line_decimal(out, base + start, 0u);
    mmgr_line_put(out, ",\"end\":");
    mmgr_line_decimal(out, base + end, 0u);
    mmgr_line_put(out, ",\"size\":");
    mmgr_line_decimal(out, size, 0u);
    mmgr_line_put(out, ",\"line\":");
    mmgr_line_decimal(out, line, 0u);
    mmgr_line_put(out, ",\"lines\":");
    mmgr_line_decimal(out, lines, 0u);
    mmgr_line_put(out, ",\"text\":");
    cli_json_quoted_lossy(out, bytes + start, end - start);
    mmgr_line_ch(out, '}');
}

int cli_files_slice(const char *root, const char *file, unsigned long long start, unsigned long long end,
                    VerbaLinea *out, VerbaLinea *err_msg)
{
    // the bytes about the slice, from the byte before `start` so that its line's start is read, with slack past `end`
    // for the line end there, the slack doubled while the piece contains neither line end and the file goes on; a slice
    // past the file's end ends at the end
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    if (!cli_inside(root, file, path, err_msg))
    {
        return 0;
    }
    if (!cli_file_size(path, &size))
    {
        cli_unread(file, err_msg);
        return 0;
    }
    start = (start < size) ? start : size;
    end = (end < size) ? end : size;
    const unsigned long long base = (start > 0ull) ? start - 1ull : 0ull;
    unsigned long long count = ((end > base) ? end - base : 0ull) + CLI_SHOWN_BYTES;
    for (;;)
    {
        unsigned long long got = 0ull;
        unsigned long long whole = 0ull;
        const unsigned char *const bytes = cli_piece(root, file, path, base, count, &got, &whole, err_msg);
        if (bytes == NULL)
        {
            return 0;
        }
        const unsigned long long at_end = ((end > base) ? end - base : 0ull);
        const unsigned long long from = cli_line_start(bytes, start - base, got);
        const unsigned long long to_at = cli_line_start(bytes, (at_end < got) ? at_end : got, got);
        const unsigned long long to = (to_at > from) ? to_at : from;
        const int reached = (base + got >= whole) || ((from < got) && (to < got));
        if (reached)
        {
            cli_slice_written(out, bytes, base, from, to, whole, 0ull, 0ull);
            cli_file_let_go(bytes, got);
            return 1;
        }
        cli_file_let_go(bytes, got);
        count *= 2ull;
    }
}
