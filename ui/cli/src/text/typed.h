// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// typed.h: the bytes typed into a box of the window, the Search pane's, the Git pane's, the prompt's and the database
// form's, in one movable cell taken once at the box's most as the first is typed, read by a copy into the caller's
// buffer, and given back as the box closes (typed.c)
#ifndef TYPED_H
#define TYPED_H
#include "../command_line/pool.h"
#include "embed_dispatch_layout.h"

typedef struct CliTypedCfg CliTypedCfg;

// The bytes typed: the cell they stand in with their end, NULL while none was typed, and how many
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned long long length;
} CliTyped;

// `size` bytes of `text` put after the bytes typed, up to `most` bytes with their end, a control character and what
// follows it left out, the cell taken once at `most` as the first byte is put; the last letter erased, its lead byte
// and the continuation bytes after it; the cell given back, the bytes none; and the bytes of `typed_read`, or of
// `typed` where that is NULL, copied with their end into `dst`, `capacity` at the most: how many were copied. The cells
// are `owner`'s
void cli_typed_put(const CliTypedCfg *args);
void cli_typed_erase(const CliTypedCfg *args);
void cli_typed_release(const CliTypedCfg *args);
unsigned long long cli_typed_copy(const CliTypedCfg *args);

struct CliTypedCfg
{
    CliTyped *typed;
    const CliTyped *typed_read;
    const char *text;
    unsigned long long size;
    unsigned long long most;
    const CellOwner *owner;
    char *dst;
    unsigned long long capacity;
};

typedef struct
{
    void (*put)(const CliTypedCfg *args);
    void (*erase)(const CliTypedCfg *args);
    void (*release)(const CliTypedCfg *args);
    unsigned long long (*copy)(const CliTypedCfg *args);
} CliTypedNs;
EMBED_TABLE_LAYOUT(CliTypedNs, put, erase, release, copy);
EMBED_TABLE_STORAGE CliTypedNs cli_typed EMBED_UNUSED = {
    .put = cli_typed_put,
    .erase = cli_typed_erase,
    .release = cli_typed_release,
    .copy = cli_typed_copy,
};

#endif
