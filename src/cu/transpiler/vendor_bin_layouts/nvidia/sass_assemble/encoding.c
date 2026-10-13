// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// encoding.c: the encoding read: bits read, signed values, the open bits, a form relocated and landed, a name within, a holder named, the places an encoding holds, an encoding's form, an operand read, the encoding read, and the loop walked
#include "sass_assemble_internal.h"

// one form that holds an encoding, where its operands sit, and how many bits it leaves to them
typedef struct
{
    const SassForm *form;
    SassPlace places[SASS_MACHINE_OPERANDS];
    unsigned int open;
} SassHolder;

// one form's operands placed and the bits it leaves open, as the encoding reader finds them
typedef struct
{
    int placed;
    SassPlace places[SASS_MACHINE_OPERANDS];
    unsigned int open;
    unsigned long long open_low;
    unsigned long long open_high;
} SassFormPlaced;

// `bits` bits of an encoding from bit `first`
static unsigned long long sass_bits_read(unsigned long long low, unsigned long long high, unsigned int first,
                                         unsigned int bits)
{
    unsigned long long value = 0ull;
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int at = first + bit;
        const unsigned long long word = (at < 64u) ? low : high;
        const unsigned int place = (at < 64u) ? at : (at - 64u);
        value |= ((word >> place) & 1ull) << bit;
    }
    return value;
}

// `value` read as a two's complement number `bits` wide
static long long sass_signed(unsigned long long value, unsigned int bits)
{
    if ((bits == 0u) || (bits >= 64u))
    {
        return (long long)value;
    }
    const unsigned long long top = 1ull << (bits - 1u);
    return (long long)((value ^ top) - top);
}

// the bits a form leaves to its operands, its guard and the scheduler, set in `low` and `high`, and how many
static unsigned int sass_open_bits(const SassForm *form, const SassPlace *places, unsigned long long *low,
                                   unsigned long long *high)
{
    *low = 0ull;
    *high = 0ull;
    sass_bits_write(low, high, SASS_GUARD_FIRST, SASS_GUARD_BITS + 1u, ~0ull);
    sass_bits_write(low, high, SASS_STALL_FIRST, 128u - SASS_STALL_FIRST, ~0ull);
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        if (places[place].by_text != 0)
        {
            continue;
        }
        sass_bits_write(low, high, places[place].value.first, places[place].value.bits, ~0ull);
        if (places[place].has_offset != 0)
        {
            sass_bits_write(low, high, places[place].offset.first, places[place].offset.bits, ~0ull);
        }
        if (places[place].has_descriptor != 0)
        {
            sass_bits_write(low, high, places[place].descriptor.first, places[place].descriptor.bits, ~0ull);
            sass_bits_write(low, high, SASS_DESCRIPTOR_SHOWN, 1u, ~0ull);
        }
    }
    // an operand the form holds and does not print is the operand's, whatever value an encoding gives it
    for (unsigned int number = 0u; number < form->runs; number += 1u)
    {
        const SassRun *const run = &form->run[number];
        if (run->operand >= form->operands)
        {
            sass_bits_write(low, high, run->first, (run->last - run->first) + 1u, ~0ull);
        }
    }
    unsigned int count = 0u;
    for (unsigned int bit = 0u; bit < 64u; bit += 1u)
    {
        count += (unsigned int)(((*low >> bit) & 1ull) + ((*high >> bit) & 1ull));
    }
    return count;
}

// 1 where an operand of the form is a label that names a symbol, whose field holds nothing to read
static int sass_form_relocated(const SassForm *form, const SassPlace *places)
{
    int relocated = 0;
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        relocated = relocated || ((form->kind[place] == SASS_OPERAND_LABEL) && (places[place].by_text != 0));
    }
    return relocated;
}

