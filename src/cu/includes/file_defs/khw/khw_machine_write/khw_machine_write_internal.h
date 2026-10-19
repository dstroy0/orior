// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_machine_write_internal.h: what the modules of the machine writer share: its includes, the bounds of the pattern,
// the kernel and the pieces, the pieces, the pattern, the kernel and the machine read, and the functions one module
// reads from another
#ifndef KHW_MACHINE_WRITE_INTERNAL_H
#define KHW_MACHINE_WRITE_INTERNAL_H

#include "../../../../includes/file_defs/kernel_blob/forms_blob.h"
#include "../../../../includes/file_defs/kernel_blob/kernel_blob.h"
#include "../../../../transpiler/vendor_bin_layouts/nvidia/cubin_write.h"
#include "../../../../transpiler/vendor_bin_layouts/nvidia/sass_machine.h"
#include "../../../parsers/line_read.h"
#include "../../qry/writer/qry_member.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most bytes a container takes, the most places its code holds, the bytes of one instruction, and the most
// endings the container records
#define KHW_PATTERN_BYTES 262144u

#define KHW_PLACES 256u

#define KHW_INSTRUCTION 16u

#define KHW_EXITS 64u

// the most pieces the vendor's table holds, and the longest piece and the longest column of reads in it
#define KHW_PIECES 128u

#define KHW_PIECE 32u

#define KHW_READS 256u

// the longest name a form is composed
#define KHW_NAME 64u

// the register number the part answers nothing at, which a form prints by the name the tree writes it under
#define KHW_REGISTER_NONE 255u

#define KHW_REGISTER_NONE_TEXT "RZ"

// the qualifiers the answers decide between where the two readings of a case differ
#define KHW_PIECE_UNSIGNED "U32"

#define KHW_PIECE_SIGNED "S32"

// what a run's operand is, as the protocol answered it beside its place: a register, a pair of registers an address
// is read through, a number, or a predicate; and the letter each is written as after the place in the answers
#define KHW_KIND_REGISTER 0u

#define KHW_KIND_ADDRESS 1u

#define KHW_KIND_IMMEDIATE 2u

#define KHW_KIND_PREDICATE 3u
// a run of the constant bank's operand: the offset read from the bank, and the bank, where the walks found a run of it
#define KHW_KIND_CONSTANT 4u

#define KHW_KIND_LETTERS "ranpc"

// What the protocol answered of one form, read from the answers file: the relation every case of it answered, whether
// the answers read a word signed and whether a case of the relation let the part answer so, the encoding, and the
// runs of bits its operands sit in with each run's place in the relation's tuple and what its operand is
typedef struct
{
    char relation[KHW_READS];
    int signed_read;
    int signedness_asked;
    unsigned long long low;
    unsigned long long high;
    unsigned int runs;
    unsigned int first[SASS_MACHINE_RUNS];
    unsigned int last[SASS_MACHINE_RUNS];
    unsigned int place[SASS_MACHINE_RUNS];
    unsigned int kind[SASS_MACHINE_RUNS];
} KhwForm;

extern unsigned char s_pattern[KHW_PATTERN_BYTES];
extern unsigned long long s_pattern_size;
extern char s_kernel[128];
extern unsigned int s_registers;
extern unsigned char s_kernel_text[KHW_PLACES * KHW_INSTRUCTION];
extern unsigned int s_kernel_places;
extern SassMachine s_machine;

unsigned long long khw_word_read(const unsigned char *instruction, unsigned int byte);
int khw_pieces_read(const char *path);
const char *khw_piece_reading(const char *kind, const char *word);
// the field the part's .ksc names the guard, `first` and `bits`: a predicate run inside it prints as the guard, and
// one outside it as an operand in its order; with none set, every predicate run is the guard
void khw_guard_field_set(unsigned int first, unsigned int bits);
void khw_text_composed(const KhwForm *held, char *written, size_t room);
int khw_form_kept(SassMachine *machine, const KhwForm *held, const char *text);
int khw_kernel_read(const char *path);
unsigned int khw_kernel_forms_kept(void);
unsigned int khw_answers_read(const char *answers, KhwForm *form, unsigned int room);
// the blob the texts the protocol hands are handed back in, assembled, and the most instructions a text holds
#define KHW_ASSEMBLED "assembled.bin"

#define KHW_ASSEMBLED_MOST 4096u

// the most texts one call assembles, the most bytes one text holds, and the blob the reason goes in where a call's
// texts were not assembled
#define KHW_ASSEMBLED_TEXTS 256u
#define KHW_ASSEMBLED_TEXT_BYTES (1u << 20u)
#define KHW_ASSEMBLED_SAID "assembled_said.txt"

// the texts of the blob `answers` assembled against the machine file at `path` and appended as KHW_ASSEMBLED
int khw_texts_assembled(const char *path, const char *answers);

#endif
