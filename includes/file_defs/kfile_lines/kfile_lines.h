// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kfile_lines.h: a k-file that people read and write, stored as its lines (kfile_lines.oracle.tsv). The code reads it
// whole, as the run stores it (qry_member_file), each line without its end. It passes it back whole to the writer of
// the run, which writes it in the place of the file (qry_member_filed), each line ended by a newline.
// Its reader turns what it reads into numbers once, as it reads it. The file stays the text that a person writes.
//
// What a run puts in it goes after a who line, `who <code>` (who.h, Who), where the who in force there is another. That
// who is the who of the lines after it, up to the next who line.
// Where lines of the file follow, the code writes again the who that was in force over them after the new lines. It
// writes `who -` where none was. The code drops a who line that is over no line. Each other line stays as it was in the
// file.
#ifndef KFILE_LINES_H
#define KFILE_LINES_H

#include "../qry/writer/qry_member.h"

#include "../../parsers/who.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // A k-file's lines, each its own allocation in the order the file contains them. A file starts as zeros
    typedef struct
    {
        char **line;
        unsigned int lines;
        unsigned int room;
    } KfileLines;

    // the file at `path`, read into `file` as the run stores it (qry_member_file). That is its latest blob, or the file
    // where the run stores none. The call lets go of the old contents of `file`.
    // The result is 1, with no lines where there is neither. It is 0 where there was no memory for a line
    int kfile_lines_read(KfileLines *file, const char *path);

    // `file`, passed to the writer of the run as the file at `path` (qry_member_filed), written by `who`. The result is
    // 1, or 0 where no writer of a run runs, the path is outside the tree, or the write failed
    int kfile_lines_write_by(const KfileLines *file, const char *path, Who who);

    // everything `file` contains let go, and the file zeros again
    void kfile_lines_free(KfileLines *file);

    // `text`, a line without its end, added after every line `file` contains: 1, or 0 where there was no memory for it
    int kfile_lines_line_add(KfileLines *file, const char *text);

    // the line at `at` replaced by `text`: 1, or 0 where there was no memory for it, the line as it was
    int kfile_lines_line_replaced(KfileLines *file, unsigned int at, const char *text);

    // the line at `at` let go, every line past it one sooner
    void kfile_lines_line_drop(KfileLines *file, unsigned int at);

    // 1 where `line` is a who line, with its who in `who` and 1 in `has_who`, or 0 in `has_who` where it is `who -`
    int kfile_lines_who_line(const char *line, Who *who, int *has_who);

    // the who in force at the line `at`: 1 with it in `who`, or 0 where no who line is before it or the nearest
    // one is `who -`
    int kfile_lines_who_at(const KfileLines *file, unsigned int at, Who *who);

    // `count` lines from `lines`, put at `at` by `who`. They come after a who line of `who` where the who in force there
    // is another. Where lines follow, the call writes again the who that was in force there after them.
    // The result is the count of lines put, with the who lines. It is 0 where there was no memory for them, and the file
    // stays as it was
    unsigned int kfile_lines_put_by(KfileLines *file, unsigned int at, const char *const *lines, unsigned int count,
                                    Who who);

    // every who line that is over no line let go: one a who line or the file's end follows
    void kfile_lines_who_tidied(KfileLines *file);

    // Each line of `file`, replaced by `lines`, `count` of them, written by `who`:
    //   - the first `first` of them are before any who line.
    //   - each line after them that was in the file, the same text, keeps the who that was in force over it there. The
    //     call takes each line once, in the order of the file.
    //   - each other line goes under `who`.
    //
    // The call writes a who line wherever the who in force changes. A file written again as it was is the same lines.
    // The result is 1, or 0 where there was no memory for them, and the file stays as it was
    int kfile_lines_written_by(KfileLines *file, unsigned int first, const char *const *lines, unsigned int count,
                               Who who);

    // the lines of the `size` bytes of `text`, each ended by a newline, cut in place into `*lines`, `*count` of them.
    // That is an allocation that the caller frees, and its lines are in `text`. The result is 1, or 0 where there was no
    // memory for it
    int kfile_lines_cut(char *text, unsigned long long size, const char ***lines, unsigned int *count);

    // The lines of the first block of `lang` (klang.h) from the line `from` on. Its opener `__<lang>__(` and its closer
    // `)__<lang>__` are each a line of their own.
    // The result is 1, with the line after the opener in `*first` and the line of the closer in `*past`. It is 0 where
    // none opens, or the one that opens never closes
    int kfile_lines_block_lines(const KfileLines *file, const char *lang, unsigned int from, unsigned int *first,
                                unsigned int *past);

// each, the who the code that calls it
#define kfile_lines_write(file_, path_) kfile_lines_write_by((file_), (path_), WHO_HERE)
#define kfile_lines_put(file_, at_, lines_, count_) kfile_lines_put_by((file_), (at_), (lines_), (count_), WHO_HERE)
#define kfile_lines_written(file_, first_, lines_, count_)                                                             \
    kfile_lines_written_by((file_), (first_), (lines_), (count_), WHO_HERE)

#ifdef __cplusplus
}
#endif

#endif
