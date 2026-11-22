// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_match.c: a pattern matched at one place by backtracking. Each node takes the text where it can and passes the
// place it stops at to what comes after it, a chain of frames on the stack: the rest of a sequence, a group's end, a
// lookahead's true, a repetition's next pass, or the match's end. A repeat over what takes the text one way at a place
// or not at all steps along without going deeper, its passes' ends stored in the match's own arena, on its stack until
// they outgrow it and then in a cell twice as large each time they fill theirs, given back as the match ends
#include "regexp_match_internal.h"

typedef struct
{
    RegexpRun *run;
    const RegexpItem *item;
    unsigned int character;
    const Regexp *regexp;
    const RegexpNode *node;
    unsigned int node_u32;
    unsigned int count;
    unsigned long long at;
    const RegexpThen *then;
} RegexpMatchCtxState;

// Room for one more end in the run's ends. Where they fill the match's own stack, they move once into a cell at their
// max. 1, or 0 where they hold the max or the pool cannot meet the cell
EMBED_INLINE int match_end_capacity(const RegexpMatchCtxState *args)
{
    if (args->run->ends_top < args->run->ends_capacity)
    {
        return 1;
    }
    // the caller's storage full, the stack goes once into a cell of the max it is at
    unsigned int *const cell =
        args->run->ends_reserved ? NULL : regexp_cell_taken(REGEXP_ENDS_MAX * sizeof(unsigned int));
    if (cell == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = cell, .src = args->run->ends, .bytes = (embed_word)(args->run->ends_top * sizeof(unsigned int))});
    args->run->ends = cell;
    args->run->ends_capacity = REGEXP_ENDS_MAX;
    args->run->ends_reserved = 1;
    return 1;
}

int regexp_word(const RegexpMatchCtx *args)
{
    return ((args->character >= 'a') && (args->character <= 'z')) ||
           ((args->character >= 'A') && (args->character <= 'Z')) ||
           ((args->character >= '0') && (args->character <= '9')) || (args->character == '_');
}

int regexp_space(const RegexpMatchCtx *args)
{
    static const unsigned int s_spaces[] = {'\t',    '\n',    0x0bu,   0x0cu,   '\r',    ' ',     0xa0u,
                                            0x1680u, 0x2028u, 0x2029u, 0x202fu, 0x205fu, 0x3000u, 0xfeffu};
    for (unsigned int at = 0u; at < (sizeof(s_spaces) / sizeof(s_spaces[0])); at += 1u)
    {
        if (args->character == s_spaces[at])
        {
            return 1;
        }
    }
    return (args->character >= 0x2000u) && (args->character <= 0x200au);
}

int regexp_ends_line(const RegexpMatchCtx *args)
{
    return (args->character == '\n') || (args->character == '\r') || (args->character == 0x2028u) ||
           (args->character == 0x2029u);
}

int regexp_same(const RegexpMatchACtx *args)
{
    return (args->a == args->b) ||
           (args->regexp->fold && (EMBED_CALL(regexp_character.lower, RegexpLetterCfg, .character = args->a) ==
                                   EMBED_CALL(regexp_character.lower, RegexpLetterCfg, .character = args->b)));
}

// whether one item of a class matches `character`
EMBED_INLINE int regexp_item_matches(const RegexpMatchCtxState *args)
{
    unsigned int character = args->character;
    switch (args->item->type)
    {
    case REGEXP_RANGE:
        return (character >= args->item->low) && (character <= args->item->high);
    case REGEXP_DIGIT:
        return ((character >= '0') && (character <= '9')) != args->item->negated;
    case REGEXP_WORD:
        return EMBED_CALL(regexp_match_internal.regexp_word, RegexpMatchCtx, .character = character) !=
               args->item->negated;
    case REGEXP_SPACE:
        return EMBED_CALL(regexp_match_internal.regexp_space, RegexpMatchCtx, .character = character) !=
               args->item->negated;
    default:
        return EMBED_CALL(regexp_character.property, RegexpLetterCfg, .property = args->item->low,
                          .character = character) != args->item->negated;
    }
}

// whether the class node `node` matches `character`: any item matches it, or under `i` of its lower or upper case
EMBED_INLINE int regexp_class_matches(const RegexpMatchCtxState *args)
{
    unsigned int character = args->character;
    const unsigned int tried[3] = {character,
                                   EMBED_CALL(regexp_character.lower, RegexpLetterCfg, .character = character),
                                   EMBED_CALL(regexp_character.upper, RegexpLetterCfg, .character = character)};
    int found = 0;
    for (unsigned int each = 0u; !found && (each < (args->regexp->fold ? 3u : 1u)); each += 1u)
    {
        for (unsigned int at = 0u; !found && (at < args->node->count); at += 1u)
        {
            found = EMBED_CALL(regexp_item_matches, RegexpMatchCtxState,
                               .item = &args->regexp->item[args->node->first + at], .character = tried[each]);
        }
    }
    return found != args->node->negated;
}

