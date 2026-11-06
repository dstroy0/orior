// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_cell.c: a file's bytes read straight into one cell a caller's owner holds, whole or a piece, so every read of
// them, the word a walk loads over a text's last bytes included, stays in the pool the cellblock bounds
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "../text/text.h"

unsigned char *cli_file_piece_alloc(const CliPathCtx *args)
{
    unsigned long long *size = args->size_pointer;
    *size = 0ULL;
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(args->count_u64 + CLI_FILE_SLACK, args->owner);
    if (cell == NULL)
    {
        return NULL;
    }
    // no bytes asked of a file are none, whether it opens or not
    if (!EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = args->path, .from = args->from,
                    .bytes_bytes = cell, .count_u64 = args->count_u64, .got = size) &&
        (args->count_u64 != 0ULL))
    {
        g_cli_guard->persistent_buf_release(cell, args->owner);
        *size = 0ULL;
        return NULL;
    }
    // a take is not zeroed, and the slack after the bytes read is
    memor.set(&(MemoriaCfg){.dst = cell + *size, .val = 0, .bytes = CLI_FILE_SLACK});
    return cell;
}

unsigned char *cli_file_alloc(const CliPathCtx *args)
{
    unsigned long long *size = args->size_pointer;
    unsigned long long whole = 0ULL;
    *size = 0ULL;
    return EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = args->path, .size_pointer = &whole)
               ? EMBED_CALL(cli_path_internal.file_piece_alloc, CliPathCtx, .path = args->path, .from = 0ULL,
                            .count_u64 = whole, .size_pointer = size, .owner = args->owner)
               : NULL;
}

unsigned char *cli_file_alloc_utf8(const CliPathCtx *args)
{
    unsigned long long *size = args->size_pointer;
    unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                            .size_pointer = size, .owner = args->owner);
    if ((bytes != NULL) &&
        !EMBED_CALL(cli_text_size_kind.utf8_valid, CliTextSizeKindCfg, .text_bytes = bytes, .size = *size))
    {
        g_cli_guard->persistent_buf_release(bytes, args->owner);
        *size = 0ULL;
        return NULL;
    }
    return bytes;
}

// A walk summing the bytes of every file under a folder: the path walked, sized for CLI_PATH_MAX_LEN, and the sum
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    unsigned long long bytes;
} CliTreeBytes;

// the folders a walk does not go into: version control's own, and those the tree's git ignores as a rule, cargo's
// target, the build folder and node_modules; the walk runs before the pool stands, and it cannot query git itself
static const char *const s_tree_skipped[4] = {".git", "target", "build", "node_modules"};

static void tree_bytes_each(const CliEachNameIsFolderCfg *args)
{
    void *context = args->context;
    CliTreeBytes *const walk = context;
    for (unsigned int at = 0u; args->is_folder && (at < 4u); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = s_tree_skipped[at], .cap = CLI_NAME_MAX_LEN}))
        {
            return;
        }
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = walk->path, .cap = CLI_PATH_MAX_LEN});
    VerbaLinea line = {walk->path, sizeof(walk->path), length};
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        walk->path[length] = '\0';
        return;
    }
    if (args->is_folder)
    {
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = walk->path,
                   .each = tree_bytes_each, .context_bytess = walk);
    }
    else
    {
        unsigned long long size = 0ULL;
        walk->bytes +=
            EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = walk->path, .size_pointer = &size)
                ? size
                : 0ULL;
    }
    walk->path[length] = '\0';
}

unsigned long long cli_tree_bytes(const CliPathRootCtx *args)
{
    static CliTreeBytes s_walk;
    VerbaLinea line = {s_walk.path, sizeof(s_walk.path), 0ULL};
    mmgr_line_put(&line, args->root);
    s_walk.bytes = 0ULL;
    if (mmgr_line_finish(&line) != 0ULL)
    {
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = s_walk.path,
                   .each = tree_bytes_each, .context_bytess = &s_walk);
    }
    const unsigned long long bytes = s_walk.bytes;
    return bytes;
}
