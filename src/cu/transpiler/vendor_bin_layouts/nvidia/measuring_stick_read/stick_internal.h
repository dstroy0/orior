// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stick_internal.h: what the modules of the stick's reader share: its includes, the spaces and the prefix, the bounds
// of its texts, the records of an operation, a kernel and an engine's line, the tables the reader keeps, and the
// functions one module reads from another
#ifndef STICK_INTERNAL_H
#define STICK_INTERNAL_H

#include "../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_linea.h"

#include <stddef.h>

// the characters Python's split takes as space
static const char s_space[] = " \t\n\r\v\f\x1c\x1d\x1e\x1f";

// the start of each kernel's name
static const char s_prefix[] = "measuring_stick_";

// The bytes of the texts a record holds inline, each with its terminator. A text past its bound ends the run with the
// reason, and the reader never cuts a text short
#define STICK_WORD_BYTES 64u
#define STICK_CATEGORY_BYTES 64u
#define STICK_FUNCTION_BYTES 512u
#define STICK_ANSWER_BYTES 16u
#define STICK_NOTE_BYTES 1024u
#define STICK_INSTRUCTION_BYTES 1024u

// the categories of the stick at max
#define STICK_CATEGORIES_MAX 256u

// a file as the run stores it: its bytes, which the run holds, and their count
typedef struct
{
    const char *bytes;
    size_t size;
} StickText;

// a piece of a line: its first byte and its bytes
typedef struct
{
    const char *at;
    size_t length;
} StickPiece;

// one operation written: its name, and its place in the table of names once the table is whole
typedef struct
{
    char text[STICK_WORD_BYTES];
    unsigned int number;
} StickOperation;

// the operations of one text, in their order: `count` of them from `operation`, a range of a cell of the reader's
typedef struct
{
    StickOperation *operation;
    unsigned int count;
} StickOperations;

// a kernel: its name, nvcc's operations, and where the manifest names it, its category and its function
typedef struct
{
    char name[STICK_WORD_BYTES];
    StickOperations operations;
    char category[STICK_CATEGORY_BYTES];
    char function[STICK_FUNCTION_BYTES];
} StickKernel;

// A kernel's line of the engine's run: its answer, its steps and its note. Where it answers, its operations as nvdisasm
// reads them back. Then the count that agree with the lane's text position by position, and the count of the text's
typedef struct
{
    char name[STICK_WORD_BYTES];
    char answer[STICK_ANSWER_BYTES];
    unsigned int steps;
    char note[STICK_NOTE_BYTES];
    StickOperations operations;
    unsigned int agree;
    unsigned int text;
} StickEngine;

// an operation's name in the table of names, in the order of the names
typedef struct
{
    char text[STICK_WORD_BYTES];
} StickName;

// The tables the reader keeps, each one cell of the persistent tier taken once at its count. The names are whole after
// the reader reads every text. Each operation's number is then its name's place among them. The listed operations are
// those of nvcc's listing, and each kernel's operations are a range of them. The ruleset's operations are those the
// forms write
extern StickName *s_name;
extern unsigned int s_names;
extern StickKernel *s_kernel;
extern unsigned int s_kernels;
extern StickEngine *s_engine;
extern unsigned int s_engines;
extern StickOperations s_listed;
extern StickOperations s_ruleset;

void *stick_allocated(void *pointer);
void stick_refused(const char *why, const char *piece, size_t length);
int stick_text_order(const char *left, const char *right);
void stick_sorted(void *elements, unsigned int count, size_t size, int (*order)(const void *left, const void *right));
StickText stick_text_read(const char *path);
size_t stick_line(const StickText *text, size_t at, StickPiece *line);
size_t stick_spaced(const char *text, size_t length);
size_t stick_end_outside(const char *text, size_t length, const char *set);
size_t stick_guard(const char *text, size_t length);
size_t stick_form_guard(const char *text, size_t length);
int stick_operation_of(StickPiece line, StickPiece *word);
int stick_is_nop(StickPiece word);
void stick_put(char *into, size_t room, const char *text, size_t length, const char *what);
StickOperation *stick_operations_taken(unsigned int count);
unsigned int stick_cells(StickPiece line, StickPiece *cell, unsigned int max);
void stick_names_made(void);
void stick_nvcc_read(const char *path);
void stick_engine_read(const char *table, const char *directory);
void stick_ruleset_operations(const char *path);
const StickEngine *stick_engine_of(const char *name);
int stick_kernel_order(const void *left, const void *right);
void stick_escaped(VerbaLinea *line, const char *text);
void stick_counted(const StickOperations *operations, unsigned int *count);
void stick_counts_written(VerbaLinea *line, const unsigned int *count, const unsigned int *less);

#endif