// 1 where `form` is `other`'s operation and reads as a label an operand `other` reads as a number: a branch's distance
// is read as the place it lands, which a number in the same bits does not say
static int sass_form_lands(const SassForm *form, const SassForm *other)
{
    if ((other == NULL) || (strcmp(form->operation, other->operation) != 0) || (form->operands != other->operands))
    {
        return 0;
    }
    int lands = 0;
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        lands = lands || ((form->kind[place] == SASS_OPERAND_LABEL) && (other->kind[place] == SASS_OPERAND_IMMEDIATE));
    }
    return lands;
}

// 1 where `named` is `plain` with modifiers added: the two share their name up to its first dot, and every modifier
// `plain` carries `named` carries too, as IMAD.IADD carries IMAD's and IMAD.MOV.U32 carries IMAD.U32's
static int sass_name_within(const char *plain, const char *named)
{
    const size_t length = strcspn(plain, ".");
    int within = (strcspn(named, ".") == length) && (strncmp(plain, named, length) == 0) &&
                 (strlen(plain) < strlen(named));
    for (const char *walk = plain + length; within && (*walk == '.'); walk += 1u + strcspn(walk + 1, "."))
    {
        const size_t modifier = 1u + strcspn(walk + 1, ".");
        int carried = 0;
        for (const char *seek = named + length; !carried && (*seek == '.'); seek += 1u + strcspn(seek + 1, "."))
        {
            carried = ((1u + strcspn(seek + 1, ".")) == modifier) && (strncmp(seek, walk, modifier) == 0);
        }
        within = carried;
    }
    return within;
}

// How `holder` stands against `sibling` where `holder` takes its name from one value of an operand: a sibling holding
// the same encoding, with the same operation's name before its first dot, the same kinds and the same marks, is named
// shorter, and the two forms' own values of an operand differ. -1 where the encoding's value there is not `holder`'s,
// which names it away; 1 where every such value is `holder`'s, which names it; 0 where the two are no such pair.
// IMAD.MOV and IMAD.IADD are IMAD with a multiplier of 0 and of 1, and IMAD.MOV is IMAD with RZ as the register it
// multiplies by: an encoding with any other multiplier is IMAD's
static int sass_holder_named(const SassHolder *holder, const SassHolder *sibling, unsigned long long low,
                             unsigned long long high)
{
    const SassForm *const form = holder->form;
    const SassForm *const other = sibling->form;
    int same = (form != other) && sass_name_within(other->operation, form->operation) &&
               (form->operands == other->operands);
    for (unsigned int place = 0u; same && (place < form->operands); place += 1u)
    {
        same = (form->kind[place] == other->kind[place]) && (form->mark[place] == other->mark[place]);
    }
    int differs = 0;
    int away = 0;
    for (unsigned int place = 0u; same && (place < form->operands); place += 1u)
    {
        const SassField *const field = &holder->places[place].value;
        const SassField *const field_other = &sibling->places[place].value;
        if ((holder->places[place].by_text != 0) || (field->bits == 0u) || (field_other->bits == 0u))
        {
            continue;
        }
        const unsigned long long own = sass_bits_read(form->low, form->high, field->first, field->bits);
        const unsigned long long theirs = sass_bits_read(other->low, other->high, field_other->first,
                                                         field_other->bits);
        const unsigned long long given = sass_bits_read(low, high, field->first, field->bits);
        // the value a name is taken from is a multiplier of 0 or 1, a number 0 or 1 or the register RZ; a register
        // the two forms' encodings were seen with apart from that is the encoding's own and names nothing
        const int naming = ((form->kind[place] == SASS_OPERAND_IMMEDIATE) && (own <= 1ull)) ||
                           ((form->kind[place] == SASS_OPERAND_REGISTER) && (own == 255ull));
        differs = differs || (naming && (own != theirs));
        away = away || (naming && (own != theirs) && (given != own));
    }
    return away ? -1 : (differs ? 1 : 0);
}

