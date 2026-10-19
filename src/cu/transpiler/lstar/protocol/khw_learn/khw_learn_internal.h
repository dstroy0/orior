// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_learn_internal.h: what the modules of the pass share: its includes, the bounds of a question and a pass,
// the state of the question, the pool, the forms named and the paths given, and the functions one module reads
// from another
#ifndef KHW_LEARN_INTERNAL_H
#define KHW_LEARN_INTERNAL_H

#include "../../../../includes/file_defs/khw/khw_write/khw_write.h"
#include "../gate/survivors.h"
#include "../order/scheduler.h"
#include "../query/answer_read.h"
#include "../teacher/run_channel.h"
#include "frame_relation.h"

#include "../../../../includes/file_defs/kernel_blob/kernel_blob.h"
#include "../../../../includes/file_defs/krf/krf.h"
#include "../../../../includes/file_defs/ksc/ksc.h"
#include "../../../vendor_bin_layouts/container_layout.h"

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

// the settings a run is probed with in a stage of the register pass, and the stages
#define KHW_SETTINGS 2u
#define KHW_STAGES 2u

// the two registers the kernel leaves a case's words in, found by asking: a run set to one of these numbers reads that
// register, and the two words differ: an operand field answers two different words, a modifier one
#define KHW_FIRST_REGISTER 0u

#define KHW_SECOND_REGISTER 7u

// a fixed register the part's .ksc does not name
#define KHW_FIXED_NONE 0xffffffffu

// the launches the kernel's own question is timed over, as a task's cost is read (P11, Step 10)
#define KHW_LAUNCHES 100u

// how many times over a form is written in a costs question, one copy a place after the slot: the many links of one
// ask, which raise a form's cost above a launch's own (P6, Q5)
#define KHW_COST_COPIES 2048u

// the row a form's cost is kept under in the part's .ksc: its word the least launch's advance of the clock the part
// named over the copies, less the kernel's alone, and its question the form and the spread of the launches,
// `run answers <least> cost <low> <high> <spread>`, in hex as every run row writes them
#define KHW_COST_ROW "run answers %016llx cost %016llx %016llx %016llx"

// the most words after `--` the channel is opened on and their end: a carrier, the airlock's pop and its cross, each of
// up to eight words, and the `--pop` and `--cross` between them (run_channel.h)
#define KHW_CARRIER_WORDS 27u

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
// the kernel's places before the one the least opening begins at, dropped from every question's code and put as the
// frame round's candidates like the kept ones, since the part ran each of them
extern unsigned char s_opening_text[KHW_PLACES * KHW_INSTRUCTION];
extern unsigned int s_opening_places;
// the slot's place among the kept places where the kernel listing names it, the part's answer in its .ksc, and
// s_kernel_places where it names none
extern unsigned int s_kernel_slot;
extern unsigned int s_slot;
extern unsigned long long s_unreached_low;
extern unsigned long long s_unreached_high;
extern const char *s_fields;
extern char s_ksc_path[1024];
extern RunQuestion s_question;
extern unsigned long long s_asks;
extern unsigned int s_launches;
extern RunQuestion *s_pool;
extern unsigned int s_pooled;
extern unsigned int s_union_word[GATE_CASES][2];
extern unsigned int s_union_cases;
extern KhwCandidate s_candidate[GATE_CANDIDATES];
extern unsigned int s_candidates;

unsigned long long khw_word_read(const unsigned char *instruction, unsigned int byte);
void khw_word_write(unsigned char *instruction, unsigned int byte, unsigned long long value);
unsigned long long khw_run_read(unsigned long long low, unsigned long long high, unsigned int first, unsigned int last);
void khw_run_write(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int last,
                   unsigned long long value);
void khw_kernel_put(unsigned char *code);
void khw_code_put(unsigned char *code, unsigned int place, unsigned long long low, unsigned long long high);
int khw_asked(const unsigned int word[][2], unsigned int count, unsigned int slot, unsigned long long *answered);
unsigned int khw_pooled(unsigned int place, unsigned long long low, unsigned long long high,
                        const unsigned int word[][2], unsigned int count);
void khw_round_carried(void);
unsigned int khw_anchor_cases(unsigned int anchor, int swapped, unsigned int word[][2], unsigned int *plain,
                              unsigned int *signed_read);
