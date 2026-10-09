// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_write.c: a part's forms learned by asking the part, and its .khw written from the answers
//
//     khw_write <part> <path.khw> <layout> <mnemonics> <folder> <machine writer> [<rounds>] -- <carrier word>...
//
// Every form in the file this writes is there because the part answered for it (P8). Nothing reads a listing, a
// disassembler or a vendor's table of operations: the part is handed code and cases over the run channel
// (run_channel.h), the host computes the same cases off the ladder's relations (ladder.h), and a form enters the file
// where the part's answer and the host's agree on every case of one relation.
//
// The kernel is the code the container the system accepted already holds (the layout's container rows, `layout`
// above). It loads a case's first two words into two registers, computes one instruction, and stores the result as
// the case's answer. That one instruction is the slot, and which place of the kernel it stands at is asked: the kernel
// is put with each place holding the encoding its unreached places hold, and the place whose answer comes back as the
// case's second word is the slot, because nothing then wrote the register the kernel stores.
//
// The first question is the kernel's own relation: the kernel the system accepted, every instruction of it waiting on
// all six barriers and otherwise unchanged, over every case of each relation of the ladder, and the launches of
// the one that answers timed, the time the part takes to run it. Every question after is put from what that one
// answered.
//
// What is asked of a form, in order:
//
//   the relation   the form put over every case of each relation of the ladder, both orders of the case's two words.
//                  The relation every case answers is the form's, and the order that answered is the order its
//                  operands are read in. A form that answers no relation on every case is kept out of the file.
//   the fields     each bit from 12 to 104 turned one at a time, the key and the scheduler's own word left alone
//                  (P9), and the runs of adjacent bits whose turning changes the answer kept. A run set to the number
//                  of each of the two registers the kernel left a case word in answers two different words where it
//                  carries a register, and one word where it does not: an operand's run answers two words and a
//                  modifier's run one. The run whose setting leaves the answer at the case's second word is where the form
//                  writes its result, and every other run is a source it reads.
//   the name       no name is composed here: the protocol keeps the relation answered, how many sources the fields
//                  found, and whether the answers read the form signed, and the vendor's writer (`machine writer`
//                  above, run through the interface) names it from its own table (mnemonic_nvidia.tsv) and writes it.
//
// Then the forms found are widened: each bit outside every run, the key and the scheduler's word is turned, and the
// turned encoding is asked its relation from the top. A turn that answers a relation is a form of its own. Widening
// repeats over what it finds for `rounds` rounds, 1 where none is given.
//
// The gate on the host (cubin_safe.h) reads every question against the .khw the vendor's writer lays out, so that
// file is written before asking and the carrier is handed that same path. The vendor's writer runs in a gate mode,
// which holds every place of the kernel whole under its own encoding so the gate reads a question against what the
// part has already run, and a final mode, which at the end holds only the forms the part named.
#include "../protocol/counterexample/ladder.h"
#include "../protocol/gate/survivors.h"
#include "../protocol/order/scheduler.h"
#include "../protocol/query/answer_read.h"
#include "../protocol/teacher/run_channel.h"

#include "../interface/interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most places a kernel's code holds, and the bytes of one instruction
#define KHW_PLACES 256u
#define KHW_INSTRUCTION 16u

// the bits a turn reaches: past the operation key, which names the operation and is never turned, and below the
// scheduler's own word, which is set once for a run (P9)
#define KHW_TURN_FIRST 12u
#define KHW_TURN_LAST 104u

// the most runs a form's operands sit in, and the most forms the part answers for
#define KHW_RUNS 32u
#define KHW_FORMS 16384u

// the two registers the kernel leaves a case's words in, found by asking: a run set to one of these numbers reads that
// register, and the two words differ: an operand field answers two different words, a modifier one
#define KHW_FIRST_REGISTER 0u
#define KHW_SECOND_REGISTER 7u

// the name a form's relation is written under in the answers file, for the vendor's writer to name it from
#define KHW_READS_LADDER "ladder."

// how many registers a thread of the kernel holds, read with the kernel
static unsigned int s_registers;

// the kernel, the question's code written from it, how many places it holds, the place the slot stands at, and the
// encoding its unreached places hold
static unsigned char s_kernel_text[KHW_PLACES * KHW_INSTRUCTION];
static unsigned char s_code[KHW_PLACES * KHW_INSTRUCTION];
static unsigned int s_kernel_places;
static unsigned int s_slot;
static unsigned long long s_unreached_low;
static unsigned long long s_unreached_high;

// the path the file is written to, the part named in it, the container layout and the vendor's table the vendor's
// writer is handed, the folder its output is written to, and the vendor's writer itself
static const char *s_path;
static const char *s_part;
static const char *s_layout;
static const char *s_mnemonics;
static const char *s_folder;
static const char *s_machine_writer;

// one question, how many the run has put, and the launches the question is timed over, 0 where it is untimed
static RunQuestion s_question;
static unsigned long long s_asks;
static unsigned int s_launches;

// One round's questions, SCHEDULER_ROUND_MOST at most, each with the code it carries, how many it settles and its
// place in the round; how many the round holds
static RunQuestion *s_pool;
static unsigned char s_pool_code[SCHEDULER_ROUND_MOST][KHW_PLACES * KHW_INSTRUCTION];
static RunQuestion *s_pool_question[SCHEDULER_ROUND_MOST];
static unsigned int s_pool_settles[SCHEDULER_ROUND_MOST];
static unsigned int s_pooled;

