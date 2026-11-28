// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_learn_internal.h: what the modules of the pass share: its includes, the bounds of a question and a pass,
// the state of the question, the pool, the forms named and the paths given, and the functions one module reads
// from another
#ifndef KHW_LEARN_INTERNAL_H
#define KHW_LEARN_INTERNAL_H

#include "../../../../../../includes/file_defs/khw/khw_write/khw_write.h"
#include "../gate/survivors.h"
#include "../order/scheduler.h"
#include "../query/answer_read.h"
#include "../teacher/run_channel.h"
#include "frame_relation.h"

#include "../../../../../../includes/file_defs/kernel_blob/kernel_blob.h"
#include "../../../../../../includes/file_defs/krf/krf.h"
#include "../../../../../../includes/file_defs/ksc/ksc.h"
#include "../../../vendor_bin_layouts/container_layout.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// the max places a kernel's code contains, and the bytes of one instruction
#define KHW_PLACES 256u

#define KHW_INSTRUCTION 16u

// the bits that a turn reaches. They are past the operation key, which names the operation and which no turn
// changes. They are below the own word of the scheduler, which the program sets once for a run (P9)
#define KHW_TURN_FIRST 12u

#define KHW_TURN_LAST 104u
#define KHW_TURNS (KHW_TURN_LAST - KHW_TURN_FIRST + 1u)

// the settings that the call probes a run with in a stage of the register pass, and the stages
#define KHW_SETTINGS 2u
#define KHW_STAGES 2u

// the two registers that the kernel leaves the words of a case in, which a query found. A run set to one of these
// numbers reads that register, and the two words differ. An operand field answers two different words, and a
// modifier answers one
#define KHW_FIRST_REGISTER 0u

#define KHW_SECOND_REGISTER 7u

// a fixed register the part's .ksc does not name
#define KHW_FIXED_NONE 0xffffffffu

// the launches that the program times the own question of the kernel over, as it reads the cost of a task (P11,
// Step 10)
#define KHW_LAUNCHES 100u

// how many times a costs question writes a form, one copy to a place after the probe place. These are the many links
// of one query, which raise the cost of a form above the own cost of a launch (P6, Q5)
#define KHW_COST_COPIES 2048u

// the max words after `--` that the program opens the channel on, and their end. They are a carrier and the pop and
// cross of the airlock, each of up to eight words, with `--pop` and `--cross` between them (run_channel.h)
#define KHW_CARRIER_WORDS 27u

// one candidate of the relation read: a relation of the ladder and the two words of the case. They read in the order
// that the host put them, or swapped. The answer reads plain or signed
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
// the places of the kernel before the place where the least opening starts. The program drops them from the code of
// every question, and puts them as candidates of the frame round as it puts the kept ones. The part ran each of them
extern unsigned char s_opening_text[KHW_PLACES * KHW_INSTRUCTION];
extern unsigned int s_opening_places;
// the probe place among the retained places where the kernel listing names it, the part's answer in its .ksc, and
// s_kernel_places where it names none
extern unsigned int s_kernel_probe_place;
extern unsigned int s_probe_place;
extern unsigned long long s_unreached_low;
extern unsigned long long s_unreached_high;
extern const char *s_fields;
extern char s_ksc_path[1024];
extern RunQuestion s_question;
extern unsigned long long s_query_count;
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
int khw_question_put(const unsigned int word[][2], unsigned int count, unsigned int place,
                     unsigned long long *answered);
unsigned int khw_pooled(unsigned int place, unsigned long long low, unsigned long long high,
                        const unsigned int word[][2], unsigned int count);
void khw_round_carried(void);
unsigned int khw_anchor_cases(unsigned int anchor, int swapped, unsigned int word[][2], unsigned int *plain,
                              unsigned int *signed_read);
