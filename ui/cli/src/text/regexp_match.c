// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_match.c: a pattern matched at one place by backtracking. Each node takes the text where it can and hands the
// place it stops at to what comes after it, a chain of frames on the stack: the rest of a sequence, a group's end, a
// lookahead's true, a repetition's next pass, or the match's end. A repeat over what takes the text one way at a place
// or not at all steps along without going deeper, its passes' ends kept in the match's own arena, on its stack until
// they outgrow it and then in a cell twice as large each time they fill theirs, given back as the match ends
#include "regexp_match_internal.h"

#include "../command_line/cells.h"

// room for one more end in the run's ends, moved to a cell of twice the room where they fill theirs: 1, or 0 where they
// hold the most or the cellblock cannot meet the cell
static int match_end_room(CliRegexpRun *run)
{
    if (run->ends_top < run->ends_room)
    {
        return 1;
    }
    if (run->ends_room >= CLI_REGEXP_ENDS_MOST)
    {
        return 0;
    }
    const unsigned long long room = run->ends_room * 2ull;
    unsigned int *const grown = cli_cell_taken(room * sizeof(unsigned int), "a match's repeats' ends");
    if (grown == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = grown, .src = run->ends, .bytes = (size_t)(run->ends_top * sizeof(unsigned int))});
    if (run->ends_held)
    {
        cli_cell_given(run->ends);
    }
    run->ends = grown;
    run->ends_room = room;
    run->ends_held = 1;
    return 1;
}

int cli_regexp_word(unsigned int letter)
{
    return ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
           ((letter >= '0') && (letter <= '9')) || (letter == '_');
}

int cli_regexp_space(unsigned int letter)
{
    static const unsigned int s_spaces[] = {'\t',    '\n',    0x0bu,   0x0cu,   '\r',    ' ',     0xa0u,
                                            0x1680u, 0x2028u, 0x2029u, 0x202fu, 0x205fu, 0x3000u, 0xfeffu};
    for (unsigned int at = 0u; at < (sizeof(s_spaces) / sizeof(s_spaces[0])); at += 1u)
    {
        if (letter == s_spaces[at])
        {
            return 1;
        }
    }
    return (letter >= 0x2000u) && (letter <= 0x200au);
}

int cli_regexp_ends_line(unsigned int letter)
{
    return (letter == '\n') || (letter == '\r') || (letter == 0x2028u) || (letter == 0x2029u);
}

int cli_regexp_same(const CliRegexp *regexp, unsigned int a, unsigned int b)
{
    return (a == b) || (regexp->fold && (cli_regexp_lower(a) == cli_regexp_lower(b)));
}

// whether one item of a class holds of `letter`
static int cli_item_holds(const CliRegexpItem *item, unsigned int letter)
{
    switch (item->kind)
    {
    case CLI_REGEXP_RANGE:
        return (letter >= item->low) && (letter <= item->high);
    case CLI_REGEXP_DIGIT:
        return ((letter >= '0') && (letter <= '9')) != item->negated;
    case CLI_REGEXP_WORD:
        return cli_regexp_word(letter) != item->negated;
    case CLI_REGEXP_SPACE:
        return cli_regexp_space(letter) != item->negated;
    default:
        return cli_regexp_property(item->low, letter) != item->negated;
    }
}

// whether the class node `node` holds of `letter`: any item holds of it, or under `i` of its lower or upper case
static int cli_class_holds(const CliRegexp *regexp, const CliRegexpNode *node, unsigned int letter)
{
    const unsigned int tried[3] = {letter, cli_regexp_lower(letter), cli_regexp_upper(letter)};
    int found = 0;
    for (unsigned int each = 0u; !found && (each < (regexp->fold ? 3u : 1u)); each += 1u)
    {
        for (unsigned int at = 0u; !found && (at < node->count); at += 1u)
        {
            found = cli_item_holds(&regexp->item[node->first + at], tried[each]);
        }
    }
    return found != node->negated;
}

int cli_regexp_one(const CliRegexp *regexp, unsigned int index, unsigned int letter)
{
    const CliRegexpNode *const node = &regexp->node[index];
    if (node->ascii_known && (letter < 128u))
    {
        return (int)((node->ascii[letter / 64u] >> (letter % 64u)) & 1ull);
    }
    switch (node->kind)
    {
    case CLI_REGEXP_CHAR:
        return cli_regexp_same(regexp, node->letter, letter);
    case CLI_REGEXP_ANY:
        return !cli_regexp_ends_line(letter);
    case CLI_REGEXP_CLASS:
        return cli_class_holds(regexp, node, letter);
    default:
        return -1;
    }
}

int cli_regexp_allows(const CliRegexpFirst *first, unsigned int letter)
{
    return (letter < 128u) ? (int)((first->ascii[letter / 64u] >> (letter % 64u)) & 1ull) : first->other;
}

// the parts of a sequence from `index` taken in turn, the rest handed to `then`
int cli_regexp_chain(const CliRegexp *regexp, unsigned int node, unsigned int index, unsigned long long at,
                     CliRegexpRun *run, const CliRegexpThen *then)
{
    const CliRegexpNode *const concat = &regexp->node[node];
    if (index == concat->count)
    {
        return cli_regexp_then(regexp, then, at, run);
    }
    const CliRegexpThen frame = {CLI_THEN_CHAIN, node, index + 1u, 0u, 0ull, then};
    return cli_regexp_walk(regexp, regexp->part[concat->first + index], at, run, &frame);
}

