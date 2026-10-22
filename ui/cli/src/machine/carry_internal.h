// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry_internal.h: what the carry sources share: files carried kept in order, settings read and written, and the
// files of a folder that travel
#ifndef CARRY_INTERNAL_H
#define CARRY_INTERNAL_H

#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "carry.h"

// What a carried file says it is
#define CLI_CARRY_MARK "orior settings"

// the bytes a settings text written is begun in before it grows
#define CLI_CARRY_WRITTEN_FIRST (4ull << 10u)

// The room settings are read in: the tree, a cell sized to the texts read into it, begun empty under the lock of the
// work that reads them and given back before it is given
typedef struct
{
    CliJson json;
} CliCarryRoom;

// a room begun empty, and its tree given back (carry.c)
void cli_carry_room_begun(CliCarryRoom *room);
void cli_carry_room_given(CliCarryRoom *room);

// the files carried made empty; `size` bytes of `value` put at `place`, in the order of the places, one there already
// written over: 1, or 0 where they do not fit; the place of `place` among them, or their count where it is not one;
// and the place and the value at `at` (carry.c)
void cli_carried_clear(CliCarried *carried);
int cli_carried_put(CliCarried *carried, const char *place, const char *value, unsigned long long size);
unsigned int cli_carried_find(const CliCarried *carried, const char *place);
const char *cli_carried_place(const CliCarried *carried, unsigned int at);
const char *cli_carried_value(const CliCarried *carried, unsigned int at);

// whether a setting's key names this machine's own and stays on it; `size` bytes of settings text read into the room's
// tree as an object, an empty one where it is no object: the object, CLI_JSON_NONE where the tree's cell is refused;
// the object written with its keys in order, as settings.json is written, into `out`; whether `place` is one a
// carried file may write; and the files of `folder` that travel, by place, each with its path. A settings text is
// read into a tree begun afresh in a cell sized to it with room over for `beside` bytes of settings read beside it and
// copied over into it, or read beside what the tree holds, into that room (carry.c)
int cli_carry_machine(const char *key);
unsigned int cli_carry_settings_of(CliCarryRoom *room, const char *text, unsigned long long size,
                                   unsigned long long beside);
unsigned int cli_carry_settings_beside(CliCarryRoom *room, const char *text, unsigned long long size);
void cli_carry_settings_written(CliCarryRoom *room, unsigned int object, CliLine *out);
int cli_carry_allowed(const char *place);
void cli_carry_files(const char *folder, CliCarried *found);

// the most takings kept, and the most files one taking writes
#define CLI_TAKINGS 1024u
#define CLI_TAKING_FILES 4096u

// A taking as kept: its id, when, from where, and each file it wrote with whether one stood there before
typedef struct
{
    char id[32];
    unsigned long long at;
    char from[CLI_PATH_LONGEST];
    unsigned int files;
    char place[CLI_TAKING_FILES][CLI_NAME_LONGEST];
    unsigned char stood[CLI_TAKING_FILES];
} CliTaking;

// a taking written as JSON, laid out as serde_json's pretty writer lays it out where `pretty` is set (carry_undo.c)
void cli_carry_taking_written(const CliTaking *taking, int pretty, CliLine *out);

#endif