int regexp_one(const RegexpMatchCtx *args)
{
    const RegexpNode *const node = &args->regexp->node[args->index];
    if (node->ascii_known && (args->character < 128u))
    {
        return (int)((node->ascii[args->character / 64u] >> (args->character % 64u)) & 1ULL);
    }
    switch (node->type)
    {
    case REGEXP_CHAR:
        return EMBED_CALL(regexp_match_a_internal.regexp_same, RegexpMatchACtx, .regexp = args->regexp,
                          .a = node->character, .b = args->character);
    case REGEXP_ANY:
        return !regexp_match_internal.regexp_ends_line(args);
    case REGEXP_CLASS:
        return EMBED_CALL(regexp_class_matches, RegexpMatchCtxState, .regexp = args->regexp, .node = node,
                          .character = args->character);
    default:
        return -1;
    }
}

int regexp_allows(const RegexpCfg *args)
{
    return (args->character < 128u)
               ? (int)((args->first->ascii[args->character / 64u] >> (args->character % 64u)) & 1ULL)
               : args->first->other;
}

// the parts of a sequence from `index` taken in turn, the rest passed to `then`
int regexp_chain(const RegexpMatchIndexCtx *args)
{
    const RegexpNode *const concat = &args->regexp->node[args->node];
    if (args->index == concat->count)
    {
        return regexp_match_index_internal.regexp_then(args);
    }
    const RegexpThen frame = {REGEXP_THEN_CHAIN, args->node, args->index + 1u, 0u, 0ULL, args->then};
    return EMBED_CALL(regexp_match_index_internal.regexp_walk, RegexpMatchIndexCtx, .regexp = args->regexp,
                      .index = args->regexp->part[concat->first + args->index], .at = args->at, .run = args->run,
                      .then = &frame);
}

// one more iteration of a repetition, and the rest of it after, one pass deeper
EMBED_INLINE int regexp_again(const RegexpMatchCtxState *args)
{
    const Regexp *regexp = args->regexp;
    unsigned long long at = args->at;
    RegexpRun *run = args->run;
    const RegexpThen *then = args->then;
    const RegexpNode *const repeat = &regexp->node[args->node_u32];
    if ((repeat->max != REGEXP_UNBOUNDED) && (args->count >= repeat->max))
    {
        return 0;
    }
    const char mark = 0;
    const embed_word here = (embed_word)&mark;
    const embed_word depth = (here > run->base) ? (here - run->base) : (run->base - here);
    if (depth > REGEXP_STACK_MAX)
    {
        // given up as STEPS gives up: no step after it goes on
        run->steps = REGEXP_STEPS + 1ULL;
        return 0;
    }
    const RegexpThen frame = {REGEXP_THEN_REPEAT, args->node_u32, 0u, args->count, at, then};
    return EMBED_CALL(regexp_match_index_internal.regexp_walk, RegexpMatchIndexCtx, .regexp = regexp,
                      .index = repeat->child, .at = at, .run = run, .then = &frame);
}

// a repetition at its `count`th pass: greedy tries one more pass before it passes on, and lazy passes on first
int regexp_repeat(const RegexpMatchIndexCtx *args)
{
    const RegexpNode *const repeat = &args->regexp->node[args->node];
    if (args->count < repeat->min)
    {
        return EMBED_CALL(regexp_again, RegexpMatchCtxState, .regexp = args->regexp, .node_u32 = args->node,
                          .count = args->count, .at = args->at, .run = args->run, .then = args->then);
    }
    if (repeat->greedy)
    {
        return EMBED_CALL(regexp_again, RegexpMatchCtxState, .regexp = args->regexp, .node_u32 = args->node,
                          .count = args->count, .at = args->at, .run = args->run, .then = args->then) ||
               regexp_match_index_internal.regexp_then(args);
    }
    return regexp_match_index_internal.regexp_then(args) ||
           EMBED_CALL(regexp_again, RegexpMatchCtxState, .regexp = args->regexp, .node_u32 = args->node,
                      .count = args->count, .at = args->at, .run = args->run, .then = args->then);
}

