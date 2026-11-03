// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive_internal.h: what the archive sources share: an archive's members read, a member's bytes, and an archive
// written anew with one member changed
#ifndef ARCHIVE_INTERNAL_H
#define ARCHIVE_INTERNAL_H
#include "archive.h"
#include "embed_dispatch_layout.h"

typedef struct CliArchiveBytesCtx CliArchiveBytesCtx;
typedef struct CliArchiveBytesSizeCtx CliArchiveBytesSizeCtx;
typedef struct CliArchiveBytesSizeErrMsgCtx CliArchiveBytesSizeErrMsgCtx;
typedef struct CliArchiveBytesSizeErrMsgMembersCtx CliArchiveBytesSizeErrMsgMembersCtx;
typedef struct CliArchiveSizeCtx CliArchiveSizeCtx;
typedef struct CliArchiveErrMsgCtx CliArchiveErrMsgCtx;
typedef struct CliArchiveBytesSizeAtCtx CliArchiveBytesSizeAtCtx;
typedef struct CliArchiveOutCtx CliArchiveOutCtx;
typedef struct CliArchiveErrMsgBufferCtx CliArchiveErrMsgBufferCtx;
typedef struct CliArchiveErrMsgPartsCtx CliArchiveErrMsgPartsCtx;
typedef struct CliArchiveMembersCtx CliArchiveMembersCtx;
typedef struct CliArchiveCtx CliArchiveCtx;
typedef struct CliArchiveNameOutCtx CliArchiveNameOutCtx;

// the base length and the extra bits of each length code, and the base distance and the extra bits of each distance
// code, as deflate gives them (archive_deflate.c)
extern const unsigned int g_cli_length_base[29];
extern const unsigned int g_cli_length_extra[29];
extern const unsigned int g_cli_distance_base[30];
extern const unsigned int g_cli_distance_extra[30];

// a little-endian number of `width` bytes at `at`: 1 with it into `value`, or 0 with why given where the bytes end
// first; a member's name cleaned of a leading `./` or `/` and a trailing `/`, put in the members' texts; and a member
// put after the members (archive.c)
int cli_archive_number(const CliArchiveBytesSizeAtCtx *args);
unsigned int cli_archive_name_store(const CliArchiveMembersCtx *args);
void cli_archive_member_append(const CliArchiveSizeCtx *args);

// a zip's members; a zip member's bytes into `out`, sized for `capacity`, their count into `out_size`; and the zip with
// the member at `target` set to `size` bytes of `data`, deflated where that is smaller, into `out`, sized for
// `capacity`, its count into `out_size`. Each 1, or 0 with why given (archive_zip.c)
int cli_zip_members(const CliArchiveBytesSizeErrMsgCtx *args);
int cli_zip_data(const CliArchiveBytesSizeErrMsgMembersCtx *args);
int cli_zip_with(const CliArchiveBytesSizeCtx *args);

// a tar's members; and the tar with the member at `target` set to `size` bytes of `data`, into `out`, sized for
// `capacity`, its count into `out_size`. Each 1, or 0 with why given (archive_tar.c)
int cli_tar_members(const CliArchiveBytesSizeErrMsgCtx *args);
int cli_tar_with(const CliArchiveBytesSizeCtx *args);

// A capacity of the archives a path runs through: a cell of `capacity` bytes, or none
typedef struct
{
    unsigned char *bytes;
    unsigned long long capacity;
} CliByteBuffer;

// `capacity` given `need` bytes for `what`: as it stands where it fits them, else in a cell of that many taken in the
// old one's place, what the old one occupied let go; 1, or 0 with why given where the cells cannot meet it or `need` is
// past CLI_ARCHIVE_MAX; and the bytes an archive written anew from `size` bytes may take, an eighth and a page over
// (archive_path.c)
int cli_archive_buffer_reserve(const CliArchiveErrMsgBufferCtx *args);
unsigned long long cli_archive_grow(const CliArchiveBytesSizeAtCtx *args);

