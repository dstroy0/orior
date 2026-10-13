// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// assemble.c: the assembly: one instruction assembled from its text, a line taken, a label and an instruction told apart, a label's address, and the lines of a listing assembled
#include "sass_assemble_internal.h"

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
        printf("  sass_assemble: no form for %s\n", text);
        return 0;
    }
    // a register past the last the part answers a question's code can name is refused, the high half of a pair
    // counted as its own register; RZ is no register a thread holds, and is named past every one
    for (unsigned int place = 0u; place < parts.operands; place += 1u)
    {
        const char *const operand = parts.operand[place];
        const char *const open = strrchr(operand, '[');
        const char *const named = (parts.kind[place] == SASS_OPERAND_ADDRESS) ? ((open != NULL) ? (open + 1) : NULL)
                                  : (parts.kind[place] == SASS_OPERAND_REGISTER) ? operand
                                                                                 : NULL;
        if ((named == NULL) || (named[0] != 'R') || (strncmp(named, "RZ", 2u) == 0))
        {
            continue;
        }
        if (sass_register_value(named) > machine->register_last)
        {
            printf("  sass_assemble: %s names a register past R%u, the last the part answers code can name\n", text,
                   machine->register_last);
            return 0;
        }
    }
    SassPlace places[SASS_MACHINE_OPERANDS];
    unsigned int unplaced = 0u;
    if (!sass_places_find(form, places, &unplaced))
    {
        printf("  sass_assemble: operand %u of %s does not place in its own encoding\n", unplaced, form->text);
        return 0;
    }
    // two operands a form prints from one field, as BAR.SYNC R0, R0 prints its one register, name one value
    for (unsigned int place = 0u; place < parts.operands; place += 1u)
    {
        for (unsigned int other = place + 1u; (places[place].by_text == 0) && (other < parts.operands); other += 1u)
        {
            unsigned long long one = 0ull;
            unsigned long long two = 0ull;
            unsigned long long offset = 0ull;
            const int shared = (places[other].by_text == 0) && (places[other].value.first == places[place].value.first);
            if (shared && sass_operand_value(&parts, place, address, target, &one, &offset) &&
                sass_operand_value(&parts, other, address, target, &two, &offset) && (one != two))
            {
                printf("  sass_assemble: %s names two values for the one field operands %u and %u share\n", text, place,
                       other);
                return 0;
            }
        }
    }
    SassInstructionParts base;
    sass_instruction_read(form->text, &base);
    *low = form->low;
    *high = form->high;
    // An operand the form holds and does not print keeps the bits the form was seen with. NVIDIA writes !PT into the
    // carry-in IMAD reads as IMAD.X and PT into the predicate ISETP reads as .EX, and the part answers alike at every
    // value of either without its modifier (interface_sass_unprinted.md)
    for (unsigned int place = 0u; place < parts.operands; place += 1u)
    {
        if (places[place].by_text != 0)
        {
            if (strcmp(parts.operand[place], base.operand[place]) != 0)
            {
                printf("  sass_assemble: %s takes %s where its form holds %s\n", text, parts.operand[place],
                       base.operand[place]);
                return 0;
            }
            continue;
        }
        unsigned long long value = 0ull;
        unsigned long long offset = 0ull;
        sass_operand_value(&parts, place, address, target, &value, &offset);
        const unsigned long long counted = value / places[place].value.scale;
        sass_bits_write(low, high, places[place].value.first, places[place].value.bits,
                        (places[place].value.inverted != 0u) ? ~counted : counted);
        if (places[place].has_offset != 0)
        {
            sass_bits_write(low, high, places[place].offset.first, places[place].offset.bits, offset);
        }
        // A memory operand that names its descriptor writes the uniform register into the descriptor field and says
        // whether it is shown: desc[URn] sets bit 101 and term[URn] clears it. One that names none keeps both as its
        // form holds them
        const int named = (strncmp(parts.operand[place], "desc[", 5u) == 0) ||
                          (strncmp(parts.operand[place], "term[", 5u) == 0);
        if ((places[place].has_descriptor != 0) && named)
        {
            const char *const uniform = parts.operand[place] + 5;
            const unsigned long long descriptor = (strncmp(uniform, "URZ", 3u) == 0) ? SASS_DESCRIPTOR_ZERO
                                                                                    : sass_register_value(uniform);
            sass_bits_write(low, high, places[place].descriptor.first, places[place].descriptor.bits, descriptor);
            sass_bits_write(low, high, SASS_DESCRIPTOR_SHOWN, 1u, (parts.operand[place][0] == 'd') ? 1ull : 0ull);
        }
        // a constant's bank must be the one the form holds, since the bank's own bits were not found by probing
        if (parts.kind[place] == SASS_OPERAND_CONSTANT)
        {
            unsigned long long bank = 0ull;
            unsigned long long base_bank = 0ull;
            sass_constant_value(parts.operand[place], &bank);
            sass_constant_value(base.operand[place], &base_bank);
            if (bank != base_bank)
            {
                printf("  sass_assemble: %s reads bank %llu where its form reads %llu\n", text, bank, base_bank);
                return 0;
            }
        }
    }
    const unsigned long long guard = (parts.guard[0] == '\0')
                                         ? 7ull
                                         : sass_register_value(parts.guard + ((parts.guard[1] == '!') ? 2u : 1u));
    sass_bits_write(low, high, SASS_GUARD_FIRST, SASS_GUARD_BITS, guard);
    sass_bits_write(low, high, SASS_GUARD_NOT, 1u, (parts.guard[1] == '!') ? 1ull : 0ull);
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
    const size_t length = strcspn(text, "\n");
    size_t kept = length;
    // a listing indents with spaces and a ruleset with tabs, and both end an instruction with a semicolon
    while ((kept != 0u) && ((text[kept - 1u] == ' ') || (text[kept - 1u] == '\t') || (text[kept - 1u] == '\r') ||
                            (text[kept - 1u] == ';')))
    {
        kept -= 1u;
    }
    size_t first = 0u;
    while ((first < kept) && ((text[first] == ' ') || (text[first] == '\t')))
    {
        first += 1u;
    }
    const size_t taken = ((kept - first) < (room - 1u)) ? (kept - first) : (room - 1u);
    memcpy(line, text + first, taken);
    line[taken] = '\0';
    return text + length + ((text[length] == '\n') ? 1u : 0u);
}

