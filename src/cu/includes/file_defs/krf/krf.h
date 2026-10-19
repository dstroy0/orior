// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krf.h: a part's removals, its .krf (krf.oracle.tsv), a k-file people read and write (kfile_lines.h): a row for every row a
// k-file of the part let go without putting it back, `removed <type> <query> <reason> <row>`, the k-file's type the
// row left, the run that let it go, why, and the row's own text whole. A k-file grows, and what leaves one is kept
// here: a row of another text put under its question or its register replaces it, and a form the part refuses moves
// here. Each program that keeps a removal puts its rows after a who line of the code that put them.
#ifndef KRF_H
#define KRF_H

#include "../kfile_lines/kfile_lines.h"

#include "../../parsers/who.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the most letters of a type, of a run's name, of a reason and of a row
#define KRF_TYPE 16u
#define KRF_QUERY 64u
#define KRF_REASON 32u
#define KRF_ROW 1024u

    // A .krf's lines (kfile_lines.h). A file starts as zeros
    typedef KfileLines KrfFile;

    // one removal: the k-file's type the row left, the run that let it go, why, and the row as the k-file held it
    typedef struct
    {
        char type[KRF_TYPE];
        char query[KRF_QUERY];
        char reason[KRF_REASON];
        char row[KRF_ROW];
    } KrfRow;

    // the .krf at `path` read into `file` as the run holds it (kfile_lines_read): 1, and none of its lines where there is no
    // such file; 0 where there was no memory for a line
    int krf_read(KrfFile *file, const char *path);

    // `file` handed to the run's writer as the file at `path`, written by `who` (kfile_lines_write_by): 1, or 0 where it
    // could not be written
    int krf_write_by(const KrfFile *file, const char *path, Who who);

    // every line `file` holds let go, and the file zeros again
    void krf_free(KrfFile *file);

    // `text`, a line without its end, kept after every line `file` holds: 1, or 0 where there was no memory for it
    int krf_line_add(KrfFile *file, const char *text);

    // 1 with the removal `line` is in `row` where it is one, `removed <type> <query> <reason> <row>`; 0 otherwise
    int krf_row_read(const char *line, KrfRow *row);

    // the run's name, query<n>-<datetime> as the writer names its .qry, into `name` of `room` bytes: 1, or 0 with `-`
    // there where no run's writer runs
    int krf_query_named(char *name, size_t room);

    // the .krf beside the k-file at `path`, the same path with its last extension .krf, into `krf` of `room` bytes
    void krf_path_beside(const char *path, char *krf, size_t room);

    // `rows`, `count` of them, each let go from a k-file of type `type` for `reason` by this run, kept in the .krf at
    // `path` as the run holds it: the file started with `krf <part>` where there is none, the part the path's own
    // name; the rows put after its last line under a who line of `who` (kfile_lines_put_by); and the file handed to
    // the run's writer (kfile_lines_write_by). 1, or 0 where it could not be written
    int krf_removed_by(const char *path, const char *type, const char *reason, const char *const *rows,
                       unsigned int count, Who who);

// each, the who the code that calls it
#define krf_write(file_, path_) krf_write_by((file_), (path_), WHO_HERE)
#define krf_removed(path_, type_, reason_, rows_, count_)                                                              \
    krf_removed_by((path_), (type_), (reason_), (rows_), (count_), WHO_HERE)

#ifdef __cplusplus
}
#endif

#endif