unsigned int khw_candidate_answer(const KhwCandidate *candidate, const unsigned int *word);
// whether the questions put now are predicate ones, the store after the slot guarded by P0 (khw_kernel_put)
void khw_store_guarded(int on);
int khw_store_is_guarded(void);
void khw_union_written(void);
// the relation `question` answers, judged against the register relations where `guarded` is 0 and against the
// predicate relations where it is 1, the question having been put with the store guarded by P0
int khw_relation_of(const RunQuestion *question, KhwAnswered *held, int guarded);
// 1 where `question`, put plain, answered the second word on every case: the slot left the register the kernel
// stores alone, as a form that writes a predicate does
int khw_second_word_standing(const RunQuestion *question);
int khw_relation_asked(unsigned long long low, unsigned long long high, KhwAnswered *held);
int khw_first_round(KhwAnswered *kernel, unsigned int *slot, unsigned long long *nanoseconds);
void khw_turns_asked(KhwAnswered *held, const unsigned int word[1][2]);
unsigned int khw_forms_in_phase(unsigned int phase, KhwAnswered **forms);
int khw_turning_words(unsigned int anchor, int swapped, unsigned int word[2], unsigned int *expected);
int khw_predicate_words(unsigned int anchor, int swapped, int signed_read, unsigned int which, unsigned int word[2]);
int khw_places_whole(const unsigned int *place, unsigned int runs, unsigned int words);
int khw_answers_alike(const KhwAnswered *held, const KhwAnswered *other);
int khw_form_stands_for(const KhwAnswered *older, const KhwAnswered *newer);
int khw_form_stood_for(const KhwAnswered *forms, unsigned int count, const KhwAnswered *newer);
unsigned int khw_forms_folded(KhwAnswered *forms, unsigned int count);
unsigned int khw_forms_placed_twice(KhwAnswered **forms, unsigned int count);
unsigned int khw_comparisons_void(KhwAnswered **forms, unsigned int count);
void khw_plan_handed(const char *pass, unsigned long long questions);
unsigned int khw_fields_turns(KhwAnswered *const *forms, unsigned int count);
// the turning cases `held` is turned over: one, or for a predicate form each of khw_predicate_words
unsigned int khw_turning_cases(const KhwAnswered *held);
unsigned int khw_fields_registers(KhwAnswered *const *forms, unsigned int count);
void khw_fields_asked(KhwAnswered *held);
int khw_kernel_read(void);
int khw_form_asked(unsigned long long low, unsigned long long high);
unsigned int khw_seeds_asked(const char *name);
unsigned int khw_widened(unsigned int from, int discover);
unsigned int khw_costs_asked(const char *path);
// the round's line on the console: its count of questions and the clocks the channel spent on it since `before`
void khw_round_printed(unsigned int count, const unsigned long long before[4]);
void khw_fields_written(const char *path);
void khw_enumerate(unsigned int slot, const char *forms);
unsigned int khw_fixed_register(const char *name);
int khw_named_already(unsigned long long low, unsigned long long high);
// the narrowest field of the instruction word, as the part's .ksc gives them, that holds the bits `first` to `last`:
// 1 with its name, into `name` of `room` bytes, its first bit and its width; 0 where the .ksc gives none that does
int khw_field_holding(unsigned int first, unsigned int last, char *name, size_t room, unsigned int *field_first,
                      unsigned int *field_bits);
int khw_field_read(const char *name, unsigned int *first, unsigned int *bits);
// the fields of the instruction word the part's .ksc gives, each its first bit and its width, into `first` and
// `bits`, which hold `room`: their count
#define KHW_FIELDS 32u
unsigned int khw_fields_listed(unsigned int *first, unsigned int *bits, unsigned int room);
// The runs of `held` gathered from `turned`, one entry a bit up to KHW_TURN_LAST, 1 where the bit's turn changed the
// answer: the adjacent turned bits, cut where the fields the part names end, `fields` of them with their first bits
// and widths. Two adjacent turned bits are one run where one field holds both, the narrowest where fields nest, or
// none holds either. Each run holds no place and a register's kind. The count of runs
unsigned int khw_runs_gathered(const int *turned, unsigned int fields, const unsigned int *field_first,
                               const unsigned int *field_bits, KhwAnswered *held);
unsigned int khw_frame_pooled(unsigned long long low, unsigned long long high, const unsigned int word[][2],
                              unsigned int count);
const unsigned int (*khw_frame_cases(unsigned int *count))[2];
void khw_frame_stalled(unsigned long long *low, unsigned long long *high);
int khw_frame_of(const RunQuestion *question, KhwAnswered *held);
unsigned int khw_frame_round(const KhwAnswered *slot_form, int discover);

// What one form holds through a stage of a pass: the form, its turning case `word` and the answer `expected` of it,
// where its questions begin in the round being written, and the answer the part gave its turning case `base`. A
// ladder form's questions carry one case, read at 0; a frame form's carry FRAME_READ_AT + 1 cases of its word pair,
// read at FRAME_READ_AT, the thread whose lane and block differ from each other and from 0. A predicate form is turned
// over each of its turning cases (khw_predicate_words), `turning` the one this stage holds
typedef struct
{
    KhwAnswered *held;
    unsigned int word[1][2];
    unsigned long long expected;
    unsigned int at;
    unsigned long long base;
    unsigned int count;
    unsigned int read_at;
    unsigned int turning;
} KhwStaged;

int khw_turning_case(KhwAnswered *held, KhwStaged *stage);
void khw_ladder_registers_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage);
void khw_frame_addresses_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage);
void khw_frame_registers_read(KhwAnswered *held, unsigned int at, const KhwStaged *stage);
void khw_frame_cases_written(void);
void khw_frame_nothing_read(const RunQuestion *question);
unsigned int khw_run_settings(const KhwAnswered *held, unsigned int stage, unsigned int run,
                              unsigned int *settings);
unsigned int khw_slot_fields(unsigned int *first, unsigned int *bits);
unsigned int khw_staged_pooled(const KhwStaged *stage, unsigned long long low, unsigned long long high);
extern unsigned int s_frame_word[FRAME_CASES][2];

#endif
