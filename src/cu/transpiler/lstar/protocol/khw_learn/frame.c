// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.c: the frame round: the frame relation each place answers read by the gate (The frame forms asked, Open 6.4)
#include "khw_learn_internal.h"

// the register the part's .ksc names fixed `name`, `run answers <n> fixed <name>`, or KHW_FIXED_NONE where it names
// none
unsigned int khw_fixed_register(const char *name)
{
    static KscFile s_file;
    static KscRow s_row;
    char asked[64];
    snprintf(asked, sizeof(asked), "fixed %s", name);
    unsigned int found = KHW_FIXED_NONE;
    ksc_read(&s_file, s_ksc_path);
    for (unsigned int at = 0u; at < s_file.lines; at += 1u)
    {
        if (ksc_row_read(s_file.line[at], "run", &s_row) && (strcmp(s_row.answer, "answers") == 0) &&
            (strcmp(s_row.question, asked) == 0))
        {
            // a register's number is at most eight hex digits
            found = (unsigned int)s_row.value;
        }
    }
    ksc_free(&s_file);
    return found;
}

// the field `name` of the instruction word as the part's .ksc gives it, `field <name> <first> <bits>`: 1 with its
// first bit and its width, 0 where the .ksc gives none
int khw_field_read(const char *name, unsigned int *first, unsigned int *bits)
{
    static KscFile s_file;
    int found = 0;
    ksc_read(&s_file, s_ksc_path);
    for (unsigned int at = 0u; at < s_file.lines; at += 1u)
    {
        char read[64];
        unsigned int from = 0u;
        unsigned int width = 0u;
        if (ksc_field_read(s_file.line[at], read, sizeof(read), &from, &width) && (strcmp(read, name) == 0))
        {
            *first = from;
            *bits = width;
            found = 1;
        }
    }
    ksc_free(&s_file);
    return found;
}

int khw_field_holding(unsigned int first, unsigned int last, char *name, size_t room, unsigned int *field_first,
                      unsigned int *field_bits)
{
    static KscFile s_file;
    int found = 0;
    unsigned int narrowest = 0u;
    ksc_read(&s_file, s_ksc_path);
    for (unsigned int at = 0u; at < s_file.lines; at += 1u)
    {
        char read[64];
        unsigned int from = 0u;
        unsigned int width = 0u;
        if (ksc_field_read(s_file.line[at], read, sizeof(read), &from, &width) && (width != 0u) &&
            (from <= first) && (last < (from + width)) && (!found || (width < narrowest)))
        {
            snprintf(name, room, "%s", read);
            *field_first = from;
            *field_bits = width;
            narrowest = width;
            found = 1;
        }
    }
    ksc_free(&s_file);
    return found;
}

unsigned int khw_fields_listed(unsigned int *first, unsigned int *bits, unsigned int room)
{
    static KscFile s_file;
    unsigned int count = 0u;
    ksc_read(&s_file, s_ksc_path);
    for (unsigned int at = 0u; (at < s_file.lines) && (count < room); at += 1u)
    {
        char read[64];
        unsigned int from = 0u;
        unsigned int width = 0u;
        if (ksc_field_read(s_file.line[at], read, sizeof(read), &from, &width) && (width != 0u))
        {
            first[count] = from;
            bits[count] = width;
            count += 1u;
        }
    }
    ksc_free(&s_file);
    return count;
}

// 1 where the instruction `low` and `high` holds `value` in some run of its bits, `value` not 0: a number a form
// writes stands in the instruction, and a constant it computes does not
static int khw_encoding_holds(unsigned long long low, unsigned long long high, unsigned long long value)
{
    if ((value == 0ull) || (value > 0xffffffffull))
    {
        return 0;
    }
    unsigned int bits = 0u;
    for (unsigned long long rest = value; rest != 0ull; rest >>= 1u)
    {
        bits += 1u;
    }
    for (unsigned int width = bits; width <= 32u; width += 1u)
    {
        for (unsigned int first = 0u; (first + width) <= 128u; first += 1u)
        {
            if (khw_run_read(low, high, first, first + width - 1u) == value)
            {
                return 1;
            }
        }
    }
    return 0;
}

