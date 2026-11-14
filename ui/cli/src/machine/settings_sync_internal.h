// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_sync_internal.h: what the sync sources share: files synced saved in order, settings read and written,
// and the files of a folder that travel
#ifndef CARRY_INTERNAL_H
#define CARRY_INTERNAL_H
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"
#include "settings_sync.h"

typedef struct CliSettingsSyncTransferredPositionCtx CliSettingsSyncTransferredPositionCtx;
typedef struct CliSettingsSyncWorkStateCtx CliSettingsSyncWorkStateCtx;
typedef struct CliSettingsSyncWorkStateObjectCtx CliSettingsSyncWorkStateObjectCtx;
typedef struct CliSettingsSyncOutCtx CliSettingsSyncOutCtx;
typedef struct CliSettingsSyncTransferredCtx CliSettingsSyncTransferredCtx;
typedef struct CliSettingsSyncTransferredAtCtx CliSettingsSyncTransferredAtCtx;
typedef struct CliSettingsSyncFolderCtx CliSettingsSyncFolderCtx;
typedef struct CliSettingsSyncCtx CliSettingsSyncCtx;

// What a synced file declares it is
#define CLI_SETTINGS_SYNC_MARK "orior settings"

// the bytes a settings text written is written in
#define CLI_SETTINGS_SYNC_WRITTEN_CAPACITY (16ull << 20u)

// The work state settings are read in: the tree, a cell sized to the texts read into it, begun empty under the lock of
// the work that reads them and given back before it is given
typedef struct
{
    CliJson json;
} CliSettingsSyncWorkState;

// a work state begun empty, and its tree given back (settings_sync.c)
void cli_settings_sync_work_state_begin(const CliSettingsSyncWorkStateObjectCtx *args);

// the files synced made empty; `size` bytes of `value` put at `place`, in the order of the places, one there already
// written over: 1, or 0 where they do not fit; the place of `place` among them, or their count where it is not one;
// and the place and the value at `at` (settings_sync.c)
void cli_transferred_clear(const CliSettingsSyncTransferredPositionCtx *args);
// the most bytes the files synced write to as one JSON object, each byte escaped to at most 6
unsigned long long cli_transferred_sent_capacity(const CliSettingsSyncTransferredCtx *args);
int cli_transferred_append(const CliSettingsSyncTransferredPositionCtx *args);
unsigned int cli_transferred_find(const CliSettingsSyncTransferredCtx *args);
const char *cli_transferred_position(const CliSettingsSyncTransferredAtCtx *args);
const char *cli_transferred_value(const CliSettingsSyncTransferredAtCtx *args);

// whether a setting's key names this machine's own and stays on it; `size` bytes of settings text read into the work
// state's tree as an object, an empty one where it is no object: the object, CLI_JSON_NONE where the tree's cell is
// refused; the object written with its keys in order, as settings.json is written, into `out`; whether `place` is one a
// synced file may write; and the files of `folder` that travel, by place, each with its path. A settings text is
// read into a tree begun afresh in a cell sized to it with work state over for `adjacent` bytes of settings read beside
// it and copied over into it, or read beside what the tree contains, into that work state (settings_sync.c)
int cli_settings_sync_machine(const CliSettingsSyncCtx *args);
unsigned int cli_settings_sync_settings_get(const CliSettingsSyncWorkStateCtx *args);
unsigned int cli_settings_sync_settings_adjacent(const CliSettingsSyncWorkStateCtx *args);
void cli_settings_sync_settings_write(const CliSettingsSyncWorkStateObjectCtx *args);
int cli_settings_sync_allow(const CliSettingsSyncTransferredCtx *args);
void cli_settings_sync_files(const CliSettingsSyncFolderCtx *args);

// the most imports saved, and the most files one import writes
#define CLI_ALLOCATIONS 1024u
#define CLI_ALLOCATING_FILES 4096u

// the bytes of an import's files and whether each stood there before, a cell of whoever reads or writes it
#define CLI_ALLOCATING_BYTES ((unsigned long long)CLI_ALLOCATING_FILES * (CLI_NAME_MAX_LEN + 1ull))

// An import as saved: its id, when, from where, and each file it wrote with whether one stood there before
typedef struct
{
    char id[32];
    unsigned long long at;
    char from[CLI_PATH_MAX_LEN];
    unsigned int files;
    char (*place)[CLI_NAME_MAX_LEN];
    unsigned char *existed;
} CliAllocating;

// an import written as JSON, laid out as serde_json's pretty writer lays it out where `pretty` is set
// (settings_sync_undo.c)
void cli_settings_sync_allocating_write(const CliSettingsSyncOutCtx *args);

struct CliSettingsSyncTransferredPositionCtx
{
    CliTransferred *transferred_out;
    const char *place;
    const char *value;
    unsigned long long size;
};

struct CliSettingsSyncWorkStateCtx
{
    CliSettingsSyncWorkState *work_state;
    unsigned long long size;
    const char *text;
    unsigned long long adjacent;
};

