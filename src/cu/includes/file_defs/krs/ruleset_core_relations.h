// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_core_relations.h: the absurd relations held among the entries: names a word apart, banks, operands,
// forms, pairs and constructs (ruleset_core_relation.h includes the parts in order)
#ifndef RULESET_CORE_RELATIONS_H
#define RULESET_CORE_RELATIONS_H

#include "ruleset_core_relation_walk.h"

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
            same = ruleset_core_relation_same(
                text, ruleset_core_relation_part(text, shorter, (unsigned char)'_', at),
                ruleset_core_relation_part(text, longer, (unsigned char)'_', (at < left_out) ? at : (at + 1u)));
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
        // a place naming a fixed register of the set is that register, and a place the head need not take
        int fixed_named = 0;
        for (unsigned int fixed = 0u; (named.length != 0u) && (fixed_named == 0) && (fixed < relations->entry_count);
             fixed += 1u)
        {
            fixed_named = (relations->entries[fixed].kind == RULESET_CORE_ENTRY_FIXED) &&
                          ruleset_core_relation_same(text, named, relations->entries[fixed].name);
        }
        if ((named.length != 0u) && (fixed_named == 0) &&
            (ruleset_core_relation_place(relations, entry, named) == count))
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
    const unsigned int kinds[4] = {
        (entry->kind == RULESET_CORE_ENTRY_ROW) ? RULESET_CORE_RELATION_ROW_TWICE : RULESET_CORE_RELATION_GIVEN_TWICE,
        RULESET_CORE_RELATION_SAME_TEXT, RULESET_CORE_RELATION_MODIFIERS, RULESET_CORE_RELATION_OPENED};
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
            renamed = ruleset_core_relation_same(
                text, ruleset_core_relation_part(text, construct->head, (unsigned char)' ', place),
                ruleset_core_relation_part(text, written->head, (unsigned char)' ', place));
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

#endif
