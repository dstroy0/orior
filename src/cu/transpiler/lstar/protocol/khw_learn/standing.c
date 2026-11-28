// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// standing.c: one form that takes the place of the encodings that answer as it does. The working set holds one form
// for each answer, and a turn of the bits of a form outside its operands is that form
#include "khw_learn_internal.h"

// 1 where `form` and `other` answer alike, with the same relation of the ladder or of the frame. They read the words in
// the same order, with the same signedness. A frame form must also have the same reading. Two encodings that the part
// answers alike for over every case are one form of the protocol. Nothing that the protocol queries tells them apart
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

// 1 where every bit that `form` and `other` differ in lies in the turned range, KHW_TURN_FIRST to KHW_TURN_LAST,
// outside the runs of `form` that take a place. Those are the bits that the widening turns off a form, whose turn the
// answer of the form does not read. A bit of the key, of the word of the scheduler or of an operand is none of them
static int khw_differs_outside_operands(const KhwAnswered *form, const KhwAnswered *other)
{
    for (unsigned int bit = 0u; bit < (2u * 64u); bit += 1u)
    {
        const unsigned long long one = (bit < 64u) ? ((form->low >> bit) & 1ULL) : ((form->high >> (bit - 64u)) & 1ULL);
        const unsigned long long two =
            (bit < 64u) ? ((other->low >> bit) & 1ULL) : ((other->high >> (bit - 64u)) & 1ULL);
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

// 1 where `older` takes the place of `newer`, as the two are one form of the protocol. They answer alike, and one of
// these is true:
//
// - both hold their runs, and hold the same runs.
// - `newer` holds none yet, and differs from `older` only in bits of the turned range outside the runs of `older`
//   that take a place. That is a turn that the widening made of `older`, whose answer the runs of `older` do not read.
//
// A form that holds no runs takes the place of nothing, because which of its bits belong to its operands is not yet
// known
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

// 1 where one of `forms`, `count` of them, takes the place of `newer`. Then the working set holds its form already
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

// `forms`, `count` of them in the order that they entered the working set. The call lets go every form whose place an
// older one takes, and keeps the rest in their order. The return is the count kept. Of the forms that are one, the
// oldest stays. That is the encoding that the set gained first, which the machine file and the .ksc of the part name
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
