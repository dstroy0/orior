// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_read.c: a file read for the editor: its text where it is text, its first bytes where it is not, and a larger
// text read a window of whole lines at a time
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "archive.h"
#include "files.h"

// the size past which a text file opens as a window, the most bytes of a file that is not text shown, and the bytes
// looked through for a 0 that marks a file as no text
#define CLI_WINDOW_FROM (1024ull * 1024ull)
#define CLI_SHOWN_BYTES (64ull * 1024ull)
#define CLI_SLICE_SLACK (1ull << 20u)
#define CLI_PROBE_BYTES 8192ull

typedef struct
{
    const unsigned char *rest;
    unsigned long long size;
    const unsigned char *bytes;
    int cut;
    VerbaLinea *out;
    const char *path_read;
    const unsigned char *text;
    unsigned long long length;
    int windowed;
    const char *root;
    const char *file;
    VerbaLinea *err_msg;
    char *path;
    unsigned long long *size_pointer;
    unsigned long long from;
    unsigned long long count;
    unsigned long long *got;
    unsigned long long *whole;
    unsigned long long at;
    unsigned long long to;
    unsigned long long base;
    unsigned long long start;
    unsigned long long end;
    unsigned long long line;
    unsigned long long lines;
} FilesReadCtx;
// whether a letter the end of `size` bytes cuts short starts at them: a lead byte and as many of the bytes after it as
// there are, each as UTF-8 takes it, too few
EMBED_INLINE int cli_cut_short(const FilesReadCtx *args)
{
    const unsigned char lead = args->rest[0];
    const unsigned long long need = ((lead >= 0xc2u) && (lead <= 0xdfu))   ? 2ull
                                    : ((lead >= 0xe0u) && (lead <= 0xefu)) ? 3ull
                                    : ((lead >= 0xf0u) && (lead <= 0xf4u)) ? 4ull
                                                                           : 0ull;
    if ((need == 0ull) || (args->size >= need))
    {
        return 0;
    }
    for (unsigned long long at = 1ull; at < args->size; at += 1ull)
    {
        if ((args->rest[at] & 0xc0u) != 0x80u)
        {
            return 0;
        }
    }
    const unsigned char second = (args->size > 1ull) ? args->rest[1] : 0x80u;
    return !((lead == 0xe0u) && (second < 0xa0u)) && !((lead == 0xedu) && (second >= 0xa0u)) &&
           !((lead == 0xf0u) && (second < 0x90u)) && !((lead == 0xf4u) && (second >= 0x90u));
}

// whether `size` bytes are UTF-8 whole, or where `cut` is 1, UTF-8 but for a letter their end cuts short
EMBED_INLINE int cli_utf8(const FilesReadCtx *args)
{
    unsigned long long at = 0ull;
    while (at < args->size)
    {
        unsigned int letter = 0u;
        const unsigned long long step =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg, .text_bytes = args->bytes,
                       .length = args->size, .at = at, .letter = &letter);
        if ((step == 1ull) && (args->bytes[at] >= 0x80u))
        {
            return args->cut &&
                   EMBED_CALL(cli_cut_short, FilesReadCtx, .rest = args->bytes + at, .size = args->size - at);
        }
        at += step;
    }
    return 1;
}

// whether the first bytes contain a 0
EMBED_INLINE int cli_probe_zero(const FilesReadCtx *args)
{
    const unsigned char *bytes = args->bytes;
    const unsigned long long look = (args->size < CLI_PROBE_BYTES) ? args->size : CLI_PROBE_BYTES;
    return (look != 0ull) && (memor.chr(&(MemoriaCfg){.src = bytes, .val = 0, .bytes = look}) != NULL);
}

