// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// defs.c: the oracle tables, the k-file head and the language tables read from the tree and written as the window
// reads them
#include "defs.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// A file read: its text, mapped, and its size; the text NULL where it could not be read
typedef struct
{
    const char *text;
    unsigned long long size;
} CliDefsFile;
// The names a folder lists, its folders where `folders` is 1, else its files ending `.tsv`, in a cell of `capacity`
typedef struct
{
    char (*name)[CLI_NAME_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
    int folders;
} CliDefsNames;
// the most folders or tables a listing contains
#define CLI_DEFS_NAMES 512u

typedef struct
{
    const char *path;
    CliDefsFile file;
    const char *at;
    const char *end;
    const char **stop;
    char letter;
    const char *from;
    const char *to;
    VerbaLinea *out;
    const char *root;
    const char *name;
    CliDefsLine *line;
    unsigned long long *length;
    unsigned long long *number;
    const char *folder;
    int folders;
    CliDefsNames *names;
} DefsCtx;
// the file at `path` mapped whole, let go with cli_unmapped
EMBED_INLINE CliDefsFile cli_map(const DefsCtx *args)
{
    const char *path = args->path;
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                  .size_pointer = &size, .owner = &g_cli_defs_cells);
    // a file's bytes are read as its text
    return (CliDefsFile){(const char *)bytes, size};
}

EMBED_INLINE void cli_unmapped(const DefsCtx *args)
{
    if (args->file.text != NULL)
    {
        // the text is the bytes the file was mapped as
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx,
                   .bytes_bytes_other = (const unsigned char *)args->file.text, .owner = &g_cli_defs_cells);
    }
}

// the line from `at` before `end`: where it ends, its `\r` before `\n` left off, into `stop`; where the next starts
EMBED_INLINE const char *cli_line(const DefsCtx *args)
{
    const char *at = args->at;
    const char **stop = args->stop;
    const char *next = at;
    next += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = next, .at = 0ULL,
                       .size = (unsigned long long)(args->end - next), .set = "\n");
    *stop = ((next > at) && (next[-1] == '\r')) ? (next - 1) : next;
    return (next < args->end) ? (next + 1) : next;
}

EMBED_INLINE int cli_is_space(const DefsCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

// whether a line contains no table: a comment, or blank
EMBED_INLINE int cli_skip(const DefsCtx *args)
{
    const char *from = args->from;
    if ((from < args->to) && (from[0] == '#'))
    {
        return 1;
    }
    while ((from < args->to) && EMBED_CALL(cli_is_space, DefsCtx, .letter = from[0]))
    {
        from += 1;
    }
    return from == args->to;
}

// a line's cells between tabs, from `from` before `to`, into `line`
EMBED_INLINE void cli_cells_get(const DefsCtx *args)
{
    args->line->count = 0u;
    const char *cell = args->from;
    for (const char *at = args->from;; at += 1)
    {
        if ((at == args->to) || (at[0] == '\t'))
        {
            if (args->line->count < CLI_DEFS_CELLS)
            {
                args->line->cell[args->line->count] = cell;
                // a cell lies within its line
                args->line->length[args->line->count] = (unsigned long long)(at - cell);
                args->line->count += 1u;
            }
            cell = at + 1;
            if (at == args->to)
            {
                break;
            }
        }
    }
}

int cli_defs_lines(const CliDefsPathCfg *args)
{
    const CliDefsFile file = EMBED_CALL(cli_map, DefsCtx, .path = args->path);
    if (file.text == NULL)
    {
        return 0;
    }
    const char *const end = file.text + file.size;
    int named = 0;
    for (const char *at = file.text; at < end;)
    {
        const char *stop = NULL;
        const char *const next = EMBED_CALL(cli_line, DefsCtx, .at = at, .end = end, .stop = &stop);
        if (!EMBED_CALL(cli_skip, DefsCtx, .from = at, .to = stop))
        {
            CliDefsLine line;
            EMBED_CALL(cli_cells_get, DefsCtx, .from = at, .to = stop, .line = &line);
            EMBED_CALL(args->each, CliDefsLineEachCfg, .context = args->context, .line = &line, .named = !named);
            named = 1;
        }
        at = next;
    }
    EMBED_CALL(cli_unmapped, DefsCtx, .file = file);
    return named;
}
