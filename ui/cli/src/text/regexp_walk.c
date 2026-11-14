// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// regexp_walk.c: each node walked from a place in the text; the characters a node's match can start with; whether a
// pass of a node takes the text one way or none, and where such a pass ends; and a match tried at one place
#include "regexp_match_internal.h"

#include "../command_line/pool.h"

typedef struct
{
    const CliRegexp *regexp;
    unsigned int node;
    unsigned long long *ascii;
    CliRegexpFirst a;
    CliRegexpFirst b;
    unsigned int index;
    unsigned long long *own;
    int *end;
    const CliRegexpNode *node_regexp_node_pointer;
    unsigned long long at;
    CliRegexpRun *run;
    const CliRegexpThen *then;
} RegexpWalkCtx;

// the ASCII characters a node's one letter takes, as two words of bits
EMBED_INLINE void cli_ascii_get(const RegexpWalkCtx *args)
{
    const CliRegexp *regexp = args->regexp;
    args->ascii[0] = 0ULL;
    args->ascii[1] = 0ULL;
    for (unsigned int letter = 0u; letter < 128u; letter += 1u)
    {
        args->ascii[letter / 64u] |= (EMBED_CALL(cli_regexp_match_internal.regexp_one, CliRegexpMatchCtx,
                                                 .regexp = regexp, .index = args->node, .letter = letter) == 1)
                                         ? (1ULL << (letter % 64u))
                                         : 0ULL;
    }
}

EMBED_INLINE CliRegexpFirst cli_union(const RegexpWalkCtx *args)
{
    const CliRegexpFirst both = {{args->a.ascii[0] | args->b.ascii[0], args->a.ascii[1] | args->b.ascii[1]},
                                 args->a.other || args->b.other,
                                 args->a.empty || args->b.empty};
    return both;
}

CliRegexpFirst cli_regexp_first_get(const CliRegexpIndexCfg *args)
{
    const CliRegexpNode *const node = &args->regexp->node[args->index];
    CliRegexpFirst set = {{0ULL, 0ULL}, 0, 0};
    switch (node->type)
    {
    case CLI_REGEXP_CHAR:
    case CLI_REGEXP_ANY:
    case CLI_REGEXP_CLASS:
        EMBED_CALL(cli_ascii_get, RegexpWalkCtx, .regexp = args->regexp, .node = args->index, .ascii = set.ascii);
        set.other = 1;
        return set;
    case CLI_REGEXP_GROUP:
        return EMBED_CALL(cli_regexp_index.first_get, CliRegexpIndexCfg, .regexp = args->regexp, .index = node->child);
    case CLI_REGEXP_ALT:
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            set = EMBED_CALL(cli_union, RegexpWalkCtx, .a = set,
                             .b = EMBED_CALL(cli_regexp_index.first_get, CliRegexpIndexCfg, .regexp = args->regexp,
                                             .index = args->regexp->part[node->first + at]));
        }
        return set;
    case CLI_REGEXP_CONCAT:
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            const CliRegexpFirst own = EMBED_CALL(cli_regexp_index.first_get, CliRegexpIndexCfg, .regexp = args->regexp,
                                                  .index = args->regexp->part[node->first + at]);
            CliRegexpFirst reserved = own;
            reserved.empty = 0;
            set = EMBED_CALL(cli_union, RegexpWalkCtx, .a = set, .b = reserved);
            if (!own.empty)
            {
                return set;
            }
        }
        set.empty = 1;
        return set;
    case CLI_REGEXP_REPEAT: {
        CliRegexpFirst own =
            EMBED_CALL(cli_regexp_index.first_get, CliRegexpIndexCfg, .regexp = args->regexp, .index = node->child);
        own.empty = own.empty || (node->min == 0u);
        return own;
    }
    case CLI_REGEXP_BACK:
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

