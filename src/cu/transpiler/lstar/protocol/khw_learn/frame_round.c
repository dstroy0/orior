// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_round.c: the frame round: each place of the kernel put at the probe place in its variants over cases that reach
// a second block, and the places that answered retained as forms
#include "../../../../../../includes/platform/platform.h"
#include "khw_learn_internal.h"
#include "memoria_operor/memoria_operor.h"

// one variant of a place: the encoding and the place it came from
typedef struct
{
    unsigned long long low;
    unsigned long long high;
    unsigned int place;
} KhwVariant;

// the max immediates that the kept places hold, to put an opening place with, each one apart
#define KHW_FRAME_IMMEDIATES 8u

// the max variants that the call puts a place in: as it is, each operand run of the probe-place form, and the guard.
// Then each of those with the first word of the constant bank, and with each immediate that a kept place holds
#define KHW_VARIANTS_A_PLACE ((2u + KHW_FRAME_IMMEDIATES) * (KHW_RUNS + 2u))

static void khw_variant_added(KhwVariant *variants, unsigned int *count, unsigned long long low,
                              unsigned long long high, unsigned int place);

// what the call sets every variant of a place from. It has the fields that the .ksc of the part names: the guard, and
// the immediate that the operand of the constant bank lies in. Then the register of the record, and the immediates that
// the kept places hold, none of them 0
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
    unsigned long long known_immediate[KHW_FRAME_IMMEDIATES];
    unsigned int known_immediates;
} KhwFrameFields;

