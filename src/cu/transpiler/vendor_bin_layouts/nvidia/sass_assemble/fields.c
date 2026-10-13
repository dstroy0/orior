// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fields.c: the fields: bits written, a register's, a constant's, an address's and an operand's value, the fields of a kind and the field at a place, the places of an instruction found, the high word written, and the control word made safe
#include "sass_assemble_internal.h"

// the fields a kind of operand may sit in, in the order an instruction fills them
static const SassField s_register_fields[] = {
    {16u, 8u, 1u, 0u}, {24u, 8u, 1u, 0u}, {32u, 8u, 1u, 0u}, {64u, 8u, 1u, 0u}};

// A predicate operand takes three bits and the fourth negates it; the probes found six places one sits in. A load's
// predicate at 64 holds its number inverted, PT as 000, and a load whose predicate is false writes 0
// (interface_sass_unprinted.md)
static const SassField s_predicate_fields[] = {{64u, 3u, 1u, 1u}, {68u, 3u, 1u, 0u}, {77u, 3u, 1u, 0u},
                                               {81u, 3u, 1u, 0u}, {84u, 3u, 1u, 0u}, {87u, 3u, 1u, 0u}};

static const SassField s_immediate_fields[] = {{32u, 32u, 1u, 0u}, {72u, 8u, 1u, 0u}};

// a constant's offset counts words, and an address's bytes; both lie above the register fields
static const SassField s_constant_fields[] = {{40u, 16u, 4u, 0u}};

static const SassField s_offset_fields[] = {{40u, 24u, 1u, 0u}};

// The uniform register a memory operand reads its descriptor from, in the order an instruction fills them: bits 32 to
// 37 where no other operand holds bit 32, as a load has it, else 64 to 69, as a store has it. A memory operand that
// takes one is an address with a run of its own at bit 101, the bit that says whether the descriptor is printed:
// set, desc[URn]; clear, term[URn]
static const SassField s_descriptor_fields[] = {{32u, 6u, 1u, 0u}, {64u, 6u, 1u, 0u}};

// `bits` of `value` written from bit `first` of the instruction, a bit at a time, since a branch's target begins in
// the low word and ends in the high one
void sass_bits_write(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int bits,
                            unsigned long long value)
{
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int at = first + bit;
        unsigned long long *const word = (at < 64u) ? low : high;
        const unsigned int place = (at < 64u) ? at : (at - 64u);
        *word = (*word & ~(1ull << place)) | (((value >> bit) & 1ull) << place);
    }
}

// The number a register, predicate or uniform register names: RZ is 255 and PT is 7. A name ending in .hi is the
// high half of what it is written on (sass.krs), which for a register is the second register of a 64-bit pair, one
// past the one it is named from; RZ has no second half, since a pair of zero words reads zero at both. The high half
// of a number is its high word, which sass_operand_value takes
unsigned long long sass_register_value(const char *text)
{
    if (strncmp(text, "RZ", 2u) == 0)
    {
        return 255ull;
    }
    if (strncmp(text, "PT", 2u) == 0)
    {
        return 7ull;
    }
    const char *const digits = text + ((text[0] == 'U') ? 2u : 1u);
    return strtoull(digits, NULL, 10) + (sass_high_half(text) ? 1ull : 0ull);
}

// the offset a constant names, c[bank][offset], and the bank through `bank`. The offset may be a sum, each term after
// a +, as sass.krs writes a kernel's parameter at the target's base and the parameter's own place, c[0x0][0x160+8]
unsigned long long sass_constant_value(const char *text, unsigned long long *bank)
{
    const char *const first = strchr(text, '[');
    const char *const second = (first != NULL) ? strchr(first + 1, '[') : NULL;
    *bank = (first != NULL) ? strtoull(first + 1, NULL, 0) : 0ull;
    if (second == NULL)
    {
        return 0ull;
    }
    char *walk = NULL;
    unsigned long long offset = strtoull(second + 1, &walk, 0);
    while (*walk == '+')
    {
        offset += strtoull(walk + 1, &walk, 0);
    }
    return offset;
}