unsigned int khw_candidate_answer(const KhwCandidate *candidate, const unsigned int *word);
// whether the questions put now are predicate ones, the store after the probe place guarded by P0 (khw_kernel_put)
void khw_store_guarded(int on);
int khw_store_is_guarded(void);
void khw_union_written(void);
// the relation that `question` answers. The call judges it against the register relations where `guarded` is 0, and
// against the predicate relations where it is 1. Then the program put the question with P0 as the guard of the store
int khw_relation_of(const RunQuestion *question, KhwAnswered *form, int guarded);
// 1 where `question`, put plain, answered the second word on every case. The probe place did not change the register
// that the kernel stores, as a form that writes a predicate does
int khw_second_word_located(const RunQuestion *question);
int khw_relation_queried(unsigned long long low, unsigned long long high, KhwAnswered *form);
int khw_first_round(KhwAnswered *kernel, unsigned int *place, unsigned long long *nanoseconds);
void khw_turns_queried(KhwAnswered *form, const unsigned int word[1][2]);
unsigned int khw_forms_in_phase(unsigned int phase, KhwAnswered **forms);
int khw_turning_words(unsigned int anchor, int swapped, unsigned int word[2], unsigned int *expected);
int khw_predicate_words(unsigned int anchor, int swapped, int signed_read, unsigned int which, unsigned int word[2]);
int khw_places_whole(const unsigned int *place, unsigned int runs, unsigned int words);
int khw_answers_alike(const KhwAnswered *form, const KhwAnswered *other);
int khw_form_is_for(const KhwAnswered *older, const KhwAnswered *newer);
int khw_form_was_for(const KhwAnswered *forms, unsigned int count, const KhwAnswered *newer);
unsigned int khw_forms_folded(KhwAnswered *forms, unsigned int count);
unsigned int khw_forms_placed_twice(KhwAnswered **forms, unsigned int count);
unsigned int khw_comparisons_void(KhwAnswered **forms, unsigned int count);
void khw_plan_passed(const char *pass, unsigned long long questions);
unsigned int khw_fields_turns(KhwAnswered *const *forms, unsigned int count);
// the turn cases that the program turns `form` over: one, or for a predicate form each of khw_predicate_words
unsigned int khw_turning_cases(const KhwAnswered *form);
unsigned int khw_fields_registers(KhwAnswered *const *forms, unsigned int count);
void khw_fields_queried(KhwAnswered *form);
int khw_kernel_read(void);
int khw_form_queried(unsigned long long low, unsigned long long high);
unsigned int khw_seeds_queried(const char *name);
unsigned int khw_widened(unsigned int from, int discover);
unsigned int khw_costs_queried(const char *path);
// the round's line on the console: its count of questions and the clocks the channel spent on it since `before`
void khw_round_printed(unsigned int count, const unsigned long long before[4]);
void khw_fields_written(const char *path);
void khw_enumerate(unsigned int place, const char *forms);
unsigned int khw_fixed_register(const char *name);
int khw_named_already(unsigned long long low, unsigned long long high);
// the narrowest field of the instruction word, as the .ksc of the part gives them, that holds the bits `first` to
// `last`. The return is 1 with its name, into `name` of `room` bytes, its first bit and its width. It is 0 where the
// .ksc gives none that does
int khw_field_covering(unsigned int first, unsigned int last, char *name, size_t room, unsigned int *field_first,
                       unsigned int *field_bits);
int khw_field_read(const char *name, unsigned int *first, unsigned int *bits);
// the fields of the instruction word the part's .ksc gives, each its first bit and its width, into `first` and
// `bits`, which have room for `room`: their count
#define KHW_FIELDS 32u
unsigned int khw_fields_listed(unsigned int *first, unsigned int *bits, unsigned int room);
// The runs of `form`, gathered from `turned`, one entry for each bit up to KHW_TURN_LAST. An entry is 1 where the turn
// of the bit changed the answer. The runs are the adjacent turned bits, cut where the fields that the part names end:
// `fields` of them, with their first bits and widths. Two adjacent turned bits are one run where one field holds both,
// the narrowest where fields nest, or where no field holds either. Each run has no place, and the type of a register.
// The return is the count of runs
unsigned int khw_runs_gathered(const int *turned, unsigned int fields, const unsigned int *field_first,
                               const unsigned int *field_bits, KhwAnswered *form);
unsigned int khw_frame_pooled(unsigned long long low, unsigned long long high, const unsigned int word[][2],
                              unsigned int count);
const unsigned int (*khw_frame_cases(unsigned int *count))[2];
void khw_frame_stalled(unsigned long long *low, unsigned long long *high);
int khw_frame_of(const RunQuestion *question, KhwAnswered *form);
unsigned int khw_frame_round(const KhwAnswered *probed_form, int discover);

// What one form carries through a stage of a pass. It has the form, its turn case `word` and the answer `expected` of
// it. It has where its questions start in the round that the call now writes, and `base`, the answer of the part to
// its turn case.
//
// The questions of a ladder form carry one case, read at 0. The questions of a frame form carry FRAME_READ_AT + 1 cases
// of its word pair, read at FRAME_READ_AT. That is the thread whose lane and block differ from each other and from 0.
// The program turns a predicate form over each of its turn cases (khw_predicate_words), and `turning` is the turn case
// that this stage carries
typedef struct
{
    KhwAnswered *form;
    unsigned int word[1][2];
    unsigned long long expected;
    unsigned int at;
    unsigned long long base;
    unsigned int count;
    unsigned int read_at;
    unsigned int turning;
} KhwStaged;

int khw_turning_case(KhwAnswered *form, KhwStaged *stage);
void khw_ladder_registers_read(KhwAnswered *form, unsigned int at, const KhwStaged *stage);
void khw_frame_addresses_read(KhwAnswered *form, unsigned int at, const KhwStaged *stage);
void khw_frame_registers_read(KhwAnswered *form, unsigned int at, const KhwStaged *stage);
void khw_frame_cases_written(void);
void khw_frame_nothing_read(const RunQuestion *question);
unsigned int khw_run_settings(const KhwAnswered *form, unsigned int stage, unsigned int run, unsigned int *settings);
unsigned int khw_probe_fields(unsigned int *first, unsigned int *bits);
unsigned int khw_staged_pooled(const KhwStaged *stage, unsigned long long low, unsigned long long high);
extern unsigned int s_frame_word[FRAME_CASES][2];

#endif
