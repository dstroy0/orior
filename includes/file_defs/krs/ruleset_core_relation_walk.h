// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_core_relation_walk.h: the entries of any files of rulesets, device maps and classifications read together,
// each line's type and its relation to the others named on it, and the walk over them (ruleset_core_relation.h
// includes the parts in order)
#ifndef RULESET_CORE_RELATION_WALK_H
#define RULESET_CORE_RELATION_WALK_H

// The walk reads each line of each file as an entry of its own. The first word of the line is its type, and the line
// names its relation to the others. No entry waits for another, and the walk reads no file before another.
//
// The walk then checks each entry against each other entry. It records each relation that no set of files must
// hold, with the file of each entry. From that, the caller reads a relation in one file, between two, or across the
// whole set.
//
// Nothing here knows a target. A register is what the text of a bank writes, and the words of a name are its parts at
// underscores.
//
// The caller gives the memory. That is an entry for each line, and a mark and a place on the stack of the walk for each
// entry. It is also RULESET_CORE_RELATION_TYPES relations for each entry, and one for each name of the schema

#include "ruleset_core_words.h"

// what an entry is, by the first word of its line. A line of a construct is by the construct that holds it. An
// arrangement of a device map is by its tabs. Each other line is by its first word alone. The channel, class, count and
// question of a classification are among those lines
#define RULESET_CORE_ENTRY_FORM 0u
#define RULESET_CORE_ENTRY_NOP 1u
#define RULESET_CORE_ENTRY_ERR 2u
#define RULESET_CORE_ENTRY_CONSTRUCT 3u
#define RULESET_CORE_ENTRY_LINE 4u
#define RULESET_CORE_ENTRY_BANK 5u
#define RULESET_CORE_ENTRY_FIXED 6u
#define RULESET_CORE_ENTRY_ROW 7u
#define RULESET_CORE_ENTRY_OTHER 8u

// an entry or a place that is none
#define RULESET_CORE_NONE 0xFFFFFFFFu

// The relations that the walk records, each between an entry and `other`. `other` is another entry, or what the
// relation names. That is a place in the head or text of the entry, a count, or the list of the schema with a name. The
// lists are 0 forms, 1 banks and 2 fixed registers.
//
//     - GIVEN_TWICE          an earlier entry of the same type gives the name.
//     - SAME_TEXT            the text of a form is the text of an earlier form, each parameter as its place in its head.
//     - RENAMED              a construct of one line writes its own parameters, in order, to the form `other`.
//     - MODIFIERS            names one word apart, with the same texts after each dot and the characters after it go.
//     - OPENED               a form is the form `other`, with literal operands opened as parameters of its own.
//     - LINE_NOT_GIVEN       a line of a construct writes a name that no entry gives.
//     - LINE_ARGUMENTS       a line of a construct gives the form `other` a count of arguments that it does not take.
//     - LOOP                 a construct reaches itself through its line `other`.
//     - PARAMETER_UNWRITTEN  a form takes its parameter at place `other` in its head, and its text never writes it.
//                            A name that opens with an underscore marks a parameter that the form takes and does not
//                            write, and does not count.
//     - PLACE_UNKNOWN        the text of a form writes a place, `other` characters into the text, that its head does
//                            not take.
//     - NO_TEXT              a form with nothing after its equals, which names neither nop nor err.
//     - NUMERIC_NAME         the text of a form uses the numeric name of the register that the fixed entry `other`
//                            names.
//     - SCHEMA_NOT_NAMED     an entry gives a name that the schema does not name.
//     - SCHEMA_NOT_GIVEN     no entry gives a name of the schema, at its place `entry` in the list `other`.
//     - SCHEMA_PARAMETERS    a form takes a count of parameters that the schema does not give.
//     - ROW_TWICE            an arrangement has the same operator and chain as an earlier one.
//     - COUNT_DISAGREES      a count that is not the `other` questions of that channel and class in its file.
//     - CLASS_NOT_NAMED      a question gets an answer with a class that its file does not name.
#define RULESET_CORE_RELATION_GIVEN_TWICE 0u
#define RULESET_CORE_RELATION_SAME_TEXT 1u
#define RULESET_CORE_RELATION_RENAMED 2u
#define RULESET_CORE_RELATION_MODIFIERS 3u
#define RULESET_CORE_RELATION_OPENED 4u
#define RULESET_CORE_RELATION_LINE_NOT_GIVEN 5u
#define RULESET_CORE_RELATION_LINE_ARGUMENTS 6u
#define RULESET_CORE_RELATION_LOOP 7u
#define RULESET_CORE_RELATION_PARAMETER_UNWRITTEN 8u
#define RULESET_CORE_RELATION_PLACE_UNKNOWN 9u
#define RULESET_CORE_RELATION_NO_TEXT 10u
#define RULESET_CORE_RELATION_NUMERIC_NAME 11u
#define RULESET_CORE_RELATION_SCHEMA_NOT_NAMED 12u
#define RULESET_CORE_RELATION_SCHEMA_NOT_GIVEN 13u
#define RULESET_CORE_RELATION_SCHEMA_PARAMETERS 14u
#define RULESET_CORE_RELATION_ROW_TWICE 15u
#define RULESET_CORE_RELATION_COUNT_DISAGREES 16u
#define RULESET_CORE_RELATION_CLASS_NOT_NAMED 17u
#define RULESET_CORE_RELATION_TYPES 18u

