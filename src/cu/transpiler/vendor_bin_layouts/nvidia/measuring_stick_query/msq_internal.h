// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// msq_internal.h: what the modules of the cross share: its includes, the bounds of a run and a question, the control operations, the listing, question, reader, registers, distinct and frame structures, the run and the reader given, and the functions one module reads from another
#ifndef MSQ_INTERNAL_H
#define MSQ_INTERNAL_H

#if !defined(_WIN32)
// the threads and the processor count are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif

#include "../../../../types/file_defs/qry/qry_buffer.h"
#include "../../../lstar/interface/interface.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

// the least encodings a range the vendor stopped in is read on as two while the pool is starving, the part to its stop
// and the part past it, so that a long range's stops are read past at once; a shorter range, or any while the pool has
// a piece waiting for every reader, is read again whole with its stop put to a NOP, one reading more for each stop
#define MSQ_SPLIT 128u

// a NOP for the architecture: put in place of an instruction the vendor calls illegal so the run reads past it. The
// instruction it stands in for is already held; its own reading is never used
static const unsigned char s_nop[16] = {0x18u, 0x79u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                                        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xc0u, 0x0fu, 0x00u};

// the operations that transfer control or wait, which a question's slot must never be (cubin_safe.c, the same list)
static const char *const s_control[] = {"BRA",   "BRX",   "JMP",  "JMX",      "CALL",   "RET",   "EXIT",   "BSSY",
                                        "BSYNC", "BREAK", "BMOV", "WARPSYNC", "YIELD",  "BAR",   "DEPBAR", "NANOSLEEP",
                                        "BPT",   "RTT",   "KILL", "RPCMOV",   "RETIRE", "PMTRIG"};

// an executable section of an ELF, by the flag its header carries
#define MSQ_EXECUTABLE 0x4ull

// the most registers a reading names, past the most a kernel allots and the widest operand past it
#define MSQ_REGISTERS 512u

// the most registers a kernel allots a thread, R254 the highest a field names short of RZ
#define MSQ_ALLOTTED_MOST 255u

static const char s_illegal_why[] = "the vendor reads an illegal instruction our gate's key passed";

// a slot's reading: none where the vendor printed no instruction at its offset, its operation and its text past the
// guard where it read one, or illegal
typedef enum
{
    MSQ_NONE = 0,
    MSQ_READ = 1,
    MSQ_ILLEGAL = 2
} MsqKind;

typedef struct
{
    MsqKind kind;
    char operation[48];
    char *body;
} MsqReading;

// one instruction of a listing: its offset, its text as printed, and that text past its guard
typedef struct
{
    unsigned long long offset;
    char *raw;
    char *body;
} MsqLine;

typedef struct
{
    MsqLine *lines;
    size_t count;
    size_t room;
} MsqListing;

// a run of bytes in the run's buffer, or of memory of the program's own
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
} MsqBytes;

// a question: its answers' stem, its code's name, its slot, the registers its kernel allots, its container's code, and
// its slot instruction as the part would be handed it, where the code holds the slot
typedef struct
{
    char stem[256];
    char code_name[256];
    unsigned long long slot;
    unsigned int registers;
    MsqBytes code;
    int slotted;
    unsigned char word[16];
    size_t word_index;
    size_t whole_index;
    size_t tail_index;
    size_t prefix_index;
    size_t piece_index;
    size_t piece_key[2];
} MsqQuestion;

// A reader of the vendor's disassembler over raw runs: the stream its runs are handed as, the run it is handing, the
// output it was handed back, and the count of runs it read. `ident` keeps the blob names of readers that run at once
// apart, and `stream` their streams
typedef struct
{
    unsigned int ident;
    unsigned int stream;
    unsigned long long calls;
    unsigned char *raw;
    size_t raw_size;
    size_t raw_room;
    char *output;
    unsigned long long output_room;
    unsigned long long output_size;
} MsqReader;

// a set of general registers, a bit a register
typedef struct
{
    unsigned long long bits[MSQ_REGISTERS / 64u];
} MsqRegisters;

// A set of distinct runs of bytes, each found by its bytes: the index of each, by the order first seen
typedef struct
{
    MsqBytes *items;
    size_t count;
    size_t room;
    size_t *slots;
    size_t slot_count;
} MsqDistinct;