// the base register an address names, [R2.64+0x4], and its offset through `offset`
static unsigned long long sass_address_value(const char *text, unsigned long long *offset)
{
    const char *const open = strrchr(text, '[');
    const char *const plus = (open != NULL) ? strchr(open, '+') : NULL;
    *offset = (plus != NULL) ? strtoull(plus + 1, NULL, 0) : 0ull;
    return (open != NULL) ? sass_register_value(open + 1) : 0ull;
}

// the value the operand at `place` of `parts` carries, and the field it wants to sit in through `wanted`; 0 where the
// assembler cannot turn it into a number, which leaves it to be matched by its text
int sass_operand_value(const SassInstructionParts *parts, unsigned int place, unsigned long long address,
                              unsigned long long target, unsigned long long *value, unsigned long long *offset)
{
    const char *const text = parts->operand[place];
    *offset = 0ull;
    switch (parts->kind[place])
    {
    case SASS_OPERAND_REGISTER:
    case SASS_OPERAND_PREDICATE:
    case SASS_OPERAND_UNIFORM:
        *value = sass_register_value(text);
        return 1;
    case SASS_OPERAND_IMMEDIATE:
    {
        // .hi is the high half of the thing it is written on, whatever that thing is (sass.krs): for a register it
        // is the pair's second register, and for a number it is the number's high word. strtoull stops at the dot:
        // so the stem is read and shifted; a negative number is shifted as the 64-bit word it is written into
        const unsigned long long whole = (text[0] == '-')
                                             ? (unsigned long long)(-(long long)strtoull(text + 1, NULL, 0))
                                             : strtoull(text, NULL, 0);
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
        *value = target - (address + 16ull);
        return 1;
    default:
        return 0;
    }
}

// the fields of `kind`, and how many there are
static const SassField *sass_fields_of(unsigned int kind, unsigned int *count)
{
    switch (kind)
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

// the field of `kind` that begins inside `run`, or NULL where the kind takes none there. A run is the bits the probe
// saw change one operand, being the field and whatever lies beside it that changes the same operand. A bit that
// says what kind the operand is, or the low bits of an offset the operation counts in wider units
static const SassField *sass_field_at(unsigned int kind, const SassRun *run)
{
    unsigned int count = 0u;
    const SassField *const fields = sass_fields_of(kind, &count);
    const SassField *found = NULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        found = ((fields[at].first >= run->first) && (fields[at].first <= run->last)) ? &fields[at] : found;
    }
    return found;
}

// the form's operands placed in the fields the probe's runs name: each operand takes the run that begins where a
// field of its kind begins, and an address takes the offset field beside its base register. 1 where every operand was
// placed, else 0 and the operand that could not be
int sass_places_find(const SassForm *form, SassPlace *places, unsigned int *unplaced)
{
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        memset(&places[place], 0, sizeof(places[place]));
        const unsigned int kind = form->kind[place];
        // a label that names a symbol and not a label of the text is a relocation: the field holds nothing and
        // the loader fills it. The instruction is then checked against the one the form was seen with
        const char *const open = strchr(base.operand[place], '(');
        const int relocated = (kind == SASS_OPERAND_LABEL) && ((open == NULL) || (open[1] != '.'));
        if ((kind == SASS_OPERAND_UNKNOWN) || (kind == SASS_OPERAND_SYSTEM) || relocated)
        {
            // a system register and anything else the assembler cannot count is checked against the base's own text
            places[place].by_text = 1;
            continue;
        }
        // a branch's target is its own field, which crosses into the high word and shares its bits with no other
        if (kind == SASS_OPERAND_LABEL)
        {
            places[place].value.first = SASS_BRANCH_FIRST;
            places[place].value.bits = SASS_BRANCH_BITS;
            places[place].value.scale = SASS_BRANCH_STEP;
            continue;
        }
        // A number in the bits a branch's distance sits in is that distance: the probe found its run beginning there and
        // reaching into the high word. A run that begins there and ends inside the low word is a field of its own, as
        // BPT.TRAP's code is, three bits from bit 34
        int distance = 0;
        for (unsigned int number = 0u; (kind == SASS_OPERAND_IMMEDIATE) && (number < form->runs); number += 1u)
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
            const SassField *const field = sass_field_at(kind, run);
            if (field != NULL)
            {
                places[place].value = *field;
                // a field holds no more bits than the run the probe saw change the operand
                const unsigned int room = (run->last - field->first) + 1u;
                places[place].value.bits = (room < field->bits) ? room : field->bits;
                found = 1;
            }
            if ((kind == SASS_OPERAND_ADDRESS) && (run->first == s_offset_fields[0].first))
            {
                places[place].offset = s_offset_fields[0];
                places[place].has_offset = 1;
            }
        }
        // a memory operand with a run of its own at the bit that shows the descriptor reads one, and its uniform
        // register takes the first descriptor field no run of the form holds
        int shown = 0;
        for (unsigned int number = 0u; (kind == SASS_OPERAND_ADDRESS) && (number < form->runs); number += 1u)
        {
            shown = shown || ((form->run[number].operand == place) && (form->run[number].first == SASS_DESCRIPTOR_SHOWN) &&
                              (form->run[number].last == SASS_DESCRIPTOR_SHOWN));
        }
        for (unsigned int at = 0u; shown && (places[place].has_descriptor == 0) && (at < SASS_DESCRIPTOR_FIELDS); at += 1u)
        {
            const SassField *const field = &s_descriptor_fields[at];
            int held = 0;
            for (unsigned int number = 0u; number < form->runs; number += 1u)
            {
                held = held || ((form->run[number].first < (field->first + field->bits)) &&
                                (form->run[number].last >= field->first));
            }
            if (held == 0)
            {
                places[place].descriptor = *field;
                places[place].has_descriptor = 1;
            }
        }
        // Where no field of the operand's kind begins inside a run of its own, the widest run the probe saw change the
        // operand is its field, from the run's first bit and as long as the run: BPT.TRAP's code begins at bit 34,
        // where no immediate field of the list above begins. An operand a field above places is placed there as it was
        const int counted = (kind == SASS_OPERAND_IMMEDIATE) || (kind == SASS_OPERAND_REGISTER) ||
                            (kind == SASS_OPERAND_PREDICATE) || (kind == SASS_OPERAND_UNIFORM);
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
    const unsigned long long mask = (1ull << bits) - 1ull;
    *high = (*high & ~(mask << (first - 64u))) | ((value & mask) << (first - 64u));
}