// The entries that a relation names witness it where each set with those entries holds it. An added file never
// undoes it. It is a verdict on any part of a set that the walk reads.
//
// A relation of absence, a name that no entry gives, is true only of a whole set. The walk does not know whether a set
// is whole: the file that gives the name undoes it. Such a relation is open. The walk keeps it for a later read to close
// or keep, and it is never a verdict.
//
// One file holds the counts and classes of a classification. The walk reads that file whole, and the file witnesses
// them
#define RULESET_CORE_RELATION_OPEN(type_)                                                                              \
    (((type_) == RULESET_CORE_RELATION_LINE_NOT_GIVEN) || ((type_) == RULESET_CORE_RELATION_SCHEMA_NOT_GIVEN))

// A relation between the writings of two forms is true of correct writings and of wrong ones. Examples are a form with
// the text of another, and a construct that writes another form under its own parameters. Others are two names a word
// and their modifiers apart, and a form that opens the literal operands of another.
//
// A target writes two operations one modifier apart, and a language writes one text for two names. The walk records
// and writes such a relation, and the bridge reads its pairs from it (klq_write.cu). It is no verdict. Each other
// relation that is not open is a verdict
#define RULESET_CORE_RELATION_VERDICT(type_)                                                                           \
    (!RULESET_CORE_RELATION_OPEN(type_) && ((type_) != RULESET_CORE_RELATION_SAME_TEXT) &&                             \
     ((type_) != RULESET_CORE_RELATION_RENAMED) && ((type_) != RULESET_CORE_RELATION_MODIFIERS) &&                     \
     ((type_) != RULESET_CORE_RELATION_OPENED))

// one line of one file. It holds its type, its file and its line number, and the construct that holds a line of a
// construct. It holds whether the line has an equals, its name, the words after the name up to the equals, and the text
// after it
struct RulesetCoreEntry
{
    unsigned int type;
    unsigned int file;
    unsigned int line;
    unsigned int owner;
    unsigned int equals;
    RulesetCoreSpan name;
    RulesetCoreSpan head;
    RulesetCoreSpan text;
};

// a relation recorded: its type, the entry it is of, and the other entry or place it names
struct RulesetCoreRelation
{
    unsigned int type;
    unsigned int entry;
    unsigned int other;
};

