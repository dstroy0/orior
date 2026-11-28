// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fields.c: the fields: bits written, a register's, a constant's, an address's and an operand's value, the fields of a
// type and the field at a place, the places of an instruction found, the high word written, and the control word made
// safe
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "sass_assemble_internal.h"

// the fields a type of operand may sit in, in the order an instruction fills them
static const SassField s_register_fields[] = {
    {16u, 8u, 1u, 0u}, {24u, 8u, 1u, 0u}, {32u, 8u, 1u, 0u}, {64u, 8u, 1u, 0u}};

// A predicate operand takes three bits and the fourth negates it; the probes found six places one sits in. A load's
// predicate at 64 stores its number inverted, PT as 000, and a load whose predicate is false writes 0
// (interface_sass_unprinted.md)
static const SassField s_predicate_fields[] = {{64u, 3u, 1u, 1u}, {68u, 3u, 1u, 0u}, {77u, 3u, 1u, 0u},
                                               {81u, 3u, 1u, 0u}, {84u, 3u, 1u, 0u}, {87u, 3u, 1u, 0u}};

static const SassField s_immediate_fields[] = {{32u, 32u, 1u, 0u}, {72u, 8u, 1u, 0u}};

// a constant's offset counts words, and an address's bytes; both lie above the register fields. A constant's offset is
// the vendor's 14 bits from bit 40, its bank the five bits from 54 (ConstBankAddress2, nvidia/tables/read.c)
static const SassField s_constant_fields[] = {{40u, 14u, 4u, 0u}};

static const SassField s_offset_fields[] = {{40u, 24u, 1u, 0u}};

// The uniform register that a memory operand reads its descriptor from, in the order that an instruction fills them.
// It is bits 32 to 37 where no other operand uses bit 32, as a load has it, else 64 to 69, as a store has it. A memory
// operand that takes one is an address with a run of its own at bit 101. That bit tells whether the listing prints the
// descriptor: set, desc[URn]; clear, term[URn]
static const SassField s_descriptor_fields[] = {{32u, 6u, 1u, 0u}, {64u, 6u, 1u, 0u}};

// `bits` of `value` written from bit `first` of the instruction, a bit at a time. The target of a branch starts in the
// low word and ends in the high one
void sass_bits_write(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int bits,
                     unsigned long long value)
{
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int at = first + bit;
        unsigned long long *const word = (at < 64u) ? low : high;
        const unsigned int place = (at < 64u) ? at : (at - 64u);
        *word = (*word & ~(1ULL << place)) | (((value >> bit) & 1ULL) << place);
    }
}

// The number that a register, predicate or uniform register names: RZ is 255 and PT is 7. A name that ends in .hi is
// the high half of what it goes on (sass.krs). For a register, that is the second register of a 64-bit pair, one past
// the register that it names. RZ has no second half, because a pair of zero words reads zero at both. The high half
// of a number is its high word, which sass_operand_value takes
unsigned long long sass_register_value(const char *text)
{
    if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "RZ",
                   .ci = EMBED_FALSE))
    {
        return 255ULL;
    }
    if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "PT",
                   .ci = EMBED_FALSE))
    {
        return 7ULL;
    }
    const char *const digits = text + ((text[0] == 'U') ? 2u : 1u);
    return EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = digits, .end = NULL, .base = 10) +
           (sass_high_half(text) ? 1ULL : 0ULL);
}

// the offset that a constant names, c[bank][offset], and the bank through `bank`. The offset can be a sum, with each
// term after a +. Thus sass.krs writes a parameter of a kernel at the base of the target plus the own place of the
// parameter, c[0x0][0x160+8]
unsigned long long sass_constant_value(const char *text, unsigned long long *bank)
{
    const char *const first =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '[');
    const char *const second = (first != NULL) ? EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = first + 1,
                                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '[')
                                               : NULL;
    *bank = (first != NULL) ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = first + 1, .end = NULL, .base = 0)
                            : 0ULL;
    if (second == NULL)
    {
        return 0ULL;
    }
    const char *walk = NULL;
    unsigned long long offset =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = second + 1, .end = &walk, .base = 0);
    while (*walk == '+')
    {
        offset += EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = walk + 1, .end = &walk, .base = 0);
    }
    return offset;
}

