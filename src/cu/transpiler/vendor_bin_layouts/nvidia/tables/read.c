// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.c: the vendor's instruction set read: each CLASS with its opcode, its fields by name and its constant-bank
// operand
#include "read.h"

#include <stdio.h>

#include <stdlib.h>
#include <string.h>

// the line at `*at` of `text`, `size` long, copied into `line` without its end, `*at` moved past it: 1, or 0 at
// the end of the text; a line longer than `room` is cut to it
static int tables_line_read(const unsigned char *text, size_t size, size_t *at, char *line, size_t room)
{
    if (*at >= size)
    {
        return 0;
    }
    size_t length = 0u;
    while ((*at < size) && (text[*at] != '\n'))
    {
        if (length + 1u < room)
        {
            line[length] = (char)text[*at];
            length += 1u;
        }
        *at += 1u;
    }
    line[length] = '\0';
    if (*at < size)
    {
        *at += 1u;
    }
    return 1;
}

static const char *tables_past_spaces(const char *at)
{
    while ((*at == ' ') || (*at == '\t'))
    {
        at += 1;
    }
    return at;
}

static int tables_set_grown(TablesSet *set)
{
    if (set->count < set->room)
    {
        return 1;
    }
    const unsigned int room = (set->room == 0u) ? 64u : (set->room * 2u);
    TablesClass *const grown = (TablesClass *)realloc(set->class_, (size_t)room * sizeof(TablesClass));
    if (grown == NULL)
    {
        return 0;
    }
    set->class_ = grown;
    set->room = room;
    return 1;
}

// `CLASS "<name>"`: the name into `name`, `room` long: 1, or 0 where the line is no class head
static int tables_class_head(const char *line, char *name, size_t room)
{
    if (strncmp(line, "CLASS \"", 7u) != 0)
    {
        return 0;
    }
    const char *const end = strchr(line + 7, '"');
    if (end == NULL)
    {
        return 0;
    }
    const size_t length = (size_t)(end - (line + 7));
    const size_t kept = (length < room) ? length : (room - 1u);
    memcpy(name, line + 7, kept);
    name[kept] = '\0';
    return 1;
}

// the opcode of an OPCODES line, `<name> = 0b<bits>;`, the bits read as they stand
static int tables_opcode_read(const char *line, unsigned long long *opcode)
{
    const char *const at = strstr(line, "0b");
    if (at == NULL)
    {
        return 0;
    }
    unsigned long long value = 0ull;
    for (const char *bit = at + 2; (*bit == '0') || (*bit == '1'); bit += 1)
    {
        value = (value << 1u) | (unsigned long long)(*bit - '0');
    }
    *opcode = value;
    return 1;
}

// `BITS_<n>_<hi>_<lo>..._<name>=<operand>;` into `field`: 1, or 0 where the line is no such field
static int tables_field_line(const char *line, TablesField *field)
{
    if (strncmp(line, "BITS_", 5u) != 0)
    {
        return 0;
    }
    memset(field, 0, sizeof(*field));
    const char *at = line + 5;
    char *end = NULL;
    // the count, then pairs of numbers while a number follows the underscore
    const unsigned long count = strtoul(at, &end, 10);
    if ((end == at) || (*end != '_'))
    {
        return 0;
    }
    at = end + 1;
    for (;;)
    {
        char *high_end = NULL;
        const unsigned long high = strtoul(at, &high_end, 10);
        if ((high_end == at) || (*high_end != '_'))
        {
            break;
        }
        char *low_end = NULL;
        const unsigned long low = strtoul(high_end + 1, &low_end, 10);
        if ((low_end == high_end + 1) || (*low_end != '_') || (low > high) || (high > 127ul))
        {
            break;
        }
        if (field->runs < TABLES_FIELD_RUNS)
        {
            // the bits are below 128, which an unsigned int holds
            field->run[field->runs].first = (unsigned int)low;
            field->run[field->runs].last = (unsigned int)high;
            field->width += (unsigned int)(high - low + 1ul);
            field->runs += 1u;
        }
        at = low_end + 1;
    }
    if (field->runs == 0u)
    {
        return 0;
    }
    const char *const equals = strchr(at, '=');
    if (equals == NULL)
    {
        return 0;
    }
    // the name runs to the equals sign, spaces before it dropped
    size_t length = (size_t)(equals - at);
    while ((length != 0u) && (at[length - 1u] == ' '))
    {
        length -= 1u;
    }
    const size_t kept = (length < TABLES_NAME) ? length : (TABLES_NAME - 1u);
    memcpy(field->name, at, kept);
    field->name[kept] = '\0';
    const char *operand = tables_past_spaces(equals + 1);
    if (*operand == '*')
    {
        field->fixed = 1;
        field->value = strtoull(operand + 1, NULL, 0);
    }
    size_t operand_length = strcspn(operand, ";");
    while ((operand_length != 0u) && (operand[operand_length - 1u] == ' '))
    {
        operand_length -= 1u;
    }
    const size_t operand_kept = (operand_length < TABLES_NAME) ? operand_length : (TABLES_NAME - 1u);
    memcpy(field->operand, operand, operand_kept);
    field->operand[operand_kept] = '\0';
    // the width the line counts is the runs' where the runs are whole; a count past them is a line cut short
    return (unsigned long)field->width == count;
}