// the launches the kernel's own question is timed over, as a task's cost is read (P11, Step 10)
#define KHW_LAUNCHES 100u

// What the part answered of one form: the relation every case of it answered, whether the case's words were read in
// the order the host put them or the other way round, whether the answers read a word signed, and the runs of bits
// its operands sit in with the place each run writes or reads
typedef struct
{
    unsigned long long low;
    unsigned long long high;
    unsigned int anchor;
    int swapped;
    int signed_read;
    int signedness_asked;
    unsigned int runs;
    unsigned int first[KHW_RUNS];
    unsigned int last[KHW_RUNS];
    // the run the form writes its result through, past every run where the part answered none
    unsigned int written;
    // the count of the relation read's candidates that survived the gate, its reading (Q11)
    unsigned int survivors;
} KhwAnswered;

// one word of an instruction, and one written back
static unsigned long long khw_word_read(const unsigned char *instruction, unsigned int byte)
{
    unsigned long long value = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        value |= (unsigned long long)instruction[byte + at] << (8u * at);
    }
    return value;
}

static void khw_word_write(unsigned char *instruction, unsigned int byte, unsigned long long value)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        instruction[byte + at] = (unsigned char)((value >> (8u * at)) & 0xffull);
    }
}

// the bits `first` to `last` of the instruction `low` and `high` make, and the pair with that run set to `value`
static unsigned long long khw_run_read(unsigned long long low, unsigned long long high, unsigned int first,
                                       unsigned int last)
{
    unsigned long long value = 0ull;
    for (unsigned int bit = first; bit <= last; bit += 1u)
    {
        const unsigned long long word = (bit < 64u) ? low : high;
        value |= ((word >> (bit % 64u)) & 1ull) << (bit - first);
    }
    return value;
}

static void khw_run_write(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int last,
                          unsigned long long value)
{
    for (unsigned int bit = first; bit <= last; bit += 1u)
    {
        unsigned long long *const word = (bit < 64u) ? low : high;
        const unsigned long long one = (value >> (bit - first)) & 1ull;
        *word = (*word & ~(1ull << (bit % 64u))) | (one << (bit % 64u));
    }
}

// the kernel into `code`, as the part accepted it. The carrier makes every instruction wait on all six barriers
// before the part sees it; nothing of the scheduler's own word is set here (P9)
static void khw_kernel_put(unsigned char *code)
{
    memcpy(code, s_kernel_text, (size_t)s_kernel_places * KHW_INSTRUCTION);
}

// The question's code, into `code`: the kernel with `low` and `high` at `place`. The slot the question names is sent
// to the carrier, which there makes the slot wait on all six barriers, stall the longest and hold no barrier; a
// form the part has not named then waits on everything, and a barrier nothing releases leaves the wait after it standing
// (P9, cubin_safe.h)
static void khw_code_put(unsigned char *code, unsigned int place, unsigned long long low, unsigned long long high)
{
    khw_kernel_put(code);
    khw_word_write(&code[place * KHW_INSTRUCTION], 0u, low);
    khw_word_write(&code[place * KHW_INSTRUCTION], 8u, high);
}

// The question in s_code put over `count` cases, each case's two words from `word`, the carrier making the slot at
// `slot` conservative (RUN_SLOT_NONE where the code holds no slot), and each answer into `answered`. 1 where the part
// answered every case, 0 where it refused the question, the gate held it off the part or nothing carried it
static int khw_asked(const unsigned int word[][2], unsigned int count, unsigned int slot, unsigned long long *answered)
{
    memset(&s_question, 0, sizeof(s_question));
    s_question.code = s_code;
    s_question.code_size = (unsigned long long)s_kernel_places * KHW_INSTRUCTION;
    s_question.registers = s_registers;
    s_question.cases = count;
    s_question.slot = slot;
    s_question.launches = s_launches;
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        s_question.word[place][0] = word[place][0];
        s_question.word[place][1] = word[place][1];
    }
    s_asks += 1ull;
    if (!run_channel_ask(&s_question))
    {
        return 0;
    }
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        answered[place] = s_question.answered[place];
    }
    return 1;
}

// A question added to the round being written: the kernel with `low` and `high` at `place`, or the kernel as it is
// where `place` is past its places, over `count` cases from `word`, settling one entry. Its place in the round, or
// SCHEDULER_ROUND_MOST where the round is full
static unsigned int khw_pooled(unsigned int place, unsigned long long low, unsigned long long high,
                               const unsigned int word[][2], unsigned int count)
{
    if (s_pooled == SCHEDULER_ROUND_MOST)
    {
        return SCHEDULER_ROUND_MOST;
    }
    const unsigned int at = s_pooled;
    if (place < s_kernel_places)
    {
        khw_code_put(s_pool_code[at], place, low, high);
    }
    else
    {
        khw_kernel_put(s_pool_code[at]);
    }
    RunQuestion *const question = &s_pool[at];
    memset(question, 0, sizeof(*question));
    question->code = s_pool_code[at];
    question->code_size = (unsigned long long)s_kernel_places * KHW_INSTRUCTION;
    question->registers = s_registers;
    question->cases = count;
    question->slot = (place < s_kernel_places) ? place : RUN_SLOT_NONE;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        question->word[each][0] = word[each][0];
        question->word[each][1] = word[each][1];
    }
    s_pool_question[at] = question;
    s_pool_settles[at] = 1u;
    s_pooled += 1u;
    return at;
}

