// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_cell.c: a file's bytes read straight into one cell a caller's owner holds, whole or a piece, so every read of
// them, the word a walk loads over a text's last bytes included, stays in the pool the cellblock bounds
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"
#include "../text/text.h"

unsigned char *cli_file_piece_taken(const char *path, unsigned long long from, unsigned long long count,
                                    unsigned long long *size, CellOwner *owner)
{
    *size = 0ull;
    unsigned char *const cell = g_cli_cells->temporary_buf_alloc(count + CLI_FILE_SLACK, owner);
    if (cell == NULL)
    {
        return NULL;
    }
    // no bytes asked of a file are none, whether it opens or not
    if (!cli_file_read_at(path, from, cell, count, size) && (count != 0ull))
    {
        g_cli_cells->persistent_buf_release(cell, owner);
        *size = 0ull;
        return NULL;
    }
    // a take is not zeroed, and the slack after the bytes read is
    memor.set(&(MemoriaCfg){.dst = cell + *size, .val = 0, .bytes = CLI_FILE_SLACK});
    return cell;
}

unsigned char *cli_file_taken(const char *path, unsigned long long *size, CellOwner *owner)
{
    unsigned long long whole = 0ull;
    *size = 0ull;
    return cli_file_size(path, &whole) ? cli_file_piece_taken(path, 0ull, whole, size, owner) : NULL;
}

unsigned char *cli_file_taken_utf8(const char *path, unsigned long long *size, CellOwner *owner)
{
    unsigned char *const bytes = cli_file_taken(path, size, owner);
    if ((bytes != NULL) && !cli_text_utf8_whole(bytes, *size))
    {
        g_cli_cells->persistent_buf_release(bytes, owner);
        *size = 0ull;
        return NULL;
    }
    return bytes;
}

void cli_file_given(const unsigned char *bytes, CellOwner *owner)
{
    // the cell is given back as it was taken; const is the reader's view of it
    g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, owner);
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

static void tree_bytes_each(void *context, const char *name, int is_folder)
{
    CliTreeBytes *const walk = context;
    for (unsigned int at = 0u; is_folder && (at < 4u); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_tree_skipped[at], .cap = CLI_NAME_MAX_LEN}))
        {
            return;
        }
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = walk->path, .cap = CLI_PATH_MAX_LEN});
    VerbaLinea line = {walk->path, sizeof(walk->path), length};
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    if (mmgr_line_finish(&line) == 0ull)
    {
        walk->path[length] = '\0';
        return;
    }
    if (is_folder)
    {
        query_folder_each(walk->path, tree_bytes_each, walk);
    }
    else
    {
        unsigned long long size = 0ull;
        walk->bytes += cli_file_size(walk->path, &size) ? size : 0ull;
    }
    walk->path[length] = '\0';
}

unsigned long long cli_tree_bytes(const char *root)
{
    static CliTreeBytes s_walk;
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    VerbaLinea line = {s_walk.path, sizeof(s_walk.path), 0ull};
    mmgr_line_put(&line, root);
    s_walk.bytes = 0ull;
    if (mmgr_line_finish(&line) != 0ull)
    {
        query_folder_each(s_walk.path, tree_bytes_each, &s_walk);
    }
    const unsigned long long bytes = s_walk.bytes;
    cli_lock_give(&s_lock);
    return bytes;
}