int sass_encoding_places_hold(SassMachine *machine)
{
    SassFormPlaced *const held = malloc((size_t)machine->forms * sizeof(SassFormPlaced) + 1u);
    if (held == NULL)
    {
        return 0;
    }
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        unsigned int unplaced = 0u;
        held[number].placed = sass_places_find(&machine->form[number], held[number].places, &unplaced);
        held[number].open = held[number].placed ? sass_open_bits(&machine->form[number], held[number].places,
                                                                 &held[number].open_low, &held[number].open_high)
                                                : 0u;
    }
    machine->places_held = held;
    return 1;
}

static const SassForm *sass_encoding_form(const SassMachine *machine, unsigned long long low, unsigned long long high,
                                          SassPlace *places)
{
    SassHolder holders[SASS_HOLDERS];
    unsigned int held = 0u;
    const SassFormPlaced *const placed = (const SassFormPlaced *)machine->places_held;
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        SassHolder holder;
        holder.form = &machine->form[number];
        unsigned long long open_low = 0ull;
        unsigned long long open_high = 0ull;
        if (placed != NULL)
        {
            if (!placed[number].placed || (((low ^ holder.form->low) & ~placed[number].open_low) != 0ull) ||
                (((high ^ holder.form->high) & ~placed[number].open_high) != 0ull))
            {
                continue;
            }
            memcpy(holder.places, placed[number].places, sizeof(holder.places));
            holder.open = placed[number].open;
        }
        else
        {
            unsigned int unplaced = 0u;
            if (!sass_places_find(holder.form, holder.places, &unplaced))
            {
                continue;
            }
            holder.open = sass_open_bits(holder.form, holder.places, &open_low, &open_high);
            if ((((low ^ holder.form->low) & ~open_low) != 0ull) || (((high ^ holder.form->high) & ~open_high) != 0ull))
            {
                continue;
            }
        }
        // a holder past the most the reader weighs is refused and not dropped: the form it would have been is unknown
        if (held == SASS_HOLDERS)
        {
            printf("  sass_assemble: more than %u forms hold the encoding %016llx %016llx\n", SASS_HOLDERS, high, low);
            return NULL;
        }
        holders[held] = holder;
        held += 1u;
    }
    const SassHolder *found = NULL;
    int found_relocated = 1;
    for (unsigned int number = 0u; number < held; number += 1u)
    {
        const SassHolder *const holder = &holders[number];
        int away = 0;
        for (unsigned int other = 0u; (away == 0) && (other < held); other += 1u)
        {
            away = (sass_holder_named(holder, &holders[other], low, high) < 0);
        }
        if (away != 0)
        {
            continue;
        }
        const SassForm *const form = holder->form;
        const SassForm *const was = (found != NULL) ? found->form : NULL;
        const int relocated = sass_form_relocated(form, holder->places);
        // a form a value names takes the encoding from the sibling it is named past, and keeps it from that sibling
        const int named = (found != NULL) && (sass_holder_named(holder, found, low, high) > 0);
        const int kept = (found != NULL) && (sass_holder_named(found, holder, low, high) > 0);
        const int fewer = (found == NULL) || named || (!kept && (holder->open < found->open));
        const int better = (found == NULL) || (found_relocated && !relocated) ||
                           ((relocated == found_relocated) &&
                            (sass_form_lands(form, was) || (!sass_form_lands(was, form) && fewer)));
        if (better)
        {
            found = holder;
            found_relocated = relocated;
        }
    }
    if (found != NULL)
    {
        memcpy(places, found->places, sizeof(found->places));
    }
    return (found != NULL) ? found->form : NULL;
}

