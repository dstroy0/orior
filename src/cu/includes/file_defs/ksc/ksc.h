// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksc.h: a system's classification, its .ksc (ksc.oracle.tsv), a k-file people read and write (kfile_lines.h): every line,
// and the rows of a channel, `<channel> <answer> <word> <question>`, each question put on the channel, what came back,
// and the word it answered in hex. Each program that writes a .ksc writes the rows of its own questions, after a who
// line of the code that wrote them, and keeps every other line as the file held it.
#ifndef KSC_H
#define KSC_H

#include "../kfile_lines/kfile_lines.h"

#include "../../parsers/who.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the most letters of a channel, of an answer, of a word and of a question
#define KSC_CHANNEL 16u
#define KSC_ANSWER 16u
#define KSC_WORD 32u
#define KSC_QUESTION 512u

    // A .ksc's lines (kfile_lines.h). A file starts as zeros
    typedef KfileLines KscFile;

    // one row of a channel: the channel, what came back, the word it answered as the row writes it and as a number,
    // and the question
    typedef struct
    {
        char channel[KSC_CHANNEL];
        char answer[KSC_ANSWER];
        char word[KSC_WORD];
        unsigned long long value;
        char question[KSC_QUESTION];
    } KscRow;

    // the .ksc at `path` read into `file` as the run holds it (kfile_lines_read): 1, and none of its lines where there is no
    // such file; 0 where there was no memory for a line
    int ksc_read(KscFile *file, const char *path);

    // `file` handed to the run's writer as the file at `path`, written by `who` (kfile_lines_write_by): 1, or 0 where it
    // could not be written
    int ksc_write_by(const KscFile *file, const char *path, Who who);

    // every line `file` holds let go, and the file zeros again
    void ksc_free(KscFile *file);

    // `text`, a line without its end, kept after every line `file` holds: 1, or 0 where there was no memory for it
    int ksc_line_add(KscFile *file, const char *text);

    // 1 with the row `line` is in `row` where it is a row of `channel`, its channel the line's first word and a word
    // after its answer; 0 otherwise
    int ksc_row_read(const char *line, const char *channel, KscRow *row);

    // the field line `field <name> <from> <width>` at `line`, its name into `name`, which holds `room`, its first bit
    // into `from` and its width into `width`: 1, or 0 where `line` is no field line or a value does not fit
    int ksc_field_read(const char *line, char *name, size_t room, unsigned int *from, unsigned int *width);

    // Every row of `channel` whose question begins with the words `asked` let go, whatever came back, and `rows`,
    // `count` of them, put by `who` where the first row of the question stood, so that a row put again keeps its
    // place; where the file holds none of the question, after the last row of the channel it keeps, or after its last
    // line where it keeps none of the channel (kfile_lines_put_by). Each count of the channel,
    // `count <channel> <answer> <number>`, is taken again over the rows it then holds, and every who line left over
    // no line is let go. 1, or 0 where there was no memory for the rows, the file as it was
    int ksc_rows_replaced_by(KscFile *file, const char *channel, const char *asked, const char *const *rows,
                             unsigned int count, Who who);

    // As ksc_rows_replaced_by, and every row let go that no row put gives again kept in `removed`, a file of the
    // caller's (kfile_lines_free), each as the file held it and in its order, for the part's .krf to keep (krf.h); NULL keeps
    // none. 1, or 0 where there was no memory for the rows, the file as it was
    int ksc_rows_replaced_traced_by(KscFile *file, const char *channel, const char *asked, const char *const *rows,
                                    unsigned int count, Who who, KfileLines *removed);

// each, the who the code that calls it
#define ksc_write(file_, path_) ksc_write_by((file_), (path_), WHO_HERE)
#define ksc_rows_replaced(file_, channel_, asked_, rows_, count_)                                                      \
    ksc_rows_replaced_by((file_), (channel_), (asked_), (rows_), (count_), WHO_HERE)
#define ksc_rows_replaced_traced(file_, channel_, asked_, rows_, count_, removed_)                                     \
    ksc_rows_replaced_traced_by((file_), (channel_), (asked_), (rows_), (count_), WHO_HERE, (removed_))

#ifdef __cplusplus
}
#endif

#endif