// An ENCODING line's fields added to `class_`: one field, or several joined by commas on the line's left and
// written from one operand, `BITS_5_58_54_Sc_bank,BITS_14_53_40_Sc_addr=ConstBankAddress2(Sb_bank,Sb_addr)`, where
// each field takes the operand's argument in its place, and the whole operand where it has none to give
static void tables_fields_line(const char *line, TablesClass *class_)
{
    const char *const equals = strchr(line, '=');
    if ((equals == NULL) || (strncmp(line, "BITS_", 5u) != 0))
    {
        return;
    }
    const char *const operand = tables_past_spaces(equals + 1);
    // the operand's arguments, where it is a call: the text between its parentheses, split on commas
    const char *const open = strchr(operand, '(');
    const char *const close = (open != NULL) ? strrchr(operand, ')') : NULL;
    unsigned int parts = 1u;
    for (const char *at = line; at < equals; at += 1)
    {
        parts += (*at == ',') ? 1u : 0u;
    }
    const char *part = line;
    for (unsigned int index = 0u; index < parts; index += 1u)
    {
        const char *const part_end = (index + 1u < parts) ? strchr(part, ',') : equals;
        if (part_end == NULL)
        {
            return;
        }
        char one[1024];
        const size_t left = (size_t)(part_end - part);
        if (left + 2u >= sizeof(one))
        {
            return;
        }
        memcpy(one, part, left);
        size_t length = left;
        one[length] = '=';
        length += 1u;
        // the argument in this field's place, or the whole operand
        const char *argument = operand;
        size_t argument_length = strcspn(operand, ";");
        if ((parts > 1u) && (open != NULL) && (close != NULL) && (close > open))
        {
            const char *at = open + 1;
            for (unsigned int skipped = 0u; (skipped < index) && (at < close); at += 1)
            {
                skipped += (*at == ',') ? 1u : 0u;
            }
            const char *const comma = strchr(at, ',');
            argument = at;
            argument_length = (size_t)((((comma != NULL) && (comma < close)) ? comma : close) - at);
        }
        if (length + argument_length + 2u >= sizeof(one))
        {
            return;
        }
        memcpy(one + length, argument, argument_length);
        length += argument_length;
        one[length] = ';';
        one[length + 1u] = '\0';
        if ((class_->fields < TABLES_CLASS_FIELDS) && tables_field_line(one, &class_->field[class_->fields]))
        {
            class_->fields += 1u;
        }
        part = part_end + 1;
    }
}

