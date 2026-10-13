// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// unprinted_internal.h: what the modules of the unprinted probe share: its includes, the bounds of a container, its code, text and output, the fixed operands and the questions, the machine, runner, folder, kernel, pattern and container held, and the functions one module reads from another
#ifndef UNPRINTED_INTERNAL_H
#define UNPRINTED_INTERNAL_H

#include "../../cubin_write.h"
#include "../../cubin_safe.h"
#include "../../sass_assemble.h"
#include "../../../../lstar/interface/interface.h"
#include "../../sass_machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most bytes a cubin, its code, a text and a run's output take
#define UNPRINTED_CUBIN_BYTES 262144u

#define UNPRINTED_CODE_BYTES 65536u

#define UNPRINTED_TEXT_BYTES 65536u

#define UNPRINTED_OUTPUT_BYTES 65536u

#define UNPRINTED_EXITS 256u

// a run's limit: one load and one launch, far under a second, and a turned encoding that leaves the part waiting
// is ended here and read as not run
#define UNPRINTED_LIMIT 10000000ull

// the widest field turned through every value, and the most bits a question holds fixed beside it
#define UNPRINTED_FIELD_MOST 6u

#define UNPRINTED_FIXED 2u

// the words a run answers: the kernel stores R7 as the first and zero as the other three, and a question may store
// over those three
#define UNPRINTED_COPIES 4u

// the registers a thread of every cubin written declares, the most a cubin declares. A kernel refuses a register
// number past the count it declares as an illegal instruction, and the pattern declares 10: a register field is asked
// under a count that holds every number
#define UNPRINTED_REGISTERS 255u

// the bits of the high word that are the operation's, 64 to 104, and not the scheduler's
#define UNPRINTED_OPERATION_HIGH 0x1ffffffffffull

// bits a question holds at one value while the field is turned
typedef struct
{
    unsigned int first;
    unsigned int bits;
    unsigned long long value;
} UnprintedFixed;

// one question: the lines put in place of the kernel's IADD3, the one among them whose bits are turned, the field
// turned, the bits held beside it, and the word the printed text says the part answers
typedef struct
{
    const char *lines;
    const char *turned;
    unsigned int first;
    unsigned int bits;
    UnprintedFixed fixed[UNPRINTED_FIXED];
    unsigned int answer;
} UnprintedQuestion;

// P1 is set true and P2 false before every question that reads a predicate, so that a field naming one reads a value
// known on each side
#define UNPRINTED_PREDICATES "ISETP.NE.U32.AND P1, PT, R0, RZ, PT\nISETP.NE.U32.AND P2, PT, RZ, RZ, PT\n"

// the load of the case's first word and the store of R0 into the answer's third word, each through the descriptor
// register the kernel loaded
#define UNPRINTED_LOAD "LDG.E.CONSTANT R7, term[UR4][R2.64]"

#define UNPRINTED_STORE "STG.E term[UR4][R4.64+0x8], R0"

#define UNPRINTED_ATOMIC "ATOMG.E.ADD.64.STRONG.GPU PT, R8, term[UR4][R4.64+0x8], R0"

// the word an atomic found stored as the answer's second word, and the third word it wrote loaded into R7
#define UNPRINTED_ATOMIC_READ "\nSTG.E term[UR4][R4.64+0x4], R8\nLDG.E.CONSTANT R7, term[UR4][R4.64+0x8]"

// every predicate from P0 to P6 set false, and every one set true
#define UNPRINTED_FALSE                                                                                                \
    "ISETP.NE.U32.AND P0, PT, RZ, RZ, PT\nISETP.NE.U32.AND P1, PT, RZ, RZ, PT\nISETP.NE.U32.AND P2, PT, RZ, RZ, PT\n"  \
    "ISETP.NE.U32.AND P3, PT, RZ, RZ, PT\nISETP.NE.U32.AND P4, PT, RZ, RZ, PT\nISETP.NE.U32.AND P5, PT, RZ, RZ, PT\n"  \
    "ISETP.NE.U32.AND P6, PT, RZ, RZ, PT\n"
