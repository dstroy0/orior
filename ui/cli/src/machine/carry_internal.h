// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry_internal.h: what the carry sources share: files carried saved in order, settings read and written, and the
// files of a folder that travel
#ifndef CARRY_INTERNAL_H
#define CARRY_INTERNAL_H
typedef struct CliCarryCarriedPlaceCtx CliCarryCarriedPlaceCtx;
typedef struct CliCarryRoomCtx CliCarryRoomCtx;
typedef struct CliCarryRoomObjectCtx CliCarryRoomObjectCtx;
typedef struct CliCarryOutCtx CliCarryOutCtx;
typedef struct CliCarryCarriedCtx CliCarryCarriedCtx;
typedef struct CliCarryCarriedAtCtx CliCarryCarriedAtCtx;
typedef struct CliCarryFolderCtx CliCarryFolderCtx;
typedef struct CliCarryCtx CliCarryCtx;

#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "carry.h"
#include "embed_dispatch_layout.h"

// What a carried file declares it is
#define CLI_CARRY_MARK "orior settings"

// the bytes a settings text written is written in
#define CLI_CARRY_WRITTEN_ROOM (16ull << 20u)

// The room settings are read in: the tree, a cell sized to the texts read into it, begun empty under the lock of the
// work that reads them and given back before it is given
typedef struct
{
    CliJson json;
} CliCarryRoom;

// a room begun empty, and its tree given back (carry.c)
void cli_carry_room_begin(const CliCarryRoomObjectCtx *args);
void cli_carry_room_free(const CliCarryRoomObjectCtx *args);

// the files carried made empty; `size` bytes of `value` put at `place`, in the order of the places, one there already
// written over: 1, or 0 where they do not fit; the place of `place` among them, or their count where it is not one;
// and the place and the value at `at` (carry.c)
void cli_carried_clear(const CliCarryCarriedPlaceCtx *args);
// the most bytes the files carried write to as one JSON object, each byte escaped to at most 6
unsigned long long cli_carried_sent_room(const CliCarryCarriedCtx *args);
int cli_carried_put(const CliCarryCarriedPlaceCtx *args);
unsigned int cli_carried_find(const CliCarryCarriedCtx *args);
const char *cli_carried_place(const CliCarryCarriedAtCtx *args);
const char *cli_carried_value(const CliCarryCarriedAtCtx *args);

// whether a setting's key names this machine's own and stays on it; `size` bytes of settings text read into the room's
// tree as an object, an empty one where it is no object: the object, CLI_JSON_NONE where the tree's cell is refused;
// the object written with its keys in order, as settings.json is written, into `out`; whether `place` is one a
// carried file may write; and the files of `folder` that travel, by place, each with its path. A settings text is
// read into a tree begun afresh in a cell sized to it with room over for `beside` bytes of settings read beside it and
// copied over into it, or read beside what the tree contains, into that room (carry.c)
int cli_carry_machine(const CliCarryCtx *args);
unsigned int cli_carry_settings_get(const CliCarryRoomCtx *args);
unsigned int cli_carry_settings_beside(const CliCarryRoomCtx *args);
void cli_carry_settings_write(const CliCarryRoomObjectCtx *args);
int cli_carry_allow(const CliCarryCarriedCtx *args);
void cli_carry_files(const CliCarryFolderCtx *args);

// the most takings saved, and the most files one taking writes
#define CLI_TAKINGS 1024u
#define CLI_TAKING_FILES 4096u

// A taking as saved: its id, when, from where, and each file it wrote with whether one stood there before
typedef struct
{
    char id[32];
    unsigned long long at;
    char from[CLI_PATH_MAX_LEN];
    unsigned int files;
    char place[CLI_TAKING_FILES][CLI_NAME_MAX_LEN];
    unsigned char stood[CLI_TAKING_FILES];
} CliTaking;

// a taking written as JSON, laid out as serde_json's pretty writer lays it out where `pretty` is set (carry_undo.c)
void cli_carry_taking_write(const CliCarryOutCtx *args);

struct CliCarryCarriedPlaceCtx
{
    CliCarried *carried_out;
    const char *place;
    const char *value;
    unsigned long long size;
};

struct CliCarryRoomCtx
{
    CliCarryRoom *room;
    unsigned long long size;
    const char *text;
    unsigned long long beside;
};

struct CliCarryRoomObjectCtx
{
    CliCarryRoom *room;
    unsigned int object;
    VerbaLinea *out;
};

