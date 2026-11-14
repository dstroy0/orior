// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// keyed.h: texts stored by a key, each with a second text beside it, as the servers store each file's diagnostics, its
// findings and its text by the file's key and its URI. Each entry is stored whole in one movable cell the store owns,
// its key, its second text and its text one after another, each ended by a 0, an entry put again giving its old cell
// back; a put the cellblock or the entries cannot fit is refused and reported, with the store's name. A reader is
// passed the entry: its cell, the token the movable reads, writes and compares take, and where its three texts start
// in it. Nothing of a store's own is locked here: each store owns its own lock (keyed.c)
#ifndef KEYED_H
#define KEYED_H
#include "../command_line/cli_internal.h"
#include "../command_line/pool.h"
#include "embed_dispatch_layout.h"

typedef struct CliKeyedKeyAdjacentCfg CliKeyedKeyAdjacentCfg;
typedef struct CliKeyedKeyCfg CliKeyedKeyCfg;
typedef struct CliKeyedNameCfg CliKeyedNameCfg;
typedef struct CliKeyedEachCfg CliKeyedEachCfg;
typedef struct CliKeyedCfg CliKeyedCfg;

// An entry: whether it contains one, the hash of its key, its cell and the cell's size; its key's size, the key at 0
// in the cell; its second text's size, that text at `key_size` + 1; and its text's size, the text at `text_at`
typedef struct
{
    int used;
    unsigned int hash;
    const CarcerMovableEntry *cell;
    unsigned long long size;
    unsigned int key_size;
    unsigned int adjacent_size;
    unsigned long long text_at;
    unsigned long long text_size;
} CliKeyedEntry;

// what each entry of a store is passed to `each` as: the caller's context and the entry
typedef struct
{
    void *context;
    CliKeyedEntry entry;
} CliEachKeyedEntryCfg;

// the entries a page of a store's contains, the max pages a store takes, and no place
#define CLI_KEYED_PAGE 256u
#define CLI_KEYED_PAGES 256u
#define CLI_KEYED_NONE 0xffffffffu

// Texts stored by a key: the store's name, reported where a put is refused; the entries in pages of CLI_KEYED_PAGE,
// each page a movable cell taken as its first place is and given back as the store empties, how many places were ever
// taken, and the max there are, at max CLI_KEYED_PAGE pages of them
typedef struct
{
    const char *name;
    const CarcerMovableEntry *page[CLI_KEYED_PAGES];
    unsigned int entries;
    unsigned int entries_max;
} CliKeyed;

// the entry at the place `at`, below the places ever taken, read out of its page; the bytes of every key and the text
// beside it, each ended by a 0; and the entry at `at` taken out
CliKeyedEntry cli_keyed_entry(const CliKeyedCfg *args);
unsigned long long cli_keyed_listing_size(const CliKeyedCfg *args);
void cli_keyed_remove_at(const CliKeyedCfg *args);
// the texts made empty, at max `entries_max` of them; `size` bytes of `text`, or of `text_cell` from `text_at` where
// that is given, stored under `key` with `adjacent`, in place of what it contained: 1, or 0, reported, where they do
// not fit; the place of the entry stored under `key`, CLI_KEYED_NONE where none is; the text under `key` taken out;
// every text taken out; and each entry that contains one passed to `each`
void cli_keyed_begin(const CliKeyedNameCfg *args);
int cli_keyed_append(const CliKeyedKeyAdjacentCfg *args);
unsigned int cli_keyed_find(const CliKeyedKeyAdjacentCfg *args);
// the hash a key is found by, for a key looked up more than once; and the same find with that hash
unsigned int cli_keyed_hash(const CliKeyedKeyAdjacentCfg *args);
unsigned int cli_keyed_find_hash(const CliKeyedKeyCfg *args);
void cli_keyed_remove(const CliKeyedKeyAdjacentCfg *args);
void cli_keyed_clear(const CliKeyedNameCfg *args);
void cli_keyed_each(const CliKeyedEachCfg *args);

struct CliKeyedKeyAdjacentCfg
{
    const CliKeyed *keyed;
    CliKeyed *keyed_out;
    const char *key;
    const char *adjacent_value;
    const char *text;
    const CarcerMovableEntry *text_cell;
    unsigned long long text_at;
    unsigned long long size_value;
};

struct CliKeyedKeyCfg
{
    const CliKeyed *keyed;
    const char *key;
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
    void (*each)(const CliEachKeyedEntryCfg *args);
    void *context;
};

struct CliKeyedCfg
{
    const CliKeyed *keyed;
    CliKeyed *keyed_out;
    unsigned int at;
};

typedef struct
{
    CliKeyedEntry (*entry)(const CliKeyedCfg *args);
    unsigned long long (*listed_size)(const CliKeyedCfg *args);
    void (*remove_at)(const CliKeyedCfg *args);
} CliKeyedNs;
EMBED_TABLE_LAYOUT(CliKeyedNs, entry, listed_size, remove_at);
EMBED_TABLE_STORAGE CliKeyedNs cli_keyed_table EMBED_UNUSED = {
    .entry = cli_keyed_entry,
    .listed_size = cli_keyed_listing_size,
    .remove_at = cli_keyed_remove_at,
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
    unsigned int (*find)(const CliKeyedKeyAdjacentCfg *args);
    unsigned int (*hash)(const CliKeyedKeyAdjacentCfg *args);
    void (*remove)(const CliKeyedKeyAdjacentCfg *args);
} CliKeyedKeyAdjacentNs;
EMBED_TABLE_LAYOUT(CliKeyedKeyAdjacentNs, put, find, hash, remove);
EMBED_TABLE_STORAGE CliKeyedKeyAdjacentNs cli_keyed_key_adjacent EMBED_UNUSED = {
    .put = cli_keyed_append,
    .find = cli_keyed_find,
    .hash = cli_keyed_hash,
    .remove = cli_keyed_remove,
};

typedef struct
{
    unsigned int (*find_hash)(const CliKeyedKeyCfg *args);
} CliKeyedKeyNs;
EMBED_TABLE_LAYOUT(CliKeyedKeyNs, find_hash);
EMBED_TABLE_STORAGE CliKeyedKeyNs cli_keyed_key EMBED_UNUSED = {
    .find_hash = cli_keyed_find_hash,
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
