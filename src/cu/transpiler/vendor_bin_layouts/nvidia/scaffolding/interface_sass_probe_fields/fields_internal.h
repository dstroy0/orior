// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fields_internal.h: what the modules of the fields probe share: its includes, the bounds of a container, its code and text, the operation bits and the registers, the machine, pattern, container, code, kernel and keys held, and the functions one module reads from another
#ifndef FIELDS_INTERNAL_H
#define FIELDS_INTERNAL_H

#include "../../cubin_write.h"
#include "../../sass_assemble.h"
#include "../../../../lstar/interface/interface.h"
#include "../../sass_machine.h"
#include "../interface_sass_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most bytes a cubin, its code and a text take, and the most exits a code holds
#define FIELDS_CUBIN_BYTES 262144u

#define FIELDS_CODE_BYTES 65536u

#define FIELDS_TEXT_BYTES 65536u

#define FIELDS_EXITS 256u

// the operation bits of an instruction, 0 to 104, and the scheduler's at 105 and above, which this never turns
#define FIELDS_OPERATION_BITS 105u

// the registers a thread of every cubin declares, the most a cubin declares: a register field reaches every number
// the part gives a kernel's code
#define FIELDS_REGISTERS 255u

// the lines a form is run in: the instruction with its result moved to R8, the register the kernel stores as the answer,
// and its source registers moved to R10 up. Each source register is set to a distinct value with a zero beside it:
// turning a bit of a source's field reaches a register of another value and changes the answer. R6 keeps the case's
// second word, which the kernel leaves in R7. P0 to P6 are each set true or false by whether their number holds an odd
// count of one bits, as PT, P7, does: turning any one bit of a predicate field reaches a predicate of the other value,
// and a predicate source or a guard changes the answer
#define FIELDS_LINES_HEAD                                                                                              \
    "MOV R10, 0xb\nMOV R11, RZ\nMOV R12, 0x7\nMOV R13, RZ\nMOV R14, 0x3\nMOV R15, RZ\n"                                \
    "MOV R16, 0x5\nMOV R17, RZ\nMOV R18, 0x2\nMOV R19, RZ\nMOV R20, 0x9\nMOV R21, RZ\n"                                \
    "ISETP.EQ.AND P0, PT, R10, RZ, PT\nISETP.NE.AND P1, PT, R10, RZ, PT\nISETP.NE.AND P2, PT, R10, RZ, PT\n"           \
    "ISETP.EQ.AND P3, PT, R10, RZ, PT\nISETP.NE.AND P4, PT, R10, RZ, PT\nISETP.EQ.AND P5, PT, R10, RZ, PT\n"           \
    "ISETP.EQ.AND P6, PT, R10, RZ, PT\n"                                                                               \
    "IMAD.MOV.U32 R6, RZ, RZ, R7\n"
#define FIELDS_LINES_TAIL "\nIMAD.MOV.U32 R7, RZ, RZ, R8\nSTG.E term[UR4][R4.64], R7"

// the line after a form that sets a predicate, its first operand moved to P0: R8 takes R12's 0x7 where P0 holds and
// R10's 0xb where it does not, which carries the predicate to the answer as one of two values
#define FIELDS_LINES_PREDICATE "\nSEL R8, R12, R10, P0"

// the registers a form's source operands are moved onto, in order, each set in the head above
#define FIELDS_SOURCES 6u

// the register pair a global load's address is moved onto: the kernel leaves R2 and R3 holding the address of the
// thread's case, which the head does not touch, and a load from it reads the case's words
#define FIELDS_LOAD_ADDRESS "R2"

extern SassMachine s_machine;
extern unsigned char s_pattern[FIELDS_CUBIN_BYTES];
extern unsigned char s_code[FIELDS_CODE_BYTES];
extern char s_kernel_text[FIELDS_TEXT_BYTES];
extern char s_asking[FIELDS_TEXT_BYTES];
extern char s_kernel[128];
extern unsigned long long s_pattern_size;

unsigned long long fields_file_read(const char *path, unsigned char *bytes, unsigned long long room);
unsigned long long fields_word_read(const unsigned char *bytes);
int fields_kernel(const char *kernel_text, char *kernel, size_t room);
int fields_splice(const char *kernel_text, const char *lines, char *text, size_t room);
unsigned int fields_find(const unsigned char *code, unsigned int count, unsigned long long low, unsigned long long high);
int fields_cubin(const unsigned char *code, unsigned int count, unsigned int place, unsigned int bit, const char *path);
void fields_keys(void);
int fields_flip_safe(unsigned int place, unsigned int bit, char *name, size_t room);
int fields_first(const char *path, char *instruction, size_t room);
unsigned int fields_operand_at(const SassForm *form, unsigned int bit);
int fields_read(const char *machine_path, const char *instructions, const char *answers, const char *record);
int fields_redirect(const char *instruction, char *redirected, size_t room, char *observe, size_t observe_room);
int fields_names(const char *machine_path, const char *instructions);

#endif
