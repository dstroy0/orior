// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_machine_write_internal.h: what the modules of the machine writer share: its includes, the bounds of the pattern, the kernel and the pieces, the pieces, the pattern, the kernel and the machine read, and the functions one module reads from another
#ifndef KHW_MACHINE_WRITE_INTERNAL_H
#define KHW_MACHINE_WRITE_INTERNAL_H

#include "../../../../types/file_defs/qry/qry_buffer.h"
#include "../cubin_write.h"
#include "../sass_machine.h"

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

// what the vendor's table calls the reads that put a piece in a name: a relation of the ladder, and the word of the
// high order language for a word read as an unsigned integer, the kind the ladder's cases hold
#define KHW_READS_WORD "hol.u<n>_t"

// the qualifiers the answers decide between where the two readings of a case differ
#define KHW_PIECE_UNSIGNED "U32"

#define KHW_PIECE_SIGNED "S32"

// What the protocol answered of one form, read from the answers file: the relation every case of it answered, whether
// the answers read a word signed and whether a case of the relation let the part answer so, the encoding, and the
// runs of bits its operands sit in with each run's place in the relation's tuple
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
} KhwForm;

// What a mode writes for the protocol, gathered in memory before it is given: the bytes, their count and the room
// they have, and 0 once a line did not fit
typedef struct
{
    char *bytes;
    size_t size;
    size_t room;
    int whole;
} KhwListing;

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
void khw_text_composed(const KhwForm *held, char *written, size_t room);
int khw_form_kept(SassMachine *machine, const KhwForm *held, const char *text);
int khw_kernel_read(const char *path);
unsigned int khw_kernel_forms_kept(void);
unsigned int khw_answers_read(const char *answers, KhwForm *form, unsigned int room);
void khw_listed(KhwListing *listing, const char *format, ...);
int khw_listing_given(KhwListing *listing, const char *answers);

#endif