// what a line is: an instruction, a label, or neither
static int sass_line_is_label(const char *line)
{
    const size_t length = strlen(line);
    return (length != 0u) && (line[length - 1u] == ':');
}

static int sass_line_is_instruction(const char *line)
{
    return (line[0] != '\0') && (line[0] != '.') && (line[0] != '/') && (line[0] != '#') && !sass_line_is_label(line);
}

// the address the labels hold for `operand`, which a listing writes `(.L_x_0); the count where they hold none
static unsigned long long sass_label_address(const SassLabels *labels, const char *operand, int *known)
{
    const char *const open = strchr(operand, '(');
    const size_t length = (open != NULL) ? strcspn(open + 1, ")") : 0u;
    *known = 0;
    unsigned long long found = 0ull;
    for (unsigned int at = 0u; (open != NULL) && (at < labels->count); at += 1u)
    {
        if ((strlen(labels->name[at]) == length) && (strncmp(labels->name[at], open + 1, length) == 0))
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
    unsigned long long address = 0ull;
    for (const char *walk = sass_line_take(text, line, sizeof(line)); walk != NULL;
         walk = sass_line_take(walk, line, sizeof(line)))
    {
        if (sass_line_is_label(line))
        {
            // a label past the most one text names, or longer than a name is kept, is refused and not dropped
            if ((s_labels.count == SASS_LABELS) || (strlen(line) > SASS_LABEL_TOKEN))
            {
                printf("  sass_assemble: %s is past the %u labels a text names or the %u letters a name is kept to\n",
                       line, SASS_LABELS, SASS_LABEL_TOKEN - 1u);
                return 0u;
            }
            snprintf(s_labels.name[s_labels.count], SASS_LABEL_TOKEN, "%.*s", (int)(strlen(line) - 1u), line);
            s_labels.address[s_labels.count] = address;
            s_labels.count += 1u;
        }
        address += sass_line_is_instruction(line) ? 16ull : 0ull;
    }
    const unsigned long long bytes = address;
    if (bytes > room)
    {
        printf("  sass_assemble: %llu bytes of code where the room is %llu\n", bytes, room);
        return 0u;
    }
    address = 0ull;
    for (const char *walk = sass_line_take(text, line, sizeof(line)); walk != NULL;
         walk = sass_line_take(walk, line, sizeof(line)))
    {
        if (!sass_line_is_instruction(line))
        {
            continue;
        }
        SassInstructionParts parts;
        sass_instruction_read(line, &parts);
        unsigned long long target = 0ull;
        int known = 1;
        for (unsigned int place = 0u; place < parts.operands; place += 1u)
        {
            const char *const open = strchr(parts.operand[place], '(');
            // a label of the text names where it stands; a symbol is a relocation the loader fills, and the text
            // says nothing about where it will be
            if ((parts.kind[place] == SASS_OPERAND_LABEL) && (open != NULL) && (open[1] == '.'))
            {
                target = sass_label_address(&s_labels, parts.operand[place], &known);
            }
        }
        if (known == 0)
        {
            printf("  sass_assemble: %s names a label the text does not\n", line);
            return 0u;
        }
        unsigned long long low = 0ull;
        unsigned long long high = 0ull;
        if (!sass_assemble(machine, line, address, target, control, &low, &high))
        {
            return 0u;
        }
        for (unsigned int byte = 0u; byte < 8u; byte += 1u)
        {
            // each word is written low byte first, as the part reads it
            code[address + byte] = (unsigned char)((low >> (8u * byte)) & 0xffu);
            code[address + 8ull + byte] = (unsigned char)((high >> (8u * byte)) & 0xffu);
        }
        address += 16ull;
    }
    return (unsigned int)(bytes / 16ull);
}