// The round written carried as one (scheduler.h), its questions read after from s_pool, and the next round written
// in its place once they are
static void khw_round_carried(void)
{
    scheduler_round(s_pool_question, s_pool_settles, s_pooled);
    s_asks += s_pooled;
}

#define LADDER_TEXT(name_, text_, words_, measured_) text_,
static const char *const s_anchor_text[] = {LADDER_ANCHORS(LADDER_TEXT)};
#undef LADDER_TEXT

// What the host answers for `anchor` on `word` where the word it reads is signed. The two readings of a case differ
// only where a word is moved toward the low end: the high place comes back as itself read signed and as nothing read
// unsigned. Every other relation of the ladder answers alike either way at one width
static unsigned int khw_signed_answer(unsigned int anchor, const unsigned int *word)
{
    unsigned int answered = 0u;
    if (anchor != (unsigned int)LADDER_DOWN)
    {
        return ladder_answer(anchor, word, 2u, &answered) ? answered : 0u;
    }
    const unsigned int count = word[1] & (LADDER_PLACES_ASSUMED - 1u);
    if (count == 0u)
    {
        return word[0];
    }
    const unsigned int moved = word[0] >> count;
    const unsigned int filled = ((word[0] & 0x80000000u) != 0u) ? (0xffffffffu << (LADDER_PLACES_ASSUMED - count)) : 0u;
    return moved | filled;
}

// the cases of `anchor` the host computes both readings of, their words into `word` in the order `swapped` says, and
// each reading into `plain` and `signed_read`. The count found
static unsigned int khw_anchor_cases(unsigned int anchor, int swapped, unsigned int word[][2], unsigned int *plain,
                                     unsigned int *signed_read)
{
    unsigned int found = 0u;
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        const LadderQuestion *const held = &s_ladder_cases[at];
        unsigned int answered = 0u;
        if ((held->anchor != anchor) || (held->words < 2u) ||
            !ladder_answer(anchor, held->word, held->words, &answered) || (found == RUN_CASES_MOST))
        {
            continue;
        }
        word[found][0] = (swapped != 0) ? held->word[1] : held->word[0];
        word[found][1] = (swapped != 0) ? held->word[0] : held->word[1];
        plain[found] = answered;
        signed_read[found] = khw_signed_answer(anchor, held->word);
        found += 1u;
    }
    return found;
}

// one candidate of the relation read: a relation of the ladder, the case's two words read in the order the host put
// them or swapped, and the answer read plain or signed
typedef struct
{
    unsigned int anchor;
    int swapped;
    int signed_read;
} KhwCandidate;

// the host's answer of `candidate` to the case of words `word`
static unsigned int khw_candidate_answer(const KhwCandidate *candidate, const unsigned int *word)
{
    const unsigned int read[2] = {(candidate->swapped != 0) ? word[1] : word[0],
                                  (candidate->swapped != 0) ? word[0] : word[1]};
    unsigned int answered = 0u;
    if (candidate->signed_read != 0)
    {
        return khw_signed_answer(candidate->anchor, read);
    }
    return ladder_answer(candidate->anchor, read, 2u, &answered) ? answered : 0u;
}

// The cases a relation is read over, written once: every two-word case of every relation of the ladder that is no
// measure, in both orders, each once; and the candidates, each such relation, its words read in order or swapped, and
// read signed beside plain where a case of it reads two ways
static unsigned int s_union_word[GATE_CASES][2];
static unsigned int s_union_cases;
static KhwCandidate s_candidate[GATE_CANDIDATES];
static unsigned int s_candidates;

static void khw_union_written(void)
{
    if (s_union_cases != 0u)
    {
        return;
    }
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        const LadderQuestion *const question = &s_ladder_cases[at];
        if ((question->words < 2u) || (s_ladder_measured[question->anchor] != 0))
        {
            continue;
        }
        for (unsigned int order = 0u; order < 2u; order += 1u)
        {
            const unsigned int first = (order == 0u) ? question->word[0] : question->word[1];
            const unsigned int second = (order == 0u) ? question->word[1] : question->word[0];
            int held_already = 0;
            for (unsigned int place = 0u; (place < s_union_cases) && !held_already; place += 1u)
            {
                held_already = (s_union_word[place][0] == first) && (s_union_word[place][1] == second);
            }
            if (!held_already && (s_union_cases < GATE_CASES))
            {
                s_union_word[s_union_cases][0] = first;
                s_union_word[s_union_cases][1] = second;
                s_union_cases += 1u;
            }
        }
    }
    for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        if (s_ladder_measured[anchor] != 0)
        {
            continue;
        }
        for (int swapped = 0; swapped < 2; swapped += 1)
        {
            const KhwCandidate plain = {anchor, swapped, 0};
            const KhwCandidate read_signed = {anchor, swapped, 1};
            int reads_two_ways = 0;
            for (unsigned int place = 0u; place < s_union_cases; place += 1u)
            {
                reads_two_ways = reads_two_ways || (khw_candidate_answer(&plain, s_union_word[place]) !=
                                                    khw_candidate_answer(&read_signed, s_union_word[place]));
            }
            if (s_candidates < GATE_CANDIDATES)
            {
                s_candidate[s_candidates] = plain;
                s_candidates += 1u;
            }
            if (reads_two_ways && (s_candidates < GATE_CANDIDATES))
            {
                s_candidate[s_candidates] = read_signed;
                s_candidates += 1u;
            }
        }
    }
}

