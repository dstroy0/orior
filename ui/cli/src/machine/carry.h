// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry.h: the reader's settings carried to another machine: settings.json, without its part for this machine alone and
// the settings that name this machine's own; the themes; the reader's plugins, and each plugin's own settings; the
// menus and toolbar, menus.json; the stylesheet, user.css; and the reader's language files. They go as one file, a JSON
// object of each file's text by its place in orior's own folder, or into a folder of the reader's laid out as orior's
// own folder lays them, which, where it is a repository, is committed and pushed, and pulled before it is read.
//
// Taking them in writes nothing until the reader has seen the differences and chosen what to take: each file chosen,
// and in settings.json each setting chosen, the settings of this machine alone kept as they are. Each file a taking
// writes over is kept as it was in `carried/`, a folder for each taking, and undoing the taking writes them back and
// takes out the files it made (carry.c, carry_send.c, carry_take.c, carry_undo.c)
#ifndef CARRY_H
#define CARRY_H

#include "../command_line/cli_internal.h"

// the most files carried, and the most bytes of one's text
#define CLI_CARRIED_MOST 4096u
#define CLI_CARRIED_TEXT (1u << 25u)

// Files carried, by place in order: each place and its text, or its path, each a cell of its own ended by a 0, given
// back as it is put again and as the files are cleared
typedef struct
{
    char *place[CLI_CARRIED_MOST];
    char *value[CLI_CARRIED_MOST];
    unsigned long long size[CLI_CARRIED_MOST];
    unsigned int count;
} CliCarried;

// what travels from orior's own folder `home` into `packed`: each file's text by its place, settings.json without the
// settings of this machine alone: 1, or 0 with why said; and what travels from `home` written to `to`, as one file
// where `to` is not a folder, or laid out in the folder `to`, the files it no longer carries taken out of it,
// committed and pushed where it is a repository: 1 with what it did said into `out`, or 0 with why said
int cli_carry_pack(const char *home, CliCarried *packed, CliLine *said);
int cli_carry_send(const char *home, const char *to, CliLine *out, CliLine *said);

// what the file or the folder `from` carries into `there`, by place, a repository with a remote pulled first: 1, or 0
// with why said (carry.c)
int cli_carry_read(const char *from, CliCarried *there, CliLine *said);

// how what `there` carries differs from orior's own folder `home`, the files alike left out, written as
// `[{"place", "how", "settings": [{"key", "here", "there"}]}]`; what `there`, read from `from`, carries taken in:
// each of `places`, `count` of them, and of settings.json the settings of `keys`, `key_count` of them, alone, as there,
// a setting there does not set taken out here, and the settings of this machine alone kept, each file it writes over
// kept first in `carried/`, the taking written as `{"id", "at", "from", "files": [[place, stood]]}`: 1, or 0 with why
// said; every taking kept, the last first, written as a JSON array of them; and the taking `id` undone, each file it
// wrote over written back as it was, each it made taken out, and the taking forgotten, the taking written: 1, or 0
// with why said (carry_take.c)
void cli_carry_differences(const char *home, const CliCarried *there, CliLine *out);
int cli_carry_take(const char *home, const char *from, const CliCarried *there, const char *const *places,
                   unsigned int count, const char *const *keys, unsigned int key_count, CliLine *out, CliLine *said);
void cli_carry_takings(const char *home, CliLine *out);
int cli_carry_undo(const char *home, const char *id, CliLine *out, CliLine *said);

#endif
