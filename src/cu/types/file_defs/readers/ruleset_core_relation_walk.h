// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_core_relation_walk.h: the entries of any files of rulesets, device maps and classifications read together,
// each line's kind and its relation to the others named on it, and the walk over them (ruleset_core_relation.h
// includes the parts in order)
#ifndef RULESET_CORE_RELATION_WALK_H
#define RULESET_CORE_RELATION_WALK_H

// Each line of each file is read as an entry of its own, its kind the line's first word and its relation to the
// others named on the same line: no entry waits on another and no file is read before another. Every entry is then
// held against every other, and a relation no set of files should hold is kept: the file each entry came from is kept
// with it, and the caller reads a relation within one file, between two, or across the whole set from that. Nothing
// here knows a target: a register is whatever a bank's text writes, and a name's words are its parts at underscores.
// The memory is the caller's: an entry a line, a mark and a place on the walk's stack an entry, and relations as many
// as RULESET_CORE_RELATION_KINDS an entry and one a name of the schema

#include "ruleset_core_words.h"

// what an entry is, by the first word of its line: a construct's line by the construct it stands in, a device map's
// arrangement by its tabs, and any other line, a classification's channel, class, count or question among them, by
// its first word alone
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

// The relations kept, each between an entry and `other`: another entry, or where the relation says so, a place in the
// entry's head or text, a count, or which of the schema's lists a name is of (0 forms, 1 banks, 2 fixed registers)
//
//     GIVEN_TWICE          a name given by an earlier entry of the same kind
//     SAME_TEXT            a form's text is an earlier form's, each parameter read as its place in its own head
//     RENAMED              a construct of one line writing its own parameters, in order, to the form `other`
//     MODIFIERS            names one word apart, and texts the same once every dot and the letters after it are struck
//     OPENED               a form is the form `other` with literal operands opened as parameters of its own
//     LINE_NOT_GIVEN       a construct's line writes a name no entry gives
//     LINE_ARGUMENTS       a construct's line gives the form `other` another count of arguments than it takes
//     LOOP                 a construct reaches itself through its line `other`
//     PARAMETER_UNWRITTEN  a form takes its parameter at place `other` in its head and its text never writes it
//     PLACE_UNKNOWN        a form's text writes a place, `other` letters into the text, that its head does not take
//     NO_TEXT              a form with nothing after its equals, which says neither nop nor err
//     NUMERIC_NAME         a form's text uses the numeric name of the register the fixed entry `other` names
//     SCHEMA_NOT_NAMED     a name given that the schema does not name
//     SCHEMA_NOT_GIVEN     a name of the schema, at its place `entry` in the list `other`, that no entry gives
//     SCHEMA_PARAMETERS    a form taking another count of parameters than the schema says
//     ROW_TWICE            an arrangement the same operator and chain as an earlier one
//     COUNT_DISAGREES      a count other than the `other` questions its file writes of that channel and class
//     CLASS_NOT_NAMED      a question answered with a class its file does not name
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
#define RULESET_CORE_RELATION_KINDS 18u

// A relation is witnessed by the entries it names where every set holding those entries holds it: a file added never
// undoes it, and it is a verdict on whatever part of a set is read. A relation of absence, a name no entry gives, holds
// only of a set that is whole, and no set read is known to be whole: the file that gives the name undoes it. Such a
// relation is open, kept for a later read to close or keep and never a verdict. A classification's counts and classes
// are held within one file, which is read whole, and its file witnesses them
#define RULESET_CORE_RELATION_OPEN(kind_)                                                                              \
    (((kind_) == RULESET_CORE_RELATION_LINE_NOT_GIVEN) || ((kind_) == RULESET_CORE_RELATION_SCHEMA_NOT_GIVEN))

// A relation between two forms' writings holds of writings that are right as well as of wrong ones: a form's text
// another's, a construct writing another form under its own parameters, two names a word and their modifiers apart,
// and a form opening another's literal operands. A target writes two operations one modifier apart, and a language
// writes one text for two names. Such a relation is kept and written, the bridge's pairs are read from it
// (klq_write.cu), and it is no verdict; every other relation that is not open is one
#define RULESET_CORE_RELATION_VERDICT(kind_)                                                                           \
    (!RULESET_CORE_RELATION_OPEN(kind_) && ((kind_) != RULESET_CORE_RELATION_SAME_TEXT) &&                             \
     ((kind_) != RULESET_CORE_RELATION_RENAMED) && ((kind_) != RULESET_CORE_RELATION_MODIFIERS) &&                     \
     ((kind_) != RULESET_CORE_RELATION_OPENED))