// a file read written as the window reads it: its path and size, its text or its first bytes, and whether it opens as
// a window
EMBED_INLINE void cli_opened_write(const FilesReadCtx *args)
{
    VerbaLinea *out = args->out;
    const unsigned char *text = args->text;
    unsigned long long length = args->length;
    mmgr_line_put(out, "{\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->path_read,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path_read, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"size\":");
    mmgr_line_decimal(out, args->size, 0u);
    mmgr_line_put(out, ",\"text\":");
    if (text != NULL)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = (const char *)text, .length = length);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, ",\"bytes\":");
    if (args->bytes != NULL)
    {
        mmgr_line_ch(out, '[');
        for (unsigned long long at = 0ull; at < length; at += 1ull)
        {
            if (at != 0ull)
            {
                mmgr_line_ch(out, ',');
            }
            mmgr_line_decimal(out, args->bytes[at], 0u);
        }
        mmgr_line_ch(out, ']');
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_put(out, args->windowed ? ",\"windowed\":true}" : ",\"windowed\":false}");
}

// a file within an archive read out of it whole into a cell of its size, given back once it is written as a file read
EMBED_INLINE int cli_archived_read(const FilesReadCtx *args)
{
    const char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long size = 0ull;
    char *const joined = EMBED_CALL(cli_archive_err_msg.take, CliArchiveErrMsgCfg, .root = root, .path = args->file,
                                    .size_pointer = &size, .owner = &g_cli_editor_cells, .err_msg = err_msg);
    if (joined == NULL)
    {
        return 0;
    }
    const unsigned char *const bytes = (const unsigned char *)joined;
    // the path with the slashes at either end left off
    const char *from = args->file;
    from += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ull,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}), .set = "/");
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
    length =
        EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = from, .at = length, .set = "/");
    char path[CLI_PATH_MAX_LEN];
    const unsigned long long copied_len = (length < CLI_PATH_MAX_LEN) ? length : (CLI_PATH_MAX_LEN - 1ull);
    memor.cpy(&(MemoriaCfg){.dst = path, .src = from, .bytes = copied_len});
    path[copied_len] = '\0';
    if (!EMBED_CALL(cli_probe_zero, FilesReadCtx, .bytes = bytes, .size = size) &&
        EMBED_CALL(cli_utf8, FilesReadCtx, .bytes = bytes, .size = size, .cut = 0))
    {
        EMBED_CALL(cli_opened_write, FilesReadCtx, .out = args->out, .path_read = path, .size = size, .text = bytes,
                   .bytes = NULL, .length = size, .windowed = 0);
    }
    else
    {
        EMBED_CALL(cli_opened_write, FilesReadCtx, .out = args->out, .path_read = path, .size = size, .text = NULL,
                   .bytes = bytes, .length = (size < CLI_SHOWN_BYTES) ? size : CLI_SHOWN_BYTES, .windowed = 0);
    }
    g_cli_cells->persistent_buf_release(joined, &g_cli_editor_cells);
    return 1;
}

// that a file could not be read, reported
EMBED_INLINE void cli_unread(const FilesReadCtx *args)
{
    mmgr_line_put(args->err_msg, args->file);
    mmgr_line_put(args->err_msg, ": it could not be read");
}

// the file at `file` inside the tree mapped whole, its full path into `path`: its bytes, or NULL with why given
EMBED_INLINE const unsigned char *cli_map(const FilesReadCtx *args)
{
    const char *root = args->root;
    char *path = args->path;
    unsigned long long *size = args->size_pointer;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = args->file, .joined = path,
                    .err_msg = err_msg))
    {
        return NULL;
    }
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                  .size_pointer = size, .owner = &g_cli_editor_cells);
    if (bytes == NULL)
    {
        EMBED_CALL(cli_unread, FilesReadCtx, .file = args->file, .err_msg = err_msg);
    }
    return bytes;
}

