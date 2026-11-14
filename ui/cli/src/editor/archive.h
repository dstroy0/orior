// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// archive.h: archives as folders: a zip, and a jar, a wheel or any other zip by another name, a tar, a gzipped tar and
// a gzipped file each open in the tree as a folder, their files read without unpacking them. A path of the tree runs on
// into an archive past its file, as `lib/a.zip/docs/read.me`, and on into an archive inside it the same way (archive.c,
// archive_deflate.c, archive_pack.c, archive_zip.c, archive_tar.c, archive_path.c, archive_write.c).
//
// A file changed in an archive is saved back into it: the archive is written anew beside the old one, each other
// file's bytes as they were, the changed one deflated, and put in the old one's place.
//
// A zip's names are UTF-8 where its flag specifies so, and otherwise the old DOS page, 437; each name's own bytes go
// with it, for the window to read in an encoding the reader chooses. Deflate, CRC-32, zip, tar and gzip are read and
// written here, with nothing from outside.
//
// A query run's .qry opens as a folder of its blobs, each name's blobs joined in the order passed into one file, a
// name with slashes in folders of its own, as a run names a file it read by its path. A run's .qry is the record of
// what the run did and is never written back
#ifndef ARCHIVE_H
#define ARCHIVE_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliArchiveSizeCfg CliArchiveSizeCfg;
typedef struct CliArchiveBytesCfg CliArchiveBytesCfg;
typedef struct CliArchiveErrMsgCfg CliArchiveErrMsgCfg;
typedef struct CliArchiveSizeDataCfg CliArchiveSizeDataCfg;
typedef struct CliArchiveErrMsgRootCfg CliArchiveErrMsgRootCfg;
typedef struct CliArchiveRootCfg CliArchiveRootCfg;
typedef struct CliArchiveBytesSizeCfg CliArchiveBytesSizeCfg;
typedef struct CliArchiveCfg CliArchiveCfg;

// the types of archive, and none
enum
{
    CLI_ARCHIVE_ZIP,
    CLI_ARCHIVE_TAR,
    CLI_ARCHIVE_TARGZ,
    CLI_ARCHIVE_GZ,
    CLI_ARCHIVE_QRY,
    CLI_ARCHIVE_NONE
};

// the max bytes a capacity of the archives a path runs through is given; the max members of one archive; the max
// bytes of their names; and the max children a folder within one lists
#define CLI_ARCHIVE_MAX (1ull << 28u)

#define CLI_ARCHIVE_MEMBERS 16384u

#define CLI_ARCHIVE_NAMES (1u << 21u)

#define CLI_ARCHIVE_CHILDREN 16384u

// A file or folder of an archive: its path in the archive, with slashes, no leading slash; its name's own bytes, in
// hex, where they are not UTF-8 by the archive's word, `raw` from `raw_at`; whether it is a folder; its size; and where
// its record starts: a zip's central record, a tar's header, or its place among a .qry's joined blobs
typedef struct
{
    unsigned int name;
    unsigned int raw;
    int has_raw;
    int dir;
    unsigned long long size;
    unsigned long long at;
} CliMember;

typedef struct
{
    CliMember item[CLI_ARCHIVE_MEMBERS];
    unsigned int count;
    char text[CLI_ARCHIVE_NAMES];
    unsigned long long used;
} CliMembers;

// What an archive, or a folder within one, contains: each child's name, its path within the archive, whether it is a
// folder or an archive, and its name's own bytes, in hex, where they are not UTF-8
typedef struct
{
    unsigned int name;
    unsigned int path;
    int dir;
    int archive;
    unsigned int raw;
    int has_raw;
} CliChild;

typedef struct
{
    CliChild item[CLI_ARCHIVE_CHILDREN];
    unsigned int count;
    char text[CLI_ARCHIVE_NAMES];
    unsigned long long used;
} CliChildren;

// the type of archive a file is by its name, CLI_ARCHIVE_NONE for none
unsigned int cli_archive_type_get(const CliArchiveCfg *args);
// the CRC-32 of `size` bytes; the bytes deflated `data` contains into `out`, sized for `capacity`, their count into
// `out_size` and the bytes of `data` it took into `used`: 1, or 0 with why given; `size` bytes of `data` deflated, in
// one block of the fixed codes, each run seen in the 32 KiB before it sent as a length and a distance back, into `out`,
// sized for `capacity`: their count, or 0 where they do not fit (archive_deflate.c)
unsigned int cli_crc32(const CliArchiveBytesSizeCfg *args);
int cli_inflate(const CliArchiveSizeCfg *args);
unsigned long long cli_deflate(const CliArchiveSizeCfg *args);
// what a gzip contains, unzipped, into `out`, sized for `capacity`, its count into `out_size`: 1, or 0 with why given;
// and `size` bytes of `data` gzipped into `out`: their count, or 0 where they do not fit (archive_tar.c)
int cli_gunzip(const CliArchiveBytesCfg *args);
unsigned long long cli_gzip(const CliArchiveSizeCfg *args);
// `size` bytes read in the old DOS page, 437, written as UTF-8 after `out` (archive_zip.c)
void cli_dos(const CliArchiveBytesSizeCfg *args);
// where a path of the tree runs into an archive: the archive's file into `file`, sized for CLI_PATH_MAX_LEN, and the
// path's parts past it, joined by `/`, into `inner`, sized for CLI_PATH_MAX_LEN: 1, or 0 where the path runs into no
// archive; the children of the folder of the tree `path` that is an archive or runs into one: 1, or 0 with why given;
// the bytes of the file of the tree `path` within an archive, in a cell of their size and a 0 after counted to `owner`,
// their size into `size`: the cell, given back by the caller, or NULL with why given; and `size` bytes of `data` saved
// as that file, the archive written anew beside the old one and then put in its place: 1, or 0 with why given
// (archive_path.c)
int cli_archive_split(const CliArchiveRootCfg *args);
int cli_archive_list(const CliArchiveErrMsgRootCfg *args);
char *cli_archive_alloc(const CliArchiveErrMsgCfg *args);
int cli_archive_write(const CliArchiveSizeDataCfg *args);

