// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.c: the vendor's instruction set read: each CLASS with its opcode, its fields by name and its constant-bank
// operand
#include "read.h"
#include "../../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

#include "../../../../../../includes/parsers/line_read.h"

#include <stdio.h>

#include <stdlib.h>
#include <string.h>

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
    const char *const end = mmgr_text_char(line + 7, '"', PLATFORM_TEXT_TO_TERMINATOR);
    if (end == NULL)
    {
        return 0;
    }
    const size_t length = (size_t)(end - (line + 7));
    const size_t copied = (length < room) ? length : (room - 1u);
    mmgr_bytes_copy(name, line + 7, copied);
    name[copied] = '\0';
    return 1;
}

// the opcode of an OPCODES line, `<name> = 0b<bits>;`, the bits read as they are
static int tables_opcode_read(const char *line, unsigned long long *opcode)
{
    const char *const at = mmgr_text_find(line, "0b", PLATFORM_TEXT_TO_TERMINATOR);
    if (at == NULL)
    {
        return 0;
    }
    unsigned long long value = 0ULL;
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
    if (!mmgr_text_starts(line, "BITS_", PLATFORM_TEXT_TO_TERMINATOR))
    {
        return 0;
    }
    mmgr_bytes_set(field, 0, sizeof(*field));
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
        if ((low_end == high_end + 1) || (*low_end != '_') || (low > high) || (high > 127UL))
        {
            break;
        }
        if (field->runs < TABLES_FIELD_RUNS)
        {
            // the bits are below 128, which an unsigned int stores
            field->run[field->runs].first = (unsigned int)low;
            field->run[field->runs].last = (unsigned int)high;
            field->width += (unsigned int)(high - low + 1UL);
            field->runs += 1u;
        }
        at = low_end + 1;
    }
    if (field->runs == 0u)
    {
        return 0;
    }
    const char *const equals = mmgr_text_char(at, '=', PLATFORM_TEXT_TO_TERMINATOR);
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
    const size_t copied = (length < TABLES_NAME) ? length : (TABLES_NAME - 1u);
    mmgr_bytes_copy(field->name, at, copied);
    field->name[copied] = '\0';
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
    const size_t operand_copied = (operand_length < TABLES_NAME) ? operand_length : (TABLES_NAME - 1u);
    mmgr_bytes_copy(field->operand, operand, operand_copied);
    field->operand[operand_copied] = '\0';
    // the width the line counts is the runs' where the runs are whole; a count past them is a line cut short
    return (unsigned long)field->width == count;
}

