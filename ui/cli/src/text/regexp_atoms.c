// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_atoms.c: an atom of a pattern read: a character, `.`, `^`, `$`, a class, a group, a lookahead, a boundary,
// a backreference, or an escape, which is a class item or one character
#include "regexp_read_internal.h"

// what an escape read is: a class item, one character, or a failure said
#define CLI_ESCAPE_ITEM 0

#define CLI_ESCAPE_LETTER 1

#define CLI_ESCAPE_FAILED 2

// `count` hex digits where reading stands, read into `letter`: 1, or 0 with reading where it stood
static int cli_hex(CliRegexpReader *reader, unsigned int count, unsigned int *letter)
{
    const unsigned long long start = reader->at;
    unsigned int value = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        unsigned int digit = 0u;
        const int there = cli_peek(reader, &digit);
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
// why said
static int cli_escape(CliRegexpReader *reader, int in_class, CliRegexpItem *item, unsigned int *letter)
{
    unsigned int escaped = 0u;
    if (!cli_next(reader, &escaped))
    {
        cli_failed(reader, "a backslash at the end");
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
    if ((escaped == 'b') && in_class)
    {
        *letter = 8u;
    }
    else if (escaped == 'x')
    {
        *letter = cli_hex(reader, 2u, letter) ? *letter : 'x';
    }
    else if (escaped == 'u')
    {
        if (cli_eat(reader, '{'))
        {
            unsigned long long code = 0ull;
            unsigned long long digits = 0ull;
            unsigned int digit = 0u;
            while (cli_peek(reader, &digit) &&
                   (((digit >= '0') && (digit <= '9')) || ((digit >= 'a') && (digit <= 'f')) ||
                    ((digit >= 'A') && (digit <= 'F'))))
            {
                code = (code << 4u) | ((digit <= '9') ? (digit - '0') : ((digit | 0x20u) - 'a' + 10u));
                digits += 1ull;
                reader->at += 1u;
            }
            if (!cli_eat(reader, '}'))
            {
                cli_failed(reader, "an unclosed \\u{");
                return CLI_ESCAPE_FAILED;
            }
            // a code past what a character holds, or none, reads as `u`
            const int valid = (digits != 0ull) && (digits <= 6ull) && (code <= 0x10ffffull) &&
                              !((code >= 0xd800ull) && (code <= 0xdfffull));
            *letter = valid ? (unsigned int)code : 'u';
        }
        else
        {
            *letter = cli_hex(reader, 4u, letter) && !((*letter >= 0xd800u) && (*letter <= 0xdfffu)) ? *letter : 'u';
        }
    }
    else if (escaped == 'c')
    {
        unsigned int control = 0u;
        if (!cli_next(reader, &control) || !(((control | 0x20u) >= 'a') && ((control | 0x20u) <= 'z')))
        {
            cli_failed(reader, "a control escape");
            return CLI_ESCAPE_FAILED;
        }
        *letter = control % 32u;
    }
    else if (((escaped == 'p') || (escaped == 'P')) && reader->unicode)
    {
        if (!cli_eat(reader, '{'))
        {
            cli_failed(reader, "a property with no name");
            return CLI_ESCAPE_FAILED;
        }
        const unsigned long long start = reader->at;
        unsigned int named = 0u;
        while (cli_peek(reader, &named) && (named != '}'))
        {
            reader->at += 1u;
        }
        const unsigned long long length = reader->at - start;
        if (!cli_eat(reader, '}'))
        {
            cli_failed(reader, "an unclosed property");
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
            cli_failed(reader, "a property not read here");
            return CLI_ESCAPE_FAILED;
        }
        *item = (CliRegexpItem){CLI_REGEXP_PROPERTY, property, 0u, escaped == 'P'};
        return CLI_ESCAPE_ITEM;
    }
    else if (escaped == 'k')
    {
        cli_failed(reader, "an escape not read here");
        return CLI_ESCAPE_FAILED;
    }
    return CLI_ESCAPE_LETTER;
}

// `item` added to the class's items: 1, or 0 with why said where a class holds as many as it can
static int cli_item_added(CliRegexpReader *reader, CliRegexpItem *items, unsigned int *count, CliRegexpItem item)
{
    if (*count >= 256u)
    {
        cli_failed(reader, "more items than a class holds");
        return 0;
    }
    items[*count] = item;
    *count += 1u;
    return 1;
}

// a class after its `[`
static unsigned int cli_class(CliRegexpReader *reader)
{
    const int negated = cli_eat(reader, '^');
    CliRegexpItem items[256];
    unsigned int count = 0u;
    for (;;)
    {
        unsigned int letter = 0u;
        if (!cli_next(reader, &letter))
        {
            return cli_failed(reader, "an unclosed class");
        }
        if (letter == ']')
        {
            break;
        }
        unsigned int low = letter;
        CliRegexpItem item;
        if (letter == '\\')
        {
            const int escape = cli_escape(reader, 1, &item, &low);
            if (escape == CLI_ESCAPE_FAILED)
            {
                return CLI_REGEXP_NONE;
            }
            if (escape == CLI_ESCAPE_ITEM)
            {
                if (!cli_item_added(reader, items, &count, item))
                {
                    return CLI_REGEXP_NONE;
                }
                continue;
            }
        }
        unsigned int peeked = 0u;
        const int ranged = cli_peek(reader, &peeked) && (peeked == '-') && ((reader->at + 1u) < reader->length) &&
                           (reader->letters[reader->at + 1u] != ']');
        if (!ranged)
        {
            if (!cli_item_added(reader, items, &count, (CliRegexpItem){CLI_REGEXP_RANGE, low, low, 0}))
            {
                return CLI_REGEXP_NONE;
            }
            continue;
        }
        reader->at += 1u;
        unsigned int high = 0u;
        cli_next(reader, &high);
        if (high == '\\')
        {
            const int escape = cli_escape(reader, 1, &item, &high);
            if (escape == CLI_ESCAPE_FAILED)
            {
                return CLI_REGEXP_NONE;
            }
            if (escape == CLI_ESCAPE_ITEM)
            {
                if (!cli_item_added(reader, items, &count, (CliRegexpItem){CLI_REGEXP_RANGE, low, low, 0}) ||
                    !cli_item_added(reader, items, &count, (CliRegexpItem){CLI_REGEXP_RANGE, '-', '-', 0}) ||
                    !cli_item_added(reader, items, &count, item))
                {
                    return CLI_REGEXP_NONE;
                }
                continue;
            }
        }
        if (high < low)
        {
            return cli_failed(reader, "a range out of order");
        }
        if (!cli_item_added(reader, items, &count, (CliRegexpItem){CLI_REGEXP_RANGE, low, high, 0}))
        {
            return CLI_REGEXP_NONE;
        }
    }
    CliRegexp *const regexp = reader->regexp;
    if ((regexp->items + count) > CLI_REGEXP_ITEMS)
    {
        return cli_failed(reader, "more class items than a pattern holds");
    }
    const unsigned int made = cli_node(reader, CLI_REGEXP_CLASS);
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
static unsigned int cli_wrapped(CliRegexpReader *reader, unsigned int kind, unsigned int child, int negated,
                                unsigned int group)
{
    if (child == CLI_REGEXP_NONE)
    {
        return child;
    }
    const unsigned int made = cli_node(reader, kind);
    if (made != CLI_REGEXP_NONE)
    {
        reader->regexp->node[made].child = child;
        reader->regexp->node[made].negated = negated;
        reader->regexp->node[made].group = group;
    }
    return made;
}

// a group after its `(`: one that does not capture, a lookahead, or one that does
static unsigned int cli_group(CliRegexpReader *reader)
{
    if (cli_eat(reader, '?'))
    {
        unsigned int way = 0u;
        if (!cli_next(reader, &way) || ((way != ':') && (way != '=') && (way != '!')))
        {
            return cli_failed(reader, "a group not read here");
        }
        const unsigned int inner = cli_alternation(reader);
        if ((inner != CLI_REGEXP_NONE) && !cli_eat(reader, ')'))
        {
            return cli_failed(reader, (way == ':') ? "an unclosed group" : "an unclosed lookahead");
        }
        return (way == ':') ? cli_wrapped(reader, CLI_REGEXP_GROUP, inner, 0, CLI_REGEXP_NONE)
                            : cli_wrapped(reader, CLI_REGEXP_LOOK, inner, way == '!', CLI_REGEXP_NONE);
    }
    if ((reader->groups + 1u) >= CLI_REGEXP_GROUPS)
    {
        return cli_failed(reader, "more groups than a pattern holds");
    }
    reader->groups += 1u;
    const unsigned int index = reader->groups;
    const unsigned int inner = cli_alternation(reader);
    if ((inner != CLI_REGEXP_NONE) && !cli_eat(reader, ')'))
    {
        return cli_failed(reader, "an unclosed group");
    }
    return cli_wrapped(reader, CLI_REGEXP_GROUP, inner, 0, index);
}

unsigned int cli_atom(CliRegexpReader *reader)
{
    unsigned int letter = 0u;
    if (!cli_next(reader, &letter))
    {
        return cli_failed(reader, "nothing to read");
    }
    if ((letter == '.') || (letter == '^') || (letter == '$'))
    {
        return cli_node(reader, (letter == '.') ? CLI_REGEXP_ANY : (letter == '^') ? CLI_REGEXP_START : CLI_REGEXP_END);
    }
    if (letter == '[')
    {
        return cli_class(reader);
    }
    if (letter == '(')
    {
        return cli_group(reader);
    }
    if ((letter == '*') || (letter == '+') || (letter == '?'))
    {
        return cli_failed(reader, "a quantifier with nothing before it");
    }
    unsigned int made = CLI_REGEXP_NONE;
    if (letter == '\\')
    {
        unsigned int peeked = 0u;
        const int there = cli_peek(reader, &peeked);
        if (there && ((peeked == 'b') || (peeked == 'B')))
        {
            reader->at += 1u;
            made = cli_node(reader, CLI_REGEXP_BOUNDARY);
            if (made != CLI_REGEXP_NONE)
            {
                reader->regexp->node[made].negated = peeked == 'B';
            }
            return made;
        }
        if (there && (peeked >= '1') && (peeked <= '9'))
        {
            unsigned int index = 0u;
            cli_number(reader, &index);
            made = cli_node(reader, CLI_REGEXP_BACK);
            if (made != CLI_REGEXP_NONE)
            {
                reader->regexp->node[made].group = index;
            }
            return made;
        }
        CliRegexpItem item;
        const int escape = cli_escape(reader, 0, &item, &letter);
        if (escape == CLI_ESCAPE_FAILED)
        {
            return CLI_REGEXP_NONE;
        }
        if (escape == CLI_ESCAPE_ITEM)
        {
            CliRegexp *const regexp = reader->regexp;
            if (regexp->items >= CLI_REGEXP_ITEMS)
            {
                return cli_failed(reader, "more class items than a pattern holds");
            }
            made = cli_node(reader, CLI_REGEXP_CLASS);
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
    made = cli_node(reader, CLI_REGEXP_CHAR);
    if (made != CLI_REGEXP_NONE)
    {
        reader->regexp->node[made].letter = letter;
    }
    return made;
}