// one line of one file: its kind, the file and the line it is, the construct a construct's line stands in, whether
// the line has an equals, its name, the words after the name up to the equals, and the text after it
struct RulesetCoreEntry
{
    unsigned int kind;
    unsigned int file;
    unsigned int line;
    unsigned int owner;
    unsigned int equals;
    RulesetCoreSpan name;
    RulesetCoreSpan head;
    RulesetCoreSpan text;
};

// a relation kept: its kind, the entry it is of, and the other entry or place it names
struct RulesetCoreRelation
{
    unsigned int kind;
    unsigned int entry;
    unsigned int other;
};

// the files read together and what is read of them: the schema, its letters NULL where the files are read against
// none; the letters of every file, each file a span of them; the entries, the walk's marks and stack, and the
// relations, each with the capacity the caller sized. A relation past the capacity is counted and not kept
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

// the place of `letter` in the span `within` from `from` on, or its length where it is not there
CODEGEN_CORE unsigned int ruleset_core_relation_find(const unsigned char *text, RulesetCoreSpan within,
                                                     unsigned char letter, unsigned int from)
{
    unsigned int found = within.length;
    for (unsigned int at = from; (found == within.length) && (at < within.length); at += 1u)
    {
        found = (text[within.first + at] == letter) ? at : found;
    }
    return found;
}