// An ENCODING line's fields added to `class_`: one field, or several joined by commas on the line's left and
// written from one operand, `BITS_5_58_54_Sc_bank,BITS_14_53_40_Sc_addr=ConstBankAddress2(Sb_bank,Sb_addr)`, where
// each field takes the operand's argument in its place, and the whole operand where it has none to give
static void tables_fields_line(const char *line, TablesClass *class_)
{
    const char *const equals = mmgr_text_char(line, '=', PLATFORM_TEXT_TO_TERMINATOR);
    if ((equals == NULL) || (!mmgr_text_starts(line, "BITS_", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        return;
    }
    const char *const operand = tables_past_spaces(equals + 1);
    // the operand's arguments, where it is a call: the text between its parentheses, split on commas
    const char *const open = mmgr_text_char(operand, '(', PLATFORM_TEXT_TO_TERMINATOR);
    const char *const close = (open != NULL) ? strrchr(operand, ')') : NULL;
    unsigned int parts = 1u;
    for (const char *at = line; at < equals; at += 1)
    {
        parts += (*at == ',') ? 1u : 0u;
    }
    const char *part = line;
    for (unsigned int index = 0u; index < parts; index += 1u)
    {
        const char *const part_end =
            (index + 1u < parts) ? mmgr_text_char(part, ',', PLATFORM_TEXT_TO_TERMINATOR) : equals;
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
        mmgr_bytes_copy(one, part, left);
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
            const char *const comma = mmgr_text_char(at, ',', PLATFORM_TEXT_TO_TERMINATOR);
            argument = at;
            argument_length = (size_t)((((comma != NULL) && (comma < close)) ? comma : close) - at);
        }
        if (length + argument_length + 2u >= sizeof(one))
        {
            return;
        }
        mmgr_bytes_copy(one + length, argument, argument_length);
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
// `UImm(5/0*):Sa_bank`, read off the digits after `Imm(`; 0 where the bracket contains none
static unsigned int tables_immediate_bits(const char *open, const char *end)
{
    for (const char *at = open; at + 4 < end; at += 1)
    {
        if (mmgr_text_starts(at, "Imm(", PLATFORM_TEXT_TO_TERMINATOR))
        {
            return (unsigned int)strtoul(at + 4, NULL, 10);
        }
    }
    return 0u;
}

// a FORMAT line containing a size operand, `/SZ_U8_S8_U16_S16_32_64("32"):sz`: the choices between `SZ_` and the
// parenthesis, each a width in bits after any characters, in the order the field counts them, and the operand's name
// after the colon
static void tables_size_read(const char *line, TablesClass *class_)
{
    const char *const size = mmgr_text_find(line, "/SZ_", PLATFORM_TEXT_TO_TERMINATOR);
    const char *const open = (size != NULL) ? mmgr_text_char(size, '(', PLATFORM_TEXT_TO_TERMINATOR) : NULL;
    const char *const close = (open != NULL) ? mmgr_text_char(open, ')', PLATFORM_TEXT_TO_TERMINATOR) : NULL;
    if ((close == NULL) || (close[1] != ':'))
    {
        return;
    }
    class_->sizes = 0u;
    // the list's name, from the slash to the parenthesis, which a condition's constant names a choice under
    const size_t list_length = (size_t)(open - (size + 1));
    const size_t list_copied = (list_length < TABLES_NAME) ? list_length : (TABLES_NAME - 1u);
    mmgr_bytes_copy(class_->size_list, size + 1, list_copied);
    class_->size_list[list_copied] = '\0';
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
        return 0ULL;
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
        {
            VerbaLinea buffer_line = mmgr_line_over(text, room);
            mmgr_line_hex(&buffer_line, low, 16u);
            mmgr_line_put(&buffer_line, " ");
            mmgr_line_hex(&buffer_line, high, 16u);
            mmgr_line_put(&buffer_line, " -");
            (void)mmgr_line_finish(&buffer_line);
        }
        return 0;
    }
    const TablesField *const bank = (class_->bank[0] != '\0') ? tables_field_by_operand(class_, class_->bank) : NULL;
    if (bank == NULL)
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(text, room);
            mmgr_line_hex(&buffer_line, low, 16u);
            mmgr_line_put(&buffer_line, " ");
            mmgr_line_hex(&buffer_line, high, 16u);
            mmgr_line_put(&buffer_line, " ");
            mmgr_line_put(&buffer_line, class_->name);
            (void)mmgr_line_finish(&buffer_line);
        }
        return 1;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(text, room);
        mmgr_line_hex(&buffer_line, low, 16u);
        mmgr_line_put(&buffer_line, " ");
        mmgr_line_hex(&buffer_line, high, 16u);
        mmgr_line_put(&buffer_line, " ");
        mmgr_line_put(&buffer_line, class_->name);
        mmgr_line_put(&buffer_line, " bank ");
        mmgr_line_decimal(&buffer_line, tables_field_read(bank, low, high), 1u);
        mmgr_line_put(&buffer_line, " offset 0x");
        mmgr_line_hex(&buffer_line, tables_class_offset(class_, low, high), 1u);
        mmgr_line_put(&buffer_line, " reads ");
        mmgr_line_decimal(&buffer_line, tables_class_bytes(class_, low, high), 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
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

// a FORMAT line containing a constant-bank operand, `C:<name>[...:<bank>]...[...:<offset>]`: the bank the name
// after the first bracket's last colon, the offset the name after the last bracket's last colon, and the offset
// operand's width the immediate's inside that bracket
static void tables_constant_read(const char *line, TablesClass *class_)
{
    const char *const constant = mmgr_text_find(line, "C:", PLATFORM_TEXT_TO_TERMINATOR);
    if ((constant == NULL) ||
        (constant != line && (constant[-1] != '\'') && (constant[-1] != ' ') && (constant[-1] != ',')))
    {
        return;
    }
    const char *const first_open = mmgr_text_char(constant, '[', PLATFORM_TEXT_TO_TERMINATOR);
    const char *const first_close =
        (first_open != NULL) ? mmgr_text_char(first_open, ']', PLATFORM_TEXT_TO_TERMINATOR) : NULL;
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
    unsigned long long at = 0ULL;
    TablesClass *class_ = NULL;
    enum
    {
        ELSEWHERE,
        FORMAT,
        OPCODES,
        ENCODING,
        CONDITIONS
    } part = ELSEWHERE;
    // within CONDITIONS, whether the conditions read are under a type on the registers, OOR_REG_ERROR or
    // MISALIGNED_REG_ERROR
    int register_type = 0;
    while (line_read(text, size, &at, line, sizeof(line)))
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
            mmgr_bytes_set(class_, 0, sizeof(*class_));
            mmgr_bytes_copy(class_->name, name, sizeof(name));
            part = ELSEWHERE;
            continue;
        }
        if (class_ == NULL)
        {
            continue;
        }
        const size_t length = mmgr_text_length(line, PLATFORM_TEXT_TO_TERMINATOR);
        if (mmgr_text_starts(line, "FORMAT", PLATFORM_TEXT_TO_TERMINATOR))
        {
            part = FORMAT;
        }
        else if (mmgr_text_equal(line, "OPCODES", PLATFORM_TEXT_TO_TERMINATOR))
        {
            part = OPCODES;
            continue;
        }
        else if (mmgr_text_equal(line, "ENCODING", PLATFORM_TEXT_TO_TERMINATOR))
        {
            part = ENCODING;
            continue;
        }
        else if (mmgr_text_equal(line, "CONDITIONS", PLATFORM_TEXT_TO_TERMINATOR))
        {
            part = CONDITIONS;
            register_type = 0;
            continue;
        }
        else if ((line[0] >= 'A') && (line[0] <= 'Z') && (strspn(line, "ABCDEFGHIJKLMNOPQRSTUVWXYZ_") == length))
        {
            // within CONDITIONS, a type of error the conditions after it are under; elsewhere another part's head,
            // PROPERTIES, PREDICATES: capitals and underscores alone
            if ((part == CONDITIONS) && (length > 6u) &&
                (mmgr_text_equal(line + length - 6u, "_ERROR", PLATFORM_TEXT_TO_TERMINATOR)))
            {
                register_type = (mmgr_text_find(line, "REG_ERROR", PLATFORM_TEXT_TO_TERMINATOR) != NULL);
                continue;
            }
            part = ELSEWHERE;
            continue;
        }
        if (part == CONDITIONS)
        {
            // a condition is a line ending in a colon, under a type on the registers. The line after it, in quotes, is
            // its message, and a condition longer than its room is not stored
            const char *const text_at = tables_past_spaces(line);
            size_t copied = mmgr_text_length(text_at, PLATFORM_TEXT_TO_TERMINATOR);
            while ((copied != 0u) && ((text_at[copied - 1u] == ' ') || (text_at[copied - 1u] == ':')))
            {
                copied -= 1u;
            }
            if (register_type && (copied != 0u) &&
                (text_at[mmgr_text_length(text_at, PLATFORM_TEXT_TO_TERMINATOR) - 1u] == ':') &&
                (copied + 1u < TABLES_CONDITION_TEXT) && (class_->conditions < TABLES_CONDITIONS))
            {
                mmgr_bytes_copy(class_->condition[class_->conditions], text_at, copied);
                class_->condition[class_->conditions][copied] = '\0';
                class_->conditions += 1u;
            }
            continue;
        }
        if (part == FORMAT)
        {
            tables_constant_read(line, class_);
            tables_size_read(line, class_);
            if (line[mmgr_text_length(line, PLATFORM_TEXT_TO_TERMINATOR) -
                     ((mmgr_text_length(line, PLATFORM_TEXT_TO_TERMINATOR) != 0u) ? 1u : 0u)] == ';')
            {
                part = ELSEWHERE;
            }
        }
        else if (part == OPCODES)
        {
            unsigned long long opcode = 0ULL;
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
        if (mmgr_text_equal(class_->field[field].name, name, PLATFORM_TEXT_TO_TERMINATOR))
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
        if (!class_->field[field].fixed &&
            (mmgr_text_equal(class_->field[field].operand, operand, PLATFORM_TEXT_TO_TERMINATOR)))
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
    const unsigned long long mask = (width >= 64u) ? ~0ULL : ((1ULL << width) - 1ULL);
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
        const unsigned long long one = (value >> (bit - first)) & 1ULL;
        if (bit < 64u)
        {
            *low = (*low & ~(1ULL << bit)) | (one << bit);
        }
        else
        {
            *high = (*high & ~(1ULL << (bit - 64u))) | (one << (bit - 64u));
        }
    }
}

unsigned long long tables_field_read(const TablesField *field, unsigned long long low, unsigned long long high)
{
    unsigned long long value = 0ULL;
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
        const unsigned long long mask = (width >= 64u) ? ~0ULL : ((1ULL << width) - 1ULL);
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
#define TABLES_REGISTER_ZERO 255LL
#define TABLES_UNIFORM_ZERO 63LL
#define TABLES_PREDICATE_TRUE 7LL
#define TABLES_REGISTER_COUNT 256LL
#define TABLES_UNIFORM_COUNT 64LL

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
    const size_t length = mmgr_text_length(word, PLATFORM_TEXT_TO_TERMINATOR);
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
// name, a choice of the class's size list by its place in the list, and anything else left undecided
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
        return 0LL;
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
    if (mmgr_text_equal(list, "Register", PLATFORM_TEXT_TO_TERMINATOR))
    {
        if (mmgr_text_equal(value, "RZ", PLATFORM_TEXT_TO_TERMINATOR))
        {
            return TABLES_REGISTER_ZERO;
        }
        if (value[0] == 'R')
        {
            return strtoll(value + 1, NULL, 10);
        }
    }
    if (mmgr_text_equal(list, "UniformRegister", PLATFORM_TEXT_TO_TERMINATOR))
    {
        if (mmgr_text_equal(value, "URZ", PLATFORM_TEXT_TO_TERMINATOR))
        {
            return TABLES_UNIFORM_ZERO;
        }
        if (mmgr_text_starts(value, "UR", PLATFORM_TEXT_TO_TERMINATOR))
        {
            return strtoll(value + 2, NULL, 10);
        }
    }
    if (((mmgr_text_equal(list, "Predicate", PLATFORM_TEXT_TO_TERMINATOR)) &&
         (mmgr_text_equal(value, "PT", PLATFORM_TEXT_TO_TERMINATOR))) ||
        ((mmgr_text_equal(list, "UniformPredicate", PLATFORM_TEXT_TO_TERMINATOR)) &&
         (mmgr_text_equal(value, "UPT", PLATFORM_TEXT_TO_TERMINATOR))))
    {
        return TABLES_PREDICATE_TRUE;
    }
    if ((condition->class_->size_list[0] != '\0') &&
        (mmgr_text_equal(list, condition->class_->size_list, PLATFORM_TEXT_TO_TERMINATOR)))
    {
        // the choice's place in the list, counted from the first underscore past the list's prefix: the size field
        // stores that place
        const char *choice = mmgr_text_char(list, '_', PLATFORM_TEXT_TO_TERMINATOR);
        long long place = 0LL;
        while (choice != NULL)
        {
            choice += 1;
            const char *const end = mmgr_text_char(choice, '_', PLATFORM_TEXT_TO_TERMINATOR);
            const size_t choice_length =
                (end != NULL) ? (size_t)(end - choice) : mmgr_text_length(choice, PLATFORM_TEXT_TO_TERMINATOR);
            if ((choice_length == mmgr_text_length(value, PLATFORM_TEXT_TO_TERMINATOR)) &&
                (strncmp(choice, value, choice_length) == 0))
            {
                return place;
            }
            place += 1LL;
            choice = end;
        }
    }
    condition->unjudged = 1;
    return 0LL;
}

static long long tables_condition_implies(TablesCondition *condition);

// a primary: a parenthesized condition, a number, a constant after a backquote, a register count after a percent,
// or an operand of the class read off the encoding; anything else is left undecided
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
        return 0LL;
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
        return 0LL;
    }
    // a field stores at max 64 bits, read as the number it stores
    return (long long)tables_field_read(field, condition->low, condition->high);
}

