// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// writings_internal.h: what the modules of the writings probe share: its includes, the bounds of a container, its code and text, the registers the cases are written in, the anchors' and precepts' texts, the machine, pattern, container and kernel held, and the functions one module reads from another
#ifndef WRITINGS_INTERNAL_H
#define WRITINGS_INTERNAL_H

#include "../../../../../engine/rmc/word_web.h"
#include "../../../../../types/file_defs/kdm/kdm.h"
#include "../../../../../types/file_defs/qry/qry_buffer.h"
#include "../../../../lstar/protocol/counterexample/ladder.h"
#include "../../cubin_safe.h"
#include "../../cubin_write.h"
#include "../../sass_assemble.h"
#include "../../sass_machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most bytes a cubin, its code and a text take, the most exits a code holds, and the registers every cubin
// declares a thread
#define WRITINGS_CUBIN_BYTES 262144u

#define WRITINGS_CODE_BYTES 65536u

#define WRITINGS_TEXT_BYTES 65536u

#define WRITINGS_EXITS 256u

#define WRITINGS_REGISTERS 255u

// the registers the case's two words are moved onto, each with a zero beside it for a form that reads a pair, and the
// register the form's result is moved to, which the kernel then stores as the answer
#define WRITINGS_LEFT "R10"

#define WRITINGS_RIGHT "R12"

#define WRITINGS_RESULT "R8"

// the most cases put at once, one thread each, the most relations a form is read against, and the longest line read
// back
#define WRITINGS_CASES 256u

#define WRITINGS_RELATIONS 64u

#define WRITINGS_LINE 4096u

// the lines a form is run between: the case's words, which the kernel leaves in R0 and R7, moved onto the sources, the
// result's register cleared, and every predicate the form may read set false. After the form its result is moved to
// R7, which the kernel's own store writes as the answer's first word
#define WRITINGS_HEAD                                                                                                  \
    "IMAD.MOV.U32 R10, RZ, RZ, R0\nMOV R11, RZ\nIMAD.MOV.U32 R12, RZ, RZ, R7\nMOV R13, RZ\nMOV R8, RZ\nMOV R9, RZ\n"  \
    "ISETP.NE.AND P0, PT, RZ, RZ, PT\nISETP.NE.AND P1, PT, RZ, RZ, PT\nISETP.NE.AND P2, PT, RZ, RZ, PT\n"              \
    "ISETP.NE.AND P3, PT, RZ, RZ, PT\nISETP.NE.AND P4, PT, RZ, RZ, PT\nISETP.NE.AND P5, PT, RZ, RZ, PT\n"              \
    "ISETP.NE.AND P6, PT, RZ, RZ, PT\n"
#define WRITINGS_TAIL "\nIMAD.MOV.U32 R7, RZ, RZ, R8"

#define LADDER_TEXT(name_, text_, words_, measured_) text_,

static const char *const s_anchor_text[] = {LADDER_ANCHORS(LADDER_TEXT)};

#undef LADDER_TEXT

#define PRECEPT_TEXT(name_, text_, arity_) text_,

static const char *const s_precept_text[] = {PRECEPTS(PRECEPT_TEXT)};

#undef PRECEPT_TEXT

// what a form is read against: a relation of the ladder, answered by ladder_answer, or a precept of the alphabet,
// answered by precept_applied
typedef struct
{
    const char *name;
    int precept;
    unsigned int which;
} WritingsRelation;

// the forms a search wrote, the answers the part gave them, and which relations each form holds
#define WRITINGS_FORMS_MOST 16384u

// the register each node of a chain writes its word to, the root's being WRITINGS_RESULT, and the register a word of
// ones is set in for a chain that reads one
#define WRITINGS_NODE_FIRST 16u

#define WRITINGS_ONES "R24"

#define WRITINGS_CHAIN_HEAD "IMAD.MOV.U32 R24, RZ, RZ, 4294967295\n"

// the most nodes a chain is written with, and the most chains of a .kdm written
#define WRITINGS_CHAIN_NODES 8u

#define WRITINGS_CHAINS_MOST 8192u

extern SassMachine s_machine;
extern unsigned char s_pattern[WRITINGS_CUBIN_BYTES];
extern unsigned char s_code[WRITINGS_CODE_BYTES];
extern char s_kernel_text[WRITINGS_TEXT_BYTES];
extern char s_asking[WRITINGS_TEXT_BYTES];
extern char s_kernel[128];
extern unsigned long long s_pattern_size;
extern unsigned int s_case[WRITINGS_CASES][2];
extern unsigned int s_cases;
extern WritingsRelation s_relation[WRITINGS_RELATIONS];
extern unsigned int s_relations;
extern char s_forms[WRITINGS_FORMS_MOST][256];
extern unsigned int s_form_count;
extern unsigned char s_ran[WRITINGS_FORMS_MOST];
extern unsigned char s_holds[WRITINGS_FORMS_MOST][WRITINGS_RELATIONS];

unsigned long long writings_file_read(const char *path, unsigned char *bytes, unsigned long long room);
void writings_relations(void);
unsigned int writings_expected(const WritingsRelation *relation, const unsigned int *word);
void writings_cases(void);
int writings_kernel(const char *kernel_text, char *kernel, size_t room);
int writings_splice(const char *kernel_text, const char *lines, char *text, size_t room);
int writings_form_fits(const SassForm *form);
unsigned int writings_table(const SassForm *form);
unsigned int writings_sources(const SassForm *form);
unsigned int writings_digit(unsigned int assignment, unsigned int source);
unsigned int writings_in_order(unsigned int sources);
int writings_instruction(const SassForm *form, unsigned int table, unsigned int value, unsigned int assignment, char *out, size_t room);
int writings_cubin(unsigned int count, const char *path);
int writings_write(const char *pattern, const char *kernel_text, const char *machine, const char *folder);
void writings_answers_read(FILE *answers, unsigned int count, unsigned int (*answered)[WRITINGS_CASES], unsigned char *ran_one, unsigned int *ran, unsigned int *refused);
int writings_load(const char *folder);
int writings_read(const char *folder, const char *record);
int writings_chains_write(const char *pattern, const char *kernel_text, const char *machine, const char *folder, const char *kdm, const char *into);
int writings_chains_read(const char *folder, const char *record);
int writings_descent_read(const char *record, unsigned int pairs, char **folders);
int writings_krs_read(const char *folder, const char *ruleset, const char *record);

#endif
