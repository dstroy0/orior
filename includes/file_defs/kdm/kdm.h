// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdm.h: the .kdm of a part (kdm.oracle.tsv), a k-file that people read and write (kfile_lines.h):
//   - its first line, `kdm <part>`.
//   - its own section: the comments, the head of its rows, and the rows. Each row is an arrangement of primitives that
//     produces an operator, and what it costs the part. Its fields are apart by tabs.
//   - after its last row, its tail. That is the registers, widths, loads and stores of the part in a block of the
//     language of the ruleset, `__krs__(` to `)__krs__` (klang.h). The ruleset reader reads it. Then its pipes, and
//     their comments.
//
// A program that writes the rows writes the whole section, after a who line of its own. The tail stays as the file
// stored it, and the code writes again before it the who that was in force over it.
// A program that writes the pipes writes them at the end of the tail, after a who line of its own. The pipes before
// them go.
#ifndef KDM_H
#define KDM_H

#include "../../parsers/who.h"
#include "../kfile_lines/kfile_lines.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the head of the rows, the last line of the section before them
#define KDM_ROWS_HEAD "# operator\tnodes\tchain\tcost\truns"

// the max characters of a row's operator, of its chain and of its cost
#define KDM_OPERATOR 64u
#define KDM_CHAIN 512u
#define KDM_COST 128u

    // A .kdm's lines (kfile_lines.h). A file starts as zeros
    typedef KfileLines KdmFile;

    // one row: the operator, the count of nodes, the chain, its cost as the row writes it, and the runs that its time comes from.
    // Where a descent wrote a sixth field, its verdict, 1 in place and 0 out. -1 where the row has no sixth
    typedef struct
    {
        char operation[KDM_OPERATOR];
        unsigned int nodes;
        char chain[KDM_CHAIN];
        char cost[KDM_COST];
        unsigned int runs;
        int verdict;
    } KdmRow;

    // the .kdm at `path`, read into `file` as the run stores it (kfile_lines_read). The result is 1, with no lines where
    // there is no such file. It is 0 where there was no memory for a line
    int kdm_read(KdmFile *file, const char *path);

    // `file` passed to the run's writer as the file at `path`, written by `who` (kfile_lines_write_by). The result is 1,
    // or 0 where the write failed
    int kdm_write_by(const KdmFile *file, const char *path, Who who);

    // every line `file` contains let go, and the file zeros again
    void kdm_free(KdmFile *file);

    // 1 with the row `line` is in `row`, five fields apart by tabs or six with a descent's verdict, and the operator no
    // comment; 0 otherwise
    int kdm_row_read(const char *line, KdmRow *row);

    // the line where the tail of the file starts: the line after its last row. Where it has no rows, it is the line
    // after the head of its rows. Where it has no head, it is its count of lines
    unsigned int kdm_tail_at(const KdmFile *file);

    // The file's section, every line from its second to its tail, replaced by `lines`, `count` of them, written by
    // `who`: its first line `kdm <part>`, `part` where the file had none. Then a who line of `who`, the lines, and
    // before the tail the who that was in force over it, `who -` where none was. 1, or 0 where there was no memory for
    // them, the file as it was
    int kdm_section_written_by(KdmFile *file, const char *part, const char *const *lines, unsigned int count, Who who);

    // Each line of the tail that is `pipe ...` or one of `comment`, `comments` of them, let go. The blank lines at the
    // end of the tail then go. Where there are `pipes`, `count` of them, `who` puts them at the end after a blank line
    // and the comment. The result is 1, or 0 where there was no memory for them, and the file stays as it was
    int kdm_pipes_replaced_by(KdmFile *file, const char *const *comment, unsigned int comments,
                              const char *const *pipes, unsigned int count, Who who);

// each, the who the code that calls it
#define kdm_write(file_, path_) kdm_write_by((file_), (path_), WHO_HERE)
#define kdm_section_written(file_, part_, lines_, count_)                                                              \
    kdm_section_written_by((file_), (part_), (lines_), (count_), WHO_HERE)
#define kdm_pipes_replaced(file_, comment_, comments_, pipes_, count_)                                                 \
    kdm_pipes_replaced_by((file_), (comment_), (comments_), (pipes_), (count_), WHO_HERE)

#ifdef __cplusplus
}
#endif

#endif