// The relation `question`, put over the cases khw_union_written writes, answers, read by the gate (P2). Each case of
// each candidate is read as P11 reads it against the host's answer, and a candidate fails a case it does not read
// alike: no cost enters S, and the survivors are the conjunction over every case. The first survivor in the ladder's
// order, in order before swapped and plain before signed, is kept in `held` with the count of survivors, its reading
// (Q11). 1 where one survived, 0 where none did
static int khw_relation_of(const RunQuestion *question, KhwAnswered *held)
{
    static unsigned char s_fails[GATE_CANDIDATES][GATE_CASES];
    held->anchor = (unsigned int)LADDER_ANCHOR_COUNT;
    held->swapped = 0;
    held->signed_read = 0;
    held->signedness_asked = 0;
    held->survivors = 0u;
    for (unsigned int candidate = 0u; candidate < s_candidates; candidate += 1u)
    {
        for (unsigned int place = 0u; place < s_union_cases; place += 1u)
        {
            const unsigned int host = khw_candidate_answer(&s_candidate[candidate], s_union_word[place]);
            const AnswerBracket bracket = {1, host, host};
            s_fails[candidate][place] =
                (answer_read(&bracket, question->outcome, question->answered[place]) != ANSWER_ALIKE) ? 1u : 0u;
        }
    }
    unsigned char survives[GATE_CANDIDATES];
    held->survivors = gate_survivors(s_fails, s_candidates, s_union_cases, survives);
    for (unsigned int candidate = 0u; candidate < s_candidates; candidate += 1u)
    {
        if (survives[candidate] == 0u)
        {
            continue;
        }
        held->anchor = s_candidate[candidate].anchor;
        held->swapped = s_candidate[candidate].swapped;
        held->signed_read = s_candidate[candidate].signed_read;
        // a relation no case of which reads two ways leaves the part nothing to answer about signedness
        for (unsigned int other = 0u; other < s_candidates; other += 1u)
        {
            held->signedness_asked = held->signedness_asked ||
                                     ((s_candidate[other].anchor == held->anchor) &&
                                      (s_candidate[other].swapped == held->swapped) && s_candidate[other].signed_read);
        }
        return 1;
    }
    return 0;
}

// The relation the code in s_code answers, asked of the part in one ask over the cases khw_union_written writes and
// read by the gate (khw_relation_of). 1 where one relation survived
static int khw_relation_read(KhwAnswered *held)
{
    static unsigned long long s_answered[RUN_CASES_MOST];
    khw_union_written();
    khw_asked(s_union_word, s_union_cases, s_slot, s_answered);
    return khw_relation_of(&s_question, held);
}

// the relation `low` and `high` at the slot answers, read as khw_relation_read reads it. 1 where one relation answered
static int khw_relation_asked(unsigned long long low, unsigned long long high, KhwAnswered *held)
{
    held->low = low;
    held->high = high;
    khw_code_put(s_code, s_slot, low, high);
    return khw_relation_read(held);
}

// The first round: the kernel's own relation and the slot, put together, since neither waits on the other's answer.
// The kernel's question is the kernel the system accepted, every instruction of it waiting on all six barriers and
// otherwise unchanged, over the cases khw_union_written writes, its relation read by the gate. The slot's are
// the kernel with each place but the unreached in turn holding the encoding its unreached places hold, over one case,
// and the slot is the first place whose answer comes back as the case's second word, because nothing then wrote the
// register the kernel stores. The relation that answered is then put once more over its own cases, its launches timed
// into `nanoseconds`, 0 where the part gave no time. 1 where the kernel answered a relation, its slot through `slot`,
// past every place where none answered so
static int khw_first_round(KhwAnswered *kernel, unsigned int *slot, unsigned long long *nanoseconds)
{
    static unsigned int s_word[RUN_CASES_MOST][2];
    static unsigned int s_plain[RUN_CASES_MOST];
    static unsigned int s_signed[RUN_CASES_MOST];
    static unsigned long long s_answered[RUN_CASES_MOST];
    unsigned int place_of[SCHEDULER_ROUND_MOST];
    const unsigned int slot_case[1][2] = {{3u, 5u}};
    *nanoseconds = 0ull;
    *slot = s_kernel_places;
    s_unreached_low = khw_word_read(&s_kernel_text[(s_kernel_places - 1u) * KHW_INSTRUCTION], 0u);
    s_unreached_high = khw_word_read(&s_kernel_text[(s_kernel_places - 1u) * KHW_INSTRUCTION], 8u);
    khw_union_written();
    s_pooled = 0u;
    const unsigned int kernel_at = khw_pooled(s_kernel_places, 0ull, 0ull, s_union_word, s_union_cases);
    for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        if ((low == s_unreached_low) && (high == s_unreached_high))
        {
            continue;
        }
        const unsigned int at = khw_pooled(place, s_unreached_low, s_unreached_high, slot_case, 1u);
        if (at < SCHEDULER_ROUND_MOST)
        {
            place_of[at] = place;
        }
    }
    khw_round_carried();
    const int related = khw_relation_of(&s_pool[kernel_at], kernel);
    for (unsigned int at = kernel_at + 1u; (at < s_pooled) && (*slot == s_kernel_places); at += 1u)
    {
        *slot = ((s_pool[at].outcome == RUN_ANSWERED) && (s_pool[at].answered[0] == (unsigned long long)slot_case[0][1]))
                    ? place_of[at]
                    : *slot;
    }
    s_pooled = 0u;
    if (!related)
    {
        return 0;
    }
    khw_kernel_put(s_code);
    const unsigned int count = khw_anchor_cases(kernel->anchor, kernel->swapped, s_word, s_plain, s_signed);
    s_launches = KHW_LAUNCHES;
    const int timed = khw_asked(s_word, count, RUN_SLOT_NONE, s_answered);
    s_launches = 0u;
    *nanoseconds = timed ? s_question.nanoseconds : 0ull;
    return 1;
}

