// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// asks.c: the asks: a question's words and runs read and written, the kernel and the code put, a question asked and pooled, the round carried, the anchor cases and a candidate's answer
#include "khw_write_internal.h"

// how many registers a thread of the kernel holds, read with the kernel
unsigned int s_registers;

// the kernel, the question's code written from it, how many places it holds, the place the slot stands at, and the
// encoding its unreached places hold
unsigned char s_kernel_text[KHW_PLACES * KHW_INSTRUCTION];

unsigned char s_code[KHW_PLACES * KHW_INSTRUCTION];
unsigned int s_kernel_places;
unsigned int s_slot;
unsigned long long s_unreached_low;
unsigned long long s_unreached_high;

// the path the file is written to, the part named in it, the container layout and the vendor's table the vendor's
// writer is handed, and the vendor's writer itself
const char *s_path;

const char *s_part;
const char *s_layout;
const char *s_mnemonics;

// the instruction word's layout, the fields the part's .ksc is given (sass_nvidia.tsv), and the .ksc beside the
// machine file, where the part's answers to the walks are kept
const char *s_fields;

char s_ksc_path[1024];
const char *s_machine_writer;

// one question, how many the run has put, and the launches the question is timed over, 0 where it is untimed
RunQuestion s_question;

unsigned long long s_asks;
unsigned int s_launches;

// One round's questions, SCHEDULER_ROUND_MOST at most, each with the code it carries, how many it settles and its
// place in the round; how many the round holds
RunQuestion *s_pool;

static unsigned char s_pool_code[SCHEDULER_ROUND_MOST][KHW_PLACES * KHW_INSTRUCTION];
static RunQuestion *s_pool_question[SCHEDULER_ROUND_MOST];
static unsigned int s_pool_settles[SCHEDULER_ROUND_MOST];
unsigned int s_pooled;

// one word of an instruction, and one written back
unsigned long long khw_word_read(const unsigned char *instruction, unsigned int byte)
{
    unsigned long long value = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        value |= (unsigned long long)instruction[byte + at] << (8u * at);
    }
    return value;
}

void khw_word_write(unsigned char *instruction, unsigned int byte, unsigned long long value)
{
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        instruction[byte + at] = (unsigned char)((value >> (8u * at)) & 0xffull);
    }
}

