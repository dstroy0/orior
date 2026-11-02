// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry.h: the reader's settings carried to another machine: settings.json, without its part for this machine alone and
// the settings that name this machine's own; the themes; the reader's plugins, and each plugin's own settings; the
// menus and toolbar, menus.json; the stylesheet, user.css; and the reader's language files. They go as one file, a JSON
// object of each file's text by its place in orior's own folder, or into a folder of the reader's laid out as orior's
// own folder lays them, which, where it is a repository, is committed and pushed, and pulled before it is read.
//
// Taking them in writes nothing until the reader has seen the differences and chosen what to take: each file chosen,
// and in settings.json each setting chosen, the settings of this machine alone retained as they are. Each file a taking
// writes over is saved as it was in `carried/`, a folder for each taking, and undoing the taking writes them back and
// takes out the files it made (carry.c, machine_transfer_send.c, machine_transfer_alloc.c, machine_transfer_undo.c)
#ifndef CARRY_H
#define CARRY_H
typedef struct CliMachineTransferHomeCfg CliMachineTransferHomeCfg;
typedef struct CliMachineTransferHomeToCfg CliMachineTransferHomeToCfg;
typedef struct CliMachineTransferHomeOutCfg CliMachineTransferHomeOutCfg;
typedef struct CliMachineTransferCfg CliMachineTransferCfg;
typedef struct CliMachineTransferFromCfg CliMachineTransferFromCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// the most files carried, and the most bytes of one's text
#define CLI_TRANSFERRED_MAX 4096u
#define CLI_TRANSFERRED_TEXT (1u << 25u)

// Files carried, by place in order: each place and its text, or its path, each a cell of its own ended by a 0, given
// back as it is put again and as the files are cleared
typedef struct
{
    char *place[CLI_TRANSFERRED_MAX];
    char *value[CLI_TRANSFERRED_MAX];
    unsigned long long size[CLI_TRANSFERRED_MAX];
    unsigned int count;
} CliTransferred;

// what travels from orior's own folder `home` into `packed`: each file's text by its place, settings.json without the
// settings of this machine alone: 1, or 0 with why given; and what travels from `home` written to `to`, as one file
// where `to` is not a folder, or laid out in the folder `to`, the files it no longer carries taken out of it,
// committed and pushed where it is a repository: 1 with what it did reported into `out`, or 0 with why given
int cli_machine_transfer_pack(const CliMachineTransferCfg *args);
int cli_machine_transfer_send(const CliMachineTransferHomeToCfg *args);

// what the file or the folder `from` carries into `there`, by place, a repository with a remote pulled first: 1, or 0
// with why given (carry.c)
int cli_machine_transfer_read(const CliMachineTransferFromCfg *args);

// how what `there` carries differs from orior's own folder `home`, the files alike left out, written as
// `[{"place", "how", "settings": [{"key", "here", "there"}]}]`; what `there`, read from `from`, carries taken in:
// each of `positions`, `count` of them, and of settings.json the settings of `keys`, `key_count` of them, alone, as
// there, a setting there does not set taken out here, and the settings of this machine alone retained, each file it
// writes over saved first in `carried/`, the taking written as `{"id", "at", "from", "files": [[place, stood]]}`: 1, or
// 0 with why given; every taking saved, the last first, written as a JSON array of them; and the taking `id` undone,
// each file it wrote over written back as it was, each it made taken out, and the taking forgotten, the taking written:
// 1, or 0 with why given (machine_transfer_alloc.c)
void cli_machine_transfer_differences(const CliMachineTransferHomeCfg *args);
int cli_machine_transfer_alloc(const CliMachineTransferHomeCfg *args);
void cli_machine_transfer_allocations(const CliMachineTransferHomeToCfg *args);
int cli_machine_transfer_undo(const CliMachineTransferHomeOutCfg *args);

struct CliMachineTransferHomeCfg
{
    const char *home;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const char *from;
    const CliTransferred *there;
    const char *const *positions;
    unsigned int count;
    const char *const *keys;
    unsigned int key_count;
};

struct CliMachineTransferHomeToCfg
{
    const char *home;
    VerbaLinea *err_msg;
    const char *to;
    VerbaLinea *out;
};

struct CliMachineTransferHomeOutCfg
{
    const char *home;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const char *id;
};

struct CliMachineTransferCfg
{
    const char *home;
    CliTransferred *packed;
    VerbaLinea *err_msg;
};

struct CliMachineTransferFromCfg
{
    VerbaLinea *err_msg;
    const char *from;
    CliTransferred *there_out;
};

typedef struct
{
    int (*pack)(const CliMachineTransferCfg *args);
} CliMachineTransferNs;
EMBED_TABLE_LAYOUT(CliMachineTransferNs, pack);
EMBED_TABLE_STORAGE CliMachineTransferNs cli_machine_transfer EMBED_UNUSED = {
    .pack = cli_machine_transfer_pack,
};

typedef struct
{
    int (*send)(const CliMachineTransferHomeToCfg *args);
    void (*allocations)(const CliMachineTransferHomeToCfg *args);
} CliMachineTransferHomeToNs;
EMBED_TABLE_LAYOUT(CliMachineTransferHomeToNs, send, allocations);
EMBED_TABLE_STORAGE CliMachineTransferHomeToNs cli_machine_transfer_home_to EMBED_UNUSED = {
    .send = cli_machine_transfer_send,
    .allocations = cli_machine_transfer_allocations,
};

typedef struct
{
    int (*read)(const CliMachineTransferFromCfg *args);
} CliMachineTransferFromNs;
EMBED_TABLE_LAYOUT(CliMachineTransferFromNs, read);
EMBED_TABLE_STORAGE CliMachineTransferFromNs cli_machine_transfer_from EMBED_UNUSED = {
    .read = cli_machine_transfer_read,
};

typedef struct
{
    void (*differences)(const CliMachineTransferHomeCfg *args);
    int (*take)(const CliMachineTransferHomeCfg *args);
} CliMachineTransferHomeNs;
EMBED_TABLE_LAYOUT(CliMachineTransferHomeNs, differences, take);
EMBED_TABLE_STORAGE CliMachineTransferHomeNs cli_machine_transfer_home EMBED_UNUSED = {
    .differences = cli_machine_transfer_differences,
    .take = cli_machine_transfer_alloc,
};

typedef struct
{
    int (*undo)(const CliMachineTransferHomeOutCfg *args);
} CliMachineTransferHomeOutNs;
EMBED_TABLE_LAYOUT(CliMachineTransferHomeOutNs, undo);
EMBED_TABLE_STORAGE CliMachineTransferHomeOutNs cli_machine_transfer_home_out EMBED_UNUSED = {
    .undo = cli_machine_transfer_undo,
};

#endif
