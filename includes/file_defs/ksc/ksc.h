// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksc.h: a system's classification, its .ksc (ksc.oracle.tsv), a k-file people read and write (kfile_lines.h): every
// line, and the rows of a channel, `<channel> <answer> <word> <question>`, each question put on the channel, what came
// back, and the word it answered in hex. Each program that writes a .ksc writes the rows of its own questions, after a
// who line of the code that wrote them, and retains every other line as the file contained it.
#ifndef KSC_H
#define KSC_H

#include "../kfile_lines/kfile_lines.h"

#include "../../parsers/who.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the max characters of a channel, of an answer, of a word and of a question
#define KSC_CHANNEL 16u
#define KSC_ANSWER 16u
#define KSC_WORD 32u
#define KSC_QUESTION 512u

    // A .ksc's lines (kfile_lines.h). A file starts as zeros
    typedef KfileLines KscFile;

    // one row of a channel: the channel, what came back, and the word that it answered, as the row writes it and as a
    // number. Then the question
    typedef struct
    {
        char channel[KSC_CHANNEL];
        char answer[KSC_ANSWER];
        char word[KSC_WORD];
        unsigned long long value;
        char question[KSC_QUESTION];
    } KscRow;

    // the .ksc at `path`, read into `file` as the run stores it (kfile_lines_read). The result is 1, with no lines where
    // there is no such file. It is 0 where there was no memory for a line
    int ksc_read(KscFile *file, const char *path);

    // `file` passed to the run's writer as the file at `path`, written by `who` (kfile_lines_write_by). The result is 1,
    // or 0 where the write failed
    int ksc_write_by(const KscFile *file, const char *path, Who who);

    // every line `file` contains let go, and the file zeros again
    void ksc_free(KscFile *file);

    // `text`, a line without its end, added after every line `file` contains: 1, or 0 where there was no memory for it
    int ksc_line_add(KscFile *file, const char *text);

    // 1 with the row `line` is in `row` where it is a row of `channel`, its channel the line's first word and a word
    // after its answer; 0 otherwise
    int ksc_row_read(const char *line, const char *channel, KscRow *row);

    // the field line `field <name> <from> <width>` at `line`. Its name goes into `name`, which has room for `room`. Its
    // first bit goes into `from`, and its width into `width`. The result is 1, or 0 where `line` is no field line or a
    // value does not fit
    int ksc_field_read(const char *line, char *name, size_t room, unsigned int *from, unsigned int *width);

    // Each row of `channel` whose question begins with the words `question_prefix`, let go, whatever came back. Then
    // `who` puts `rows`, `count` of them, where the first row of the question was. A row put again then keeps its place.
    // Where the file holds none of the question, they go after the last row of the channel that it keeps. Where it keeps
    // none of the channel, they go after its last line (kfile_lines_put_by).
    //
    // The call takes each count of the channel, `count <channel> <answer> <number>`, again over the rows that it then
    // holds. It drops each who line that is over no line. The result is 1, or 0 where there was no memory for the rows,
    // and the file stays as it was
    int ksc_rows_replaced_by(KscFile *file, const char *channel, const char *question_prefix, const char *const *rows,
                             unsigned int count, Who who);

    // As ksc_rows_replaced_by. Each row that the call lets go, and that no put row gives again, goes in `removed`. That
    // is a file of the caller (kfile_lines_free), for the .krf of the part (krf.h). Each row is as it was in the file, and
    // in its order. NULL records none. The result is 1, or 0 where there was no memory for the rows, and the file stays as
    // it was
    int ksc_rows_replaced_traced_by(KscFile *file, const char *channel, const char *question_prefix,
                                    const char *const *rows, unsigned int count, Who who, KfileLines *removed);

// each, the who the code that calls it
#define ksc_write(file_, path_) ksc_write_by((file_), (path_), WHO_HERE)
#define ksc_rows_replaced(file_, channel_, question_prefix_, rows_, count_)                                            \
    ksc_rows_replaced_by((file_), (channel_), (question_prefix_), (rows_), (count_), WHO_HERE)
#define ksc_rows_replaced_traced(file_, channel_, question_prefix_, rows_, count_, removed_)                           \
    ksc_rows_replaced_traced_by((file_), (channel_), (question_prefix_), (rows_), (count_), WHO_HERE, (removed_))

#ifdef __cplusplus
}
#endif

#endif