#define UNPRINTED_TRUE                                                                                                 \
    "ISETP.NE.U32.AND P0, PT, R0, RZ, PT\nISETP.NE.U32.AND P1, PT, R0, RZ, PT\nISETP.NE.U32.AND P2, PT, R0, RZ, PT\n"  \
    "ISETP.NE.U32.AND P3, PT, R0, RZ, PT\nISETP.NE.U32.AND P4, PT, R0, RZ, PT\nISETP.NE.U32.AND P5, PT, R0, RZ, PT\n"  \
    "ISETP.NE.U32.AND P6, PT, R0, RZ, PT\n"

// The questions of the machine file's open item. The case is 0xb and 0x7, and R2 holds the case's address, R4 the
// answer's, UR4 the memory descriptor the kernel loaded from c[0x0][0x118].
static const UnprintedQuestion s_questions[] = {
    // IMAD's carry-in predicate, which IMAD reads as IMAD.X and prints nowhere else
    {UNPRINTED_PREDICATES "IMAD.IADD R7, R0, 0x1, R7", "IMAD.IADD R7, R0, 0x1, R7", 87u, 4u, {{0u, 0u, 0ull}},
     0x00000012u},
    {UNPRINTED_PREDICATES "IMAD.IADD R7, R0, 0x1, -R7", "IMAD.IADD R7, R0, 0x1, -R7", 87u, 4u, {{0u, 0u, 0ull}},
     0x00000004u},
    // ISETP's predicate beside its printed ones, which ISETP reads as .EX, asked where the comparison holds and where
    // it does not
    {UNPRINTED_PREDICATES "ISETP.GE.U32.AND P0, PT, R0, R7, PT\nSEL R7, R0, RZ, P0",
     "ISETP.GE.U32.AND P0, PT, R0, R7, PT", 68u, 4u, {{0u, 0u, 0ull}}, 0x0000000bu},
    {UNPRINTED_PREDICATES "ISETP.GE.U32.AND P0, PT, R7, R0, PT\nSEL R7, R0, RZ, P0",
     "ISETP.GE.U32.AND P0, PT, R7, R0, PT", 68u, 4u, {{0u, 0u, 0ull}}, 0x00000000u},
    // the load's predicate at 64 to 67, its number inverted at 64 to 66 and bit 67 negating it, asked with every
    // predicate false and then with every predicate true
    {UNPRINTED_FALSE UNPRINTED_LOAD, UNPRINTED_LOAD, 64u, 4u, {{0u, 0u, 0ull}}, 0x0000000bu},
    {UNPRINTED_TRUE UNPRINTED_LOAD, UNPRINTED_LOAD, 64u, 4u, {{0u, 0u, 0ull}}, 0x0000000bu},
    // the load's predicate printed, each assembled into the field at 64 and run as written: P1 is true and P2 false
    {UNPRINTED_PREDICATES UNPRINTED_LOAD ", P1", UNPRINTED_LOAD ", P1", 0u, 0u, {{0u, 0u, 0ull}}, 0x0000000bu},
    {UNPRINTED_PREDICATES UNPRINTED_LOAD ", P2", UNPRINTED_LOAD ", P2", 0u, 0u, {{0u, 0u, 0ull}}, 0x00000000u},
    {UNPRINTED_PREDICATES UNPRINTED_LOAD ", !P1", UNPRINTED_LOAD ", !P1", 0u, 0u, {{0u, 0u, 0ull}}, 0x00000000u},
    {UNPRINTED_PREDICATES UNPRINTED_LOAD ", !P2", UNPRINTED_LOAD ", !P2", 0u, 0u, {{0u, 0u, 0ull}}, 0x0000000bu},
    {UNPRINTED_PREDICATES UNPRINTED_LOAD ", !PT", UNPRINTED_LOAD ", !PT", 0u, 0u, {{0u, 0u, 0ull}}, 0x00000000u},
    // the load's descriptor register, with bit 101 clear and with it set
    {UNPRINTED_LOAD, UNPRINTED_LOAD, 32u, 6u, {{101u, 1u, 0ull}}, 0x0000000bu},
    {UNPRINTED_LOAD, UNPRINTED_LOAD, 32u, 6u, {{101u, 1u, 1ull}}, 0x0000000bu},
    // the two bits past the load's descriptor register
    {UNPRINTED_LOAD, UNPRINTED_LOAD, 38u, 2u, {{0u, 0u, 0ull}}, 0x0000000bu},
    // NVIDIA's 64-bit atomic add, which the resident runs, at every value of bits 64 to 71, where its compare and swap
    // holds the second register it reads and its descriptor register sits: R0 and R1 added to the answer's third and
    // fourth words, the word it found stored as the second, and the third read back
    {UNPRINTED_ATOMIC UNPRINTED_ATOMIC_READ, UNPRINTED_ATOMIC, 64u, 8u, {{0u, 0u, 0ull}}, 0x0000000bu},
    // the same with 64 to 71 held at 100, R100 and R101 given known words first: 0 and 0, 4 and 0, 0 and 1. Where the
    // pair were read into the address, 4 would move the add to the fourth word and 1 in the high word past every
    // word the question holds
    {"IMAD.MOV.U32 R100, RZ, RZ, 0x0\nIMAD.MOV.U32 R101, RZ, RZ, 0x0\n" UNPRINTED_ATOMIC UNPRINTED_ATOMIC_READ,
     UNPRINTED_ATOMIC, 64u, 0u, {{64u, 8u, 100ull}}, 0x0000000bu},
    {"IMAD.MOV.U32 R100, RZ, RZ, 0x4\nIMAD.MOV.U32 R101, RZ, RZ, 0x0\n" UNPRINTED_ATOMIC UNPRINTED_ATOMIC_READ,
     UNPRINTED_ATOMIC, 64u, 0u, {{64u, 8u, 100ull}}, 0x0000000bu},
    {"IMAD.MOV.U32 R100, RZ, RZ, 0x0\nIMAD.MOV.U32 R101, RZ, RZ, 0x1\n" UNPRINTED_ATOMIC UNPRINTED_ATOMIC_READ,
     UNPRINTED_ATOMIC, 64u, 0u, {{64u, 8u, 100ull}}, 0x0000000bu},
    // the store's descriptor register with bit 101 clear, and the two bits past it, read back through a load
    {UNPRINTED_STORE "\nLDG.E.CONSTANT R7, term[UR4][R4.64+0x8]", UNPRINTED_STORE, 64u, 6u, {{101u, 1u, 0ull}},
     0x0000000bu},
    {UNPRINTED_STORE "\nLDG.E.CONSTANT R7, term[UR4][R4.64+0x8]", UNPRINTED_STORE, 70u, 2u, {{0u, 0u, 0ull}},
     0x0000000bu},
};

