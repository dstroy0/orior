// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scheduler_internal.h: what the modules of the monolith's scheduler share: its includes, the bounds of a container, a section and the operations, an instruction and an operation as read, the machine, container, section and operations held, and the functions one module reads from another
#ifndef MONOLITH_SCHEDULER_INTERNAL_H
#define MONOLITH_SCHEDULER_INTERNAL_H

#include "../../sass_assemble.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most bytes a cubin takes, the most instructions a section holds, the most operations kept apart, the most
// registers an instruction reads, and the farthest a reader is looked for past the instruction it reads from
#define SCHEDULER_CUBIN_BYTES 4194304u

#define SCHEDULER_SECTION_INSTRUCTIONS 65536u

#define SCHEDULER_OPERATIONS 2048u

#define SCHEDULER_READS 16u

#define SCHEDULER_REACH 32u

// a register number no instruction names, for an instruction that writes none
#define SCHEDULER_NO_REGISTER 0xffffffffu

// one instruction of a section, its scheduler's fields read apart and the registers it writes and reads
typedef struct
{
    unsigned int operation;
    unsigned int stall;
    unsigned int write_barrier;
    unsigned int read_barrier;
    unsigned int wait;
    unsigned int control;
    unsigned int writes;
    unsigned int reads;
    unsigned int read[SCHEDULER_READS];
} SchedulerInstruction;

// one operation as NVIDIA wrote it, everything gathered over every place it was written
typedef struct
{
    char name[SASS_MACHINE_TOKEN];
    unsigned long long count;
    unsigned long long stalls[16];
    unsigned long long yields;
    unsigned long long write_barriers;
    unsigned long long read_barriers;
    unsigned long long waits;
    unsigned int soonest_read;
    unsigned long long timed_reads;
    unsigned long long readers_waiting;
    unsigned long long readers_not_waiting;
    unsigned long long readers_behind_later;
    unsigned long long readers_behind_earlier;
    unsigned int soonest_after_earlier;
    unsigned long long machine_barriers;
    unsigned long long machine_read_barriers;
    unsigned long long machine_formed;
} SchedulerOperation;

extern SassMachine s_machine;
extern unsigned char s_cubin[SCHEDULER_CUBIN_BYTES];
extern SchedulerInstruction s_section[SCHEDULER_SECTION_INSTRUCTIONS];
extern SchedulerOperation s_operation[SCHEDULER_OPERATIONS];
extern unsigned int s_operations;
extern unsigned long long s_instructions;
extern unsigned long long s_unread;
extern unsigned int s_cubins;

unsigned long long scheduler_word(const unsigned char *bytes, unsigned int width);
int scheduler_reads_register(const SchedulerInstruction *instruction, unsigned int number);
void scheduler_instruction(const unsigned char *bytes, unsigned long long address, SchedulerInstruction *instruction);
int scheduler_cubin(unsigned long long size);
int scheduler_record(const char *path);

#endif
