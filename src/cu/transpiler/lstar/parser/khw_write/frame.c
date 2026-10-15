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
        if ((sscanf(s_file.line[at], "field %63s %u %u", read, &from, &width) == 3) && (width != 0u) &&
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

// nothing's answer: what the record shows with the unreached encoding at the slot, over the frame's cases, each of
// the gate's cases read at its thread, known once the frame round carried that question. A candidate alike with it
// on every case is not read: a place that leaves the stored register alone answers as it does
static unsigned long long s_nothing[GATE_CASES];

static int s_nothing_answered[GATE_CASES];

static int s_nothing_known;

static void khw_frame_nothing_read(const RunQuestion *question)
{
    for (unsigned int read = 0u; read < GATE_CASES; read += 1u)
    {
        const unsigned int thread = khw_frame_thread(read);
        s_nothing_answered[read] = (question->outcome == RUN_ANSWERED);
        s_nothing[read] = question->answered[thread];
    }
    s_nothing_known = 1;
}

// The frame relation `question` answered, read by the gate as the ladder's is (khw_relation_of): each candidate's
// answer to each case the host's (frame_answer), a candidate failing a case it does not read alike, a candidate whose
// answers are nothing's on every case failing every one, and the first survivor kept in `held` with its reading and
// the count of survivors. The candidates in their order: lane, block, the load of the first word and of the second,
// the store of the second, exit, number where every case answers one constant, the first case's, and the
// instruction `held` holds that constant in its immediate field, and dim where every case answers the block size
// and the immediate field does not hold it. 1 where one survived
int khw_frame_of(const RunQuestion *question, KhwAnswered *held)
{
    static unsigned char s_fails[GATE_CANDIDATES][GATE_CASES];
    static const unsigned int s_relation[8] = {FRAME_LANE,  FRAME_BLOCK, FRAME_LOAD,   FRAME_LOAD,
                                               FRAME_STORE, FRAME_EXIT,  FRAME_NUMBER, FRAME_DIM};
    const unsigned long long reading[8] = {0ull, 0ull, 0ull, 1ull, 1ull, 0ull, question->answered[0], 0ull};
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
    for (unsigned int candidate = 0u; candidate < 8u; candidate += 1u)
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
    held->survivors = gate_survivors(s_fails, 8u, GATE_CASES, survives);
    for (unsigned int candidate = 0u; candidate < 8u; candidate += 1u)
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

// the most variants a place is put in: as it stands, each operand run of the slot's form and the guard, and each
// of those with the constant bank's first word
#define KHW_VARIANTS_A_PLACE (2u * (KHW_RUNS + 2u))

static void khw_variant_added(KhwVariant *variants, unsigned int *count, unsigned long long low,
                              unsigned long long high, unsigned int place);

// what every variant of a place is set from: the fields the part's .ksc names, the guard and the immediate, which
// the constant bank's operand lies in, and the record's register
typedef struct
{
    int guarded;
    unsigned int guard_first;
    unsigned int guard_bits;
    unsigned long long every_predicate;
    int immediate;
    unsigned int immediate_first;
    unsigned int immediate_bits;
    unsigned int record;
} KhwFrameFields;

// the variants of the place `place` holding `low` and `high` added to `variants`: as it stands; with the field
// holding each operand run of the slot's own form `slot_form` set to the register the kernel stores, the relation's
// first word's to the record's register; with its guard at every predicate where it holds another; and, where the
// place is the opening's, `opening` not 0, each of those again with the field the .ksc names `immediate` set to 0,
// the constant bank's first word, which the launch's block size stands at, so that a place reading the bank writes
// it to the register the record reads. The opening's places are the ones that read the launch's bank, and the
// kernel runs without them; a kept place's immediate set to 0 writes a register a place after the slot reads, a
// number or an address, and the part faults on the address
static void khw_place_varied(const KhwAnswered *slot_form, const KhwFrameFields *fields, unsigned long long low,
                             unsigned long long high, unsigned int place, int opening, KhwVariant *variants,
                             unsigned int *count)
{
    const unsigned int first = *count;
    khw_variant_added(variants, count, low, high, place);
    for (unsigned int run = 0u; run < slot_form->runs; run += 1u)
    {
        const int first_word = (slot_form->place[run] == 0u);
        unsigned int field_first = 0u;
        unsigned int field_bits = 0u;
        char field[64];
        if ((slot_form->place[run] == KHW_PLACE_NONE) || (first_word && (fields->record == KHW_FIXED_NONE)) ||
            !khw_field_holding(slot_form->first[run], slot_form->last[run], field, sizeof(field), &field_first,
                               &field_bits))
        {
            continue;
        }
        unsigned long long set_low = low;
        unsigned long long set_high = high;
        khw_run_write(&set_low, &set_high, field_first, field_first + field_bits - 1u,
                      first_word ? fields->record : KHW_SECOND_REGISTER);
        khw_variant_added(variants, count, set_low, set_high, place);
    }
    if (fields->guarded && (khw_run_read(low, high, fields->guard_first,
                                         fields->guard_first + fields->guard_bits - 1u) != fields->every_predicate))
    {
        unsigned long long set_low = low;
        unsigned long long set_high = high;
        khw_run_write(&set_low, &set_high, fields->guard_first, fields->guard_first + fields->guard_bits - 1u,
                      fields->every_predicate);
        khw_variant_added(variants, count, set_low, set_high, place);
    }
    const unsigned int last = *count;
    for (unsigned int at = first; (opening != 0) && fields->immediate && (at < last); at += 1u)
    {
        unsigned long long set_low = variants[at].low;
        unsigned long long set_high = variants[at].high;
        if (khw_run_read(set_low, set_high, fields->immediate_first,
                         fields->immediate_first + fields->immediate_bits - 1u) == 0ull)
        {
            continue;
        }
        khw_run_write(&set_low, &set_high, fields->immediate_first,
                      fields->immediate_first + fields->immediate_bits - 1u, 0ull);
        khw_variant_added(variants, count, set_low, set_high, place);
    }
}

// `low` and `high` from `place` added to `variants`, `*count` of them, where no variant of that place holds it
// already; two places that stand alike each keep their own, so that each place's first variant is its own as it stands
static void khw_variant_added(KhwVariant *variants, unsigned int *count, unsigned long long low,
                              unsigned long long high, unsigned int place)
{
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if ((variants[at].place == place) && (variants[at].low == low) && (variants[at].high == high))
        {
            return;
        }
    }
    variants[*count].low = low;
    variants[*count].high = high;
    variants[*count].place = place;
    *count += 1u;
}

// The frame round: each place of the kernel, the slot and the unreached ones apart, put at the slot over the frame's
// cases in its variants, every one pooled and carried as one round with nothing, the unreached encoding at the slot,
// which the candidates are read against. The places are the kept ones from the opening on and the opening's own
// before it, which the part ran as well and which every question drops, numbered after the kept ones. The variants
// (khw_place_varied): as it stands; with the field of the instruction word that holds each operand run of the
// slot's own classified form `slot_form`, as the part's .ksc gives the fields, set to the register the kernel
// stores, KHW_SECOND_REGISTER, the field of the relation's first word to the record's register instead, since a load
// or a store reads that field as an address and the record's register holds one, and not put where the part's .ksc
// names none; with its guard at PT where the guard field, as the part's .ksc gives it, holds another predicate; and
// the opening's places alone each of those with the field the .ksc names `immediate` set to 0, where the constant
// bank's operand lies and its first word is the launch's block size. A variant the working set holds already is read
// no further, and the place's other variants are. The first variant of a place to answer a frame relation is kept as
// a form of the working set, discovered where `discover` is not 0 and its fields asked where it is. The count kept
unsigned int khw_frame_round(const KhwAnswered *slot_form, int discover)
{
    static KhwVariant s_variants[2u * KHW_PLACES * KHW_VARIANTS_A_PLACE];
    static unsigned int s_pooled_at[2u * KHW_PLACES * KHW_VARIANTS_A_PLACE];
    unsigned int variants = 0u;
    KhwFrameFields fields;
    memset(&fields, 0, sizeof(fields));
    fields.guarded = khw_field_read("guard", &fields.guard_first, &fields.guard_bits);
    fields.every_predicate = fields.guarded ? ((1ull << fields.guard_bits) - 1ull) : 0ull;
    fields.immediate = khw_field_read("immediate", &fields.immediate_first, &fields.immediate_bits);
    fields.record = khw_fixed_register("record");
    khw_frame_cases_written();
    for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        if ((place == s_slot) || ((low == s_unreached_low) && (high == s_unreached_high)))
        {
            continue;
        }
        khw_place_varied(slot_form, &fields, low, high, place, 0, s_variants, &variants);
    }
    for (unsigned int place = 0u; place < s_opening_places; place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_opening_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_opening_text[place * KHW_INSTRUCTION], 8u);
        khw_place_varied(slot_form, &fields, low, high, s_kernel_places + place, 1, s_variants, &variants);
    }
    printf("  the frame round: %u variants of %u kept places and %u opening ones, the slot at %u\n", variants,
           s_kernel_places, s_opening_places, s_slot);
    s_pooled = 0u;
    const unsigned int nothing_at = khw_frame_pooled(s_unreached_low, s_unreached_high, s_frame_word, FRAME_CASES);
    for (unsigned int at = 0u; at < variants; at += 1u)
    {
        s_pooled_at[at] = khw_frame_pooled(s_variants[at].low, s_variants[at].high, s_frame_word, FRAME_CASES);
    }
    khw_round_carried();
    if (nothing_at < SCHEDULER_ROUND_MOST)
    {
        khw_frame_nothing_read(&s_pool[nothing_at]);
    }
    unsigned int kept = 0u;
    unsigned int kept_place = s_kernel_places + s_opening_places;
    for (unsigned int at = 0u; (at < variants) && (s_names < KHW_FORMS); at += 1u)
    {
        if ((s_variants[at].place == kept_place) || (s_pooled_at[at] >= SCHEDULER_ROUND_MOST))
        {
            continue;
        }
        // a variant the working set holds already is read no further; the place's other variants are, each an
        // encoding of its own, and the first of them to answer a relation is the place's form
        if (khw_named_already(s_variants[at].low, s_variants[at].high))
        {
            continue;
        }
        KhwAnswered held;
        memset(&held, 0, sizeof(held));
        held.low = s_variants[at].low;
        held.high = s_variants[at].high;
        // a variant the set's form answers for already, a turn of it, is that form, and enters nowhere
        if (!khw_frame_of(&s_pool[s_pooled_at[at]], &held) || khw_form_stood_for(s_named, s_names, &held))
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
