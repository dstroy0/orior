// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// machine_transfer_internal.h: what the carry sources share: files carried saved in order, settings read and written,
// and the files of a folder that travel
#ifndef CARRY_INTERNAL_H
#define CARRY_INTERNAL_H
typedef struct CliMachineTransferTransferredPositionCtx CliMachineTransferTransferredPositionCtx;
typedef struct CliMachineTransferWorkStateCtx CliMachineTransferWorkStateCtx;
typedef struct CliMachineTransferWorkStateObjectCtx CliMachineTransferWorkStateObjectCtx;
typedef struct CliMachineTransferOutCtx CliMachineTransferOutCtx;
typedef struct CliMachineTransferTransferredCtx CliMachineTransferTransferredCtx;
typedef struct CliMachineTransferTransferredAtCtx CliMachineTransferTransferredAtCtx;
typedef struct CliMachineTransferFolderCtx CliMachineTransferFolderCtx;
typedef struct CliMachineTransferCtx CliMachineTransferCtx;

#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "carry.h"
#include "embed_dispatch_layout.h"

// What a carried file declares it is
#define CLI_MACHINE_TRANSFER_MARK "orior settings"

// the bytes a settings text written is written in
#define CLI_MACHINE_TRANSFER_WRITTEN_CAPACITY (16ull << 20u)

// The work state settings are read in: the tree, a cell sized to the texts read into it, begun empty under the lock of
// the work that reads them and given back before it is given
typedef struct
{
    CliJson json;
} CliMachineTransferWorkState;

// a work state begun empty, and its tree given back (carry.c)
void cli_machine_transfer_work_state_begin(const CliMachineTransferWorkStateObjectCtx *args);
void cli_machine_transfer_work_state_free(const CliMachineTransferWorkStateObjectCtx *args);

// the files carried made empty; `size` bytes of `value` put at `place`, in the order of the places, one there already
// written over: 1, or 0 where they do not fit; the place of `place` among them, or their count where it is not one;
// and the place and the value at `at` (carry.c)
void cli_transferred_clear(const CliMachineTransferTransferredPositionCtx *args);
// the most bytes the files carried write to as one JSON object, each byte escaped to at most 6
unsigned long long cli_transferred_sent_capacity(const CliMachineTransferTransferredCtx *args);
int cli_transferred_append(const CliMachineTransferTransferredPositionCtx *args);
unsigned int cli_transferred_find(const CliMachineTransferTransferredCtx *args);
const char *cli_transferred_position(const CliMachineTransferTransferredAtCtx *args);
const char *cli_transferred_value(const CliMachineTransferTransferredAtCtx *args);

// whether a setting's key names this machine's own and stays on it; `size` bytes of settings text read into the work
// state's tree as an object, an empty one where it is no object: the object, CLI_JSON_NONE where the tree's cell is
// refused; the object written with its keys in order, as settings.json is written, into `out`; whether `place` is one a
// carried file may write; and the files of `folder` that travel, by place, each with its path. A settings text is
// read into a tree begun afresh in a cell sized to it with work state over for `adjacent` bytes of settings read beside
// it and copied over into it, or read beside what the tree contains, into that work state (carry.c)
int cli_machine_transfer_machine(const CliMachineTransferCtx *args);
unsigned int cli_machine_transfer_settings_get(const CliMachineTransferWorkStateCtx *args);
unsigned int cli_machine_transfer_settings_adjacent(const CliMachineTransferWorkStateCtx *args);
void cli_machine_transfer_settings_write(const CliMachineTransferWorkStateObjectCtx *args);
int cli_machine_transfer_allow(const CliMachineTransferTransferredCtx *args);
void cli_machine_transfer_files(const CliMachineTransferFolderCtx *args);

// the most takings saved, and the most files one taking writes
#define CLI_ALLOCATIONS 1024u
#define CLI_ALLOCATING_FILES 4096u

// the bytes of a taking's files and whether each stood there before, a cell of whoever reads or writes it
#define CLI_ALLOCATING_BYTES ((unsigned long long)CLI_ALLOCATING_FILES * (CLI_NAME_MAX_LEN + 1ull))

// A taking as saved: its id, when, from where, and each file it wrote with whether one stood there before
typedef struct
{
    char id[32];
    unsigned long long at;
    char from[CLI_PATH_MAX_LEN];
    unsigned int files;
    char (*place)[CLI_NAME_MAX_LEN];
    unsigned char *existed;
} CliAllocating;

// a taking written as JSON, laid out as serde_json's pretty writer lays it out where `pretty` is set
// (machine_transfer_undo.c)
void cli_machine_transfer_allocating_write(const CliMachineTransferOutCtx *args);

struct CliMachineTransferTransferredPositionCtx
{
    CliTransferred *transferred_out;
    const char *place;
    const char *value;
    unsigned long long size;
};

struct CliMachineTransferWorkStateCtx
{
    CliMachineTransferWorkState *work_state;
    unsigned long long size;
    const char *text;
    unsigned long long adjacent;
};

struct CliMachineTransferWorkStateObjectCtx
{
    CliMachineTransferWorkState *work_state;
    unsigned int object;
    VerbaLinea *out;
};