// the name after the last ':' before `end` in `text`, into `name`
static void tables_name_before(const char *text, const char *end, char *name)
{
    const char *colon = NULL;
    for (const char *at = text; at < end; at += 1)
    {
        colon = (*at == ':') ? at : colon;
    }
    name[0] = '\0';
    if (colon == NULL)
    {
        return;
    }
    const char *at = colon + 1;
    size_t length = 0u;
    while ((at + length < end) && (length + 1u < TABLES_NAME) &&
           (((at[length] >= 'a') && (at[length] <= 'z')) || ((at[length] >= 'A') && (at[length] <= 'Z')) ||
            ((at[length] >= '0') && (at[length] <= '9')) || (at[length] == '_')))
    {
        name[length] = at[length];
        length += 1u;
    }
    name[length] = '\0';
}

// the width of the immediate written before `end` inside a FORMAT bracket, `SImm(17/0)*:Ra_offset` or
// `UImm(5/0*):Sa_bank`, read off the digits after `Imm(`; 0 where the bracket holds none
static unsigned int tables_immediate_bits(const char *open, const char *end)
{
    for (const char *at = open; at + 4 < end; at += 1)
    {
        if (strncmp(at, "Imm(", 4u) == 0)
        {
            return (unsigned int)strtoul(at + 4, NULL, 10);
        }
    }
    return 0u;
}

// a FORMAT line holding a size operand, `/SZ_U8_S8_U16_S16_32_64("32"):sz`: the choices between `SZ_` and the
// parenthesis, each a width in bits after any letters, in the order the field counts them, and the operand's name
// after the colon
static void tables_size_read(const char *line, TablesClass *class_)
{
    const char *const size = strstr(line, "/SZ_");
    const char *const open = (size != NULL) ? strchr(size, '(') : NULL;
    const char *const close = (open != NULL) ? strchr(open, ')') : NULL;
    if ((close == NULL) || (close[1] != ':'))
    {
        return;
    }
    class_->sizes = 0u;
    // the list's name, from the slash to the parenthesis, which a condition's constant names a choice under
    const size_t list_length = (size_t)(open - (size + 1));
    const size_t list_kept = (list_length < TABLES_NAME) ? list_length : (TABLES_NAME - 1u);
    memcpy(class_->size_list, size + 1, list_kept);
    class_->size_list[list_kept] = '\0';
    for (const char *at = size + 4; (at < open) && (class_->sizes < TABLES_SIZES);)
    {
        const char *end = at;
        while ((end < open) && (*end != '_'))
        {
            end += 1;
        }
        const char *digits = at;
        while ((digits < end) && ((*digits < '0') || (*digits > '9')))
        {
            digits += 1;
        }
        class_->size_bytes[class_->sizes] = (unsigned int)strtoul(digits, NULL, 10) / 8u;
        class_->sizes += 1u;
        at = end + 1;
    }
    size_t length = 0u;
    for (const char *at = close + 2; (*at != '\0') && (*at != ' ') && (*at != ';') && (length + 1u < TABLES_NAME);
         at += 1)
    {
        class_->size[length] = *at;
        length += 1u;
    }
    class_->size[length] = '\0';
}

unsigned long long tables_class_offset(const TablesClass *class_, unsigned long long low, unsigned long long high)
{
    const TablesField *const offset =
        (class_->offset[0] != '\0') ? tables_field_by_operand(class_, class_->offset) : NULL;
    if (offset == NULL)
    {
        return 0ull;
    }
    const unsigned int magnitude = (class_->offset_bits != 0u) ? (class_->offset_bits - 1u) : offset->width;
    const unsigned int dropped = (magnitude > offset->width) ? (magnitude - offset->width) : 0u;
    return tables_field_read(offset, low, high) << dropped;
}

int tables_class_described(const TablesSet *set, unsigned long long low, unsigned long long high, char *text,
                           size_t room)
{
    const TablesClass *const class_ = tables_class_of(set, low, high);
    if (class_ == NULL)
    {
        snprintf(text, room, "%016llx %016llx -", low, high);
        return 0;
    }
    const TablesField *const bank = (class_->bank[0] != '\0') ? tables_field_by_operand(class_, class_->bank) : NULL;
    if (bank == NULL)
    {
        snprintf(text, room, "%016llx %016llx %s", low, high, class_->name);
        return 1;
    }
    snprintf(text, room, "%016llx %016llx %s bank %llu offset 0x%llx reads %u", low, high, class_->name,
             tables_field_read(bank, low, high), tables_class_offset(class_, low, high),
             tables_class_bytes(class_, low, high));
    return 1;
}

