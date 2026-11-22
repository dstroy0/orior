// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_walk.c: each node walked from a place in the text; the characters a node's match can start with; whether a
// pass of a node takes the text one way or none, and where such a pass ends; and a match tried at one place
#include "regexp_match_internal.h"

typedef struct
{
    const Regexp *regexp;
    unsigned int node;
    unsigned long long *ascii;
    RegexpFirst a;
    RegexpFirst b;
    unsigned int index;
    unsigned long long *own;
    int *end;
    const RegexpNode *node_regexp_node_pointer;
    unsigned long long at;
    RegexpRun *run;
    const RegexpThen *then;
} RegexpWalkCtx;

// the ASCII characters a node's one character takes, as two words of bits
EMBED_INLINE void regexp_ascii_get(const RegexpWalkCtx *args)
{
    const Regexp *regexp = args->regexp;
    args->ascii[0] = 0ULL;
    args->ascii[1] = 0ULL;
    for (unsigned int character = 0u; character < 128u; character += 1u)
    {
        args->ascii[character / 64u] |= (EMBED_CALL(regexp_match_internal.regexp_one, RegexpMatchCtx, .regexp = regexp,
                                                    .index = args->node, .character = character) == 1)
                                            ? (1ULL << (character % 64u))
                                            : 0ULL;
    }
}

EMBED_INLINE RegexpFirst regexp_union(const RegexpWalkCtx *args)
{
    const RegexpFirst both = {{args->a.ascii[0] | args->b.ascii[0], args->a.ascii[1] | args->b.ascii[1]},
                              args->a.other || args->b.other,
                              args->a.empty || args->b.empty};
    return both;
}

RegexpFirst regexp_first_get(const RegexpIndexCfg *args)
{
    const RegexpNode *const node = &args->regexp->node[args->index];
    RegexpFirst set = {{0ULL, 0ULL}, 0, 0};
    switch (node->type)
    {
    case REGEXP_CHAR:
    case REGEXP_ANY:
    case REGEXP_CLASS:
        EMBED_CALL(regexp_ascii_get, RegexpWalkCtx, .regexp = args->regexp, .node = args->index, .ascii = set.ascii);
        set.other = 1;
        return set;
    case REGEXP_GROUP:
        return EMBED_CALL(regexp_index.first_get, RegexpIndexCfg, .regexp = args->regexp, .index = node->child);
    case REGEXP_ALT:
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            set = EMBED_CALL(regexp_union, RegexpWalkCtx, .a = set,
                             .b = EMBED_CALL(regexp_index.first_get, RegexpIndexCfg, .regexp = args->regexp,
                                             .index = args->regexp->part[node->first + at]));
        }
        return set;
    case REGEXP_CONCAT:
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            const RegexpFirst own = EMBED_CALL(regexp_index.first_get, RegexpIndexCfg, .regexp = args->regexp,
                                               .index = args->regexp->part[node->first + at]);
            RegexpFirst reserved = own;
            reserved.empty = 0;
            set = EMBED_CALL(regexp_union, RegexpWalkCtx, .a = set, .b = reserved);
            if (!own.empty)
            {
                return set;
            }
        }
        set.empty = 1;
        return set;
    case REGEXP_REPEAT: {
        RegexpFirst own =
            EMBED_CALL(regexp_index.first_get, RegexpIndexCfg, .regexp = args->regexp, .index = node->child);
        own.empty = own.empty || (node->min == 0u);
        return own;
    }
    case REGEXP_BACK:
        set.ascii[0] = ~0ULL;
        set.ascii[1] = ~0ULL;
        set.other = 1;
        set.empty = 1;
        return set;
    default:
        set.empty = 1;
        return set;
    }
}

