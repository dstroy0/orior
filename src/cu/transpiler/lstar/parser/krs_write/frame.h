// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.h: the frame forms of a part's ruleset, each written as the schema's form through a table of names (The frame
// forms asked, Open 6.4)
#ifndef KRS_FRAME_H
#define KRS_FRAME_H

#include "../../../../types/file_defs/qry/qry_buffer.h"

#include <stddef.h>

// the longest relation name and the longest placed text a frame form carries
#define KRS_FRAME_RELATION 64u

#define KRS_FRAME_TEXT 512u

// one frame form the vendor's writer hands back: its relation, frame.<relation>, and its text with each operand of
// the relation's tuple written as {<place>}
typedef struct
{
    char relation[KRS_FRAME_RELATION];
    char text[KRS_FRAME_TEXT];
} KrsFrameForm;

// The rows of the table of frame forms, relation, form and places, read into the module from `path`, or where `path`
// is NULL from frame_forms.tsv beside this header in the tree it was built from. The count read, 0 where none
unsigned int krs_frame_read(const char *path);

// The ruleset line of `form`, `form <name> <parameters> = <text>\n`, into `line`, which holds `room`: the name the
// table gives its relation, by the row whose places are as many as the text holds; the parameters as the ruleset at
// `named_ruleset` names that form; and the text with each {<place>} written as {<parameter>} through the row's places.
// 1, or 0 where the table maps none, the ruleset names another count of parameters than the row gives places, the
// text holds a place no parameter takes, or the line does not fit
int krs_frame_named(const KrsFrameForm *form, const char *named_ruleset, char *line, size_t room);

// The frame forms written to `forms`, one line a row of the table in the table's order: of the `count` forms in
// `form`, the first the row names writes the line, the writers of one placed text being one; and where the run hands
// none the row names, the line the ruleset at `krs_path` gave the row's form before this run, kept whole. A given line
// a written one replaces with another text is kept in the .krf beside the .ksc at `ksc_path` (krf.h). `krs_path` may
// be NULL or name no file, and then nothing is given. The count written
unsigned int krs_frame_written(QryText *forms, const KrsFrameForm *form, unsigned int count, const char *named_ruleset,
                               const char *krs_path, const char *ksc_path);

#endif
