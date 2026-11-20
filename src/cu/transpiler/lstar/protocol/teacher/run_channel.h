// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef RUN_CHANNEL_H
#define RUN_CHANNEL_H

// The run channel: words put at a part's address, the part started, words read back.
//
// This is the only way to learn anything about a part. The channel puts a question as the part's own code, the part
// runs it, and a word comes back. Nothing here reads a name, a listing or a source. Code and its cases go in, and an
// answer a case comes out. What those words are is the part's answer, and no software's opinion of it.
//
// Whatever owns the bus mapping of the part carries the bytes. On a part behind a driver, that is the driver, which for
// a load and a start does what the channel specifies. It writes the container into the memory of the part over its
// address window, and rings the register that starts it. The channel reaches it by name and by its own entry points.
// The channel compiles nothing against the toolchain of its vendor, and queries nothing of it.
//
// A translator is not transport. A thing that turns a source into a coding is the part of the loop that the run
// derives. A run through one would read the answer of that translator instead of that of the part.
//
// The carrier is a program of the system's own (vendor_bin_layouts/<vendor>), and the channel knows it only as the
// command that it gets. The carrier puts the code in the container that the system accepted, and checks it against the
// gate of the system on the host. It then runs it a thread a case.
//
// The carrier carries a question in a process of its own. It can carry many questions of no shape of their own in one
// process, and times each timed one over its own launches. A part that refuses a question ends that process. Its end is
// the answer to that question, and the channel goes on to the next in a process of its own.
//
// A part whose bus needs no driver for its addresses is the same channel with a shorter carrier, and the interface does
// not move.

