// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// typed.c: the bytes typed into a box, each change a cell sized to the bytes as they then stand
#include "typed.h"

#include "../command_line/cli_internal.h"

typedef struct
{
    CliTyped *typed;
    unsigned long long length;
    CellOwner *owner;
} TypedCtx;

// the first `length` bytes typed moved into a cell sized to them with their end, the cell they stood in released:
// whether the cell was taken
EMBED_INLINE int typed_sized(const TypedCtx *args)
{
    CliTyped *const typed = args->typed;
    char *const bytes = g_cli_cells->persistent_buf_alloc(args->length + 1ULL, args->owner);
    if (bytes == NULL)
    {
        return 0;
    }
    if (typed->bytes != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = bytes, .src = typed->bytes, .bytes = typed->length});
    }
    bytes[args->length] = '\0';
    g_cli_cells->persistent_buf_release(typed->bytes, args->owner);
    typed->bytes = bytes;
    typed->length = args->length;
    return 1;
}

void cli_typed_put(const CliTypedCfg *args)
{
    CliTyped *const typed = args->typed;
    unsigned long long size = args->size;
    for (unsigned long long at = 0ULL; at < size; at += 1ULL)
    {
        // a control character typed, a tab or a return, is no part of the text
        size = ((unsigned char)args->text[at] < 0x20u) ? at : size;
    }
    const unsigned long long length = typed->length;
    if ((size == 0ULL) || (length + size + 1ULL > args->most) ||
        !EMBED_CALL(typed_sized, TypedCtx, .typed = typed, .length = length + size, .owner = args->owner))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = typed->bytes + length, .src = args->text, .bytes = size});
}

void cli_typed_erase(const CliTypedCfg *args)
{
    CliTyped *const typed = args->typed;
    unsigned long long length = typed->length;
    while (length > 0ULL)
    {
        length -= 1ULL;
        if (((unsigned char)typed->bytes[length] & 0xC0u) != 0x80u)
        {
            break;
        }
    }
    if (length == 0ULL)
    {
        g_cli_cells->persistent_buf_release(typed->bytes, args->owner);
        typed->bytes = NULL;
        typed->length = 0ULL;
        return;
    }
    (void)EMBED_CALL(typed_sized, TypedCtx, .typed = typed, .length = length, .owner = args->owner);
}