extern SassMachine s_machine;
extern const char *s_runner;
extern const char *s_folder;
extern char s_kernel[128];
extern unsigned long long s_pattern_size;
extern unsigned char s_pattern[UNPRINTED_CUBIN_BYTES];
extern unsigned char s_code[UNPRINTED_CODE_BYTES];
extern unsigned char s_turned[UNPRINTED_CODE_BYTES];
extern char s_kernel_text[UNPRINTED_TEXT_BYTES];
extern char s_asking[UNPRINTED_TEXT_BYTES];

unsigned long long unprinted_file_read(const char *path, unsigned char *bytes, unsigned long long room);
unsigned long long unprinted_bits_read(unsigned long long low, unsigned long long high, unsigned int first, unsigned int bits);
void unprinted_binary(unsigned long long value, unsigned int bits, char *text);
int unprinted_text(const char *kernel_text, const char *lines, char *text, size_t room);
int unprinted_kernel(const char *kernel_text, char *kernel, size_t room);
unsigned int unprinted_find(const unsigned char *code, unsigned int count, unsigned long long low, unsigned long long high, unsigned int *found, unsigned int room);
void unprinted_place_write(unsigned char *code, unsigned int place, const UnprintedFixed *fixed, unsigned int fixed_count, unsigned int first, unsigned int bits, unsigned long long value);
int unprinted_run(const unsigned char *code, unsigned long long code_size, unsigned int *answered, char *ended, size_t room);
int unprinted_ask(const UnprintedQuestion *question, unsigned int number, FILE *record);
void unprinted_forms(FILE *record);

#endif