// A frame's reading, a question's code with its slot put to a NOP where it holds one: the place of every instruction
// the vendor calls illegal, and the listing it reads once each of those is put to a NOP. `readable` 0 where the vendor
// stopped at no instruction it named, and the frame can be held by no encoding
typedef struct
{
    unsigned long long *illegal;
    size_t illegal_count;
    MsqListing listing;
    int readable;
} MsqFrame;

// one piece of the cross's work: a range of the distinct slot encodings read as one run, one frame read whole, or every
// distinct tail read together
typedef enum
{
    MSQ_WORK_RANGE = 0,
    MSQ_WORK_FRAME = 1,
    MSQ_WORK_TAILS = 2
} MsqWorkKind;

typedef struct
{
    MsqWorkKind kind;
    size_t first;
    size_t count;
} MsqWork;

// The cross's work, read by a pool of reader threads at once, each thread a reader of its own: the ranges of the
// distinct slot encodings, each frame and the tails, taken off the queue in the order put. A range the vendor stops in
// puts ranges of its own to the queue (msq_range_read). The queue is kept under its lock; `active` counts the readers
// at work, so that a thread finding the queue empty waits while one of them may still put to it
typedef struct
{
    const MsqDistinct *words;
    MsqReading *readings;
    const MsqBytes *frames;
    MsqFrame *frame_read;
    const MsqDistinct *tails;
    MsqRegisters *addressed;
    MsqWork *queue;
    size_t queued;
    size_t room;
    size_t taken;
    pthread_mutex_t lock;
    _Atomic unsigned int active;
    _Atomic unsigned int readers;
    _Atomic unsigned long long calls;
    _Atomic int failed;
} MsqPool;

// what the cross holds, kept as it goes: the held list, the report, the lines it says of each hold, the holds the
// teacher reads, and the slots it read clear, a line `<stem> <operation>` each, for a conference the gate may call
typedef struct
{
    unsigned char *held;
    size_t held_size;
    size_t held_room;
    unsigned char *report;
    size_t report_size;
    size_t report_room;
    unsigned char *said;
    size_t said_size;
    size_t said_room;
    unsigned char *listed;
    size_t listed_size;
    size_t listed_room;
    unsigned char *read;
    size_t read_size;
    size_t read_room;
} MsqHolds;

extern QryBuffer *s_run;
extern const char *s_nvdisasm;
extern const char *s_architecture;
extern _Atomic unsigned long long s_streamed_nanoseconds;
extern _Atomic unsigned long long s_vendor_nanoseconds;

unsigned long long msq_now(void);
int msq_added(unsigned char **memory, size_t *held, size_t *room, const void *bytes, size_t size);
int msq_read(MsqReader *reader, const MsqBytes *runs, size_t count);
int msq_space(char c);
int msq_word(char c);
void msq_listing_free(MsqListing *listing);
int msq_listing_read(const char *text, size_t length, MsqListing *listing);
MsqReading msq_listing_at(const MsqListing *listing, unsigned long long offset);
int msq_stopped_at(const char *text, size_t length, unsigned long long *address);
void msq_addresses(const MsqListing *listing, size_t first, size_t end, MsqRegisters *set);
size_t msq_first_word(const char *text, size_t *start);
int msq_register_reached(const char *body, unsigned int registers, unsigned long long *reached);
void msq_registers_written(const char *body, MsqRegisters *set);
int msq_memory(const char *body);
int msq_container_code(const MsqBytes *container, MsqBytes *code);
int msq_distinct_made(MsqDistinct *set, size_t most);
size_t msq_distinct_index(MsqDistinct *set, const MsqBytes *item);
void msq_distinct_free(MsqDistinct *set);
int msq_illegal_said(const char *text, size_t length);
int msq_pool_put(MsqPool *pool, MsqWork work);
int msq_pool_starving(MsqPool *pool);
void *msq_pool_worker(void *argument);
unsigned int msq_processors(void);
int msq_tails_read(MsqReader *reader, const MsqDistinct *tails, MsqRegisters *addressed);
int msq_frame_read(MsqReader *reader, const MsqBytes *frame, MsqFrame *read);
void msq_frame_free(MsqFrame *read);
int msq_control_read(const char *body);
void msq_clear_said(MsqHolds *holds, const MsqQuestion *question, const char *body, const unsigned char *encoding);
unsigned int msq_clearing(unsigned long long reached);
void msq_hold_said(MsqHolds *holds, const MsqQuestion *question, const unsigned char *encoding, const char *why, unsigned int clearing);
void msq_stem(const char *name, char *stem, size_t room);
void msq_extension_put(const char *name, const char *extension, char *put, size_t room);

#endif
