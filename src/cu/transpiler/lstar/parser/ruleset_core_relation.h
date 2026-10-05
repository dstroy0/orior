// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_core_relation.h: the absurd relations among the entries of any files of rulesets, device maps and
// classifications read together (ruleset_core.h includes the parts in order)
#ifndef RULESET_CORE_RELATION_H
#define RULESET_CORE_RELATION_H

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

// 1 where the names `one` and `other` are one word apart: as many words with one different, or one with a word more
// and the rest the same, words being the parts at underscores
CODEGEN_CORE int ruleset_core_relation_one_word(const unsigned char *text, RulesetCoreSpan one, RulesetCoreSpan other)
{
    const unsigned int count = ruleset_core_relation_parts(text, one, (unsigned char)'_');
    const unsigned int other_count = ruleset_core_relation_parts(text, other, (unsigned char)'_');
    if (count == other_count)
    {
        unsigned int different = 0u;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            different += ruleset_core_relation_same(text, ruleset_core_relation_part(text, one, (unsigned char)'_', at),
                                                    ruleset_core_relation_part(text, other, (unsigned char)'_', at))
                             ? 0u
                             : 1u;
        }
        return different == 1u;
    }
    const RulesetCoreSpan longer = (count > other_count) ? one : other;
    const RulesetCoreSpan shorter = (count > other_count) ? other : one;
    const unsigned int longer_count = (count > other_count) ? count : other_count;
    const unsigned int shorter_count = (count > other_count) ? other_count : count;
    if (longer_count != (shorter_count + 1u))
    {
        return 0;
    }
    for (unsigned int left_out = 0u; left_out < longer_count; left_out += 1u)
    {
        int same = 1;
        for (unsigned int at = 0u; (same != 0) && (at < shorter_count); at += 1u)
        {
            same = ruleset_core_relation_same(text, ruleset_core_relation_part(text, shorter, (unsigned char)'_', at),
                                              ruleset_core_relation_part(text, longer, (unsigned char)'_',
                                                                         (at < left_out) ? at : (at + 1u)));
        }
        if (same != 0)
        {
            return 1;
        }
    }
    return 0;
}

// 1 where the fixed entry `fixed`'s register is a numeric name of a bank's register: the bank's letters before and
// after its {n}, and digits between
CODEGEN_CORE int ruleset_core_relation_banked(const RulesetCoreRelations *relations, const RulesetCoreEntry *fixed)
{
    const unsigned char *const text = relations->text;
    for (unsigned int at = 0u; at < relations->entry_count; at += 1u)
    {
        const RulesetCoreEntry *const bank = &relations->entries[at];
        const unsigned int open = (bank->kind == RULESET_CORE_ENTRY_BANK)
                                      ? ruleset_core_relation_find(text, bank->text, (unsigned char)'{', 0u)
                                      : 0u;
        // a bank whose text is its number alone, an immediate, names no register
        if ((bank->kind != RULESET_CORE_ENTRY_BANK) || (open == bank->text.length) || (bank->text.length <= 3u) ||
            (fixed->text.length < (bank->text.length - 2u)))
        {
            continue;
        }
        const unsigned int after = bank->text.length - open - 3u;
        int numeric = ruleset_core_equal(&text[fixed->text.first], open, &text[bank->text.first], open) &&
                      ruleset_core_equal(&text[fixed->text.first + fixed->text.length - after], after,
                                         &text[bank->text.first + open + 3u], after);
        const unsigned int digits = fixed->text.length - open - after;
        numeric = numeric && (digits != 0u);
        for (unsigned int digit = 0u; digit < digits; digit += 1u)
        {
            const unsigned char letter = text[fixed->text.first + open + digit];
            numeric = numeric && (letter >= '0') && (letter <= '9');
        }
        if (numeric != 0)
        {
            return 1;
        }
    }
    return 0;
}

