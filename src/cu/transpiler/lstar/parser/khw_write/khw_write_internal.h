// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_write_internal.h: what the modules of khw_write share: its includes, the bounds of a question and a pass, the state of the question, the pool, the forms named and the paths given, and the functions one module reads from another
#ifndef KHW_WRITE_INTERNAL_H
#define KHW_WRITE_INTERNAL_H

#include "../../protocol/counterexample/ladder.h"
#include "../../protocol/gate/survivors.h"
#include "../../protocol/order/scheduler.h"
#include "../../protocol/query/answer_read.h"
#include "../../protocol/teacher/run_channel.h"
#include "frame_relation.h"

#include "../../../../types/file_defs/krf/krf.h"
#include "../../../../types/file_defs/ksc/ksc.h"
#include "../../../../types/file_defs/qry/qry_buffer.h"
#include "../../../vendor_bin_layouts/container_layout.h"
#include "../../interface/interface.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most places a kernel's code holds, and the bytes of one instruction
#define KHW_PLACES 256u

#define KHW_INSTRUCTION 16u

// the bits a turn reaches: past the operation key, which names the operation and is never turned, and below the
// scheduler's own word, which is set once for a run (P9)
#define KHW_TURN_FIRST 12u

#define KHW_TURN_LAST 104u
#define KHW_TURNS (KHW_TURN_LAST - KHW_TURN_FIRST + 1u)

// the most runs a form's operands sit in, and the most forms the part answers for
#define KHW_RUNS 32u

// the settings a run is probed with in a stage of the register pass, and the stages
#define KHW_SETTINGS 2u
#define KHW_STAGES 2u

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

// the two registers the kernel leaves a case's words in, found by asking: a run set to one of these numbers reads that
// register, and the two words differ: an operand field answers two different words, a modifier one
#define KHW_FIRST_REGISTER 0u

#define KHW_SECOND_REGISTER 7u

// the place of a run that carries no word of the relation's tuple, and the text it is written under
#define KHW_PLACE_NONE 0xffffffffu

#define KHW_PLACE_NONE_TEXT "-"

// what a run's operand is, kept beside its place: a register, a pair of registers an address is read through, a
// number, or a predicate; and the letter each is written as after the place in the answers and the working set
#define KHW_KIND_REGISTER 0u

#define KHW_KIND_ADDRESS 1u

#define KHW_KIND_IMMEDIATE 2u

#define KHW_KIND_PREDICATE 3u

#define KHW_KIND_LETTERS "ranp"

// a fixed register the part's .ksc does not name
#define KHW_FIXED_NONE 0xffffffffu

// the name a form's relation is written under in the answers file, for the vendor's writer to name it from
#define KHW_READS_LADDER "ladder."

// the blobs of the run's buffer the vendor's writer hands: the kernel it reads out of the container, and what it
// printed each time it ran
#define KHW_KERNEL_NAME "kernel.txt"

#define KHW_MACHINE_NAME "machine.txt"

// the blobs of the run's buffer the forms are kept in: the forms the part answered for, which the vendor's writer
// names and lays out, and the working set the passes of the split hand each other
#define KHW_FORMS_NAME "forms.txt"

#define KHW_WORK_NAME "forms_work.txt"

// the launches the kernel's own question is timed over, as a task's cost is read (P11, Step 10)
#define KHW_LAUNCHES 100u

// the most words after `--` the channel is opened on and their end: a carrier, the airlock's pop and its cross, each of
// up to eight words, and the `--pop` and `--cross` between them (run_channel.h)
#define KHW_CARRIER_WORDS 27u

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

// one candidate of the relation read: a relation of the ladder, the case's two words read in the order the host put
// them or swapped, and the answer read plain or signed
typedef struct
{
    unsigned int anchor;
    int swapped;
    int signed_read;
} KhwCandidate;