unsigned int tables_class_bytes(const TablesClass *class_, unsigned long long low, unsigned long long high)
{
    const TablesField *const field = (class_->size[0] != '\0') ? tables_field_by_operand(class_, class_->size) : NULL;
    if (field == NULL)
    {
        return 4u;
    }
    const unsigned long long choice = tables_field_read(field, low, high);
    return (choice < class_->sizes) ? class_->size_bytes[choice] : 4u;
}

// a FORMAT line holding a constant-bank operand, `C:<name>[...:<bank>]...[...:<offset>]`: the bank the name
// after the first bracket's last colon, the offset the name after the last bracket's last colon, and the offset
// operand's width the immediate's inside that bracket
static void tables_constant_read(const char *line, TablesClass *class_)
{
    const char *const constant = strstr(line, "C:");
    if ((constant == NULL) ||
        (constant != line && (constant[-1] != '\'') && (constant[-1] != ' ') && (constant[-1] != ',')))
    {
        return;
    }
    const char *const first_open = strchr(constant, '[');
    const char *const first_close = (first_open != NULL) ? strchr(first_open, ']') : NULL;
    if (first_close == NULL)
    {
        return;
    }
    tables_name_before(first_open, first_close, class_->bank);
    const char *const last_close = strrchr(first_close, ']');
    const char *last_open = NULL;
    for (const char *at = first_close; (last_close != NULL) && (at < last_close); at += 1)
    {
        last_open = (*at == '[') ? at : last_open;
    }
    if ((last_open != NULL) && (last_close != NULL))
    {
        tables_name_before(last_open, last_close, class_->offset);
        class_->offset_bits = tables_immediate_bits(last_open, last_close);
    }
}

int tables_set_read(TablesSet *set, const unsigned char *text, size_t size)
{
    char line[1024];
    size_t at = 0u;
    TablesClass *class_ = NULL;
    enum
    {
        ELSEWHERE,
        FORMAT,
        OPCODES,
        ENCODING,
        CONDITIONS
    } part = ELSEWHERE;
    // within CONDITIONS, whether the conditions read are under a kind on the registers, OOR_REG_ERROR or
    // MISALIGNED_REG_ERROR
    int register_kind = 0;
    while (tables_line_read(text, size, &at, line, sizeof(line)))
    {
        char name[TABLES_NAME + 16u];
        if (tables_class_head(line, name, sizeof(name)))
        {
            if (!tables_set_grown(set))
            {
                return 0;
            }
            class_ = &set->class_[set->count];
            set->count += 1u;
            memset(class_, 0, sizeof(*class_));
            memcpy(class_->name, name, sizeof(name));
            part = ELSEWHERE;
            continue;
        }
        if (class_ == NULL)
        {
            continue;
        }
        const size_t length = strlen(line);
        if (strncmp(line, "FORMAT", 6u) == 0)
        {
            part = FORMAT;
        }
        else if (strcmp(line, "OPCODES") == 0)
        {
            part = OPCODES;
            continue;
        }
        else if (strcmp(line, "ENCODING") == 0)
        {
            part = ENCODING;
            continue;
        }
        else if (strcmp(line, "CONDITIONS") == 0)
        {
            part = CONDITIONS;
            register_kind = 0;
            continue;
        }
        else if ((line[0] >= 'A') && (line[0] <= 'Z') && (strspn(line, "ABCDEFGHIJKLMNOPQRSTUVWXYZ_") == length))
        {
            // within CONDITIONS, a kind of error the conditions after it are under; elsewhere another part's head,
            // PROPERTIES, PREDICATES: capitals and underscores alone
            if ((part == CONDITIONS) && (length > 6u) && (strcmp(line + length - 6u, "_ERROR") == 0))
            {
                register_kind = (strstr(line, "REG_ERROR") != NULL);
                continue;
            }
            part = ELSEWHERE;
            continue;
        }
        if (part == CONDITIONS)
        {
            // a condition is a line ending in a colon, under a kind on the registers; the line after it, in quotes, is
            // its message, and a condition longer than its room is not kept
            const char *const text_at = tables_past_spaces(line);
            size_t kept = strlen(text_at);
            while ((kept != 0u) && ((text_at[kept - 1u] == ' ') || (text_at[kept - 1u] == ':')))
            {
                kept -= 1u;
            }
            if (register_kind && (kept != 0u) && (text_at[strlen(text_at) - 1u] == ':') &&
                (kept + 1u < TABLES_CONDITION_TEXT) && (class_->conditions < TABLES_CONDITIONS))
            {
                memcpy(class_->condition[class_->conditions], text_at, kept);
                class_->condition[class_->conditions][kept] = '\0';
                class_->conditions += 1u;
            }
            continue;
        }
        if (part == FORMAT)
        {
            tables_constant_read(line, class_);
            tables_size_read(line, class_);
            if (line[strlen(line) - ((strlen(line) != 0u) ? 1u : 0u)] == ';')
            {
                part = ELSEWHERE;
            }
        }
        else if (part == OPCODES)
        {
            unsigned long long opcode = 0ull;
            if (tables_opcode_read(line, &opcode))
            {
                class_->opcode = opcode;
            }
        }
        else if (part == ENCODING)
        {
            tables_fields_line(line, class_);
        }
    }
    return 1;
}