// 1 where `entry`'s text holds the fixed entry `fixed`'s register as an operand of its own, not part of a longer word
CODEGEN_CORE int ruleset_core_relation_operand(const RulesetCoreRelations *relations, const RulesetCoreEntry *entry,
                                               const RulesetCoreEntry *fixed)
{
    const unsigned char *const text = relations->text;
    const unsigned int length = fixed->text.length;
    for (unsigned int at = 0u; (length != 0u) && ((at + length) <= entry->text.length); at += 1u)
    {
        // a tab or a line's end written as an escape ends the word before it, though its second letter is a letter
        const int escaped = (at >= 2u) && (text[entry->text.first + at - 2u] == '\\');
        const unsigned char before = ((at == 0u) || escaped) ? (unsigned char)' ' : text[entry->text.first + at - 1u];
        const unsigned char past =
            ((at + length) == entry->text.length) ? (unsigned char)' ' : text[entry->text.first + at + length];
        const int word_before = ((before >= 'a') && (before <= 'z')) || ((before >= 'A') && (before <= 'Z')) ||
                                ((before >= '0') && (before <= '9')) || (before == '_') || (before == '{');
        const int word_past = ((past >= 'a') && (past <= 'z')) || ((past >= 'A') && (past <= 'Z')) ||
                              ((past >= '0') && (past <= '9')) || (past == '_');
        if (!word_before && !word_past &&
            ruleset_core_equal(&text[entry->text.first + at], length, &text[fixed->text.first], length))
        {
            return 1;
        }
    }
    return 0;
}

// what each form says of itself: a text that says nothing, a parameter it never writes and a place it does not take
CODEGEN_CORE void ruleset_core_relation_form(RulesetCoreRelations *relations, unsigned int at)
{
    const unsigned char *const text = relations->text;
    const RulesetCoreEntry *const entry = &relations->entries[at];
    if ((entry->equals == 0u) || (entry->text.length == 0u))
    {
        ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_NO_TEXT, at, RULESET_CORE_NONE);
        return;
    }
    const unsigned int count = ruleset_core_relation_parts(text, entry->head, (unsigned char)' ');
    for (unsigned int place = 0u; place < count; place += 1u)
    {
        const RulesetCoreSpan parameter = ruleset_core_relation_part(text, entry->head, (unsigned char)' ', place);
        int written = 0;
        for (unsigned int letter = 0u; (written == 0) && (letter < entry->text.length); letter += 1u)
        {
            const RulesetCoreSpan named = (text[entry->text.first + letter] == '{')
                                              ? ruleset_core_relation_braced(relations, entry, letter)
                                              : ruleset_core_relation_after(entry->text, entry->text.length);
            written = (named.length != 0u) && ruleset_core_relation_same(text, named, parameter);
        }
        if (written == 0)
        {
            ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_PARAMETER_UNWRITTEN, at, place);
            break;
        }
    }
    for (unsigned int letter = 0u; letter < entry->text.length; letter += 1u)
    {
        const RulesetCoreSpan named = (text[entry->text.first + letter] == '{')
                                          ? ruleset_core_relation_braced(relations, entry, letter)
                                          : ruleset_core_relation_after(entry->text, entry->text.length);
        if ((named.length != 0u) && (ruleset_core_relation_place(relations, entry, named) == count))
        {
            ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_PLACE_UNKNOWN, at, letter);
            break;
        }
    }
    for (unsigned int fixed = 0u; fixed < relations->entry_count; fixed += 1u)
    {
        const RulesetCoreEntry *const register_entry = &relations->entries[fixed];
        if ((register_entry->kind == RULESET_CORE_ENTRY_FIXED) &&
            ruleset_core_relation_banked(relations, register_entry) &&
            ruleset_core_relation_operand(relations, entry, register_entry))
        {
            ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_NUMERIC_NAME, at, fixed);
            break;
        }
    }
}