// `count` bytes of the file `file` of `root` from `from`, stored in a cell, the file's whole size into `whole`: the
// bytes, `got` of them, or NULL with why given. A file larger than the window pages is never stored whole: its shape is
// read off its first bytes and a slice off the bytes about it, each read at its offset
EMBED_INLINE const unsigned char *cli_piece(const FilesReadCtx *args)
{
    const char *root = args->root;
    char *path = args->path;
    unsigned long long from = args->from;
    unsigned long long count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = args->file, .joined = path,
                    .err_msg = err_msg))
    {
        return NULL;
    }
    if (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = args->whole))
    {
        EMBED_CALL(cli_unread, FilesReadCtx, .file = args->file, .err_msg = err_msg);
        return NULL;
    }
    const unsigned long long room = (from < *args->whole) ? *args->whole - from : 0ull;
    const unsigned char *const bytes =
        EMBED_CALL(cli_path_internal.file_piece_take, CliPathCtx, .path = path, .from = from,
                   .count_u64 = (count < room) ? count : room, .size_pointer = args->got, .owner = &g_cli_editor_cells);
    if (bytes == NULL)
    {
        EMBED_CALL(cli_unread, FilesReadCtx, .file = args->file, .err_msg = err_msg);
    }
    return bytes;
}

int cli_files_read(const CliFilesRootOutErrMsgFileCfg *args)
{
    const char *root = args->root;
    const char *file = args->file;
    char archive[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_archive_root.split, CliArchiveRootCfg, .root = root, .path = file, .file = archive,
                   .inner = inner) &&
        (inner[0] != '\0'))
    {
        return EMBED_CALL(cli_archived_read, FilesReadCtx, .root = root, .file = file, .out = args->out,
                          .err_msg = args->err_msg);
    }
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = file, .joined = path,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size))
    {
        EMBED_CALL(cli_unread, FilesReadCtx, .file = file, .err_msg = args->err_msg);
        return 0;
    }
    char shown_path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_path_root_internal.relative, CliPathRootCtx, .root = root, .path = path, .relative_out = shown_path);
    if (size <= CLI_WINDOW_FROM)
    {
        // a file the window takes whole is stored whole
        const unsigned char *const text = EMBED_CALL(cli_map, FilesReadCtx, .root = root, .file = file, .path = path,
                                                     .size_pointer = &size, .err_msg = args->err_msg);
        if (text == NULL)
        {
            return 0;
        }
        const unsigned long long shown = (size < CLI_SHOWN_BYTES) ? size : CLI_SHOWN_BYTES;
        if (!EMBED_CALL(cli_probe_zero, FilesReadCtx, .bytes = text, .size = size) &&
            EMBED_CALL(cli_utf8, FilesReadCtx, .bytes = text, .size = size, .cut = 0))
        {
            EMBED_CALL(cli_opened_write, FilesReadCtx, .out = args->out, .path_read = shown_path, .size = size,
                       .text = text, .bytes = NULL, .length = size, .windowed = 0);
        }
        else
        {
            EMBED_CALL(cli_opened_write, FilesReadCtx, .out = args->out, .path_read = shown_path, .size = size,
                       .text = NULL, .bytes = text, .length = shown, .windowed = 0);
        }
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = text, .owner = &g_cli_editor_cells);
        return 1;
    }
    // a file the window pages: its first bytes alone, read at their offset; text where they contain no 0 and read as
    // UTF-8, a letter the cut splits aside
    unsigned long long got = 0ull;
    const unsigned char *const bytes =
        EMBED_CALL(cli_piece, FilesReadCtx, .root = root, .file = file, .path = path, .from = 0ull,
                   .count = CLI_SHOWN_BYTES, .got = &got, .whole = &size, .err_msg = args->err_msg);
    if (bytes == NULL)
    {
        return 0;
    }
    const unsigned long long probe = (got < CLI_PROBE_BYTES) ? got : CLI_PROBE_BYTES;
    const int text = !EMBED_CALL(cli_probe_zero, FilesReadCtx, .bytes = bytes, .size = got) &&
                     EMBED_CALL(cli_utf8, FilesReadCtx, .bytes = bytes, .size = probe, .cut = 1);
    EMBED_CALL(cli_opened_write, FilesReadCtx, .out = args->out, .path_read = shown_path, .size = size, .text = NULL,
               .bytes = text ? NULL : bytes, .length = text ? 0ull : got, .windowed = text);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_editor_cells);
    return 1;
}

