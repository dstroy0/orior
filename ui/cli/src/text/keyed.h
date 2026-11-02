// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keyed.h: texts stored by a key, each with a second text beside it, as the servers store each file's diagnostics, its
// findings and its text by the file's key and its URI. Each entry is stored whole in one cell of the stores' shared
// cellblock, its key, its second text and its text one after another, an entry put again giving its old cell back;
// a put the cellblock or the entries cannot fit is refused and reported, with the store's name. Nothing of a store's
// own is locked here: each store owns its own lock (keyed.c)
#ifndef KEYED_H
#define KEYED_H
typedef struct CliKeyedKeyAdjacentCfg CliKeyedKeyAdjacentCfg;
typedef struct CliKeyedKeyCfg CliKeyedKeyCfg;
typedef struct CliKeyedNameCfg CliKeyedNameCfg;
typedef struct CliKeyedEachCfg CliKeyedEachCfg;
typedef struct CliKeyedCfg CliKeyedCfg;

#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// An entry: whether it contains one, the hash of its key, its cell and the cell's size, its key's and its second text's
// sizes
typedef struct
{
    int used;
    unsigned int hash;
    char *cell;
    unsigned long long size;
    unsigned int key_size;
    unsigned int adjacent_size;
} CliKeyedEntry;

// the entries a page of a store's contains, and the most pages a store takes
#define CLI_KEYED_PAGE 256u
#define CLI_KEYED_PAGES 256u

// Texts stored by a key: the store's name, reported where a put is refused; the entries in pages of CLI_KEYED_PAGE,
// each page a cell taken as its first place is and given back as the store empties, how many places were ever taken,
// and the most there are, at most CLI_KEYED_PAGE pages of them
typedef struct
{
    const char *name;
    CliKeyedEntry *page[CLI_KEYED_PAGES];
    unsigned int entries;
    unsigned int entries_max;
} CliKeyed;

// the entry at the place `at`, below the places ever taken
CliKeyedEntry *cli_keyed_entry(const CliKeyedCfg *args);

// the texts made empty, at most `entries_max` of them; `size` bytes of `text` stored under `key` with `adjacent`, in
// place of what it contained: 1, or 0, reported, where they do not fit; the text stored under `key`, its size into
// `size` and the text beside it into `adjacent`: NULL where none is stored, each good until the texts next change; the
// text under `key` taken out; every text taken out; each key handed to `each` with its texts; and the bytes of every
// key and the text beside it, each ended by a 0
void cli_keyed_begin(const CliKeyedNameCfg *args);
int cli_keyed_append(const CliKeyedKeyAdjacentCfg *args);
const char *cli_keyed_get(const CliKeyedKeyAdjacentCfg *args);
// the hash a key is found by, for a key looked up more than once; and the same get with that hash
unsigned int cli_keyed_hash(const CliKeyedKeyAdjacentCfg *args);
const char *cli_keyed_get_hash(const CliKeyedKeyCfg *args);
void cli_keyed_remove(const CliKeyedKeyAdjacentCfg *args);
void cli_keyed_clear(const CliKeyedNameCfg *args);
void cli_keyed_each(const CliKeyedEachCfg *args);
unsigned long long cli_keyed_listing_size(const CliKeyedCfg *args);

struct CliKeyedKeyAdjacentCfg
{
    const CliKeyed *keyed;
    CliKeyed *keyed_out;
    const char *key;
    const char *adjacent_value;
    const char *text;
    unsigned long long size_value;
    const char **adjacent;
    unsigned long long *size;
};

struct CliKeyedKeyCfg
{
    const CliKeyed *keyed;
    const char *key;
    const char **adjacent;
    unsigned long long *size;
    unsigned int hash;
};

struct CliKeyedNameCfg
{
    CliKeyed *keyed_out;
    const char *name;
    unsigned int entries_max;
};

struct CliKeyedEachCfg
{
    const CliKeyed *keyed;
    void (*each)(const CliEachKeyAdjacentTextSizeCfg *args);
    void *context;
};

struct CliKeyedCfg
{
    const CliKeyed *keyed;
    unsigned int at;
};

typedef struct
{
    CliKeyedEntry *(*entry)(const CliKeyedCfg *args);
    unsigned long long (*listed_size)(const CliKeyedCfg *args);
} CliKeyedNs;
EMBED_TABLE_LAYOUT(CliKeyedNs, entry, listed_size);
EMBED_TABLE_STORAGE CliKeyedNs cli_keyed_table EMBED_UNUSED = {
    .entry = cli_keyed_entry,
    .listed_size = cli_keyed_listing_size,
};

typedef struct
{
    void (*begin)(const CliKeyedNameCfg *args);
    void (*clear)(const CliKeyedNameCfg *args);
} CliKeyedNameNs;
EMBED_TABLE_LAYOUT(CliKeyedNameNs, begin, clear);
EMBED_TABLE_STORAGE CliKeyedNameNs cli_keyed_name EMBED_UNUSED = {
    .begin = cli_keyed_begin,
    .clear = cli_keyed_clear,
};

typedef struct
{
    int (*put)(const CliKeyedKeyAdjacentCfg *args);
    const char *(*get)(const CliKeyedKeyAdjacentCfg *args);
    unsigned int (*hash)(const CliKeyedKeyAdjacentCfg *args);
    void (*remove)(const CliKeyedKeyAdjacentCfg *args);
} CliKeyedKeyAdjacentNs;
EMBED_TABLE_LAYOUT(CliKeyedKeyAdjacentNs, put, get, hash, remove);
EMBED_TABLE_STORAGE CliKeyedKeyAdjacentNs cli_keyed_key_adjacent EMBED_UNUSED = {
    .put = cli_keyed_append,
    .get = cli_keyed_get,
    .hash = cli_keyed_hash,
    .remove = cli_keyed_remove,
};

typedef struct
{
    const char *(*get_hash)(const CliKeyedKeyCfg *args);
} CliKeyedKeyNs;
EMBED_TABLE_LAYOUT(CliKeyedKeyNs, get_hash);
EMBED_TABLE_STORAGE CliKeyedKeyNs cli_keyed_key EMBED_UNUSED = {
    .get_hash = cli_keyed_get_hash,
};

typedef struct
{
    void (*each)(const CliKeyedEachCfg *args);
} CliKeyedEachNs;
EMBED_TABLE_LAYOUT(CliKeyedEachNs, each);
EMBED_TABLE_STORAGE CliKeyedEachNs cli_keyed_each_table EMBED_UNUSED = {
    .each = cli_keyed_each,
};

#endif