// what each form is to every other form: the first earlier entry of its name and kind, the first earlier form of its
// text, the first earlier form one word and its modifiers apart, and the first form it opens
CODEGEN_CORE void ruleset_core_relation_pairs(RulesetCoreRelations *relations, unsigned int at)
{
    const unsigned char *const text = relations->text;
    const RulesetCoreEntry *const entry = &relations->entries[at];
    const int gives = ruleset_core_relation_gives(entry->kind);
    unsigned int found[4] = {RULESET_CORE_NONE, RULESET_CORE_NONE, RULESET_CORE_NONE, RULESET_CORE_NONE};
    for (unsigned int earlier = 0u; earlier < relations->entry_count; earlier += 1u)
    {
        const RulesetCoreEntry *const other = &relations->entries[earlier];
        const int same_kind = gives ? ruleset_core_relation_gives(other->kind)
                                    : ((other->kind == entry->kind) && ((entry->kind == RULESET_CORE_ENTRY_BANK) ||
                                                                        (entry->kind == RULESET_CORE_ENTRY_FIXED) ||
                                                                        (entry->kind == RULESET_CORE_ENTRY_ROW)));
        if ((earlier < at) && (found[0] == RULESET_CORE_NONE) && same_kind &&
            ruleset_core_relation_same(text, entry->name, other->name) &&
            ((entry->kind != RULESET_CORE_ENTRY_ROW) || ruleset_core_relation_same(text, entry->head, other->head)))
        {
            found[0] = earlier;
        }
        const int both_forms = (entry->kind == RULESET_CORE_ENTRY_FORM) && (other->kind == RULESET_CORE_ENTRY_FORM) &&
                               (entry->text.length != 0u) && (other->text.length != 0u) && (earlier != at);
        const int same_text = both_forms && ruleset_core_relation_walk(relations, entry, other, 0, 0);
        if ((earlier < at) && (found[1] == RULESET_CORE_NONE) && same_text)
        {
            found[1] = earlier;
        }
        if ((earlier < at) && (found[2] == RULESET_CORE_NONE) && both_forms && !same_text &&
            ruleset_core_relation_one_word(text, entry->name, other->name) &&
            ruleset_core_relation_walk(relations, entry, other, 1, 0))
        {
            found[2] = earlier;
        }
        unsigned int opened = 0u;
        if ((found[3] == RULESET_CORE_NONE) && both_forms &&
            (ruleset_core_relation_parts(text, entry->head, (unsigned char)' ') >
             ruleset_core_relation_parts(text, other->head, (unsigned char)' ')) &&
            ruleset_core_relation_walk(relations, entry, other, 0, &opened) && (opened != 0u))
        {
            found[3] = earlier;
        }
    }
    const unsigned int kinds[4] = {(entry->kind == RULESET_CORE_ENTRY_ROW) ? RULESET_CORE_RELATION_ROW_TWICE
                                                                           : RULESET_CORE_RELATION_GIVEN_TWICE,
                                   RULESET_CORE_RELATION_SAME_TEXT, RULESET_CORE_RELATION_MODIFIERS,
                                   RULESET_CORE_RELATION_OPENED};
    for (unsigned int kind = 0u; kind < 4u; kind += 1u)
    {
        if (found[kind] != RULESET_CORE_NONE)
        {
            ruleset_core_relation_keep(relations, kinds[kind], at, found[kind]);
        }
    }
}

// what each construct's lines are: a name no entry gives, another count of arguments than the form takes, the
// construct one line naming another form under its own parameters, and a walk from it that comes back to it
CODEGEN_CORE void ruleset_core_relation_construct(RulesetCoreRelations *relations, unsigned int at)
{
    const unsigned char *const text = relations->text;
    const RulesetCoreEntry *const construct = &relations->entries[at];
    unsigned int lines = 0u;
    unsigned int only = RULESET_CORE_NONE;
    for (unsigned int line = 0u; line < relations->entry_count; line += 1u)
    {
        const RulesetCoreEntry *const written = &relations->entries[line];
        if ((written->kind != RULESET_CORE_ENTRY_LINE) || (written->owner != at))
        {
            continue;
        }
        lines += 1u;
        only = line;
        const unsigned int giver = ruleset_core_relation_giver(relations, written->name);
        if (giver == RULESET_CORE_NONE)
        {
            ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_LINE_NOT_GIVEN, line, RULESET_CORE_NONE);
        }
        else if (ruleset_core_relation_parts(text, written->head, (unsigned char)' ') !=
                 ruleset_core_relation_parts(text, relations->entries[giver].head, (unsigned char)' '))
        {
            ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_LINE_ARGUMENTS, line, giver);
        }
    }
    if (lines == 1u)
    {
        const RulesetCoreEntry *const written = &relations->entries[only];
        const unsigned int count = ruleset_core_relation_parts(text, construct->head, (unsigned char)' ');
        int renamed = (count == ruleset_core_relation_parts(text, written->head, (unsigned char)' '));
        for (unsigned int place = 0u; (renamed != 0) && (place < count); place += 1u)
        {
            renamed = ruleset_core_relation_same(text,
                                                 ruleset_core_relation_part(text, construct->head, (unsigned char)' ',
                                                                            place),
                                                 ruleset_core_relation_part(text, written->head, (unsigned char)' ',
                                                                            place));
        }
        const unsigned int giver = ruleset_core_relation_giver(relations, written->name);
        if ((renamed != 0) && (giver != RULESET_CORE_NONE))
        {
            ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_RENAMED, at, giver);
        }
    }
    // the walk from the construct over the constructs its lines name, each marked with this walk's number once seen
    const unsigned int walk = at + 1u;
    unsigned int depth = 0u;
    relations->stack[depth] = at;
    depth += 1u;
    relations->marks[at] = walk;
    while (depth != 0u)
    {
        depth -= 1u;
        const unsigned int from = relations->stack[depth];
        for (unsigned int line = 0u; line < relations->entry_count; line += 1u)
        {
            const RulesetCoreEntry *const written = &relations->entries[line];
            if ((written->kind != RULESET_CORE_ENTRY_LINE) || (written->owner != from))
            {
                continue;
            }
            const unsigned int giver = ruleset_core_relation_giver(relations, written->name);
            if (giver == at)
            {
                ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_LOOP, at, line);
                return;
            }
            if ((giver != RULESET_CORE_NONE) && (relations->entries[giver].kind == RULESET_CORE_ENTRY_CONSTRUCT) &&
                (relations->marks[giver] != walk))
            {
                relations->marks[giver] = walk;
                relations->stack[depth] = giver;
                depth += 1u;
            }
        }
    }
}

