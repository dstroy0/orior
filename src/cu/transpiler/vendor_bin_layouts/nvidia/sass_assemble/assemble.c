// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// assemble.c: the assembly: one instruction assembled from its text, a line taken, a label and an instruction
// distinguished, a label's address, and the lines of a listing assembled
#include "sass_assemble_internal.h"
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

typedef struct
{
    unsigned int count;
    char name[SASS_LABELS][SASS_LABEL_TOKEN];
    unsigned long long address[SASS_LABELS];
} SassLabels;

int sass_assemble(const SassMachine *machine, const char *text, unsigned long long address, unsigned long long target,
                  unsigned int control, unsigned long long *low, unsigned long long *high)
{
    SassInstructionParts parts;
    sass_instruction_read(text, &parts);
    const SassForm *const form = sass_machine_form(machine, &parts);
    if (form == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  sass_assemble: no form for ");
            mmgr_line_put(output_line, text);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    // a register past the last the part answers a question's code can name is refused, the high half of a pair
    // counted as its own register; RZ is no register a thread uses, and is named past every one
    for (unsigned int place = 0u; place < parts.operands; place += 1u)
    {
        const char *const operand = parts.operand[place];
        const char *const open = mmgr_text_char_last(operand, '[', PLATFORM_TEXT_TO_TERMINATOR);
        const char *const named = (parts.type[place] == SASS_OPERAND_ADDRESS)    ? ((open != NULL) ? (open + 1) : NULL)
                                  : (parts.type[place] == SASS_OPERAND_REGISTER) ? operand
                                                                                 : NULL;
        if ((named == NULL) || (named[0] != 'R') || (mmgr_text_starts(named, "RZ", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            continue;
        }
        if (sass_register_value(named) > machine->register_last)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  sass_assemble: ");
                mmgr_line_put(output_line, text);
                mmgr_line_put(output_line, " names a register past R");
                mmgr_line_decimal(output_line, machine->register_last, 1u);
                mmgr_line_put(output_line, ", the last the part answers code can name");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            return 0;
        }
    }
    SassPlace places[SASS_MACHINE_OPERANDS];
    unsigned int unplaced = 0u;
    if (!sass_places_find(form, places, &unplaced))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  sass_assemble: operand ");
            mmgr_line_decimal(output_line, unplaced, 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_put(output_line, form->text);
            mmgr_line_put(output_line, " does not place in its own encoding");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    // two operands a form prints from one field, as BAR.SYNC R0, R0 prints its one register, name one value
    for (unsigned int place = 0u; place < parts.operands; place += 1u)
    {
        for (unsigned int other = place + 1u; (places[place].by_text == 0) && (other < parts.operands); other += 1u)
        {
            unsigned long long one = 0ULL;
            unsigned long long two = 0ULL;
            unsigned long long offset = 0ULL;
            const int shared = (places[other].by_text == 0) && (places[other].value.first == places[place].value.first);
            if (shared && sass_operand_value(&parts, place, address, target, &one, &offset) &&
                sass_operand_value(&parts, other, address, target, &two, &offset) && (one != two))
            {
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  sass_assemble: ");
                    mmgr_line_put(output_line, text);
                    mmgr_line_put(output_line, " names two values for the one field operands ");
                    mmgr_line_decimal(output_line, place, 1u);
                    mmgr_line_put(output_line, " and ");
                    mmgr_line_decimal(output_line, other, 1u);
                    mmgr_line_put(output_line, " share");
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
                return 0;
            }
        }
    }
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    *low = form->low;
    *high = form->high;
    // An operand the form carries and does not print retains the bits the form was seen with. NVIDIA writes !PT into
    // the carry-in IMAD reads as IMAD.X and PT into the predicate ISETP reads as .EX, and the part answers alike at
    // every value of either without its modifier (interface_sass_unprinted.md)
    for (unsigned int place = 0u; place < parts.operands; place += 1u)
    {
        if (places[place].by_text != 0)
        {
            if (!mmgr_text_equal(parts.operand[place], base.operand[place], PLATFORM_TEXT_TO_TERMINATOR))
            {
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  sass_assemble: ");
                    mmgr_line_put(output_line, text);
                    mmgr_line_put(output_line, " takes ");
                    mmgr_line_put(output_line, parts.operand[place]);
                    mmgr_line_put(output_line, " where its form carries ");
                    mmgr_line_put(output_line, base.operand[place]);
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
                return 0;
            }
            continue;
        }
        unsigned long long value = 0ULL;
        unsigned long long offset = 0ULL;
        sass_operand_value(&parts, place, address, target, &value, &offset);
        const unsigned long long counted = value / places[place].value.scale;
        sass_bits_write(low, high, places[place].value.first, places[place].value.bits,
                        (places[place].value.inverted != 0u) ? ~counted : counted);
        if (places[place].has_offset != 0)
        {
            sass_bits_write(low, high, places[place].offset.first, places[place].offset.bits, offset);
        }
        // A memory operand that names its descriptor writes the uniform register into the descriptor field and
        // specifies whether it is displayed: desc[URn] sets bit 101 and term[URn] clears it. One that names none
        // retains both as its form carries them
        const int named = (mmgr_text_starts(parts.operand[place], "desc[", PLATFORM_TEXT_TO_TERMINATOR)) ||
                          (mmgr_text_starts(parts.operand[place], "term[", PLATFORM_TEXT_TO_TERMINATOR));
        if ((places[place].has_descriptor != 0) && named)
        {
            const char *const uniform = parts.operand[place] + 5;
            const unsigned long long descriptor = (mmgr_text_starts(uniform, "URZ", PLATFORM_TEXT_TO_TERMINATOR))
                                                      ? SASS_DESCRIPTOR_ZERO
                                                      : sass_register_value(uniform);
            sass_bits_write(low, high, places[place].descriptor.first, places[place].descriptor.bits, descriptor);
            sass_bits_write(low, high, SASS_DESCRIPTOR_DISPLAYED, 1u, (parts.operand[place][0] == 'd') ? 1ULL : 0ULL);
        }
        // a constant's bank must be the one the form carries, since the bank's own bits were not found by probing. And
        // its offset a whole word the field holds, refused and not cut where it is part of a word or past the bank
        if (parts.type[place] == SASS_OPERAND_CONSTANT)
        {
            unsigned long long bank = 0ULL;
            unsigned long long base_bank = 0ULL;
            sass_constant_value(parts.operand[place], &bank);
            sass_constant_value(base.operand[place], &base_bank);
            if (bank != base_bank)
            {
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  sass_assemble: ");
                    mmgr_line_put(output_line, text);
                    mmgr_line_put(output_line, " reads bank ");
                    mmgr_line_decimal(output_line, bank, 1u);
                    mmgr_line_put(output_line, " where its form reads ");
                    mmgr_line_decimal(output_line, base_bank, 1u);
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
                return 0;
            }
            if (((value % places[place].value.scale) != 0ULL) || ((counted >> places[place].value.bits) != 0ULL))
            {
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  sass_assemble: ");
                    mmgr_line_put(output_line, text);
                    mmgr_line_put(output_line, " reads offset 0x");
                    mmgr_line_hex(output_line, value, 1u);
                    mmgr_line_put(output_line, ", which is no word its ");
                    mmgr_line_decimal(output_line, places[place].value.bits, 1u);
                    mmgr_line_put(output_line, "-bit field holds");
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
                return 0;
            }
        }
    }
    const unsigned long long guard =
        (parts.guard[0] == '\0') ? 7ULL : sass_register_value(parts.guard + ((parts.guard[1] == '!') ? 2u : 1u));
    sass_bits_write(low, high, SASS_GUARD_FIRST, SASS_GUARD_BITS, guard);
    sass_bits_write(low, high, SASS_GUARD_NOT, 1u, (parts.guard[1] == '!') ? 1ULL : 0ULL);
    if (control == SASS_CONTROL_SAFE)
    {
        sass_control_safe(machine, form, high);
    }
    return 1;
}

// one line of a listing taken from `text` into `line` and `text` moved past it: the line with the spaces at either
// end cut, and the ';' a listing ends an instruction with dropped. NULL where the text is spent
static const char *sass_line_take(const char *text, char *line, size_t room)
{
    if ((text == NULL) || (*text == '\0'))
    {
        return NULL;
    }
    const size_t length = mmgr_text_span_not(text, "\n", PLATFORM_TEXT_TO_TERMINATOR);
    size_t line_length = length;
    // a listing indents with spaces and a ruleset with tabs, and both end an instruction with a semicolon
    while ((line_length != 0u) && ((text[line_length - 1u] == ' ') || (text[line_length - 1u] == '\t') ||
                                   (text[line_length - 1u] == '\r') || (text[line_length - 1u] == ';')))
    {
        line_length -= 1u;
    }
    size_t first = 0u;
    while ((first < line_length) && ((text[first] == ' ') || (text[first] == '\t')))
    {
        first += 1u;
    }
    const size_t taken = ((line_length - first) < (room - 1u)) ? (line_length - first) : (room - 1u);
    mmgr_bytes_copy(line, text + first, taken);
    line[taken] = '\0';
    return text + length + ((text[length] == '\n') ? 1u : 0u);
}

// what a line is: an instruction, a label, or neither
static int sass_line_is_label(const char *line)
{
    const size_t length = mmgr_text_length(line, PLATFORM_TEXT_TO_TERMINATOR);
    return (length != 0u) && (line[length - 1u] == ':');
}

static int sass_line_is_instruction(const char *line)
{
    return (line[0] != '\0') && (line[0] != '.') && (line[0] != '/') && (line[0] != '#') && !sass_line_is_label(line);
}

// the address the labels give for `operand`, which a listing writes `(.L_x_0); the count where they give none
static unsigned long long sass_label_address(const SassLabels *labels, const char *operand, int *known)
{
    const char *const open = mmgr_text_char(operand, '(', PLATFORM_TEXT_TO_TERMINATOR);
    const size_t length = (open != NULL) ? mmgr_text_span_not(open + 1, ")", PLATFORM_TEXT_TO_TERMINATOR) : 0u;
    *known = 0;
    unsigned long long found = 0ULL;
    for (unsigned int at = 0u; (open != NULL) && (at < labels->count); at += 1u)
    {
        if ((mmgr_text_length(labels->name[at], PLATFORM_TEXT_TO_TERMINATOR) == length) &&
            (mmgr_text_order(labels->name[at], open + 1, (size_t)(length)) == 0))
        {
            found = labels->address[at];
            *known = 1;
        }
    }
    return found;
}

unsigned int sass_assemble_lines(const SassMachine *machine, const char *text, unsigned int control,
                                 unsigned char *code, unsigned long long room)
{
    static SassLabels s_labels;
    s_labels.count = 0u;
    char line[SASS_MACHINE_TEXT];
    unsigned long long address = 0ULL;
    for (const char *walk = sass_line_take(text, line, sizeof(line)); walk != NULL;
         walk = sass_line_take(walk, line, sizeof(line)))
    {
        if (sass_line_is_label(line))
        {
            // a label past the max one text names, or longer than a name is stored, is refused and not dropped
            if ((s_labels.count == SASS_LABELS) ||
                (mmgr_text_length(line, PLATFORM_TEXT_TO_TERMINATOR) > SASS_LABEL_TOKEN))
            {
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  sass_assemble: ");
                    mmgr_line_put(output_line, line);
                    mmgr_line_put(output_line, " is past the ");
                    mmgr_line_decimal(output_line, SASS_LABELS, 1u);
                    mmgr_line_put(output_line, " labels a text names or the ");
                    mmgr_line_decimal(output_line, SASS_LABEL_TOKEN - 1u, 1u);
                    mmgr_line_put(output_line, " characters a name is limited to");
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
                return 0u;
            }
            {
                VerbaLinea buffer_line = mmgr_line_over(s_labels.name[s_labels.count], SASS_LABEL_TOKEN);
                mmgr_line_put_max(&buffer_line, line,
                                  (size_t)((int)(mmgr_text_length(line, PLATFORM_TEXT_TO_TERMINATOR) - 1u)));
                (void)mmgr_line_finish(&buffer_line);
            }
            s_labels.address[s_labels.count] = address;
            s_labels.count += 1u;
        }
        address += sass_line_is_instruction(line) ? 16ULL : 0ULL;
    }
    const unsigned long long bytes = address;
    if (bytes > room)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  sass_assemble: ");
            mmgr_line_decimal(output_line, bytes, 1u);
            mmgr_line_put(output_line, " bytes of code where the room is ");
            mmgr_line_decimal(output_line, room, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0u;
    }
    address = 0ULL;
    for (const char *walk = sass_line_take(text, line, sizeof(line)); walk != NULL;
         walk = sass_line_take(walk, line, sizeof(line)))
    {
        if (!sass_line_is_instruction(line))
        {
            continue;
        }
        SassInstructionParts parts;
        sass_instruction_read(line, &parts);
        unsigned long long target = 0ULL;
        int known = 1;
        for (unsigned int place = 0u; place < parts.operands; place += 1u)
        {
            const char *const open = mmgr_text_char(parts.operand[place], '(', PLATFORM_TEXT_TO_TERMINATOR);
            // a label of the text names where it is; a symbol is a relocation the loader fills, and the text
            // specifies nothing about where it will be
            if ((parts.type[place] == SASS_OPERAND_LABEL) && (open != NULL) && (open[1] == '.'))
            {
                target = sass_label_address(&s_labels, parts.operand[place], &known);
            }
        }
        if (known == 0)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  sass_assemble: ");
                mmgr_line_put(output_line, line);
                mmgr_line_put(output_line, " names a label the text does not");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            return 0u;
        }
        unsigned long long low = 0ULL;
        unsigned long long high = 0ULL;
        if (!sass_assemble(machine, line, address, target, control, &low, &high))
        {
            return 0u;
        }
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            // each word is written low byte first, as the part reads it
            code[address + byte] = (unsigned char)((low >> (8u * byte)) & 0xffu);
            code[address + 8ULL + byte] = (unsigned char)((high >> (8u * byte)) & 0xffu);
        }
        address += 16ULL;
    }
    return (unsigned int)(bytes / 16ULL);
}
