// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_atoms.c: an atom of a pattern read: a character, `.`, `^`, `$`, a class, a group, a lookahead, a boundary,
// a backreference, or an escape, which is a class item or one character
#include "regexp_read_internal.h"

// what an escape read is: a class item, one character, or a failure reported
#define REGEXP_ESCAPE_ITEM 0

#define REGEXP_ESCAPE_CHARACTER 1

#define REGEXP_ESCAPE_FAILED 2

typedef struct
{
    RegexpReader *reader;
    unsigned int count;
    unsigned int *character;
    int in_class;
    RegexpItem *item;
    RegexpItem *items;
    unsigned int *count_pointer;
    RegexpItem item_value;
    unsigned int type;
    unsigned int child;
    int negated;
    unsigned int group;
} RegexpAtomsCtx;
// `count` hex digits where the read is, into `character`: 1, or 0 with the read where it was
EMBED_INLINE int regexp_hex(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    unsigned int *character = args->character;
    const unsigned long long start = reader->at;
    unsigned int value = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        unsigned int digit = 0u;
        const int there = EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx,
                                     .reader_read = reader, .character = &digit);
        const int decimal = there && (digit >= '0') && (digit <= '9');
        const int lower = there && (digit >= 'a') && (digit <= 'f');
        const int upper = there && (digit >= 'A') && (digit <= 'F');
        if (!decimal && !lower && !upper)
        {
            reader->at = start;
            return 0;
        }
        const unsigned int character_value = lower ? (digit - 'a' + 10u) : (digit - 'A' + 10u);
        value = (value << 4u) | (decimal ? (digit - '0') : character_value);
        reader->at += 1u;
    }
    *character = value;
    return 1;
}

// a `\u` escape after its `u`, four hex digits or a code in braces, into `character`: REGEXP_ESCAPE_CHARACTER, `u`
// where it names no character, or REGEXP_ESCAPE_FAILED with why given
EMBED_INLINE int regexp_escape_code(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    unsigned int *character = args->character;
    if (!EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = '{'))
    {
        const int read = EMBED_CALL(regexp_hex, RegexpAtomsCtx, .reader = reader, .count = 4u, .character = character);
        *character = (read && !((*character >= 0xd800u) && (*character <= 0xdfffu))) ? *character : 'u';
        return REGEXP_ESCAPE_CHARACTER;
    }
    unsigned long long code = 0ULL;
    unsigned long long digits = 0ULL;
    unsigned int digit = 0u;
    while (EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx, .reader_read = reader,
                      .character = &digit) &&
           (((digit >= '0') && (digit <= '9')) || ((digit >= 'a') && (digit <= 'f')) ||
            ((digit >= 'A') && (digit <= 'F'))))
    {
        code = (code << 4u) | ((digit <= '9') ? (digit - '0') : ((digit | 0x20u) - 'a' + 10u));
        digits += 1ULL;
        reader->at += 1u;
    }
    if (!EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = '}'))
    {
        EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader, .why = "an unclosed \\u{");
        return REGEXP_ESCAPE_FAILED;
    }
    // a code past what a character represents, or none, reads as `u`
    const int valid =
        (digits != 0ULL) && (digits <= 6ULL) && (code <= 0x10ffffULL) && !((code >= 0xd800ULL) && (code <= 0xdfffULL));
    *character = valid ? (unsigned int)code : 'u';
    return REGEXP_ESCAPE_CHARACTER;
}

// the property the `count` characters from `character` name, by its short name or its long one: its place, 8 where the
// reader knows none of that name
EMBED_INLINE unsigned int regexp_property_named(const RegexpAtomsCtx *args)
{
    static const char *const s_properties[8] = {"L",  "Character",           "N",  "Number",
                                                "Lu", "Uppercase_Character", "Ll", "Lowercase_Character"};
    const unsigned int *const characters = args->character;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        const embed_word name_length = cellul.len(&(CatenaFinitaCfg){.src = s_properties[at], .cap = REGEXP_TEXT_CAP});
        int same = name_length == args->count;
        embed_word character_at = 0u;
        while (same && (character_at < name_length))
        {
            same = characters[character_at] == (unsigned char)s_properties[at][character_at];
            character_at += 1u;
        }
        if (same)
        {
            return at / 2u;
        }
    }
    return 8u;
}

