// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_run_internal.h: what the modules of the carrier share: its includes, the bounds of a container, a launch and a question, the driver, the telemetry, the events and the shape of a launch, the state the carrier holds through a round, and the functions one module reads from another
#ifndef CUBIN_RUN_INTERNAL_H
#define CUBIN_RUN_INTERNAL_H

#if !defined(_WIN32)
// clock_gettime and the monotonic clock are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../../../types/file_defs/qry/qry_buffer.h"
#include "../cubin_safe.h"
#include "../cubin_write.h"
#include "../sass_assemble.h"
#include "../sass_machine.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <sched.h>
#include <time.h>
#endif

// the most bytes a container and its code take, and the longest line of the cases
#define CUBIN_RUN_BYTES 262144u

#define CUBIN_RUN_LINE 1024u

// the words of a case and of an answer, the stick's arity, and the threads a block holds where the question names
// no shape of its own
#define CUBIN_RUN_IN_WORDS 8u

#define CUBIN_RUN_OUT_WORDS 2u

// the room each thread is given to write its answer in, as many words as a case, and the byte that room is filled
// with before a launch, so that the words the kernel wrote are told from the words it did not
#define CUBIN_RUN_OUT_ROOM CUBIN_RUN_IN_WORDS

#define CUBIN_RUN_FILL 0xffu

#define CUBIN_RUN_FILL_WORD (CUBIN_RUN_FILL * 0x01010101u)

#define CUBIN_RUN_THREADS 256u

// the most threads one launch gives a case each, and the most cases a question gives: every case is a thread's
#define CUBIN_RUN_THREADS_MOST (1u << 20u)

// the exits a question's code holds at most
#define CUBIN_RUN_EXITS 256u

// the run's tables of the gate's verdicts and the carrier's launches, and what came of a launch, its row's outcome
// (verdicts.oracle.tsv, launches.oracle.tsv)
#define CUBIN_RUN_VERDICTS "verdicts"

#define CUBIN_RUN_LAUNCHES "launches"

// the slot instructions of launches that faulted the part, a line `<low> <high> <cause>` each, the cause the listener
// heard or `-`: each held off the part from then on, in this run and in every run it is carried into
// (cubin_safe_verdict_added)
#define CUBIN_RUN_FAULTS "faulted.txt"

#define CUBIN_RUN_ANSWERED 0u

#define CUBIN_RUN_REFUSED 1u

#define CUBIN_RUN_FAULTED 2u

#define CUBIN_RUN_TIMED_OUT 3u

// the attribute the driver reports a kernel's register count under (cuFuncGetAttribute, CU_FUNC_ATTRIBUTE_NUM_REGS):
// the count the part says the kernel allots, read off a loaded function and not probed, and the register count before
// it is read
#define CUBIN_RUN_ATTRIBUTE_NUM_REGS 4

#define CUBIN_RUN_REGISTERS_UNREAD 0xffffffffu

// the driver's own types as its entry points take them: a status, handles, and a device address
typedef int CubinRunStatus;

typedef unsigned long long CubinRunAddress;

// how a question is launched: the threads of a block and the blocks of a launch
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

// the lines of a channel input: its blob in the run's buffer, read in place
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long at;
} CubinRunLines;

// What the carrier says, gathered and given whole once it is said (cubin_run_said): grown as it is written, since an
// answer holds a word of every case. A question's answer and its dry record are each gathered in one of their own
typedef struct
{
    char *text;
    size_t size;
    size_t room;
} CubinRunSaying;

// the event type the Xid critical errors are registered and read under, and the event a wait fills: the part's handle,
// the type stamped and its datum, with room past them for the fields a newer management layer adds; an older one's
// shorter event is read into that same space
#define CUBIN_RUN_XID 0x0000000000000008ull

#if defined(_WIN32)

// a subscription to the events a channel is written from now on, and an event rendered as its XML
#define CUBIN_RUN_EVENTS_FUTURE 1ul

#define CUBIN_RUN_EVENTS_XML 1ul

#endif

#define CUBIN_RUN_EVENTS_WAIT_MILLISECONDS 1000u

// the Xid critical errors the part stamped against the last launch, each one's datum, its Xid, with the text the
// driver wrote of it where the system's log holds one, kept for the listener's account of a panic
// (cubin_run_refusal_said): as many as CUBIN_RUN_XID_KEPT, and the count of all
#define CUBIN_RUN_XID_KEPT 16u

