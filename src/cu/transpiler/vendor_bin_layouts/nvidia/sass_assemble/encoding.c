// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// encoding.c: the encoding read: bits read, signed values, the open bits, a form relocated and landed, a name within, a
// holder named, the places an encoding matches, an encoding's form, an operand read, the encoding read, and the loop
// walked
#include "../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "sass_assemble_internal.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int encoding_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// one form that matches an encoding, where its operands sit, and how many bits it leaves to them
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
    unsigned long long value = 0ULL;
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int at = first + bit;
        const unsigned long long word = (at < 64u) ? low : high;
        const unsigned int place = (at < 64u) ? at : (at - 64u);
        value |= ((word >> place) & 1ULL) << bit;
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
    const unsigned long long top = 1ULL << (bits - 1u);
    return (long long)((value ^ top) - top);
}

// the bits a form leaves to its operands, its guard and the scheduler, set in `low` and `high`, and how many
static unsigned int sass_open_bits(const SassForm *form, const SassPlace *places, unsigned long long *low,
                                   unsigned long long *high)
{
    *low = 0ULL;
    *high = 0ULL;
    sass_bits_write(low, high, SASS_GUARD_FIRST, SASS_GUARD_BITS + 1u, ~0ULL);
    sass_bits_write(low, high, SASS_STALL_FIRST, 128u - SASS_STALL_FIRST, ~0ULL);
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        if (places[place].by_text != 0)
        {
            continue;
        }
        sass_bits_write(low, high, places[place].value.first, places[place].value.bits, ~0ULL);
        if (places[place].has_offset != 0)
        {
            sass_bits_write(low, high, places[place].offset.first, places[place].offset.bits, ~0ULL);
        }
        if (places[place].has_descriptor != 0)
        {
            sass_bits_write(low, high, places[place].descriptor.first, places[place].descriptor.bits, ~0ULL);
            sass_bits_write(low, high, SASS_DESCRIPTOR_DISPLAYED, 1u, ~0ULL);
        }
    }
    // an operand the form carries and does not print is the operand's, whatever value an encoding gives it
    for (unsigned int number = 0u; number < form->runs; number += 1u)
    {
        const SassRun *const run = &form->run[number];
        if (run->operand >= form->operands)
        {
            sass_bits_write(low, high, run->first, (run->last - run->first) + 1u, ~0ULL);
        }
    }
    unsigned int count = 0u;
    for (unsigned int bit = 0u; bit < 64u; bit += 1u)
    {
        count += (unsigned int)(((*low >> bit) & 1ULL) + ((*high >> bit) & 1ULL));
    }
    return count;
}

// 1 where an operand of the form is a label that names a symbol, whose field contains nothing to read
static int sass_form_relocated(const SassForm *form, const SassPlace *places)
{
    int relocated = 0;
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        relocated = relocated || ((form->type[place] == SASS_OPERAND_LABEL) && (places[place].by_text != 0));
    }
    return relocated;
}

// 1 where `form` is the operation of `other`, and reads as a label an operand that `other` reads as a number. The
// distance of a branch reads as the place where it lands, which a number in the same bits does not tell
static int sass_form_lands(const SassForm *form, const SassForm *other)
{
    if ((other == NULL) ||
        (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = form->operation, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                     .other = other->operation, .ci = EMBED_FALSE)) ||
        (form->operands != other->operands))
    {
        return 0;
    }
    int lands = 0;
    for (unsigned int place = 0u; place < form->operands; place += 1u)
    {
        lands = lands || ((form->type[place] == SASS_OPERAND_LABEL) && (other->type[place] == SASS_OPERAND_IMMEDIATE));
    }
    return lands;
}

// 1 where `named` is `plain` with modifiers added. The two share their name up to its first dot, and `named` carries
// every modifier that `plain` carries. IMAD.IADD carries those of IMAD, and IMAD.MOV.U32 those of IMAD.U32
static int sass_name_within(const char *plain, const char *named)
{
    const size_t length =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = plain, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".");
    int within = (EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = named, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = ".") == length) &&
                 (encoding_text_order(plain, named, (size_t)(length)) == 0) &&
                 (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = plain, .cap = PLATFORM_TEXT_TO_TERMINATOR) <
                  EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = named, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    for (const char *walk = plain + length; within && (*walk == '.');
         walk += 1u + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = walk + 1, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                 .other = "."))
    {
        const size_t modifier = 1u + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = walk + 1,
                                                .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".");
        int carried = 0;
        for (const char *seek = named + length; !carried && (*seek == '.');
             seek += 1u + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = seek + 1,
                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "."))
        {
            carried = ((1u + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = seek + 1,
                                        .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".")) == modifier) &&
                      (encoding_text_order(seek, walk, (size_t)(modifier)) == 0);
        }
        within = carried;
    }
    return within;
}