// a `\p{..}` or `\P{..}` escape after its character, the property named and whether the escape is its opposite
// into `item`: REGEXP_ESCAPE_ITEM, or REGEXP_ESCAPE_FAILED with why given
EMBED_INLINE int regexp_escape_property(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    if (!EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = '{'))
    {
        EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                   .why = "a property with no name");
        return REGEXP_ESCAPE_FAILED;
    }
    const unsigned long long start = reader->at;
    unsigned int named = 0u;
    while (EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx, .reader_read = reader,
                      .character = &named) &&
           (named != '}'))
    {
        reader->at += 1u;
    }
    // a property's name is a few characters
    const unsigned int length = (unsigned int)(reader->at - start);
    if (!EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = '}'))
    {
        EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                   .why = "an unclosed property");
        return REGEXP_ESCAPE_FAILED;
    }
    const unsigned int property =
        EMBED_CALL(regexp_property_named, RegexpAtomsCtx, .character = &reader->characters[start], .count = length);
    if (property == 8u)
    {
        EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                   .why = "a property not read here");
        return REGEXP_ESCAPE_FAILED;
    }
    *args->item = (RegexpItem){REGEXP_PROPERTY, property, 0u, args->negated};
    return REGEXP_ESCAPE_ITEM;
}

// a `\c` escape after its `c`, a character whose place in the alphabet is the control character, into `character`:
// REGEXP_ESCAPE_CHARACTER, or REGEXP_ESCAPE_FAILED with why given
EMBED_INLINE int regexp_escape_control(const RegexpAtomsCtx *args)
{
    unsigned int control = 0u;
    if (!EMBED_CALL(regexp_read_character_internal.next, RegexpReadLetterCtx, .reader = args->reader,
                    .character = &control) ||
        !(((control | 0x20u) >= 'a') && ((control | 0x20u) <= 'z')))
    {
        EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = args->reader,
                   .why = "a control escape");
        return REGEXP_ESCAPE_FAILED;
    }
    *args->character = control % 32u;
    return REGEXP_ESCAPE_CHARACTER;
}

// An escape after its backslash, into `item` or `character`: REGEXP_ESCAPE_ITEM, REGEXP_ESCAPE_CHARACTER, or
// REGEXP_ESCAPE_FAILED with why given
static int regexp_escape(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    RegexpItem *item = args->item;
    unsigned int *character = args->character;
    unsigned int escaped = 0u;
    if (!EMBED_CALL(regexp_read_character_internal.next, RegexpReadLetterCtx, .reader = reader, .character = &escaped))
    {
        EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                   .why = "a backslash at the end");
        return REGEXP_ESCAPE_FAILED;
    }
    static const char s_classes[6] = {'d', 'D', 'w', 'W', 's', 'S'};
    static const unsigned int s_types[6] = {REGEXP_DIGIT, REGEXP_DIGIT, REGEXP_WORD,
                                            REGEXP_WORD,  REGEXP_SPACE, REGEXP_SPACE};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (escaped == (unsigned char)s_classes[at])
        {
            *item = (RegexpItem){s_types[at], 0u, 0u, (at % 2u) == 1u};
            return REGEXP_ESCAPE_ITEM;
        }
    }
    static const char s_named[6] = {'t', 'n', 'r', 'f', 'v', '0'};
    static const unsigned int s_characters[6] = {'\t', '\n', '\r', 0x0cu, 0x0bu, 0u};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (escaped == (unsigned char)s_named[at])
        {
            *character = s_characters[at];
            return REGEXP_ESCAPE_CHARACTER;
        }
    }
    *character = escaped;
    if ((escaped == 'b') && args->in_class)
    {
        *character = 8u;
    }
    else if (escaped == 'x')
    {
        *character = EMBED_CALL(regexp_hex, RegexpAtomsCtx, .reader = reader, .count = 2u, .character = character)
                         ? *character
                         : 'x';
    }
    else if (escaped == 'u')
    {
        return EMBED_CALL(regexp_escape_code, RegexpAtomsCtx, .reader = reader, .character = character);
    }
    else if (escaped == 'c')
    {
        return EMBED_CALL(regexp_escape_control, RegexpAtomsCtx, .reader = reader, .character = character);
    }
    else if (((escaped == 'p') || (escaped == 'P')) && reader->unicode)
    {
        return EMBED_CALL(regexp_escape_property, RegexpAtomsCtx, .reader = reader, .item = item,
                          .negated = escaped == 'P');
    }
    else if (escaped == 'k')
    {
        EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                   .why = "an escape not read here");
        return REGEXP_ESCAPE_FAILED;
    }
    return REGEXP_ESCAPE_CHARACTER;
}