// the variants of the place `place`, which holds `low` and `high`, added to `variants`:
//
// - the place as it is
// - with the field that holds each operand run of the own form of the probe place, `probed_form`, set to the register
//   that the kernel stores. The field of the first word of the relation goes to the register of the record.
// - with its guard at every predicate, where it holds another one
//
// Where the place belongs to the opening (`opening` not 0), each of those comes again in two more forms. One has the
// field that the .ksc names `immediate` set to 0, the first word of the constant bank, where the block size of the
// launch is. Thus a place that reads the bank writes it to the register that the record reads. The other has that
// field set to each immediate that a kept place holds. Thus a place that reads the bank reads what a kept place reads
// there, with the count that the launch passes the kernel among it.
//
// The places of the opening are the ones that read the bank of the launch, and the kernel runs without them. A kept
// place with its immediate at 0 writes a register that a place after the probe place reads, a number or an address.
// The part then faults on the address
static void khw_place_varied(const KhwAnswered *probed_form, const KhwFrameFields *fields, unsigned long long low,
                             unsigned long long high, unsigned int place, int opening, KhwVariant *variants,
                             unsigned int *count)
{
    const unsigned int first = *count;
    khw_variant_added(variants, count, low, high, place);
    for (unsigned int run = 0u; run < probed_form->runs; run += 1u)
    {
        const int first_word = (probed_form->place[run] == 0u);
        unsigned int field_first = 0u;
        unsigned int field_bits = 0u;
        char field[64];
        if ((probed_form->place[run] == KHW_PLACE_NONE) || (first_word && (fields->record == KHW_FIXED_NONE)) ||
            !khw_field_covering(probed_form->first[run], probed_form->last[run], field, sizeof(field), &field_first,
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
        const unsigned long long is =
            khw_run_read(variants[at].low, variants[at].high, fields->immediate_first, immediate_last);
        // 0 first, then each immediate a retained place contains; a variant that contains one already is that variant
        for (unsigned int each = 0u; (is != 0ULL) && (each <= fields->known_immediates); each += 1u)
        {
            const unsigned long long set = (each == 0u) ? 0ULL : fields->known_immediate[each - 1u];
            if (set == is)
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

// the immediates that the kept places hold, read from the field that the .ksc names `immediate` into `fields`. Each is
// not 0, and apart from those before it. The call does not read the place where a question goes, or the unreached
// places. At max KHW_FRAME_IMMEDIATES
static void khw_known_immediates_read(KhwFrameFields *fields)
{
    fields->known_immediates = 0u;
    for (unsigned int place = 0u; fields->immediate && (place < s_kernel_places); place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        const unsigned long long value_read =
            khw_run_read(low, high, fields->immediate_first, fields->immediate_first + fields->immediate_bits - 1u);
        int known = (value_read == 0ULL) || (place == s_probe_place) ||
                    ((low == s_unreached_low) && (high == s_unreached_high));
        for (unsigned int at = 0u; at < fields->known_immediates; at += 1u)
        {
            known = known || (fields->known_immediate[at] == value_read);
        }
        if (!known && (fields->known_immediates < KHW_FRAME_IMMEDIATES))
        {
            fields->known_immediate[fields->known_immediates] = value_read;
            fields->known_immediates += 1u;
        }
    }
}

// `low` and `high` from `place`, added to `variants`, `*count` of them, where no variant of that place holds it
// already. Two places that are alike each keep their own, so that the first variant of each place is its own, as it
// is
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

// The frame round. Each kernel place, except the probe place and the unreached ones, goes at the probe place over the
// frame cases, in its variants. The call pools every one, and carries them as one round with nothing. Nothing is the
// unreached encoding at the probe place, which the call reads the candidates against. The places are the kept ones from
// the opening on, and the own places of the opening before them, numbered after the kept ones. The part ran those too,
// and every question drops them.
//
// The variants (khw_place_varied):
//
// - the place as it is
// - with the instruction-word field that holds each operand run of the classified probe-place form, `probed_form`. It
//   goes to the register that the kernel stores, KHW_SECOND_REGISTER. The .ksc of the part gives the fields. The field
//   of the first word of the relation goes to the register of the record instead. A load or a store reads that field as
//   an address, and the register of the record holds one. The call does not put it where the .ksc of the part names
//   none.
// - with its guard at PT, where the guard field, as the .ksc of the part gives it, holds another predicate
// - for the places of the opening alone, each of those with the field that the .ksc names `immediate` set to 0. That is
//   where the operand of the constant bank lies, and its first word is the block size of the launch. The call also sets
//   the field to each immediate that a kept place holds. Those are the reads of the bank by the kernel, with that of
//   the count among them.
//
// The call reads no further a variant that the working set already holds, and reads the other variants of the place.
// The first variant of a place to answer a frame relation stays as a form of the working set. Where `discover` is not
// 0, it is a discovered form. Else the call queries its fields. The return is the count kept
unsigned int khw_frame_round(const KhwAnswered *probed_form, int discover)
{
    static KhwVariant s_variants[2u * KHW_PLACES * KHW_VARIANTS_A_PLACE];
    static unsigned int s_pooled_at[2u * KHW_PLACES * KHW_VARIANTS_A_PLACE];
    unsigned int variants = 0u;
    KhwFrameFields fields;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &fields, .bytes = sizeof(fields), .val = 0);
    fields.guarded = khw_field_read("guard", &fields.guard_first, &fields.guard_bits);
    fields.every_predicate = fields.guarded ? ((1ULL << fields.guard_bits) - 1ULL) : 0ULL;
    fields.immediate = khw_field_read("immediate", &fields.immediate_first, &fields.immediate_bits);
    fields.record = khw_fixed_register("record");
    khw_known_immediates_read(&fields);
    khw_frame_cases_written();
    for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        if ((place == s_probe_place) || ((low == s_unreached_low) && (high == s_unreached_high)))
        {
            continue;
        }
        khw_place_varied(probed_form, &fields, low, high, place, 0, s_variants, &variants);
    }
    for (unsigned int place = 0u; place < s_opening_places; place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_opening_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_opening_text[place * KHW_INSTRUCTION], 8u);
        khw_place_varied(probed_form, &fields, low, high, s_kernel_places + place, 1, s_variants, &variants);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the frame round: ");
        mmgr_line_decimal(output_line, variants, 1u);
        mmgr_line_put(output_line, " variants of ");
        mmgr_line_decimal(output_line, s_kernel_places, 1u);
        mmgr_line_put(output_line, " retained places and ");
        mmgr_line_decimal(output_line, s_opening_places, 1u);
        mmgr_line_put(output_line, " opening ones, the probe place at ");
        mmgr_line_decimal(output_line, s_probe_place, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    s_pooled = 0u;
    const unsigned int nothing_at = khw_frame_pooled(s_unreached_low, s_unreached_high, s_frame_word, FRAME_CASES);
    for (unsigned int at = 0u; at < variants; at += 1u)
    {
        s_pooled_at[at] = khw_frame_pooled(s_variants[at].low, s_variants[at].high, s_frame_word, FRAME_CASES);
    }
    khw_round_carried();
    if (nothing_at < SCHEDULER_ROUND_MAX)
    {
        khw_frame_nothing_read(&s_pool[nothing_at]);
    }
    unsigned int retained = 0u;
    unsigned int retained_place = s_kernel_places + s_opening_places;
    for (unsigned int at = 0u; (at < variants) && (s_names < KHW_FORMS); at += 1u)
    {
        if ((s_variants[at].place == retained_place) || (s_pooled_at[at] >= SCHEDULER_ROUND_MAX))
        {
            continue;
        }
        // the call reads no further a variant that the working set already holds. It reads the other variants of the
        // place, each an encoding of its own. The first of them to answer a relation is the form of the place
        if (khw_named_already(s_variants[at].low, s_variants[at].high))
        {
            continue;
        }
        KhwAnswered form;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &form, .bytes = sizeof(form), .val = 0);
        form.low = s_variants[at].low;
        form.high = s_variants[at].high;
        // a variant the set's form answers for already, a turn of it, is that form, and enters nowhere
        if (!khw_frame_of(&s_pool[s_pooled_at[at]], &form) || khw_form_was_for(s_named, s_names, &form))
        {
            continue;
        }
        form.phase = KHW_PHASE_DISCOVERED;
        if (discover == 0)
        {
            khw_fields_queried(&form);
        }
        s_named[s_names] = form;
        s_names += 1u;
        retained += 1u;
        retained_place = s_variants[at].place;
        char relation[64];
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    ");
            mmgr_line_put_columns(output_line, khw_relation_named(&form, 1, relation, sizeof(relation)), 14u);
            mmgr_line_put(output_line, " ");
            mmgr_line_hex(output_line, form.low, 16u);
            mmgr_line_put(output_line, " ");
            mmgr_line_hex(output_line, form.high, 16u);
            mmgr_line_put(output_line, "  place ");
            mmgr_line_decimal(output_line, s_variants[at].place, 1u);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, form.survivors, 1u);
            mmgr_line_put(output_line, " surviving");
            mmgr_line_put(output_line, (discover != 0) ? ", its turns await a cross-check" : "");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    s_pooled = 0u;
    return retained;
}