#define CUBIN_RUN_XID_TEXT 192u

#if defined(_WIN32)

#endif

// the facets a dry run pops out, each the object a stage of the carry hands on, for any one to be examined off
// the part. The flags that set them come after --dry; --full sets them all
#define CUBIN_FACET_QUERY 1u     // --query: the query object, the whole code read back, every place our reader names it

#define CUBIN_FACET_GATE 2u      // --gate-check: the safety word laid on and cubin_safe's verdict on what it then holds

#define CUBIN_FACET_VENDOR 4u    // --diff-output-against-vendor: the post-safety-word container written out to be read

extern SassMachine s_machine;
extern unsigned char s_pattern[CUBIN_RUN_BYTES];
extern unsigned char s_code[CUBIN_RUN_BYTES];
extern unsigned char s_container[CUBIN_RUN_BYTES];
extern unsigned int s_exits[CUBIN_RUN_EXITS];
extern unsigned int s_cases[CUBIN_RUN_THREADS_MOST][CUBIN_RUN_IN_WORDS];
extern unsigned int s_case_count;
extern unsigned int s_answers[CUBIN_RUN_THREADS_MOST][CUBIN_RUN_OUT_WORDS];
extern QryBuffer *s_run;
extern CubinRunSaying s_answer_said;
extern CubinRunSaying s_dry_said;
extern CubinRunSaying s_call_said;
extern int s_telemetry_ready;
extern int s_events_ready;
extern unsigned long long s_xid_data[CUBIN_RUN_XID_KEPT];
extern char s_xid_text[CUBIN_RUN_XID_KEPT][CUBIN_RUN_XID_TEXT];
extern unsigned int s_xid_count;
extern int s_launch_timed_out;
extern int s_launch_faulted;
extern unsigned long long s_timed_least;
extern unsigned long long s_timed_most;
extern unsigned int s_part_registers;
extern CubinRunDriver s_driver;
extern int s_driver_opened;
extern int s_dry;
extern unsigned int s_facets;

unsigned long long cubin_run_input(const char *name, unsigned char *bytes, unsigned long long room);
unsigned long long cubin_run_ask_key(const char *code_path);
void cubin_run_verdict_row(unsigned long long ask, unsigned int verdict, unsigned long long at, int dry);
void cubin_run_launch_row(unsigned long long ask, unsigned int outcome, unsigned int launches, unsigned long long nanoseconds, unsigned long long least, unsigned long long most, const char *said);
int cubin_run_lines_open(CubinRunLines *lines, const char *name);
int cubin_run_line(CubinRunLines *lines, char *line, size_t room);
void cubin_run_lines_close(CubinRunLines *lines);
int cubin_run_cases(const char *name);
int cubin_run_say(CubinRunSaying *saying, const char *format, ...);
int cubin_run_said(CubinRunSaying *saying, const char *name, unsigned int priority, int carried);
void *cubin_run_entry(void *library, const char *name);
int cubin_run_driver(CubinRunDriver *driver);
int cubin_run_telemetry_open(void);
void cubin_run_telemetry_drain(void);
int cubin_run_telemetry_faulted(CubinRunStatus status);
CubinRunStatus cubin_run_launch(const CubinRunDriver *driver, const char *kernel, const CubinRunShape *shape, unsigned int launches, unsigned long long *nanoseconds);
void cubin_run_container_out(const char *answers_path, const unsigned char *container, unsigned long long size);
void cubin_run_dry_written(unsigned long long code_size, unsigned int registers, unsigned int launches, const CubinRunShape *shape, unsigned int verdict, unsigned long long at);
void cubin_run_refusal_said(CubinRunSaying *answers, int panicked, const char *code_path, unsigned long long code_size, unsigned int registers, unsigned int slot, unsigned int launches, const CubinRunShape *shape, const char *status_name, CubinRunStatus status, unsigned int verdict, unsigned long long at);
int cubin_run_question(const char *kernel, unsigned long long pattern_size, const char *code_path, unsigned int registers, const char *cases_path, const char *answers_path, unsigned int slot, unsigned int launches, CubinRunShape shape, int shaped);
int cubin_run_verdict_load(const char *name);

#endif
