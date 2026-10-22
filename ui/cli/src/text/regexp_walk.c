// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_walk.c: each node walked from a place in the text; the characters a node's match can start with; whether a
// pass of a node takes the text one way or none, and where such a pass ends; and a match tried at one place
#include "regexp_match_internal.h"

#include "../command_line/cells.h"

// the ASCII characters a node's one letter takes, as two words of bits
static void cli_ascii_of(const CliRegexp *regexp, unsigned int node, unsigned long long *ascii)
{
    ascii[0] = 0ull;
    ascii[1] = 0ull;
    for (unsigned int letter = 0u; letter < 128u; letter += 1u)
    {
        ascii[letter / 64u] |= (cli_regexp_one(regexp, node, letter) == 1) ? (1ull << (letter % 64u)) : 0ull;
    }
}

static CliRegexpFirst cli_union(CliRegexpFirst a, CliRegexpFirst b)
{
    const CliRegexpFirst both = {
        {a.ascii[0] | b.ascii[0], a.ascii[1] | b.ascii[1]}, a.other || b.other, a.empty || b.empty};
    return both;
}

CliRegexpFirst cli_regexp_first_of(const CliRegexp *regexp, unsigned int index)
{
    const CliRegexpNode *const node = &regexp->node[index];
    CliRegexpFirst set = {{0ull, 0ull}, 0, 0};
    switch (node->kind)
    {
    case CLI_REGEXP_CHAR:
    case CLI_REGEXP_ANY:
    case CLI_REGEXP_CLASS:
        cli_ascii_of(regexp, index, set.ascii);
        set.other = 1;
        return set;
    case CLI_REGEXP_GROUP:
        return cli_regexp_first_of(regexp, node->child);
    case CLI_REGEXP_ALT:
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            set = cli_union(set, cli_regexp_first_of(regexp, regexp->part[node->first + at]));
        }
        return set;
    case CLI_REGEXP_CONCAT:
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            const CliRegexpFirst own = cli_regexp_first_of(regexp, regexp->part[node->first + at]);
            CliRegexpFirst taken = own;
            taken.empty = 0;
            set = cli_union(set, taken);
            if (!own.empty)
            {
                return set;
            }
        }
        set.empty = 1;
        return set;
    case CLI_REGEXP_REPEAT: {
        CliRegexpFirst own = cli_regexp_first_of(regexp, node->child);
        own.empty = own.empty || (node->min == 0u);
        return own;
    }
    case CLI_REGEXP_BACK:
        set.ascii[0] = ~0ull;
        set.ascii[1] = ~0ull;
        set.other = 1;
        set.empty = 1;
        return set;
    default:
        set.empty = 1;
        return set;
    }
}

// The characters a node can take first, those past ASCII as one in `past`: a character of the text no two
// alternatives can both take first leaves a pass one way to go. 1, or 0 where the node takes none it can name
static int cli_starts(const CliRegexp *regexp, unsigned int index, unsigned long long *own, int *past)
{
    const CliRegexpNode *const node = &regexp->node[index];
    switch (node->kind)
    {
    case CLI_REGEXP_CHAR:
        cli_ascii_of(regexp, index, own);
        *past = (node->letter >= 128u) || (regexp->fold && (cli_regexp_property(CLI_REGEXP_LETTER, node->letter) != 0));
        return 1;
    case CLI_REGEXP_ANY:
        cli_ascii_of(regexp, index, own);
        *past = 1;
        return 1;
    case CLI_REGEXP_CLASS:
        cli_ascii_of(regexp, index, own);
        *past = node->negated;
        for (unsigned int at = 0u; !*past && (at < node->count); at += 1u)
        {
            const CliRegexpItem *const item = &regexp->item[node->first + at];
            *past = (item->kind == CLI_REGEXP_RANGE)   ? ((item->high >= 128u) || regexp->fold)
                    : (item->kind == CLI_REGEXP_DIGIT) ? item->negated
                                                       : 1;
        }
        return 1;
    case CLI_REGEXP_GROUP:
        return (node->group == CLI_REGEXP_NONE) && cli_starts(regexp, node->child, own, past);
    case CLI_REGEXP_CONCAT:
        return (node->count != 0u) && cli_starts(regexp, regexp->part[node->first], own, past);
    case CLI_REGEXP_ALT: {
        own[0] = 0ull;
        own[1] = 0ull;
        *past = 0;
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            unsigned long long option[2];
            int option_past = 0;
            if (!cli_starts(regexp, regexp->part[node->first + at], option, &option_past))
            {
                return 0;
            }
            own[0] |= option[0];
            own[1] |= option[1];
            *past = *past || option_past;
        }
        return 1;
    }
    default:
        return 0;
    }
}

