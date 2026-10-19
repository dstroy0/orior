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
// A zip's names are UTF-8 where its flag says so, and otherwise the old DOS page, 437; each name's own bytes go with
// it, for the window to read in an encoding the reader chooses. Deflate, CRC-32, zip, tar and gzip are read and
// written here, with nothing from outside.
//
// A query run's .qry opens as a folder of its blobs, each name's blobs joined in the order handed into one file, a
// name with slashes in folders of its own, as a run names a file it read by its path. A run's .qry is the record of
// what the run did and is never written back
#ifndef ARCHIVE_H
#define ARCHIVE_H

#include "../command_line/cli_internal.h"

// the kinds of archive, and none
enum
{
    CLI_ARCHIVE_ZIP,
    CLI_ARCHIVE_TAR,
    CLI_ARCHIVE_TARGZ,
    CLI_ARCHIVE_GZ,
    CLI_ARCHIVE_QRY,
    CLI_ARCHIVE_NONE
};

// the most bytes an archive's file, unpacked, holds here; the most members of one archive; the most bytes of their
// names; and the most children a folder within one lists
#define CLI_ARCHIVE_BYTES (1u << 24u)

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

// What an archive, or a folder within one, holds: each child's name, its path within the archive, whether it is a
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

// the kind of archive a file is by its name, CLI_ARCHIVE_NONE for none
unsigned int cli_archive_kind_of(const char *name);
// the CRC-32 of `size` bytes; the bytes deflated `data` holds into `out`, which holds `room`, their count into
// `out_size` and the bytes of `data` it took into `used`: 1, or 0 with why said; `size` bytes of `data` deflated, in
// one block of the fixed codes, each run seen in the 32 KiB before it sent as a length and a distance back, into `out`,
// which holds `room`: their count, or 0 where they do not fit (archive_deflate.c)
unsigned int cli_crc32(const unsigned char *bytes, unsigned long long size);
int cli_inflate(const unsigned char *data, unsigned long long size, unsigned char *out, unsigned long long room,
                unsigned long long *out_size, unsigned long long *used, CliLine *said);
unsigned long long cli_deflate(const unsigned char *data, unsigned long long size, unsigned char *out,
                               unsigned long long room);
// what a gzip holds, unzipped, into `out`, which holds `room`, its count into `out_size`: 1, or 0 with why said; and
// `size` bytes of `data` gzipped into `out`: their count, or 0 where they do not fit (archive_tar.c)
int cli_gunzip(const unsigned char *bytes, unsigned long long size, unsigned char *out, unsigned long long room,
               unsigned long long *out_size, CliLine *said);
unsigned long long cli_gzip(const unsigned char *data, unsigned long long size, unsigned char *out,
                            unsigned long long room);
// `size` bytes read in the old DOS page, 437, written as UTF-8 after `out` (archive_zip.c)
void cli_dos(const unsigned char *bytes, unsigned long long size, CliLine *out);
// where a path of the tree runs into an archive: the archive's file into `file`, which holds CLI_PATH_LONGEST, and the
// path's parts past it, joined by `/`, into `inner`, which holds CLI_PATH_LONGEST: 1, or 0 where the path runs into no
// archive; the children of the folder of the tree `path` that is an archive or runs into one; the bytes of the file of
// the tree `path` within an archive, into `out`; and `size` bytes of `data` saved as that file, the archive written
// anew beside the old one and then put in its place. Each 1, or 0 with why said (archive_path.c)
int cli_archive_split(const char *root, const char *path, char *file, char *inner);
int cli_archive_list(const char *root, const char *path, CliChildren *found, CliLine *said);
int cli_archive_read(const char *root, const char *path, CliLine *out, CliLine *said);
int cli_archive_write(const char *root, const char *path, const unsigned char *data, unsigned long long size,
                      CliLine *said);

#endif