// the frame's cases: the union's cases taken round over FRAME_CASES threads, written once
unsigned int s_frame_word[FRAME_CASES][2];

static unsigned int s_frame_cases;

void khw_frame_cases_written(void)
{
    if (s_frame_cases != 0u)
    {
        return;
    }
    khw_union_written();
    for (unsigned int at = 0u; at < FRAME_CASES; at += 1u)
    {
        s_frame_word[at][0] = s_union_word[at % s_union_cases][0];
        s_frame_word[at][1] = s_union_word[at % s_union_cases][1];
    }
    s_frame_cases = FRAME_CASES;
}

// the frame's cases, written where they are not yet, and their count through `count`
const unsigned int (*khw_frame_cases(unsigned int *count))[2]
{
    khw_frame_cases_written();
    *count = s_frame_cases;
    return s_frame_word;
}

// `low` and `high` stalled the longest with the yield bit after the stall clear, the one scheduling the gate lets an
// instruction no form holds through and the vendor's reader takes; the stall field as the part's .ksc gives it, read
// once
void khw_frame_stalled(unsigned long long *low, unsigned long long *high)
{
    static int s_read = 0;
    static int s_given = 0;
    static unsigned int s_first = 0u;
    static unsigned int s_bits = 0u;
    if (s_read == 0)
    {
        s_given = khw_field_read("stall", &s_first, &s_bits);
        s_read = 1;
    }
    if (s_given == 0)
    {
        return;
    }
    khw_run_write(low, high, s_first, s_first + s_bits - 1u, (1ull << s_bits) - 1ull);
    khw_run_write(low, high, s_first + s_bits, s_first + s_bits, 0ull);
}

// The question of the frame added to the round being written: the kernel with `low` and `high` at the probe point,
// the changed place stalled the longest, named at the probe point as every question is, read by the cross and the
// gate as every question is, and a fault it causes kept as every fault is. Over `count` cases from `word`. Its place
// in the round, or SCHEDULER_ROUND_MOST where the round is full
unsigned int khw_frame_pooled(unsigned long long low, unsigned long long high, const unsigned int word[][2],
                              unsigned int count)
{
    unsigned long long stalled_low = low;
    unsigned long long stalled_high = high;
    khw_frame_stalled(&stalled_low, &stalled_high);
    return khw_pooled(s_slot, stalled_low, stalled_high, word, count);
}

// the thread of the frame's cases the gate reads as its case `read`: the first half of the gate's cases from the
// first block and the second half from the second, so that both blocks are read within the gate's room
static unsigned int khw_frame_thread(unsigned int read)
{
    const unsigned int half = GATE_CASES / 2u;
    return (read < half) ? read : (RUN_THREADS_A_BLOCK + (read - half));
}

// nothing's answer: what the record shows with the unreached encoding at the slot, over the frame's cases, each of
// the gate's cases read at its thread, known once the frame round carried that question. A candidate alike with it
// on every case is not read: a place that leaves the stored register alone answers as it does
static unsigned long long s_nothing[GATE_CASES];

static int s_nothing_answered[GATE_CASES];

static int s_nothing_known;

void khw_frame_nothing_read(const RunQuestion *question)
{
    for (unsigned int read = 0u; read < GATE_CASES; read += 1u)
    {
        const unsigned int thread = khw_frame_thread(read);
        s_nothing_answered[read] = (question->outcome == RUN_ANSWERED);
        s_nothing[read] = question->answered[thread];
    }
    s_nothing_known = 1;
}

// the frame relations' candidates: each relation once, and the load twice, of the case's first word and its second
#define KHW_FRAME_CANDIDATES (FRAME_RELATION_COUNT + 1u)

