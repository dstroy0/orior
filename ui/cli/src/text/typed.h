// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// typed.h: the bytes typed into a box of the window, the Search pane's and the Git pane's, in one cell sized to them
// with their end, taken as the first is typed and released as the last is erased (typed.c)
#ifndef TYPED_H
#define TYPED_H
#include "../command_line/cells.h"
#include "embed_dispatch_layout.h"

typedef struct CliTypedCfg CliTypedCfg;

// The bytes typed: the cell they stand in with their end, NULL where none are, and how many
typedef struct
{
    char *bytes;
    unsigned long long length;
} CliTyped;

// `size` bytes of `text` put after the bytes typed, up to `most` bytes with their end, a control character and what
// follows it left out; and the last letter erased, its lead byte and the continuation bytes after it, the cell released
// as the last byte goes. The cells are `owner`'s
void cli_typed_put(const CliTypedCfg *args);
void cli_typed_erase(const CliTypedCfg *args);

struct CliTypedCfg
{
    CliTyped *typed;
    const char *text;
    unsigned long long size;
    unsigned long long most;
    CellOwner *owner;
};

typedef struct
{
    void (*put)(const CliTypedCfg *args);
    void (*erase)(const CliTypedCfg *args);
} CliTypedNs;
EMBED_TABLE_LAYOUT(CliTypedNs, put, erase);
EMBED_TABLE_STORAGE CliTypedNs cli_typed EMBED_UNUSED = {
    .put = cli_typed_put,
    .erase = cli_typed_erase,
};

#endif
