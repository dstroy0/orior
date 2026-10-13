// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stick_internal.h: what the modules of the stick's reader share: its includes, the spaces and the prefix, the numbers, kernel and engine structures, the names held and hashed, the kernels and engines read, and the functions one module reads from another
#ifndef STICK_INTERNAL_H
#define STICK_INTERNAL_H

#include "../../../../types/file_defs/ktext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the letters Python's split takes as space
static const char s_space[] = " \t\n\r\v\f\x1c\x1d\x1e\x1f";

// the start of each kernel's name
static const char s_prefix[] = "measuring_stick_";

// a list of numbers, each an operation's place in the table of names
typedef struct
{
    unsigned int *number;
    unsigned int count;
    unsigned int room;
} StickNumbers;

// a kernel: its name, nvcc's operations, and where the manifest names it, its category and its function
typedef struct
{
    char *name;
    StickNumbers operations;
    const char *category;
    const char *function;
} StickKernel;

// a kernel's line of the engine's run: its answer, its steps, its note, and where it answers, its operations as
// nvdisasm reads them back, how many agree with the lane's text position by position, and the count of the text's
typedef struct
{
    char *name;
    char *answer;
    unsigned int steps;
    char *note;
    StickNumbers operations;
    unsigned int agree;
    unsigned int text;
} StickEngine;

#define STICK_HASH 65536u

extern char **s_name;
extern unsigned int s_names;
extern StickKernel *s_kernel;
extern unsigned int s_kernels;
extern StickEngine *s_engine;
extern unsigned int s_engines;
extern KtextFile s_file;

void *stick_held(void *pointer);
char *stick_copy(const char *text, size_t length);
void stick_number_add(StickNumbers *numbers, unsigned int number);
unsigned int stick_named(const char *text, size_t length);
void stick_read(const char *path);
const char *stick_spaced(const char *text);
size_t stick_guard(const char *text);
size_t stick_form_guard(const char *text);
int stick_operation_of(const char *line, unsigned int *operation);
void stick_nvcc_read(const char *path);
unsigned int stick_cells(const char *line, char **cell, unsigned int most);
void stick_engine_read(const char *table, const char *directory);
void stick_ruleset_operations(const char *path, unsigned char **held, unsigned int *room);
const StickEngine *stick_engine_of(const char *name);
int stick_kernel_order(const void *left, const void *right);
int stick_name_order(const void *left, const void *right);
void stick_escaped(QryText *record, const char *text);
void stick_counted(const StickNumbers *operations, unsigned int *count);
void stick_counts_written(QryText *record, const unsigned int *order, const unsigned int *count, const unsigned int *less);

#endif