// the scheduler's bits set so that every instruction waits for every one before it: a stall of the soonest its
// operation's result is read, no reuse, and a wait on every barrier. A fixed result is back once that many cycles
// pass, and an operation with no measured count stalls the longest. An instruction whose result comes back late has
// to set a barrier for the wait to have anything to wait on, and a store has to set one for whatever writes its
// operands next: which instructions those are is the operation's schedule (sass_operation_schedule), and a barrier the
// form's own encoding sets is set too, except on an operation whose result is back in a measured count of cycles.
// Nothing releases a barrier such an operation sets, and the next instruction's wait on all six never ends. A barrier
// no instruction set is already at rest, and waiting on all six costs nothing where none was set
void sass_control_safe(const SassMachine *machine, const SassForm *form, unsigned long long *high)
{
    unsigned int soonest = SASS_STALL_LONGEST;
    const unsigned int schedule = sass_operation_schedule(machine, form->operation, &soonest);
    const int fixed = (schedule == SASS_SCHEDULE_FIXED) && (soonest != SASS_STALL_LONGEST);
    const int wrote = (schedule == SASS_SCHEDULE_LATE) ||
                      (!fixed && sass_barrier_set(form->high, SASS_WRITE_BARRIER_FIRST));
    const int read = (schedule == SASS_SCHEDULE_STORE) ||
                     (!fixed && sass_barrier_set(form->high, SASS_READ_BARRIER_FIRST));
    sass_high_write(high, SASS_STALL_FIRST, 4u, soonest);
    sass_high_write(high, SASS_YIELD_FIRST, 1u, 0ull);
    sass_high_write(high, SASS_WRITE_BARRIER_FIRST, 3u, wrote ? 0ull : SASS_BARRIER_NONE);
    sass_high_write(high, SASS_READ_BARRIER_FIRST, 3u, read ? 1ull : SASS_BARRIER_NONE);
    sass_high_write(high, SASS_WAIT_FIRST, 6u, SASS_WAIT_EVERY);
    sass_high_write(high, SASS_REUSE_FIRST, 4u, 0ull);
}