// The members of an archive of `kind` whose bytes are `bytes`, read: what a gzipped archive contains, unzipped, into
// `unpacked`, given the capacity its trailer specifies, its count into `unpacked_size`; and a member's bytes, by the
// archive's kind, into `out`, given the capacity the member takes. Each 1, or 0 with why given (archive.c)
int cli_members_get(const CliArchiveBytesSizeErrMsgCtx *args);
int cli_data_get(const CliArchiveBytesCtx *args);
// the name a gzipped file's one member goes by, the file's own less its .gz, into `out`, sized for CLI_NAME_MAX_LEN
void cli_gz_inner(const CliArchiveNameOutCtx *args);

// the most archives deep a file is saved within
#define CLI_ARCHIVE_DEPTH 3u

#define CLI_ARCHIVE_PARTS 64u

// The archives a path runs through, a cell taken with the lock and given back with it: each level's members, what it
// contains unpacked, the bytes of the archive within it, and the level written anew; and a tar written before it is
// gzipped. Each capacity is a cell of its own, taken sized to what it contains as it is first written and given back
// with the levels
typedef struct
{
    CliMembers members[CLI_ARCHIVE_DEPTH];
    CliByteBuffer unpacked[CLI_ARCHIVE_DEPTH];
    CliByteBuffer inner[CLI_ARCHIVE_DEPTH - 1u];
    CliByteBuffer written[CLI_ARCHIVE_DEPTH];
    CliByteBuffer tar;
} CliLevels;

extern CliLevels *g_cli_archive_levels;

// the lock taken and the levels with it: 1, or 0 with why given and the lock given back where the cells cannot meet
// them; and the levels given back with the lock (archive_path.c)
int cli_archive_lock(const CliArchiveErrMsgPartsCtx *args);
void cli_archive_release(void);

// A path's parts within an archive
typedef struct
{
    char part[CLI_ARCHIVE_PARTS][CLI_NAME_MAX_LEN];
    unsigned int count;
} CliParts;

// Where in an archive a path leads: the bytes of the innermost archive it runs into, that archive's kind and name, and
// the parts of the path left within it, from `left`
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned int kind;
    char name[CLI_PATH_MAX_LEN];
    unsigned int left;
} CliDescent;

// a path's parts, its slashes between them and its empty ones left out, put into `parts`; parts `from` to `to`
// joined by `/` into `out`, sized for CLI_PATH_MAX_LEN; the file member named `name` of the members, or NULL where
// none is; the path's parts followed from the archive at `descent` on into each archive inside it they name, the
// lock acquired, each archive's members read into the first level's: 1, or 0 with why given; and the archive a path
// runs into opened, its file mapped, its kind and name, and the path's parts within it, `missing` specifying how a path
// that runs into no archive is reported: 1, or 0 with why given (archive_path.c)
void cli_archive_parts_get(const CliArchiveErrMsgCtx *args);
void cli_archive_join(const CliArchiveOutCtx *args);
const CliMember *cli_archive_file_by_name(const CliArchiveCtx *args);
int cli_archive_descend(const CliArchiveErrMsgPartsCtx *args);
int cli_archive_open(const CliArchiveErrMsgCtx *args);

struct CliArchiveBytesCtx
{
    const unsigned char *bytes;
    unsigned long long size;
    VerbaLinea *err_msg;
    const CliMembers *members_read;
    const CliMember *member;
    unsigned long long *out_size;
    unsigned int kind;
    const unsigned char *unpacked_bytes;
    unsigned long long unpacked_size_value;
    CliByteBuffer *out_buffer_pointer;
};

struct CliArchiveBytesSizeCtx
{
    const unsigned char *bytes;
    unsigned long long size;
    VerbaLinea *err_msg;
    unsigned char *out;
    unsigned long long capacity;
    unsigned long long *out_size;
    const CliMember *target;
    const unsigned char *data;
    unsigned long long data_size;
};

struct CliArchiveBytesSizeErrMsgCtx
{
    const unsigned char *bytes;
    unsigned long long size;
    VerbaLinea *err_msg;
    CliMembers *members;
    unsigned int kind;
    const char *inner;
    CliByteBuffer *unpacked;
    unsigned long long *unpacked_size;
};

