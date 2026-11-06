// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// typed.c: the bytes typed into a box, each change a cell sized to the bytes as they then stand
#include "typed.h"

#include "../command_line/cli_internal.h"

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
    if ((size == 0ULL) || (length + size + 1ULL > args->most))
    {
        return;
    }
    // the bytes typed so far and the new ones, in a cell sized to them with their end, the cell they stood in released
    char *const bytes = g_cli_guard->persistent_buf_alloc(length + size + 1ULL, args->owner);
    if (bytes == NULL)
    {
        return;
    }
    if (typed->bytes != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = bytes, .src = typed->bytes, .bytes = length});
    }
    memor.cpy(&(MemoriaCfg){.dst = bytes + length, .src = args->text, .bytes = size});
    bytes[length + size] = '\0';
    g_cli_guard->persistent_buf_release(typed->bytes, args->owner);
    typed->bytes = bytes;
    typed->length = length + size;
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
    // the bytes left, in a cell sized to them with their end, the cell they stood in released; none where none is left
    char *const bytes = (length != 0ULL) ? g_cli_guard->persistent_buf_alloc(length + 1ULL, args->owner) : NULL;
    if ((length != 0ULL) && (bytes == NULL))
    {
        return;
    }
    if (bytes != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = bytes, .src = typed->bytes, .bytes = length});
        bytes[length] = '\0';
    }
    g_cli_guard->persistent_buf_release(typed->bytes, args->owner);
    typed->bytes = bytes;
    typed->length = length;
}
