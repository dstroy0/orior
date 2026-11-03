// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_atoms.c: an atom of a pattern read: a character, `.`, `^`, `$`, a class, a group, a lookahead, a boundary,
// a backreference, or an escape, which is a class item or one character
#include "regexp_read_internal.h"

// what an escape read is: a class item, one character, or a failure reported
#define CLI_ESCAPE_ITEM 0

#define CLI_ESCAPE_LETTER 1

#define CLI_ESCAPE_FAILED 2

typedef struct
{
    CliRegexpReader *reader;
    unsigned int count;
    unsigned int *letter;
    int in_class;
    CliRegexpItem *item;
    CliRegexpItem *items;
    unsigned int *count_pointer;
    CliRegexpItem item_value;
    unsigned int kind;
    unsigned int child;
    int negated;
    unsigned int group;
} RegexpAtomsCtx;
// `count` hex digits where reading stands, read into `letter`: 1, or 0 with reading where it stood
EMBED_INLINE int cli_hex(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    unsigned int *letter = args->letter;
    const unsigned long long start = reader->at;
    unsigned int value = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        unsigned int digit = 0u;
        const int there = EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx,
                                     .reader_read = reader, .letter = &digit);
        const int decimal = there && (digit >= '0') && (digit <= '9');
        const int lower = there && (digit >= 'a') && (digit <= 'f');
        const int upper = there && (digit >= 'A') && (digit <= 'F');
        if (!decimal && !lower && !upper)
        {
            reader->at = start;
            return 0;
        }
        const unsigned int letter_value = lower ? (digit - 'a' + 10u) : (digit - 'A' + 10u);
        value = (value << 4u) | (decimal ? (digit - '0') : letter_value);
        reader->at += 1u;
    }
    *letter = value;
    return 1;
}

// a `\u` escape after its `u`, four hex digits or a code in braces, into `letter`: CLI_ESCAPE_LETTER, `u` where it
// names no character, or CLI_ESCAPE_FAILED with why given
EMBED_INLINE int cli_escape_code(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    unsigned int *letter = args->letter;
    if (!EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '{'))
    {
        const int read = EMBED_CALL(cli_hex, RegexpAtomsCtx, .reader = reader, .count = 4u, .letter = letter);
        *letter = (read && !((*letter >= 0xd800u) && (*letter <= 0xdfffu))) ? *letter : 'u';
        return CLI_ESCAPE_LETTER;
    }
    unsigned long long code = 0ULL;
    unsigned long long digits = 0ULL;
    unsigned int digit = 0u;
    while (EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx, .reader_read = reader,
                      .letter = &digit) &&
           (((digit >= '0') && (digit <= '9')) || ((digit >= 'a') && (digit <= 'f')) ||
            ((digit >= 'A') && (digit <= 'F'))))
    {
        code = (code << 4u) | ((digit <= '9') ? (digit - '0') : ((digit | 0x20u) - 'a' + 10u));
        digits += 1ULL;
        reader->at += 1u;
    }
    if (!EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '}'))
    {
        EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                   .why = "an unclosed \\u{");
        return CLI_ESCAPE_FAILED;
    }
    // a code past what a character represents, or none, reads as `u`
    const int valid = (digits != 0ULL) && (digits <= 6ULL) && (code <= 0x10ffffULL) &&
                      !((code >= 0xd800ULL) && (code <= 0xdfffULL));
    *letter = valid ? (unsigned int)code : 'u';
    return CLI_ESCAPE_LETTER;
}

// the property the `count` letters from `letter` name, by its short name or its long one: its place, 8 where none is
// read here
EMBED_INLINE unsigned int cli_property_named(const RegexpAtomsCtx *args)
{
    static const char *const s_properties[8] = {"L",  "Letter",           "N",  "Number",
                                                "Lu", "Uppercase_Letter", "Ll", "Lowercase_Letter"};
    const unsigned int *const letters = args->letter;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        const size_t name_length = cellul.len(&(CatenaFinitaCfg){.src = s_properties[at], .cap = CLI_CELLS_POOL});
        int same = name_length == args->count;
        for (size_t letter_at = 0u; same && (letter_at < name_length); letter_at += 1u)
        {
            same = letters[letter_at] == (unsigned char)s_properties[at][letter_at];
        }
        if (same)
        {
            return at / 2u;
        }
    }
    return 8u;
}