void tables_set_freed(TablesSet *set)
{
    free(set->class_);
    set->class_ = NULL;
    set->count = 0u;
    set->room = 0u;
}

const TablesField *tables_field_named(const TablesClass *class_, const char *name)
{
    for (unsigned int field = 0u; field < class_->fields; field += 1u)
    {
        if (strcmp(class_->field[field].name, name) == 0)
        {
            return &class_->field[field];
        }
    }
    return NULL;
}

const TablesField *tables_field_by_operand(const TablesClass *class_, const char *operand)
{
    for (unsigned int field = 0u; field < class_->fields; field += 1u)
    {
        if (!class_->field[field].fixed && (strcmp(class_->field[field].operand, operand) == 0))
        {
            return &class_->field[field];
        }
    }
    return NULL;
}

static unsigned long long tables_bits_read(unsigned long long low, unsigned long long high, unsigned int first,
                                           unsigned int last)
{
    const unsigned int width = last - first + 1u;
    const unsigned long long mask = (width >= 64u) ? ~0ull : ((1ull << width) - 1ull);
    if (last < 64u)
    {
        return (low >> first) & mask;
    }
    if (first >= 64u)
    {
        return (high >> (first - 64u)) & mask;
    }
    // a run across the words: the low word's part under the high word's
    const unsigned int low_width = 64u - first;
    return ((low >> first) | (high << low_width)) & mask;
}

static void tables_bits_write(unsigned long long *low, unsigned long long *high, unsigned int first, unsigned int last,
                              unsigned long long value)
{
    for (unsigned int bit = first; bit <= last; bit += 1u)
    {
        const unsigned long long one = (value >> (bit - first)) & 1ull;
        if (bit < 64u)
        {
            *low = (*low & ~(1ull << bit)) | (one << bit);
        }
        else
        {
            *high = (*high & ~(1ull << (bit - 64u))) | (one << (bit - 64u));
        }
    }
}

unsigned long long tables_field_read(const TablesField *field, unsigned long long low, unsigned long long high)
{
    unsigned long long value = 0ull;
    for (unsigned int run = 0u; run < field->runs; run += 1u)
    {
        const unsigned int width = field->run[run].last - field->run[run].first + 1u;
        value = (value << width) | tables_bits_read(low, high, field->run[run].first, field->run[run].last);
    }
    return value;
}