struct CliMachineTransferOutCtx
{
    VerbaLinea *out;
    const CliAllocating *allocating;
    int pretty;
};

struct CliMachineTransferTransferredCtx
{
    const CliTransferred *carried;
    const char *place;
};

struct CliMachineTransferTransferredAtCtx
{
    const CliTransferred *carried;
    unsigned int at;
};

struct CliMachineTransferFolderCtx
{
    const char *folder;
    CliTransferred *found;
};

struct CliMachineTransferCtx
{
    const char *key;
};

typedef struct
{
    void (*capacity_begin)(const CliMachineTransferWorkStateObjectCtx *args);
    void (*capacity_free)(const CliMachineTransferWorkStateObjectCtx *args);
    void (*settings_write)(const CliMachineTransferWorkStateObjectCtx *args);
} CliMachineTransferWorkStateObjectNs;
EMBED_TABLE_LAYOUT(CliMachineTransferWorkStateObjectNs, capacity_begin, capacity_free, settings_write);
EMBED_TABLE_STORAGE CliMachineTransferWorkStateObjectNs cli_machine_transfer_capacity_object_internal EMBED_UNUSED = {
    .capacity_begin = cli_machine_transfer_work_state_begin,
    .capacity_free = cli_machine_transfer_work_state_free,
    .settings_write = cli_machine_transfer_settings_write,
};

typedef struct
{
    void (*transferred_clear)(const CliMachineTransferTransferredPositionCtx *args);
    int (*transferred_append)(const CliMachineTransferTransferredPositionCtx *args);
} CliMachineTransferTransferredPositionNs;
EMBED_TABLE_LAYOUT(CliMachineTransferTransferredPositionNs, transferred_clear, transferred_append);
EMBED_TABLE_STORAGE CliMachineTransferTransferredPositionNs cli_machine_transfer_transferred_position_internal
    EMBED_UNUSED = {
        .transferred_clear = cli_transferred_clear,
        .transferred_append = cli_transferred_append,
};

typedef struct
{
    unsigned long long (*transferred_sent_capacity)(const CliMachineTransferTransferredCtx *args);
    unsigned int (*transferred_find)(const CliMachineTransferTransferredCtx *args);
    int (*allow)(const CliMachineTransferTransferredCtx *args);
} CliMachineTransferTransferredNs;
EMBED_TABLE_LAYOUT(CliMachineTransferTransferredNs, transferred_sent_capacity, transferred_find, allow);
EMBED_TABLE_STORAGE CliMachineTransferTransferredNs cli_machine_transfer_transferred_internal EMBED_UNUSED = {
    .transferred_sent_capacity = cli_transferred_sent_capacity,
    .transferred_find = cli_transferred_find,
    .allow = cli_machine_transfer_allow,
};

typedef struct
{
    const char *(*transferred_position)(const CliMachineTransferTransferredAtCtx *args);
    const char *(*transferred_value)(const CliMachineTransferTransferredAtCtx *args);
} CliMachineTransferTransferredAtNs;
EMBED_TABLE_LAYOUT(CliMachineTransferTransferredAtNs, transferred_position, transferred_value);
EMBED_TABLE_STORAGE CliMachineTransferTransferredAtNs cli_machine_transfer_transferred_at_internal EMBED_UNUSED = {
    .transferred_position = cli_transferred_position,
    .transferred_value = cli_transferred_value,
};

typedef struct
{
    int (*machine)(const CliMachineTransferCtx *args);
} CliMachineTransferNsTable;
EMBED_TABLE_LAYOUT(CliMachineTransferNsTable, machine);
EMBED_TABLE_STORAGE CliMachineTransferNsTable cli_machine_transfer_internal EMBED_UNUSED = {
    .machine = cli_machine_transfer_machine,
};

typedef struct
{
    unsigned int (*settings_get)(const CliMachineTransferWorkStateCtx *args);
    unsigned int (*settings_adjacent)(const CliMachineTransferWorkStateCtx *args);
} CliMachineTransferWorkStateNs;
EMBED_TABLE_LAYOUT(CliMachineTransferWorkStateNs, settings_get, settings_adjacent);
EMBED_TABLE_STORAGE CliMachineTransferWorkStateNs cli_machine_transfer_capacity_internal EMBED_UNUSED = {
    .settings_get = cli_machine_transfer_settings_get,
    .settings_adjacent = cli_machine_transfer_settings_adjacent,
};

typedef struct
{
    void (*files)(const CliMachineTransferFolderCtx *args);
} CliMachineTransferFolderNs;
EMBED_TABLE_LAYOUT(CliMachineTransferFolderNs, files);
EMBED_TABLE_STORAGE CliMachineTransferFolderNs cli_machine_transfer_folder_internal EMBED_UNUSED = {
    .files = cli_machine_transfer_files,
};

typedef struct
{
    void (*allocating_write)(const CliMachineTransferOutCtx *args);
} CliMachineTransferOutNs;
EMBED_TABLE_LAYOUT(CliMachineTransferOutNs, allocating_write);
EMBED_TABLE_STORAGE CliMachineTransferOutNs cli_machine_transfer_out_internal EMBED_UNUSED = {
    .allocating_write = cli_machine_transfer_allocating_write,
};

#endif
