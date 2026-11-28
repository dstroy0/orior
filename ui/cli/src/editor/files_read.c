// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_read.c: a file read for the editor: its text where it is text, its first bytes where it is not, and a larger
// text read a window of whole lines at a time
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "archive.h"
#include "document.h"
#include "files.h"
#include "pages.h"

// the size past which a text file opens as a window, the max bytes of a file that is not text displayed, and the bytes
// looked through for a 0 that marks a file as no text
#define CLI_WINDOW_FROM (1024ULL * 1024ULL)
#define CLI_DISPLAYED_BYTES (64ULL * 1024ULL)
#define CLI_SLICE_SLACK (1ULL << 20u)
#define CLI_PROBE_BYTES 8192ULL

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
// whether a character the end of `size` bytes cuts short starts at them: a lead byte and as many of the bytes after it
// as there are, each as UTF-8 takes it, too few
EMBED_INLINE int cli_cut_short(const FilesReadCtx *args)
{
    const unsigned char lead = args->rest[0];
    unsigned long long need;
    if ((lead >= 0xc2u) && (lead <= 0xdfu))
    {
        need = 2ULL;
    }
    else if ((lead >= 0xe0u) && (lead <= 0xefu))
    {
        need = 3ULL;
    }
    else if ((lead >= 0xf0u) && (lead <= 0xf4u))
    {
        need = 4ULL;
    }
    else
    {
        need = 0ULL;
    }
    if ((need == 0ULL) || (args->size >= need))
    {
        return 0;
    }
    for (unsigned long long at = 1ULL; at < args->size; at += 1ULL)
    {
        if ((args->rest[at] & 0xc0u) != 0x80u)
        {
            return 0;
        }
    }
    const unsigned char second = (args->size > 1ULL) ? args->rest[1] : 0x80u;
    return !((lead == 0xe0u) && (second < 0xa0u)) && !((lead == 0xedu) && (second >= 0xa0u)) &&
           !((lead == 0xf0u) && (second < 0x90u)) && !((lead == 0xf4u) && (second >= 0x90u));
}

// whether `size` bytes are UTF-8 whole, or where `cut` is 1, UTF-8 but for a character their end cuts short
EMBED_INLINE int cli_utf8(const FilesReadCtx *args)
{
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        unsigned int character = 0u;
        const unsigned long long step =
            EMBED_CALL(cli_json_tree_length.lossy_character, CliJsonTreeLengthCfg, .text_bytes = args->bytes,
                       .length = args->size, .at = at, .character = &character);
        if ((step == 1ULL) && (args->bytes[at] >= 0x80u))
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
    return (look != 0ULL) && (memor.chr(&(MemoriaCfg){.src = bytes, .val = 0, .bytes = look}) != NULL);
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
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = size, .owner = g_cli_editor_owner);
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
    const unsigned long long capacity = (from < *args->whole) ? *args->whole - from : 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_piece_alloc, CliPathCtx, .path = path,
                                                  .from = from, .count_u64 = (count < capacity) ? count : capacity,
                                                  .size_pointer = args->got, .owner = g_cli_editor_owner);
    if (bytes == NULL)
    {
        EMBED_CALL(cli_unread, FilesReadCtx, .file = args->file, .err_msg = err_msg);
    }
    return bytes;
}

// What a file's bytes leave once read: the key the text goes under, whether the bytes are text, and whether the text
// is stored
typedef struct
{
    const char *key;
    int text;
    int stored;
} FilesStored;

// the bytes passed checked for text, and the text stored under the context's key where they are
static void files_bytes_stored(const CliEachBytesSizeCfg *args)
{
    FilesStored *const kept = args->context;
    const unsigned char *const bytes = (const unsigned char *)args->bytes;
    kept->text = !EMBED_CALL(cli_probe_zero, FilesReadCtx, .bytes = bytes, .size = args->size) &&
                 EMBED_CALL(cli_utf8, FilesReadCtx, .bytes = bytes, .size = args->size, .cut = 0);
    CliShape shape;
    kept->stored = kept->text && EMBED_CALL(cli_document_text.store, CliDocumentTextCfg, .key = kept->key,
                                            .text = args->bytes, .size = args->size, .shape = &shape);
}