// the place of the name `name` among `count` names of the schema, or `count` where it is none of them
CODEGEN_CORE unsigned int ruleset_core_relation_schema_find(const RulesetCoreRelations *relations,
                                                            const RulesetCoreSpan *names, unsigned int count,
                                                            RulesetCoreSpan name)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (ruleset_core_equal(&relations->text[name.first], name.length, &relations->schema.letters[names[at].first],
                               names[at].length))
        {
            return at;
        }
    }
    return count;
}

// every entry held against the schema, and every name of the schema against the entries
CODEGEN_CORE void ruleset_core_relation_schema(RulesetCoreRelations *relations)
{
    const RulesetCoreSchema *const schema = &relations->schema;
    const RulesetCoreSpan *const lists[3] = {schema->forms, schema->banks, schema->fixed};
    const unsigned int counts[3] = {schema->form_count, schema->bank_count, schema->fixed_count};
    for (unsigned int at = 0u; at < relations->entry_count; at += 1u)
    {
        const RulesetCoreEntry *const entry = &relations->entries[at];
        const unsigned int list = ruleset_core_relation_gives(entry->kind)        ? 0u
                                  : (entry->kind == RULESET_CORE_ENTRY_BANK)  ? 1u
                                  : (entry->kind == RULESET_CORE_ENTRY_FIXED) ? 2u
                                                                              : 3u;
        if (list == 3u)
        {
            continue;
        }
        const unsigned int named = ruleset_core_relation_schema_find(relations, lists[list], counts[list], entry->name);
        if (named == counts[list])
        {
            ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_SCHEMA_NOT_NAMED, at, list);
        }
        else if ((list == 0u) &&
                 (ruleset_core_relation_parts(relations->text, entry->head, (unsigned char)' ') !=
                  schema->form_parameters[named]))
        {
            ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_SCHEMA_PARAMETERS, at, named);
        }
    }
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        for (unsigned int named = 0u; named < counts[list]; named += 1u)
        {
            int given = 0;
            for (unsigned int at = 0u; (given == 0) && (at < relations->entry_count); at += 1u)
            {
                const RulesetCoreEntry *const entry = &relations->entries[at];
                const int kind_fits = (list == 0u)   ? ruleset_core_relation_gives(entry->kind)
                                      : (list == 1u) ? (entry->kind == RULESET_CORE_ENTRY_BANK)
                                                     : (entry->kind == RULESET_CORE_ENTRY_FIXED);
                given = kind_fits && ruleset_core_equal(&relations->text[entry->name.first], entry->name.length,
                                                        &schema->letters[lists[list][named].first],
                                                        lists[list][named].length);
            }
            if (given == 0)
            {
                ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_SCHEMA_NOT_GIVEN, named, list);
            }
        }
    }
}