// The characters a node can take first, those past ASCII as one in `end`. Where no two alternatives can both take a
// character of the text first, a pass has one way to go. 1, or 0 where the node takes none it can name
static int regexp_starts(const RegexpWalkCtx *args)
{
    int *end = args->end;
    const RegexpNode *const node = &args->regexp->node[args->index];
    switch (node->type)
    {
    case REGEXP_CHAR:
        EMBED_CALL(regexp_ascii_get, RegexpWalkCtx, .regexp = args->regexp, .node = args->index, .ascii = args->own);
        *end = (node->character >= 128u) ||
               (args->regexp->fold && (EMBED_CALL(regexp_character.property, RegexpLetterCfg,
                                                  .property = REGEXP_CHARACTER, .character = node->character) != 0));
        return 1;
    case REGEXP_ANY:
        EMBED_CALL(regexp_ascii_get, RegexpWalkCtx, .regexp = args->regexp, .node = args->index, .ascii = args->own);
        *end = 1;
        return 1;
    case REGEXP_CLASS:
        EMBED_CALL(regexp_ascii_get, RegexpWalkCtx, .regexp = args->regexp, .node = args->index, .ascii = args->own);
        *end = node->negated;
        for (unsigned int at = 0u; !*end && (at < node->count); at += 1u)
        {
            const RegexpItem *const item = &args->regexp->item[node->first + at];
            if (item->type == REGEXP_RANGE)
            {
                *end = (item->high >= 128u) || args->regexp->fold;
            }
            else if (item->type == REGEXP_DIGIT)
            {
                *end = item->negated;
            }
            else
            {
                *end = 1;
            }
        }
        return 1;
    case REGEXP_GROUP:
        return (node->group == REGEXP_NONE) && EMBED_CALL(regexp_starts, RegexpWalkCtx, .regexp = args->regexp,
                                                          .index = node->child, .own = args->own, .end = end);
    case REGEXP_CONCAT:
        return (node->count != 0u) &&
               EMBED_CALL(regexp_starts, RegexpWalkCtx, .regexp = args->regexp,
                          .index = args->regexp->part[node->first], .own = args->own, .end = end);
    case REGEXP_ALT: {
        args->own[0] = 0ULL;
        args->own[1] = 0ULL;
        *end = 0;
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            unsigned long long option[2];
            int option_end = 0;
            if (!EMBED_CALL(regexp_starts, RegexpWalkCtx, .regexp = args->regexp,
                            .index = args->regexp->part[node->first + at], .own = option, .end = &option_end))
            {
                return 0;
            }
            args->own[0] |= option[0];
            args->own[1] |= option[1];
            *end = *end || option_end;
        }
        return 1;
    }
    default:
        return 0;
    }
}

int regexp_one_way(const RegexpIndexCfg *args)
{
    const RegexpNode *const node = &args->regexp->node[args->index];
    switch (node->type)
    {
    case REGEXP_CHAR:
    case REGEXP_ANY:
    case REGEXP_CLASS:
        return 1;
    case REGEXP_GROUP:
        return (node->group == REGEXP_NONE) &&
               EMBED_CALL(regexp_index.one_way, RegexpIndexCfg, .regexp = args->regexp, .index = node->child);
    case REGEXP_CONCAT: {
        int all = node->count != 0u;
        for (unsigned int at = 0u; all && (at < node->count); at += 1u)
        {
            all = EMBED_CALL(regexp_index.one_way, RegexpIndexCfg, .regexp = args->regexp,
                             .index = args->regexp->part[node->first + at]);
        }
        return all;
    }
    case REGEXP_ALT: {
        unsigned long long seen[2] = {0ULL, 0ULL};
        int seen_end = 0;
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            const unsigned int option = args->regexp->part[node->first + at];
            unsigned long long own[2];
            int end = 0;
            if (!EMBED_CALL(regexp_starts, RegexpWalkCtx, .regexp = args->regexp, .index = option, .own = own,
                            .end = &end) ||
                !EMBED_CALL(regexp_index.one_way, RegexpIndexCfg, .regexp = args->regexp, .index = option))
            {
                return 0;
            }
            if (((seen[0] & own[0]) != 0ULL) || ((seen[1] & own[1]) != 0ULL) || (seen_end && end))
            {
                return 0;
            }
            seen[0] |= own[0];
            seen[1] |= own[1];
            seen_end = seen_end || end;
        }
        return 1;
    }
    default:
        return 0;
    }
}

unsigned long long regexp_pass(const RegexpMatchIndexAtCtx *args)
{
    const RegexpNode *const node = &args->regexp->node[args->index];
    switch (node->type)
    {
    case REGEXP_CHAR:
    case REGEXP_ANY:
    case REGEXP_CLASS:
        return ((args->at < args->run_read->length) &&
                (EMBED_CALL(regexp_match_internal.regexp_one, RegexpMatchCtx, .regexp = args->regexp,
                            .index = args->index, .character = args->run_read->text[args->at]) == 1))
                   ? (args->at + 1ULL)
                   : REGEXP_NO_PASS;
    case REGEXP_GROUP:
        return EMBED_CALL(regexp_match_index_at_internal.regexp_pass, RegexpMatchIndexAtCtx, .regexp = args->regexp,
                          .index = node->child, .run_read = args->run_read, .at = args->at);
    case REGEXP_CONCAT: {
        unsigned long long to = args->at;
        for (unsigned int part = 0u; (to != REGEXP_NO_PASS) && (part < node->count); part += 1u)
        {
            to = EMBED_CALL(regexp_match_index_at_internal.regexp_pass, RegexpMatchIndexAtCtx, .regexp = args->regexp,
                            .index = args->regexp->part[node->first + part], .run_read = args->run_read, .at = to);
        }
        return to;
    }
    case REGEXP_ALT:
        // no two alternatives take the same character first: the first that passes takes the pass
        for (unsigned int option = 0u; option < node->count; option += 1u)
        {
            const unsigned long long to = EMBED_CALL(
                regexp_match_index_at_internal.regexp_pass, RegexpMatchIndexAtCtx, .regexp = args->regexp,
                .index = args->regexp->part[node->first + option], .run_read = args->run_read, .at = args->at);
            if (to != REGEXP_NO_PASS)
            {
                return to;
            }
        }
        return REGEXP_NO_PASS;
    default:
        return REGEXP_NO_PASS;
    }
}

