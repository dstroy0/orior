// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keyed.h: texts stored by a key, each with a second text beside it, as the servers store each file's diagnostics, its
// findings and its text by the file's key and its URI. Each entry is stored whole in one cell of the keepers' shared
// cellblock, its key, its second text and its text one after another, an entry put again giving its old cell back;
// a put the cellblock or the entries cannot fit is refused and reported, with the keeper's name. Nothing of a keeper's
// own is locked here: each keeper owns its own lock (keyed.c)
#ifndef KEYED_H
#define KEYED_H

#include "../command_line/cli_internal.h"

// An entry: whether it contains one, the hash of its key, its cell and the cell's size, its key's and its second text's
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

// the entries a page of a keeper's contains, and the most pages a keeper takes
#define CLI_KEYED_PAGE 256u
#define CLI_KEYED_PAGES 256u

// Texts stored by a key: the keeper's name, reported where a put is refused; the entries in pages of CLI_KEYED_PAGE,
// each page a cell taken as its first place is and given back as the keeper empties, how many places were ever taken,
// and the most there are, at most CLI_KEYED_PAGE pages of them
typedef struct
{
    const char *name;
    CliKeyedEntry *page[CLI_KEYED_PAGES];
    unsigned int entries;
    unsigned int entries_most;
} CliKeyed;

// the entry at the place `at`, below the places ever taken
CliKeyedEntry *cli_keyed_entry(const CliKeyed *keyed, unsigned int at);

// the texts made empty, at most `entries_most` of them; `size` bytes of `text` stored under `key` with `beside`, in
// place of what it contained: 1, or 0, reported, where they do not fit; the text stored under `key`, its size into
// `size` and the text beside it into `beside`: NULL where none is stored, each good until the texts next change; the
// text under `key` taken out; every text taken out; each key handed to `each` with its texts; and the bytes of every
// key and the text beside it, each ended by a 0
void cli_keyed_begun(CliKeyed *keyed, const char *name, unsigned int entries_most);
int cli_keyed_put(CliKeyed *keyed, const char *key, const char *beside, const char *text, unsigned long long size);
const char *cli_keyed_get(const CliKeyed *keyed, const char *key, const char **beside, unsigned long long *size);
// the hash a key is found by, for a key looked up more than once; and the same get with that hash
unsigned int cli_keyed_hash(const char *key);
const char *cli_keyed_get_hashed(const CliKeyed *keyed, const char *key, unsigned int hash, const char **beside,
                                 unsigned long long *size);
void cli_keyed_remove(CliKeyed *keyed, const char *key);
void cli_keyed_clear(CliKeyed *keyed);
void cli_keyed_each(const CliKeyed *keyed,
                    void (*each)(void *context, const char *key, const char *beside, const char *text,
                                 unsigned long long size),
                    void *context);
unsigned long long cli_keyed_listed_size(const CliKeyed *keyed);

#endif