int cli_regexp_one_way(const CliRegexp *regexp, unsigned int index)
{
    const CliRegexpNode *const node = &regexp->node[index];
    switch (node->kind)
    {
    case CLI_REGEXP_CHAR:
    case CLI_REGEXP_ANY:
    case CLI_REGEXP_CLASS:
        return 1;
    case CLI_REGEXP_GROUP:
        return (node->group == CLI_REGEXP_NONE) && cli_regexp_one_way(regexp, node->child);
    case CLI_REGEXP_CONCAT: {
        int all = node->count != 0u;
        for (unsigned int at = 0u; all && (at < node->count); at += 1u)
        {
            all = cli_regexp_one_way(regexp, regexp->part[node->first + at]);
        }
        return all;
    }
    case CLI_REGEXP_ALT: {
        unsigned long long seen[2] = {0ull, 0ull};
        int seen_past = 0;
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            const unsigned int option = regexp->part[node->first + at];
            unsigned long long own[2];
            int past = 0;
            if (!cli_starts(regexp, option, own, &past) || !cli_regexp_one_way(regexp, option))
            {
                return 0;
            }
            if (((seen[0] & own[0]) != 0ull) || ((seen[1] & own[1]) != 0ull) || (seen_past && past))
            {
                return 0;
            }
            seen[0] |= own[0];
            seen[1] |= own[1];
            seen_past = seen_past || past;
        }
        return 1;
    }
    default:
        return 0;
    }
}

unsigned long long cli_regexp_pass(const CliRegexp *regexp, unsigned int index, const CliRegexpRun *run,
                                   unsigned long long at)
{
    const CliRegexpNode *const node = &regexp->node[index];
    switch (node->kind)
    {
    case CLI_REGEXP_CHAR:
    case CLI_REGEXP_ANY:
    case CLI_REGEXP_CLASS:
        return ((at < run->length) && (cli_regexp_one(regexp, index, run->text[at]) == 1)) ? (at + 1ull)
                                                                                           : CLI_REGEXP_NO_PASS;
    case CLI_REGEXP_GROUP:
        return cli_regexp_pass(regexp, node->child, run, at);
    case CLI_REGEXP_CONCAT: {
        unsigned long long to = at;
        for (unsigned int part = 0u; (to != CLI_REGEXP_NO_PASS) && (part < node->count); part += 1u)
        {
            to = cli_regexp_pass(regexp, regexp->part[node->first + part], run, to);
        }
        return to;
    }
    case CLI_REGEXP_ALT:
        // no two alternatives take the same character first: the first that passes takes the pass
        for (unsigned int option = 0u; option < node->count; option += 1u)
        {
            const unsigned long long to = cli_regexp_pass(regexp, regexp->part[node->first + option], run, at);
            if (to != CLI_REGEXP_NO_PASS)
            {
                return to;
            }
        }
        return CLI_REGEXP_NO_PASS;
    default:
        return CLI_REGEXP_NO_PASS;
    }
}

// a lookahead at `at`: the rest handed on where the inner holds, or where it does not for `(?!`, every group it set
// let go where it does not stand
static int cli_looked(const CliRegexp *regexp, const CliRegexpNode *node, unsigned long long at, CliRegexpRun *run,
                      const CliRegexpThen *then)
{
    CliRegexpSpan kept[CLI_REGEXP_GROUPS];
    memor.cpy(&(MemoriaCfg){.dst = kept, .src = run->groups, .bytes = (regexp->groups + 1u) * sizeof(kept[0])});
    const CliRegexpThen frame = {CLI_THEN_TRUE, 0u, 0u, 0u, 0ull, NULL};
    const int found = cli_regexp_walk(regexp, node->child, at, run, &frame);
    if (node->negated)
    {
        memor.cpy(&(MemoriaCfg){.dst = run->groups, .src = kept, .bytes = (regexp->groups + 1u) * sizeof(kept[0])});
        return !found && cli_regexp_then(regexp, then, at, run);
    }
    if (found && cli_regexp_then(regexp, then, at, run))
    {
        return 1;
    }
    memor.cpy(&(MemoriaCfg){.dst = run->groups, .src = kept, .bytes = (regexp->groups + 1u) * sizeof(kept[0])});
    return 0;
}