// part `index` of the span `within` cut at `letter`, runs of it counted as one where `letter` is a space, or an empty
// span past its end where it has fewer parts
CODEGEN_CORE RulesetCoreSpan ruleset_core_relation_part(const unsigned char *text, RulesetCoreSpan within,
                                                        unsigned char letter, unsigned int index)
{
    unsigned int at = 0u;
    unsigned int counted = 0u;
    while (at <= within.length)
    {
        const unsigned int end = ruleset_core_relation_find(text, within, letter, at);
        // a space between words may be several, and an empty part between two of them is no word
        if ((end > at) || (letter != (unsigned char)' '))
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

// how many parts the span `within` has cut at `letter`, as ruleset_core_relation_part counts them
CODEGEN_CORE unsigned int ruleset_core_relation_parts(const unsigned char *text, RulesetCoreSpan within,
                                                      unsigned char letter)
{
    unsigned int counted = 0u;
    unsigned int at = 0u;
    while (at <= within.length)
    {
        const unsigned int end = ruleset_core_relation_find(text, within, letter, at);
        counted += ((end > at) || (letter != (unsigned char)' ')) ? 1u : 0u;
        at = end + 1u;
    }
    return counted;
}

// 1 where the spans `one` and `other` hold the same letters
CODEGEN_CORE int ruleset_core_relation_same(const unsigned char *text, RulesetCoreSpan one, RulesetCoreSpan other)
{
    return ruleset_core_equal(&text[one.first], one.length, &text[other.first], other.length);
}

// 1 where the span `word` is the text `literal`
CODEGEN_CORE int ruleset_core_relation_is(const unsigned char *text, RulesetCoreSpan word, const char *literal)
{
    unsigned int length = 0u;
    while (literal[length] != '\0')
    {
        length += 1u;
    }
    return ruleset_core_equal(&text[word.first], word.length, (const unsigned char *)literal, length);
}

// the span `within` from `at` to its end
CODEGEN_CORE RulesetCoreSpan ruleset_core_relation_after(RulesetCoreSpan within, unsigned int at)
{
    const unsigned int kept = (at < within.length) ? at : within.length;
    const RulesetCoreSpan after = {within.first + kept, within.length - kept};
    return after;
}

// an entry kept where the caller made room for it
CODEGEN_CORE void ruleset_core_relation_entry(RulesetCoreRelations *relations, const RulesetCoreEntry *entry)
{
    if (relations->entry_count < relations->entry_capacity)
    {
        relations->entries[relations->entry_count] = *entry;
        relations->entry_count += 1u;
    }
}

// a relation counted, and kept where the caller made room for it
CODEGEN_CORE void ruleset_core_relation_keep(RulesetCoreRelations *relations, unsigned int kind, unsigned int entry,
                                             unsigned int other)
{
    if (relations->relation_count < relations->relation_capacity)
    {
        const RulesetCoreRelation relation = {kind, entry, other};
        relations->relations[relations->relation_count] = relation;
    }
    relations->relation_count += 1u;
}

// the entries of file `file`: its first line names what the file is and is no entry, a line opening with # is a
// comment and a blank line is nothing
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
        // a checkout that ends its lines with a carriage return as well leaves each line as it was written
        const unsigned int kept = ((end > at) && (text[whole.first + end - 1u] == '\r')) ? (end - 1u) : end;
        const RulesetCoreSpan line = {whole.first + at, kept - at};
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
            entry.kind = RULESET_CORE_ENTRY_LINE;
            entry.owner = building;
            ruleset_core_relation_entry(relations, &entry);
            continue;
        }
        const unsigned int kind = ruleset_core_relation_is(text, first, "form")        ? RULESET_CORE_ENTRY_FORM
                                  : ruleset_core_relation_is(text, first, "nop")       ? RULESET_CORE_ENTRY_NOP
                                  : ruleset_core_relation_is(text, first, "err")       ? RULESET_CORE_ENTRY_ERR
                                  : ruleset_core_relation_is(text, first, "construct") ? RULESET_CORE_ENTRY_CONSTRUCT
                                  : ruleset_core_relation_is(text, first, "bank")      ? RULESET_CORE_ENTRY_BANK
                                  : ruleset_core_relation_is(text, first, "fixed")     ? RULESET_CORE_ENTRY_FIXED
                                                                                      : RULESET_CORE_ENTRY_OTHER;
        const unsigned int tab = ruleset_core_relation_find(text, line, (unsigned char)'\t', 0u);
        if (kind != RULESET_CORE_ENTRY_OTHER)
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
            entry.kind = kind;
            entry.equals = (equals != rest.length) ? 1u : 0u;
            entry.name = ruleset_core_relation_part(text, head, (unsigned char)' ', 0u);
            entry.head = ruleset_core_relation_after(head, entry.name.first + entry.name.length - head.first);
            entry.text = ruleset_core_relation_after(rest, text_at);
        }
        else if (tab != line.length)
        {
            // an arrangement's cells are its operator, its nodes, its chain, its cost and its runs
            entry.kind = RULESET_CORE_ENTRY_ROW;
            entry.name = ruleset_core_relation_part(text, line, (unsigned char)'\t', 0u);
            entry.head = ruleset_core_relation_part(text, line, (unsigned char)'\t', 2u);
        }
        ruleset_core_relation_entry(relations, &entry);
        if (kind == RULESET_CORE_ENTRY_CONSTRUCT)
        {
            building = relations->entry_count - 1u;
        }
    }
}

// 1 where an entry of kind `kind` gives a name: a form, nop, err or construct
CODEGEN_CORE int ruleset_core_relation_gives(unsigned int kind)
{
    return (kind == RULESET_CORE_ENTRY_FORM) || (kind == RULESET_CORE_ENTRY_NOP) || (kind == RULESET_CORE_ENTRY_ERR) ||
           (kind == RULESET_CORE_ENTRY_CONSTRUCT);
}

// the first entry giving the name `name`, or RULESET_CORE_NONE where none does
CODEGEN_CORE unsigned int ruleset_core_relation_giver(const RulesetCoreRelations *relations, RulesetCoreSpan name)
{
    for (unsigned int at = 0u; at < relations->entry_count; at += 1u)
    {
        const RulesetCoreEntry *const entry = &relations->entries[at];
        if (ruleset_core_relation_gives(entry->kind) && ruleset_core_relation_same(relations->text, entry->name, name))
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
        if (ruleset_core_relation_same(relations->text, word,
                                       ruleset_core_relation_part(relations->text, entry->head, (unsigned char)' ',
                                                                  place)))
        {
            return place;
        }
    }
    return count;
}

