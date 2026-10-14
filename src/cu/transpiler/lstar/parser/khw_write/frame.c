// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.c: the frame round: each place of the kernel put at the slot in its variants over cases that reach a second
// block, the frame relation each answers read by the gate, and the places that answered kept as forms (The frame
// forms asked, Open 6.4)
#include "khw_write_internal.h"

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
        if ((sscanf(s_file.line[at], "field %63s %u %u", read, &from, &width) == 3) && (strcmp(read, name) == 0))
        {
            *first = from;
            *bits = width;
            found = 1;
        }
    }
    ksc_free(&s_file);
    return found;
}

// the frame's cases: the union's cases taken round over FRAME_CASES threads, written once
static unsigned int s_frame_word[FRAME_CASES][2];

static unsigned int s_frame_cases;

static void khw_frame_cases_written(void)
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

// `low` and `high` stalled the longest with the yield bit after the stall clear, the one scheduling the gate lets an
// instruction no form holds through and the vendor's reader takes; the stall field as the part's .ksc gives it, read
// once
static void khw_frame_stalled(unsigned long long *low, unsigned long long *high)
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

// The question of the frame added to the round being written: the kernel with `low` and `high` at the slot, put whole,
// naming no slot, the changed place stalled the longest. A question that names the slot is held by the cross for a
// memory operand there, and a load or a store is what the frame asks. Over `count` cases from `word`. Its place in
// the round, or SCHEDULER_ROUND_MOST where the round is full
unsigned int khw_frame_pooled(unsigned long long low, unsigned long long high, const unsigned int word[][2],
                              unsigned int count)
{
    unsigned long long stalled_low = low;
    unsigned long long stalled_high = high;
    khw_frame_stalled(&stalled_low, &stalled_high);
    const unsigned int at = khw_pooled(s_slot, stalled_low, stalled_high, word, count);
    if (at < SCHEDULER_ROUND_MOST)
    {
        s_pool[at].slot = RUN_SLOT_NONE;
    }
    return at;
}

// the thread of the frame's cases the gate reads as its case `read`: the first half of the gate's cases from the
// first block and the second half from the second, so that both blocks are read within the gate's room
static unsigned int khw_frame_thread(unsigned int read)
{
    const unsigned int half = GATE_CASES / 2u;
    return (read < half) ? read : (RUN_THREADS_A_BLOCK + (read - half));
}

// The frame relation `question` answered, read by the gate as the ladder's is (khw_relation_of): each candidate's
// answer to each case the host's (frame_answer), a candidate failing a case it does not read alike, and the first
// survivor kept in `held` with its reading and the count of survivors. The candidates in their order: lane, block,
// the load of the first word and of the second, the store of the second, exit, and number where every case answers
// one constant, the first case's. 1 where one survived
int khw_frame_of(const RunQuestion *question, KhwAnswered *held)
{
    static unsigned char s_fails[GATE_CANDIDATES][GATE_CASES];
    static const unsigned int s_relation[7] = {FRAME_LANE,  FRAME_BLOCK, FRAME_LOAD,  FRAME_LOAD,
                                               FRAME_STORE, FRAME_EXIT,  FRAME_NUMBER};
    const unsigned long long reading[7] = {0ull, 0ull, 0ull, 1ull, 1ull, 0ull, question->answered[0]};
    khw_frame_cases_written();
    for (unsigned int candidate = 0u; candidate < 7u; candidate += 1u)
    {
        for (unsigned int read = 0u; read < GATE_CASES; read += 1u)
        {
            const unsigned int thread = khw_frame_thread(read);
            unsigned long long host = 0ull;
            frame_answer(s_relation[candidate], thread, s_frame_word[thread], reading[candidate], &host);
            const AnswerBracket bracket = {1, host, host};
            s_fails[candidate][read] =
                (answer_read(&bracket, question->outcome, question->answered[thread]) != ANSWER_ALIKE) ? 1u : 0u;
        }
    }
    unsigned char survives[GATE_CANDIDATES];
    held->anchor = (unsigned int)LADDER_ANCHOR_COUNT;
    held->frame = (unsigned int)FRAME_RELATION_COUNT;
    held->swapped = 0;
    held->signed_read = 0;
    held->signedness_asked = 0;
    held->survivors = gate_survivors(s_fails, 7u, GATE_CASES, survives);
    for (unsigned int candidate = 0u; candidate < 7u; candidate += 1u)
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

// one variant of a place: the encoding and the place it came from
typedef struct
{
    unsigned long long low;
    unsigned long long high;
    unsigned int place;
} KhwVariant;

// the most variants a place is put in: as it stands, each operand run of the slot's form, and the guard
#define KHW_VARIANTS_A_PLACE (KHW_RUNS + 2u)

// `low` and `high` from `place` added to `variants`, `*count` of them, where no variant holds it already
static void khw_variant_added(KhwVariant *variants, unsigned int *count, unsigned long long low,
                              unsigned long long high, unsigned int place)
{
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if ((variants[at].low == low) && (variants[at].high == high))
        {
            return;
        }
    }
    variants[*count].low = low;
    variants[*count].high = high;
    variants[*count].place = place;
    *count += 1u;
}