// a classification's counts held against the questions its file writes, and each question's class against the
// classes its file names: a channel is named `channel <name> ...`, a class `class <name> ...`, a count `count
// <channel> <class> <number>`, and a question opens with a channel's name, its class the next word
CODEGEN_CORE void ruleset_core_relation_counts(RulesetCoreRelations *relations, unsigned int at)
{
    const unsigned char *const text = relations->text;
    const RulesetCoreEntry *const entry = &relations->entries[at];
    if (entry->kind != RULESET_CORE_ENTRY_OTHER)
    {
        return;
    }
    const RulesetCoreSpan first = ruleset_core_relation_part(text, entry->head, (unsigned char)' ', 0u);
    const RulesetCoreSpan second = ruleset_core_relation_part(text, entry->head, (unsigned char)' ', 1u);
    unsigned int asked = 0u;
    int question = 0;
    int class_named = 0;
    for (unsigned int other_at = 0u; other_at < relations->entry_count; other_at += 1u)
    {
        const RulesetCoreEntry *const other = &relations->entries[other_at];
        if ((other->kind != RULESET_CORE_ENTRY_OTHER) || (other->file != entry->file))
        {
            continue;
        }
        const RulesetCoreSpan other_first = ruleset_core_relation_part(text, other->head, (unsigned char)' ', 0u);
        if (ruleset_core_relation_is(text, entry->name, "count") &&
            ruleset_core_relation_same(text, other->name, first) &&
            ruleset_core_relation_same(text, other_first, second))
        {
            asked += 1u;
        }
        question = question || (ruleset_core_relation_is(text, other->name, "channel") &&
                                ruleset_core_relation_same(text, other_first, entry->name));
        class_named = class_named || (ruleset_core_relation_is(text, other->name, "class") &&
                                      ruleset_core_relation_same(text, other_first, first));
    }
    if (ruleset_core_relation_is(text, entry->name, "count"))
    {
        const RulesetCoreSpan digits = ruleset_core_relation_part(text, entry->head, (unsigned char)' ', 2u);
        unsigned int number = 0u;
        for (unsigned int digit = 0u; digit < digits.length; digit += 1u)
        {
            // a count is a few digits, and one past 32 bits is written as no count holds
            number = (number * 10u) + (unsigned int)(text[digits.first + digit] - '0');
        }
        if (number != asked)
        {
            ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_COUNT_DISAGREES, at, asked);
        }
    }
    else if (question && !class_named)
    {
        ruleset_core_relation_keep(relations, RULESET_CORE_RELATION_CLASS_NOT_NAMED, at, RULESET_CORE_NONE);
    }
}

// The relations of a map from one set of files to another, the two read apart, each name written in `from` against the
// same name written in `to`, each relation between an entry of `from` and `other`:
//
//     BREAKS     two names one text in `from` and two texts in `to`: a reading of `from` cannot say which `to` writes,
//                `other` the second name's entry in `from`
//     COLLAPSES  two names two texts in `from` and one text in `to`, `other` the second name's entry in `from`
//     KIND       a name `from` gives as one kind and `to` as another, of form, nop, err and construct, `other` the
//                entry of `to`
//     ABSENT     a name `from` gives and no entry of `to` does, which a file of `to` added undoes: open
#define RULESET_CORE_MAP_BREAKS 0u
#define RULESET_CORE_MAP_COLLAPSES 1u
#define RULESET_CORE_MAP_KIND 2u
#define RULESET_CORE_MAP_ABSENT 3u
#define RULESET_CORE_MAP_KINDS 4u
#define RULESET_CORE_MAP_OPEN(kind_) ((kind_) == RULESET_CORE_MAP_ABSENT)

// the first entry of `to` giving the name `name` of `from`, or RULESET_CORE_NONE where none does
CODEGEN_CORE unsigned int ruleset_core_map_giver(const RulesetCoreRelations *from, RulesetCoreSpan name,
                                                 const RulesetCoreRelations *to)
{
    for (unsigned int at = 0u; at < to->entry_count; at += 1u)
    {
        const RulesetCoreEntry *const entry = &to->entries[at];
        if (ruleset_core_relation_gives(entry->kind) &&
            ruleset_core_equal(&to->text[entry->name.first], entry->name.length, &from->text[name.first], name.length))
        {
            return at;
        }
    }
    return RULESET_CORE_NONE;
}

// 1 where entries `one` and `other` of `relations` are forms with the same text, each parameter read as its place
CODEGEN_CORE int ruleset_core_map_same(const RulesetCoreRelations *relations, unsigned int one, unsigned int other)
{
    const RulesetCoreEntry *const left = &relations->entries[one];
    const RulesetCoreEntry *const right = &relations->entries[other];
    return (left->kind == RULESET_CORE_ENTRY_FORM) && (right->kind == RULESET_CORE_ENTRY_FORM) &&
           (left->text.length != 0u) && (right->text.length != 0u) &&
           ruleset_core_relation_walk(relations, left, right, 0, 0);
}