// the first case of `anchor` whose two words differ from each other and whose answer differs from both, read in the
// order `swapped` says: the case a turn is asked over, where a changed answer is a changed answer and not a
// coincidence. 1, or 0 where the relation holds no such case
static int khw_turning_case(unsigned int anchor, int swapped, unsigned int *word, unsigned int *answered)
{
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        const LadderQuestion *const held = &s_ladder_cases[at];
        unsigned int gave = 0u;
        if ((held->anchor != anchor) || (held->words < 2u) || !ladder_answer(anchor, held->word, held->words, &gave) ||
            (held->word[0] == held->word[1]) || (gave == held->word[0]) || (gave == held->word[1]))
        {
            continue;
        }
        word[0] = (swapped != 0) ? held->word[1] : held->word[0];
        word[1] = (swapped != 0) ? held->word[0] : held->word[1];
        *answered = gave;
        return 1;
    }
    return 0;
}

// The first round of `held`'s fields put: each bit from KHW_TURN_FIRST to KHW_TURN_LAST turned one at a time over
// `word`, each a question of the slot, and the round carried. What came back is left in s_pool, a question a turned
// bit in its order. The code a turned bit holds is the form's with that one bit over, and no answer is needed to put
// it: a dry run writes every one of them to the folder, where the vendor's own disassembler reads them back
static void khw_turns_asked(const KhwAnswered *held, const unsigned int word[1][2])
{
    s_pooled = 0u;
    for (unsigned int bit = KHW_TURN_FIRST; bit <= KHW_TURN_LAST; bit += 1u)
    {
        unsigned long long low = held->low;
        unsigned long long high = held->high;
        khw_run_write(&low, &high, bit, bit, khw_run_read(low, high, bit, bit) ^ 1ull);
        khw_pooled(s_slot, low, high, word, 1u);
    }
    khw_round_carried();
}

// The runs of bits the operands of `held` sit in, asked of the part: each bit from 12 to 104 turned one at a time
// over one case, the adjacent bits whose turning changes the answer gathered into runs, and each run set to the
// number of each of the two registers the kernel left a case word in. A run that answers two different words there
// carries a register and is an operand's; a run that answers one is a modifier and is left out. The run whose
// setting leaves the answer at the register the kernel stores is where the form writes its result. The case is put
// first alone; every turned bit, which waits on that answer and on no other, is one round; and every run's two
// registers, which wait on the turned bits' answers, another
static void khw_fields_asked(KhwAnswered *held)
{
    unsigned int word[1][2];
    unsigned int expected = 0u;
    unsigned long long answered[1];
    held->runs = 0u;
    held->written = KHW_RUNS;
    if (!khw_turning_case(held->anchor, held->swapped, word[0], &expected))
    {
        return;
    }
    khw_code_put(s_code, s_slot, held->low, held->high);
    if (!khw_asked(word, 1u, s_slot, answered) || (answered[0] != (unsigned long long)expected))
    {
        return;
    }
    const unsigned long long base = answered[0];
    int turned[KHW_TURN_LAST + 1u];
    khw_turns_asked(held, word);
    for (unsigned int bit = KHW_TURN_FIRST; bit <= KHW_TURN_LAST; bit += 1u)
    {
        const RunQuestion *const question = &s_pool[bit - KHW_TURN_FIRST];
        turned[bit] = ((question->outcome == RUN_ANSWERED) && (question->answered[0] != base)) ? 1 : 0;
    }
    // the adjacent bits that changed the answer, each run then asked whether it carries a register
    unsigned int first[KHW_RUNS];
    unsigned int last[KHW_RUNS];
    unsigned int runs = 0u;
    for (unsigned int bit = KHW_TURN_FIRST; (bit <= KHW_TURN_LAST) && (runs < KHW_RUNS); bit += 1u)
    {
        if (turned[bit] == 0)
        {
            continue;
        }
        unsigned int end = bit;
        while ((end < KHW_TURN_LAST) && (turned[end + 1u] != 0))
        {
            end += 1u;
        }
        first[runs] = bit;
        last[runs] = end;
        runs += 1u;
        bit = end;
    }
    s_pooled = 0u;
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        unsigned long long low = held->low;
        unsigned long long high = held->high;
        khw_run_write(&low, &high, first[run], last[run], KHW_FIRST_REGISTER);
        khw_pooled(s_slot, low, high, word, 1u);
        low = held->low;
        high = held->high;
        khw_run_write(&low, &high, first[run], last[run], KHW_SECOND_REGISTER);
        khw_pooled(s_slot, low, high, word, 1u);
    }
    if (runs != 0u)
    {
        khw_round_carried();
    }
    for (unsigned int run = 0u; run < runs; run += 1u)
    {
        const RunQuestion *const first_question = &s_pool[2u * run];
        const RunQuestion *const second_question = &s_pool[(2u * run) + 1u];
        const int first_held = (first_question->outcome == RUN_ANSWERED);
        const int second_held = (second_question->outcome == RUN_ANSWERED);
        const unsigned long long first_answer = first_question->answered[0];
        const unsigned long long second_answer = second_question->answered[0];
        if (first_held && second_held && (first_answer != second_answer) && (held->runs < KHW_RUNS))
        {
            // the run that leaves the answer at the word the kernel's own register still holds wrote nothing: that is
            // where this form writes its result
            if ((first_answer == (unsigned long long)word[0][1]) || (second_answer == (unsigned long long)word[0][1]))
            {
                held->written = held->runs;
            }
            held->first[held->runs] = first[run];
            held->last[held->runs] = last[run];
            held->runs += 1u;
        }
    }
    s_pooled = 0u;
}