// one more iteration of a repetition, and the rest of it after, one pass deeper
static int cli_again(const CliRegexp *regexp, unsigned int node, unsigned int count, unsigned long long at,
                     CliRegexpRun *run, const CliRegexpThen *then)
{
    const CliRegexpNode *const repeat = &regexp->node[node];
    if ((repeat->max != CLI_REGEXP_UNBOUNDED) && (count >= repeat->max))
    {
        return 0;
    }
    const char mark = 0;
    const uintptr_t here = (uintptr_t)&mark;
    const uintptr_t depth = (here > run->base) ? (here - run->base) : (run->base - here);
    if (depth > CLI_REGEXP_STACK_MOST)
    {
        // given up as STEPS gives up: no step after it goes on
        run->steps = CLI_REGEXP_STEPS + 1ull;
        return 0;
    }
    const CliRegexpThen frame = {CLI_THEN_REPEAT, node, 0u, count, at, then};
    return cli_regexp_walk(regexp, repeat->child, at, run, &frame);
}

// a repetition at its `count`th pass: greedy tries one more pass before handing on, lazy hands on first
int cli_regexp_repeat(const CliRegexp *regexp, unsigned int node, unsigned int count, unsigned long long at,
                      CliRegexpRun *run, const CliRegexpThen *then)
{
    const CliRegexpNode *const repeat = &regexp->node[node];
    if (count < repeat->min)
    {
        return cli_again(regexp, node, count, at, run, then);
    }
    if (repeat->greedy)
    {
        return cli_again(regexp, node, count, at, run, then) || cli_regexp_then(regexp, then, at, run);
    }
    return cli_regexp_then(regexp, then, at, run) || cli_again(regexp, node, count, at, run, then);
}

int cli_regexp_then(const CliRegexp *regexp, const CliRegexpThen *then, unsigned long long at, CliRegexpRun *run)
{
    switch (then->kind)
    {
    case CLI_THEN_END:
        run->end = at;
        return 1;
    case CLI_THEN_CHAIN:
        return cli_regexp_chain(regexp, then->node, then->index, at, run, then->next);
    case CLI_THEN_GROUP: {
        const CliRegexpSpan kept = run->groups[then->index];
        run->groups[then->index] = (CliRegexpSpan){then->from, at, 1};
        if (cli_regexp_then(regexp, then->next, at, run))
        {
            return 1;
        }
        run->groups[then->index] = kept;
        return 0;
    }
    case CLI_THEN_TRUE:
        return 1;
    default: {
        // an iteration that takes nothing ends the repetition, once the fewest are taken
        if ((at == then->from) && (then->count >= regexp->node[then->node].min))
        {
            return 0;
        }
        return cli_regexp_repeat(regexp, then->node, then->count + 1u, at, run, then->next);
    }
    }
}

// a repeat over one character: counts how far the run goes, then hands on from the longest to the shortest, or the
// other way where lazy
int cli_regexp_counted(const CliRegexp *regexp, const CliRegexpNode *repeat, unsigned long long at, CliRegexpRun *run,
                       const CliRegexpThen *then)
{
    unsigned long long reach = 0ull;
    while (((repeat->max == CLI_REGEXP_UNBOUNDED) || (reach < repeat->max)) && ((at + reach) < run->length) &&
           (cli_regexp_one(regexp, repeat->child, run->text[at + reach]) == 1))
    {
        reach += 1ull;
    }
    if (reach < repeat->min)
    {
        return 0;
    }
    for (unsigned long long step = 0ull; step <= (reach - repeat->min); step += 1ull)
    {
        const unsigned long long count = repeat->greedy ? (reach - step) : (repeat->min + step);
        if (cli_regexp_then(regexp, then, at + count, run))
        {
            return 1;
        }
    }
    return 0;
}

// a repeat whose pass takes the text one way or none: counts the passes, each where the one before ended, and hands
// on from the most to the fewest, or the other way where lazy
int cli_regexp_passed(const CliRegexp *regexp, const CliRegexpNode *repeat, unsigned long long at, CliRegexpRun *run,
                      const CliRegexpThen *then)
{
    const unsigned long long begun = run->ends_top;
    if (!match_end_room(run))
    {
        run->steps = CLI_REGEXP_STEPS + 1ull;
        return 0;
    }
    // a place in the text far below what an unsigned int counts
    run->ends[begun] = (unsigned int)at;
    run->ends_top = begun + 1ull;
    while (((run->ends_top - begun - 1ull) < repeat->max) || (repeat->max == CLI_REGEXP_UNBOUNDED))
    {
        run->steps += 1ull;
        if ((run->steps > CLI_REGEXP_STEPS) || !match_end_room(run))
        {
            run->ends_top = begun;
            run->steps = CLI_REGEXP_STEPS + 1ull;
            return 0;
        }
        const unsigned long long from = run->ends[run->ends_top - 1u];
        const unsigned long long to = cli_regexp_pass(regexp, repeat->child, run, from);
        if ((to == CLI_REGEXP_NO_PASS) || (to <= from))
        {
            break;
        }
        run->ends[run->ends_top] = (unsigned int)to;
        run->ends_top += 1ull;
    }
    const unsigned long long passes = run->ends_top - begun - 1ull;
    int found = 0;
    for (unsigned long long step = 0ull; !found && (passes >= repeat->min) && (step <= (passes - repeat->min));
         step += 1ull)
    {
        const unsigned long long count = repeat->greedy ? (passes - step) : (repeat->min + step);
        found = cli_regexp_then(regexp, then, run->ends[begun + count], run);
    }
    run->ends_top = begun;
    return found;
}