struct CliCarryOutCtx
{
    VerbaLinea *out;
    const CliTaking *taking;
    int pretty;
};

struct CliCarryCarriedCtx
{
    const CliCarried *carried;
    const char *place;
};

struct CliCarryCarriedAtCtx
{
    const CliCarried *carried;
    unsigned int at;
};

struct CliCarryFolderCtx
{
    const char *folder;
    CliCarried *found;
};

struct CliCarryCtx
{
    const char *key;
};

typedef struct
{
    void (*room_begin)(const CliCarryRoomObjectCtx *args);
    void (*room_free)(const CliCarryRoomObjectCtx *args);
    void (*settings_write)(const CliCarryRoomObjectCtx *args);
} CliCarryRoomObjectNs;
EMBED_TABLE_LAYOUT(CliCarryRoomObjectNs, room_begin, room_free, settings_write);
EMBED_TABLE_STORAGE CliCarryRoomObjectNs cli_carry_room_object_internal EMBED_UNUSED = {
    .room_begin = cli_carry_room_begin,
    .room_free = cli_carry_room_free,
    .settings_write = cli_carry_settings_write,
};

typedef struct
{
    void (*carried_clear)(const CliCarryCarriedPlaceCtx *args);
    int (*carried_put)(const CliCarryCarriedPlaceCtx *args);
} CliCarryCarriedPlaceNs;
EMBED_TABLE_LAYOUT(CliCarryCarriedPlaceNs, carried_clear, carried_put);
EMBED_TABLE_STORAGE CliCarryCarriedPlaceNs cli_carry_carried_place_internal EMBED_UNUSED = {
    .carried_clear = cli_carried_clear,
    .carried_put = cli_carried_put,
};

typedef struct
{
    unsigned long long (*carried_sent_room)(const CliCarryCarriedCtx *args);
    unsigned int (*carried_find)(const CliCarryCarriedCtx *args);
    int (*allow)(const CliCarryCarriedCtx *args);
} CliCarryCarriedNs;
EMBED_TABLE_LAYOUT(CliCarryCarriedNs, carried_sent_room, carried_find, allow);
EMBED_TABLE_STORAGE CliCarryCarriedNs cli_carry_carried_internal EMBED_UNUSED = {
    .carried_sent_room = cli_carried_sent_room,
    .carried_find = cli_carried_find,
    .allow = cli_carry_allow,
};

typedef struct
{
    const char *(*carried_place)(const CliCarryCarriedAtCtx *args);
    const char *(*carried_value)(const CliCarryCarriedAtCtx *args);
} CliCarryCarriedAtNs;
EMBED_TABLE_LAYOUT(CliCarryCarriedAtNs, carried_place, carried_value);
EMBED_TABLE_STORAGE CliCarryCarriedAtNs cli_carry_carried_at_internal EMBED_UNUSED = {
    .carried_place = cli_carried_place,
    .carried_value = cli_carried_value,
};

typedef struct
{
    int (*machine)(const CliCarryCtx *args);
} CliCarryNsTable;
EMBED_TABLE_LAYOUT(CliCarryNsTable, machine);
EMBED_TABLE_STORAGE CliCarryNsTable cli_carry_internal EMBED_UNUSED = {
    .machine = cli_carry_machine,
};

typedef struct
{
    unsigned int (*settings_get)(const CliCarryRoomCtx *args);
    unsigned int (*settings_beside)(const CliCarryRoomCtx *args);
} CliCarryRoomNs;
EMBED_TABLE_LAYOUT(CliCarryRoomNs, settings_get, settings_beside);
EMBED_TABLE_STORAGE CliCarryRoomNs cli_carry_room_internal EMBED_UNUSED = {
    .settings_get = cli_carry_settings_get,
    .settings_beside = cli_carry_settings_beside,
};

typedef struct
{
    void (*files)(const CliCarryFolderCtx *args);
} CliCarryFolderNs;
EMBED_TABLE_LAYOUT(CliCarryFolderNs, files);
EMBED_TABLE_STORAGE CliCarryFolderNs cli_carry_folder_internal EMBED_UNUSED = {
    .files = cli_carry_files,
};

typedef struct
{
    void (*taking_write)(const CliCarryOutCtx *args);
} CliCarryOutNs;
EMBED_TABLE_LAYOUT(CliCarryOutNs, taking_write);
EMBED_TABLE_STORAGE CliCarryOutNs cli_carry_out_internal EMBED_UNUSED = {
    .taking_write = cli_carry_taking_write,
};

#endif