// The frame round: each place of the kernel from the opening on, the slot and the unreached ones apart, put at the slot
// over the frame's cases in its variants, every one pooled and carried as one round: as it stands; with each operand
// run of the slot's own classified form `slot_form` set to the register the kernel stores, KHW_SECOND_REGISTER; and
// with its guard at PT where the guard field, as the part's .ksc gives it, holds another predicate. The first variant
// of a place to answer a frame relation is kept as a form of the working set, discovered where `discover` is not 0 and
// its fields asked where it is. The count kept
unsigned int khw_frame_round(const KhwAnswered *slot_form, int discover)
{
    static KhwVariant s_variants[KHW_PLACES * KHW_VARIANTS_A_PLACE];
    static unsigned int s_pooled_at[KHW_PLACES * KHW_VARIANTS_A_PLACE];
    unsigned int variants = 0u;
    unsigned int guard_first = 0u;
    unsigned int guard_bits = 0u;
    const int guarded = khw_field_read("guard", &guard_first, &guard_bits);
    const unsigned long long every_predicate = guarded ? ((1ull << guard_bits) - 1ull) : 0ull;
    khw_frame_cases_written();
    for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        if ((place == s_slot) || ((low == s_unreached_low) && (high == s_unreached_high)))
        {
            continue;
        }
        khw_variant_added(s_variants, &variants, low, high, place);
        for (unsigned int run = 0u; run < slot_form->runs; run += 1u)
        {
            if (slot_form->place[run] == KHW_PLACE_NONE)
            {
                continue;
            }
            unsigned long long set_low = low;
            unsigned long long set_high = high;
            khw_run_write(&set_low, &set_high, slot_form->first[run], slot_form->last[run], KHW_SECOND_REGISTER);
            khw_variant_added(s_variants, &variants, set_low, set_high, place);
        }
        if (guarded && (khw_run_read(low, high, guard_first, guard_first + guard_bits - 1u) != every_predicate))
        {
            unsigned long long set_low = low;
            unsigned long long set_high = high;
            khw_run_write(&set_low, &set_high, guard_first, guard_first + guard_bits - 1u, every_predicate);
            khw_variant_added(s_variants, &variants, set_low, set_high, place);
        }
    }
    s_pooled = 0u;
    for (unsigned int at = 0u; at < variants; at += 1u)
    {
        s_pooled_at[at] = khw_frame_pooled(s_variants[at].low, s_variants[at].high, s_frame_word, FRAME_CASES);
    }
    khw_round_carried();
    unsigned int kept = 0u;
    unsigned int kept_place = s_kernel_places;
    for (unsigned int at = 0u; (at < variants) && (s_names < KHW_FORMS); at += 1u)
    {
        if ((s_variants[at].place == kept_place) || (s_pooled_at[at] >= SCHEDULER_ROUND_MOST))
        {
            continue;
        }
        KhwAnswered held;
        memset(&held, 0, sizeof(held));
        held.low = s_variants[at].low;
        held.high = s_variants[at].high;
        if (!khw_frame_of(&s_pool[s_pooled_at[at]], &held))
        {
            continue;
        }
        held.phase = KHW_PHASE_DISCOVERED;
        if (discover == 0)
        {
            khw_fields_asked(&held);
        }
        s_named[s_names] = held;
        s_names += 1u;
        kept += 1u;
        kept_place = s_variants[at].place;
        char relation[64];
        printf("    %-14s %016llx %016llx  place %u, %u surviving%s\n",
               khw_relation_named(&held, 1, relation, sizeof(relation)), held.low, held.high, s_variants[at].place,
               held.survivors, (discover != 0) ? ", its turns await a cross-check" : "");
    }
    s_pooled = 0u;
    return kept;
}
