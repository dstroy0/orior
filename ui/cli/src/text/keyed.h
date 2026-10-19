// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keyed.h: texts kept by a key, each with a second text beside it, as the servers keep each file's diagnostics, its
// findings and its text by the file's key and its URI. Each entry is kept whole in one cell of the keepers' shared
// cellblock, its key, its second text and its text one after another, an entry put again giving its old cell back;
// a put the cellblock or the entries cannot hold is refused and said, with the keeper's name. Nothing of a keeper's
// own is locked here: each keeper holds its own lock (keyed.c)
#ifndef KEYED_H
#define KEYED_H

#include "../command_line/cli_internal.h"

// An entry: whether it holds one, the hash of its key, its cell and the cell's size, its key's and its second text's
// sizes
typedef struct
{
    int used;
    unsigned int hash;
    char *cell;
    unsigned long long size;
    unsigned int key_size;
    unsigned int beside_size;
} CliKeyedEntry;

// the entries a page of a keeper's holds, and the most pages a keeper takes
#define CLI_KEYED_PAGE 256u
#define CLI_KEYED_PAGES 256u

// Texts kept by a key: the keeper's name, said where a put is refused; the entries in pages of CLI_KEYED_PAGE, each
// page a cell taken as its first place is and given back as the keeper empties, how many places were ever taken, and
// the most there are, at most CLI_KEYED_PAGE pages of them
typedef struct
{
    const char *name;
    CliKeyedEntry *page[CLI_KEYED_PAGES];
    unsigned int entries;
    unsigned int entries_most;
} CliKeyed;

// the entry at the place `at`, below the places ever taken
CliKeyedEntry *cli_keyed_entry(const CliKeyed *keyed, unsigned int at);

// the texts made empty, at most `entries_most` of them; `size` bytes of `text` kept under `key` with `beside`, in place
// of what it held: 1, or 0, said, where they do not fit; the text kept under `key`, its size into `size` and the text
// beside it into `beside`: NULL where none is kept, each good until the texts next change; the text under `key` taken
// out; every text taken out; and each key handed to `each` with its texts
void cli_keyed_begun(CliKeyed *keyed, const char *name, unsigned int entries_most);
int cli_keyed_put(CliKeyed *keyed, const char *key, const char *beside, const char *text, unsigned long long size);
const char *cli_keyed_get(const CliKeyed *keyed, const char *key, const char **beside, unsigned long long *size);
void cli_keyed_remove(CliKeyed *keyed, const char *key);
void cli_keyed_clear(CliKeyed *keyed);
void cli_keyed_each(const CliKeyed *keyed,
                    void (*each)(void *context, const char *key, const char *beside, const char *text,
                                 unsigned long long size),
                    void *context);

#endif
