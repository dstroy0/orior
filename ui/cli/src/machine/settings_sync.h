// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_sync.h: the reader's settings synced to another machine: settings.json, without its part for this machine
// alone and the settings that name this machine's own; the themes; the reader's plugins, and each plugin's own
// settings; the menus and toolbar, menus.json; the stylesheet, user.css; and the reader's language files. They go as
// one file, a JSON object of each file's text by its place in orior's own folder, or into a folder of the reader's laid
// out as orior's own folder lays them, which, where it is a repository, is committed and pushed, and pulled before it
// is read.
//
// Taking them in writes nothing until the reader has seen the differences and chosen what to take: each file chosen,
// and in settings.json each setting chosen, the settings of this machine alone retained as they are. Each file an
// import writes over is saved as it was in `synced/`, a folder for each import, and undoing the import writes them back
// and takes out the files it made (settings_sync.c, settings_sync_export.c, settings_sync_import.c,
// settings_sync_undo.c)
#ifndef CARRY_H
#define CARRY_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliSettingsSyncHomeCfg CliSettingsSyncHomeCfg;
typedef struct CliSettingsSyncHomeToCfg CliSettingsSyncHomeToCfg;
typedef struct CliSettingsSyncHomeOutCfg CliSettingsSyncHomeOutCfg;
typedef struct CliSettingsSyncCfg CliSettingsSyncCfg;
typedef struct CliSettingsSyncFromCfg CliSettingsSyncFromCfg;

// the most files synced, and the most bytes of one's text
#define CLI_TRANSFERRED_MAX 4096u
#define CLI_TRANSFERRED_TEXT (1u << 25u)

// Files synced, by place in order: each place and its text, or its path, each a cell of its own ended by a 0, given
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
// where `to` is not a folder, or laid out in the folder `to`, the files it no longer syncs taken out of it,
// committed and pushed where it is a repository: 1 with what it did reported into `out`, or 0 with why given
int cli_settings_sync_pack(const CliSettingsSyncCfg *args);
int cli_settings_sync_send(const CliSettingsSyncHomeToCfg *args);

// what the file or the folder `from` syncs into `there`, by place, a repository with a remote pulled first: 1, or 0
// with why given (settings_sync.c)
int cli_settings_sync_read(const CliSettingsSyncFromCfg *args);

// how what `there` syncs differs from orior's own folder `home`, the files alike left out, written as
// `[{"place", "how", "settings": [{"key", "here", "there"}]}]`; what `there`, read from `from`, syncs taken in:
// each of `positions`, `count` of them, and of settings.json the settings of `keys`, `key_count` of them, alone, as
// there, a setting there does not set taken out here, and the settings of this machine alone retained, each file it
// writes over saved first in `synced/`, the import written as `{"id", "at", "from", "files": [[place, stood]]}`: 1, or
// 0 with why given; every import saved, the last first, written as a JSON array of them; and the import `id` undone,
// each file it wrote over written back as it was, each it made taken out, and the import forgotten, the import written:
// 1, or 0 with why given (settings_sync_import.c)
void cli_settings_sync_differences(const CliSettingsSyncHomeCfg *args);
int cli_settings_sync_alloc(const CliSettingsSyncHomeCfg *args);
void cli_settings_sync_allocations(const CliSettingsSyncHomeToCfg *args);
int cli_settings_sync_undo(const CliSettingsSyncHomeOutCfg *args);

struct CliSettingsSyncHomeCfg
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

struct CliSettingsSyncHomeToCfg
{
    const char *home;
    VerbaLinea *err_msg;
    const char *to;
    VerbaLinea *out;
};

struct CliSettingsSyncHomeOutCfg
{
    const char *home;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const char *id;
};

struct CliSettingsSyncCfg
{
    const char *home;
    CliTransferred *packed;
    VerbaLinea *err_msg;
};

struct CliSettingsSyncFromCfg
{
    VerbaLinea *err_msg;
    const char *from;
    CliTransferred *there_out;
};

typedef struct
{
    int (*pack)(const CliSettingsSyncCfg *args);
} CliSettingsSyncNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncNs, pack);
EMBED_TABLE_STORAGE CliSettingsSyncNs cli_settings_sync EMBED_UNUSED = {
    .pack = cli_settings_sync_pack,
};

typedef struct
{
    int (*send)(const CliSettingsSyncHomeToCfg *args);
    void (*allocations)(const CliSettingsSyncHomeToCfg *args);
} CliSettingsSyncHomeToNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncHomeToNs, send, allocations);
EMBED_TABLE_STORAGE CliSettingsSyncHomeToNs cli_settings_sync_home_to EMBED_UNUSED = {
    .send = cli_settings_sync_send,
    .allocations = cli_settings_sync_allocations,
};

typedef struct
{
    int (*read)(const CliSettingsSyncFromCfg *args);
} CliSettingsSyncFromNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncFromNs, read);
EMBED_TABLE_STORAGE CliSettingsSyncFromNs cli_settings_sync_from EMBED_UNUSED = {
    .read = cli_settings_sync_read,
};

typedef struct
{
    void (*differences)(const CliSettingsSyncHomeCfg *args);
    int (*take)(const CliSettingsSyncHomeCfg *args);
} CliSettingsSyncHomeNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncHomeNs, differences, take);
EMBED_TABLE_STORAGE CliSettingsSyncHomeNs cli_settings_sync_home EMBED_UNUSED = {
    .differences = cli_settings_sync_differences,
    .take = cli_settings_sync_alloc,
};

typedef struct
{
    int (*undo)(const CliSettingsSyncHomeOutCfg *args);
} CliSettingsSyncHomeOutNs;
EMBED_TABLE_LAYOUT(CliSettingsSyncHomeOutNs, undo);
EMBED_TABLE_STORAGE CliSettingsSyncHomeOutNs cli_settings_sync_home_out EMBED_UNUSED = {
    .undo = cli_settings_sync_undo,
};

#endif