// Every relation of the map from `from` to `to`, both read by ruleset_core_relations, into `maps`, which holds
// `capacity`: for each name `from` gives, its kind and whether `to` gives it, and the first earlier name it is one text
// with on one side and two on the other. The count found, those past the capacity counted and not kept; three an entry
// of `from` at most
CODEGEN_CORE unsigned int ruleset_core_map(const RulesetCoreRelations *from, const RulesetCoreRelations *to,
                                           RulesetCoreRelation *maps, unsigned int capacity)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < from->entry_count; at += 1u)
    {
        const RulesetCoreEntry *const entry = &from->entries[at];
        if (!ruleset_core_relation_gives(entry->kind) || (ruleset_core_relation_giver(from, entry->name) != at))
        {
            continue;
        }
        const unsigned int giver = ruleset_core_map_giver(from, entry->name, to);
        unsigned int found[2] = {RULESET_CORE_NONE, RULESET_CORE_NONE};
        for (unsigned int earlier = 0u; (giver != RULESET_CORE_NONE) && (earlier < at); earlier += 1u)
        {
            const RulesetCoreEntry *const other = &from->entries[earlier];
            const unsigned int other_giver = ruleset_core_relation_gives(other->kind)
                                                 ? ruleset_core_map_giver(from, other->name, to)
                                                 : RULESET_CORE_NONE;
            if ((other_giver == RULESET_CORE_NONE) || (ruleset_core_relation_giver(from, other->name) != earlier))
            {
                continue;
            }
            const int same_from = ruleset_core_map_same(from, at, earlier);
            const int same_to = ruleset_core_map_same(to, giver, other_giver);
            const int both_forms = (entry->kind == RULESET_CORE_ENTRY_FORM) &&
                                   (other->kind == RULESET_CORE_ENTRY_FORM) &&
                                   (to->entries[giver].kind == RULESET_CORE_ENTRY_FORM) &&
                                   (to->entries[other_giver].kind == RULESET_CORE_ENTRY_FORM);
            found[0] = ((found[0] == RULESET_CORE_NONE) && both_forms && same_from && !same_to) ? earlier : found[0];
            found[1] = ((found[1] == RULESET_CORE_NONE) && both_forms && !same_from && same_to) ? earlier : found[1];
        }
        const RulesetCoreRelation kept[4] = {
            {RULESET_CORE_MAP_BREAKS, at, found[0]},
            {RULESET_CORE_MAP_COLLAPSES, at, found[1]},
            {RULESET_CORE_MAP_KIND, at, giver},
            {RULESET_CORE_MAP_ABSENT, at, RULESET_CORE_NONE}};
        const int holds[4] = {found[0] != RULESET_CORE_NONE, found[1] != RULESET_CORE_NONE,
                              (giver != RULESET_CORE_NONE) && (to->entries[giver].kind != entry->kind),
                              giver == RULESET_CORE_NONE};
        for (unsigned int kind = 0u; kind < 4u; kind += 1u)
        {
            if (holds[kind] != 0)
            {
                if (count < capacity)
                {
                    maps[count] = kept[kind];
                }
                count += 1u;
            }
        }
    }
    return count;
}

// every file's entries read, and every absurd relation among them kept: each entry against every other, each
// construct's lines and walk, each classification's counts, and where a schema is given, the whole against it
CODEGEN_CORE void ruleset_core_relations(RulesetCoreRelations *relations)
{
    relations->entry_count = 0u;
    relations->relation_count = 0u;
    for (unsigned int file = 0u; file < relations->file_count; file += 1u)
    {
        ruleset_core_relation_file(relations, file);
    }
    for (unsigned int at = 0u; at < relations->entry_count; at += 1u)
    {
        relations->marks[at] = 0u;
    }
    for (unsigned int at = 0u; at < relations->entry_count; at += 1u)
    {
        const unsigned int kind = relations->entries[at].kind;
        ruleset_core_relation_pairs(relations, at);
        if (kind == RULESET_CORE_ENTRY_FORM)
        {
            ruleset_core_relation_form(relations, at);
        }
        if (kind == RULESET_CORE_ENTRY_CONSTRUCT)
        {
            ruleset_core_relation_construct(relations, at);
        }
        ruleset_core_relation_counts(relations, at);
    }
    if (relations->schema.letters != 0)
    {
        ruleset_core_relation_schema(relations);
    }
}

#endif
