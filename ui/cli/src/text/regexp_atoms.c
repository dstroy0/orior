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
        value = (value << 4u) | (decimal ? (digit - '0') : lower ? (digit - 'a' + 10u) : (digit - 'A' + 10u));
        reader->at += 1u;
    }
    *letter = value;
    return 1;
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
        if (EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '{'))
        {
            unsigned long long code = 0ull;
            unsigned long long digits = 0ull;
            unsigned int digit = 0u;
            while (EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx,
                              .reader_read = reader, .letter = &digit) &&
                   (((digit >= '0') && (digit <= '9')) || ((digit >= 'a') && (digit <= 'f')) ||
                    ((digit >= 'A') && (digit <= 'F'))))
            {
                code = (code << 4u) | ((digit <= '9') ? (digit - '0') : ((digit | 0x20u) - 'a' + 10u));
                digits += 1ull;
                reader->at += 1u;
            }
            if (!EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '}'))
            {
                EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                           .why = "an unclosed \\u{");
                return CLI_ESCAPE_FAILED;
            }
            // a code past what a character represents, or none, reads as `u`
            const int valid = (digits != 0ull) && (digits <= 6ull) && (code <= 0x10ffffull) &&
                              !((code >= 0xd800ull) && (code <= 0xdfffull));
            *letter = valid ? (unsigned int)code : 'u';
        }
        else
        {
            *letter = EMBED_CALL(cli_hex, RegexpAtomsCtx, .reader = reader, .count = 4u, .letter = letter) &&
                              !((*letter >= 0xd800u) && (*letter <= 0xdfffu))
                          ? *letter
                          : 'u';
        }
    }
    else if (escaped == 'c')
    {
        unsigned int control = 0u;
        if (!EMBED_CALL(cli_regexp_read_letter_internal.next, CliRegexpReadLetterCtx, .reader = reader,
                        .letter = &control) ||
            !(((control | 0x20u) >= 'a') && ((control | 0x20u) <= 'z')))
        {
            EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                       .why = "a control escape");
            return CLI_ESCAPE_FAILED;
        }
        *letter = control % 32u;
    }
    else if (((escaped == 'p') || (escaped == 'P')) && reader->unicode)
    {
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
        const unsigned long long length = reader->at - start;
        if (!EMBED_CALL(cli_regexp_read_internal.eat, CliRegexpReadCtx, .reader = reader, .required = '}'))
        {
            EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                       .why = "an unclosed property");
            return CLI_ESCAPE_FAILED;
        }
        static const char *const s_properties[8] = {"L",  "Letter",           "N",  "Number",
                                                    "Lu", "Uppercase_Letter", "Ll", "Lowercase_Letter"};
        unsigned int property = 8u;
        for (unsigned int at = 0u; (property == 8u) && (at < 8u); at += 1u)
        {
            const size_t name_length = cellul.len(&(CatenaFinitaCfg){.src = s_properties[at], .cap = CLI_CELLS_POOL});
            int same = name_length == length;
            for (size_t letter_at = 0u; same && (letter_at < name_length); letter_at += 1u)
            {
                same = reader->letters[start + letter_at] == (unsigned char)s_properties[at][letter_at];
            }
            property = same ? (at / 2u) : property;
        }
        if (property == 8u)
        {
            EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                       .why = "a property not read here");
            return CLI_ESCAPE_FAILED;
        }
        *item = (CliRegexpItem){CLI_REGEXP_PROPERTY, property, 0u, escaped == 'P'};
        return CLI_ESCAPE_ITEM;
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
        unsigned int low = letter;
        CliRegexpItem item;
        if (letter == '\\')
        {
            const int escape =
                EMBED_CALL(cli_escape, RegexpAtomsCtx, .reader = reader, .in_class = 1, .item = &item, .letter = &low);
            if (escape == CLI_ESCAPE_FAILED)
            {
                return CLI_REGEXP_NONE;
            }
            if (escape == CLI_ESCAPE_ITEM)
            {
                if (!EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = items, .count_pointer = &count,
                                .item_value = item))
                {
                    return CLI_REGEXP_NONE;
                }
                continue;
            }
        }
        unsigned int peeked = 0u;
        const int ranged = EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx,
                                      .reader_read = reader, .letter = &peeked) &&
                           (peeked == '-') && ((reader->at + 1u) < reader->length) &&
                           (reader->letters[reader->at + 1u] != ']');
        if (!ranged)
        {
            if (!EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = items, .count_pointer = &count,
                            .item_value = (CliRegexpItem){CLI_REGEXP_RANGE, low, low, 0}))
            {
                return CLI_REGEXP_NONE;
            }
            continue;
        }
        reader->at += 1u;
        unsigned int high = 0u;
        EMBED_CALL(cli_regexp_read_letter_internal.next, CliRegexpReadLetterCtx, .reader = reader, .letter = &high);
        if (high == '\\')
        {
            const int escape =
                EMBED_CALL(cli_escape, RegexpAtomsCtx, .reader = reader, .in_class = 1, .item = &item, .letter = &high);
            if (escape == CLI_ESCAPE_FAILED)
            {
                return CLI_REGEXP_NONE;
            }
            if (escape == CLI_ESCAPE_ITEM)
            {
                if (!EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = items, .count_pointer = &count,
                                .item_value = (CliRegexpItem){CLI_REGEXP_RANGE, low, low, 0}) ||
                    !EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = items, .count_pointer = &count,
                                .item_value = (CliRegexpItem){CLI_REGEXP_RANGE, '-', '-', 0}) ||
                    !EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = items, .count_pointer = &count,
                                .item_value = item))
                {
                    return CLI_REGEXP_NONE;
                }
                continue;
            }
        }
        if (high < low)
        {
            return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = reader,
                              .why = "a range out of order");
        }
        if (!EMBED_CALL(cli_item_add, RegexpAtomsCtx, .reader = reader, .items = items, .count_pointer = &count,
                        .item_value = (CliRegexpItem){CLI_REGEXP_RANGE, low, high, 0}))
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
        return EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx, .reader = args->reader,
                          .kind = (letter == '.')   ? CLI_REGEXP_ANY
                                  : (letter == '^') ? CLI_REGEXP_START
                                                    : CLI_REGEXP_END);
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
    unsigned int made = CLI_REGEXP_NONE;
    if (letter == '\\')
    {
        unsigned int peeked = 0u;
        const int there = EMBED_CALL(cli_regexp_read_letter_kind_internal.peek, CliRegexpReadLetterKindCtx,
                                     .reader_read = args->reader, .letter = &peeked);
        if (there && ((peeked == 'b') || (peeked == 'B')))
        {
            args->reader->at += 1u;
            made = EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx, .reader = args->reader,
                              .kind = CLI_REGEXP_BOUNDARY);
            if (made != CLI_REGEXP_NONE)
            {
                args->reader->regexp->node[made].negated = peeked == 'B';
            }
            return made;
        }
        if (there && (peeked >= '1') && (peeked <= '9'))
        {
            unsigned int index = 0u;
            EMBED_CALL(cli_regexp_read_value_internal.number, CliRegexpReadValueCtx, .reader = args->reader,
                       .value = &index);
            made = EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx, .reader = args->reader,
                              .kind = CLI_REGEXP_BACK);
            if (made != CLI_REGEXP_NONE)
            {
                args->reader->regexp->node[made].group = index;
            }
            return made;
        }
        CliRegexpItem item;
        const int escape = EMBED_CALL(cli_escape, RegexpAtomsCtx, .reader = args->reader, .in_class = 0, .item = &item,
                                      .letter = &letter);
        if (escape == CLI_ESCAPE_FAILED)
        {
            return CLI_REGEXP_NONE;
        }
        if (escape == CLI_ESCAPE_ITEM)
        {
            CliRegexp *const regexp = args->reader->regexp;
            if (regexp->items >= CLI_REGEXP_ITEMS)
            {
                return EMBED_CALL(cli_regexp_read_error_internal.fail, CliRegexpReadErrorCtx, .reader = args->reader,
                                  .why = "more class items than a pattern allows");
            }
            made = EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx, .reader = args->reader,
                              .kind = CLI_REGEXP_CLASS);
            if (made != CLI_REGEXP_NONE)
            {
                regexp->node[made].first = regexp->items;
                regexp->node[made].count = 1u;
                regexp->item[regexp->items] = item;
                regexp->items += 1u;
            }
            return made;
        }
    }
    made = EMBED_CALL(cli_regexp_read_kind_internal.node, CliRegexpReadKindCtx, .reader = args->reader,
                      .kind = CLI_REGEXP_CHAR);
    if (made != CLI_REGEXP_NONE)
    {
        args->reader->regexp->node[made].letter = letter;
    }
    return made;
}