// A lookahead at `at`. The rest passes on where the inner matches, or where it does not for `(?!`. Where the rest
// does not pass on, each group the lookahead set lets go
EMBED_INLINE int regexp_on_look(const RegexpWalkCtx *args)
{
    const Regexp *regexp = args->regexp;
    unsigned long long at = args->at;
    RegexpRun *run = args->run;
    const RegexpThen *then = args->then;
    RegexpSpan saved_groups[REGEXP_GROUPS];
    memor.cpy(&(MemoriaCfg){
        .dst = saved_groups, .src = run->groups, .bytes = (regexp->groups + 1u) * sizeof(saved_groups[0])});
    const RegexpThen frame = {REGEXP_THEN_TRUE, 0u, 0u, 0u, 0ULL, NULL};
    const int found = EMBED_CALL(regexp_match_index_internal.regexp_walk, RegexpMatchIndexCtx, .regexp = regexp,
                                 .index = args->node_regexp_node_pointer->child, .at = at, .run = run, .then = &frame);
    if (args->node_regexp_node_pointer->negated)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = run->groups, .src = saved_groups, .bytes = (regexp->groups + 1u) * sizeof(saved_groups[0])});
        return !found && EMBED_CALL(regexp_match_index_internal.regexp_then, RegexpMatchIndexCtx, .regexp = regexp,
                                    .then = then, .at = at, .run = run);
    }
    if (found && EMBED_CALL(regexp_match_index_internal.regexp_then, RegexpMatchIndexCtx, .regexp = regexp,
                            .then = then, .at = at, .run = run))
    {
        return 1;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = run->groups, .src = saved_groups, .bytes = (regexp->groups + 1u) * sizeof(saved_groups[0])});
    return 0;
}