void tables_field_write(const TablesField *field, unsigned long long *low, unsigned long long *high,
                        unsigned long long value)
{
    // the low run takes the value's low bits: the runs are written last first
    unsigned int run = field->runs;
    while (run != 0u)
    {
        run -= 1u;
        const unsigned int width = field->run[run].last - field->run[run].first + 1u;
        const unsigned long long mask = (width >= 64u) ? ~0ull : ((1ull << width) - 1ull);
        tables_bits_write(low, high, field->run[run].first, field->run[run].last, value & mask);
        value >>= width;
    }
}

// A condition read: the text walked, the class and the encoding it is judged over, and whether anything it names is
// past this reader's judgment; a condition naming one such thing is not judged at all
typedef struct
{
    const char *at;
    const TablesClass *class_;
    unsigned long long low;
    unsigned long long high;
    int unjudged;
} TablesCondition;

// the registers' own constants, as a condition names them after the backquote: RZ, R<n>, URZ, UR<n>, PT and UPT
#define TABLES_REGISTER_ZERO 255ll
#define TABLES_UNIFORM_ZERO 63ll
#define TABLES_PREDICATE_TRUE 7ll
#define TABLES_REGISTER_COUNT 256ll
#define TABLES_UNIFORM_COUNT 64ll

static void tables_condition_spaces(TablesCondition *condition)
{
    while (*condition->at == ' ')
    {
        condition->at += 1;
    }
}

// 1 where the text at the condition's place begins with `word`, which is then stepped over
static int tables_condition_word(TablesCondition *condition, const char *word)
{
    tables_condition_spaces(condition);
    const size_t length = strlen(word);
    if (strncmp(condition->at, word, length) != 0)
    {
        return 0;
    }
    condition->at += length;
    return 1;
}

static int tables_condition_name_char(char character)
{
    return ((character >= 'a') && (character <= 'z')) || ((character >= 'A') && (character <= 'Z')) ||
           ((character >= '0') && (character <= '9')) || (character == '_');
}

// a constant after the backquote, `` `Register@RZ ``, `` `SZ_U8_S8_U16_S16_32_64@"64" ``: the registers' own by
// name, a choice of the class's size list by its place in the list, and anything else unjudged
static long long tables_condition_constant(TablesCondition *condition)
{
    char list[TABLES_NAME];
    char value[TABLES_NAME];
    size_t length = 0u;
    while (tables_condition_name_char(*condition->at) && (length + 1u < sizeof(list)))
    {
        list[length] = *condition->at;
        length += 1u;
        condition->at += 1;
    }
    list[length] = '\0';
    if (*condition->at != '@')
    {
        condition->unjudged = 1;
        return 0ll;
    }
    condition->at += 1;
    const int quoted = (*condition->at == '"');
    condition->at += quoted ? 1 : 0;
    length = 0u;
    while ((*condition->at != '\0') && (*condition->at != ')') && (*condition->at != ' ') &&
           (!quoted || (*condition->at != '"')) && (quoted || tables_condition_name_char(*condition->at)) &&
           (length + 1u < sizeof(value)))
    {
        value[length] = *condition->at;
        length += 1u;
        condition->at += 1;
    }
    value[length] = '\0';
    condition->at += (quoted && (*condition->at == '"')) ? 1 : 0;
    if (strcmp(list, "Register") == 0)
    {
        if (strcmp(value, "RZ") == 0)
        {
            return TABLES_REGISTER_ZERO;
        }
        if (value[0] == 'R')
        {
            return strtoll(value + 1, NULL, 10);
        }
    }
    if (strcmp(list, "UniformRegister") == 0)
    {
        if (strcmp(value, "URZ") == 0)
        {
            return TABLES_UNIFORM_ZERO;
        }
        if (strncmp(value, "UR", 2u) == 0)
        {
            return strtoll(value + 2, NULL, 10);
        }
    }
    if (((strcmp(list, "Predicate") == 0) && (strcmp(value, "PT") == 0)) ||
        ((strcmp(list, "UniformPredicate") == 0) && (strcmp(value, "UPT") == 0)))
    {
        return TABLES_PREDICATE_TRUE;
    }
    if ((condition->class_->size_list[0] != '\0') && (strcmp(list, condition->class_->size_list) == 0))
    {
        // the choice's place in the list, counted from the first underscore past the list's prefix: the size field
        // holds that place
        const char *choice = strchr(list, '_');
        long long place = 0ll;
        while (choice != NULL)
        {
            choice += 1;
            const char *const end = strchr(choice, '_');
            const size_t choice_length = (end != NULL) ? (size_t)(end - choice) : strlen(choice);
            if ((choice_length == strlen(value)) && (strncmp(choice, value, choice_length) == 0))
            {
                return place;
            }
            place += 1ll;
            choice = end;
        }
    }
    condition->unjudged = 1;
    return 0ll;
}