static long long tables_condition_unary(TablesCondition *condition)
{
    tables_condition_spaces(condition);
    if ((*condition->at == '!') && (condition->at[1] != '='))
    {
        condition->at += 1;
        return (tables_condition_unary(condition) == 0LL) ? 1LL : 0LL;
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
            value = (divisor != 0LL) ? (value % divisor) : 0LL;
            condition->unjudged = condition->unjudged || (divisor == 0LL);
        }
        else if (*condition->at == '/')
        {
            condition->at += 1;
            const long long divisor = tables_condition_unary(condition);
            value = (divisor != 0LL) ? (value / divisor) : 0LL;
            condition->unjudged = condition->unjudged || (divisor == 0LL);
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
        return (left == tables_condition_sum(condition)) ? 1LL : 0LL;
    }
    if (tables_condition_word(condition, "!="))
    {
        return (left != tables_condition_sum(condition)) ? 1LL : 0LL;
    }
    if (tables_condition_word(condition, "<="))
    {
        return (left <= tables_condition_sum(condition)) ? 1LL : 0LL;
    }
    if (tables_condition_word(condition, ">="))
    {
        return (left >= tables_condition_sum(condition)) ? 1LL : 0LL;
    }
    if ((*condition->at == '<') && (condition->at[1] != '<'))
    {
        condition->at += 1;
        return (left < tables_condition_sum(condition)) ? 1LL : 0LL;
    }
    if ((*condition->at == '>') && (condition->at[1] != '>'))
    {
        condition->at += 1;
        return (left > tables_condition_sum(condition)) ? 1LL : 0LL;
    }
    return left;
}

