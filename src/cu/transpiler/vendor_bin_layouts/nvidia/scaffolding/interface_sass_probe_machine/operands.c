// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// operands.c: the operands: the machine's listing, the same operation root, an operand dropped, changed, twinned or gained, a form turned, and a form unprinted
#include "probe_machine_internal.h"

// the encodings one form is decoded with: itself, and the 128 that are one bit from it
unsigned long long s_low[SASS_ENCODINGS];

unsigned long long s_high[SASS_ENCODINGS];
char s_texts[SASS_ENCODINGS][SASS_TEXT];

void sass_machine_listing(SassMachine *machine, const SassListing *listing)
{
    for (unsigned int number = 0u; number < listing->count; number += 1u)
    {
        const SassInstruction *const instruction = &listing->instructions[number];
        SassForm *kept = NULL;
        // an instruction the listing gave no encoding for says nothing about how its form encodes
        if ((instruction->low != 0ull) || (instruction->high != 0ull))
        {
            sass_machine_take(machine, instruction->text, instruction->low, instruction->high, &kept);
        }
    }
}

// 1 where two operations share their name up to its first dot: IMAD.IADD, IMAD.MOV and IMAD share IMAD
static int sass_operation_root_same(const char *one, const char *other)
{
    size_t length = 0u;
    while ((one[length] != '\0') && (one[length] != '.') && (one[length] == other[length]))
    {
        length += 1u;
    }
    return ((one[length] == '\0') || (one[length] == '.')) && ((other[length] == '\0') || (other[length] == '.'));
}

// the immediate or predicate of `base` whose leaving the text gives `parts`, every other operand as it was, or the
// operand count where no one of them does. A load's predicate leaves the text at PT under the same name, as
// BPT.TRAP's code does at 0. An operand that leaves with the name, as IMAD.X's carry-in leaves with its .X and
// BAR.SYNC's barrier with BAR.SYNCALL, is the operation's bit and not the operand's
static unsigned int sass_operand_dropped(const SassInstructionParts *base, const SassInstructionParts *parts)
{
    const int named = (strcmp(parts->operation, base->operation) == 0);
    for (unsigned int dropped = 0u; dropped < base->operands; dropped += 1u)
    {
        const int kind = (base->kind[dropped] == SASS_OPERAND_IMMEDIATE) || (base->kind[dropped] == SASS_OPERAND_PREDICATE);
        int rest = (kind && named) ? 1 : 0;
        for (unsigned int place = 0u; rest && (place < parts->operands); place += 1u)
        {
            const unsigned int was = (place < dropped) ? place : (place + 1u);
            rest = (strcmp(parts->operand[place], base->operand[was]) == 0) && (parts->mark[place] == base->mark[was]);
        }
        if (rest != 0)
        {
            return dropped;
        }
    }
    return base->operands;
}

// The operand `text` changed against `base`, or the operand count where it changed none of them, more than one of
// them, the guard, or the operation past its name's first part. The disassembler hides a field two ways, and both are
// read as the operand's: a value that renames the operation, as IMAD's multiplier prints IMAD.MOV at 0, IMAD.IADD at 1
// and IMAD past them, where the name up to its first dot, the count of operands and each one's kind stay and one
// operand's value moves; and a value that leaves an immediate or a predicate out of the text, as BPT.TRAP's code does
// at 0 and a load's predicate at PT, where every other operand stays as it was
unsigned int sass_operand_changed(const SassInstructionParts *base, const char *text)
{
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    if ((strcmp(parts.guard, base->guard) != 0) || !sass_operation_root_same(parts.operation, base->operation))
    {
        return base->operands;
    }
    if ((parts.operands + 1u) == base->operands)
    {
        return sass_operand_dropped(base, &parts);
    }
    if (parts.operands != base->operands)
    {
        return base->operands;
    }
    const int renamed = (strcmp(parts.operation, base->operation) != 0);
    unsigned int changed = base->operands;
    unsigned int count = 0u;
    // operands that print one field twice, as BAR.SYNC R0, R0 prints its one register: each that moved held the
    // text the first did and moved to the text the first did
    int twins = 1;
    for (unsigned int place = 0u; place < base->operands; place += 1u)
    {
        if ((strcmp(parts.operand[place], base->operand[place]) != 0) || (parts.mark[place] != base->mark[place]))
        {
            changed = (count == 0u) ? place : changed;
            count += 1u;
            twins = twins && (strcmp(base->operand[place], base->operand[changed]) == 0) &&
                    (strcmp(parts.operand[place], parts.operand[changed]) == 0) &&
                    (base->kind[place] == base->kind[changed]);
        }
    }
    // a renamed operation holds the field only where the operand that moved is still the kind it was
    if ((count != 0u) && renamed && (parts.kind[changed] != base->kind[changed]))
    {
        return base->operands;
    }
    return ((count == 1u) || ((count > 1u) && twins)) ? changed : base->operands;
}

