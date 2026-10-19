// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_write.h: the .khw written: the bounds of a form, how far it has been asked, its places and the
// kinds of its operands, the names the forms are handed under, what the part answered of one form, and
// the forms read and written for the vendor's writer to lay out
#ifndef KHW_WRITE_H
#define KHW_WRITE_H

#include "../../../../transpiler/lstar/interface/interface.h"
#include "../../../../transpiler/lstar/protocol/counterexample/ladder.h"
#include "../../../../transpiler/lstar/protocol/khw_learn/frame_relation.h"
#include "../../../parsers/line_read.h"
#include "../../qry/writer/qry_member.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most runs a form's operands sit in, and the most forms the part answers for
#define KHW_RUNS 32u

#define KHW_FORMS 16384u

// how far a form has been asked, for a pass to ask only what a cross-check has read before it: a discovered form holds
// only its relation, a turned form the runs its turns found but not which carry a register, a classified form the
// operand runs its register-runs kept. A widening carries only a classified form, a turns pass only a discovered one,
// a register pass only a turned one
#define KHW_PHASE_DISCOVERED 0u

#define KHW_PHASE_TURNED 1u

#define KHW_PHASE_CLASSIFIED 2u

// a frame form whose register-runs hold no run at a place of its relation's tuple answered no relation it holds a
// field of: void, which no pass asks again and the vendor lays out nowhere, kept in the working set so that it is
// not put again
#define KHW_PHASE_VOID 3u

// the place of a run that carries no word of the relation's tuple, and the text it is written under
#define KHW_PLACE_NONE 0xffffffffu

#define KHW_PLACE_NONE_TEXT "-"

// what a run's operand is, kept beside its place: a register, a pair of registers an address is read through, a
// number, or a predicate; and the letter each is written as after the place in the answers and the working set
#define KHW_KIND_REGISTER 0u

#define KHW_KIND_ADDRESS 1u

#define KHW_KIND_IMMEDIATE 2u

#define KHW_KIND_PREDICATE 3u
// a run of the constant bank's operand: the offset read from the bank, and the bank, where the walks found a run of it
#define KHW_KIND_CONSTANT 4u

#define KHW_KIND_LETTERS "ranpc"

// the name a form's relation is written under in the answers file, for the vendor's writer to name it from
#define KHW_READS_LADDER "ladder."

// the blob of the run the vendor's writer appends what it printed each time it ran in; the kernel it reads
// out of the container is KERNEL_BLOB_NAME (kernel_blob.h)
#define KHW_MACHINE_NAME "machine.txt"

// the blobs of the run the forms are kept in: the forms the part answered for, which the vendor's writer
// names and lays out, and the working set the passes of the split hand each other
#define KHW_FORMS_NAME "forms.txt"

#define KHW_WORK_NAME "forms_work.txt"

// What the part answered of one form: the relation every case of it answered, whether the case's words were read in
// the order the host put them or the other way round, whether the answers read a word signed, and the runs of bits
// its operands sit in with the place each run writes or reads
typedef struct
{
    unsigned long long low;
    unsigned long long high;
    unsigned int anchor;
    int swapped;
    int signed_read;
    int signedness_asked;
    unsigned int runs;
    unsigned int first[KHW_RUNS];
    unsigned int last[KHW_RUNS];
    // each run's place in the relation's tuple, the words in their order and then the answer, KHW_PLACE_NONE where
    // the part answered it none
    unsigned int place[KHW_RUNS];
    // what each run's operand is, one of KHW_KIND_*
    unsigned int kind[KHW_RUNS];
    // the frame relation the form answered, FRAME_RELATION_COUNT where it answered the ladder's `anchor`, and the
    // relation's reading: the word a load reads, the register's word a store keeps, a number's constant
    unsigned int frame;
    unsigned long long reading;
    // the count of the relation read's candidates that survived the gate, its reading (Q11)
    unsigned int survivors;
    // how far the form has been asked, one of KHW_PHASE_*
    unsigned int phase;
} KhwAnswered;

#define LADDER_TEXT(name_, text_, words_, measured_) text_,

static const char *const s_anchor_text[] = {LADDER_ANCHORS(LADDER_TEXT)};

#undef LADDER_TEXT

extern const char *s_path;
extern const char *s_part;
extern const char *s_layout;
extern const char *s_mnemonics;
extern const char *s_machine_writer;
extern KhwAnswered s_named[KHW_FORMS];
extern unsigned int s_names;

int khw_answers_written(void);
int khw_forms_work_written(void);
int khw_vendor_run(const char *mode, const char *answers);
unsigned int khw_forms_read(const char *name);
const char *khw_relation_named(const KhwAnswered *held, int with_reading, char *written, size_t room);
int khw_runs_read(KhwAnswered *held, const char *text);
int khw_form_listed(const unsigned char *bytes, unsigned long long size, unsigned long long low,
                    unsigned long long high);

#endif
