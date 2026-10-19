// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_core_relation_schema.h: the schema the entries are held to, the counts, the device maps, and every
// relation read together (ruleset_core_relation.h includes the parts in order)
#ifndef RULESET_CORE_RELATION_SCHEMA_H
#define RULESET_CORE_RELATION_SCHEMA_H

#include "ruleset_core_relations.h"

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
        const unsigned int list = ruleset_core_relation_gives(entry->kind)    ? 0u
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
        else if ((list == 0u) && (ruleset_core_relation_parts(relations->text, entry->head, (unsigned char)' ') !=
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
                given = kind_fits &&
                        ruleset_core_equal(&relations->text[entry->name.first], entry->name.length,
                                           &schema->letters[lists[list][named].first], lists[list][named].length);
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
//
// Each of the others is a line a lane cannot be read through as it stands (P8): it holds between two languages that
// each write their own names rightly, and is no verdict on either set
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
        const RulesetCoreRelation kept[4] = {{RULESET_CORE_MAP_BREAKS, at, found[0]},
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
