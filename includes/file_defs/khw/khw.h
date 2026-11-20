// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw.h: a part's machine file, <part>.khw beside its rulesets (khw.oracle.tsv), a k-file people read (kfile_lines.h).
// Its first line is `forms 2`, then `part <name>`, then a line a form:
//
//   - form <operation> <type><mark>,... <low> <high> <operand>:<first>-<last>:<place>;... <relation> <signed>
//     <the instruction as first seen>
//
// The form is on one line. The fields <low> and <high> are the two 64-bit words of the instruction in hex, low first,
// as a listing prints them. The runs are the bits that change each operand, by the place of the operand in the
// printed text.
//
// The <place> of a run is where its operand is in the tuple of the relation. The tuple is the words in their order,
// and then the answer, as 1,1 -> 2 satisfies them (ladder.h). It is - where the part answered none for it.
//
// The field <relation> is the relation that each case of the form answered. The field <signed> is 1 where the answers
// read a word as signed. Both are - where the queries of the form found no relation. A file at `forms 1` holds no
// place, relation or signedness, and reads as - in each.
//
// Its reader turns each type and mark into its number as it reads it. A program that writes the file writes all of
// it. Each old line of the file keeps the who that was in force over it. Each other line goes after a who line of the
// program (kfile_lines_written_by).
#ifndef KHW_H
#define KHW_H

#include "../../parsers/who.h"
#include "../kfile_lines/kfile_lines.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the format of the lines after the first
#define KHW_VERSION 2u

// the longest stored instruction text, and the longest operation or operand. Then the most operands in an instruction,
// and the most runs of bits that the operands of one form take between them
#define KHW_TEXT 192u
#define KHW_TOKEN 64u
#define KHW_OPERANDS 8u
#define KHW_RUNS 32u

// the place of a run whose operand is nowhere in the tuple of the relation, and the text for it
#define KHW_NO_PLACE 0xffffffffu
#define KHW_NO_PLACE_TEXT "-"

    // what one printed operand is, each by the place of its name in the file
    enum KhwKind
    {
        // an operand that the parts reader did not know. No instruction of that form can assemble with it
        KHW_KIND_UNKNOWN = 0,
        KHW_KIND_REGISTER = 1,
        KHW_KIND_PREDICATE = 2,
        KHW_KIND_IMMEDIATE = 3,
        KHW_KIND_CONSTANT = 4,
        KHW_KIND_ADDRESS = 5,
        KHW_KIND_LABEL = 6,
        KHW_KIND_UNIFORM = 7,
        KHW_KIND_SYSTEM = 8
    };
#define KHW_KINDS 9u

    // the mark an operand carries, by the place of its character in the file: none, - , ~ and !
    enum KhwMark
    {
        KHW_MARK_NONE = 0,
        KHW_MARK_NEGATE = 1,
        KHW_MARK_INVERT = 2,
        KHW_MARK_NOT = 3
    };
#define KHW_MARKS 4u

    // one run of bits that changes one printed operand, and its operand's place in the relation's tuple, KHW_NO_PLACE
    // where the part answered it none
    typedef struct
    {
        unsigned int operand;
        unsigned int first;
        unsigned int last;
        unsigned int place;
    } KhwRun;

    // one form, the encoding that it first came with, and the bits that its operands sit in. Then the relation that
    // each case of it answered, and whether the answers read a word as signed. `relation` is empty where its queries
    // found none
    typedef struct
    {
        char operation[KHW_TOKEN];
        unsigned int operands;
        unsigned int type[KHW_OPERANDS];
        unsigned int mark[KHW_OPERANDS];
        unsigned long long low;
        unsigned long long high;
        unsigned int runs;
        KhwRun run[KHW_RUNS];
        char relation[KHW_TOKEN];
        int signed_read;
        char text[KHW_TEXT];
    } KhwForm;

    // A .khw's lines (kfile_lines.h). A file starts as zeros
    typedef KfileLines KhwFile;

    // the .khw at `path`, read into `file` as the run stores it (kfile_lines_read). The result is 1, with no lines where
    // there is no such file. It is 0 where there was no memory for a line
    int khw_read(KhwFile *file, const char *path);

    // `file` passed to the run's writer as the file at `path`, written by `who` (kfile_lines_write_by). The result is 1,
    // or 0 where the write failed
    int khw_write_by(const KhwFile *file, const char *path, Who who);

    // every line `file` contains let go, and the file zeros again
    void khw_free(KhwFile *file);

    // the name of the type `type`, and the character of the mark `mark`, as the file writes them. A type past the last
    // is "unknown", and a mark past the last is ""
    const char *khw_type_name(unsigned int type);
    const char *khw_mark_name(unsigned int mark);

    // 1 with the format `line` names in `version` where it is `forms <n>`; 0 otherwise
    int khw_version_read(const char *line, unsigned int *version);

    // 1 with the part `line` names in `part`, which has room for `room`, cut to it, where it is `part <name>`; 0
    // otherwise
    int khw_part_read(const char *line, char *part, size_t room);

    // 1 with the form that `line` writes in `form`, read at the format `version`. A form at 2 holds its relation and its
    // signedness, and a form at any other format holds neither. 0 where it is no form, or a column of it does not read
    int khw_form_read(const char *line, unsigned int version, KhwForm *form);

    // Every line of `file` replaced by the machine's: `forms 2`, the comment, `part <part>` and a line for each of
    // `forms`, `count` of them, written by `who` (kfile_lines_written_by), `forms 2` before any who line. 1, or 0 where
    // there was no memory for them, the file as it was
    int khw_written_by(KhwFile *file, const char *part, const KhwForm *forms, unsigned int count, Who who);

// each, the who the code that calls it
#define khw_write(file_, path_) khw_write_by((file_), (path_), WHO_HERE)
#define khw_written(file_, part_, forms_, count_) khw_written_by((file_), (part_), (forms_), (count_), WHO_HERE)

#ifdef __cplusplus
}
#endif

#endif