#ifdef __cplusplus
extern "C"
{
#endif

// the words of a case, the words of its answer, and the max cases that one question runs over. That max is past every
// case that the host computes a question on, and the carrier gives each case a thread
#define RUN_IN_WORDS 8u
#define RUN_OUT_WORDS 2u
#define RUN_CASES_MAX 4096u

// a question's probe place where its code contains none: the carrier makes no place conservative past the wait on all
// six
#define RUN_PLACE_NONE 0xffffffffu

// what the carrier leaves in a record that no lane wrote, and the threads a block that it launches where a question
// fixes no shape. Each is the carrier's own number (cubin_run_internal.h). The queries of the frame read a lane that
// wrote nothing, and compute the lane and block of each thread from them
#define RUN_UNWRITTEN_WORD 0xffffffffu
#define RUN_THREADS_A_BLOCK 256u

// the place of the count among the parameters of the launch, in bytes, the carrier's own (cubin_run_internal.h). The
// queries of the frame read the count as the word of a parameter, the threads that the launch gives a case each
#define RUN_COUNT_PLACE 16u

// the gate's block under rule 3 of the airlock, an operation key unknown, as the carrier answers it. That is `skipped`
// and the number of the rule (cubin_safe.h, CUBIN_SAFE_KEY). A query blocked so is open in R, as a timed-out one is.
// The part never saw it, and a conference may admit it (overseer.h)
#define RUN_GATE_KEY_RULE "skipped 5 "

// the gate's block on an encoding that the vendor's own reader called illegal, as the carrier answers it (cubin_safe.h,
// CUBIN_SAFE_GATED). A query blocked so stays blocked in R: the verdict is the vendor's and stands. A query that the
// gate blocked under any other rule is open in R. The part never saw it, and the gate reads it again under its current
// rules
#define RUN_GATE_VENDOR_RULE "skipped 12 "

// what a run came to
typedef enum
{
    // the words came back
    RUN_ANSWERED,
    // the part took the container and the words are what it left
    RUN_NOTHING,
    // the part would not take the container as it is
    RUN_ILLEGAL,
    // nothing carried the bytes: no part, or no mapping to it
    RUN_NO_CHANNEL,
    // the system's gate blocked the code from the part, and the part never saw it
    RUN_GATED
} RunOutcome;

// one question and what came of it
typedef struct
{
    // the question's code, sixteen bytes an instruction, and its size
    const unsigned char *code;
    unsigned long long code_size;
    // how many registers a thread of it uses, which the container states and no instruction declares
    unsigned int registers;
    // the place that the carrier makes conservative before the part sees it. RUN_PLACE_NONE where the code contains no
    // probe place, and every instruction only waits on all six barriers
    unsigned int place;
    // the cases that it runs over, a thread a case, and how many
    unsigned int word[RUN_CASES_MAX][RUN_IN_WORDS];
    unsigned int cases;
    // how many times the carrier launches it again once it answers. The driver's event clock times those launches
    // together. That is the driver's word on a launch, and not the clock that the query reads (query_put.h). 0 leaves
    // it untimed
    unsigned int launches;
    // the threads of a block and the blocks of a launch. Every thread gets a case, thread t case t of the cases taken
    // round. Where both are 0, it is one block of a carrier's own count
    unsigned int threads;
    unsigned int blocks;
    // each case's answer, its two words as one value
    unsigned long long answered[RUN_CASES_MAX];
    // the time that the launches took together, in nanoseconds off the driver's event timer, which ticks about every
    // half microsecond. 0 where no timer ran
    unsigned long long nanoseconds;
    // the least and the max time that one launch of them took. That is a launch whole, and not the question's own cost.
    // A form costs a few cycles under the overhead of a launch, and P6 ranks nothing off these. 0 where no timer ran
    unsigned long long least;
    unsigned long long max;
    // the address of the timer register that timed the launches, read through the BAR0 driver (bus_clock.c). 0 where
    // the driver's event timer timed them
    unsigned int clock_address;
    unsigned int outcome;
    // what the carrier reported where the outcome differs from RUN_ANSWERED
    char refused[128];
} RunQuestion;

// The channel opened on the carrier `carrier`, with the words that start it ended by NULL. Each question gets
// `limit_microseconds` before the channel ends its process. 1, or 0 with the reason printed where no writer runs for
// the run.
//
// Every file of the channel is a blob of the run's .qry, passed to the run (qry_member.h). The carrier gets the names
// of the blobs. Only the run's writer writes a file of the channel to disk. The channel passes the round that a carrier
// is about to carry, questions.txt, as critical, and it is on disk before the carrier starts. The channel does not
// carry a round that it cannot put there. The channel opens once, and every question after goes through it.
//
// A carrier whose first word is `dry` is a dry run: nothing carries a question, and nothing answers it. The channel
// writes a round as it writes a live round. Each question goes as question<n>.bin and cases<n>.txt, and the round as
// questions.txt in the form of a carrier's list, `<code> <registers> <cases> <answers> <probe> <launches>`. The round
// passes the airlock that the words after `dry` open, as a live round does. Each question then reads RUN_GATED, and the
// channel passes the list of the last round whole once it closes.
//
// A carrier that stops before the part then checks what the protocol put, from that list. A question queried alone on a
// dry channel goes to disk and to the gate with no airlock. A carrier whose own words include --dry stops before the
// driver too, and every round reaches it through the airlock as a live round does. Nothing launches, and the channel
// reads R and stores nothing that it answers in R.
//
// Every round that a live channel carries passes the airlock first, and nothing goes to the part before the vendor's
// reader reads it. The words after the carrier's may open one:
//   - `--pop <words>`, the carrier with the part taken out, which passes the container of each question
//     (`--list <list>` put after its words).
//   - `--cross <words>`, the vendor's reader over the containers (the list put after its first word). It passes the
//     gated list that the carrier reads by its `--gated <name>`.
//
// Before the channel carries each round, it passes its list as airlock.txt, with the answers of each question named
// airlock_ before its own name. The pop and the cross run over it. The channel carries the round only where the cross
// read it and passed the gated list after it. The carrier then blocks every code that the vendor called out
// (cubin_safe.h).
//
// A channel with no airlock carries a round only where its carrier gets a gated list. A cross must make that list from
// a read of the whole pass before it. Nothing carries a round that the airlock does not pass, and the round reads
// RUN_NO_CHANNEL, with the reason of the airlock as its refusal.
//
// The channel checks the round, fixes it and checks it again before the part.
//
// The cross gets one word more, the name of the refusals that it passes the teacher. That file holds a line a refusal,
// with the count of registers that clears it (measuring_stick_query.c). Some questions of no shape of their own have
// refusals only for registers past the count that their kernel allots. Each such question gets the count that clears
// every refusal of it. The channel lists and reads the round again, until no question gets one.
//
// A count changes how the channel puts a question, and never what the question requires. Every refusal, every question
// still refused and every count given is a line of the teacher's trace (the record `teacher`). Each line is
// `teacher: <identity> checked: <what>`, and a branch that the learner took then traces back to the check that put its
// question
int run_channel_open(const char *const *carrier, unsigned long long limit_microseconds);

// R, the part's record at `path` (kqr.h), opened for the channel. The channel answers an untimed question from R where
// R contains an answer to it, and never carries it. It carries again a question that timed out in R. It stores in R,
// under a cycle of `mode`, what comes back of every question that it carries.
//
// The channel writes R back when it closes, where R changed and the read of it was whole. 1, or 0 with the reason
// printed. A dry run reads R and stores nothing in it
int run_channel_record(const char *path, const char *member, const char *mode);

// How many registers past the highest that its code names a container declares. The channel reads it from the .ksc of
// the part at `ksc`, as the part answered it (`run answers <past> register past`, sass_machine.h). A refusal of the
// cross for a register past a kernel's count goes again, with the count that reaches that register by so many. Where
// the .ksc contains no answer, or no caller calls this, no count clears such a refusal, and the question stays refused.
// 1 where it contains one, 0 where not
int run_channel_registers_past(const char *ksc);

// 1 where `refused`, what a carrier reported of a question, is the gate's block for an unknown operation key
// (RUN_GATE_KEY_RULE)
int run_channel_key_gated(const char *refused);

// 1 where `refused` is the gate's block on an encoding the vendor's reader called illegal (RUN_GATE_VENDOR_RULE)
int run_channel_vendor_gated(const char *refused);

// `question` put and answered. 1 where RUN_ANSWERED is the outcome, 0 otherwise, with `question->outcome` to show which
int run_channel_put(RunQuestion *question);

// The `count` questions of `questions`, each of no shape of its own, put together. The channel carries them in one
// process, answers each as it carries it, and times each timed one over its own launches. A question that the part
// refuses ends that process, and that end is its answer. The questions after it go in a process of their own, and one
// refusal then costs no other question its answer. Returns the count of those answered, each with its own outcome
unsigned int run_channel_put_many(RunQuestion *const *questions, unsigned int count);

// 1 once the run settles a panic on the line. The run stops, and the gate blocks every question put after it with that
// reason. That holds on a dry channel as on a live one, and nothing carries them. One panic a run
int run_channel_panic_ruled(void);

// the channel closed, R written back where the channel opened it, and what it contained given back
void run_channel_close(void);

// the nanoseconds that the airlock, its pop and its cross within it, and the carrier took since the channel opened,
// each into its own
void run_channel_clocks(unsigned long long *airlock, unsigned long long *pop, unsigned long long *cross,
                        unsigned long long *carried);

// what carries the channel, for a run to report where its words came from
const char *run_channel_carrier(void);

#ifdef __cplusplus
}
#endif

#endif
