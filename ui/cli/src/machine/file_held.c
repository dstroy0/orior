// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// file_held.c: a file's bytes in a cell of the cellblock, whole or a piece, so every read of them, the word a walk
// loads over a text's last bytes included, stays in the pool the cellblock bounds
#include "../command_line/cells.h"
#include "../command_line/cli_internal.h"
#include "../text/text.h"

// the bytes past a file's held in its cell, readable and zero: a vector of the widest walk, and the terminator
#define CLI_FILE_SLACK 64ull

static const unsigned char s_none[CLI_FILE_SLACK];

// `count` bytes of the file at `path` from `from` held in a cell: the cell, or NULL
static const unsigned char *file_taken(const char *path, unsigned long long from, unsigned long long count,
                                       unsigned long long *size)
{
    *size = 0ull;
    if (count == 0ull)
    {
        return s_none;
    }
    unsigned char *const cell = cli_cell_taken(count + CLI_FILE_SLACK, "a file held");
    if (cell == NULL)
    {
        return NULL;
    }
    if (!cli_file_read_at(path, from, cell, count, size))
    {
        cli_cell_given(cell);
        *size = 0ull;
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = cell + *size, .val = 0, .bytes = CLI_FILE_SLACK});
    return cell;
}

// the cell a whole file is read into, its slack after it: a cell of the file's size and the slack, kept in `context`
// for its reader; a file of no bytes is given the shared empty slack
static unsigned char *file_roomed(void *context, unsigned long long size)
{
    unsigned char **const cell = context;
    *cell = (size == 0ull) ? NULL : cli_cell_taken(size + CLI_FILE_SLACK, "a file held");
    return *cell;
}

const unsigned char *cli_file_held(const char *path, unsigned long long *size)
{
    unsigned char *cell = NULL;
    const int read = cli_file_read_whole(path, file_roomed, &cell, size);
    if (!read && (cell == NULL))
    {
        // an unopened file, or one of no bytes: the first is none, the second the shared empty slack
        unsigned long long whole = 0ull;
        *size = 0ull;
        return (cli_file_size(path, &whole) && (whole == 0ull)) ? s_none : NULL;
    }
    if (!read)
    {
        cli_cell_given(cell);
        *size = 0ull;
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = cell + *size, .val = 0, .bytes = CLI_FILE_SLACK});
    return cell;
}

const unsigned char *cli_file_held_utf8(const char *path, unsigned long long *size)
{
    const unsigned char *const bytes = cli_file_held(path, size);
    if ((bytes != NULL) && !cli_text_utf8_whole(bytes, *size))
    {
        cli_file_let_go(bytes, *size);
        return NULL;
    }
    return bytes;
}

const unsigned char *cli_file_piece_held(const char *path, unsigned long long from, unsigned long long count,
                                         unsigned long long *size)
{
    return file_taken(path, from, count, size);
}

// A walk summing the bytes of every file under a folder: the path walked, which holds CLI_PATH_LONGEST, and the sum
typedef struct
{
    char path[CLI_PATH_LONGEST];
    unsigned long long bytes;
} CliTreeBytes;

// the folders a walk does not go into: version control's own, and those the tree's git ignores as a rule, cargo's
// target, the build folder and node_modules; the walk runs before the pool stands, so it cannot ask git itself
static const char *const s_tree_skipped[4] = {".git", "target", "build", "node_modules"};

static void tree_bytes_each(void *context, const char *name, int is_folder)
{
    CliTreeBytes *const walk = context;
    for (unsigned int at = 0u; is_folder && (at < 4u); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_tree_skipped[at], .cap = CLI_NAME_LONGEST}))
        {
            return;
        }
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = walk->path, .cap = CLI_PATH_LONGEST});
    CliLine line = {walk->path, sizeof(walk->path), length};
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    if (cli_line_closed(&line) == 0ull)
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
    CliLine line = {s_walk.path, sizeof(s_walk.path), 0ull};
    cli_line_text(&line, root);
    s_walk.bytes = 0ull;
    if (cli_line_closed(&line) != 0ull)
    {
        query_folder_each(s_walk.path, tree_bytes_each, &s_walk);
    }
    const unsigned long long bytes = s_walk.bytes;
    cli_lock_give(&s_lock);
    return bytes;
}

void cli_file_let_go(const unsigned char *bytes, unsigned long long size)
{
    if ((bytes != NULL) && (bytes != s_none) && (size != 0ull))
    {
        // the cell is given back as it was taken; const is the reader's view of it
        cli_cell_given((void *)(uintptr_t)bytes);
    }
}