struct CliArchiveBytesSizeErrMsgMembersCtx
{
    const unsigned char *bytes;
    unsigned long long size;
    VerbaLinea *err_msg;
    const CliMembers *members_read;
    const CliMember *member;
    unsigned char *out;
    unsigned long long capacity;
    unsigned long long *out_size;
};

struct CliArchiveSizeCtx
{
    unsigned long long size;
    unsigned long long at;
    CliMembers *members;
    unsigned int name_u32;
    unsigned int raw;
    int has_raw;
    int dir;
};

struct CliArchiveErrMsgCtx
{
    VerbaLinea *err_msg;
    const char *path;
    CliParts *parts;
    CliDescent *descent;
    const char *root;
    const char *missing;
    char *file;
};

struct CliArchiveBytesSizeAtCtx
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long at;
    unsigned int width;
    unsigned long long *value;
    VerbaLinea *err_msg;
};

struct CliArchiveOutCtx
{
    char *out_capacity;
    const CliParts *parts_read;
    unsigned int from;
    unsigned int to;
};

struct CliArchiveErrMsgBufferCtx
{
    VerbaLinea *err_msg;
    CliByteBuffer *buffer_pointer;
    unsigned long long need;
    const char *what;
};

struct CliArchiveErrMsgPartsCtx
{
    VerbaLinea *err_msg;
    const CliParts *parts_read;
    CliDescent *descent;
};

struct CliArchiveMembersCtx
{
    CliMembers *members;
    const char *name;
    unsigned long long length;
};

struct CliArchiveCtx
{
    const char *name;
    const CliMembers *members_read;
};

struct CliArchiveNameOutCtx
{
    const char *name;
    char *out_capacity;
};

typedef struct
{
    int (*number)(const CliArchiveBytesSizeAtCtx *args);
    unsigned long long (*grow)(const CliArchiveBytesSizeAtCtx *args);
} CliArchiveBytesSizeAtNs;
EMBED_TABLE_LAYOUT(CliArchiveBytesSizeAtNs, number, grow);
EMBED_TABLE_STORAGE CliArchiveBytesSizeAtNs cli_archive_bytes_size_at_internal EMBED_UNUSED = {
    .number = cli_archive_number,
    .grow = cli_archive_grow,
};

typedef struct
{
    unsigned int (*name_store)(const CliArchiveMembersCtx *args);
} CliArchiveMembersNs;
EMBED_TABLE_LAYOUT(CliArchiveMembersNs, name_store);
EMBED_TABLE_STORAGE CliArchiveMembersNs cli_archive_members_internal EMBED_UNUSED = {
    .name_store = cli_archive_name_store,
};

typedef struct
{
    void (*member_append)(const CliArchiveSizeCtx *args);
} CliArchiveSizeNsTable;
EMBED_TABLE_LAYOUT(CliArchiveSizeNsTable, member_append);
EMBED_TABLE_STORAGE CliArchiveSizeNsTable cli_archive_size_internal EMBED_UNUSED = {
    .member_append = cli_archive_member_append,
};

typedef struct
{
    int (*zip_members)(const CliArchiveBytesSizeErrMsgCtx *args);
    int (*tar_members)(const CliArchiveBytesSizeErrMsgCtx *args);
    int (*members_get)(const CliArchiveBytesSizeErrMsgCtx *args);
} CliArchiveBytesSizeErrMsgNs;
EMBED_TABLE_LAYOUT(CliArchiveBytesSizeErrMsgNs, zip_members, tar_members, members_get);
EMBED_TABLE_STORAGE CliArchiveBytesSizeErrMsgNs cli_archive_bytes_size_err_msg_internal EMBED_UNUSED = {
    .zip_members = cli_zip_members,
    .tar_members = cli_tar_members,
    .members_get = cli_members_get,
};