// `item` added to the class's items: 1, or 0 with why given where a class contains as many as it can
EMBED_INLINE int regexp_item_add(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    unsigned int *count = args->count_pointer;
    if (*count >= 256u)
    {
        EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                   .why = "more items than a class allows");
        return 0;
    }
    args->items[*count] = args->item_value;
    *count += 1u;
    return 1;
}

// A range of a class from `count` after its `-`, its high end read and the range put among `items`. Where the high
// end is an escape's item, the low end, the `-` and the item each go in as one. 1, or 0 with why given
EMBED_INLINE int regexp_class_range(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    const unsigned int low = args->count;
    unsigned int high = 0u;
    RegexpItem item;
    EMBED_CALL(regexp_read_character_internal.next, RegexpReadLetterCtx, .reader = reader, .character = &high);
    const int escape = (high == '\\') ? EMBED_CALL(regexp_escape, RegexpAtomsCtx, .reader = reader, .in_class = 1,
                                                   .item = &item, .character = &high)
                                      : REGEXP_ESCAPE_CHARACTER;
    if (escape == REGEXP_ESCAPE_FAILED)
    {
        return 0;
    }
    if (escape == REGEXP_ESCAPE_ITEM)
    {
        return EMBED_CALL(regexp_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                          .count_pointer = args->count_pointer,
                          .item_value = (RegexpItem){REGEXP_RANGE, low, low, 0}) &&
               EMBED_CALL(regexp_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                          .count_pointer = args->count_pointer,
                          .item_value = (RegexpItem){REGEXP_RANGE, '-', '-', 0}) &&
               EMBED_CALL(regexp_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                          .count_pointer = args->count_pointer, .item_value = item);
    }
    if (high < low)
    {
        (void)EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                         .why = "a range out of order");
        return 0;
    }
    return EMBED_CALL(regexp_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                      .count_pointer = args->count_pointer, .item_value = (RegexpItem){REGEXP_RANGE, low, high, 0});
}

// a member of a class from its first character `count`: a character, a range, or an escape's item, put among `items`;
// 1, or 0 with why given
EMBED_INLINE int regexp_class_member(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    unsigned int low = args->count;
    if (low == '\\')
    {
        RegexpItem item;
        const int escape = EMBED_CALL(regexp_escape, RegexpAtomsCtx, .reader = reader, .in_class = 1, .item = &item,
                                      .character = &low);
        if (escape != REGEXP_ESCAPE_CHARACTER)
        {
            return (escape == REGEXP_ESCAPE_ITEM) &&
                   EMBED_CALL(regexp_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                              .count_pointer = args->count_pointer, .item_value = item);
        }
    }
    unsigned int peeked = 0u;
    const int ranged = EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx,
                                  .reader_read = reader, .character = &peeked) &&
                       (peeked == '-') && ((reader->at + 1u) < reader->length) &&
                       (reader->characters[reader->at + 1u] != ']');
    if (!ranged)
    {
        return EMBED_CALL(regexp_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                          .count_pointer = args->count_pointer, .item_value = (RegexpItem){REGEXP_RANGE, low, low, 0});
    }
    reader->at += 1u;
    return EMBED_CALL(regexp_class_range, RegexpAtomsCtx, .reader = reader, .count = low, .items = args->items,
                      .count_pointer = args->count_pointer);
}

