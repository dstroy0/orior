// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_read.c: a file read for the editor: its text where it is text, its first bytes where it is not, and a larger
// text read a window of whole lines at a time
#include "../text/json_tree.h"
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

// whether the first bytes hold a 0
static int cli_probe_zero(const unsigned char *bytes, unsigned long long size)
{
    const unsigned long long look = (size < CLI_PROBE_BYTES) ? size : CLI_PROBE_BYTES;
    return (look != 0ull) && (memor.chr(&(MemoriaCfg){.src = bytes, .val = 0, .bytes = look}) != NULL);
}

// a file read written as the window reads it: its path and size, its text or its first bytes, and whether it opens as
// a window
static void cli_opened_written(CliLine *out, const char *path, unsigned long long size, const unsigned char *text,
                               const unsigned char *bytes, unsigned long long length, int windowed)
{
    cli_line_text(out, "{\"path\":");
    cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"size\":");
    cli_line_decimal(out, size, 0u);
    cli_line_text(out, ",\"text\":");
    if (text != NULL)
    {
        cli_json_quoted(out, (const char *)text, length);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, ",\"bytes\":");
    if (bytes != NULL)
    {
        cli_line_character(out, '[');
        for (unsigned long long at = 0ull; at < length; at += 1ull)
        {
            if (at != 0ull)
            {
                cli_line_character(out, ',');
            }
            cli_line_decimal(out, bytes[at], 0u);
        }
        cli_line_character(out, ']');
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_text(out, windowed ? ",\"windowed\":true}" : ",\"windowed\":false}");
}

// a file within an archive read out of it whole and written as a file read
static int cli_archived_read(const char *root, const char *file, CliLine *out, CliLine *said)
{
    static char s_bytes[CLI_ARCHIVE_BYTES];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    CliLine read = {s_bytes, sizeof(s_bytes), 0ull};
    if (!cli_archive_read(root, file, &read, said))
    {
        cli_lock_give(&s_lock);
        return 0;
    }
    const unsigned char *const bytes = (const unsigned char *)s_bytes;
    const unsigned long long size = read.at;
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
    char path[CLI_PATH_LONGEST];
    const unsigned long long kept = (length < CLI_PATH_LONGEST) ? length : (CLI_PATH_LONGEST - 1ull);
    memor.cpy(&(MemoriaCfg){.dst = path, .src = from, .bytes = kept});
    path[kept] = '\0';
    if (!cli_probe_zero(bytes, size) && cli_utf8(bytes, size, 0))
    {
        cli_opened_written(out, path, size, bytes, NULL, size, 0);
    }
    else
    {
        cli_opened_written(out, path, size, NULL, bytes, (size < CLI_SHOWN_BYTES) ? size : CLI_SHOWN_BYTES, 0);
    }
    cli_lock_give(&s_lock);
    return 1;
}

// that a file could not be read, said
static void cli_unread(const char *file, CliLine *said)
{
    cli_line_text(said, file);
    cli_line_text(said, ": it could not be read");
}

// the file at `file` inside the tree mapped whole, its full path into `path`: its bytes, or NULL with why said
static const unsigned char *cli_mapped(const char *root, const char *file, char *path, unsigned long long *size,
                                       CliLine *said)
{
    if (!cli_inside(root, file, path, said))
    {
        return NULL;
    }
    const unsigned char *const bytes = qry_member_file_mapped(path, size);
    if (bytes == NULL)
    {
        cli_unread(file, said);
    }
    return bytes;
}