// the parameter `entry`'s text names at `at`, a place of its text holding {: the name between the braces, or an empty
// span where no } closes it or what the braces hold is not a name of letters, digits and underscores, as a brace of
// the target's own language is not
CODEGEN_CORE RulesetCoreSpan ruleset_core_relation_braced(const RulesetCoreRelations *relations,
                                                          const RulesetCoreEntry *entry, unsigned int at)
{
    const unsigned int close = ruleset_core_relation_find(relations->text, entry->text, (unsigned char)'}', at + 1u);
    const RulesetCoreSpan none = {entry->text.first + at, 0u};
    const RulesetCoreSpan named = {entry->text.first + at + 1u, close - at - 1u};
    int name = (close != entry->text.length) && (named.length != 0u);
    for (unsigned int letter = 0u; (name != 0) && (letter < named.length); letter += 1u)
    {
        const unsigned char one = relations->text[named.first + letter];
        name = ((one >= 'a') && (one <= 'z')) || ((one >= 'A') && (one <= 'Z')) || ((one >= '0') && (one <= '9')) ||
               (one == '_');
    }
    return (name != 0) ? named : none;
}

// the place past a dot at `at` of `entry`'s text and the letters and digits after it, or `at` where no dot is there
CODEGEN_CORE unsigned int ruleset_core_relation_struck(const RulesetCoreRelations *relations,
                                                       const RulesetCoreEntry *entry, unsigned int at)
{
    unsigned int past = at;
    while ((past < entry->text.length) && (relations->text[entry->text.first + past] == '.'))
    {
        past += 1u;
        unsigned char letter = (past < entry->text.length) ? relations->text[entry->text.first + past] : 0u;
        while ((past < entry->text.length) &&
               (((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
                ((letter >= '0') && (letter <= '9'))))
        {
            past += 1u;
            letter = (past < entry->text.length) ? relations->text[entry->text.first + past] : 0u;
        }
    }
    return past;
}

// 1 where the letter is one an operand ends at
CODEGEN_CORE int ruleset_core_relation_ends(unsigned char letter)
{
    return (letter == ' ') || (letter == ',') || (letter == ';') || (letter == '[') || (letter == ']') ||
           (letter == '(') || (letter == ')') || (letter == '+') || (letter == '\\') || (letter == '\t') ||
           (letter == '{');
}

// The texts of forms `one` and `other` walked together, each parameter read as its place in its own head. With
// `struck`, every dot and the letters and digits after it are passed over in both. With `opened`, a parameter of
// `one` past `other`'s count stands for a literal operand of `other`, and the operands it stood for are counted into
// `opened`. 1 where the walk reaches both ends together
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
        const unsigned char letter = text[one->text.first + at];
        const unsigned char other_letter = text[other->text.first + other_at];
        const RulesetCoreSpan named = ruleset_core_relation_braced(relations, one, at);
        const RulesetCoreSpan other_named = ruleset_core_relation_braced(relations, other, other_at);
        const unsigned int place = (letter == '{') && (named.length != 0u)
                                       ? ruleset_core_relation_place(relations, one, named)
                                       : RULESET_CORE_NONE;
        const unsigned int other_place = (other_letter == '{') && (other_named.length != 0u)
                                             ? ruleset_core_relation_place(relations, other, other_named)
                                             : RULESET_CORE_NONE;
        const unsigned int one_count = ruleset_core_relation_parts(text, one->head, (unsigned char)' ');
        const int is_parameter = (place != RULESET_CORE_NONE) && (place < one_count);
        const int other_is_parameter = (other_place != RULESET_CORE_NONE) && (other_place < other_count);
        if ((opened != 0) && is_parameter && (place >= other_count) && !other_is_parameter &&
            !ruleset_core_relation_ends(other_letter))
        {
            // a parameter `one` added, standing for the operand `other` writes as it stands
            while ((other_at < other->text.length) && !ruleset_core_relation_ends(text[other->text.first + other_at]))
            {
                other_at += 1u;
            }
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
        if (letter != other_letter)
        {
            return 0;
        }
        at += 1u;
        other_at += 1u;
    }
}

#endif