// The frame relation `question` answered, read by the gate as the ladder's is (khw_relation_of): each candidate's
// answer to each case the host's (frame_answer), a candidate failing a case it does not read alike, a candidate whose
// answers are nothing's on every case failing every one, and the first survivor kept in `held` with its reading and
// the count of survivors. The candidates in their order: lane, block, the load of the first word and of the second,
// the store of the second, exit, number where every case answers one constant, the first case's, and the
// instruction `held` holds that constant in its immediate field, dim where every case answers the block size and the
// immediate field does not hold it, and parameter where every case answers the count the launch hands the kernel,
// read at the count's place among the parameters. 1 where one survived
int khw_frame_of(const RunQuestion *question, KhwAnswered *held)
{
    static unsigned char s_fails[GATE_CANDIDATES][GATE_CASES];
    static const unsigned int s_relation[KHW_FRAME_CANDIDATES] = {
        FRAME_LANE, FRAME_BLOCK, FRAME_LOAD, FRAME_LOAD, FRAME_STORE, FRAME_EXIT, FRAME_NUMBER, FRAME_DIM,
        FRAME_PARAMETER};
    const unsigned long long reading[KHW_FRAME_CANDIDATES] = {
        0ull, 0ull, 0ull, 1ull, 1ull, 0ull, question->answered[0], 0ull, (unsigned long long)RUN_COUNT_PLACE};
    khw_frame_cases_written();
    // a number stands in the field the .ksc names `immediate`, where a number operand lies: a place holding its
    // constant there is a number, and one holding the block size there is a number and names no dim. A bit run
    // anywhere else reads one set bit over eight clear ones as the block size, which nearly every encoding holds, and
    // says nothing; where the .ksc names no immediate field, a number is read as any bit run of the instruction. A
    // constant of 0 is held by nothing: every field of a form the number is not in holds 0, and the zero register is
    // the part's 0
    unsigned int immediate_first = 0u;
    unsigned int immediate_bits = 0u;
    const int immediate = khw_field_read("immediate", &immediate_first, &immediate_bits);
    const unsigned long long in_immediate =
        immediate ? khw_run_read(held->low, held->high, immediate_first, immediate_first + immediate_bits - 1u) : 0ull;
    const int dim_in_immediate = immediate && (in_immediate == (unsigned long long)RUN_THREADS_A_BLOCK);
    const int number_held = (question->answered[0] != 0ull) &&
                            (immediate ? (in_immediate == question->answered[0])
                                       : khw_encoding_holds(held->low, held->high, question->answered[0]));
    for (unsigned int candidate = 0u; candidate < KHW_FRAME_CANDIDATES; candidate += 1u)
    {
        int as_nothing = s_nothing_known;
        // a number the instruction does not hold is a constant computed, and names no relation; the block size held
        // in the immediate field is a number, and names no dim
        const int unheld = ((s_relation[candidate] == FRAME_NUMBER) && !number_held) ||
                           ((s_relation[candidate] == FRAME_DIM) && dim_in_immediate);
        for (unsigned int read = 0u; read < GATE_CASES; read += 1u)
        {
            const unsigned int thread = khw_frame_thread(read);
            unsigned long long host = 0ull;
            frame_answer(s_relation[candidate], thread, s_frame_word[thread], reading[candidate], &host);
            const AnswerBracket bracket = {1, host, host};
            s_fails[candidate][read] =
                (answer_read(&bracket, question->outcome, question->answered[thread]) != ANSWER_ALIKE) ? 1u : 0u;
            as_nothing = as_nothing && s_nothing_answered[read] && (s_nothing[read] == host);
        }
        for (unsigned int read = 0u; (as_nothing || unheld) && (read < GATE_CASES); read += 1u)
        {
            s_fails[candidate][read] = 1u;
        }
    }
    unsigned char survives[GATE_CANDIDATES];
    held->anchor = (unsigned int)LADDER_ANCHOR_COUNT;
    held->frame = (unsigned int)FRAME_RELATION_COUNT;
    held->swapped = 0;
    held->signed_read = 0;
    held->signedness_asked = 0;
    held->survivors = gate_survivors(s_fails, KHW_FRAME_CANDIDATES, GATE_CASES, survives);
    for (unsigned int candidate = 0u; candidate < KHW_FRAME_CANDIDATES; candidate += 1u)
    {
        if (survives[candidate] != 0u)
        {
            held->frame = s_relation[candidate];
            held->reading = reading[candidate];
            return 1;
        }
    }
    return 0;
}