static int khw_vendor_run(const char *mode, const char *answers);

// the kernel the vendor's writer reads out of the container, got through the interface and read back from the file it
// writes (khw_machine_write, kernel mode): the places it holds into s_kernel_places, the registers a thread holds into
// s_registers, and each place's encoding into s_kernel_text. 1, or 0 with the reason printed
static int khw_kernel_read(void)
{
    char kernel_path[1024];
    snprintf(kernel_path, sizeof(kernel_path), "%s/kernel.txt", s_folder);
    if (!khw_vendor_run("kernel", kernel_path))
    {
        return 0;
    }
    FILE *const file = fopen(kernel_path, "rb");
    if (file == NULL)
    {
        printf("  khw_write: %s could not be read\n", kernel_path);
        return 0;
    }
    char line[256];
    if ((fgets(line, sizeof(line), file) == NULL) ||
        (sscanf(line, "kernel %u %u", &s_kernel_places, &s_registers) != 2) || (s_kernel_places == 0u) ||
        (s_kernel_places > KHW_PLACES) || (s_registers == 0u))
    {
        printf("  khw_write: %s holds no kernel this reads\n", kernel_path);
        fclose(file);
        return 0;
    }
    for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
    {
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        if ((fgets(line, sizeof(line), file) == NULL) || (sscanf(line, "%llx %llx", &low, &high) != 2))
        {
            printf("  khw_write: %s holds %u places, fewer than its header says\n", kernel_path, place);
            fclose(file);
            return 0;
        }
        khw_word_write(&s_kernel_text[place * KHW_INSTRUCTION], 0u, low);
        khw_word_write(&s_kernel_text[place * KHW_INSTRUCTION], 8u, high);
    }
    fclose(file);
    return 1;
}

// the forms the part has answered for, each with the relation it answered
static KhwAnswered s_named[KHW_FORMS];
static unsigned int s_names;

// 1 where `low` and `high` is already one of the forms the part has answered for
static int khw_named_already(unsigned long long low, unsigned long long high)
{
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        if ((s_named[at].low == low) && (s_named[at].high == high))
        {
            return 1;
        }
    }
    return 0;
}

// `low` and `high` asked its relation and its fields, and kept among the forms the part has answered for where it
// answered one, whether the vendor's table names it: what the part answered is the protocol's, and only the
// file written at the end takes a name (P14). 1 where it was kept
static int khw_form_asked(unsigned long long low, unsigned long long high)
{
    if (khw_named_already(low, high) || (s_names == KHW_FORMS))
    {
        return 0;
    }
    KhwAnswered held;
    memset(&held, 0, sizeof(held));
    if (!khw_relation_asked(low, high, &held))
    {
        return 0;
    }
    khw_fields_asked(&held);
    s_named[s_names] = held;
    s_names += 1u;
    printf("    ladder.%-8s %016llx %016llx  %u runs, %u surviving\n", s_anchor_text[held.anchor], low, high,
           held.runs, held.survivors);
    return 1;
}