// the base register an address names, [R2.64+0x4], and its offset through `offset`
static unsigned long long sass_address_value(const char *text, unsigned long long *offset)
{
    const char *const open =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '[');
    const char *const plus = (open != NULL) ? EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = open,
                                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '+')
                                            : NULL;
    *offset =
        (plus != NULL) ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = plus + 1, .end = NULL, .base = 0) : 0ULL;
    return (open != NULL) ? sass_register_value(open + 1) : 0ULL;
}

// the value that the operand at `place` of `parts` carries, and through `wanted` the field that it must sit in. The
// return is 0 where the assembler cannot make a number of it, and then the assembler matches it by its text
int sass_operand_value(const SassInstructionParts *parts, unsigned int place, unsigned long long address,
                       unsigned long long target, unsigned long long *value, unsigned long long *offset)
{
    const char *const text = parts->operand[place];
    *offset = 0ULL;
    switch (parts->type[place])
    {
    case SASS_OPERAND_REGISTER:
    case SASS_OPERAND_PREDICATE:
    case SASS_OPERAND_UNIFORM:
        *value = sass_register_value(text);
        return 1;
    case SASS_OPERAND_IMMEDIATE: {
        // .hi is the high half of the thing that it goes on, whatever that thing is (sass.krs). For a register it is
        // the second register of the pair, and for a number it is the high word of the number. The number reader
        // (cellul.to_ulong_in) stops at the dot, thus the call reads the stem and shifts it. The call shifts a
        // negative number as the 64-bit word that holds it
        const unsigned long long whole =
            (text[0] == '-') ? (unsigned long long)(-(long long)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                                                           .src = text + 1, .end = NULL, .base = 0))
                             : EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = text, .end = NULL, .base = 0);
        *value = sass_high_half(text) ? (whole >> 32u) : whole;
        return 1;
    }
    case SASS_OPERAND_CONSTANT:
        *value = sass_constant_value(text, offset);
        return 1;
    case SASS_OPERAND_ADDRESS:
        *value = sass_address_value(text, offset);
        return 1;
    case SASS_OPERAND_LABEL:
        // a branch counts its target from the instruction after it
        *value = target - (address + 16ULL);
        return 1;
    default:
        return 0;
    }
}

// the fields of `type`, and how many there are
static const SassField *sass_fields_of(unsigned int type, unsigned int *count)
{
    switch (type)
    {
    case SASS_OPERAND_REGISTER:
    case SASS_OPERAND_UNIFORM:
    case SASS_OPERAND_ADDRESS:
        *count = (unsigned int)SASS_REGISTER_FIELDS;
        return s_register_fields;
    case SASS_OPERAND_PREDICATE:
        *count = (unsigned int)SASS_PREDICATE_FIELDS;
        return s_predicate_fields;
    case SASS_OPERAND_IMMEDIATE:
    case SASS_OPERAND_LABEL:
        *count = (unsigned int)SASS_IMMEDIATE_FIELDS;
        return s_immediate_fields;
    case SASS_OPERAND_CONSTANT:
        *count = 1u;
        return s_constant_fields;
    default:
        *count = 0u;
        return NULL;
    }
}

// the field of `type` that starts inside `run`, or NULL where the type takes none there. A run is the bits that the
// probe saw change one operand. It is the field, and the bits beside it that change the same operand, such as a bit
// that sets the type of the operand. Another example is the low bits of an offset that the operation counts in wider
// units. The run of a constant can lie anywhere inside its field. The run of a frame form is the bits that its place
// moved, as the run of a parameter is 42 to 44
static const SassField *sass_field_at(unsigned int type, const SassRun *run)
{
    unsigned int count = 0u;
    const SassField *const fields = sass_fields_of(type, &count);
    const SassField *found = NULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int last = (fields[at].first + fields[at].bits) - 1u;
        const int begins = (fields[at].first >= run->first) && (fields[at].first <= run->last);
        const int inside = (type == SASS_OPERAND_CONSTANT) && (run->first <= last) && (run->last >= fields[at].first);
        found = (begins || inside) ? &fields[at] : found;
    }
    return found;
}

