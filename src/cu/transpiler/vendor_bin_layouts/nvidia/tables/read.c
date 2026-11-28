// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.c: the vendor's instruction set read: each CLASS with its opcode, its fields by name and its constant-bank
// operand
#include "read.h"
#include "../../../../../../includes/platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#include "../../../../../../includes/parsers/line_read.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int read_text_order(const char *left, const char *right, size_t cap)
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

static const char *tables_past_spaces(const char *at)
{
    return at + EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \t");
}

static int tables_set_grown(TablesSet *set)
{
    if (set->count < set->room)
    {
        return 1;
    }
    const unsigned int room = (set->room == 0u) ? 64u : (set->room * 2u);
    TablesClass *const grown =
        (TablesClass *)platform_cell_grown(set->class_, (size_t)set->room * sizeof(TablesClass),
                                           (size_t)room * sizeof(TablesClass), platform_place_g_program_owner, 0);
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
    if (read_text_order(line, "CLASS \"", (size_t)(7u)) != 0)
    {
        return 0;
    }
    const char *const end =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line + 7, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '"');
    if (end == NULL)
    {
        return 0;
    }
    const size_t length = (size_t)(end - (line + 7));
    const size_t copied = (length < room) ? length : (room - 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = name, .src = line + 7, .bytes = copied);
    name[copied] = '\0';
    return 1;
}

// the opcode of an OPCODES line, `<name> = 0b<bits>;`, the bits read as they are
static int tables_opcode_read(const char *line, unsigned long long *opcode)
{
    const char *const at = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                      .other = "0b", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
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
    if (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "BITS_",
                    .ci = EMBED_FALSE))
    {
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = field, .bytes = sizeof(*field), .val = 0);
    const char *at = line + 5;
    const char *end = NULL;
    // the count, then pairs of numbers while a number follows the underscore
    const unsigned long count = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = at, .end = &end, .base = 10);
    if ((end == at) || (*end != '_'))
    {
        return 0;
    }
    at = end + 1;
    for (;;)
    {
        const char *high_end = NULL;
        const unsigned long high =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = at, .end = &high_end, .base = 10);
        if ((high_end == at) || (*high_end != '_'))
        {
            break;
        }
        const char *low_end = NULL;
        const unsigned long low =
            EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = high_end + 1, .end = &low_end, .base = 10);
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
    const char *const equals =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '=');
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
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = field->name, .src = at, .bytes = copied);
    field->name[copied] = '\0';
    const char *operand = tables_past_spaces(equals + 1);
    if (*operand == '*')
    {
        field->fixed = 1;
        field->value = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = operand + 1, .end = NULL, .base = 0);
    }
    size_t operand_length =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = operand, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ";");
    while ((operand_length != 0u) && (operand[operand_length - 1u] == ' '))
    {
        operand_length -= 1u;
    }
    const size_t operand_copied = (operand_length < TABLES_NAME) ? operand_length : (TABLES_NAME - 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = field->operand, .src = operand, .bytes = operand_copied);
    field->operand[operand_copied] = '\0';
    // the width the line counts is the runs' where the runs are whole; a count past them is a line cut short
    return (unsigned long)field->width == count;
}

// The fields of an ENCODING line, added to `class_`. That is one field, or several joined by commas on the left of
// the line and written from one operand, as
// `BITS_5_58_54_Sc_bank,BITS_14_53_40_Sc_addr=ConstBankAddress2(Sb_bank,Sb_addr)`. There, each field takes the argument
// of the operand in its place, and the whole operand where it has none to give
static void tables_fields_line(const char *line, TablesClass *class_)
{
    const char *const equals =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '=');
    if ((equals == NULL) || (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = line,
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "BITS_", .ci = EMBED_FALSE)))
    {
        return;
    }
    const char *const operand = tables_past_spaces(equals + 1);
    // the operand's arguments, where it is a call: the text between its parentheses, split on commas
    const char *const open =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = operand, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '(');
    const char *const close = (open != NULL) ? EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = operand,
                                                          .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ')')
                                             : NULL;
    unsigned int parts = 1u;
    for (const char *at = line; at < equals; at += 1)
    {
        parts += (*at == ',') ? 1u : 0u;
    }
    const char *part = line;
    for (unsigned int index = 0u; index < parts; index += 1u)
    {
        const char *const part_end = (index + 1u < parts) ? EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = part,
                                                                       .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ',')
                                                          : equals;
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
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = one, .src = part, .bytes = left);
        size_t length = left;
        one[length] = '=';
        length += 1u;
        // the argument in this field's place, or the whole operand
        const char *argument = operand;
        size_t argument_length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = operand,
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ";");
        if ((parts > 1u) && (open != NULL) && (close != NULL) && (close > open))
        {
            const char *at = open + 1;
            for (unsigned int skipped = 0u; (skipped < index) && (at < close); at += 1)
            {
                skipped += (*at == ',') ? 1u : 0u;
            }
            const char *const comma =
                EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ',');
            argument = at;
            argument_length = (size_t)((((comma != NULL) && (comma < close)) ? comma : close) - at);
        }
        if (length + argument_length + 2u >= sizeof(one))
        {
            return;
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = one + length, .src = argument, .bytes = argument_length);
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
    // Explicit cast reads the byte as its code point
    while (
        (at + length < end) && (length + 1u < TABLES_NAME) &&
        (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM, .byte = (uint8_t)at[length]) || (at[length] == '_')))
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
        if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "Imm(",
                       .ci = EMBED_FALSE))
        {
            return (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = at + 4, .end = NULL, .base = 10);
        }
    }
    return 0u;
}