// Every form the part answered for widened: each bit outside its runs, its key and the scheduler's word turned, and
// the turned encoding asked its relation from the top. Every turned encoding's relation waits on no other answer, and
// they are put SCHEDULER_ROUND_MOST to a round; each that answered one is then asked its fields. The count found this
// round
static unsigned int khw_widened(unsigned int from)
{
    const unsigned int until = s_names;
    unsigned int found = 0u;
    // every turned encoding not yet answered for, each once
    static unsigned long long s_turned_low[KHW_FORMS];
    static unsigned long long s_turned_high[KHW_FORMS];
    unsigned int turned = 0u;
    for (unsigned int at = from; at < until; at += 1u)
    {
        const KhwAnswered held = s_named[at];
        for (unsigned int bit = KHW_TURN_FIRST; (bit <= KHW_TURN_LAST) && (turned < KHW_FORMS); bit += 1u)
        {
            int inside = 0;
            for (unsigned int run = 0u; run < held.runs; run += 1u)
            {
                inside = inside || ((bit >= held.first[run]) && (bit <= held.last[run]));
            }
            unsigned long long low = held.low;
            unsigned long long high = held.high;
            khw_run_write(&low, &high, bit, bit, khw_run_read(low, high, bit, bit) ^ 1ull);
            int seen = inside || khw_named_already(low, high);
            for (unsigned int other = 0u; (other < turned) && !seen; other += 1u)
            {
                seen = (s_turned_low[other] == low) && (s_turned_high[other] == high);
            }
            if (!seen)
            {
                s_turned_low[turned] = low;
                s_turned_high[turned] = high;
                turned += 1u;
            }
        }
    }
    khw_union_written();
    static KhwAnswered s_related[SCHEDULER_ROUND_MOST];
    for (unsigned int first = 0u; first < turned; first += SCHEDULER_ROUND_MOST)
    {
        const unsigned int last = ((first + SCHEDULER_ROUND_MOST) < turned) ? (first + SCHEDULER_ROUND_MOST) : turned;
        s_pooled = 0u;
        for (unsigned int at = first; at < last; at += 1u)
        {
            khw_pooled(s_slot, s_turned_low[at], s_turned_high[at], s_union_word, s_union_cases);
        }
        khw_round_carried();
        unsigned int related = 0u;
        for (unsigned int at = first; at < last; at += 1u)
        {
            KhwAnswered held;
            memset(&held, 0, sizeof(held));
            held.low = s_turned_low[at];
            held.high = s_turned_high[at];
            if (khw_relation_of(&s_pool[at - first], &held))
            {
                s_related[related] = held;
                related += 1u;
            }
        }
        s_pooled = 0u;
        for (unsigned int at = 0u; (at < related) && (s_names < KHW_FORMS); at += 1u)
        {
            KhwAnswered held = s_related[at];
            khw_fields_asked(&held);
            s_named[s_names] = held;
            s_names += 1u;
            found += 1u;
            printf("    ladder.%-8s %016llx %016llx  %u runs, %u surviving\n", s_anchor_text[held.anchor], held.low,
                   held.high, held.runs, held.survivors);
        }
    }
    return found;
}

// the forms the part answered for written to the answers file at `path`, one a line, for the vendor's writer to name
// and lay out: the relation the gate read, whether the answers read a word signed and whether the part was asked, the
// encoding, and each run's first and last bit. 1, or 0 with the reason printed
static int khw_answers_written(const char *path)
{
    FILE *const file = fopen(path, "wb");
    if (file == NULL)
    {
        printf("  khw_write: %s could not be written\n", path);
        return 0;
    }
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const held = &s_named[at];
        fprintf(file, "%s%s %d %d %016llx %016llx %u", KHW_READS_LADDER, s_anchor_text[held->anchor],
                held->signed_read, held->signedness_asked, held->low, held->high, held->runs);
        for (unsigned int run = 0u; run < held->runs; run += 1u)
        {
            fprintf(file, " %u %u", held->first[run], held->last[run]);
        }
        fprintf(file, "\n");
    }
    fclose(file);
    return 1;
}

// The vendor's writer run through the interface in `mode`, handed the part, the file to write, the container layout,
// the vendor's table and the answers file. Its output is read and printed. 1 where it ended clean
static int khw_vendor_run(const char *mode, const char *answers)
{
    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/machine.txt", s_folder);
    char *const command[] = {(char *)s_machine_writer, (char *)s_part,  (char *)s_path, (char *)s_layout,
                             (char *)s_mnemonics,       (char *)answers, (char *)mode,   NULL};
    static char s_output[65536];
    const InterfaceProbe probe = {command, output_path, 0ull};
    InterfaceAnswer answer = {0};
    answer.output = s_output;
    answer.output_capacity = sizeof(s_output);
    EngineError error = {0};
    const long ran = interface_probe_run(&probe, &answer, &error);
    if (answer.output_bytes != 0ull)
    {
        printf("%s", s_output);
    }
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        printf("  khw_write: the vendor's writer %s did not end clean (%s, code %llx)\n", mode,
               interface_ending_name(answer.ending), answer.code);
        return 0;
    }
    return 1;
}

// The slot's first field-finding round emitted off the part: the kernel's instruction at `slot` turned one bit at a
// time and each put as a question, none carried; a dry run writes the round to the folder for the vendor's own
// disassembler to read back. The slot is the protocol's own, read from a part before; the vendor names none of the
// questions and no byte of its knowledge enters a form. It is how the process is tuned against questions the part has
// not yet answered, off the part (measuring_stick_query.sh)
static void khw_enumerate(unsigned int slot)
{
    s_slot = slot;
    KhwAnswered held;
    memset(&held, 0, sizeof(held));
    held.low = khw_word_read(&s_kernel_text[slot * KHW_INSTRUCTION], 0u);
    held.high = khw_word_read(&s_kernel_text[slot * KHW_INSTRUCTION], 8u);
    const unsigned int word[1][2] = {{3u, 5u}};
    khw_turns_asked(&held, word);
}

