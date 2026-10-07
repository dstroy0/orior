// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef RUN_CHANNEL_H
#define RUN_CHANNEL_H

// The run channel: words put at a part's address, the part started, words read back.
//
// This is the only way anything is learned about a part. A question is put as the part's own code, the part runs it,
// and a word comes back. Nothing here reads a name, a listing or a source: code and its cases go in and an answer a
// case comes out, and what those words are is the part's answer and no software's opinion of it.
//
// What carries the bytes is whatever holds the part's bus mapping. On a part behind a driver that is the driver,
// which for a load and a start does what the channel says it does: it writes the container into the part's memory
// over its address window and rings the register that starts it. It is reached by name and by its own entry points,
// and nothing of its vendor's toolchain is compiled against or asked anything. A translator is not transport: a
// thing that turns a source into a coding is the part of the loop being derived, and a run that went through one
// would be reading that translator's answer instead of the part's.
//
// The carrier is a program of the system's own (vendor_bin_layouts/<vendor>), and the channel knows it only as the
// command it is given. The carrier puts the code in the container the system accepted, holds it to the system's gate
// on the host, and runs it a thread a case. A question is carried in a process of its own, and many untimed questions
// can be carried in one: a part that refuses a question ends that process, its ending is that question's answer, and
// the channel goes on to the next in a process of its own.
//
// A part whose bus is addressed without a driver is the same channel with a shorter carrier, and the interface does
// not move.

#ifdef __cplusplus
extern "C"
{
#endif

// the words of a case, the words of its answer, and the most cases one question is run over, past every case the
// host computes a question on: the carrier gives each case a thread
#define RUN_IN_WORDS 8u
#define RUN_OUT_WORDS 2u
#define RUN_CASES_MOST 4096u

// what a run came to
typedef enum
{
    // the words came back
    RUN_ANSWERED,
    // the part took the container and the words are what it left
    RUN_NOTHING,
    // the part would not take the container as it stands
    RUN_ILLEGAL,
    // nothing carried the bytes: no part, or no mapping to it
    RUN_NO_CHANNEL,
    // the system's gate held the code off the part, and the part never saw it
    RUN_HELD
} RunOutcome;

// one question and what came of it
typedef struct
{
    // the question's code, sixteen bytes an instruction, and its size
    const unsigned char *code;
    unsigned long long code_size;
    // how many registers a thread of it holds, which the container states and no instruction declares
    unsigned int registers;
    // the cases it is run over, a thread a case, and how many
    unsigned int word[RUN_CASES_MOST][RUN_IN_WORDS];
    unsigned int cases;
    // how many times it is launched again once it answers, those launches timed together on the part's own timer;
    // 0 leaves it untimed
    unsigned int launches;
    // the threads of a block and the blocks of a launch, every thread given a case, thread t case t of the cases taken
    // round; where both are 0 it is one block of a carrier's own count
    unsigned int threads;
    unsigned int blocks;
    // each case's answer, its two words as one value
    unsigned long long answered[RUN_CASES_MOST];
    // the time the launches took together, in nanoseconds as the part's timer counts them; 0 where it is untimed
    unsigned long long nanoseconds;
    unsigned int outcome;
    // what the carrier said where the outcome is not RUN_ANSWERED
    char refused[128];
} RunQuestion;

// The channel opened on the carrier `carrier`, the words that start it ended by NULL, each question's files kept in
// `folder`, and each given `limit_microseconds` before its process is ended. 1, or 0 with the reason printed. A
// channel is opened once and every question after goes through it
int run_channel_open(const char *const *carrier, const char *folder, unsigned long long limit_microseconds);

// `asked` put and answered. 1 where the outcome is RUN_ANSWERED, 0 otherwise, with `asked->outcome` saying which
int run_channel_ask(RunQuestion *asked);

// The `count` questions of `asked`, each untimed and of no shape of its own, put together: carried in one process,
// each answered as it is carried. A question the part refuses ends that process, its ending is its answer, and the
// questions after it are carried in a process of their own, so that one refusal costs no other question its answer.
// The count of those answered, each with its own outcome
unsigned int run_channel_ask_many(RunQuestion *const *asked, unsigned int count);

// the channel closed, and what it held given back
void run_channel_close(void);

// what the channel is carried by, for a run to say where its words came from
const char *run_channel_carrier(void);

#ifdef __cplusplus
}
#endif

#endif
