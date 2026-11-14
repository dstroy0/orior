// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// located.c: one form located for the encodings that answer as it does: the working set contains one form for each
// answer, and a turn of a form's bits outside its operands is that form
#include "khw_learn_internal.h"

// 1 where `form` and `other` answer alike: the same relation of the ladder or of the frame, the words read in the
// same order and the same signedness, and a frame form's the same reading. Two encodings the part answers alike for
// over every case are one form of the protocol: nothing the protocol queries distinguishes them
int khw_answers_alike(const KhwAnswered *form, const KhwAnswered *other)
{
    return (form->anchor == other->anchor) && (form->frame == other->frame) && (form->swapped == other->swapped) &&
           (form->signed_read == other->signed_read) &&
           ((form->frame == (unsigned int)FRAME_RELATION_COUNT) || (form->reading == other->reading));
}

// 1 where the two forms contain the same runs: as many, each at the same bits, the same place and the same type
static int khw_runs_alike(const KhwAnswered *form, const KhwAnswered *other)
{
    if (form->runs != other->runs)
    {
        return 0;
    }
    for (unsigned int run = 0u; run < form->runs; run += 1u)
    {
        if ((form->first[run] != other->first[run]) || (form->last[run] != other->last[run]) ||
            (form->place[run] != other->place[run]) || (form->type[run] != other->type[run]))
        {
            return 0;
        }
    }
    return 1;
}

// 1 where every bit `form` and `other` differ in lies in the turned range, KHW_TURN_FIRST to KHW_TURN_LAST, outside
// the runs of `form` that take a place: the bits the widening turns off a form, whose turn the form's answer does not
// read. A bit of the key, of the scheduler's word or of an operand is none of them
static int khw_differs_outside_operands(const KhwAnswered *form, const KhwAnswered *other)
{
    for (unsigned int bit = 0u; bit < (2u * 64u); bit += 1u)
    {
        const unsigned long long one = (bit < 64u) ? ((form->low >> bit) & 1ull) : ((form->high >> (bit - 64u)) & 1ull);
        const unsigned long long two =
            (bit < 64u) ? ((other->low >> bit) & 1ull) : ((other->high >> (bit - 64u)) & 1ull);
        if (one == two)
        {
            continue;
        }
        if ((bit < KHW_TURN_FIRST) || (bit > KHW_TURN_LAST))
        {
            return 0;
        }
        for (unsigned int run = 0u; run < form->runs; run += 1u)
        {
            if ((form->place[run] != KHW_PLACE_NONE) && (bit >= form->first[run]) && (bit <= form->last[run]))
            {
                return 0;
            }
        }
    }
    return 1;
}

// 1 where `older` is for `newer`, the two one form of the protocol: they answer alike, and either both contain
// their runs and contain the same, or `newer` contains none yet and differs from `older` only in bits of the turned
// range outside the runs of `older` that take a place, a turn the widening made of `older` whose answer `older`'s runs
// do not read. A form containing no runs is for nothing: which of its bits are its operands' is not yet known
int khw_form_is_for(const KhwAnswered *older, const KhwAnswered *newer)
{
    if ((older->runs == 0u) || !khw_answers_alike(older, newer))
    {
        return 0;
    }
    if (newer->runs != 0u)
    {
        return khw_runs_alike(older, newer);
    }
    return khw_differs_outside_operands(older, newer);
}

// 1 where one of `forms`, `count` of them, is for `newer`: the working set contains its form already
int khw_form_was_for(const KhwAnswered *forms, unsigned int count, const KhwAnswered *newer)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (khw_form_is_for(&forms[at], newer))
        {
            return 1;
        }
    }
    return 0;
}

// `forms`, `count` of them in the order they entered the working set, with every form an older one is for let go,
// the rest retained in their order: the count retained. The oldest of the forms that are one is, the encoding the
// set gained first, which the machine file and the part's .ksc name
unsigned int khw_forms_folded(KhwAnswered *forms, unsigned int count)
{
    unsigned int retained = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (khw_form_was_for(forms, retained, &forms[at]))
        {
            continue;
        }
        if (retained != at)
        {
            forms[retained] = forms[at];
        }
        retained += 1u;
    }
    return retained;
}