// the files that the walk reads together, and what it reads of them. The schema has its characters, NULL where the
// files have no schema. The characters of each file follow, each file a span of them. Then come the entries, the marks
// and stack of the walk, and the relations, each with the capacity that the caller gives. The walk counts a relation
// past the capacity and does not record it
struct RulesetCoreRelations
{
    RulesetCoreSchema schema;
    const unsigned char *text;
    const RulesetCoreSpan *files;
    unsigned int file_count;
    RulesetCoreEntry *entries;
    unsigned int entry_capacity;
    unsigned int entry_count;
    unsigned int *marks;
    unsigned int *stack;
    RulesetCoreRelation *relations;
    unsigned int relation_capacity;
    unsigned int relation_count;
};

// the place of `character` in the span `within` from `from` on, or its length where it is not there
CODEGEN_CORE unsigned int ruleset_core_relation_find(const unsigned char *text, RulesetCoreSpan within,
                                                     unsigned char character, unsigned int from)
{
    if (from >= within.length)
    {
        return within.length;
    }
    // Explicit casts read the found byte as a place in the span, which an unsigned int counts
    const unsigned char *const start = &text[within.first + from];
    const void *const found =
        EMBED_CALL(memor.chr, MemoriaCfg, .src = start, .bytes = within.length - from, .val = character);
    return (found != NULL) ? (from + (unsigned int)((const unsigned char *)found - start)) : within.length;
}

// part `index` of the span `within`, cut at `character`. Where `character` is a space, a run of it counts as one.
// Where the span has fewer parts, the result is an empty span past its end
CODEGEN_CORE RulesetCoreSpan ruleset_core_relation_part(const unsigned char *text, RulesetCoreSpan within,
                                                        unsigned char character, unsigned int index)
{
    unsigned int at = 0u;
    unsigned int counted = 0u;
    while (at <= within.length)
    {
        const unsigned int end = ruleset_core_relation_find(text, within, character, at);
        // a space between words may be several, and an empty part between two of them is no word
        if ((end > at) || (character != (unsigned char)' '))
        {
            if (counted == index)
            {
                const RulesetCoreSpan part = {within.first + at, end - at};
                return part;
            }
            counted += 1u;
        }
        at = end + 1u;
    }
    const RulesetCoreSpan none = {within.first + within.length, 0u};
    return none;
}

// how many parts the span `within` cut at `character`, as ruleset_core_relation_part counts them
CODEGEN_CORE unsigned int ruleset_core_relation_parts(const unsigned char *text, RulesetCoreSpan within,
                                                      unsigned char character)
{
    unsigned int counted = 0u;
    unsigned int at = 0u;
    while (at <= within.length)
    {
        const unsigned int end = ruleset_core_relation_find(text, within, character, at);
        counted += ((end > at) || (character != (unsigned char)' ')) ? 1u : 0u;
        at = end + 1u;
    }
    return counted;
}

// 1 where the spans `one` and `other` contain the same characters
CODEGEN_CORE int ruleset_core_relation_same(const unsigned char *text, RulesetCoreSpan one, RulesetCoreSpan other)
{
    return ruleset_core_equal(&text[one.first], one.length, &text[other.first], other.length);
}

// 1 where the span `word` is the text `literal`
CODEGEN_CORE int ruleset_core_relation_is(const unsigned char *text, RulesetCoreSpan word, const char *literal)
{
    // Explicit cast takes the length of a literal of the schema, which an unsigned int counts
    const unsigned int length =
        (unsigned int)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = literal, .cap = ~(size_t)0);
    return ruleset_core_equal(&text[word.first], word.length, (const unsigned char *)literal, length);
}

// the span `within` from `at` to its end
CODEGEN_CORE RulesetCoreSpan ruleset_core_relation_after(RulesetCoreSpan within, unsigned int at)
{
    const unsigned int clamped = (at < within.length) ? at : within.length;
    const RulesetCoreSpan after = {within.first + clamped, within.length - clamped};
    return after;
}

// an entry recorded where the caller made room for it
CODEGEN_CORE void ruleset_core_relation_entry(RulesetCoreRelations *relations, const RulesetCoreEntry *entry)
{
    if (relations->entry_count < relations->entry_capacity)
    {
        relations->entries[relations->entry_count] = *entry;
        relations->entry_count += 1u;
    }
}

