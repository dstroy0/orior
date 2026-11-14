// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// typed.c: the bytes typed into a box, in one movable cell taken once at the box's most, the bytes and their end
// written in place as they change
#include "typed.h"

#include "../command_line/cli_internal.h"

// the most bytes a letter takes, which an erase reads back over
#define TYPED_LETTER_MAX 4ULL

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
    if (typed->cell == NULL)
    {
        typed->cell = g_cli_guard->movable_buf_alloc(args->most, args->owner);
        if (typed->cell == NULL)
        {
            return;
        }
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = typed->cell, .offset = length, .src = args->text, .bytes = size});
    const char end = '\0';
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = typed->cell, .offset = length + size, .src = &end, .bytes = 1ULL});
    typed->length = length + size;
}

void cli_typed_erase(const CliTypedCfg *args)
{
    CliTyped *const typed = args->typed;
    unsigned long long length = typed->length;
    if ((typed->cell == NULL) || (length == 0ULL))
    {
        return;
    }
    // the last letter's bytes read back: a continuation byte each until its lead byte
    const unsigned long long from = (length > TYPED_LETTER_MAX) ? length - TYPED_LETTER_MAX : 0ULL;
    unsigned char tail[TYPED_LETTER_MAX];
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = typed->cell, .offset = from, .dst = tail, .bytes = length - from});
    while (length > from)
    {
        length -= 1ULL;
        if ((tail[length - from] & 0xC0u) != 0x80u)
        {
            break;
        }
    }
    const char end = '\0';
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = typed->cell, .offset = length, .src = &end, .bytes = 1ULL});
    typed->length = length;
}

void cli_typed_release(const CliTypedCfg *args)
{
    CliTyped *const typed = args->typed;
    (void)g_cli_guard->movable_buf_release(typed->cell, args->owner);
    typed->cell = NULL;
    typed->length = 0ULL;
}

unsigned long long cli_typed_copy(const CliTypedCfg *args)
{
    const CliTyped *const typed = (args->typed_read != NULL) ? args->typed_read : args->typed;
    if ((typed == NULL) || (args->dst == NULL) || (args->capacity == 0ULL))
    {
        return 0ULL;
    }
    const unsigned long long room = args->capacity - 1ULL;
    const unsigned long long copied = ((typed->cell != NULL) && (typed->length < room)) ? typed->length
                                      : (typed->cell != NULL)                           ? room
                                                                                        : 0ULL;
    if (copied != 0ULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = typed->cell, .dst = args->dst, .bytes = copied});
    }
    args->dst[copied] = '\0';
    return copied;
}