int cli_regexp_walk(const CliRegexp *regexp, unsigned int index, unsigned long long at, CliRegexpRun *run,
                    const CliRegexpThen *then)
{
    run->steps += 1ull;
    if (run->steps > CLI_REGEXP_STEPS)
    {
        return 0;
    }
    const CliRegexpNode *const node = &regexp->node[index];
    switch (node->kind)
    {
    case CLI_REGEXP_EMPTY:
        return cli_regexp_then(regexp, then, at, run);
    case CLI_REGEXP_CHAR:
    case CLI_REGEXP_ANY:
    case CLI_REGEXP_CLASS:
        return (at < run->length) && (cli_regexp_one(regexp, index, run->text[at]) == 1) &&
               cli_regexp_then(regexp, then, at + 1ull, run);
    case CLI_REGEXP_START:
        return (at == 0ull) && cli_regexp_then(regexp, then, at, run);
    case CLI_REGEXP_END:
        return (at == run->length) && cli_regexp_then(regexp, then, at, run);
    case CLI_REGEXP_BOUNDARY: {
        const int before = (at > 0ull) && cli_regexp_word(run->text[at - 1u]);
        const int after = (at < run->length) && cli_regexp_word(run->text[at]);
        return ((before != after) != node->negated) && cli_regexp_then(regexp, then, at, run);
    }
    case CLI_REGEXP_GROUP: {
        if (node->group == CLI_REGEXP_NONE)
        {
            return cli_regexp_walk(regexp, node->child, at, run, then);
        }
        const CliRegexpThen frame = {CLI_THEN_GROUP, 0u, node->group, 0u, at, then};
        return cli_regexp_walk(regexp, node->child, at, run, &frame);
    }
    case CLI_REGEXP_LOOK:
        return cli_looked(regexp, node, at, run, then);
    case CLI_REGEXP_CONCAT:
        return cli_regexp_chain(regexp, index, 0u, at, run, then);
    case CLI_REGEXP_ALT:
        for (unsigned int option = 0u; option < node->count; option += 1u)
        {
            const CliRegexpFirst *const first = &regexp->part_first[node->first + option];
            const int may = first->empty || ((at < run->length) && cli_regexp_allows(first, run->text[at]));
            if (may && cli_regexp_walk(regexp, regexp->part[node->first + option], at, run, then))
            {
                return 1;
            }
        }
        return 0;
    case CLI_REGEXP_REPEAT: {
        const unsigned int child = regexp->node[node->child].kind;
        if ((child == CLI_REGEXP_CHAR) || (child == CLI_REGEXP_ANY) || (child == CLI_REGEXP_CLASS))
        {
            return cli_regexp_counted(regexp, node, at, run, then);
        }
        if (node->one_way)
        {
            return cli_regexp_passed(regexp, node, at, run, then);
        }
        return cli_regexp_repeat(regexp, index, 0u, at, run, then);
    }
    default: {
        // a backreference: the group's text again, or nothing where the group took no part
        if ((node->group > regexp->groups) || !run->groups[node->group].set)
        {
            return cli_regexp_then(regexp, then, at, run);
        }
        const unsigned long long from = run->groups[node->group].from;
        const unsigned long long length = run->groups[node->group].to - from;
        if ((at + length) > run->length)
        {
            return 0;
        }
        for (unsigned long long step = 0ull; step < length; step += 1ull)
        {
            if (!cli_regexp_same(regexp, run->text[from + step], run->text[at + step]))
            {
                return 0;
            }
        }
        return cli_regexp_then(regexp, then, at + length, run);
    }
    }
}

int cli_regexp_match_into(const CliRegexp *regexp, const unsigned int *text, unsigned long long length,
                          unsigned long long at, CliRegexpSpan *groups)
{
    if (regexp->anchored && (at != 0ull))
    {
        return 0;
    }
    if (!regexp->first.empty && !((at < length) && cli_regexp_allows(&regexp->first, text[at])))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = groups, .val = 0, .bytes = (regexp->groups + 1u) * sizeof(groups[0])});
    const char mark = 0;
    // the repeats' ends kept on this stack until they outgrow it
    unsigned int ends[CLI_REGEXP_ENDS_STACKED];
    CliRegexpRun run = {text, length, groups, 0ull, 0ull, (uintptr_t)&mark, ends, 0ull, CLI_REGEXP_ENDS_STACKED, 0};
    const CliRegexpThen end = {CLI_THEN_END, 0u, 0u, 0u, 0ull, NULL};
    const int found = cli_regexp_walk(regexp, regexp->root, at, &run, &end);
    if (run.ends_held)
    {
        cli_cell_given(run.ends);
    }
    if (found)
    {
        groups[0] = (CliRegexpSpan){at, run.end, 1};
    }
    return found;
}