// a `\p{..}` or `\P{..}` escape after its letter, the property named and whether it is negated into `item`:
// CLI_ESCAPE_ITEM, or CLI_ESCAPE_FAILED with why given
EMBED_INLINE int cli_escape_property(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    if (!EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '{'))
    {
        EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                   .why = "a property with no name");
        return CLI_ESCAPE_FAILED;
    }
    const unsigned long long start = reader->at;
    unsigned int named = 0u;
    while (EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx, .reader_read = reader,
                      .letter = &named) &&
           (named != '}'))
    {
        reader->at += 1u;
    }
    // a property's name is a few letters
    const unsigned int length = (unsigned int)(reader->at - start);
    if (!EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '}'))
    {
        EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                   .why = "an unclosed property");
        return CLI_ESCAPE_FAILED;
    }
    const unsigned int property =
        EMBED_CALL(cli_property_named, RegexpAtomsCtx, .letter = &reader->letters[start], .count = length);
    if (property == 8u)
    {
        EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                   .why = "a property not read here");
        return CLI_ESCAPE_FAILED;
    }
    *args->item = (CliRegexpItem){CLI_REGEXP_PROPERTY, property, 0u, args->negated};
    return CLI_ESCAPE_ITEM;
}

// a `\c` escape after its `c`, a letter whose place in the alphabet is the control character, into `letter`:
// CLI_ESCAPE_LETTER, or CLI_ESCAPE_FAILED with why given
EMBED_INLINE int cli_escape_control(const RegexpAtomsCtx *args)
{
    unsigned int control = 0u;
    if (!EMBED_CALL(cli_regexp_read_letter_internal.next, CliRegexpReadLetterCtx, .reader = args->reader,
                    .letter = &control) ||
        !(((control | 0x20u) >= 'a') && ((control | 0x20u) <= 'z')))
    {
        EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = args->reader,
                   .why = "a control escape");
        return CLI_ESCAPE_FAILED;
    }
    *args->letter = control % 32u;
    return CLI_ESCAPE_LETTER;
}

// An escape after its backslash, into `item` or `letter`: CLI_ESCAPE_ITEM, CLI_ESCAPE_LETTER, or CLI_ESCAPE_FAILED with
// why given
static int cli_escape(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    CliRegexpItem *item = args->item;
    unsigned int *letter = args->letter;
    unsigned int escaped = 0u;
    if (!EMBED_CALL(cli_regexp_read_letter_internal.next, CliRegexpReadLetterCtx, .reader = reader, .letter = &escaped))
    {
        EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                   .why = "a backslash at the end");
        return CLI_ESCAPE_FAILED;
    }
    static const char s_classes[6] = {'d', 'D', 'w', 'W', 's', 'S'};
    static const unsigned int s_kinds[6] = {CLI_REGEXP_DIGIT, CLI_REGEXP_DIGIT, CLI_REGEXP_WORD,
                                            CLI_REGEXP_WORD,  CLI_REGEXP_SPACE, CLI_REGEXP_SPACE};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (escaped == (unsigned char)s_classes[at])
        {
            *item = (CliRegexpItem){s_kinds[at], 0u, 0u, (at % 2u) == 1u};
            return CLI_ESCAPE_ITEM;
        }
    }
    static const char s_named[6] = {'t', 'n', 'r', 'f', 'v', '0'};
    static const unsigned int s_letters[6] = {'\t', '\n', '\r', 0x0cu, 0x0bu, 0u};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (escaped == (unsigned char)s_named[at])
        {
            *letter = s_letters[at];
            return CLI_ESCAPE_LETTER;
        }
    }
    *letter = escaped;
    if ((escaped == 'b') && args->in_class)
    {
        *letter = 8u;
    }
    else if (escaped == 'x')
    {
        *letter = EMBED_CALL(cli_hex, RegexpAtomsCtx, .reader = reader, .count = 2u, .letter = letter) ? *letter : 'x';
    }
    else if (escaped == 'u')
    {
        return EMBED_CALL(cli_escape_code, RegexpAtomsCtx, .reader = reader, .letter = letter);
    }
    else if (escaped == 'c')
    {
        return EMBED_CALL(cli_escape_control, RegexpAtomsCtx, .reader = reader, .letter = letter);
    }
    else if (((escaped == 'p') || (escaped == 'P')) && reader->unicode)
    {
        return EMBED_CALL(cli_escape_property, RegexpAtomsCtx, .reader = reader, .item = item,
                          .negated = escaped == 'P');
    }
    else if (escaped == 'k')
    {
        EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                   .why = "an escape not read here");
        return CLI_ESCAPE_FAILED;
    }
    return CLI_ESCAPE_LETTER;
}

