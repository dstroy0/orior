// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_forms.h: the frame forms of a part's ruleset, each written as the schema's form through a table of names (The frame
// Open 6.4)
#ifndef KRS_FRAME_FORMS_H
#define KRS_FRAME_FORMS_H

#include "../../qry/writer/qry_member.h"

#include <stddef.h>

// the longest relation name and the longest placed text a frame form carries
#define KRS_FRAME_RELATION 64u

#define KRS_FRAME_TEXT 512u

// one frame form the vendor's writer passes back: its relation, frame.<relation>, and its text with each operand of
// the relation's tuple written as {<place>}
typedef struct
{
    char relation[KRS_FRAME_RELATION];
    char text[KRS_FRAME_TEXT];
} KrsFrameForm;

// The rows of the table of frame forms, relation, form and places, read into the module from `path`. Where `path` is
// NULL, they come from frame_forms.tsv beside this header in the tree of its build. The result is the count read, 0
// where none
unsigned int krs_frame_read(const char *path);

// The ruleset line of `form`, `form <name> <parameters> = <text>\n`, into `line`, which has room for `room`:
//   - the name that the table gives its relation, by the row whose places are as many as the text holds.
//   - the parameters, as the ruleset at `named_ruleset` names that form.
//   - the text, with each {<place>} written as {<parameter>} through the places of the row.
//
// The result is 1, or 0 where the table maps none or the line does not fit. It is also 0 where the ruleset names
// another count of parameters than the row gives places. It is also 0 where the text holds a place that no parameter
// takes
int krs_frame_named(const KrsFrameForm *form, const char *named_ruleset, char *line, size_t room);

// The frame forms, written at `*at` into `out`, which has room for `cap` (verba_scribo.h). `*at` moves past them, or to
// `cap` where they did not fit. There is one line a row of the table, in the order of the table:
//   - of the `count` forms in `form`, the first that the row names writes the line. The writers of one placed text are
//     one.
//   - a row of no places also comes from a form of its relation whose one place is a guard, with the guard struck.
//     The vendor writes a guard at each predicate as none.
//   - where the run passes none that the row names, the line comes from the ruleset at `krs_path`. It is the line that
//     it gave the form of the row before this run, kept whole.
//
// A written line can replace a given line with another text. The code then stores the given line in the .krf beside
// the .ksc at `ksc_path` (krf.h). `krs_path` can be NULL or name no file, and then it gives nothing. The result is the
// count written
unsigned int krs_frame_written(char *out, size_t cap, size_t *at, const KrsFrameForm *form, unsigned int count,
                               const char *named_ruleset, const char *krs_path, const char *ksc_path);

#endif