// the operands of the form, put in the fields that the runs of the probe name. Each operand takes the run that starts
// where a field of its type starts, and an address takes the offset field beside its base register. The return is 1
// where the call put every operand, else 0 and the operand that it could not put
int sass_places_find(const SassForm *form, SassPlace *places, unsigned int *unplaced)
{
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &places[place], .bytes = sizeof(places[place]), .val = 0);
        const unsigned int type = form->type[place];
        // a label that names a symbol and not a label of the text is a relocation. The field holds nothing, and the
        // loader fills it. The assembler then checks the instruction against the one that the probe saw the form with
        const char *const open = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = base.operand[place],
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '(');
        const int relocated = (type == SASS_OPERAND_LABEL) && ((open == NULL) || (open[1] != '.'));
        if ((type == SASS_OPERAND_UNKNOWN) || (type == SASS_OPERAND_SYSTEM) || relocated)
        {
            // the assembler checks a system register, and all else that it cannot count, against the own text of the
            // base
            places[place].by_text = 1;
            continue;
        }
        // a branch's target is its own field, which crosses into the high word and shares its bits with no other
        if (type == SASS_OPERAND_LABEL)
        {
            places[place].value.first = SASS_BRANCH_FIRST;
            places[place].value.bits = SASS_BRANCH_BITS;
            places[place].value.scale = SASS_BRANCH_STEP;
            continue;
        }
        // A number in the bits where the distance of a branch sits is that distance. The probe found its run, which
        // starts there and goes into the high word. A run that starts there and ends inside the low word is a field of
        // its own. The code of BPT.TRAP is such a field, three bits from bit 34
        int distance = 0;
        for (unsigned int number = 0u; (type == SASS_OPERAND_IMMEDIATE) && (number < form->runs); number += 1u)
        {
            distance = distance || ((form->run[number].operand == place) &&
                                    (form->run[number].first == SASS_BRANCH_FIRST) && (form->run[number].last >= 63u));
        }
        if (distance)
        {
            places[place].value.first = SASS_BRANCH_FIRST;
            places[place].value.bits = SASS_BRANCH_BITS;
            places[place].value.scale = SASS_BRANCH_STEP;
            continue;
        }
        int found = 0;
        for (unsigned int number = 0u; number < form->runs; number += 1u)
        {
            const SassRun *const run = &form->run[number];
            if (run->operand != place)
            {
                continue;
            }
            const SassField *const field = sass_field_at(type, run);
            if (field != NULL)
            {
                places[place].value = *field;
                // a field holds no more bits than the run that the probe saw change the operand. The call writes the
                // offset of a constant whole, its base plus its place, in all of its field
                const unsigned int room = (run->last - field->first) + 1u;
                places[place].value.bits =
                    ((type == SASS_OPERAND_CONSTANT) || (room >= field->bits)) ? field->bits : room;
                found = 1;
            }
            if ((type == SASS_OPERAND_ADDRESS) && (run->first == s_offset_fields[0].first))
            {
                places[place].offset = s_offset_fields[0];
                places[place].has_offset = 1;
            }
        }
        // a memory operand with a run of its own at the bit that shows the descriptor reads one. Its uniform register
        // takes the first descriptor field that no run of the form uses
        int displayed = 0;
        for (unsigned int number = 0u; (type == SASS_OPERAND_ADDRESS) && (number < form->runs); number += 1u)
        {
            displayed = displayed || ((form->run[number].operand == place) &&
                                      (form->run[number].first == SASS_DESCRIPTOR_DISPLAYED) &&
                                      (form->run[number].last == SASS_DESCRIPTOR_DISPLAYED));
        }
        for (unsigned int at = 0u; displayed && (places[place].has_descriptor == 0) && (at < SASS_DESCRIPTOR_FIELDS);
             at += 1u)
        {
            const SassField *const field = &s_descriptor_fields[at];
            int place_found = 0;
            for (unsigned int number = 0u; number < form->runs; number += 1u)
            {
                place_found = place_found || ((form->run[number].first < (field->first + field->bits)) &&
                                              (form->run[number].last >= field->first));
            }
            if (place_found == 0)
            {
                places[place].descriptor = *field;
                places[place].has_descriptor = 1;
            }
        }
        // Where no field of the operand type starts inside its own run, the widest run that the probe saw change the
        // operand is its field. The field starts at the first bit of the run and is as long as the run. The
        // code of BPT.TRAP starts at bit 34, where no immediate field of the list above starts. An operand that a field
        // above puts stays where that field put it
        const int counted = (type == SASS_OPERAND_IMMEDIATE) || (type == SASS_OPERAND_REGISTER) ||
                            (type == SASS_OPERAND_PREDICATE) || (type == SASS_OPERAND_UNIFORM);
        const SassRun *widest = NULL;
        for (unsigned int number = 0u; (found == 0) && counted && (number < form->runs); number += 1u)
        {
            const SassRun *const run = &form->run[number];
            const int wider = (widest == NULL) || ((run->last - run->first) > (widest->last - widest->first));
            widest = ((run->operand == place) && wider) ? run : widest;
        }
        if ((found == 0) && (widest != NULL))
        {
            places[place].value.first = widest->first;
            places[place].value.bits = (widest->last - widest->first) + 1u;
            places[place].value.scale = 1u;
            found = 1;
        }
        if (found == 0)
        {
            *unplaced = place;
            return 0;
        }
    }
    return 1;
}

