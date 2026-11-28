// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// operands.c: the operands: the machine's listing, the same operation root, an operand dropped, changed, twinned or
// gained, a form turned, and a form unprinted
#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "probe_machine_internal.h"

// the encodings that the call decodes one form with: itself, and the 128 that are one bit from it
unsigned long long s_low[SASS_ENCODINGS];

unsigned long long s_high[SASS_ENCODINGS];
char s_texts[SASS_ENCODINGS][SASS_TEXT];

void sass_machine_listing(SassMachine *machine, const SassListing *listing)
{
    for (unsigned int number = 0u; number < listing->count; number += 1u)
    {
        const SassInstruction *const instruction = &listing->instructions[number];
        SassForm *found_form = NULL;
        // an instruction the listing gave no encoding for shows nothing about how its form encodes
        if ((instruction->low != 0ULL) || (instruction->high != 0ULL))
        {
            sass_machine_take(machine, instruction->text, instruction->low, instruction->high, &found_form);
        }
    }
}

// 1 where two operations share their name up to its first dot: IMAD.IADD, IMAD.MOV and IMAD share IMAD
static int sass_operation_root_same(const char *one, const char *other)
{
    // the two names up to their points, alike in length and in every byte
    const size_t length =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = one, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".");
    return (EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = other, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = ".") == length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = one, .other = other, .bytes = length) == 0);
}

// the immediate or predicate of `base` whose exit from the text gives `parts`, with every other operand as it was. It
// is the operand count where no one of them does. The predicate of a load leaves the text at PT under the same name,
// as the code of BPT.TRAP does at 0. An operand that leaves with the name is a bit of the operation and not of the
// operand. Examples are the carry-in of IMAD.X, which leaves with its .X, and the barrier of BAR.SYNC with BAR.SYNCALL
static unsigned int sass_operand_dropped(const SassInstructionParts *base, const SassInstructionParts *parts)
{
    const int named = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts->operation,
                                  .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = base->operation, .ci = EMBED_FALSE));
    for (unsigned int dropped = 0u; dropped < base->operands; dropped += 1u)
    {
        const int type =
            (base->type[dropped] == SASS_OPERAND_IMMEDIATE) || (base->type[dropped] == SASS_OPERAND_PREDICATE);
        int rest = (type && named) ? 1 : 0;
        for (unsigned int place = 0u; rest && (place < parts->operands); place += 1u)
        {
            const unsigned int was = (place < dropped) ? place : (place + 1u);
            rest = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts->operand[place],
                               .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = base->operand[was], .ci = EMBED_FALSE)) &&
                   (parts->mark[place] == base->mark[was]);
        }
        if (rest != 0)
        {
            return dropped;
        }
    }
    return base->operands;
}

// The operand that `text` changed against `base`. It is the operand count where the text changed none of them, or more
// than one. The same is true where it changed the guard, or the operation past the first part of its name. The
// disassembler hides a field in two ways, and the call reads both as fields of the operand:
//
// - a value that renames the operation, as the multiplier of IMAD prints IMAD.MOV at 0, IMAD.IADD at 1 and IMAD past
//   them. The name up to its first dot, the count of operands and the type of each stay, and the value of one operand
//   moves.
// - a value that leaves an immediate or a predicate out of the text. The code of BPT.TRAP does that at 0, and the
//   predicate of a load at PT. Every other operand stays as it was.
unsigned int sass_operand_changed(const SassInstructionParts *base, const char *text)
{
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    if ((!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.guard, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                     .other = base->guard, .ci = EMBED_FALSE)) ||
        !sass_operation_root_same(parts.operation, base->operation))
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
    const int renamed = (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operation,
                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = base->operation, .ci = EMBED_FALSE));
    unsigned int changed = base->operands;
    unsigned int count = 0u;
    // operands that print one field twice, as BAR.SYNC R0, R0 prints its one register. Each that moved carried the text
    // that the first did, and moved to the text that the first did
    int twins = 1;
    for (unsigned int place = 0u; place < base->operands; place += 1u)
    {
        if ((!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operand[place], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                         .other = base->operand[place], .ci = EMBED_FALSE)) ||
            (parts.mark[place] != base->mark[place]))
        {
            changed = (count == 0u) ? place : changed;
            count += 1u;
            twins =
                twins &&
                (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = base->operand[place], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = base->operand[changed], .ci = EMBED_FALSE)) &&
                (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operand[place], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = parts.operand[changed], .ci = EMBED_FALSE)) &&
                (base->type[place] == base->type[changed]);
        }
    }
    // a renamed operation carries the field only where the operand that moved is still the type it was
    if ((count != 0u) && renamed && (parts.type[changed] != base->type[changed]))
    {
        return base->operands;
    }
    return ((count == 1u) || ((count > 1u) && twins)) ? changed : base->operands;
}