// How `holder` is against `sibling`, where `holder` takes its name from one value of an operand. A sibling that carries
// the same encoding has a shorter name. It has the same operation name before its first dot, the same types and the
// same marks. The own values of an operand of the two forms differ.
//
// The return is -1 where the value of the encoding there is not that of `holder`, which names it away. It is 1 where
// every such value is that of `holder`, which names it, and 0 where the two are no such pair. IMAD.MOV and IMAD.IADD
// are IMAD with a multiplier of 0 and of 1. IMAD.MOV is IMAD with RZ as the register that it multiplies by. An
// encoding with any other multiplier belongs to IMAD
static int sass_holder_named(const SassHolder *holder, const SassHolder *sibling, unsigned long long low,
                             unsigned long long high)
{
    const SassForm *const form = holder->form;
    const SassForm *const other = sibling->form;
    int same =
        (form != other) && sass_name_within(other->operation, form->operation) && (form->operands == other->operands);
    for (unsigned int place = 0u; same && (place < form->operands); place += 1u)
    {
        same = (form->type[place] == other->type[place]) && (form->mark[place] == other->mark[place]);
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
        const unsigned long long theirs =
            sass_bits_read(other->low, other->high, field_other->first, field_other->bits);
        const unsigned long long given = sass_bits_read(low, high, field->first, field->bits);
        // the value that a name comes from is a multiplier of 0 or 1, a number 0 or 1, or the register RZ. A register
        // that the probe saw the encodings of the two forms with, apart from that, belongs to the encoding and names
        // nothing
        const int naming = ((form->type[place] == SASS_OPERAND_IMMEDIATE) && (own <= 1ULL)) ||
                           ((form->type[place] == SASS_OPERAND_REGISTER) && (own == 255ULL));
        differs = differs || (naming && (own != theirs));
        away = away || (naming && (own != theirs) && (given != own));
    }
    return away ? -1 : (differs ? 1 : 0);
}

int sass_encoding_places_built(SassMachine *machine)
{
    SassFormPlaced *const placed_all =
        platform_cell_taken((size_t)machine->forms * sizeof(SassFormPlaced) + 1u, platform_place_g_program_owner, 0);
    if (placed_all == NULL)
    {
        return 0;
    }
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        unsigned int unplaced = 0u;
        placed_all[number].placed = sass_places_find(&machine->form[number], placed_all[number].places, &unplaced);
        placed_all[number].open = placed_all[number].placed
                                      ? sass_open_bits(&machine->form[number], placed_all[number].places,
                                                       &placed_all[number].open_low, &placed_all[number].open_high)
                                      : 0u;
    }
    machine->places_built = placed_all;
    return 1;
}