// The characters a node can take first, those past ASCII as one in `end`: a character of the text no two
// alternatives can both take first leaves a pass one way to go. 1, or 0 where the node takes none it can name
static int cli_starts(const RegexpWalkCtx *args)
{
    int *end = args->end;
    const CliRegexpNode *const node = &args->regexp->node[args->index];
    switch (node->type)
    {
    case CLI_REGEXP_CHAR:
        EMBED_CALL(cli_ascii_get, RegexpWalkCtx, .regexp = args->regexp, .node = args->index, .ascii = args->own);
        *end = (node->letter >= 128u) ||
               (args->regexp->fold && (EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg,
                                                  .property = CLI_REGEXP_LETTER, .letter = node->letter) != 0));
        return 1;
    case CLI_REGEXP_ANY:
        EMBED_CALL(cli_ascii_get, RegexpWalkCtx, .regexp = args->regexp, .node = args->index, .ascii = args->own);
        *end = 1;
        return 1;
    case CLI_REGEXP_CLASS:
        EMBED_CALL(cli_ascii_get, RegexpWalkCtx, .regexp = args->regexp, .node = args->index, .ascii = args->own);
        *end = node->negated;
        for (unsigned int at = 0u; !*end && (at < node->count); at += 1u)
        {
            const CliRegexpItem *const item = &args->regexp->item[node->first + at];
            if (item->type == CLI_REGEXP_RANGE)
            {
                *end = (item->high >= 128u) || args->regexp->fold;
            }
            else if (item->type == CLI_REGEXP_DIGIT)
            {
                *end = item->negated;
            }
            else
            {
                *end = 1;
            }
        }
        return 1;
    case CLI_REGEXP_GROUP:
        return (node->group == CLI_REGEXP_NONE) && EMBED_CALL(cli_starts, RegexpWalkCtx, .regexp = args->regexp,
                                                              .index = node->child, .own = args->own, .end = end);
    case CLI_REGEXP_CONCAT:
        return (node->count != 0u) &&
               EMBED_CALL(cli_starts, RegexpWalkCtx, .regexp = args->regexp, .index = args->regexp->part[node->first],
                          .own = args->own, .end = end);
    case CLI_REGEXP_ALT: {
        args->own[0] = 0ULL;
        args->own[1] = 0ULL;
        *end = 0;
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            unsigned long long option[2];
            int option_end = 0;
            if (!EMBED_CALL(cli_starts, RegexpWalkCtx, .regexp = args->regexp,
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

int cli_regexp_one_way(const CliRegexpIndexCfg *args)
{
    const CliRegexpNode *const node = &args->regexp->node[args->index];
    switch (node->type)
    {
    case CLI_REGEXP_CHAR:
    case CLI_REGEXP_ANY:
    case CLI_REGEXP_CLASS:
        return 1;
    case CLI_REGEXP_GROUP:
        return (node->group == CLI_REGEXP_NONE) &&
               EMBED_CALL(cli_regexp_index.one_way, CliRegexpIndexCfg, .regexp = args->regexp, .index = node->child);
    case CLI_REGEXP_CONCAT: {
        int all = node->count != 0u;
        for (unsigned int at = 0u; all && (at < node->count); at += 1u)
        {
            all = EMBED_CALL(cli_regexp_index.one_way, CliRegexpIndexCfg, .regexp = args->regexp,
                             .index = args->regexp->part[node->first + at]);
        }
        return all;
    }
    case CLI_REGEXP_ALT: {
        unsigned long long seen[2] = {0ULL, 0ULL};
        int seen_end = 0;
        for (unsigned int at = 0u; at < node->count; at += 1u)
        {
            const unsigned int option = args->regexp->part[node->first + at];
            unsigned long long own[2];
            int end = 0;
            if (!EMBED_CALL(cli_starts, RegexpWalkCtx, .regexp = args->regexp, .index = option, .own = own,
                            .end = &end) ||
                !EMBED_CALL(cli_regexp_index.one_way, CliRegexpIndexCfg, .regexp = args->regexp, .index = option))
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

unsigned long long cli_regexp_pass(const CliRegexpMatchIndexAtCtx *args)
{
    const CliRegexpNode *const node = &args->regexp->node[args->index];
    switch (node->type)
    {
    case CLI_REGEXP_CHAR:
    case CLI_REGEXP_ANY:
    case CLI_REGEXP_CLASS:
        return ((args->at < args->run_read->length) &&
                (EMBED_CALL(cli_regexp_match_internal.regexp_one, CliRegexpMatchCtx, .regexp = args->regexp,
                            .index = args->index, .letter = args->run_read->text[args->at]) == 1))
                   ? (args->at + 1ULL)
                   : CLI_REGEXP_NO_PASS;
    case CLI_REGEXP_GROUP:
        return EMBED_CALL(cli_regexp_match_index_at_internal.regexp_pass, CliRegexpMatchIndexAtCtx,
                          .regexp = args->regexp, .index = node->child, .run_read = args->run_read, .at = args->at);
    case CLI_REGEXP_CONCAT: {
        unsigned long long to = args->at;
        for (unsigned int part = 0u; (to != CLI_REGEXP_NO_PASS) && (part < node->count); part += 1u)
        {
            to = EMBED_CALL(cli_regexp_match_index_at_internal.regexp_pass, CliRegexpMatchIndexAtCtx,
                            .regexp = args->regexp, .index = args->regexp->part[node->first + part],
                            .run_read = args->run_read, .at = to);
        }
        return to;
    }
    case CLI_REGEXP_ALT:
        // no two alternatives take the same character first: the first that passes takes the pass
        for (unsigned int option = 0u; option < node->count; option += 1u)
        {
            const unsigned long long to = EMBED_CALL(
                cli_regexp_match_index_at_internal.regexp_pass, CliRegexpMatchIndexAtCtx, .regexp = args->regexp,
                .index = args->regexp->part[node->first + option], .run_read = args->run_read, .at = args->at);
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

// a lookahead at `at`: the rest handed on where the inner matches, or where it does not for `(?!`, every group it set
// let go where it does not stand
EMBED_INLINE int cli_on_look(const RegexpWalkCtx *args)
{
    const CliRegexp *regexp = args->regexp;
    unsigned long long at = args->at;
    CliRegexpRun *run = args->run;
    const CliRegexpThen *then = args->then;
    CliRegexpSpan saved_groups[CLI_REGEXP_GROUPS];
    memor.cpy(&(MemoriaCfg){
        .dst = saved_groups, .src = run->groups, .bytes = (regexp->groups + 1u) * sizeof(saved_groups[0])});
    const CliRegexpThen frame = {CLI_THEN_TRUE, 0u, 0u, 0u, 0ULL, NULL};
    const int found = EMBED_CALL(cli_regexp_match_index_internal.regexp_walk, CliRegexpMatchIndexCtx, .regexp = regexp,
                                 .index = args->node_regexp_node_pointer->child, .at = at, .run = run, .then = &frame);
    if (args->node_regexp_node_pointer->negated)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = run->groups, .src = saved_groups, .bytes = (regexp->groups + 1u) * sizeof(saved_groups[0])});
        return !found && EMBED_CALL(cli_regexp_match_index_internal.regexp_then, CliRegexpMatchIndexCtx,
                                    .regexp = regexp, .then = then, .at = at, .run = run);
    }
    if (found && EMBED_CALL(cli_regexp_match_index_internal.regexp_then, CliRegexpMatchIndexCtx, .regexp = regexp,
                            .then = then, .at = at, .run = run))
    {
        return 1;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = run->groups, .src = saved_groups, .bytes = (regexp->groups + 1u) * sizeof(saved_groups[0])});
    return 0;
}

int cli_regexp_walk(const CliRegexpMatchIndexCtx *args)
{
    args->run->steps += 1ULL;
    if (args->run->steps > CLI_REGEXP_STEPS)
    {
        return 0;
    }
    const CliRegexpNode *const node = &args->regexp->node[args->index];
    switch (node->type)
    {
    case CLI_REGEXP_EMPTY:
        return cli_regexp_match_index_internal.regexp_then(args);
    case CLI_REGEXP_CHAR:
    case CLI_REGEXP_ANY:
    case CLI_REGEXP_CLASS:
        return (args->at < args->run->length) &&
               (EMBED_CALL(cli_regexp_match_internal.regexp_one, CliRegexpMatchCtx, .regexp = args->regexp,
                           .index = args->index, .letter = args->run->text[args->at]) == 1) &&
               EMBED_CALL(cli_regexp_match_index_internal.regexp_then, CliRegexpMatchIndexCtx, .regexp = args->regexp,
                          .then = args->then, .at = args->at + 1ULL, .run = args->run);
    case CLI_REGEXP_START:
        return (args->at == 0ULL) && cli_regexp_match_index_internal.regexp_then(args);
    case CLI_REGEXP_END:
        return (args->at == args->run->length) && cli_regexp_match_index_internal.regexp_then(args);
    case CLI_REGEXP_BOUNDARY: {
        const int before = (args->at > 0ULL) && EMBED_CALL(cli_regexp_match_internal.regexp_word, CliRegexpMatchCtx,
                                                           .letter = args->run->text[args->at - 1u]);
        const int after =
            (args->at < args->run->length) &&
            EMBED_CALL(cli_regexp_match_internal.regexp_word, CliRegexpMatchCtx, .letter = args->run->text[args->at]);
        return ((before != after) != node->negated) && cli_regexp_match_index_internal.regexp_then(args);
    }
    case CLI_REGEXP_GROUP: {
        if (node->group == CLI_REGEXP_NONE)
        {
            return EMBED_CALL(cli_regexp_match_index_internal.regexp_walk, CliRegexpMatchIndexCtx,
                              .regexp = args->regexp, .index = node->child, .at = args->at, .run = args->run,
                              .then = args->then);
        }
        const CliRegexpThen frame = {CLI_THEN_GROUP, 0u, node->group, 0u, args->at, args->then};
        return EMBED_CALL(cli_regexp_match_index_internal.regexp_walk, CliRegexpMatchIndexCtx, .regexp = args->regexp,
                          .index = node->child, .at = args->at, .run = args->run, .then = &frame);
    }
    case CLI_REGEXP_LOOK:
        return EMBED_CALL(cli_on_look, RegexpWalkCtx, .regexp = args->regexp, .node_regexp_node_pointer = node,
                          .at = args->at, .run = args->run, .then = args->then);
    case CLI_REGEXP_CONCAT:
        return EMBED_CALL(cli_regexp_match_index_internal.regexp_chain, CliRegexpMatchIndexCtx, .regexp = args->regexp,
                          .node = args->index, .index = 0u, .at = args->at, .run = args->run, .then = args->then);
    case CLI_REGEXP_ALT:
        for (unsigned int option = 0u; option < node->count; option += 1u)
        {
            const CliRegexpFirst *const first = &args->regexp->part_first[node->first + option];
            const int may = first->empty || ((args->at < args->run->length) &&
                                             EMBED_CALL(cli_regexp.allows, CliRegexpCfg, .first = first,
                                                        .letter = args->run->text[args->at]));
            if (may && EMBED_CALL(cli_regexp_match_index_internal.regexp_walk, CliRegexpMatchIndexCtx,
                                  .regexp = args->regexp, .index = args->regexp->part[node->first + option],
                                  .at = args->at, .run = args->run, .then = args->then))
            {
                return 1;
            }
        }
        return 0;
    case CLI_REGEXP_REPEAT: {
        const unsigned int child = args->regexp->node[node->child].type;
        if ((child == CLI_REGEXP_CHAR) || (child == CLI_REGEXP_ANY) || (child == CLI_REGEXP_CLASS))
        {
            return EMBED_CALL(cli_regexp_match_then_internal.regexp_count, CliRegexpMatchThenCtx,
                              .regexp = args->regexp, .repeat = node, .at = args->at, .run = args->run,
                              .then = args->then);
        }
        if (node->one_way)
        {
            return EMBED_CALL(cli_regexp_match_then_internal.regexp_pass, CliRegexpMatchThenCtx, .regexp = args->regexp,
                              .repeat = node, .at = args->at, .run = args->run, .then = args->then);
        }
        return EMBED_CALL(cli_regexp_match_index_internal.regexp_repeat, CliRegexpMatchIndexCtx, .regexp = args->regexp,
                          .node = args->index, .count = 0u, .at = args->at, .run = args->run, .then = args->then);
    }
    default: {
        // a backreference: the group's text again, or nothing where the group took no part
        if ((node->group > args->regexp->groups) || !args->run->groups[node->group].set)
        {
            return cli_regexp_match_index_internal.regexp_then(args);
        }
        const unsigned long long from = args->run->groups[node->group].from;
        const unsigned long long length = args->run->groups[node->group].to - from;
        if ((args->at + length) > args->run->length)
        {
            return 0;
        }
        for (unsigned long long step = 0ULL; step < length; step += 1ULL)
        {
            if (!EMBED_CALL(cli_regexp_match_a_internal.regexp_same, CliRegexpMatchACtx, .regexp = args->regexp,
                            .a = args->run->text[from + step], .b = args->run->text[args->at + step]))
            {
                return 0;
            }
        }
        return EMBED_CALL(cli_regexp_match_index_internal.regexp_then, CliRegexpMatchIndexCtx, .regexp = args->regexp,
                          .then = args->then, .at = args->at + length, .run = args->run);
    }
    }
}

int cli_regexp_match_into(const CliRegexpTextU32sCfg *args)
{
    if (args->regexp->anchored && (args->at != 0ULL))
    {
        return 0;
    }
    if (!args->regexp->first.empty &&
        !((args->at < args->length) && EMBED_CALL(cli_regexp.allows, CliRegexpCfg, .first = &args->regexp->first,
                                                  .letter = args->text_u32s[args->at])))
    {
        return 0;
    }
    memor.set(
        &(MemoriaCfg){.dst = args->groups, .val = 0, .bytes = (args->regexp->groups + 1u) * sizeof(args->groups[0])});
    const char mark = 0;
    // the repeats' ends stored on this stack until they outgrow it
    unsigned int ends[CLI_REGEXP_ENDS_STACKED];
    CliRegexpRun run = {args->text_u32s,
                        args->length,
                        args->groups,
                        0ULL,
                        0ULL,
                        (embed_word)&mark,
                        ends,
                        0ULL,
                        CLI_REGEXP_ENDS_STACKED,
                        0};
    const CliRegexpThen end = {CLI_THEN_END, 0u, 0u, 0u, 0ULL, NULL};
    const int found =
        EMBED_CALL(cli_regexp_match_index_internal.regexp_walk, CliRegexpMatchIndexCtx, .regexp = args->regexp,
                   .index = args->regexp->root, .at = args->at, .run = &run, .then = &end);
    if (run.ends_reserved)
    {
        g_cli_guard->persistent_buf_release(run.ends, g_cli_text_owner);
    }
    if (found)
    {
        args->groups[0] = (CliRegexpSpan){args->at, run.end, 1};
    }
    return found;
}