int regexp_walk(const RegexpMatchIndexCtx *args)
{
    args->run->steps += 1ULL;
    if (args->run->steps > REGEXP_STEPS)
    {
        return 0;
    }
    const RegexpNode *const node = &args->regexp->node[args->index];
    switch (node->type)
    {
    case REGEXP_EMPTY:
        return regexp_match_index_internal.regexp_then(args);
    case REGEXP_CHAR:
    case REGEXP_ANY:
    case REGEXP_CLASS:
        return (args->at < args->run->length) &&
               (EMBED_CALL(regexp_match_internal.regexp_one, RegexpMatchCtx, .regexp = args->regexp,
                           .index = args->index, .character = args->run->text[args->at]) == 1) &&
               EMBED_CALL(regexp_match_index_internal.regexp_then, RegexpMatchIndexCtx, .regexp = args->regexp,
                          .then = args->then, .at = args->at + 1ULL, .run = args->run);
    case REGEXP_START:
        return (args->at == 0ULL) && regexp_match_index_internal.regexp_then(args);
    case REGEXP_END:
        return (args->at == args->run->length) && regexp_match_index_internal.regexp_then(args);
    case REGEXP_BOUNDARY: {
        const int before = (args->at > 0ULL) && EMBED_CALL(regexp_match_internal.regexp_word, RegexpMatchCtx,
                                                           .character = args->run->text[args->at - 1u]);
        const int after =
            (args->at < args->run->length) &&
            EMBED_CALL(regexp_match_internal.regexp_word, RegexpMatchCtx, .character = args->run->text[args->at]);
        return ((before != after) != node->negated) && regexp_match_index_internal.regexp_then(args);
    }
    case REGEXP_GROUP: {
        if (node->group == REGEXP_NONE)
        {
            return EMBED_CALL(regexp_match_index_internal.regexp_walk, RegexpMatchIndexCtx, .regexp = args->regexp,
                              .index = node->child, .at = args->at, .run = args->run, .then = args->then);
        }
        const RegexpThen frame = {REGEXP_THEN_GROUP, 0u, node->group, 0u, args->at, args->then};
        return EMBED_CALL(regexp_match_index_internal.regexp_walk, RegexpMatchIndexCtx, .regexp = args->regexp,
                          .index = node->child, .at = args->at, .run = args->run, .then = &frame);
    }
    case REGEXP_LOOK:
        return EMBED_CALL(regexp_on_look, RegexpWalkCtx, .regexp = args->regexp, .node_regexp_node_pointer = node,
                          .at = args->at, .run = args->run, .then = args->then);
    case REGEXP_CONCAT:
        return EMBED_CALL(regexp_match_index_internal.regexp_chain, RegexpMatchIndexCtx, .regexp = args->regexp,
                          .node = args->index, .index = 0u, .at = args->at, .run = args->run, .then = args->then);
    case REGEXP_ALT:
        for (unsigned int option = 0u; option < node->count; option += 1u)
        {
            const RegexpFirst *const first = &args->regexp->part_first[node->first + option];
            const int may = first->empty || ((args->at < args->run->length) &&
                                             EMBED_CALL(regexp_first_set.allows, RegexpCfg, .first = first,
                                                        .character = args->run->text[args->at]));
            if (may && EMBED_CALL(regexp_match_index_internal.regexp_walk, RegexpMatchIndexCtx, .regexp = args->regexp,
                                  .index = args->regexp->part[node->first + option], .at = args->at, .run = args->run,
                                  .then = args->then))
            {
                return 1;
            }
        }
        return 0;
    case REGEXP_REPEAT: {
        const unsigned int child = args->regexp->node[node->child].type;
        if ((child == REGEXP_CHAR) || (child == REGEXP_ANY) || (child == REGEXP_CLASS))
        {
            return EMBED_CALL(regexp_match_then_internal.regexp_count, RegexpMatchThenCtx, .regexp = args->regexp,
                              .repeat = node, .at = args->at, .run = args->run, .then = args->then);
        }
        if (node->one_way)
        {
            return EMBED_CALL(regexp_match_then_internal.regexp_pass, RegexpMatchThenCtx, .regexp = args->regexp,
                              .repeat = node, .at = args->at, .run = args->run, .then = args->then);
        }
        return EMBED_CALL(regexp_match_index_internal.regexp_repeat, RegexpMatchIndexCtx, .regexp = args->regexp,
                          .node = args->index, .count = 0u, .at = args->at, .run = args->run, .then = args->then);
    }
    default: {
        // a backreference: the group's text again, or nothing where the group took no part
        if ((node->group > args->regexp->groups) || !args->run->groups[node->group].set)
        {
            return regexp_match_index_internal.regexp_then(args);
        }
        const unsigned long long from = args->run->groups[node->group].from;
        const unsigned long long length = args->run->groups[node->group].to - from;
        if ((args->at + length) > args->run->length)
        {
            return 0;
        }
        for (unsigned long long step = 0ULL; step < length; step += 1ULL)
        {
            if (!EMBED_CALL(regexp_match_a_internal.regexp_same, RegexpMatchACtx, .regexp = args->regexp,
                            .a = args->run->text[from + step], .b = args->run->text[args->at + step]))
            {
                return 0;
            }
        }
        return EMBED_CALL(regexp_match_index_internal.regexp_then, RegexpMatchIndexCtx, .regexp = args->regexp,
                          .then = args->then, .at = args->at + length, .run = args->run);
    }
    }
}

int regexp_match_into(const RegexpTextU32sCfg *args)
{
    if (args->regexp->anchored && (args->at != 0ULL))
    {
        return 0;
    }
    if (!args->regexp->first.empty &&
        !((args->at < args->length) && EMBED_CALL(regexp_first_set.allows, RegexpCfg, .first = &args->regexp->first,
                                                  .character = args->text_u32s[args->at])))
    {
        return 0;
    }
    memor.set(
        &(MemoriaCfg){.dst = args->groups, .val = 0, .bytes = (args->regexp->groups + 1u) * sizeof(args->groups[0])});
    const char mark = 0;
    // the repeats' ends stored on this stack until they outgrow it
    unsigned int ends[REGEXP_ENDS_STACKED];
    RegexpRun run = {
        args->text_u32s, args->length, args->groups, 0ULL, 0ULL, (embed_word)&mark, ends, 0ULL, REGEXP_ENDS_STACKED, 0};
    const RegexpThen end = {REGEXP_THEN_END, 0u, 0u, 0u, 0ULL, NULL};
    const int found = EMBED_CALL(regexp_match_index_internal.regexp_walk, RegexpMatchIndexCtx, .regexp = args->regexp,
                                 .index = args->regexp->root, .at = args->at, .run = &run, .then = &end);
    if (run.ends_reserved)
    {
        regexp_cell_released(run.ends);
    }
    if (found)
    {
        args->groups[0] = (RegexpSpan){args->at, run.end, 1};
    }
    return found;
}