// `item` added to the class's items: 1, or 0 with why given where a class contains as many as it can
EMBED_INLINE int cli_item_add(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    unsigned int *count = args->count_pointer;
    if (*count >= 256u)
    {
        EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                   .why = "more items than a class allows");
        return 0;
    }
    args->items[*count] = args->item_value;
    *count += 1u;
    return 1;
}

// a range of a class from `count` after its `-`: its high end read and the range put among `items`, or where the high
// end is an escape's item, the low end, the `-` and the item each put as one; 1, or 0 with why given
EMBED_INLINE int cli_class_range(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    const unsigned int low = args->count;
    unsigned int high = 0u;
    CliRegexpItem item;
    EMBED_CALL(cli_regexp_read_letter_internal.next, CliRegexpReadLetterCtx, .reader = reader, .letter = &high);
    const int escape = (high == '\\') ? EMBED_CALL(cli_escape, RegexpAtomsCtx, .reader = reader, .in_class = 1,
                                                   .item = &item, .letter = &high)
                                      : CLI_ESCAPE_LETTER;
    if (escape == CLI_ESCAPE_FAILED)
    {
        return 0;
    }
    if (escape == CLI_ESCAPE_ITEM)
    {
        return EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                          .count_pointer = args->count_pointer,
                          .item_value = (CliRegexpItem){CLI_REGEXP_RANGE, low, low, 0}) &&
               EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                          .count_pointer = args->count_pointer,
                          .item_value = (CliRegexpItem){CLI_REGEXP_RANGE, '-', '-', 0}) &&
               EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                          .count_pointer = args->count_pointer, .item_value = item);
    }
    if (high < low)
    {
        (void)EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                         .why = "a range out of order");
        return 0;
    }
    return EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                      .count_pointer = args->count_pointer,
                      .item_value = (CliRegexpItem){CLI_REGEXP_RANGE, low, high, 0});
}

// a member of a class from its first letter `count`: a letter, a range, or an escape's item, put among `items`; 1, or
// 0 with why given
EMBED_INLINE int cli_class_member(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    unsigned int low = args->count;
    if (low == '\\')
    {
        CliRegexpItem item;
        const int escape =
            EMBED_CALL(cli_escape, RegexpAtomsCtx, .reader = reader, .in_class = 1, .item = &item, .letter = &low);
        if (escape != CLI_ESCAPE_LETTER)
        {
            return (escape == CLI_ESCAPE_ITEM) &&
                   EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                              .count_pointer = args->count_pointer, .item_value = item);
        }
    }
    unsigned int peeked = 0u;
    const int ranged = EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx,
                                  .reader_read = reader, .letter = &peeked) &&
                       (peeked == '-') && ((reader->at + 1u) < reader->length) &&
                       (reader->letters[reader->at + 1u] != ']');
    if (!ranged)
    {
        return EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = args->items,
                          .count_pointer = args->count_pointer,
                          .item_value = (CliRegexpItem){CLI_REGEXP_RANGE, low, low, 0});
    }
    reader->at += 1u;
    return EMBED_CALL(cli_class_range, RegexpAtomsCtx, .reader = reader, .count = low, .items = args->items,
                      .count_pointer = args->count_pointer);
}