// a relation counted, and recorded where the caller made room for it
CODEGEN_CORE void ruleset_core_relation_add(RulesetCoreRelations *relations, unsigned int type, unsigned int entry,
                                            unsigned int other)
{
    if (relations->relation_count < relations->relation_capacity)
    {
        const RulesetCoreRelation relation = {type, entry, other};
        relations->relations[relations->relation_count] = relation;
    }
    relations->relation_count += 1u;
}

// the entries of file `file`. Its first line names what the file is, and is no entry. A line that opens with # is a
// comment, and a blank line is nothing
CODEGEN_CORE void ruleset_core_relation_file(RulesetCoreRelations *relations, unsigned int file)
{
    const unsigned char *const text = relations->text;
    const RulesetCoreSpan whole = relations->files[file];
    unsigned int building = RULESET_CORE_NONE;
    unsigned int number = 0u;
    unsigned int at = 0u;
    while (at < whole.length)
    {
        const unsigned int end = ruleset_core_relation_find(text, whole, (unsigned char)'\n', at);
        // a checkout that also ends its lines with a carriage return keeps each line as its writer wrote it
        const unsigned int line_end = ((end > at) && (text[whole.first + end - 1u] == '\r')) ? (end - 1u) : end;
        const RulesetCoreSpan line = {whole.first + at, line_end - at};
        number += 1u;
        at = end + 1u;
        const RulesetCoreSpan first = ruleset_core_relation_part(text, line, (unsigned char)' ', 0u);
        if ((number == 1u) || (first.length == 0u) || (text[line.first] == '#'))
        {
            continue;
        }
        const unsigned int space = first.first + first.length - line.first;
        RulesetCoreEntry entry = {RULESET_CORE_ENTRY_OTHER,
                                  file,
                                  number,
                                  RULESET_CORE_NONE,
                                  0u,
                                  first,
                                  ruleset_core_relation_after(line, space),
                                  ruleset_core_relation_after(line, line.length)};
        if (building != RULESET_CORE_NONE)
        {
            if (ruleset_core_relation_is(text, line, "end"))
            {
                building = RULESET_CORE_NONE;
                continue;
            }
            entry.type = RULESET_CORE_ENTRY_LINE;
            entry.owner = building;
            ruleset_core_relation_entry(relations, &entry);
            continue;
        }
        const unsigned int type = ruleset_core_relation_is(text, first, "form")        ? RULESET_CORE_ENTRY_FORM
                                  : ruleset_core_relation_is(text, first, "nop")       ? RULESET_CORE_ENTRY_NOP
                                  : ruleset_core_relation_is(text, first, "err")       ? RULESET_CORE_ENTRY_ERR
                                  : ruleset_core_relation_is(text, first, "construct") ? RULESET_CORE_ENTRY_CONSTRUCT
                                  : ruleset_core_relation_is(text, first, "bank")      ? RULESET_CORE_ENTRY_BANK
                                  : ruleset_core_relation_is(text, first, "fixed")     ? RULESET_CORE_ENTRY_FIXED
                                                                                       : RULESET_CORE_ENTRY_OTHER;
        const unsigned int tab = ruleset_core_relation_find(text, line, (unsigned char)'\t', 0u);
        if (type != RULESET_CORE_ENTRY_OTHER)
        {
            // a named entry's head is its name and parameters, and its text follows the head's "= "
            const RulesetCoreSpan rest = ruleset_core_relation_after(line, space + 1u);
            const unsigned int equals = ruleset_core_relation_find(text, rest, (unsigned char)'=', 0u);
            const RulesetCoreSpan head = {rest.first, equals};
            const unsigned int text_at =
                (equals == rest.length)
                    ? rest.length
                    : ((((equals + 1u) < rest.length) && (text[rest.first + equals + 1u] == ' ')) ? (equals + 2u)
                                                                                                  : (equals + 1u));
            entry.type = type;
            entry.equals = (equals != rest.length) ? 1u : 0u;
            entry.name = ruleset_core_relation_part(text, head, (unsigned char)' ', 0u);
            entry.head = ruleset_core_relation_after(head, entry.name.first + entry.name.length - head.first);
            entry.text = ruleset_core_relation_after(rest, text_at);
        }
        else if (tab != line.length)
        {
            // an arrangement's cells are its operator, its nodes, its chain, its cost and its runs
            entry.type = RULESET_CORE_ENTRY_ROW;
            entry.name = ruleset_core_relation_part(text, line, (unsigned char)'\t', 0u);
            entry.head = ruleset_core_relation_part(text, line, (unsigned char)'\t', 2u);
        }
        ruleset_core_relation_entry(relations, &entry);
        if (type == RULESET_CORE_ENTRY_CONSTRUCT)
        {
            building = relations->entry_count - 1u;
        }
    }
}