static long long tables_condition_both(TablesCondition *condition)
{
    long long value = tables_condition_compared(condition);
    while (tables_condition_word(condition, "&&"))
    {
        const long long right = tables_condition_compared(condition);
        value = ((value != 0LL) && (right != 0LL)) ? 1LL : 0LL;
    }
    return value;
}

static long long tables_condition_either(TablesCondition *condition)
{
    long long value = tables_condition_both(condition);
    while (tables_condition_word(condition, "||"))
    {
        const long long right = tables_condition_both(condition);
        value = ((value != 0LL) || (right != 0LL)) ? 1LL : 0LL;
    }
    return value;
}

// `left -> right`: the right is true where the left is, and the whole is true where the left is not
static long long tables_condition_implies(TablesCondition *condition)
{
    const long long left = tables_condition_either(condition);
    if (tables_condition_word(condition, "->"))
    {
        const long long right = tables_condition_implies(condition);
        return ((left == 0LL) || (right != 0LL)) ? 1LL : 0LL;
    }
    return left;
}

int tables_class_legal(const TablesClass *class_, unsigned long long low, unsigned long long high, const char **failed)
{
    for (unsigned int at = 0u; at < class_->conditions; at += 1u)
    {
        TablesCondition condition = {class_->condition[at], class_, low, high, 0};
        const long long implied = tables_condition_implies(&condition);
        tables_condition_spaces(&condition);
        // text left over is a condition this reader did not read whole, and does not judge
        if (*condition.at != '\0')
        {
            condition.unjudged = 1;
        }
        if (!condition.unjudged && (implied == 0LL))
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
