// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ktext.h: a k-file people read and write, held as its lines (ktext.oracle.tsv): read whole, as the run holds it
// (qry_file_read), each line without its end, and handed back whole to the run's writer, which writes it in the
// file's place (qry_filed), each line ended by a newline. Its reader turns what it reads into numbers once, as it reads
// it; the file stays the text a person writes.
//
// What a run puts in it goes after a who line, `who <code>` (qry_buffer.h, QryWho), the who of the lines after it to
// the next who line, where the who in force there is another's; and where lines of the file follow, the who that was
// in force over them is written again after what was put, `who -` where none was. A who line that stands over no line
// is let go. Every other line stands as the file held it.
#ifndef KTEXT_H
#define KTEXT_H

#include "qry/qry_buffer.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // A k-file's lines, each its own allocation in the order the file holds them. A file starts as zeros
    typedef struct
    {
        char **line;
        unsigned int lines;
        unsigned int room;
    } KtextFile;

    // the file at `path` read into `file` as the run holds it, its latest blob or the file where the run holds none
    // (qry_file_read), whatever it held before let go: 1, and none of its lines where there is neither; 0 where there
    // was no memory for a line
    int ktext_read(KtextFile *file, const char *path);

    // `file` handed to the run's writer as the file at `path` (qry_filed_by), written by `who`: 1, or 0 where no run's
    // writer runs, the path stands outside the tree, or it could not be written
    int ktext_write_by(const KtextFile *file, const char *path, QryWho who);

    // everything `file` holds let go, and the file zeros again
    void ktext_free(KtextFile *file);

    // `text`, a line without its end, kept after every line `file` holds: 1, or 0 where there was no memory for it
    int ktext_line_add(KtextFile *file, const char *text);

    // the line at `at` replaced by `text`: 1, or 0 where there was no memory for it, the line as it was
    int ktext_line_replaced(KtextFile *file, unsigned int at, const char *text);

    // the line at `at` let go, every line past it one sooner
    void ktext_line_drop(KtextFile *file, unsigned int at);

    // 1 where `line` is a who line, with its who in `who` and 1 in `held`, or 0 in `held` where it is `who -`
    int ktext_who_line(const char *line, QryWho *who, int *held);

    // the who in force at the line `at`: 1 with it in `who`, or 0 where no who line stands before it or the nearest
    // one is `who -`
    int ktext_who_at(const KtextFile *file, unsigned int at, QryWho *who);

    // `count` lines from `lines` put at `at` by `who`, after a who line of `who` where the who in force there is
    // another's, and the who that was in force there written again after them where lines follow: the count of lines
    // put, who lines and all, or 0 where there was no memory for them, the file as it was
    unsigned int ktext_lines_put_by(KtextFile *file, unsigned int at, const char *const *lines, unsigned int count,
                                    QryWho who);

    // every who line that stands over no line let go: one a who line or the file's end follows
    void ktext_who_tidied(KtextFile *file);

    // Every line of `file` replaced by `lines`, `count` of them, written by `who`: the first `first` of them stand
    // before any who line; each line after them that the file held, the same text, keeps the who that was in force
    // over it there, each line taken once in the order the file held them; and every other line goes under `who`. A
    // who line is written wherever the who in force changes: a file written again as it was is the same lines. 1, or
    // 0 where there was no memory for them, the file as it was
    int ktext_lines_written_by(KtextFile *file, unsigned int first, const char *const *lines, unsigned int count,
                               QryWho who);

    // the lines of `text`, each ended by a newline, cut in place into `*lines`, an allocation of the caller's to free
    // whose lines stand in `text`, `*count` of them: 1, or 0 where there was no memory for it or `text` lacks some
    int ktext_lines_cut(QryText *text, const char ***lines, unsigned int *count);

    // The lines of the first block of `lang` (klang.h) from the line `from` on, its opener `__<lang>__(` and its
    // closer `)__<lang>__` each a line of its own: 1 with the line after the opener's in `*first` and the closer's in
    // `*past`; 0 where none opens or the one that opens is never closed
    int ktext_block_lines(const KtextFile *file, const char *lang, unsigned int from, unsigned int *first,
                          unsigned int *past);

// each, the who the code that calls it
#define ktext_write(file_, path_) ktext_write_by((file_), (path_), QRY_WHO_HERE)
#define ktext_lines_put(file_, at_, lines_, count_) ktext_lines_put_by((file_), (at_), (lines_), (count_), QRY_WHO_HERE)
#define ktext_lines_written(file_, first_, lines_, count_)                                                             \
    ktext_lines_written_by((file_), (first_), (lines_), (count_), QRY_WHO_HERE)

#ifdef __cplusplus
}
#endif

#endif