// the text of operand `at`, read from where the assembler writes it
static void sass_operand_read(const SassForm *form, const SassInstructionParts *base, const SassPlace *place,
                              unsigned int at, unsigned long long low, unsigned long long high,
                              unsigned long long address, char *operand, size_t room)
{
    static const char *const s_marks[] = {"", "-", "~", "!"};
    const char *const mark = (form->mark[at] < (sizeof(s_marks) / sizeof(s_marks[0]))) ? s_marks[form->mark[at]] : "";
    if (place->by_text != 0)
    {
        snprintf(operand, room, "%s%s", mark, base->operand[at]);
        return;
    }
    const unsigned long long held = sass_bits_read(low, high, place->value.first, place->value.bits);
    const unsigned long long mask = (place->value.bits < 64u) ? ((1ull << place->value.bits) - 1ull) : ~0ull;
    const unsigned long long value = ((place->value.inverted != 0u) ? (~held & mask) : held) * place->value.scale;
    switch (form->kind[at])
    {
    case SASS_OPERAND_REGISTER:
        (value == 255ull) ? snprintf(operand, room, "%sRZ", mark) : snprintf(operand, room, "%sR%llu", mark, value);
        return;
    case SASS_OPERAND_UNIFORM:
        (value == 63ull) ? snprintf(operand, room, "%sURZ", mark) : snprintf(operand, room, "%sUR%llu", mark, value);
        return;
    case SASS_OPERAND_PREDICATE:
        (value == 7ull) ? snprintf(operand, room, "%sPT", mark) : snprintf(operand, room, "%sP%llu", mark, value);
        return;
    case SASS_OPERAND_IMMEDIATE:
        if (place->value.first == SASS_BRANCH_FIRST)
        {
            // a distance, signed over its field
            const long long distance =
                sass_signed(value / place->value.scale, place->value.bits) * (long long)place->value.scale;
            snprintf(operand, room, "%s%s0x%llx", mark, (distance < 0) ? "-" : "",
                     (unsigned long long)((distance < 0) ? -distance : distance));
            return;
        }
        snprintf(operand, room, "%s0x%llx", mark, value);
        return;
    case SASS_OPERAND_LABEL:
        // a branch counts its target from the instruction after it, and the field holds a two's complement distance
        snprintf(operand, room, "`(0x%llx)",
                 address + 16ull +
                     (unsigned long long)(sass_signed(value / place->value.scale, place->value.bits) *
                                          (long long)place->value.scale));
        return;
    case SASS_OPERAND_CONSTANT:
    {
        unsigned long long bank = 0ull;
        sass_constant_value(base->operand[at], &bank);
        snprintf(operand, room, "%sc[0x%llx][0x%llx]", mark, bank, value);
        return;
    }
    case SASS_OPERAND_ADDRESS:
    {
        const unsigned long long offset =
            (place->has_offset != 0) ? sass_bits_read(low, high, place->offset.first, place->offset.bits) : 0ull;
        const char *const wide = (strstr(base->operand[at], ".64") != NULL) ? ".64" : "";
        char named[16];
        (value == 255ull) ? snprintf(named, sizeof(named), "RZ%s", wide)
                          : snprintf(named, sizeof(named), "R%llu%s", value, wide);
        // a memory operand that reads a descriptor names its uniform register, desc[] where bit 101 shows it and term[]
        // where it does not
        char held[24] = "";
        if (place->has_descriptor != 0)
        {
            const unsigned long long descriptor =
                sass_bits_read(low, high, place->descriptor.first, place->descriptor.bits);
            const int shown = (int)sass_bits_read(low, high, SASS_DESCRIPTOR_SHOWN, 1u);
            (descriptor == SASS_DESCRIPTOR_ZERO) ? snprintf(held, sizeof(held), "%s[URZ]", shown ? "desc" : "term")
                                                 : snprintf(held, sizeof(held), "%s[UR%llu]", shown ? "desc" : "term",
                                                            descriptor);
        }
        (offset == 0ull) ? snprintf(operand, room, "%s[%s]", held, named)
                         : snprintf(operand, room, "%s[%s+0x%llx]", held, named, offset);
        return;
    }
    default:
        snprintf(operand, room, "%s%s", mark, base->operand[at]);
        return;
    }
}