// the first line start at or after `at`: `at` itself where a line ends just before it, or else just past the next
// newline, or the end of the file
EMBED_INLINE unsigned long long cli_line_start(const FilesReadCtx *args)
{
    unsigned long long at = args->at;
    unsigned long long size = args->size;
    if ((at == 0ull) || (at >= size))
    {
        return (at < size) ? at : size;
    }
    return EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = (const char *)args->bytes, .at = at - 1ull,
                      .size = size)
        .next;
}

// the newlines from `from` to `to`
EMBED_INLINE unsigned long long cli_newlines(const FilesReadCtx *args)
{
    unsigned long long count = 0ull;
    for (unsigned long long at = args->from; at < args->to; at += 1ull)
    {
        count += (args->bytes[at] == '\n') ? 1ull : 0ull;
    }
    return count;
}

// whole lines written as the window reads them: the bytes from `start` to `end` of `bytes`, which contain the file from
// its byte `base`, the file's size, the number of the first line, and the lines in the whole file
EMBED_INLINE void cli_slice_write(const FilesReadCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_put(out, "{\"start\":");
    mmgr_line_decimal(out, args->base + args->start, 0u);
    mmgr_line_put(out, ",\"end\":");
    mmgr_line_decimal(out, args->base + args->end, 0u);
    mmgr_line_put(out, ",\"size\":");
    mmgr_line_decimal(out, args->size, 0u);
    mmgr_line_put(out, ",\"line\":");
    mmgr_line_decimal(out, args->line, 0u);
    mmgr_line_put(out, ",\"lines\":");
    mmgr_line_decimal(out, args->lines, 0u);
    mmgr_line_put(out, ",\"text\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = args->bytes + args->start, .length = args->end - args->start);
    mmgr_line_ch(out, '}');
}

int cli_files_slice(const CliFilesRootOutErrMsgFileCfg *args)
{
    unsigned long long start = args->start;
    unsigned long long end = args->end;
    // the bytes about the slice, from the byte before `start` so that its line's start is read, with CLI_SLICE_SLACK
    // past `end` for the line end there; a slice past the file's end ends at the end, and a line longer than the slack
    // is cut where the piece ends
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ull;
    if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->file, .joined = path,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size))
    {
        EMBED_CALL(cli_unread, FilesReadCtx, .file = args->file, .err_msg = args->err_msg);
        return 0;
    }
    start = (start < size) ? start : size;
    end = (end < size) ? end : size;
    const unsigned long long base = (start > 0ull) ? start - 1ull : 0ull;
    const unsigned long long at_end = (end > base) ? end - base : 0ull;
    const unsigned long long left = size - base;
    const unsigned long long count = ((at_end + CLI_SLICE_SLACK) < left) ? (at_end + CLI_SLICE_SLACK) : left;
    unsigned long long got = 0ull;
    const unsigned char *const bytes =
        EMBED_CALL(cli_path_internal.file_piece_take, CliPathCtx, .path = path, .from = base, .count_u64 = count,
                   .size_pointer = &got, .owner = &g_cli_editor_cells);
    if (bytes == NULL)
    {
        EMBED_CALL(cli_unread, FilesReadCtx, .file = args->file, .err_msg = args->err_msg);
        return 0;
    }
    const unsigned long long line_from =
        EMBED_CALL(cli_line_start, FilesReadCtx, .bytes = bytes, .at = start - base, .size = got);
    const unsigned long long from = ((line_from < got) || ((base + got) >= size)) ? line_from : (start - base);
    const unsigned long long to_at =
        EMBED_CALL(cli_line_start, FilesReadCtx, .bytes = bytes, .at = (at_end < got) ? at_end : got, .size = got);
    const unsigned long long to = (to_at > from) ? to_at : from;
    EMBED_CALL(cli_slice_write, FilesReadCtx, .out = args->out, .bytes = bytes, .base = base, .start = from, .end = to,
               .size = size, .line = 0ull, .lines = 0ull);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_editor_cells);
    return 1;
}
