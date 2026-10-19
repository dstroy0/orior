// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_read.c: a pattern read into nodes: alternation, sequence, quantifiers, escapes, classes and atoms, as
// JavaScript's syntax writes them; and each alternation's first characters and each repeat's one way filled in once
// the pattern is read
#include "regexp_read_internal.h"

unsigned long long cli_regexp_letters(const char *text, unsigned long long size, unsigned int *letters,
                                      unsigned long long room)
{
    unsigned long long count = 0ull;
    unsigned long long at = 0ull;
    while ((at < size) && (count < room))
    {
        const unsigned char byte = (unsigned char)text[at];
        const unsigned int length = (byte < 0x80u) ? 1u : (byte < 0xe0u) ? 2u : (byte < 0xf0u) ? 3u : 4u;
        unsigned int letter = (length == 1u)   ? byte
                              : (length == 2u) ? (byte & 0x1fu)
                              : (length == 3u) ? (byte & 0x0fu)
                                               : (byte & 0x07u);
        for (unsigned int more = 1u; (more < length) && ((at + more) < size); more += 1u)
        {
            letter = (letter << 6u) | ((unsigned char)text[at + more] & 0x3fu);
        }
        letters[count] = letter;
        count += 1ull;
        at += length;
    }
    return count;
}

// `/<source>/: <why> at <place>` said: CLI_REGEXP_FAILED
unsigned int cli_failed(CliRegexpReader *reader, const char *why)
{
    if (!reader->failed)
    {
        cli_line_character(reader->said, '/');
        cli_line_text(reader->said, reader->source);
        cli_line_text(reader->said, "/: ");
        cli_line_text(reader->said, why);
        cli_line_text(reader->said, " at ");
        cli_line_decimal(reader->said, reader->at, 0u);
    }
    reader->failed = 1;
    return CLI_REGEXP_FAILED;
}

int cli_peek(const CliRegexpReader *reader, unsigned int *letter)
{
    if (reader->at >= reader->length)
    {
        return 0;
    }
    *letter = reader->letters[reader->at];
    return 1;
}

int cli_next(CliRegexpReader *reader, unsigned int *letter)
{
    const int there = cli_peek(reader, letter);
    reader->at += there ? 1u : 0u;
    return there;
}

int cli_eat(CliRegexpReader *reader, unsigned int wanted)
{
    unsigned int letter = 0u;
    if (cli_peek(reader, &letter) && (letter == wanted))
    {
        reader->at += 1u;
        return 1;
    }
    return 0;
}

// a node of `kind` made, its fields zero but its group none: its index, or CLI_REGEXP_FAILED where the pattern holds
// as many as it can
unsigned int cli_node(CliRegexpReader *reader, unsigned int kind)
{
    CliRegexp *const regexp = reader->regexp;
    if (regexp->nodes >= CLI_REGEXP_NODES)
    {
        return cli_failed(reader, "more nodes than a pattern holds");
    }
    const unsigned int made = regexp->nodes;
    memor.set(&(MemoriaCfg){.dst = &regexp->node[made], .val = 0, .bytes = sizeof(regexp->node[made])});
    regexp->node[made].kind = kind;
    regexp->node[made].group = CLI_REGEXP_NONE;
    regexp->nodes += 1u;
    return made;
}

// the nodes `children`, `count` of them, put in the parts as one node of `kind`: its index
unsigned int cli_parented(CliRegexpReader *reader, unsigned int kind, const unsigned int *children, unsigned int count)
{
    CliRegexp *const regexp = reader->regexp;
    if ((regexp->parts + count) > CLI_REGEXP_PARTS)
    {
        return cli_failed(reader, "more parts than a pattern holds");
    }
    const unsigned int made = cli_node(reader, kind);
    if (made == CLI_REGEXP_FAILED)
    {
        return made;
    }
    regexp->node[made].first = regexp->parts;
    regexp->node[made].count = count;
    memor.cpy(
        &(MemoriaCfg){.dst = &regexp->part[regexp->parts], .src = children, .bytes = count * sizeof(children[0])});
    regexp->parts += count;
    return made;
}

int cli_number(CliRegexpReader *reader, unsigned int *value)
{
    const unsigned long long start = reader->at;
    unsigned long long read = 0ull;
    unsigned int letter = 0u;
    while (cli_peek(reader, &letter) && (letter >= '0') && (letter <= '9'))
    {
        read = (read * 10ull) + (letter - '0');
        reader->at += 1u;
    }
    // a count past what an unsigned int holds is read as the most
    *value = (read < CLI_REGEXP_UNBOUNDED) ? (unsigned int)read : (CLI_REGEXP_UNBOUNDED - 1u);
    return reader->at != start;
}