static long long tables_condition_implies(TablesCondition *condition);

// a primary: a parenthesized condition, a number, a constant after a backquote, a register count after a percent,
// or an operand of the class read off the encoding; anything else is unjudged
static long long tables_condition_primary(TablesCondition *condition)
{
    tables_condition_spaces(condition);
    if (*condition->at == '(')
    {
        condition->at += 1;
        const long long value = tables_condition_implies(condition);
        tables_condition_spaces(condition);
        if (*condition->at == ')')
        {
            condition->at += 1;
        }
        else
        {
            condition->unjudged = 1;
        }
        return value;
    }
    if ((*condition->at >= '0') && (*condition->at <= '9'))
    {
        char *end = NULL;
        const long long value = strtoll(condition->at, &end, 0);
        condition->at = end;
        return value;
    }
    if (*condition->at == '`')
    {
        condition->at += 1;
        return tables_condition_constant(condition);
    }
    if (*condition->at == '%')
    {
        condition->at += 1;
        if (tables_condition_word(condition, "MAX_REG_COUNT"))
        {
            return TABLES_REGISTER_COUNT;
        }
        if (tables_condition_word(condition, "MAX_UNIFORM_REG_COUNT"))
        {
            return TABLES_UNIFORM_COUNT;
        }
        while (tables_condition_name_char(*condition->at))
        {
            condition->at += 1;
        }
        condition->unjudged = 1;
        return 0ll;
    }
    char operand[TABLES_NAME];
    size_t length = 0u;
    while (tables_condition_name_char(*condition->at) && (length + 1u < sizeof(operand)))
    {
        operand[length] = *condition->at;
        length += 1u;
        condition->at += 1;
    }
    operand[length] = '\0';
    const TablesField *const field = (length != 0u) ? tables_field_by_operand(condition->class_, operand) : NULL;
    if (field == NULL)
    {
        // a name no field of the class is written from, or no name at all where one was due
        condition->at += (length == 0u && *condition->at != '\0') ? 1 : 0;
        condition->unjudged = 1;
        return 0ll;
    }
    // a field holds at most 64 bits, read as the number it holds
    return (long long)tables_field_read(field, condition->low, condition->high);
}

static long long tables_condition_unary(TablesCondition *condition)
{
    tables_condition_spaces(condition);
    if ((*condition->at == '!') && (condition->at[1] != '='))
    {
        condition->at += 1;
        return (tables_condition_unary(condition) == 0ll) ? 1ll : 0ll;
    }
    if (*condition->at == '-')
    {
        condition->at += 1;
        return -tables_condition_unary(condition);
    }
    return tables_condition_primary(condition);
}

static long long tables_condition_product(TablesCondition *condition)
{
    long long value = tables_condition_unary(condition);
    for (;;)
    {
        tables_condition_spaces(condition);
        if (*condition->at == '*')
        {
            condition->at += 1;
            value *= tables_condition_unary(condition);
        }
        else if ((*condition->at == '%') && (condition->at[1] != 'M'))
        {
            condition->at += 1;
            const long long divisor = tables_condition_unary(condition);
            value = (divisor != 0ll) ? (value % divisor) : 0ll;
            condition->unjudged = condition->unjudged || (divisor == 0ll);
        }
        else if (*condition->at == '/')
        {
            condition->at += 1;
            const long long divisor = tables_condition_unary(condition);
            value = (divisor != 0ll) ? (value / divisor) : 0ll;
            condition->unjudged = condition->unjudged || (divisor == 0ll);
        }
        else
        {
            return value;
        }
    }
}