int cli_files_read(const char *root, const char *file, CliLine *out, CliLine *said)
{
    char archive[CLI_PATH_LONGEST];
    char inner[CLI_PATH_LONGEST];
    if (cli_archive_split(root, file, archive, inner) && (inner[0] != '\0'))
    {
        return cli_archived_read(root, file, out, said);
    }
    char path[CLI_PATH_LONGEST];
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_mapped(root, file, path, &size, said);
    if (bytes == NULL)
    {
        return 0;
    }
    char shown_path[CLI_PATH_LONGEST];
    cli_relative(root, path, shown_path);
    const unsigned long long shown = (size < CLI_SHOWN_BYTES) ? size : CLI_SHOWN_BYTES;
    if (size <= CLI_WINDOW_FROM)
    {
        if (!cli_probe_zero(bytes, size) && cli_utf8(bytes, size, 0))
        {
            cli_opened_written(out, shown_path, size, bytes, NULL, size, 0);
        }
        else
        {
            cli_opened_written(out, shown_path, size, NULL, bytes, shown, 0);
        }
        qry_member_file_unmapped(bytes, size);
        return 1;
    }
    // text where the first bytes hold no 0 and read as UTF-8, a letter the cut splits aside
    const unsigned long long probe = (shown < CLI_PROBE_BYTES) ? shown : CLI_PROBE_BYTES;
    const int text = !cli_probe_zero(bytes, shown) && cli_utf8(bytes, probe, 1);
    cli_opened_written(out, shown_path, size, NULL, text ? NULL : bytes, text ? 0ull : shown, text);
    qry_member_file_unmapped(bytes, size);
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

// whole lines written as the window reads them: the bytes from `start` to `end`, the file's size, the number of the
// first line, and the lines in the whole file
static void cli_slice_written(CliLine *out, const unsigned char *bytes, unsigned long long start,
                              unsigned long long end, unsigned long long size, unsigned long long line,
                              unsigned long long lines)
{
    cli_line_text(out, "{\"start\":");
    cli_line_decimal(out, start, 0u);
    cli_line_text(out, ",\"end\":");
    cli_line_decimal(out, end, 0u);
    cli_line_text(out, ",\"size\":");
    cli_line_decimal(out, size, 0u);
    cli_line_text(out, ",\"line\":");
    cli_line_decimal(out, line, 0u);
    cli_line_text(out, ",\"lines\":");
    cli_line_decimal(out, lines, 0u);
    cli_line_text(out, ",\"text\":");
    cli_json_quoted_lossy(out, bytes + start, end - start);
    cli_line_character(out, '}');
}

int cli_files_window(const char *root, const char *file, unsigned long long line, unsigned long long half, CliLine *out,
                     CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_mapped(root, file, path, &size, said);
    if (bytes == NULL)
    {
        return 0;
    }
    // where line `line` starts, counted from 0; past the end of the file, where the file's last line starts
    unsigned long long seen = 0ull;
    unsigned long long last_start = 0ull;
    for (unsigned long long at = 0ull; (seen < line) && (at < size); at += 1ull)
    {
        if (bytes[at] == '\n')
        {
            seen += 1ull;
            last_start = at + 1ull;
        }
    }
    const unsigned long long target = (last_start < size) ? last_start : size;
    const unsigned long long target_line = (seen < line) ? seen : line;
    const unsigned long long back = (target > half) ? (target - half) : 0ull;
    const unsigned long long start_at = cli_line_start(bytes, back, size);
    const unsigned long long start = (start_at < target) ? start_at : target;
    const unsigned long long ahead = ((size - target) > half) ? (target + half) : size;
    const unsigned long long end = cli_line_start(bytes, ahead, size);
    const unsigned long long first = target_line - cli_newlines(bytes, start, target);
    cli_slice_written(out, bytes, start, (end > start) ? end : start, size, first,
                      cli_newlines(bytes, 0ull, size) + 1ull);
    qry_member_file_unmapped(bytes, size);
    return 1;
}

int cli_files_slice(const char *root, const char *file, unsigned long long start, unsigned long long end, CliLine *out,
                    CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_mapped(root, file, path, &size, said);
    if (bytes == NULL)
    {
        return 0;
    }
    const unsigned long long from = cli_line_start(bytes, start, size);
    const unsigned long long to_at = cli_line_start(bytes, end, size);
    const unsigned long long to = (to_at > from) ? to_at : from;
    cli_slice_written(out, bytes, from, to, size, 0ull, 0ull);
    qry_member_file_unmapped(bytes, size);
    return 1;
}