// a class after its `[`
EMBED_INLINE unsigned int cli_class(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    const int negated = EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '^');
    CliRegexpItem items[256];
    unsigned int count = 0u;
    for (;;)
    {
        unsigned int letter = 0u;
        if (!EMBED_CALL(cli_regexp_read_letter_internal.next, CliRegexpReadLetterCtx, .reader = reader,
                        .letter = &letter))
        {
            return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                              .why = "an unclosed class");
        }
        if (letter == ']')
        {
            break;
        }
        if (!EMBED_CALL(cli_class_member, RegexpAtomsCtx, .reader = reader, .count = letter, .items = items,
                        .count_pointer = &count))
        {
            return CLI_REGEXP_NONE;
        }
    }
    CliRegexp *const regexp = reader->regexp;
    if ((regexp->items + count) > CLI_REGEXP_ITEMS)
    {
        return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                          .why = "more class items than a pattern allows");
    }
    const unsigned int made = EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx, .reader = reader,
                                         .kind = CLI_REGEXP_CLASS);
    if (made != CLI_REGEXP_NONE)
    {
        regexp->node[made].first = regexp->items;
        regexp->node[made].count = count;
        regexp->node[made].negated = negated;
        memor.cpy(&(MemoriaCfg){.dst = &regexp->item[regexp->items], .src = items, .bytes = count * sizeof(items[0])});
        regexp->items += count;
    }
    return made;
}

// a node of `kind` over `child`, its flag `negated` and its group `group`
EMBED_INLINE unsigned int cli_on_wrap(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    unsigned int kind = args->kind;
    unsigned int child = args->child;
    int negated = args->negated;
    unsigned int group = args->group;
    if (child == CLI_REGEXP_NONE)
    {
        return child;
    }
    const unsigned int made =
        EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx, .reader = reader, .kind = kind);
    if (made != CLI_REGEXP_NONE)
    {
        reader->regexp->node[made].child = child;
        reader->regexp->node[made].negated = negated;
        reader->regexp->node[made].group = group;
    }
    return made;
}

// a group after its `(`: one that does not capture, a lookahead, or one that does
EMBED_INLINE unsigned int cli_group(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    if (EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '?'))
    {
        unsigned int way = 0u;
        if (!EMBED_CALL(cli_regexp_read_letter_internal.next, CliRegexpReadLetterCtx, .reader = reader,
                        .letter = &way) ||
            ((way != ':') && (way != '=') && (way != '!')))
        {
            return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                              .why = "a group not read here");
        }
        const unsigned int inner = EMBED_CALL(cli_regexp_read_internal.alternation, CliRegexpReadCtx, .reader = reader);
        if ((inner != CLI_REGEXP_NONE) &&
            !EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = ')'))
        {
            return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                              .why = (way == ':') ? "an unclosed group" : "an unclosed lookahead");
        }
        return (way == ':') ? EMBED_CALL(cli_on_wrap, RegexpAtomsCtx, .reader = reader, .kind = CLI_REGEXP_GROUP,
                                         .child = inner, .negated = 0, .group = CLI_REGEXP_NONE)
                            : EMBED_CALL(cli_on_wrap, RegexpAtomsCtx, .reader = reader, .kind = CLI_REGEXP_LOOK,
                                         .child = inner, .negated = way == '!', .group = CLI_REGEXP_NONE);
    }
    if ((reader->groups + 1u) >= CLI_REGEXP_GROUPS)
    {
        return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                          .why = "more groups than a pattern allows");
    }
    reader->groups += 1u;
    const unsigned int index = reader->groups;
    const unsigned int inner = EMBED_CALL(cli_regexp_read_internal.alternation, CliRegexpReadCtx, .reader = reader);
    if ((inner != CLI_REGEXP_NONE) &&
        !EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = ')'))
    {
        return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                          .why = "an unclosed group");
    }
    return EMBED_CALL(cli_on_wrap, RegexpAtomsCtx, .reader = reader, .kind = CLI_REGEXP_GROUP, .child = inner,
                      .negated = 0, .group = index);
}

// a node of the one character `count`
EMBED_INLINE unsigned int cli_atom_letter(const RegexpAtomsCtx *args)
{
    const unsigned int made = EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx,
                                         .reader = args->reader, .kind = CLI_REGEXP_CHAR);
    if (made != CLI_REGEXP_NONE)
    {
        args->reader->regexp->node[made].letter = args->count;
    }
    return made;
}