// The bytes in the archive's cell read into a buffer of this call, checked and stored as the bytes of a file on disk
// are. The buffer goes back before the call returns
static void files_archived_stored(const CliArchiveEachCfg *args)
{
    char *const bytes = g_platform_guard->temporary_buf_alloc(args->size + 1ULL, g_cli_editor_owner);
    if ((bytes == NULL) || !g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
                               .cell = args->cell, .offset = 0ULL, .dst = bytes, .bytes = args->size + 1ULL}))
    {
        g_platform_guard->persistent_buf_release(bytes, g_cli_editor_owner);
        return;
    }
    files_bytes_stored(&(CliEachBytesSizeCfg){.context = args->context, .bytes = bytes, .size = args->size});
    g_platform_guard->persistent_buf_release(bytes, g_cli_editor_owner);
}

unsigned int cli_files_open(const CliFilesOpenCfg *args)
{
    char archive[CLI_PATH_MAX_LEN];
    char inner[CLI_PATH_MAX_LEN];
    const int archived = EMBED_CALL(cli_archive_root.split, CliArchiveRootCfg, .root = args->root, .path = args->file,
                                    .file = archive, .inner = inner) &&
                         (inner[0] != '\0');
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ULL;
    if (!archived)
    {
        if (!EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = args->root, .relative = args->file,
                        .joined = path, .err_msg = args->err_msg))
        {
            return CLI_FILES_OPEN_REFUSED;
        }
        if (!EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = &size))
        {
            EMBED_CALL(cli_unread, FilesReadCtx, .file = args->file, .err_msg = args->err_msg);
            return CLI_FILES_OPEN_REFUSED;
        }
        if (size > CLI_WINDOW_FROM)
        {
            // a file the window pages: its first bytes alone tell whether it is text
            unsigned long long got = 0ULL;
            const unsigned char *const first =
                EMBED_CALL(cli_piece, FilesReadCtx, .root = args->root, .file = args->file, .path = path, .from = 0ULL,
                           .count = CLI_DISPLAYED_BYTES, .got = &got, .whole = &size, .err_msg = args->err_msg);
            if (first == NULL)
            {
                return CLI_FILES_OPEN_REFUSED;
            }
            const unsigned long long probe = (got < CLI_PROBE_BYTES) ? got : CLI_PROBE_BYTES;
            const int paged_text = !EMBED_CALL(cli_probe_zero, FilesReadCtx, .bytes = first, .size = got) &&
                                   EMBED_CALL(cli_utf8, FilesReadCtx, .bytes = first, .size = probe, .cut = 1);
            g_platform_guard->persistent_buf_release((void *)(embed_word)first, g_cli_editor_owner);
            if (!paged_text)
            {
                return CLI_FILES_OPEN_NOT_TEXT;
            }
            return EMBED_CALL(cli_pages_path.open, CliPagesPathCfg, .key = args->key, .path = path, .out = NULL,
                              .err_msg = args->err_msg)
                       ? CLI_FILES_OPEN_PAGED
                       : CLI_FILES_OPEN_REFUSED;
        }
    }
    FilesStored kept = {args->key, 0, 0};
    if (archived)
    {
        // the archive passes a movable cell of its own with the file's bytes, which it gives back as the call returns
        if (!EMBED_CALL(cli_archive_err_msg.each, CliArchiveErrMsgCfg, .root = args->root, .path = args->file,
                        .each = files_archived_stored, .context = &kept, .err_msg = args->err_msg))
        {
            return CLI_FILES_OPEN_REFUSED;
        }
    }
    else
    {
        const unsigned char *const bytes = EMBED_CALL(cli_map, FilesReadCtx, .root = args->root, .file = args->file,
                                                      .path = path, .size_pointer = &size, .err_msg = args->err_msg);
        if (bytes == NULL)
        {
            return CLI_FILES_OPEN_REFUSED;
        }
        files_bytes_stored(&(CliEachBytesSizeCfg){.context = &kept, .bytes = (const char *)bytes, .size = size});
        g_platform_guard->persistent_buf_release((void *)(embed_word)bytes, g_cli_editor_owner);
    }
    const int text = kept.text;
    const int stored = kept.stored;
    if (text && !stored)
    {
        mmgr_line_put(args->err_msg, args->file);
        mmgr_line_put(args->err_msg, ": the cells cannot meet its text");
    }
    if (!text)
    {
        return CLI_FILES_OPEN_NOT_TEXT;
    }
    if (stored)
    {
        return CLI_FILES_OPEN_WHOLE;
    }
    return CLI_FILES_OPEN_REFUSED;
}
