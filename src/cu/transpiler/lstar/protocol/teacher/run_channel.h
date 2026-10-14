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
// on the host, and runs it a thread a case. A question is carried in a process of its own, and many questions of no
// shape of their own can be carried in one, each timed one over its own launches: a part that refuses a question
// ends that process, its ending is that question's answer, and the channel goes on to the next in a process of its
// own.
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

// a question's slot where its code holds none: the carrier makes no place conservative past the wait on all six
#define RUN_SLOT_NONE 0xffffffffu

// what the carrier leaves in a record no lane wrote, and the threads a block it launches where a question fixes no
// shape, each the carrier's own number (cubin_run_internal.h): the frame's asks read a lane that wrote nothing and
// compute each thread's lane and block from them
#define RUN_UNWRITTEN_WORD 0xffffffffu
#define RUN_THREADS_A_BLOCK 256u

// the gate's hold under rule 3 of the airlock, an operation key unknown, as the carrier answers it: `skipped` and the
// rule's number (cubin_safe.h, CUBIN_SAFE_KEY). An ask held so is open in R, as a timed-out one is: the part never saw
// it, and a conference may admit it (overseer.h)
#define RUN_GATE_KEY_RULE "skipped 5 "

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
    // the place the carrier makes conservative before the part sees it, RUN_SLOT_NONE where the code holds no slot and
    // every instruction only waits on all six barriers
    unsigned int slot;
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
    // the least and the most one launch of them took, its block latency, the runs P6 reads; 0 where it is untimed
    unsigned long long least;
    unsigned long long most;
    unsigned int outcome;
    // what the carrier said where the outcome is not RUN_ANSWERED
    char refused[128];
} RunQuestion;

// The channel opened on the carrier `carrier`, the words that start it ended by NULL, and each question given
// `limit_microseconds` before its process is ended. Every file of the channel is a blob of the run's .qry, handed to
// the run's buffer (qry_buffer.h), and the carrier is handed the blobs' names: no file of the channel is written to
// disk but by the run's writer. The round a carrier is about to carry, questions.txt, is handed critical and is on disk
// before the carrier starts; a round that cannot be put there is not carried. 1, or 0 with the reason printed where
// no writer runs for the run. A channel is opened once and every question after goes through it.
//
// A carrier whose only word is `dry` is a dry run: nothing carries a question and nothing answers it. Each question is
// handed as question<n>.bin and cases<n>.txt, and as a line of questions.txt in the form a carrier's list takes,
// `<code> <registers> <cases> <answers> <slot> <launches>`, the list handed whole once the channel closes, and it reads
// RUN_HELD. What the protocol put is then checked by handing that list to a carrier that stops before the part. A
// carrier whose own words hold --dry stops before the driver too, and every round reaches it through the airlock as a
// live round does: nothing is launched, and R is read and nothing it answers is kept in R.
//
// Every round a live channel carries passes the airlock first, and nothing the vendor's reader has not read goes to the
// part. The words after the carrier's may open one: `--pop <words>`, the carrier with the part taken out, which hands
// each question's container (`--list <list>` put after its words), and `--cross <words>`, the vendor's reader over the
// containers (the list put after its first word), which hands the held list the carrier reads by its `--held <name>`.
// Before each round is carried, its list is handed as airlock.txt, each question's answers named airlock_ before its
// own name, the pop and the cross are run over it, and the round is carried only where the cross read it and handed
// the held list after it; the carrier then holds every code the vendor called out (cubin_safe.h). A channel with no
// airlock carries a round only where its carrier is handed a held list of a cross that read the whole pass before it.
// A round the airlock does not pass is carried by nothing and reads RUN_NO_CHANNEL, the airlock's reason its refusal.
//
// The round is checked, fixed and checked again before the part. The cross is handed one word more, the name of the
// holds it hands the teacher, a line a hold with the count of registers that clears it (measuring_stick_query.c): a
// question of no shape of its own held only for registers past the count its kernel allots is given the count that
// clears every hold of it, and the round is listed again and read again, until no question is given one. A count
// changes how a question is put and never what it asks. Every hold, every question kept held and every count given is
// a line of the teacher's trace (QRY_TRACE, the path `teacher`), `teacher: <identity> checked: <what>`, so that a
// branch the learner took is traced back to the check that put its question
int run_channel_open(const char *const *carrier, unsigned long long limit_microseconds);

// R, the part's record at `path` (kqr.h), opened for the channel: an untimed question R holds an answer to is
// answered from R and never carried, one it holds timed out is carried again, and what comes back of every question
// carried is kept in R under a cycle of `mode`. R is written back when the channel is closed, where it changed and
// was read whole. 1, or 0 with the reason printed. A dry run reads R and keeps nothing in it
int run_channel_record(const char *path, const char *member, const char *mode);

// How many registers past the highest its code names a container declares, read for the channel from the part's .ksc
// at `ksc` as the part answered it (`run answers <past> register past`, sass_machine.h): a hold of the cross for a
// register past a kernel's count is put again with the count that reaches that register by so many. Where the .ksc
// holds no answer, or this is not called, no count clears such a hold and the question stays held. 1 where it holds
// one, 0 where not
int run_channel_registers_past(const char *ksc);

// 1 where `refused`, what a carrier said of a question, is the gate's hold for an unknown operation key
// (RUN_GATE_KEY_RULE)
int run_channel_key_held(const char *refused);

// `asked` put and answered. 1 where the outcome is RUN_ANSWERED, 0 otherwise, with `asked->outcome` saying which
int run_channel_ask(RunQuestion *asked);

// The `count` questions of `asked`, each of no shape of its own, put together: carried in one process, each answered
// as it is carried, and each timed one timed over its own launches. A question the part refuses ends that process,
// its ending is its answer, and the questions after it are carried in a process of their own, so that one refusal
// costs no other question its answer. The count of those answered, each with its own outcome
unsigned int run_channel_ask_many(RunQuestion *const *asked, unsigned int count);

// the channel closed, R written back where it was opened, and what it held given back
void run_channel_close(void);

// the nanoseconds the airlock, its pop and its cross within it, and the carrier have taken since the channel opened,
// each into its own
void run_channel_clocks(unsigned long long *airlock, unsigned long long *pop, unsigned long long *cross,
                        unsigned long long *carried);

// what the channel is carried by, for a run to say where its words came from
const char *run_channel_carrier(void);

#ifdef __cplusplus
}
#endif

#endif
