// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_channel_internal.h: what the modules of the run channel share. Each question is handed to the run's buffer as blobs of its .qry (qry_buffer.h) and carried by the carrier in a process of its own, or many in one, and the blob the carrier handed for each is read back as that question's answer; the round a carrier is about to carry is handed critical, on disk before the carrier starts, and a round whose record cannot be put on disk is never carried. Here are its includes, the bounds of a line, a list and an answer, the channel's state, the record, and the functions one module reads from another
#ifndef RUN_CHANNEL_INTERNAL_H
#define RUN_CHANNEL_INTERNAL_H

#include "../run_channel.h"

#include "../../../../../types/file_defs/kqr/kqr.h"
#include "../../../../../types/file_defs/ksc/ksc.h"
#include "../../../../../types/file_defs/qry/qry_buffer.h"
#include "../../../interface/interface.h"
#include "../../overseer/overseer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most words that start a carrier, and the longest of them
#define RUN_CARRIER_WORDS 8u

#define RUN_PATH_LONGEST 1024u

// the longest name of a blob the channel hands
#define RUN_NAME_LONGEST 32u

#define RUN_ANSWER_LONGEST (16u + (RUN_CASES_MOST * 17u))

// the longest line of a round's list: three names, three counts and their spaces
#define RUN_LINE_LONGEST ((3u * RUN_NAME_LONGEST) + 64u)

// the most of a carrier's output kept in its blob, and the line before it that says how the carrier ended
#define RUN_CARRIER_OUTPUT (1u << 20u)

#define RUN_CARRIER_ENDING 192u

// a question's cases as the carrier reads them: a case a line, its eight words in hex, each at most eight digits
#define RUN_CASES_TEXT (RUN_CASES_MOST * ((RUN_IN_WORDS * 9u) + 1u))

// the round's list as the airlock reads it, each question's answers named after the prefix, and the holds its cross
// hands the teacher, a line a hold, `<answers stem> <registers> <why>` (measuring_stick_query.c)
#define RUN_AIRLOCK_LIST "airlock.txt"

#define RUN_AIRLOCK_ANSWERS "airlock_"

#define RUN_AIRLOCK_HOLDS "airlock_holds.txt"

// what the cross read clear, a line `<stem> <operation> <low> <high>` each, for the conferences the gate calls
#define RUN_AIRLOCK_READ "airlock_read.txt"

typedef struct
{
    char word[RUN_CARRIER_WORDS][RUN_PATH_LONGEST];
    unsigned int words;
    // the airlock: the words that start the carrier with the part taken out, which pops each question's container
    // (`--list <list>` put after them), and those that start the vendor's reader over the containers (the list put
    // after its first word); the held list the carrier reads, named by its `--held`, and 1 where the carrier is handed
    // one at all
    char pop[RUN_CARRIER_WORDS][RUN_PATH_LONGEST];
    unsigned int pop_words;
    char cross[RUN_CARRIER_WORDS][RUN_PATH_LONGEST];
    unsigned int cross_words;
    char held[RUN_PATH_LONGEST];
    int held_given;
    // the sequence of the list the airlock last read: what its cross hands comes after it
    unsigned long long airlock_began;
    // the nanoseconds the airlock, its pop and its cross within it, and the carrier have taken since the channel
    // opened (run_channel_clocks)
    unsigned long long airlock_nanoseconds;
    unsigned long long pop_nanoseconds;
    unsigned long long cross_nanoseconds;
    unsigned long long carried_nanoseconds;
    unsigned long long limit_microseconds;
    int open;
    // 1 where the carrier's only word is `dry`, and the count of questions written so far
    int dry;
    unsigned int dry_written;
    // 1 where the carrier's own words hold --dry: it stops before the driver, every round through the airlock as a
    // live one, and nothing it answers is kept in R
    int stopped;
    // the run's buffer every blob of the channel is handed to
    QryBuffer *buffer;
    // the list of a round: the dry run's every question, handed whole once the channel closes, or a carried round's,
    // handed before its carrier starts
    char *list;
    size_t list_size;
    size_t list_room;
} RunChannel;

// how many registers past the highest its code names a container declares, as the part answered it, and the mark of
// no answer: no hold of a register past a kernel's count is then cleared
#define RUN_REGISTERS_UNANSWERED 0xffffffffu

// the most registers a kernel allots a thread, R254 the highest a field names short of RZ
#define RUN_REGISTERS_MOST 255ull

extern RunChannel s_channel;
extern unsigned int s_registers_past;
extern char s_answer_line[RUN_ANSWER_LONGEST];
extern char s_carrier_output[RUN_CARRIER_OUTPUT];
extern char s_carrier_said[RUN_CARRIER_ENDING + RUN_CARRIER_OUTPUT];
extern char s_cases_text[RUN_CASES_TEXT];
extern KqrRecord s_record;
extern char s_record_path[1024];
extern char s_airlock_output[RUN_CARRIER_OUTPUT];

void run_channel_conference_called(const RunQuestion *asked, const char *how, const unsigned char *said, unsigned long long said_size);
unsigned long long run_channel_on_line(void);
void run_channel_line_ruled(const RunQuestion *asked);
void run_channel_halted(RunQuestion *const *asked, unsigned int first, unsigned int count);
void run_channel_teacher_said(unsigned int questions, unsigned long long round, const InterfaceAnswer *answer, unsigned int answered, const char *carrying);
int run_channel_listed(const char *line, size_t length);
int run_channel_write(const RunQuestion *asked, const char *code_name, const char *cases_name);
void run_channel_read(RunQuestion *asked, const char *answers_name, unsigned long long after);
void run_channel_dry_written(RunQuestion *asked);
void run_channel_identity(const RunQuestion *question, KqrIdentity *identity);
int run_channel_recorded(RunQuestion *asked);
void run_channel_kept(const RunQuestion *asked);
void run_channel_list_line(unsigned int place, char *line);
unsigned long long run_channel_round_handed(void);
int run_channel_airlocked(unsigned int questions, char *refused, size_t room);
void run_channel_named(unsigned int at, int alone, char *code_name, char *cases_name, char *answers_name);
int run_channel_relisted(RunQuestion *const *asked, unsigned int first, unsigned int count, int alone);
void run_channel_check_traced(const RunQuestion *asked, const char *said);
unsigned int run_channel_raised(RunQuestion *const *asked, unsigned int first, unsigned int count, int alone);
int run_channel_checked(RunQuestion *const *asked, unsigned int first, unsigned int count, int alone, char *refused, size_t room);
void run_channel_output_handed(const InterfaceAnswer *answer);
void run_channel_traced(RunQuestion *const *asked, unsigned int count, const char *how);
int run_channel_carried(RunQuestion *asked);
unsigned int run_channel_carried_many(RunQuestion *const *asked, unsigned int count);
// the host's clock in nanoseconds, read only against itself
unsigned long long run_channel_now(void);

#endif