struct CliArchiveSizeCfg
{
    unsigned long long size;
    const unsigned char *data;
    unsigned char *out;
    unsigned long long capacity;
    unsigned long long *out_size;
    unsigned long long *used;
    VerbaLinea *err_msg;
};

struct CliArchiveBytesCfg
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned char *out;
    unsigned long long capacity;
    unsigned long long *out_size;
    VerbaLinea *err_msg;
};

struct CliArchiveErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *root;
    const char *path;
    unsigned long long *size_pointer;
    const CellOwner *owner;
};

struct CliArchiveSizeDataCfg
{
    unsigned long long size;
    const unsigned char *data;
    VerbaLinea *err_msg;
    const char *root;
    const char *path;
};

struct CliArchiveErrMsgRootCfg
{
    VerbaLinea *err_msg;
    const char *root;
    const char *path;
    CliChildren *found;
};

struct CliArchiveRootCfg
{
    const char *root;
    const char *path;
    char *file;
    char *inner;
};

struct CliArchiveBytesSizeCfg
{
    const unsigned char *bytes;
    unsigned long long size;
    VerbaLinea *out_verba_linea_pointer;
};

struct CliArchiveCfg
{
    const char *name;
};

typedef struct
{
    unsigned int (*type_get)(const CliArchiveCfg *args);
} CliArchiveNs;
EMBED_TABLE_LAYOUT(CliArchiveNs, type_get);
EMBED_TABLE_STORAGE CliArchiveNs cli_archive EMBED_UNUSED = {
    .type_get = cli_archive_type_get,
};

typedef struct
{
    unsigned int (*crc32)(const CliArchiveBytesSizeCfg *args);
    void (*dos)(const CliArchiveBytesSizeCfg *args);
} CliArchiveBytesSizeNs;
EMBED_TABLE_LAYOUT(CliArchiveBytesSizeNs, crc32, dos);
EMBED_TABLE_STORAGE CliArchiveBytesSizeNs cli_archive_bytes_size EMBED_UNUSED = {
    .crc32 = cli_crc32,
    .dos = cli_dos,
};

typedef struct
{
    int (*inflate)(const CliArchiveSizeCfg *args);
    unsigned long long (*deflate)(const CliArchiveSizeCfg *args);
    unsigned long long (*gzip)(const CliArchiveSizeCfg *args);
} CliArchiveSizeNs;
EMBED_TABLE_LAYOUT(CliArchiveSizeNs, inflate, deflate, gzip);
EMBED_TABLE_STORAGE CliArchiveSizeNs cli_archive_size EMBED_UNUSED = {
    .inflate = cli_inflate,
    .deflate = cli_deflate,
    .gzip = cli_gzip,
};

typedef struct
{
    int (*gunzip)(const CliArchiveBytesCfg *args);
} CliArchiveBytesNs;
EMBED_TABLE_LAYOUT(CliArchiveBytesNs, gunzip);
EMBED_TABLE_STORAGE CliArchiveBytesNs cli_archive_bytes EMBED_UNUSED = {
    .gunzip = cli_gunzip,
};

typedef struct
{
    int (*split)(const CliArchiveRootCfg *args);
} CliArchiveRootNs;
EMBED_TABLE_LAYOUT(CliArchiveRootNs, split);
EMBED_TABLE_STORAGE CliArchiveRootNs cli_archive_root EMBED_UNUSED = {
    .split = cli_archive_split,
};

typedef struct
{
    int (*list)(const CliArchiveErrMsgRootCfg *args);
} CliArchiveErrMsgRootNs;
EMBED_TABLE_LAYOUT(CliArchiveErrMsgRootNs, list);
EMBED_TABLE_STORAGE CliArchiveErrMsgRootNs cli_archive_err_msg_root EMBED_UNUSED = {
    .list = cli_archive_list,
};

typedef struct
{
    char *(*take)(const CliArchiveErrMsgCfg *args);
} CliArchiveErrMsgNs;
EMBED_TABLE_LAYOUT(CliArchiveErrMsgNs, take);
EMBED_TABLE_STORAGE CliArchiveErrMsgNs cli_archive_err_msg EMBED_UNUSED = {
    .take = cli_archive_alloc,
};

typedef struct
{
    int (*write)(const CliArchiveSizeDataCfg *args);
} CliArchiveSizeDataNs;
EMBED_TABLE_LAYOUT(CliArchiveSizeDataNs, write);
EMBED_TABLE_STORAGE CliArchiveSizeDataNs cli_archive_size_data EMBED_UNUSED = {
    .write = cli_archive_write,
};

#endif