// the bits `first` to `last` of the instruction `low` and `high` make, and the pair with that run set to `value`
unsigned long long khw_run_read(unsigned long long low, unsigned long long high, unsigned int first,
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

void khw_run_write(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int last,
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
void khw_kernel_put(unsigned char *code)
{
    memcpy(code, s_kernel_text, (size_t)s_kernel_places * KHW_INSTRUCTION);
}

// The question's code, into `code`: the kernel with `low` and `high` at `place`. The slot the question names is sent
// to the carrier, which there makes the slot wait on all six barriers, stall the longest and hold no barrier; a
// form the part has not named then waits on everything, and a barrier nothing releases leaves the wait after it standing
// (P9, cubin_safe.h)
void khw_code_put(unsigned char *code, unsigned int place, unsigned long long low, unsigned long long high)
{
    khw_kernel_put(code);
    khw_word_write(&code[place * KHW_INSTRUCTION], 0u, low);
    khw_word_write(&code[place * KHW_INSTRUCTION], 8u, high);
}

// The question in s_code put over `count` cases, each case's two words from `word`, the carrier making the slot at

// `slot` conservative (RUN_SLOT_NONE where the code holds no slot), and each answer into `answered`. 1 where the part
// answered every case, 0 where it refused the question, the gate held it off the part or nothing carried it
int khw_asked(const unsigned int word[][2], unsigned int count, unsigned int slot, unsigned long long *answered)
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
    unsigned long long before[4] = {0ull, 0ull, 0ull, 0ull};
    run_channel_clocks(&before[0], &before[1], &before[2], &before[3]);
    const int asked = run_channel_ask(&s_question);
    khw_round_printed(1u, before);
    if (!asked)
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
unsigned int khw_pooled(unsigned int place, unsigned long long low, unsigned long long high,
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

// a round's line printed: how many questions it put, and the seconds its airlock and its carrier took, read off the
// channel's clocks against the readings `airlock_before` and `carried_before` taken before it
void khw_round_printed(unsigned int count, const unsigned long long before[4])
{
    unsigned long long now[4] = {0ull, 0ull, 0ull, 0ull};
    run_channel_clocks(&now[0], &now[1], &now[2], &now[3]);
    printf("  round: %u question(s), airlock %.2f s (pop %.2f s, cross %.2f s), carrier %.2f s\n", count,
           (double)(now[0] - before[0]) / 1e9, (double)(now[1] - before[1]) / 1e9, (double)(now[2] - before[2]) / 1e9,
           (double)(now[3] - before[3]) / 1e9);
    // the line reaches the console as the round ends, not when a pipe's block fills
    fflush(stdout);
}

// The plan of a pass handed to the run as the pass starts, the blob `plan.txt`, one line `<pass> <questions>`: the
// count of questions the pass is to put, an upper bound a reader of the run's tap takes the pass's progress against
void khw_plan_handed(const char *pass, unsigned long long questions)
{
    char line[128];
    const int length = snprintf(line, sizeof(line), "%s %llu\n", pass, questions);
    if ((length > 0) && (qry_run() != NULL))
    {
        // the line fits its room, and its length is positive
        qry_hand(qry_run(), "plan.txt", line, (unsigned long long)length, QRY_ORDINARY);
    }
    // the plan on the console as well, for a reader of the console that has no hand on the run
    printf("  plan: %s %llu\n", pass, questions);
}

// The round written carried as one (scheduler.h), its questions read after from s_pool, and the next round written
// in its place once they are
void khw_round_carried(void)
{
    // a stopped run carries no round: said once, and every round after is held by the channel with that reason
    static int s_stop_said = 0;
    if (run_channel_panic_ruled() && !s_stop_said)
    {
        s_stop_said = 1;
        printf("  the run stopped on a panic: no round after it reaches the part, and the pass ends with what the "
               "rounds before it left\n");
    }
    unsigned long long before[4] = {0ull, 0ull, 0ull, 0ull};
    run_channel_clocks(&before[0], &before[1], &before[2], &before[3]);
    scheduler_round(s_pool_question, s_pool_settles, s_pooled);
    s_asks += s_pooled;
    khw_round_printed(s_pooled, before);
}

// the cases of `anchor` the host computes both readings of, their words into `word` in the order `swapped` says, and
// each reading into `plain` and `signed_read`. The count found
unsigned int khw_anchor_cases(unsigned int anchor, int swapped, unsigned int word[][2], unsigned int *plain,
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
        signed_read[found] = ladder_signed_answer(anchor, held->word);
        found += 1u;
    }
    return found;
}

// the host's answer of `candidate` to the case of words `word`
unsigned int khw_candidate_answer(const KhwCandidate *candidate, const unsigned int *word)
{
    const unsigned int read[2] = {(candidate->swapped != 0) ? word[1] : word[0],
                                  (candidate->swapped != 0) ? word[0] : word[1]};
    unsigned int answered = 0u;
    if (candidate->signed_read != 0)
    {
        return ladder_signed_answer(candidate->anchor, read);
    }
    return ladder_answer(candidate->anchor, read, 2u, &answered) ? answered : 0u;
}

// The cases a relation is read over, written once: every two-word case of every relation of the ladder that is no
// measure, in both orders, each once; and the candidates, each such relation, its words read in order or swapped, and
// read signed beside plain where a case of it reads two ways
unsigned int s_union_word[GATE_CASES][2];

unsigned int s_union_cases;
KhwCandidate s_candidate[GATE_CANDIDATES];
unsigned int s_candidates;