extern unsigned int s_registers;
extern unsigned char s_kernel_text[KHW_PLACES * KHW_INSTRUCTION];
extern unsigned char s_code[KHW_PLACES * KHW_INSTRUCTION];
extern unsigned int s_kernel_places;
// the slot's place among the kept places where the kernel listing names it, the part's answer in its .ksc, and
// s_kernel_places where it names none
extern unsigned int s_kernel_slot;
extern unsigned int s_slot;
extern unsigned long long s_unreached_low;
extern unsigned long long s_unreached_high;
extern const char *s_path;
extern const char *s_part;
extern const char *s_layout;
extern const char *s_mnemonics;
extern const char *s_fields;
extern char s_ksc_path[1024];
extern const char *s_machine_writer;
extern RunQuestion s_question;
extern unsigned long long s_asks;
extern unsigned int s_launches;
extern RunQuestion *s_pool;
extern unsigned int s_pooled;
extern unsigned int s_union_word[GATE_CASES][2];
extern unsigned int s_union_cases;
extern KhwCandidate s_candidate[GATE_CANDIDATES];
extern unsigned int s_candidates;
extern KhwAnswered s_named[KHW_FORMS];
extern unsigned int s_names;

unsigned long long khw_word_read(const unsigned char *instruction, unsigned int byte);
void khw_word_write(unsigned char *instruction, unsigned int byte, unsigned long long value);
unsigned long long khw_run_read(unsigned long long low, unsigned long long high, unsigned int first, unsigned int last);
void khw_run_write(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int last, unsigned long long value);
void khw_kernel_put(unsigned char *code);
void khw_code_put(unsigned char *code, unsigned int place, unsigned long long low, unsigned long long high);
int khw_asked(const unsigned int word[][2], unsigned int count, unsigned int slot, unsigned long long *answered);
unsigned int khw_pooled(unsigned int place, unsigned long long low, unsigned long long high, const unsigned int word[][2], unsigned int count);
void khw_round_carried(void);
unsigned int khw_anchor_cases(unsigned int anchor, int swapped, unsigned int word[][2], unsigned int *plain, unsigned int *signed_read);
unsigned int khw_candidate_answer(const KhwCandidate *candidate, const unsigned int *word);
void khw_union_written(void);
int khw_relation_of(const RunQuestion *question, KhwAnswered *held);
int khw_relation_asked(unsigned long long low, unsigned long long high, KhwAnswered *held);
int khw_first_round(KhwAnswered *kernel, unsigned int *slot, unsigned long long *nanoseconds);
void khw_turns_asked(KhwAnswered *held, const unsigned int word[1][2]);
unsigned int khw_forms_in_phase(unsigned int phase, KhwAnswered **forms);
int khw_turning_words(unsigned int anchor, int swapped, unsigned int word[2], unsigned int *expected);
int khw_places_whole(const unsigned int *place, unsigned int runs, unsigned int words);
unsigned int khw_forms_placed_twice(KhwAnswered **forms, unsigned int count);
void khw_plan_handed(const char *pass, unsigned long long questions);
unsigned int khw_fields_turns(KhwAnswered *const *forms, unsigned int count);
unsigned int khw_fields_registers(KhwAnswered *const *forms, unsigned int count);
void khw_fields_asked(KhwAnswered *held);
int khw_line_read(const unsigned char *bytes, unsigned long long size, unsigned long long *at, char *line, size_t room);
int khw_kernel_read(void);
int khw_form_asked(unsigned long long low, unsigned long long high);
unsigned int khw_seeds_asked(const char *name);
unsigned int khw_widened(unsigned int from, int discover);
int khw_answers_written(void);
int khw_forms_work_written(void);
void khw_fields_written(const char *path);
int khw_vendor_run(const char *mode, const char *answers);
unsigned int khw_forms_read(const char *name);
void khw_enumerate(unsigned int slot, const char *forms);
const char *khw_relation_named(const KhwAnswered *held, int with_reading, char *written, size_t room);
unsigned int khw_fixed_register(const char *name);
int khw_named_already(unsigned long long low, unsigned long long high);
// the narrowest field of the instruction word, as the part's .ksc gives them, that holds the bits `first` to `last`:
// 1 with its name, into `name` of `room` bytes, its first bit and its width; 0 where the .ksc gives none that does
int khw_field_holding(unsigned int first, unsigned int last, char *name, size_t room, unsigned int *field_first,
                      unsigned int *field_bits);
int khw_field_read(const char *name, unsigned int *first, unsigned int *bits);
unsigned int khw_frame_pooled(unsigned long long low, unsigned long long high, const unsigned int word[][2], unsigned int count);
const unsigned int (*khw_frame_cases(unsigned int *count))[2];
void khw_frame_stalled(unsigned long long *low, unsigned long long *high);
int khw_frame_of(const RunQuestion *question, KhwAnswered *held);
unsigned int khw_frame_round(const KhwAnswered *slot_form, int discover);

#endif