static long long tables_condition_sum(TablesCondition *condition)
{
    long long value = tables_condition_product(condition);
    for (;;)
    {
        tables_condition_spaces(condition);
        if (*condition->at == '+')
        {
            condition->at += 1;
            value += tables_condition_product(condition);
        }
        else if ((*condition->at == '-') && (condition->at[1] != '>'))
        {
            condition->at += 1;
            value -= tables_condition_product(condition);
        }
        else
        {
            return value;
        }
    }
}

static long long tables_condition_compared(TablesCondition *condition)
{
    const long long left = tables_condition_sum(condition);
    tables_condition_spaces(condition);
    if (tables_condition_word(condition, "=="))
    {
        return (left == tables_condition_sum(condition)) ? 1ll : 0ll;
    }
    if (tables_condition_word(condition, "!="))
    {
        return (left != tables_condition_sum(condition)) ? 1ll : 0ll;
    }
    if (tables_condition_word(condition, "<="))
    {
        return (left <= tables_condition_sum(condition)) ? 1ll : 0ll;
    }
    if (tables_condition_word(condition, ">="))
    {
        return (left >= tables_condition_sum(condition)) ? 1ll : 0ll;
    }
    if ((*condition->at == '<') && (condition->at[1] != '<'))
    {
        condition->at += 1;
        return (left < tables_condition_sum(condition)) ? 1ll : 0ll;
    }
    if ((*condition->at == '>') && (condition->at[1] != '>'))
    {
        condition->at += 1;
        return (left > tables_condition_sum(condition)) ? 1ll : 0ll;
    }
    return left;
}

static long long tables_condition_both(TablesCondition *condition)
{
    long long value = tables_condition_compared(condition);
    while (tables_condition_word(condition, "&&"))
    {
        const long long right = tables_condition_compared(condition);
        value = ((value != 0ll) && (right != 0ll)) ? 1ll : 0ll;
    }
    return value;
}

static long long tables_condition_either(TablesCondition *condition)
{
    long long value = tables_condition_both(condition);
    while (tables_condition_word(condition, "||"))
    {
        const long long right = tables_condition_both(condition);
        value = ((value != 0ll) || (right != 0ll)) ? 1ll : 0ll;
    }
    return value;
}

// `left -> right`: the right holds where the left does, and the whole holds where the left does not
static long long tables_condition_implies(TablesCondition *condition)
{
    const long long left = tables_condition_either(condition);
    if (tables_condition_word(condition, "->"))
    {
        const long long right = tables_condition_implies(condition);
        return ((left == 0ll) || (right != 0ll)) ? 1ll : 0ll;
    }
    return left;
}

int tables_class_legal(const TablesClass *class_, unsigned long long low, unsigned long long high,
                       const char **failed)
{
    for (unsigned int at = 0u; at < class_->conditions; at += 1u)
    {
        TablesCondition condition = {class_->condition[at], class_, low, high, 0};
        const long long held = tables_condition_implies(&condition);
        tables_condition_spaces(&condition);
        // text left over is a condition this reader did not read whole, and does not judge
        if (*condition.at != '\0')
        {
            condition.unjudged = 1;
        }
        if (!condition.unjudged && (held == 0ll))
        {
            if (failed != NULL)
            {
                *failed = class_->condition[at];
            }
            return 0;
        }
    }
    return 1;
}

const TablesClass *tables_class_of(const TablesSet *set, unsigned long long low, unsigned long long high)
{
    for (unsigned int at = 0u; at < set->count; at += 1u)
    {
        const TablesClass *const class_ = &set->class_[at];
        const TablesField *const opcode = tables_field_named(class_, "opcode");
        if ((opcode != NULL) && (tables_field_read(opcode, low, high) == class_->opcode))
        {
            return class_;
        }
    }
    return NULL;
}