struct CliSettingsSyncWorkStateObjectCtx
{
    CliSettingsSyncWorkState *work_state;
    unsigned int object;
    VerbaLinea *out;
};

struct CliSettingsSyncOutCtx
{
    VerbaLinea *out;
    const CliAllocating *allocating;
    int pretty;
};

struct CliSettingsSyncTransferredCtx
{
    const CliTransferred *synced;
    const char *place;
};

struct CliSettingsSyncTransferredAtCtx
{
    const CliTransferred *synced;
    unsigned int at;
};

struct CliSettingsSyncFolderCtx
{
    const char *folder;
    CliTransferred *found;
};

struct CliSettingsSyncCtx
{
    const char *key;
};

typedef struct
{
    void (*capacity_begin)(const CliSettingsSyncWorkStateObjectCtx *args);
    void (*settings_write)(const CliSettingsSyncWorkStateObjectCtx *args);
} CliSettingsSyncWorkStateObjectNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncWorkStateObjectNs, capacity_begin, settings_write);
EMBED_TABLE_STORAGE CliSettingsSyncWorkStateObjectNs cli_settings_sync_capacity_object_internal EMBED_UNUSED = {
    .capacity_begin = cli_settings_sync_work_state_begin,
    .settings_write = cli_settings_sync_settings_write,
};

typedef struct
{
    void (*transferred_clear)(const CliSettingsSyncTransferredPositionCtx *args);
    int (*transferred_append)(const CliSettingsSyncTransferredPositionCtx *args);
} CliSettingsSyncTransferredPositionNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncTransferredPositionNs, transferred_clear, transferred_append);
EMBED_TABLE_STORAGE CliSettingsSyncTransferredPositionNs cli_settings_sync_transferred_position_internal
    EMBED_UNUSED = {
        .transferred_clear = cli_transferred_clear,
        .transferred_append = cli_transferred_append,
};

typedef struct
{
    unsigned long long (*transferred_sent_capacity)(const CliSettingsSyncTransferredCtx *args);
    unsigned int (*transferred_find)(const CliSettingsSyncTransferredCtx *args);
    int (*allow)(const CliSettingsSyncTransferredCtx *args);
} CliSettingsSyncTransferredNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncTransferredNs, transferred_sent_capacity, transferred_find, allow);
EMBED_TABLE_STORAGE CliSettingsSyncTransferredNs cli_settings_sync_transferred_internal EMBED_UNUSED = {
    .transferred_sent_capacity = cli_transferred_sent_capacity,
    .transferred_find = cli_transferred_find,
    .allow = cli_settings_sync_allow,
};

typedef struct
{
    const char *(*transferred_position)(const CliSettingsSyncTransferredAtCtx *args);
    const char *(*transferred_value)(const CliSettingsSyncTransferredAtCtx *args);
} CliSettingsSyncTransferredAtNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncTransferredAtNs, transferred_position, transferred_value);
EMBED_TABLE_STORAGE CliSettingsSyncTransferredAtNs cli_settings_sync_transferred_at_internal EMBED_UNUSED = {
    .transferred_position = cli_transferred_position,
    .transferred_value = cli_transferred_value,
};

typedef struct
{
    int (*machine)(const CliSettingsSyncCtx *args);
} CliSettingsSyncNsTable;
EMBED_TABLE_LAYOUT(CliSettingsSyncNsTable, machine);
EMBED_TABLE_STORAGE CliSettingsSyncNsTable cli_settings_sync_internal EMBED_UNUSED = {
    .machine = cli_settings_sync_machine,
};

typedef struct
{
    unsigned int (*settings_get)(const CliSettingsSyncWorkStateCtx *args);
    unsigned int (*settings_adjacent)(const CliSettingsSyncWorkStateCtx *args);
} CliSettingsSyncWorkStateNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncWorkStateNs, settings_get, settings_adjacent);
EMBED_TABLE_STORAGE CliSettingsSyncWorkStateNs cli_settings_sync_capacity_internal EMBED_UNUSED = {
    .settings_get = cli_settings_sync_settings_get,
    .settings_adjacent = cli_settings_sync_settings_adjacent,
};

typedef struct
{
    void (*files)(const CliSettingsSyncFolderCtx *args);
} CliSettingsSyncFolderNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncFolderNs, files);
EMBED_TABLE_STORAGE CliSettingsSyncFolderNs cli_settings_sync_folder_internal EMBED_UNUSED = {
    .files = cli_settings_sync_files,
};

typedef struct
{
    void (*allocating_write)(const CliSettingsSyncOutCtx *args);
} CliSettingsSyncOutNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncOutNs, allocating_write);
EMBED_TABLE_STORAGE CliSettingsSyncOutNs cli_settings_sync_out_internal EMBED_UNUSED = {
    .allocating_write = cli_settings_sync_allocating_write,
};

#endif