// the quantifier after `atom`, where one stands: `* + ? {n} {n,} {n,m}`, lazy after `?`; `{` that opens no quantifier
// is the atom's alone
static unsigned int cli_quantified(CliRegexpReader *reader, unsigned int atom)
{
    unsigned int letter = 0u;
    unsigned int min = 0u;
    unsigned int max = CLI_REGEXP_UNBOUNDED;
    if (!cli_peek(reader, &letter))
    {
        return atom;
    }
    if (letter == '*')
    {
        min = 0u;
    }
    else if (letter == '+')
    {
        min = 1u;
    }
    else if (letter == '?')
    {
        max = 1u;
    }
    else if (letter == '{')
    {
        const unsigned long long back = reader->at;
        reader->at += 1u;
        if (!cli_number(reader, &min))
        {
            reader->at = back;
            return atom;
        }
        max = min;
        if (cli_eat(reader, ','))
        {
            unsigned int peeked = 0u;
            const int closes = cli_peek(reader, &peeked) && (peeked == '}');
            max = (!closes && cli_number(reader, &max)) ? max : CLI_REGEXP_UNBOUNDED;
        }
        if (!cli_eat(reader, '}'))
        {
            reader->at = back;
            return atom;
        }
        reader->at -= 1u;
    }
    else
    {
        return atom;
    }
    reader->at += 1u;
    const unsigned int kind = reader->regexp->node[atom].kind;
    if ((kind == CLI_REGEXP_START) || (kind == CLI_REGEXP_END) || (kind == CLI_REGEXP_BOUNDARY) ||
        (kind == CLI_REGEXP_LOOK))
    {
        return cli_failed(reader, "a quantifier after an assertion");
    }
    const int greedy = !cli_eat(reader, '?');
    const unsigned int made = cli_node(reader, CLI_REGEXP_REPEAT);
    if (made != CLI_REGEXP_FAILED)
    {
        CliRegexpNode *const node = &reader->regexp->node[made];
        node->child = atom;
        node->min = min;
        node->max = max;
        node->greedy = greedy;
    }
    return made;
}

// a sequence of atoms, each with its quantifier, up to `|` or `)`
static unsigned int cli_sequence(CliRegexpReader *reader)
{
    unsigned int parts[CLI_REGEXP_LEVEL_MOST];
    unsigned int count = 0u;
    unsigned int letter = 0u;
    while (cli_peek(reader, &letter) && (letter != '|') && (letter != ')'))
    {
        const unsigned int atom = cli_atom(reader);
        const unsigned int quantified = (atom == CLI_REGEXP_FAILED) ? atom : cli_quantified(reader, atom);
        if (quantified == CLI_REGEXP_FAILED)
        {
            return quantified;
        }
        if (count >= CLI_REGEXP_LEVEL_MOST)
        {
            return cli_failed(reader, "more parts than a sequence holds");
        }
        parts[count] = quantified;
        count += 1u;
    }
    if (count == 0u)
    {
        return cli_node(reader, CLI_REGEXP_EMPTY);
    }
    return (count == 1u) ? parts[0] : cli_parented(reader, CLI_REGEXP_CONCAT, parts, count);
}

unsigned int cli_alternation(CliRegexpReader *reader)
{
    unsigned int options[CLI_REGEXP_LEVEL_MOST];
    unsigned int count = 0u;
    do
    {
        const unsigned int option = cli_sequence(reader);
        if (option == CLI_REGEXP_FAILED)
        {
            return option;
        }
        if (count >= CLI_REGEXP_LEVEL_MOST)
        {
            return cli_failed(reader, "more alternatives than an alternation holds");
        }
        options[count] = option;
        count += 1u;
    } while (cli_eat(reader, '|'));
    return (count == 1u) ? options[0] : cli_parented(reader, CLI_REGEXP_ALT, options, count);
}

// Each alternative of every alternation given its first characters, a place whose character none of them can start
// with passed over; each repeat whether a pass takes the text one way or none
static void cli_filled(CliRegexp *regexp, unsigned int index)
{
    CliRegexpNode *const node = &regexp->node[index];
    if ((node->kind == CLI_REGEXP_ALT) || (node->kind == CLI_REGEXP_CONCAT))
    {
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            cli_filled(regexp, regexp->part[node->first + at]);
        }
        for (unsigned int at = 0u; (node->kind == CLI_REGEXP_ALT) && (at < node->count); at += 1u)
        {
            regexp->part_first[node->first + at] = cli_regexp_first_of(regexp, regexp->part[node->first + at]);
        }
    }
    else if (node->kind == CLI_REGEXP_REPEAT)
    {
        cli_filled(regexp, node->child);
        node->one_way = cli_regexp_one_way(regexp, node->child);
    }
    else if ((node->kind == CLI_REGEXP_GROUP) || (node->kind == CLI_REGEXP_LOOK))
    {
        cli_filled(regexp, node->child);
    }
}

void cli_regexp_roomed(CliRegexp *regexp, CliRegexpRoom *room)
{
    regexp->node = room->node;
    regexp->item = room->item;
    regexp->part = room->part;
    regexp->part_first = room->part_first;
}

int cli_regexp_read(CliRegexp *regexp, const char *source, const char *flags, CliLine *said)
{
    static CliRegexpReader s_reader;
    memor.set(&(MemoriaCfg){.dst = &s_reader, .val = 0, .bytes = sizeof(s_reader)});
    s_reader.length = cli_regexp_letters(source, cellul.len(&(CatenaFinitaCfg){.src = source, .cap = CLI_CELLS_POOL}),
                                         s_reader.letters, CLI_REGEXP_LETTERS);
    s_reader.unicode = cellul.chr(&(CatenaFinitaCfg){.src = flags, .cap = CLI_CELLS_POOL, .byte = 'u'}) != NULL;
    s_reader.regexp = regexp;
    s_reader.source = source;
    s_reader.said = said;
    regexp->nodes = 0u;
    regexp->items = 0u;
    regexp->parts = 0u;
    regexp->fold = cellul.chr(&(CatenaFinitaCfg){.src = flags, .cap = CLI_CELLS_POOL, .byte = 'i'}) != NULL;
    const unsigned int root = cli_alternation(&s_reader);
    if (root == CLI_REGEXP_FAILED)
    {
        return 0;
    }
    if (s_reader.at < s_reader.length)
    {
        cli_failed(&s_reader, "an unmatched )");
        return 0;
    }
    regexp->root = root;
    regexp->groups = s_reader.groups;
    cli_filled(regexp, root);
    regexp->first = cli_regexp_first_of(regexp, root);
    return 1;
}