// a class after its `[`
EMBED_INLINE unsigned int regexp_class(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    const int negated = EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = '^');
    RegexpItem items[256];
    unsigned int count = 0u;
    for (;;)
    {
        unsigned int character = 0u;
        if (!EMBED_CALL(regexp_read_character_internal.next, RegexpReadLetterCtx, .reader = reader,
                        .character = &character))
        {
            return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                              .why = "an unclosed class");
        }
        if (character == ']')
        {
            break;
        }
        if (!EMBED_CALL(regexp_class_member, RegexpAtomsCtx, .reader = reader, .count = character, .items = items,
                        .count_pointer = &count))
        {
            return REGEXP_NONE;
        }
    }
    Regexp *const regexp = reader->regexp;
    if ((regexp->items + count) > REGEXP_ITEMS)
    {
        return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                          .why = "more class items than a pattern allows");
    }
    const unsigned int made =
        EMBED_CALL(regexp_read_type_internal.node, RegexpReadTypeCtx, .reader = reader, .type = REGEXP_CLASS);
    if (made != REGEXP_NONE)
    {
        regexp->node[made].first = regexp->items;
        regexp->node[made].count = count;
        regexp->node[made].negated = negated;
        memor.cpy(&(MemoriaCfg){.dst = &regexp->item[regexp->items], .src = items, .bytes = count * sizeof(items[0])});
        regexp->items += count;
    }
    return made;
}

// a node of `type` over `child`, its flag `negated` and its group `group`
EMBED_INLINE unsigned int regexp_on_wrap(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    unsigned int type = args->type;
    unsigned int child = args->child;
    int negated = args->negated;
    unsigned int group = args->group;
    if (child == REGEXP_NONE)
    {
        return child;
    }
    const unsigned int made =
        EMBED_CALL(regexp_read_type_internal.node, RegexpReadTypeCtx, .reader = reader, .type = type);
    if (made != REGEXP_NONE)
    {
        reader->regexp->node[made].child = child;
        reader->regexp->node[made].negated = negated;
        reader->regexp->node[made].group = group;
    }
    return made;
}

// a group after its `(`: one that does not capture, a lookahead, or one that does
EMBED_INLINE unsigned int regexp_group(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    if (EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = '?'))
    {
        unsigned int way = 0u;
        if (!EMBED_CALL(regexp_read_character_internal.next, RegexpReadLetterCtx, .reader = reader,
                        .character = &way) ||
            ((way != ':') && (way != '=') && (way != '!')))
        {
            return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                              .why = "a group not read here");
        }
        const unsigned int inner = EMBED_CALL(regexp_read_internal.alternation, RegexpReadCtx, .reader = reader);
        if ((inner != REGEXP_NONE) &&
            !EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = ')'))
        {
            return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                              .why = (way == ':') ? "an unclosed group" : "an unclosed lookahead");
        }
        return (way == ':') ? EMBED_CALL(regexp_on_wrap, RegexpAtomsCtx, .reader = reader, .type = REGEXP_GROUP,
                                         .child = inner, .negated = 0, .group = REGEXP_NONE)
                            : EMBED_CALL(regexp_on_wrap, RegexpAtomsCtx, .reader = reader, .type = REGEXP_LOOK,
                                         .child = inner, .negated = way == '!', .group = REGEXP_NONE);
    }
    if ((reader->groups + 1u) >= REGEXP_GROUPS)
    {
        return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                          .why = "more groups than a pattern allows");
    }
    reader->groups += 1u;
    const unsigned int index = reader->groups;
    const unsigned int inner = EMBED_CALL(regexp_read_internal.alternation, RegexpReadCtx, .reader = reader);
    if ((inner != REGEXP_NONE) &&
        !EMBED_CALL(regexp_read_internal.eat, RegexpReadCtx, .reader = reader, .required = ')'))
    {
        return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = reader,
                          .why = "an unclosed group");
    }
    return EMBED_CALL(regexp_on_wrap, RegexpAtomsCtx, .reader = reader, .type = REGEXP_GROUP, .child = inner,
                      .negated = 0, .group = index);
}

// a node of the one character `count`
EMBED_INLINE unsigned int regexp_atom_character(const RegexpAtomsCtx *args)
{
    const unsigned int made =
        EMBED_CALL(regexp_read_type_internal.node, RegexpReadTypeCtx, .reader = args->reader, .type = REGEXP_CHAR);
    if (made != REGEXP_NONE)
    {
        args->reader->regexp->node[made].character = args->count;
    }
    return made;
}