// a node of the class item `item_value` alone, where the pattern's items have capacity for one more
EMBED_INLINE unsigned int cli_atom_item(const RegexpAtomsCtx *args)
{
    CliRegexp *const regexp = args->reader->regexp;
    if (regexp->items >= CLI_REGEXP_ITEMS)
    {
        return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = args->reader,
                          .why = "more class items than a pattern allows");
    }
    const unsigned int made = EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx,
                                         .reader = args->reader, .kind = CLI_REGEXP_CLASS);
    if (made != CLI_REGEXP_NONE)
    {
        regexp->node[made].first = regexp->items;
        regexp->node[made].count = 1u;
        regexp->item[regexp->items] = args->item_value;
        regexp->items += 1u;
    }
    return made;
}

// an atom after its backslash: a word boundary, a backreference, an escape's class item, or the character it escapes
EMBED_INLINE unsigned int cli_atom_escape(const RegexpAtomsCtx *args)
{
    CliRegexpReader *reader = args->reader;
    unsigned int peeked = 0u;
    const int there = EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx,
                                 .reader_read = reader, .letter = &peeked);
    if (there && ((peeked == 'b') || (peeked == 'B')))
    {
        reader->at += 1u;
        const unsigned int made = EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx,
                                             .reader = reader, .kind = CLI_REGEXP_BOUNDARY);
        if (made != CLI_REGEXP_NONE)
        {
            reader->regexp->node[made].negated = peeked == 'B';
        }
        return made;
    }
    if (there && (peeked >= '1') && (peeked <= '9'))
    {
        unsigned int index = 0u;
        EMBED_CALL(cli_regexp_read_value_internal.number, CliRegexpReadValueCtx, .reader = reader, .value = &index);
        const unsigned int made = EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx,
                                             .reader = reader, .kind = CLI_REGEXP_BACK);
        if (made != CLI_REGEXP_NONE)
        {
            reader->regexp->node[made].group = index;
        }
        return made;
    }
    unsigned int letter = 0u;
    CliRegexpItem item;
    const int escape =
        EMBED_CALL(cli_escape, RegexpAtomsCtx, .reader = reader, .in_class = 0, .item = &item, .letter = &letter);
    if (escape == CLI_ESCAPE_FAILED)
    {
        return CLI_REGEXP_NONE;
    }
    if (escape == CLI_ESCAPE_ITEM)
    {
        return EMBED_CALL(cli_atom_item, RegexpAtomsCtx, .reader = reader, .item_value = item);
    }
    return EMBED_CALL(cli_atom_letter, RegexpAtomsCtx, .reader = reader, .count = letter);
}

unsigned int cli_atom(const CliRegexpReadCtx *args)
{
    unsigned int letter = 0u;
    if (!EMBED_CALL(cli_regexp_read_letter_internal.next, CliRegexpReadLetterCtx, .reader = args->reader,
                    .letter = &letter))
    {
        return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = args->reader,
                          .why = "nothing to read");
    }
    if ((letter == '.') || (letter == '^') || (letter == '$'))
    {
        const unsigned int anchor_kind = (letter == '^') ? CLI_REGEXP_START : CLI_REGEXP_END;
        return EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx, .reader = args->reader,
                          .kind = (letter == '.') ? CLI_REGEXP_ANY : anchor_kind);
    }
    if (letter == '[')
    {
        return EMBED_CALL(cli_class, RegexpAtomsCtx, .reader = args->reader);
    }
    if (letter == '(')
    {
        return EMBED_CALL(cli_group, RegexpAtomsCtx, .reader = args->reader);
    }
    if ((letter == '*') || (letter == '+') || (letter == '?'))
    {
        return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = args->reader,
                          .why = "a quantifier with nothing before it");
    }
    if (letter == '\\')
    {
        return EMBED_CALL(cli_atom_escape, RegexpAtomsCtx, .reader = args->reader);
    }
    return EMBED_CALL(cli_atom_letter, RegexpAtomsCtx, .reader = args->reader, .count = letter);
}