typedef struct
{
    int (*zip_data)(const CliArchiveBytesSizeErrMsgMembersCtx *args);
} CliArchiveBytesSizeErrMsgMembersNs;
EMBED_TABLE_LAYOUT(CliArchiveBytesSizeErrMsgMembersNs, zip_data);
EMBED_TABLE_STORAGE CliArchiveBytesSizeErrMsgMembersNs cli_archive_bytes_size_err_msg_members_internal EMBED_UNUSED = {
    .zip_data = cli_zip_data,
};

typedef struct
{
    int (*zip_with)(const CliArchiveBytesSizeCtx *args);
    int (*tar_with)(const CliArchiveBytesSizeCtx *args);
} CliArchiveBytesSizeNsTable;
EMBED_TABLE_LAYOUT(CliArchiveBytesSizeNsTable, zip_with, tar_with);
EMBED_TABLE_STORAGE CliArchiveBytesSizeNsTable cli_archive_bytes_size_internal EMBED_UNUSED = {
    .zip_with = cli_zip_with,
    .tar_with = cli_tar_with,
};

typedef struct
{
    int (*capacity)(const CliArchiveErrMsgBufferCtx *args);
} CliArchiveErrMsgBufferNs;
EMBED_TABLE_LAYOUT(CliArchiveErrMsgBufferNs, capacity);
EMBED_TABLE_STORAGE CliArchiveErrMsgBufferNs cli_archive_err_msg_buffer_internal EMBED_UNUSED = {
    .capacity = cli_archive_buffer_reserve,
};

typedef struct
{
    int (*data_get)(const CliArchiveBytesCtx *args);
} CliArchiveBytesNsTable;
EMBED_TABLE_LAYOUT(CliArchiveBytesNsTable, data_get);
EMBED_TABLE_STORAGE CliArchiveBytesNsTable cli_archive_bytes_internal EMBED_UNUSED = {
    .data_get = cli_data_get,
};

typedef struct
{
    void (*gz_inner)(const CliArchiveNameOutCtx *args);
} CliArchiveNameOutNs;
EMBED_TABLE_LAYOUT(CliArchiveNameOutNs, gz_inner);
EMBED_TABLE_STORAGE CliArchiveNameOutNs cli_archive_name_out_internal EMBED_UNUSED = {
    .gz_inner = cli_gz_inner,
};

typedef struct
{
    int (*lock)(const CliArchiveErrMsgPartsCtx *args);
    int (*descend)(const CliArchiveErrMsgPartsCtx *args);
} CliArchiveErrMsgPartsNs;
EMBED_TABLE_LAYOUT(CliArchiveErrMsgPartsNs, lock, descend);
EMBED_TABLE_STORAGE CliArchiveErrMsgPartsNs cli_archive_err_msg_parts_internal EMBED_UNUSED = {
    .lock = cli_archive_lock,

    .descend = cli_archive_descend,
};

typedef struct
{
    void (*parts_get)(const CliArchiveErrMsgCtx *args);
    int (*open)(const CliArchiveErrMsgCtx *args);
} CliArchiveErrMsgNsTable;
EMBED_TABLE_LAYOUT(CliArchiveErrMsgNsTable, parts_get, open);
EMBED_TABLE_STORAGE CliArchiveErrMsgNsTable cli_archive_err_msg_internal EMBED_UNUSED = {
    .parts_get = cli_archive_parts_get,
    .open = cli_archive_open,
};

typedef struct
{
    void (*join)(const CliArchiveOutCtx *args);
} CliArchiveOutNs;
EMBED_TABLE_LAYOUT(CliArchiveOutNs, join);
EMBED_TABLE_STORAGE CliArchiveOutNs cli_archive_out_internal EMBED_UNUSED = {
    .join = cli_archive_join,
};

typedef struct
{
    const CliMember *(*file_by_name)(const CliArchiveCtx *args);
} CliArchiveNsTable;
EMBED_TABLE_LAYOUT(CliArchiveNsTable, file_by_name);
EMBED_TABLE_STORAGE CliArchiveNsTable cli_archive_internal EMBED_UNUSED = {
    .file_by_name = cli_archive_file_by_name,
};

#endif