// a node of the class item `item_value` alone, where the pattern's items have capacity for one more
EMBED_INLINE unsigned int regexp_atom_item(const RegexpAtomsCtx *args)
{
    Regexp *const regexp = args->reader->regexp;
    if (regexp->items >= REGEXP_ITEMS)
    {
        return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = args->reader,
                          .why = "more class items than a pattern allows");
    }
    const unsigned int made =
        EMBED_CALL(regexp_read_type_internal.node, RegexpReadTypeCtx, .reader = args->reader, .type = REGEXP_CLASS);
    if (made != REGEXP_NONE)
    {
        regexp->node[made].first = regexp->items;
        regexp->node[made].count = 1u;
        regexp->item[regexp->items] = args->item_value;
        regexp->items += 1u;
    }
    return made;
}

// an atom after its backslash: a word boundary, a backreference, an escape's class item, or the character it escapes
EMBED_INLINE unsigned int regexp_atom_escape(const RegexpAtomsCtx *args)
{
    RegexpReader *reader = args->reader;
    unsigned int peeked = 0u;
    const int there = EMBED_CALL(regexp_read_character_type_internal.peek, RegexpReadLetterTypeCtx,
                                 .reader_read = reader, .character = &peeked);
    if (there && ((peeked == 'b') || (peeked == 'B')))
    {
        reader->at += 1u;
        const unsigned int made =
            EMBED_CALL(regexp_read_type_internal.node, RegexpReadTypeCtx, .reader = reader, .type = REGEXP_BOUNDARY);
        if (made != REGEXP_NONE)
        {
            reader->regexp->node[made].negated = peeked == 'B';
        }
        return made;
    }
    if (there && (peeked >= '1') && (peeked <= '9'))
    {
        unsigned int index = 0u;
        EMBED_CALL(regexp_read_value_internal.number, RegexpReadValueCtx, .reader = reader, .value = &index);
        const unsigned int made =
            EMBED_CALL(regexp_read_type_internal.node, RegexpReadTypeCtx, .reader = reader, .type = REGEXP_BACK);
        if (made != REGEXP_NONE)
        {
            reader->regexp->node[made].group = index;
        }
        return made;
    }
    unsigned int character = 0u;
    RegexpItem item;
    const int escape = EMBED_CALL(regexp_escape, RegexpAtomsCtx, .reader = reader, .in_class = 0, .item = &item,
                                  .character = &character);
    if (escape == REGEXP_ESCAPE_FAILED)
    {
        return REGEXP_NONE;
    }
    if (escape == REGEXP_ESCAPE_ITEM)
    {
        return EMBED_CALL(regexp_atom_item, RegexpAtomsCtx, .reader = reader, .item_value = item);
    }
    return EMBED_CALL(regexp_atom_character, RegexpAtomsCtx, .reader = reader, .count = character);
}

unsigned int regexp_atom(const RegexpReadCtx *args)
{
    unsigned int character = 0u;
    if (!EMBED_CALL(regexp_read_character_internal.next, RegexpReadLetterCtx, .reader = args->reader,
                    .character = &character))
    {
        return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = args->reader,
                          .why = "nothing to read");
    }
    if ((character == '.') || (character == '^') || (character == '$'))
    {
        const unsigned int anchor_type = (character == '^') ? REGEXP_START : REGEXP_END;
        return EMBED_CALL(regexp_read_type_internal.node, RegexpReadTypeCtx, .reader = args->reader,
                          .type = (character == '.') ? REGEXP_ANY : anchor_type);
    }
    if (character == '[')
    {
        return EMBED_CALL(regexp_class, RegexpAtomsCtx, .reader = args->reader);
    }
    if (character == '(')
    {
        return EMBED_CALL(regexp_group, RegexpAtomsCtx, .reader = args->reader);
    }
    if ((character == '*') || (character == '+') || (character == '?'))
    {
        return EMBED_CALL(regexp_read_error_internal.fail, RegexpReadErrorCtx, .reader = args->reader,
                          .why = "a quantifier with nothing before it");
    }
    if (character == '\\')
    {
        return EMBED_CALL(regexp_atom_escape, RegexpAtomsCtx, .reader = args->reader);
    }
    return EMBED_CALL(regexp_atom_character, RegexpAtomsCtx, .reader = args->reader, .count = character);
}
