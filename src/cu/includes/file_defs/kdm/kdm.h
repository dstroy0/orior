// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdm.h: a part's .kdm (kdm.oracle.tsv), a k-file people read and write (kfile_lines.h): its first line `kdm <part>`, then
// its own section, the comments, the head of its rows and the rows, every arrangement of primitives that produces an
// operator and what it costs the part, a row each, its fields apart by tabs; and after its last row, its tail: the
// part's registers, widths, loads and stores in a block of the ruleset's language, `__krs__(` to `)__krs__` (klang.h),
// which the ruleset reader reads, its pipes, and their comments.
//
// A program that writes the rows writes the whole section, after a who line of its own; the tail stands as the file
// held it, the who that was in force over it written again before it. A program that writes the pipes writes them at
// the end of the tail, after a who line of its own, and the pipes before them are let go.
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

// the most letters of a row's operator, of its chain and of its cost
#define KDM_OPERATOR 64u
#define KDM_CHAIN 512u
#define KDM_COST 128u

    // A .kdm's lines (kfile_lines.h). A file starts as zeros
    typedef KfileLines KdmFile;

    // one row: the operator, the count of nodes, the chain, its cost as the row writes it, the runs it was timed over,
    // and where a descent wrote a sixth field, its verdict, 1 standing and 0 out; -1 where the row has no sixth
    typedef struct
    {
        char operation[KDM_OPERATOR];
        unsigned int nodes;
        char chain[KDM_CHAIN];
        char cost[KDM_COST];
        unsigned int runs;
        int verdict;
    } KdmRow;

    // the .kdm at `path` read into `file` as the run holds it (kfile_lines_read): 1, and none of its lines where there is no
    // such file; 0 where there was no memory for a line
    int kdm_read(KdmFile *file, const char *path);

    // `file` handed to the run's writer as the file at `path`, written by `who` (kfile_lines_write_by): 1, or 0 where it
    // could not be written
    int kdm_write_by(const KdmFile *file, const char *path, Who who);

    // every line `file` holds let go, and the file zeros again
    void kdm_free(KdmFile *file);

    // 1 with the row `line` is in `row`, five fields apart by tabs or six with a descent's verdict, and the operator no
    // comment; 0 otherwise
    int kdm_row_read(const char *line, KdmRow *row);

    // the line the file's tail starts at, the one after its last row, or after the head of its rows where it holds
    // none, or its count of lines where it holds no head
    unsigned int kdm_tail_at(const KdmFile *file);

    // The file's section, every line from its second to its tail, replaced by `lines`, `count` of them, written by
    // `who`: its first line `kdm <part>`, `part` where the file had none; then a who line of `who`, the lines, and
    // before the tail the who that was in force over it, `who -` where none was. 1, or 0 where there was no memory for
    // them, the file as it was
    int kdm_section_written_by(KdmFile *file, const char *part, const char *const *lines, unsigned int count, Who who);

    // Every line of the tail that is `pipe ...` or one of `comment`, `comments` of them, let go, the blank lines then
    // ending the tail taken down to none, and `pipes`, `count` of them, put by `who` at the end after a blank line and
    // the comment, where there are any. 1, or 0 where there was no memory for them, the file as it was
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