static const SassForm *sass_encoding_form(const SassMachine *machine, unsigned long long low, unsigned long long high,
                                          SassPlace *places)
{
    SassHolder holders[SASS_HOLDERS];
    unsigned int found_count = 0u;
    const SassFormPlaced *const placed = (const SassFormPlaced *)machine->places_built;
    for (unsigned int number = 0u; number < machine->forms; number += 1u)
    {
        SassHolder holder;
        holder.form = &machine->form[number];
        unsigned long long open_low = 0ULL;
        unsigned long long open_high = 0ULL;
        if (placed != NULL)
        {
            if (!placed[number].placed || (((low ^ holder.form->low) & ~placed[number].open_low) != 0ULL) ||
                (((high ^ holder.form->high) & ~placed[number].open_high) != 0ULL))
            {
                continue;
            }
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = holder.places, .src = placed[number].places,
                       .bytes = sizeof(holder.places));
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
            if ((((low ^ holder.form->low) & ~open_low) != 0ULL) || (((high ^ holder.form->high) & ~open_high) != 0ULL))
            {
                continue;
            }
        }
        // the reader refuses, and does not drop, a holder past the max that it weighs, because the form that it would
        // be is unknown
        if (found_count == SASS_HOLDERS)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  sass_assemble: more than ");
                mmgr_line_decimal(output_line, SASS_HOLDERS, 1u);
                mmgr_line_put(output_line, " forms match the encoding ");
                mmgr_line_hex(output_line, high, 16u);
                mmgr_line_put(output_line, " ");
                mmgr_line_hex(output_line, low, 16u);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            return NULL;
        }
        holders[found_count] = holder;
        found_count += 1u;
    }
    const SassHolder *found = NULL;
    int found_relocated = 1;
    for (unsigned int number = 0u; number < found_count; number += 1u)
    {
        const SassHolder *const holder = &holders[number];
        int away = 0;
        for (unsigned int other = 0u; (away == 0) && (other < found_count); other += 1u)
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
        // a form that a value names takes the encoding from the sibling that its name passes, and keeps it from that
        // sibling
        const int named = (found != NULL) && (sass_holder_named(holder, found, low, high) > 0);
        const int holder_named = (found != NULL) && (sass_holder_named(found, holder, low, high) > 0);
        const int fewer = (found == NULL) || named || (!holder_named && (holder->open < found->open));
        const int better =
            (found == NULL) || (found_relocated && !relocated) ||
            ((relocated == found_relocated) && (sass_form_lands(form, was) || (!sass_form_lands(was, form) && fewer)));
        if (better)
        {
            found = holder;
            found_relocated = relocated;
        }
    }
    if (found != NULL)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = places, .src = found->places, .bytes = sizeof(found->places));
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
        {
            VerbaLinea buffer_line = mmgr_line_over(operand, room);
            mmgr_line_put(&buffer_line, mark);
            mmgr_line_put(&buffer_line, base->operand[at]);
            (void)mmgr_line_finish(&buffer_line);
        }
        return;
    }
    const unsigned long long operand_value = sass_bits_read(low, high, place->value.first, place->value.bits);
    const unsigned long long mask = (place->value.bits < 64u) ? ((1ULL << place->value.bits) - 1ULL) : ~0ULL;
    const unsigned long long value =
        ((place->value.inverted != 0u) ? (~operand_value & mask) : operand_value) * place->value.scale;
    switch (form->type[at])
    {
    case SASS_OPERAND_REGISTER: {
        VerbaLinea formed_line_17;
        VerbaLinea formed_line_18;
        (value == 255ULL)
            ? (formed_line_17 = mmgr_line_over(operand, room), mmgr_line_put(&formed_line_17, mark),
               mmgr_line_put(&formed_line_17, "RZ"), (void)mmgr_line_finish(&formed_line_17), (int)formed_line_17.at)
            : (formed_line_18 = mmgr_line_over(operand, room), mmgr_line_put(&formed_line_18, mark),
               mmgr_line_put(&formed_line_18, "R"), mmgr_line_decimal(&formed_line_18, value, 1u),
               (void)mmgr_line_finish(&formed_line_18), (int)formed_line_18.at);
    }
        return;
    case SASS_OPERAND_UNIFORM: {
        VerbaLinea formed_line_19;
        VerbaLinea formed_line_20;
        (value == 63ULL)
            ? (formed_line_19 = mmgr_line_over(operand, room), mmgr_line_put(&formed_line_19, mark),
               mmgr_line_put(&formed_line_19, "URZ"), (void)mmgr_line_finish(&formed_line_19), (int)formed_line_19.at)
            : (formed_line_20 = mmgr_line_over(operand, room), mmgr_line_put(&formed_line_20, mark),
               mmgr_line_put(&formed_line_20, "UR"), mmgr_line_decimal(&formed_line_20, value, 1u),
               (void)mmgr_line_finish(&formed_line_20), (int)formed_line_20.at);
    }
        return;
    case SASS_OPERAND_PREDICATE: {
        VerbaLinea formed_line_21;
        VerbaLinea formed_line_22;
        (value == 7ULL)
            ? (formed_line_21 = mmgr_line_over(operand, room), mmgr_line_put(&formed_line_21, mark),
               mmgr_line_put(&formed_line_21, "PT"), (void)mmgr_line_finish(&formed_line_21), (int)formed_line_21.at)
            : (formed_line_22 = mmgr_line_over(operand, room), mmgr_line_put(&formed_line_22, mark),
               mmgr_line_put(&formed_line_22, "P"), mmgr_line_decimal(&formed_line_22, value, 1u),
               (void)mmgr_line_finish(&formed_line_22), (int)formed_line_22.at);
    }
        return;
    case SASS_OPERAND_IMMEDIATE:
        if (place->value.first == SASS_BRANCH_FIRST)
        {
            // a distance, signed over its field
            const long long distance =
                sass_signed(value / place->value.scale, place->value.bits) * (long long)place->value.scale;
            {
                VerbaLinea buffer_line = mmgr_line_over(operand, room);
                mmgr_line_put(&buffer_line, mark);
                mmgr_line_put(&buffer_line, (distance < 0) ? "-" : "");
                mmgr_line_put(&buffer_line, "0x");
                mmgr_line_hex(&buffer_line, (unsigned long long)((distance < 0) ? -distance : distance), 1u);
                (void)mmgr_line_finish(&buffer_line);
            }
            return;
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(operand, room);
            mmgr_line_put(&buffer_line, mark);
            mmgr_line_put(&buffer_line, "0x");
            mmgr_line_hex(&buffer_line, value, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        return;
    case SASS_OPERAND_LABEL:
        // a branch counts its target from the instruction after it, and the field stores a two's complement distance
        {
            VerbaLinea buffer_line = mmgr_line_over(operand, room);
            mmgr_line_put(&buffer_line, "`(0x");
            mmgr_line_hex(&buffer_line,
                          address + 16ULL +
                              (unsigned long long)(sass_signed(value / place->value.scale, place->value.bits) *
                                                   (long long)place->value.scale),
                          1u);
            mmgr_line_put(&buffer_line, ")");
            (void)mmgr_line_finish(&buffer_line);
        }
        return;
    case SASS_OPERAND_CONSTANT: {
        unsigned long long bank = 0ULL;
        sass_constant_value(base->operand[at], &bank);
        {
            VerbaLinea buffer_line = mmgr_line_over(operand, room);
            mmgr_line_put(&buffer_line, mark);
            mmgr_line_put(&buffer_line, "c[0x");
            mmgr_line_hex(&buffer_line, bank, 1u);
            mmgr_line_put(&buffer_line, "][0x");
            mmgr_line_hex(&buffer_line, value, 1u);
            mmgr_line_put(&buffer_line, "]");
            (void)mmgr_line_finish(&buffer_line);
        }
        return;
    }
    case SASS_OPERAND_ADDRESS: {
        const unsigned long long offset =
            (place->has_offset != 0) ? sass_bits_read(low, high, place->offset.first, place->offset.bits) : 0ULL;
        const char *const wide =
            (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = base->operand[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = ".64", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL)
                ? ".64"
                : "";
        char named[16];
        VerbaLinea formed_line_23;
        VerbaLinea formed_line_24;
        (value == 255ULL)
            ? (formed_line_23 = mmgr_line_over(named, sizeof(named)), mmgr_line_put(&formed_line_23, "RZ"),
               mmgr_line_put(&formed_line_23, wide), (void)mmgr_line_finish(&formed_line_23), (int)formed_line_23.at)
            : (formed_line_24 = mmgr_line_over(named, sizeof(named)), mmgr_line_put(&formed_line_24, "R"),
               mmgr_line_decimal(&formed_line_24, value, 1u), mmgr_line_put(&formed_line_24, wide),
               (void)mmgr_line_finish(&formed_line_24), (int)formed_line_24.at);
        // a memory operand that reads a descriptor names its uniform register, desc[] where bit 101 shows it and term[]
        // where it does not
        char operand_value[24] = "";
        if (place->has_descriptor != 0)
        {
            const unsigned long long descriptor =
                sass_bits_read(low, high, place->descriptor.first, place->descriptor.bits);
            const int displayed = (int)sass_bits_read(low, high, SASS_DESCRIPTOR_DISPLAYED, 1u);
            VerbaLinea formed_line_25;
            VerbaLinea formed_line_26;
            (descriptor == SASS_DESCRIPTOR_ZERO)
                ? (formed_line_25 = mmgr_line_over(operand_value, sizeof(operand_value)),
                   mmgr_line_put(&formed_line_25, displayed ? "desc" : "term"), mmgr_line_put(&formed_line_25, "[URZ]"),
                   (void)mmgr_line_finish(&formed_line_25), (int)formed_line_25.at)
                : (formed_line_26 = mmgr_line_over(operand_value, sizeof(operand_value)),
                   mmgr_line_put(&formed_line_26, displayed ? "desc" : "term"), mmgr_line_put(&formed_line_26, "[UR"),
                   mmgr_line_decimal(&formed_line_26, descriptor, 1u), mmgr_line_put(&formed_line_26, "]"),
                   (void)mmgr_line_finish(&formed_line_26), (int)formed_line_26.at);
        }
        VerbaLinea formed_line_27;
        VerbaLinea formed_line_28;
        (offset == 0ULL)
            ? (formed_line_27 = mmgr_line_over(operand, room), mmgr_line_put(&formed_line_27, operand_value),
               mmgr_line_put(&formed_line_27, "["), mmgr_line_put(&formed_line_27, named),
               mmgr_line_put(&formed_line_27, "]"), (void)mmgr_line_finish(&formed_line_27), (int)formed_line_27.at)
            : (formed_line_28 = mmgr_line_over(operand, room), mmgr_line_put(&formed_line_28, operand_value),
               mmgr_line_put(&formed_line_28, "["), mmgr_line_put(&formed_line_28, named),
               mmgr_line_put(&formed_line_28, "+0x"), mmgr_line_hex(&formed_line_28, offset, 1u),
               mmgr_line_put(&formed_line_28, "]"), (void)mmgr_line_finish(&formed_line_28), (int)formed_line_28.at);
        return;
    }
    default: {
        VerbaLinea formed_line_29 = mmgr_line_over(operand, room);
        mmgr_line_put(&formed_line_29, mark);
        mmgr_line_put(&formed_line_29, base->operand[at]);
        (void)mmgr_line_finish(&formed_line_29);
        (void)formed_line_29.at;
    }
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
        {
            VerbaLinea buffer_line = mmgr_line_over(text, room);
            mmgr_line_put(&buffer_line, "no form");
            (void)mmgr_line_finish(&buffer_line);
        }
        return 0;
    }
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    const unsigned long long guard = sass_bits_read(low, high, SASS_GUARD_FIRST, SASS_GUARD_BITS);
    const unsigned long long negated = sass_bits_read(low, high, SASS_GUARD_NOT, 1u);
    size_t at = 0u;
    if ((guard != 7ULL) || (negated != 0ULL))
    {
        char predicate[16];
        VerbaLinea formed_line_30;
        VerbaLinea formed_line_31;
        (guard == 7ULL)
            ? (formed_line_30 = mmgr_line_over(predicate, sizeof(predicate)), mmgr_line_put(&formed_line_30, "PT"),
               (void)mmgr_line_finish(&formed_line_30), (int)formed_line_30.at)
            : (formed_line_31 = mmgr_line_over(predicate, sizeof(predicate)), mmgr_line_put(&formed_line_31, "P"),
               mmgr_line_decimal(&formed_line_31, guard, 1u), (void)mmgr_line_finish(&formed_line_31),
               (int)formed_line_31.at);
        VerbaLinea formed_line_1 = mmgr_line_over(text + at, room - at);
        mmgr_line_put(&formed_line_1, "@");
        mmgr_line_put(&formed_line_1, (negated != 0ULL) ? "!" : "");
        mmgr_line_put(&formed_line_1, predicate);
        mmgr_line_put(&formed_line_1, " ");
        (void)mmgr_line_finish(&formed_line_1);
        at += (size_t)(int)formed_line_1.at;
    }
    VerbaLinea formed_line_2 = mmgr_line_over(text + at, (at < room) ? (room - at) : 0u);
    mmgr_line_put(&formed_line_2, form->operation);
    (void)mmgr_line_finish(&formed_line_2);
    at += (size_t)(int)formed_line_2.at;
    for (unsigned int place = 0u; (place < form->operands) && (at < room); place += 1u)
    {
        char operand[SASS_MACHINE_TOKEN];
        sass_operand_read(form, &base, &places[place], place, low, high, address, operand, sizeof(operand));
        VerbaLinea formed_line_3 = mmgr_line_over(text + at, room - at);
        mmgr_line_put(&formed_line_3, (place == 0u) ? " " : ", ");
        mmgr_line_put(&formed_line_3, operand);
        (void)mmgr_line_finish(&formed_line_3);
        at += (size_t)(int)formed_line_3.at;
    }
    return 1;
}

int sass_loop_walk(const SassMachine *machine, const SassLoopWalk *walk, unsigned long long low,
                   unsigned long long high, unsigned int *step)
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
        if ((guard == walk->flag) && (negated == 0ULL))
        {
            stopped = 2u;
            int lands = 0;
            for (unsigned int place = 0u; place < form->operands; place += 1u)
            {
                const unsigned int type = form->type[place];
                if ((places[place].by_text != 0) || ((type != SASS_OPERAND_LABEL) && (type != SASS_OPERAND_IMMEDIATE)))
                {
                    continue;
                }
                const unsigned long long value =
                    sass_bits_read(low, high, places[place].value.first, places[place].value.bits) *
                    places[place].value.scale;
                const long long distance = sass_signed(value / places[place].value.scale, places[place].value.bits) *
                                           (long long)places[place].value.scale;
                const unsigned long long landing = walk->address + 16ULL + (unsigned long long)distance;
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
                    for (unsigned int live = 0u; (form->type[0] == SASS_OPERAND_REGISTER) && (live < walk->lives);
                         live += 1u)
                    {
                        writes = writes || (first == walk->live[live]);
                    }
                    writes = writes || ((form->type[0] == SASS_OPERAND_PREDICATE) && (first == walk->flag));
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
