// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krs_write_internal.h: what the modules of krs_write share: its includes, the bounds of the forms, lines, parameters and names, the anchors' texts, the forms, costs and givens read, and the functions one module reads from another
#ifndef KRS_WRITE_INTERNAL_H
#define KRS_WRITE_INTERNAL_H

#include "../../../../engine/rmc/word_web.h"
#include "../../../../types/file_defs/ksc/ksc.h"
#include "../../../../types/file_defs/qry/qry_buffer.h"
#include "../../interface/interface.h"
#include "../../protocol/counterexample/ladder.h"
#include "frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most forms the vendor's writer hands back, the longest line read, the most parameters a ruleset names a form
// with, and the longest parameter name
#define KRS_FORMS 16384u

#define KRS_LINE 512u

#define KRS_PARAMETERS 32u

#define KRS_NAME 64u

// the words of the web
#define KRS_WORDS ((unsigned int)(sizeof(s_word_web) / sizeof(s_word_web[0])))

// the names a form's relation is handed back under, as khw_write writes them: a ladder relation's, and a frame
// relation's (The frame forms asked, Open 6.4)
#define KRS_READS_LADDER "ladder."

#define KRS_READS_FRAME "frame."

// the most frame forms the vendor's writer hands back
#define KRS_FRAME_FORMS 64u

// the blobs of the run's buffer the vendor's writer hands: the forms it hands back, and what it printed
#define KRS_FORMS_NAME "krs_forms.txt"

#define KRS_MACHINE_NAME "machine.txt"

// one form the part answered for, as the vendor's writer hands it back: the relation's anchor, whether the answers
// read a word signed, the encoding it was first seen with, and its text with each operand of the relation's tuple
// written as {<place>}
typedef struct
{
    unsigned int anchor;
    int signed_read;
    unsigned long long low;
    unsigned long long high;
    char text[KRS_LINE];
} KrsForm;

extern KrsForm s_form[KRS_FORMS];
extern unsigned int s_forms;
extern KrsFrameForm s_frame_form[KRS_FRAME_FORMS];
extern unsigned int s_frame_forms;

int krs_vendor_run(char *const *word);
unsigned int krs_forms_read(const unsigned char *bytes, unsigned long long size);
unsigned int krs_costs_read(const char *path);
unsigned int krs_given_read(const char *path);
const char *krs_given_of(const char *name);
unsigned int krs_ranked(const unsigned int *writer, unsigned int writers);
// the place in `writer` of the form of the fewest operands, the first where several have as few: the operands are
// the comma-separated places after the operation in the form's text
unsigned int krs_fewest_operands(const unsigned int *writer, unsigned int writers);
unsigned int krs_parameters_named(const char *path, const char *name, char parameter[][KRS_NAME]);
int krs_word_written(const Word *word, const KrsForm *form, unsigned int *order);
int krs_text_named(const KrsForm *form, const unsigned int *order, char parameter[][KRS_NAME], unsigned int parameters, char *written, size_t room);
void krs_folder_of(const char *path, char *folder, size_t room);

#endif
