// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_round.c: the frame round: each place of the kernel put at the slot in its variants over cases that reach a
// second block, and the places that answered kept as forms
#include "khw_learn_internal.h"

// one variant of a place: the encoding and the place it came from
typedef struct
{
    unsigned long long low;
    unsigned long long high;
    unsigned int place;
} KhwVariant;

// the most immediates the kept places hold that an opening place is put with, each one apart
#define KHW_FRAME_IMMEDIATES 8u

// the most variants a place is put in: as it stands, each operand run of the slot's form and the guard, and each
// of those with the constant bank's first word and with each immediate a kept place holds
#define KHW_VARIANTS_A_PLACE ((2u + KHW_FRAME_IMMEDIATES) * (KHW_RUNS + 2u))

static void khw_variant_added(KhwVariant *variants, unsigned int *count, unsigned long long low,
                              unsigned long long high, unsigned int place);

// what every variant of a place is set from: the fields the part's .ksc names, the guard and the immediate, which
// the constant bank's operand lies in, the record's register, and the immediates the kept places hold, none of them 0
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
    unsigned long long held_immediate[KHW_FRAME_IMMEDIATES];
    unsigned int held_immediates;
} KhwFrameFields;

// the variants of the place `place` holding `low` and `high` added to `variants`: as it stands; with the field
// holding each operand run of the slot's own form `slot_form` set to the register the kernel stores, the relation's
// first word's to the record's register; with its guard at every predicate where it holds another; and, where the
// place is the opening's, `opening` not 0, each of those again with the field the .ksc names `immediate` set to 0,
// the constant bank's first word, which the launch's block size stands at, so that a place reading the bank writes
// it to the register the record reads, and set to each immediate a kept place holds, so that a place reading the
// bank reads what a kept place reads there, the count the launch hands the kernel among it. The opening's places are
// the ones that read the launch's bank, and the kernel runs without them; a kept place's immediate set to 0 writes a
// register a place after the slot reads, a number or an address, and the part faults on the address
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
    const unsigned int immediate_last = fields->immediate_first + fields->immediate_bits - 1u;
    for (unsigned int at = first; (opening != 0) && fields->immediate && (at < last); at += 1u)
    {
        const unsigned long long stands = khw_run_read(variants[at].low, variants[at].high, fields->immediate_first,
                                                       immediate_last);
        // 0 first, then each immediate a kept place holds; a variant that holds one already is that variant
        for (unsigned int each = 0u; (stands != 0ull) && (each <= fields->held_immediates); each += 1u)
        {
            const unsigned long long set = (each == 0u) ? 0ull : fields->held_immediate[each - 1u];
            if (set == stands)
            {
                continue;
            }
            unsigned long long set_low = variants[at].low;
            unsigned long long set_high = variants[at].high;
            khw_run_write(&set_low, &set_high, fields->immediate_first, immediate_last, set);
            khw_variant_added(variants, count, set_low, set_high, place);
        }
    }
}

// the immediates the kept places hold, read off the field the .ksc names `immediate` into `fields`: each not 0 and
// apart from those before it, the place a question is put at and the unreached places not read, at most
// KHW_FRAME_IMMEDIATES
static void khw_held_immediates_read(KhwFrameFields *fields)
{
    fields->held_immediates = 0u;
    for (unsigned int place = 0u; fields->immediate && (place < s_kernel_places); place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        const unsigned long long held = khw_run_read(low, high, fields->immediate_first,
                                                     fields->immediate_first + fields->immediate_bits - 1u);
        int known = (held == 0ull) || (place == s_slot) || ((low == s_unreached_low) && (high == s_unreached_high));
        for (unsigned int at = 0u; at < fields->held_immediates; at += 1u)
        {
            known = known || (fields->held_immediate[at] == held);
        }
        if (!known && (fields->held_immediates < KHW_FRAME_IMMEDIATES))
        {
            fields->held_immediate[fields->held_immediates] = held;
            fields->held_immediates += 1u;
        }
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
// bank's operand lies and its first word is the launch's block size, and set to each immediate a kept place holds,
// the kernel's own reads of the bank, the count's among them. A variant the working set holds already is read
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
    khw_held_immediates_read(&fields);
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
