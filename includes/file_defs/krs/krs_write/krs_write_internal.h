// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com> SPDX-License-Identifier: AGPL-3.0-or-later
// OR LicenseRef-Commercial OR LicenseRef-Educational krs_write_internal.h: what the modules of orior_krs_write share:
// its includes, the bounds of the forms, lines, parameters and names, the anchors' texts, the forms, costs and givens
// read, and the functions one module reads from another
#ifndef KRS_WRITE_INTERNAL_H
#define KRS_WRITE_INTERNAL_H

#include "../../../../src/cu/engine/rmc/word_web.h"
#include "../../../../src/cu/transpiler/lstar/protocol/counterexample/ladder.h"
#include "../../kernel_blob/forms_blob.h"
#include "../../ksc/ksc.h"
#include "../../qry/writer/qry_member.h"
#include "../../vendor_writer_run.h"
#include "frame_forms.h"

#include <stdio.h>
#include <stdlib.h>

// the max forms that the writer of the vendor passes back, and the longest line read. Then the max parameters that a
// ruleset names a form with, and the longest parameter name
#define KRS_FORMS 16384u

#define KRS_LINE 512u

#define KRS_PARAMETERS 32u

#define KRS_NAME 64u

// the words of the web
#define KRS_WORDS ((unsigned int)(sizeof(s_word_web) / sizeof(s_word_web[0])))

// the names under which the writer passes back the relation of a form, as khw_write writes them. There is one for a
// ladder relation, and one for a frame relation (Open 6.4)
#define KRS_READS_LADDER "ladder."

#define KRS_READS_FRAME "frame."

// the max frame forms the vendor's writer passes back
#define KRS_FRAME_FORMS 64u

// the blob of the run the vendor's writer appends what it printed in; the forms it appends are FORMS_BLOB_NAME
#define KRS_MACHINE_NAME "machine.txt"

// one form that the part answered for, as the writer of the vendor passes it back. It holds the anchor of the relation,
// whether the answers read a word signed, and the encoding that the writer first saw it with. It also holds its text,
// with each operand of the tuple of the relation written as {<place>}
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
// the place in `writer` of the form with the fewest operands, the first where several have as few. The operands are
// the comma-separated places after the operation in the text of the form
unsigned int krs_fewest_operands(const unsigned int *writer, unsigned int writers);
unsigned int krs_parameters_named(const char *path, const char *name, char parameter[][KRS_NAME]);

// the place `{<place>}` at `walk` into `place`, its characters, braces too, into `length`: 1, or 0 where `walk`
// contains none or a place an unsigned int does not represent
int krs_place_read(const char *walk, unsigned int *place, int *length);
int krs_word_written(const Word *word, const KrsForm *form, unsigned int *order);
int krs_text_named(const KrsForm *form, const unsigned int *order, char parameter[][KRS_NAME], unsigned int parameters,
                   char *written, size_t room);
void krs_folder_of(const char *path, char *folder, size_t room);

#endif
