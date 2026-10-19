// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_internal.h: what the archive sources share: an archive's members read, a member's bytes, and an archive
// written anew with one member changed
#ifndef ARCHIVE_INTERNAL_H
#define ARCHIVE_INTERNAL_H

#include "archive.h"

// the base length and the extra bits of each length code, and the base distance and the extra bits of each distance
// code, as deflate gives them (archive_deflate.c)
extern const unsigned int g_cli_length_base[29];
extern const unsigned int g_cli_length_extra[29];
extern const unsigned int g_cli_distance_base[30];
extern const unsigned int g_cli_distance_extra[30];

// a little-endian number of `width` bytes at `at`: 1 with it into `value`, or 0 with why said where the bytes end
// first; a member's name cleaned of a leading `./` or `/` and a trailing `/`, put in the members' texts; and a member
// put after the members (archive.c)
int cli_archive_number(const unsigned char *bytes, unsigned long long size, unsigned long long at, unsigned int width,
                       unsigned long long *value, CliLine *said);
unsigned int cli_archive_name_kept(CliMembers *members, const char *name, unsigned long long length);
void cli_archive_member_put(CliMembers *members, unsigned int name, unsigned int raw, int has_raw, int dir,
                            unsigned long long size, unsigned long long at);

// a zip's members; a zip member's bytes into `out`, which holds `room`, their count into `out_size`; and the zip with
// the member at `target` set to `size` bytes of `data`, deflated where that is smaller, into `out`, which holds `room`,
// its count into `out_size`. Each 1, or 0 with why said (archive_zip.c)
int cli_zip_members(const unsigned char *bytes, unsigned long long size, CliMembers *members, CliLine *said);
int cli_zip_data(const unsigned char *bytes, unsigned long long size, const CliMembers *members,
                 const CliMember *member, unsigned char *out, unsigned long long room, unsigned long long *out_size,
                 CliLine *said);
int cli_zip_with(const unsigned char *bytes, unsigned long long size, const CliMember *target,
                 const unsigned char *data, unsigned long long data_size, unsigned char *out, unsigned long long room,
                 unsigned long long *out_size, CliLine *said);

// a tar's members; and the tar with the member at `target` set to `size` bytes of `data`, into `out`, which holds
// `room`, its count into `out_size`. Each 1, or 0 with why said (archive_tar.c)
int cli_tar_members(const unsigned char *bytes, unsigned long long size, CliMembers *members, CliLine *said);
int cli_tar_with(const unsigned char *bytes, unsigned long long size, const CliMember *target,
                 const unsigned char *data, unsigned long long data_size, unsigned char *out, unsigned long long room,
                 unsigned long long *out_size, CliLine *said);

// The members of an archive of `kind` whose bytes are `bytes`, read: what a gzipped archive holds, unzipped, into
// `unpacked`, its count into `unpacked_size`; and a member's bytes, by the archive's kind, into `out`. Each 1, or 0
// with why said (archive.c)
int cli_members_of(const unsigned char *bytes, unsigned long long size, unsigned int kind, const char *inner,
                   CliMembers *members, unsigned char *unpacked, unsigned long long *unpacked_size, CliLine *said);
int cli_data_of(const unsigned char *bytes, unsigned long long size, unsigned int kind, const unsigned char *unpacked,
                unsigned long long unpacked_size, const CliMembers *members, const CliMember *member,
                unsigned char *out, unsigned long long room, unsigned long long *out_size, CliLine *said);
// the name a gzipped file's one member goes by, the file's own less its .gz, into `out`, which holds CLI_NAME_LONGEST
void cli_gz_inner(const char *name, char *out);

// the most archives deep a file is saved within
#define CLI_ARCHIVE_DEPTH 3u

#define CLI_ARCHIVE_PARTS 64u

// The archives a path runs through, a cell taken with the lock and given back with it: each level's members, what it
// holds unpacked, the bytes of the archive within it, and the level written anew; and a tar written before it is
// gzipped
typedef struct
{
    CliMembers members[CLI_ARCHIVE_DEPTH];
    unsigned char unpacked[CLI_ARCHIVE_DEPTH][CLI_ARCHIVE_BYTES];
    unsigned char inner[CLI_ARCHIVE_DEPTH - 1u][CLI_ARCHIVE_BYTES];
    unsigned char written[CLI_ARCHIVE_DEPTH][CLI_ARCHIVE_BYTES];
    unsigned char tar[CLI_ARCHIVE_BYTES];
} CliLevels;

extern CliLevels *g_cli_archive_levels;
extern CliLock g_cli_archive_lock;

// the lock taken and the levels with it: 1, or 0 with why said and the lock given back where the cells cannot meet
// them; and the levels given back with the lock (archive_path.c)
int cli_archive_held(CliLine *said);
void cli_archive_released(void);

// A path's parts within an archive
typedef struct
{
    char part[CLI_ARCHIVE_PARTS][CLI_NAME_LONGEST];
    unsigned int count;
} CliParts;

// Where in an archive a path leads: the bytes of the innermost archive it runs into, that archive's kind and name, and
// the parts of the path left within it, from `left`
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned int kind;
    char name[CLI_PATH_LONGEST];
    unsigned int left;
} CliDescent;

// a path's parts, its slashes between them and its empty ones left out, put into `parts`; parts `from` to `to`
// joined by `/` into `out`, which holds CLI_PATH_LONGEST; the file member named `name` of the members, or NULL where
// none is; the path's parts followed from the archive at `descent` on into each archive inside it they name, the
// lock held, each archive's members read into the first level's: 1, or 0 with why said; and the archive a path runs
// into opened, its file mapped, its kind and name, and the path's parts within it, `missing` saying how a path that
// runs into no archive is told: 1, or 0 with why said (archive_path.c)
void cli_archive_parts_of(const char *path, CliParts *parts);
void cli_archive_joined(const CliParts *parts, unsigned int from, unsigned int to, char *out);
const CliMember *cli_archive_file_named(const CliMembers *members, const char *name);
int cli_archive_descend(CliDescent *descent, const CliParts *parts, CliLine *said);
int cli_archive_opened(const char *root, const char *path, const char *missing, CliDescent *descent, CliParts *parts,
                       char *file, CliLine *said);

#endif
