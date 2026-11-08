// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// state.h: what the window stores between runs, as files in orior's own folder, laid out as the README's "orior's own
// folder" describes (state.c, state_save.c):
//
// - `settings.json`, the reader's settings, those the settings list names, each under its key, and the keys of a
//   settings file's own that settings.c reads, which the page is not given;
// - `themes/`, each theme of the reader's in a file of its own, named for the theme, the themes read back in the order
//   of their files' names;
// - `state.json`, what the window stores of itself: its panes, its sessions, the files opened last, the places left in
//   them, bookmarks, breakpoints and watches;
// - `backups/`, the text of each tab with changes not saved, a file each.
//
// The page stores each entry as text under a key starting `orior.`; here the key goes without it. An entry whose text
// is JSON other than a string is written as JSON, and any other as a string, and each reads back as the text it was. A
// key the reader takes out of a file is gone at the next start, and the window's default stands for it
#ifndef STATE_H
#define STATE_H
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"

typedef struct CliStateFolderCfg CliStateFolderCfg;
typedef struct CliStateJsonCfg CliStateJsonCfg;
typedef struct CliStateOutCfg CliStateOutCfg;
typedef struct CliStateCfg CliStateCfg;

// whether the entry `key`, without `orior.`, is a setting or the list of themes, those an import of another machine's
// settings changes
int cli_state_is_synced(const CliStateCfg *args);
// an entry as the page stores it, from the value written: a string's text, else the value as compact JSON, after `out`
void cli_state_read_back(const CliStateJsonCfg *args);
// every entry stored in `folder`, by its key without `orior.`, written as a JSON object of texts in the order of the
// keys into `out`: 1, or 0 where the window has stored nothing there yet, which is while there is no `state.json`
int cli_state_read(const CliStateOutCfg *args);
// the changes in `folder` stored, given as a JSON object of each key without `orior.` and its new text, or null where
// it is gone, `length` bytes of `changes`; `state.json` made where it is not there yet: 1, or 0 with why given
// (state_save.c)
int cli_state_save(const CliStateFolderCfg *args);

// One entry of orior's folder, its key without `orior.` and its text
typedef struct
{
    const char *key;
    const char *value;
} CliStateEntry;

// Every entry of orior's folder in one cell: how many and each
typedef struct
{
    unsigned int count;
    CliStateEntry *entry;
} CliStateEntries;

// every entry stored in `folder` read into a cell of `owner`, the files' JSON decoded here at their edge: the cell, or
// NULL where the window has stored nothing there yet or the cells cannot meet it; and the text of `key` among them,
// NULL where none is
typedef struct
{
    const char *folder;
    const CellOwner *owner;
} CliStateEntriesCfg;
CliStateEntries *cli_state_entries_alloc(const CliStateEntriesCfg *args);
const char *cli_state_entry_value(const CliStateEntries *entries, const char *key);

// the text `value` stored under `key`, without `orior.`, in `folder`, written as the file's JSON here at its edge: 1,
// or 0 with why given
typedef struct
{
    const char *folder;
    const char *key;
    const char *value;
    VerbaLinea *err_msg;
} CliStateValueCfg;
int cli_state_value_save(const CliStateValueCfg *args);

struct CliStateFolderCfg
{
    const char *folder;
    const char *changes;
    unsigned long long length;
    VerbaLinea *err_msg;
};

struct CliStateJsonCfg
{
    const CliJson *json;
    unsigned int value;
    VerbaLinea *out;
};

struct CliStateOutCfg
{
    VerbaLinea *out;
    const char *folder;
};

struct CliStateCfg
{
    const char *key;
};

typedef struct
{
    int (*is_synced)(const CliStateCfg *args);
} CliStateNs;
EMBED_TABLE_LAYOUT(CliStateNs, is_synced);
EMBED_TABLE_STORAGE CliStateNs cli_state_table EMBED_UNUSED = {
    .is_synced = cli_state_is_synced,
};

typedef struct
{
    void (*read_back)(const CliStateJsonCfg *args);
} CliStateJsonNs;
EMBED_TABLE_LAYOUT(CliStateJsonNs, read_back);
EMBED_TABLE_STORAGE CliStateJsonNs cli_state_json EMBED_UNUSED = {
    .read_back = cli_state_read_back,
};

typedef struct
{
    int (*read)(const CliStateOutCfg *args);
} CliStateOutNs;
EMBED_TABLE_LAYOUT(CliStateOutNs, read);
EMBED_TABLE_STORAGE CliStateOutNs cli_state_out EMBED_UNUSED = {
    .read = cli_state_read,
};

typedef struct
{
    int (*save)(const CliStateFolderCfg *args);
} CliStateFolderNs;
EMBED_TABLE_LAYOUT(CliStateFolderNs, save);
EMBED_TABLE_STORAGE CliStateFolderNs cli_state_folder EMBED_UNUSED = {
    .save = cli_state_save,
};

#endif