int main(int count, char **word)
{
    int split = 0;
    while ((split < count) && (strcmp(word[split], "--") != 0))
    {
        split += 1;
    }
    if ((split < 7) || (split >= count) || ((count - split) < 2))
    {
        printf("khw_write <part> <path.khw> <layout> <mnemonics> <folder> <machine writer> [<rounds>] -- <carrier "
               "word>...\n");
        return 2;
    }
    s_part = word[1];
    s_path = word[2];
    s_layout = word[3];
    s_mnemonics = word[4];
    s_folder = word[5];
    s_machine_writer = word[6];
    // [<rounds>] and, for a dry run tuned off the part, [--slot <n>]: the slot whose first field-finding round is
    // emitted for the vendor's disassembler, in place of the learn loop (khw_enumerate)
    unsigned int rounds = 1u;
    unsigned int enumerate = 0xffffffffu;
    for (int at = 7; at < split; at += 1)
    {
        if ((strcmp(word[at], "--slot") == 0) && ((at + 1) < split))
        {
            enumerate = (unsigned int)strtoul(word[at + 1], NULL, 10);
            at += 1;
        }
        else
        {
            rounds = (unsigned int)strtoul(word[at], NULL, 10);
        }
    }
    if (!khw_kernel_read())
    {
        return 1;
    }
    char forms_path[1024];
    snprintf(forms_path, sizeof(forms_path), "%s/forms.txt", s_folder);
    // the gate's file written before asking: the vendor's writer lays the kernel's places out, and the carrier reads
    // the questions of every round against them
    if (!khw_vendor_run("gate", forms_path))
    {
        return 1;
    }
    const char *carrier[8];
    unsigned int words = 0u;
    for (int at = split + 1; (at < count) && (words < ((sizeof(carrier) / sizeof(carrier[0])) - 1u)); at += 1)
    {
        carrier[words] = word[at];
        words += 1u;
    }
    carrier[words] = NULL;
    if (!run_channel_open(carrier, s_folder, 60000000ull))
    {
        return 1;
    }
    // R beside the machine file, its .kqr, so that no question the part has answered is put to it again
    char record[1024];
    const size_t stem = strlen(s_path) - (((strlen(s_path) > 4u) && (strcmp(s_path + strlen(s_path) - 4u, ".khw") == 0)) ? 4u : 0u);
    snprintf(record, sizeof(record), "%.*s.kqr", (int)stem, s_path);
    if (!run_channel_record(record, s_part, "khw_write"))
    {
        run_channel_close();
        return 1;
    }
    s_pool = (RunQuestion *)calloc(SCHEDULER_ROUND_MOST, sizeof(RunQuestion));
    if (s_pool == NULL)
    {
        printf("  khw_write: no room for a round's questions\n");
        run_channel_close();
        return 1;
    }
    // tuned off the part: the slot's first field-finding round emitted for the vendor's disassembler, the learn loop
    // left for a run that reaches the part
    if (enumerate != 0xffffffffu)
    {
        if (enumerate >= s_kernel_places)
        {
            printf("  khw_write: the slot %u is past the kernel's %u places\n", enumerate, s_kernel_places);
            run_channel_close();
            return 1;
        }
        khw_enumerate(enumerate);
        run_channel_close();
        printf("  enumerated the slot %u's first field-finding round off the part, %llu asks\n", enumerate, s_asks);
        return 0;
    }
    KhwAnswered kernel;
    memset(&kernel, 0, sizeof(kernel));
    unsigned long long nanoseconds = 0ull;
    unsigned int slot = 0u;
    const int related = khw_first_round(&kernel, &slot, &nanoseconds);
    printf("  the first round: the kernel's relation and %llu places asked for the slot, %llu asks\n",
           s_asks - 1ull, s_asks);
    if (!related)
    {
        printf("  khw_write: the kernel the system accepted answered no relation of the ladder (%s), and "
               "nothing was asked after it\n",
               s_pool[0].refused);
        run_channel_close();
        return 1;
    }
    printf("  the kernel: ladder.%s, its words read %s, %u candidates surviving, %llu ns a launch over %u launches\n",
           s_anchor_text[kernel.anchor], (kernel.swapped != 0) ? "swapped" : "in order", kernel.survivors,
           nanoseconds / KHW_LAUNCHES, KHW_LAUNCHES);
    s_slot = slot;
    if (s_slot == s_kernel_places)
    {
        printf("  khw_write: no place of the kernel answered as the slot, and nothing was asked of a form\n");
        run_channel_close();
        return 1;
    }
    printf("  the slot: place %u of %u, at 0x%x\n", s_slot, s_kernel_places, s_slot * KHW_INSTRUCTION);
    const unsigned long long slot_low = khw_word_read(&s_kernel_text[s_slot * KHW_INSTRUCTION], 0u);
    const unsigned long long slot_high = khw_word_read(&s_kernel_text[s_slot * KHW_INSTRUCTION], 8u);
    printf("  the forms the part answers for:\n");
    khw_form_asked(slot_low, slot_high);
    unsigned int from = 0u;
    for (unsigned int round = 0u; round < rounds; round += 1u)
    {
        const unsigned int until = s_names;
        const unsigned int found = khw_widened(from);
        from = until;
        printf("  round %u: %u forms found, %u in all, %llu asks\n", round + 1u, found, s_names, s_asks);
        if (found == 0u)
        {
            break;
        }
    }
    run_channel_close();
    // the forms the part answered written out, and the vendor's writer run once more to name them and lay out the file
    if (!khw_answers_written(forms_path) || !khw_vendor_run("final", forms_path))
    {
        return 1;
    }
    printf("  khw_write: %llu asks put\n", s_asks);
    return 0;
}
