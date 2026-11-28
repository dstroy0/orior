// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_run_internal.h: what the modules of the carrier share: its includes, the bounds of a container, a launch and a
// question, the driver, the telemetry, the events and the shape of a launch, the state the carrier maintains through a
// round, and the functions one module reads from another
#ifndef CUBIN_RUN_INTERNAL_H
#define CUBIN_RUN_INTERNAL_H

#if !defined(_WIN32)
// clock_gettime and the monotonic clock are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../cubin_safe.h"
#include "../cubin_write.h"
#include "../sass_assemble.h"
#include "../sass_machine.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <sched.h>
#include <time.h>
#endif

// the max bytes a container and its code take, and the longest line of the cases
#define CUBIN_RUN_BYTES 262144u

#define CUBIN_RUN_LINE 1024u

// the words of a case and of an answer, and the arity of the stick. Then the threads that a block runs where the
// question names no shape of its own
#define CUBIN_RUN_IN_WORDS 8u

#define CUBIN_RUN_OUT_WORDS 2u

// the room that each thread gets to write its answer in, as many words as a case. Then the byte that fills that room
// before a launch. Thus the words that the kernel wrote show apart from the words that it did not
#define CUBIN_RUN_OUT_ROOM CUBIN_RUN_IN_WORDS

#define CUBIN_RUN_FILL 0xffu

#define CUBIN_RUN_FILL_WORD (CUBIN_RUN_FILL * 0x01010101u)

#define CUBIN_RUN_THREADS 256u

// the place among the launch's parameters the count is at, in bytes: past the cases' address and the records',
// the kernel's (in, out, count)
#define CUBIN_RUN_COUNT_PLACE (2u * (unsigned int)sizeof(CubinRunAddress))

// the max threads one launch gives a case each, and the max cases a question gives: every case is a thread's
#define CUBIN_RUN_THREADS_MAX (1u << 20u)

// the exits a question's code contains at max
#define CUBIN_RUN_EXITS 256u

// the run tables of the gate verdicts and the carrier launches (verdicts.oracle.tsv, launches.oracle.tsv). Then what
// came of a launch, the outcome of its row
#define CUBIN_RUN_VERDICTS "verdicts"

#define CUBIN_RUN_LAUNCHES "launches"

// the probe instructions of launches that faulted the part, each a line `<low> <high> <cause>`. The cause is what the
// listener received, or `-`. The gate blocks each from the part from then on, in this run and in every run that it
// goes into (cubin_safe_verdict_added)
#define CUBIN_RUN_FAULTS "faulted.txt"

#define CUBIN_RUN_ANSWERED 0u

#define CUBIN_RUN_REFUSED 1u

#define CUBIN_RUN_FAULTED 2u

#define CUBIN_RUN_TIMED_OUT 3u

// the attribute that the driver reports the register count of a kernel under (cuFuncGetAttribute,
// CU_FUNC_ATTRIBUTE_NUM_REGS). It is the count that, as the part reports, the kernel allots, read from a loaded
// function and not probed. Then the register count before the call reads it
#define CUBIN_RUN_ATTRIBUTE_NUM_REGS 4

#define CUBIN_RUN_REGISTERS_UNREAD 0xffffffffu

// the driver's own types as its entry points take them: a status, handles, and a device address
typedef int CubinRunStatus;

typedef unsigned long long CubinRunAddress;

// how the carrier launches a question: the threads of a block and the blocks of a launch
typedef struct
{
    unsigned int threads;
    unsigned int blocks;
} CubinRunShape;

// the driver's entry points this reaches, each by the name the driver exports it under
typedef struct
{
    CubinRunStatus (*init)(unsigned int);
    CubinRunStatus (*device_get)(int *, int);
    CubinRunStatus (*context_retain)(void **, int);
    CubinRunStatus (*context_set)(void *);
    CubinRunStatus (*module_load)(void **, const void *);
    CubinRunStatus (*function_get)(void **, void *, const char *);
    CubinRunStatus (*function_attribute)(int *, int, void *);
    CubinRunStatus (*allocate)(CubinRunAddress *, size_t);
    CubinRunStatus (*copy_in)(CubinRunAddress, const void *, size_t);
    CubinRunStatus (*clear)(CubinRunAddress, unsigned char, size_t);
    CubinRunStatus (*launch)(void *, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int,
                             unsigned int, void *, void **, void **);
    CubinRunStatus (*synchronize)(void);
    CubinRunStatus (*copy_out)(void *, CubinRunAddress, size_t);
    CubinRunStatus (*error_name)(CubinRunStatus, const char **);
    CubinRunStatus (*event_create)(void **, unsigned int);
    CubinRunStatus (*event_record)(void *, void *);
    CubinRunStatus (*event_synchronize)(void *);
    CubinRunStatus (*event_query)(void *);
    CubinRunStatus (*event_elapsed)(float *, void *, void *);
    CubinRunStatus (*event_destroy)(void *);
    CubinRunStatus (*module_unload)(void *);
    CubinRunStatus (*release)(CubinRunAddress);
} CubinRunDriver;

// the lines of a channel input: its blob in the run, read in place
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long at;
} CubinRunLines;

// the event type that the program registers and reads the Xid critical errors under
#define CUBIN_RUN_XID 0x0000000000000008ULL

#if defined(_WIN32)

// a subscription to the events that the system writes to a channel from now on, and an event rendered as its XML
#define CUBIN_RUN_EVENTS_FUTURE 1UL

#define CUBIN_RUN_EVENTS_XML 1UL

#endif

#define CUBIN_RUN_EVENTS_WAIT_MILLISECONDS 1000u