// 1 where an entry of type `type` gives a name: a form, nop, err or construct
CODEGEN_CORE int ruleset_core_relation_gives(unsigned int type)
{
    return (type == RULESET_CORE_ENTRY_FORM) || (type == RULESET_CORE_ENTRY_NOP) || (type == RULESET_CORE_ENTRY_ERR) ||
           (type == RULESET_CORE_ENTRY_CONSTRUCT);
}

// the first entry that gives the name `name`, or RULESET_CORE_NONE where none does
CODEGEN_CORE unsigned int ruleset_core_relation_giver(const RulesetCoreRelations *relations, RulesetCoreSpan name)
{
    for (unsigned int at = 0u; at < relations->entry_count; at += 1u)
    {
        const RulesetCoreEntry *const entry = &relations->entries[at];
        if (ruleset_core_relation_gives(entry->type) && ruleset_core_relation_same(relations->text, entry->name, name))
        {
            return at;
        }
    }
    return RULESET_CORE_NONE;
}

// the place of `word` among the words of entry `entry`'s head, or their count where it is none of them
CODEGEN_CORE unsigned int ruleset_core_relation_place(const RulesetCoreRelations *relations,
                                                      const RulesetCoreEntry *entry, RulesetCoreSpan word)
{
    const unsigned int count = ruleset_core_relation_parts(relations->text, entry->head, (unsigned char)' ');
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        if (ruleset_core_relation_same(
                relations->text, word,
                ruleset_core_relation_part(relations->text, entry->head, (unsigned char)' ', place)))
        {
            return place;
        }
    }
    return count;
}