// the scheduler's bits, every one of them above bit 63 and so in the high word alone
static void sass_high_write(unsigned long long *high, unsigned int first, unsigned int bits, unsigned long long value)
{
    const unsigned long long mask = (1ULL << bits) - 1ULL;
    *high = (*high & ~(mask << (first - 64u))) | ((value & mask) << (first - 64u));
}

// the bits of the scheduler, set so that every instruction waits for all the instructions before it. The bits set a
// stall of the soonest read of the result of its operation, no reuse, and a wait on every barrier. A fixed result is
// back after that many cycles, and an operation with no measured count stalls the longest.
//
// An instruction whose result comes back late must set a barrier, so that the wait has a thing to wait on. A store must
// set one for the next writer of its operands. The schedule of the operation tells which instructions those are
// (sass_operation_schedule). The call also sets a barrier that the own encoding of the form sets, except on an
// operation whose result has a measured cycle count. Nothing releases a barrier that such an operation sets, and the
// wait of the next instruction on all six never ends. A barrier that no instruction set is already at rest, and a wait
// on all six costs nothing where no instruction set one
void sass_control_safe(const SassMachine *machine, const SassForm *form, unsigned long long *high)
{
    unsigned int soonest = SASS_STALL_MAX_LEN;
    const unsigned int schedule = sass_operation_schedule(machine, form->operation, &soonest);
    const int fixed = (schedule == SASS_SCHEDULE_FIXED) && (soonest != SASS_STALL_MAX_LEN);
    const int wrote =
        (schedule == SASS_SCHEDULE_LATE) || (!fixed && sass_barrier_set(form->high, SASS_WRITE_BARRIER_FIRST));
    const int read =
        (schedule == SASS_SCHEDULE_STORE) || (!fixed && sass_barrier_set(form->high, SASS_READ_BARRIER_FIRST));
    sass_high_write(high, SASS_STALL_FIRST, 4u, soonest);
    sass_high_write(high, SASS_YIELD_FIRST, 1u, 0ULL);
    sass_high_write(high, SASS_WRITE_BARRIER_FIRST, 3u, wrote ? 0ULL : SASS_BARRIER_NONE);
    sass_high_write(high, SASS_READ_BARRIER_FIRST, 3u, read ? 1ULL : SASS_BARRIER_NONE);
    sass_high_write(high, SASS_WAIT_FIRST, 6u, SASS_WAIT_EVERY);
    sass_high_write(high, SASS_REUSE_FIRST, 4u, 0ULL);
}