// a FORMAT line that holds a size operand, `/SZ_U8_S8_U16_S16_32_64("32"):sz`. It has the choices between `SZ_` and
// the parenthesis, each a width in bits after any characters, in the order that the field counts them. Then the name
// of the operand after the colon
static void tables_size_read(const char *line, TablesClass *class_)
{
    const char *const size = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                        .other = "/SZ_", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    const char *const open = (size != NULL) ? EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = size,
                                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '(')
                                            : NULL;
    const char *const close = (open != NULL) ? EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = open,
                                                          .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ')')
                                             : NULL;
    if ((close == NULL) || (close[1] != ':'))
    {
        return;
    }
    class_->sizes = 0u;
    // the list's name, from the slash to the parenthesis, which a condition's constant names a choice under
    const size_t list_length = (size_t)(open - (size + 1));
    const size_t list_copied = (list_length < TABLES_NAME) ? list_length : (TABLES_NAME - 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = class_->size_list, .src = size + 1, .bytes = list_copied);
    class_->size_list[list_copied] = '\0';
    for (const char *at = size + 4; (at < open) && (class_->sizes < TABLES_SIZES);)
    {
        // the size up to its underscore, and its digits past the letters before them. Explicit casts take the bytes
        // before the parenthesis and before the underscore as counts, which never run back
        const char *const end =
            at + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = at, .cap = (size_t)(open - at), .other = "_");
        const char *const digits = at + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = at,
                                                   .cap = (size_t)(end - at), .other = "0123456789");
        class_->size_bytes[class_->sizes] =
            (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = digits, .end = NULL, .base = 10) / 8u;
        class_->sizes += 1u;
        at = end + 1;
    }
    // the size's name up to its space or its semicolon, cut to the room
    const size_t length =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = close + 2, .cap = TABLES_NAME - 1u, .other = " ;");
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = class_->size, .src = close + 2, .bytes = length);
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

// a FORMAT line that holds a constant-bank operand, `C:<name>[...:<bank>]...[...:<offset>]`. The bank is the name after
// the last colon of the first bracket. The offset is the name after the last colon of the last bracket. The width of
// the offset operand is that of the immediate inside that bracket
static void tables_constant_read(const char *line, TablesClass *class_)
{
    const char *const constant =
        EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "C:", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    if ((constant == NULL) ||
        (constant != line && (constant[-1] != '\'') && (constant[-1] != ' ') && (constant[-1] != ',')))
    {
        return;
    }
    const char *const first_open =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = constant, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '[');
    const char *const first_close = (first_open != NULL) ? EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = first_open,
                                                                      .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ']')
                                                         : NULL;
    if (first_close == NULL)
    {
        return;
    }
    tables_name_before(first_open, first_close, class_->bank);
    const char *const last_close = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = first_close,
                                              .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ']');
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
            EMBED_CALL(memor.set, MemoriaCfg, .dst = class_, .bytes = sizeof(*class_), .val = 0);
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = class_->name, .src = name, .bytes = sizeof(name));
            part = ELSEWHERE;
            continue;
        }
        if (class_ == NULL)
        {
            continue;
        }
        const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = "FORMAT", .ci = EMBED_FALSE))
        {
            part = FORMAT;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "OPCODES", .ci = EMBED_FALSE))
        {
            part = OPCODES;
            continue;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "ENCODING", .ci = EMBED_FALSE))
        {
            part = ENCODING;
            continue;
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "CONDITIONS", .ci = EMBED_FALSE))
        {
            part = CONDITIONS;
            register_type = 0;
            continue;
        }
        // Explicit cast reads the byte as its code point
        else if (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_UPPER, .byte = (uint8_t)line[0]) &&
                 (EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "ABCDEFGHIJKLMNOPQRSTUVWXYZ_") == length))
        {
            // within CONDITIONS, a type of error the conditions after it are under; elsewhere another part's head,
            // PROPERTIES, PREDICATES: capitals and underscores alone
            if ((part == CONDITIONS) && (length > 6u) &&
                (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = line + length - 6u, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "_ERROR", .ci = EMBED_FALSE)))
            {
                register_type = (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line,
                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "REG_ERROR",
                                            .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL);
                continue;
            }
            part = ELSEWHERE;
            continue;
        }
        if (part == CONDITIONS)
        {
            // a condition is a line that ends in a colon, under a type on the registers. The line after it, in
            // quotes, is its message. The reader does not store a condition longer than its room
            const char *const text_at = tables_past_spaces(line);
            size_t copied = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text_at, .cap = PLATFORM_TEXT_TO_TERMINATOR);
            while ((copied != 0u) && ((text_at[copied - 1u] == ' ') || (text_at[copied - 1u] == ':')))
            {
                copied -= 1u;
            }
            if (register_type && (copied != 0u) &&
                (text_at[EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text_at, .cap = PLATFORM_TEXT_TO_TERMINATOR) -
                         1u] == ':') &&
                (copied + 1u < TABLES_CONDITION_TEXT) && (class_->conditions < TABLES_CONDITIONS))
            {
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = class_->condition[class_->conditions], .src = text_at,
                           .bytes = copied);
                class_->condition[class_->conditions][copied] = '\0';
                class_->conditions += 1u;
            }
            continue;
        }
        if (part == FORMAT)
        {
            tables_constant_read(line, class_);
            tables_size_read(line, class_);
            if (line[EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR) -
                     ((EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR) != 0u)
                          ? 1u
                          : 0u)] == ';')
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
    platform_cell_released(set->class_, platform_place_g_program_owner);
    set->class_ = NULL;
    set->count = 0u;
    set->room = 0u;
}