// the Xid critical errors that the part stamped against the last launch: the datum of each, and its Xid. Each has the
// text that the driver wrote of it where the log of the system records one. The program stores them for the account
// of a panic by the listener (cubin_run_refusal_written), as many as CUBIN_RUN_XID_MAX, and the count of all
#define CUBIN_RUN_XID_MAX 16u

#define CUBIN_RUN_XID_TEXT 192u

#if defined(_WIN32)

#endif

// the facets that a dry run pops out, each the object that a stage of the carry passes on. A person can examine any
// one of them with no part. The flags that set them come after --dry, and --full sets them all
#define CUBIN_FACET_QUERY 1u     // --query: the query object, the whole code read back where our reader names it

#define CUBIN_FACET_GATE 2u // --gate-check: the safety word put on, and the verdict of cubin_safe on the code then

#define CUBIN_FACET_VENDOR 4u    // --diff-output-against-vendor: the container after the safety word, written out

extern SassMachine s_machine;
extern unsigned char s_pattern[CUBIN_RUN_BYTES];
extern unsigned char s_code[CUBIN_RUN_BYTES];
extern unsigned char s_container[CUBIN_RUN_BYTES];
extern unsigned int s_exits[CUBIN_RUN_EXITS];
extern unsigned int s_cases[CUBIN_RUN_THREADS_MAX][CUBIN_RUN_IN_WORDS];
extern unsigned int s_case_count;
extern unsigned int s_answers[CUBIN_RUN_THREADS_MAX][CUBIN_RUN_OUT_WORDS];
extern QryShared *s_run;
// What the carrier reports, gathered in a line of its own and given whole after the call writes it
// (cubin_run_member_written). The answer of a question, its dry record and its call each have one
extern VerbaLinea s_answers_line;
extern VerbaLinea s_dry_line;
extern VerbaLinea s_call_line;
extern int s_telemetry_ready;
extern int s_events_ready;
extern unsigned long long s_xid_data[CUBIN_RUN_XID_MAX];
extern char s_xid_text[CUBIN_RUN_XID_MAX][CUBIN_RUN_XID_TEXT];
extern unsigned int s_xid_count;
extern int s_launch_timed_out;
extern int s_launch_faulted;
extern unsigned long long s_timed_least;
extern unsigned long long s_timed_max;
extern unsigned int s_part_registers;
extern CubinRunDriver s_driver;
extern int s_driver_opened;
extern int s_dry;
extern unsigned int s_facets;

// the clocks of the carrier over a list. The call sums the nanoseconds of the host in each stage of a question over
// every question, and reports them as the list ends. The stages are these:
//
// - the inputs read
// - the container written, and read by the gate
// - the module loaded, and its function found
// - the memory allotted, filled, read back and given back
// - the launch between its events
// - the fault telemetry read
// - the rows and the answer passed
typedef struct
{
    unsigned long long input;
    unsigned long long gate;
    unsigned long long load;
    unsigned long long memory;
    unsigned long long launch;
    unsigned long long telemetry;
    unsigned long long passed;
} CubinRunClocks;

extern CubinRunClocks s_clocks;

unsigned long long cubin_run_host_now(void);

// the clock that the part named in its .ksc beside `machine_path`, opened through the listener. The return is 1 where
// the program reads its word from here on. It is 0 where the part names none, or the call cannot reach the listener
// (clock.c)
int cubin_run_clock_open(const char *machine_path);
// the clock's word read into `word`: 1, or 0 where no clock is open
int cubin_run_clock_read(unsigned int *word);
// the clock's offset into the aperture, 0 where none is open
unsigned int cubin_run_clock_offset(void);
void cubin_run_clock_close(void);

unsigned long long cubin_run_input(const char *name, unsigned char *bytes, unsigned long long room);
unsigned long long cubin_run_query_key(const char *code_path);
void cubin_run_verdict_row(unsigned long long query_key, unsigned int verdict, unsigned long long at, int dry);
void cubin_run_launch_row(unsigned long long query_key, unsigned int outcome, unsigned int launches,
                          unsigned long long nanoseconds, unsigned long long least, unsigned long long max,
                          const char *message);
int cubin_run_lines_open(CubinRunLines *lines, const char *name);
int cubin_run_line(CubinRunLines *lines, char *line, size_t room);
void cubin_run_lines_close(CubinRunLines *lines);
int cubin_run_cases(const char *name);
int cubin_run_member_written(VerbaLinea *line, const char *name, unsigned int priority, int carried);
void *cubin_run_entry(void *library, const char *name);
int cubin_run_driver(CubinRunDriver *driver);
int cubin_run_telemetry_open(void);
void cubin_run_telemetry_drain(void);
int cubin_run_telemetry_faulted(CubinRunStatus status);
CubinRunStatus cubin_run_launch(const CubinRunDriver *driver, const char *kernel, const CubinRunShape *shape, unsigned int launches, unsigned long long *nanoseconds);
void cubin_run_container_out(const char *answers_path, const unsigned char *container, unsigned long long size);
void cubin_run_dry_written(unsigned long long code_size, unsigned int registers, unsigned int launches, const CubinRunShape *shape, unsigned int verdict, unsigned long long at);
void cubin_run_refusal_written(VerbaLinea *answers, int panicked, const char *code_path,
                               unsigned long long code_size, unsigned int registers, unsigned int place,
                               unsigned int launches, const CubinRunShape *shape, const char *status_name,
                               CubinRunStatus status, unsigned int verdict, unsigned long long at);
int cubin_run_question(const char *kernel, unsigned long long pattern_size, const char *code_path,
                       unsigned int registers, const char *cases_path, const char *answers_path, unsigned int place,
                       unsigned int launches, CubinRunShape shape, int shaped);
int cubin_run_verdict_load(const char *name);

#endif
