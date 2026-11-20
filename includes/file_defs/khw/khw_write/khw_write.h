// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_write.h: the .khw written: the bounds of a form, how far it has been queried, its places and the
// types of its operands, the names the forms are passed under, what the part answered of one form, and
// the forms read and written for the vendor's writer to arrange
#ifndef KHW_WRITE_H
#define KHW_WRITE_H

#include "../../../../src/cu/transpiler/lstar/protocol/counterexample/ladder.h"
#include "../../../../src/cu/transpiler/lstar/protocol/khw_learn/frame_relation.h"
#include "../../../parsers/line_read.h"
#include "../../vendor_writer_run.h"
#include "../../qry/writer/qry_member.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the max runs a form's operands sit in, and the max forms the part answers for
#define KHW_RUNS 32u

#define KHW_FORMS 16384u

// how far the queries of a form went, so that a pass queries only what a cross-check read before it:
//   - a discovered form has only its relation.
//   - a turned form has the runs that its turns found, but not which runs carry a register.
//   - a classified form has the operand runs that its register-runs kept.
//
// A widening carries only a classified form. A turns pass carries only a discovered form, and a register pass only a
// turned form
#define KHW_PHASE_DISCOVERED 0u

#define KHW_PHASE_TURNED 1u

#define KHW_PHASE_CLASSIFIED 2u

// a frame form can have register-runs with no run at a place of the tuple of its relation. Such a form answered no
// relation that it holds a field of. It is void. No pass queries it again, and the vendor arranges it nowhere. The working set keeps it, so that
// no pass puts it again
#define KHW_PHASE_VOID 3u

// the place of a run that carries no word of the tuple of the relation, and the text that the answers write for it
#define KHW_PLACE_NONE 0xffffffffu

#define KHW_PLACE_NONE_TEXT "-"

// what the operand of a run is, which the code keeps beside its place. It is a register, a pair of registers that an
// address comes through, a number, or a predicate. Then the character for each after the place in the answers and the working set
#define KHW_KIND_REGISTER 0u

#define KHW_KIND_ADDRESS 1u

#define KHW_KIND_IMMEDIATE 2u

#define KHW_KIND_PREDICATE 3u
// a run of the constant bank's operand: the offset read from the bank, and the bank, where the walks found a run of it
#define KHW_KIND_CONSTANT 4u

#define KHW_KIND_CHARACTERS "ranpc"

// the name of the relation of a form in the answers file, from which the writer of the vendor names it
#define KHW_READS_LADDER "ladder."

// the blob of the run the vendor's writer appends what it printed each time it ran in. The kernel it reads
// out of the container is KERNEL_BLOB_NAME (kernel_blob.h)
#define KHW_MACHINE_NAME "machine.txt"

// the blobs of the run that hold the forms. One holds the forms that the part answered for, which the writer of the
// vendor names and arranges. The other holds the working set that the passes of the split hand each other
#define KHW_FORMS_NAME "forms.txt"

#define KHW_WORK_NAME "forms_work.txt"

// What the part answered of one form:
//   - the relation that each case of it answered.
//   - whether the part read the words of the case in the order of the host, or the other way round.
//   - whether the answers read a word as signed.
//   - the runs of bits that its operands sit in, with the place that each run writes or reads.
typedef struct
{
    unsigned long long low;
    unsigned long long high;
    unsigned int anchor;
    int swapped;
    int signed_read;
    int signedness_queried;
    unsigned int runs;
    unsigned int first[KHW_RUNS];
    unsigned int last[KHW_RUNS];
    // each run's place in the relation's tuple, the words in their order and then the answer, KHW_PLACE_NONE where
    // the part answered it none
    unsigned int place[KHW_RUNS];
    // what each run's operand is, one of KHW_KIND_*
    unsigned int type[KHW_RUNS];
    // the frame relation that the form answered, FRAME_RELATION_COUNT where it answered the `anchor` of the ladder. Then
    // the read of the relation. That is the word that a load reads, the word of the register that a store stores, or
    // the constant of a number
    unsigned int frame;
    unsigned long long reading;
    // the count of the relation read's candidates that survived the gate, its reading (Q11)
    unsigned int survivors;
    // how far the queries of the form went, one of KHW_PHASE_*
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
const char *khw_relation_named(const KhwAnswered *answered, int with_reading, char *written, size_t room);
int khw_runs_read(KhwAnswered *answered, const char *text);
int khw_form_listed(const unsigned char *bytes, unsigned long long size, unsigned long long low,
                    unsigned long long high);

#endif