const TablesField *tables_field_named(const TablesClass *class_, const char *name)
{
    for (unsigned int field = 0u; field < class_->fields; field += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = class_->field[field].name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = name, .ci = EMBED_FALSE))
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
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = class_->field[field].operand,
                        .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = operand, .ci = EMBED_FALSE)))
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
    // the low run takes the low bits of the value, because the table writes the runs last first
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

// A condition read: the text walked, and the class and the encoding that the reader judges it over. Then whether a
// thing that it names is past the judgment of this reader. The reader does not judge a condition that names one such
// thing
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

// 1 where the text at the place of the condition starts with `word`, and the call then steps over it
static int tables_condition_word(TablesCondition *condition, const char *word)
{
    tables_condition_spaces(condition);
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if (read_text_order(condition->at, word, (size_t)(length)) != 0)
    {
        return 0;
    }
    condition->at += length;
    return 1;
}

static int tables_condition_name_char(char character)
{
    // Explicit cast reads the byte as its code point
    return EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALNUM, .byte = (uint8_t)character) || (character == '_');
}

// a constant after the backquote, `` `Register@RZ ``, `` `SZ_U8_S8_U16_S16_32_64@"64" ``. It is the own constant of the
// registers by name, or a choice of the size list of the class by its list place. The reader leaves all else undecided
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
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = list, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "Register",
                   .ci = EMBED_FALSE))
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = value, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "RZ",
                       .ci = EMBED_FALSE))
        {
            return TABLES_REGISTER_ZERO;
        }
        if (value[0] == 'R')
        {
            // Explicit cast widens the signed word of the read to the long long of the constant
            return (long long)EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = value + 1, .end = NULL);
        }
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = list, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "UniformRegister", .ci = EMBED_FALSE))
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = value, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "URZ",
                       .ci = EMBED_FALSE))
        {
            return TABLES_UNIFORM_ZERO;
        }
        if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = value, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "UR",
                       .ci = EMBED_FALSE))
        {
            // Explicit cast widens the signed word of the read to the long long of the constant
            return (long long)EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = value + 2, .end = NULL);
        }
    }
    if (((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = list, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "Predicate",
                     .ci = EMBED_FALSE)) &&
         (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = value, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "PT",
                     .ci = EMBED_FALSE))) ||
        ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = list, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                     .other = "UniformPredicate", .ci = EMBED_FALSE)) &&
         (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = value, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "UPT",
                     .ci = EMBED_FALSE))))
    {
        return TABLES_PREDICATE_TRUE;
    }
    if ((condition->class_->size_list[0] != '\0') &&
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = list, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = condition->class_->size_list, .ci = EMBED_FALSE)))
    {
        // the choice's place in the list, counted from the first underscore past the list's prefix: the size field
        // stores that place
        const char *choice =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = list, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '_');
        long long place = 0LL;
        while (choice != NULL)
        {
            choice += 1;
            const char *const end =
                EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = choice, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '_');
            const size_t choice_length = (end != NULL) ? (size_t)(end - choice)
                                                       : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = choice,
                                                                    .cap = PLATFORM_TEXT_TO_TERMINATOR);
            if ((choice_length ==
                 EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = value, .cap = PLATFORM_TEXT_TO_TERMINATOR)) &&
                (read_text_order(choice, value, (size_t)(choice_length)) == 0))
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

// a primary: a parenthesized condition, a number, a constant after a backquote, or a register count after a percent.
// Else it is an operand of the class, read from the encoding. The reader leaves all else undecided
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
    // Explicit cast reads the byte as its code point
    if (EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)*condition->at))
    {
        const char *end = NULL;
        // The text opens with a digit, and so the read has no sign. Explicit cast carries the unsigned word to the
        // long long of the constant
        const long long value =
            (long long)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = condition->at, .end = &end, .base = 0u);
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
        // a name that no field of the class comes from, or no name at all where the line must have one
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