// the parameter that the text of `entry` names at `at`, a place of its text that holds {. The result is the name
// between the braces. It is an empty span where no } closes it, or where the braces do not hold a name of characters,
// digits and underscores. A brace of the own language of the target holds no such name
CODEGEN_CORE RulesetCoreSpan ruleset_core_relation_braced(const RulesetCoreRelations *relations,
                                                          const RulesetCoreEntry *entry, unsigned int at)
{
    const unsigned int close = ruleset_core_relation_find(relations->text, entry->text, (unsigned char)'}', at + 1u);
    const RulesetCoreSpan none = {entry->text.first + at, 0u};
    const RulesetCoreSpan named = {entry->text.first + at + 1u, close - at - 1u};
    // a name: one byte or more, every one a letter, a digit or an underscore. Explicit cast reads the bytes as text
    const int name =
        (close != entry->text.length) && (named.length != 0u) &&
        (EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = (const char *)&relations->text[named.first],
                    .cap = named.length,
                    .other = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") == named.length);
    return (name != 0) ? named : none;
}

// the place past a dot at `at` of `entry`'s text and the characters and digits after it, or `at` where no dot is there
CODEGEN_CORE unsigned int ruleset_core_relation_struck(const RulesetCoreRelations *relations,
                                                       const RulesetCoreEntry *entry, unsigned int at)
{
    unsigned int past = at;
    while ((past < entry->text.length) && (relations->text[entry->text.first + past] == '.'))
    {
        past += 1u;
        // the letters and digits after the dot. Explicit cast reads the bytes as text
        past += (past < entry->text.length)
                    ? EMBED_CALL(cellul.span, CatenaFinitaCfg,
                                 .src = (const char *)&relations->text[entry->text.first + past],
                                 .cap = entry->text.length - past,
                                 .other = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")
                    : 0u;
    }
    return past;
}

// 1 where the character is one an operand ends at
// the bytes that ruleset_core_relation_ends takes as the end of an operand, for a span over them
#define RULESET_CORE_RELATION_ENDS " ,;[]()+\\\t{"

CODEGEN_CORE int ruleset_core_relation_ends(unsigned char character)
{
    return (character == ' ') || (character == ',') || (character == ';') || (character == '[') || (character == ']') ||
           (character == '(') || (character == ')') || (character == '+') || (character == '\\') || (character == '\t') ||
           (character == '{');
}

// The texts of forms `one` and `other`, walked together, each parameter as its place in its own head. With `struck`,
// the walk passes over each dot and the characters and digits after it in both. With `opened`, a parameter of `one`
// past the count of `other` is for a literal operand of `other`. The walk counts those operands into `opened`. The
// result is 1 where the walk reaches both ends together
CODEGEN_CORE int ruleset_core_relation_walk(const RulesetCoreRelations *relations, const RulesetCoreEntry *one,
                                            const RulesetCoreEntry *other, int struck, unsigned int *opened)
{
    const unsigned char *const text = relations->text;
    const unsigned int other_count = ruleset_core_relation_parts(text, other->head, (unsigned char)' ');
    unsigned int at = 0u;
    unsigned int other_at = 0u;
    for (;;)
    {
        at = (struck != 0) ? ruleset_core_relation_struck(relations, one, at) : at;
        other_at = (struck != 0) ? ruleset_core_relation_struck(relations, other, other_at) : other_at;
        if ((at == one->text.length) || (other_at == other->text.length))
        {
            return (at == one->text.length) && (other_at == other->text.length);
        }
        const unsigned char character = text[one->text.first + at];
        const unsigned char other_character = text[other->text.first + other_at];
        const RulesetCoreSpan named = ruleset_core_relation_braced(relations, one, at);
        const RulesetCoreSpan other_named = ruleset_core_relation_braced(relations, other, other_at);
        const unsigned int place = (character == '{') && (named.length != 0u)
                                       ? ruleset_core_relation_place(relations, one, named)
                                       : RULESET_CORE_NONE;
        const unsigned int other_place = (other_character == '{') && (other_named.length != 0u)
                                             ? ruleset_core_relation_place(relations, other, other_named)
                                             : RULESET_CORE_NONE;
        const unsigned int one_count = ruleset_core_relation_parts(text, one->head, (unsigned char)' ');
        const int is_parameter = (place != RULESET_CORE_NONE) && (place < one_count);
        const int other_is_parameter = (other_place != RULESET_CORE_NONE) && (other_place < other_count);
        if ((opened != 0) && is_parameter && (place >= other_count) && !other_is_parameter &&
            !ruleset_core_relation_ends(other_character))
        {
            // a parameter `one` added, located for the operand `other` writes as it is
            // Explicit cast reads the bytes as text
            other_at += (other_at < other->text.length)
                            ? EMBED_CALL(cellul.span_not, CatenaFinitaCfg,
                                         .src = (const char *)&text[other->text.first + other_at],
                                         .cap = other->text.length - other_at, .other = RULESET_CORE_RELATION_ENDS)
                            : 0u;
            at = named.first + named.length + 1u - one->text.first;
            *opened += 1u;
            continue;
        }
        if (is_parameter || other_is_parameter)
        {
            if (!is_parameter || !other_is_parameter || (place != other_place))
            {
                return 0;
            }
            at = named.first + named.length + 1u - one->text.first;
            other_at = other_named.first + other_named.length + 1u - other->text.first;
            continue;
        }
        if (character != other_character)
        {
            return 0;
        }
        at += 1u;
        other_at += 1u;
    }
}

#endif