// the operand of `base` past `operand` that `text` moves as it moves `operand`, holding the same text before and
// after, or the operand count where none does
unsigned int sass_operand_twin(const SassInstructionParts *base, const char *text, unsigned int operand)
{
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    for (unsigned int place = operand + 1u; (parts.operands == base->operands) && (place < base->operands);
         place += 1u)
    {
        if ((strcmp(base->operand[place], base->operand[operand]) == 0) &&
            (strcmp(parts.operand[place], parts.operand[operand]) == 0) &&
            (strcmp(parts.operand[place], base->operand[place]) != 0))
        {
            return place;
        }
    }
    return base->operands;
}

// a form's encoding and the 128 that are one bit from it, laid into s_low and s_high
void sass_form_turned(const SassForm *form)
{
    s_low[0] = form->low;
    s_high[0] = form->high;
    for (unsigned int bit = 0u; bit < SASS_BITS; bit += 1u)
    {
        s_low[1u + bit] = s_low[0] ^ ((bit < 64u) ? (1ull << bit) : 0ull);
        s_high[1u + bit] = s_high[0] ^ ((bit >= 64u) ? (1ull << (bit - 64u)) : 0ull);
    }
}

// 1 where `text` is `base` under another name with one operand more at its end: the guard and the operation's name
// before its first dot kept, the name past it changed, and every operand `base` prints still there, a mark aside,
// since IMAD.X prints as ~R the register IMAD.IADD prints as -R. The same operation printing one operand more is
// that operand at a value the text leaves out, as a load prints its predicate past PT, and the form holds it as its
// own bits
static int sass_operand_gained(const SassInstructionParts *base, const char *text)
{
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    int kept = (strcmp(parts.guard, base->guard) == 0) && sass_operation_root_same(parts.operation, base->operation) &&
               (strcmp(parts.operation, base->operation) != 0) && (parts.operands == (base->operands + 1u));
    for (unsigned int place = 0u; kept && (place < base->operands); place += 1u)
    {
        kept = (strcmp(parts.operand[place], base->operand[place]) == 0);
    }
    return kept;
}

// The operand a form holds and does not print, its run kept as operand `base->operands`, one past those it prints. A
// turned bit that adds one operand at the end and keeps the rest names it, as bit 74 of IMAD.IADD does, which prints
// IMAD.X with its carry-in predicate; that encoding's own bits turned over give the run of the operand it added. The
// form takes the first such bit and one unprinted operand. 1, or 0 where the disassembler failed on the turned bits
int sass_form_unprinted(SassForm *form, const SassInstructionParts *base, const char *architecture,
                               const char *folder, unsigned int number)
{
    unsigned int gained = SASS_BITS;
    for (unsigned int bit = 0u; (gained == SASS_BITS) && (bit < SASS_BITS); bit += 1u)
    {
        gained = sass_operand_gained(base, s_texts[1u + bit]) ? bit : gained;
    }
    if (gained == SASS_BITS)
    {
        return 1;
    }
    SassForm sibling = *form;
    sibling.low = s_low[1u + gained];
    sibling.high = s_high[1u + gained];
    sass_form_turned(&sibling);
    char path[1024];
    snprintf(path, sizeof(path), "%s/form_%03u_unprinted", folder, number);
    if (!sass_decode(architecture, path, s_low, s_high, SASS_ENCODINGS, s_texts))
    {
        return 0;
    }
    SassInstructionParts with;
    sass_instruction_read(s_texts[0], &with);
    const unsigned int added = base->operands;
    unsigned int first = 0u;
    int running = 0;
    for (unsigned int bit = 0u; bit <= SASS_BITS; bit += 1u)
    {
        const int changes = (bit < SASS_BITS) && (sass_operand_changed(&with, s_texts[1u + bit]) == added);
        // a run ends where the operand stops changing, and the two words never share one
        if (running && (!changes || (bit == 64u)) && (form->runs < SASS_MACHINE_RUNS))
        {
            form->run[form->runs].operand = added;
            form->run[form->runs].first = first;
            form->run[form->runs].last = bit - 1u;
            form->run[form->runs].place = SASS_RUN_NO_PLACE;
            form->runs += 1u;
            running = 0;
        }
        if (changes && !running)
        {
            first = bit;
            running = 1;
        }
    }
    return 1;
}