int regexp_then(const RegexpMatchIndexCtx *args)
{
    switch (args->then->type)
    {
    case REGEXP_THEN_END:
        args->run->end = args->at;
        return 1;
    case REGEXP_THEN_CHAIN:
        return EMBED_CALL(regexp_match_index_internal.regexp_chain, RegexpMatchIndexCtx, .regexp = args->regexp,
                          .node = args->then->node, .index = args->then->index, .at = args->at, .run = args->run,
                          .then = args->then->next);
    case REGEXP_THEN_GROUP: {
        const RegexpSpan group_span = args->run->groups[args->then->index];
        args->run->groups[args->then->index] = (RegexpSpan){args->then->from, args->at, 1};
        if (EMBED_CALL(regexp_match_index_internal.regexp_then, RegexpMatchIndexCtx, .regexp = args->regexp,
                       .then = args->then->next, .at = args->at, .run = args->run))
        {
            return 1;
        }
        args->run->groups[args->then->index] = group_span;
        return 0;
    }
    case REGEXP_THEN_TRUE:
        return 1;
    default: {
        // an iteration that takes nothing ends the repetition, once it took the fewest
        if ((args->at == args->then->from) && (args->then->count >= args->regexp->node[args->then->node].min))
        {
            return 0;
        }
        return EMBED_CALL(regexp_match_index_internal.regexp_repeat, RegexpMatchIndexCtx, .regexp = args->regexp,
                          .node = args->then->node, .count = args->then->count + 1u, .at = args->at, .run = args->run,
                          .then = args->then->next);
    }
    }
}

// A repeat over one character. It counts how far the run goes, then passes on from the longest to the shortest, or
// the other way where lazy
int regexp_count(const RegexpMatchThenCtx *args)
{
    unsigned long long reach = 0ULL;
    while (((args->repeat->max == REGEXP_UNBOUNDED) || (reach < args->repeat->max)) &&
           ((args->at + reach) < args->run->length) &&
           (EMBED_CALL(regexp_match_internal.regexp_one, RegexpMatchCtx, .regexp = args->regexp,
                       .index = args->repeat->child, .character = args->run->text[args->at + reach]) == 1))
    {
        reach += 1ULL;
    }
    if (reach < args->repeat->min)
    {
        return 0;
    }
    for (unsigned long long step = 0ULL; step <= (reach - args->repeat->min); step += 1ULL)
    {
        const unsigned long long count = args->repeat->greedy ? (reach - step) : (args->repeat->min + step);
        if (EMBED_CALL(regexp_match_index_internal.regexp_then, RegexpMatchIndexCtx, .regexp = args->regexp,
                       .then = args->then, .at = args->at + count, .run = args->run))
        {
            return 1;
        }
    }
    return 0;
}

// A repeat whose pass takes the text one way or none. It counts the passes, each where the one before ended. It
// passes on from the max to the fewest, or the other way where lazy
int regexp_on_pass(const RegexpMatchThenCtx *args)
{
    const unsigned long long begun = args->run->ends_top;
    if (!EMBED_CALL(match_end_capacity, RegexpMatchCtxState, .run = args->run))
    {
        args->run->steps = REGEXP_STEPS + 1ULL;
        return 0;
    }
    // a place in the text far below what an unsigned int counts
    args->run->ends[begun] = (unsigned int)args->at;
    args->run->ends_top = begun + 1ULL;
    while (((args->run->ends_top - begun - 1ULL) < args->repeat->max) || (args->repeat->max == REGEXP_UNBOUNDED))
    {
        args->run->steps += 1ULL;
        if ((args->run->steps > REGEXP_STEPS) || !EMBED_CALL(match_end_capacity, RegexpMatchCtxState, .run = args->run))
        {
            args->run->ends_top = begun;
            args->run->steps = REGEXP_STEPS + 1ULL;
            return 0;
        }
        const unsigned long long from = args->run->ends[args->run->ends_top - 1u];
        const unsigned long long to =
            EMBED_CALL(regexp_match_index_at_internal.regexp_pass, RegexpMatchIndexAtCtx, .regexp = args->regexp,
                       .index = args->repeat->child, .run_read = args->run, .at = from);
        if ((to == REGEXP_NO_PASS) || (to <= from))
        {
            break;
        }
        args->run->ends[args->run->ends_top] = (unsigned int)to;
        args->run->ends_top += 1ULL;
    }
    const unsigned long long passes = args->run->ends_top - begun - 1ULL;
    int found = 0;
    for (unsigned long long step = 0ULL;
         !found && (passes >= args->repeat->min) && (step <= (passes - args->repeat->min)); step += 1ULL)
    {
        const unsigned long long count = args->repeat->greedy ? (passes - step) : (args->repeat->min + step);
        found = EMBED_CALL(regexp_match_index_internal.regexp_then, RegexpMatchIndexCtx, .regexp = args->regexp,
                           .then = args->then, .at = args->run->ends[begun + count], .run = args->run);
    }
    args->run->ends_top = begun;
    return found;
}