int sass_encoding_read(const SassMachine *machine, unsigned long long low, unsigned long long high,
                       unsigned long long address, char *text, size_t room)
{
    SassPlace places[SASS_MACHINE_OPERANDS];
    const SassForm *const form = sass_encoding_form(machine, low, high, places);
    if (form == NULL)
    {
        snprintf(text, room, "no form");
        return 0;
    }
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    const unsigned long long guard = sass_bits_read(low, high, SASS_GUARD_FIRST, SASS_GUARD_BITS);
    const unsigned long long negated = sass_bits_read(low, high, SASS_GUARD_NOT, 1u);
    size_t at = 0u;
    if ((guard != 7ull) || (negated != 0ull))
    {
        char predicate[16];
        (guard == 7ull) ? snprintf(predicate, sizeof(predicate), "PT")
                        : snprintf(predicate, sizeof(predicate), "P%llu", guard);
        at += (size_t)snprintf(text + at, room - at, "@%s%s ", (negated != 0ull) ? "!" : "", predicate);
    }
    at += (size_t)snprintf(text + at, (at < room) ? (room - at) : 0u, "%s", form->operation);
    for (unsigned int place = 0u; (place < form->operands) && (at < room); place += 1u)
    {
        char operand[SASS_MACHINE_TOKEN];
        sass_operand_read(form, &base, &places[place], place, low, high, address, operand, sizeof(operand));
        at += (size_t)snprintf(text + at, room - at, "%s%s", (place == 0u) ? " " : ", ", operand);
    }
    return 1;
}

int sass_loop_walk(const SassMachine *machine, const SassLoopWalk *walk, unsigned long long low, unsigned long long high,
                   unsigned int *step)
{
    unsigned int stopped = 0u;
    int through = 0;
    SassPlace places[SASS_MACHINE_OPERANDS];
    const SassForm *const form = sass_encoding_form(machine, low, high, places);
    if (form != NULL)
    {
        stopped = 1u;
        const unsigned long long guard = sass_bits_read(low, high, SASS_GUARD_FIRST, SASS_GUARD_BITS);
        const unsigned long long negated = sass_bits_read(low, high, SASS_GUARD_NOT, 1u);
        if ((guard == walk->flag) && (negated == 0ull))
        {
            stopped = 2u;
            int lands = 0;
            for (unsigned int place = 0u; place < form->operands; place += 1u)
            {
                const unsigned int kind = form->kind[place];
                if ((places[place].by_text != 0) || ((kind != SASS_OPERAND_LABEL) && (kind != SASS_OPERAND_IMMEDIATE)))
                {
                    continue;
                }
                const unsigned long long value =
                    sass_bits_read(low, high, places[place].value.first, places[place].value.bits) *
                    places[place].value.scale;
                const long long distance = sass_signed(value / places[place].value.scale, places[place].value.bits) *
                                           (long long)places[place].value.scale;
                const unsigned long long landing = walk->address + 16ull + (unsigned long long)distance;
                lands = lands || (landing == walk->target);
            }
            if (lands)
            {
                stopped = 3u;
                int writes = 0;
                if ((form->operands > 0u) && (places[0].by_text == 0))
                {
                    const unsigned long long first =
                        sass_bits_read(low, high, places[0].value.first, places[0].value.bits);
                    for (unsigned int kept = 0u; (form->kind[0] == SASS_OPERAND_REGISTER) && (kept < walk->lives);
                         kept += 1u)
                    {
                        writes = writes || (first == walk->live[kept]);
                    }
                    writes = writes || ((form->kind[0] == SASS_OPERAND_PREDICATE) && (first == walk->flag));
                }
                if (!writes)
                {
                    stopped = 4u;
                    through = 1;
                }
            }
        }
    }
    if (step != NULL)
    {
        *step = stopped;
    }
    return through;
}