// the operand of `base` past `operand` that `text` moves as it moves `operand`, with the same text before and after.
// It is the operand count where none does
unsigned int sass_operand_twin(const SassInstructionParts *base, const char *text, unsigned int operand)
{
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    for (unsigned int place = operand + 1u; (parts.operands == base->operands) && (place < base->operands); place += 1u)
    {
        if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = base->operand[place], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = base->operand[operand], .ci = EMBED_FALSE)) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operand[place], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = parts.operand[operand], .ci = EMBED_FALSE)) &&
            (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operand[place], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                         .other = base->operand[place], .ci = EMBED_FALSE)))
        {
            return place;
        }
    }
    return base->operands;
}

// a form's encoding and the 128 that are one bit from it, positioned into s_low and s_high
void sass_form_turned(const SassForm *form)
{
    s_low[0] = form->low;
    s_high[0] = form->high;
    for (unsigned int bit = 0u; bit < SASS_BITS; bit += 1u)
    {
        s_low[1u + bit] = s_low[0] ^ ((bit < 64u) ? (1ULL << bit) : 0ULL);
        s_high[1u + bit] = s_high[0] ^ ((bit >= 64u) ? (1ULL << (bit - 64u)) : 0ULL);
    }
}

// 1 where `text` is `base` under another name, with one operand more at its end. The guard, and the name of the
// operation before its first dot, stay. The name past it changes. Every operand that `base` prints is still there, a
// mark aside: IMAD.X prints as ~R the register that IMAD.IADD prints as -R.
//
// Where the same operation prints one operand more, that is the operand at a value that the text leaves out. A load
// prints its predicate so, past PT. The form carries it as its own bits
static int sass_operand_gained(const SassInstructionParts *base, const char *text)
{
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    int same_base = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.guard, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = base->guard, .ci = EMBED_FALSE)) &&
                    sass_operation_root_same(parts.operation, base->operation) &&
                    (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                 .other = base->operation, .ci = EMBED_FALSE)) &&
                    (parts.operands == (base->operands + 1u));
    for (unsigned int place = 0u; same_base && (place < base->operands); place += 1u)
    {
        same_base = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = parts.operand[place],
                                .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = base->operand[place], .ci = EMBED_FALSE));
    }
    return same_base;
}

// The operand that a form carries and does not print, with its run stored as operand `base->operands`, one past those
// that it prints. A turned bit that adds one operand at the end and keeps the rest names it. Bit 74 of IMAD.IADD does
// that, and prints IMAD.X with its carry-in predicate. The own bits of that encoding, turned over, give the run of the
// operand that it added. The form takes the first such bit and one unprinted operand. The return is 1, or 0 where the
// disassembler failed on the turned bits
int sass_form_unprinted(SassForm *form, const SassInstructionParts *base, const char *architecture, const char *folder,
                        unsigned int number)
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
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, folder);
        mmgr_line_put(&buffer_line, "/form_");
        mmgr_line_decimal(&buffer_line, number, 3u);
        mmgr_line_put(&buffer_line, "_unprinted");
        (void)mmgr_line_finish(&buffer_line);
    }
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
        // a run ends where the operand stops its change, and the two words never share one
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
