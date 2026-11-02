// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds_adjacent.c: the names nearest one that is not known, by the fewest edits of one letter that turn one into the
// other, near enough to be what was meant, the three nearest retained, each once. A name of more than
// CLI_BUILD_ADJACENT_MAX_LEN letters is near none
#include "../text/text.h"
#include "builds.h"

// the most letters of a name measured against another
#define CLI_BUILD_ADJACENT_MAX_LEN 1024u

unsigned long long cli_build_distance(const CliBuildsACfg *args)
{
    unsigned int letters[CLI_BUILD_ADJACENT_MAX_LEN];
    unsigned long long row[CLI_BUILD_ADJACENT_MAX_LEN + 1u];
    unsigned int count = 0u;
    for (unsigned long long at = 0ull; at < args->b_size; count += 1u)
    {
        if (count == CLI_BUILD_ADJACENT_MAX_LEN)
        {
            return ~0ull;
        }
        unsigned long long length = 0ull;
        letters[count] = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->b, .size = args->b_size, .at = at,
                                    .length = &length);
        at += length;
    }
    for (unsigned int at = 0u; at <= count; at += 1u)
    {
        row[at] = at;
    }
    unsigned long long index = 0ull;
    for (unsigned long long at = 0ull; at < args->a_size; index += 1u)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->a, .size = args->a_size,
                                               .at = at, .length = &length);
        at += length;
        unsigned long long last = row[0];
        row[0] = index + 1u;
        for (unsigned int other = 0u; other < count; other += 1u)
        {
            unsigned long long next = row[other + 1u] + 1u;
            next = (row[other] + 1u < next) ? row[other] + 1u : next;
            const unsigned long long changed = last + ((letter != letters[other]) ? 1u : 0u);
            next = (changed < next) ? changed : next;
            last = row[other + 1u];
            row[other + 1u] = next;
        }
    }
    return row[count];
}

void cli_build_adjacent_begin(const CliBuildsSizeCfg *args)
{
    const char *name = args->name;
    args->adjacent->name = name;
    args->adjacent->name_size = args->size;
    args->adjacent->count = 0u;
}

typedef struct
{
    const CliBuildAdjacent *adjacent;
    unsigned long long far;
    const char *one;
    unsigned long long size;
    unsigned int at;
} BuildsNearCtx;
// whether the pair `far`, `one` sorts before the near name at `at`, as a distance and then a text
EMBED_INLINE int help_before(const BuildsNearCtx *args)
{
    if (args->far != args->adjacent->far[args->at])
    {
        return args->far < args->adjacent->far[args->at];
    }
    const unsigned long long min =
        (args->size < args->adjacent->adjacent_size[args->at]) ? args->size : args->adjacent->adjacent_size[args->at];
    const int order =
        memor.cmp(&(MemoriaCfg){.src = args->one, .other = args->adjacent->adjacent[args->at], .bytes = (size_t)min});
    return (order != 0) ? (order < 0) : (args->size < args->adjacent->adjacent_size[args->at]);
}

void cli_build_adjacent_offer(const CliBuildsSizeAdjacentCfg *args)
{
    if ((args->size == args->adjacent->name_size) &&
        (memor.cmp(&(MemoriaCfg){.src = args->one, .other = args->adjacent->name, .bytes = (size_t)args->size}) == 0))
    {
        return;
    }
    const unsigned long long letters = EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg,
                                                  .text = args->adjacent->name, .size = args->adjacent->name_size);
    const unsigned long long most = ((letters / 4u) > 2u) ? (letters / 4u) : 2u;
    const unsigned long long far =
        EMBED_CALL(cli_builds_a.build_distance, CliBuildsACfg, .a = args->adjacent->name,
                   .a_size = args->adjacent->name_size, .b = args->one, .b_size = args->size);
    if (far > most)
    {
        return;
    }
    unsigned int place = args->adjacent->count;
    for (unsigned int at = 0u; at < args->adjacent->count; at += 1u)
    {
        if ((far == args->adjacent->far[at]) && (args->size == args->adjacent->adjacent_size[at]) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = args->one, .other = args->adjacent->adjacent[at], .bytes = (size_t)args->size}) == 0))
        {
            return;
        }
        if ((place == args->adjacent->count) && EMBED_CALL(help_before, BuildsNearCtx, .adjacent = args->adjacent,
                                                           .far = far, .one = args->one, .size = args->size, .at = at))
        {
            place = at;
        }
    }
    if (place == 3u)
    {
        return;
    }
    const unsigned int count = (args->adjacent->count < 3u) ? args->adjacent->count + 1u : 3u;
    for (unsigned int at = count - 1u; at > place; at -= 1u)
    {
        args->adjacent->adjacent[at] = args->adjacent->adjacent[at - 1u];
        args->adjacent->adjacent_size[at] = args->adjacent->adjacent_size[at - 1u];
        args->adjacent->far[at] = args->adjacent->far[at - 1u];
    }
    args->adjacent->adjacent[place] = args->one;
    args->adjacent->adjacent_size[place] = args->size;
    args->adjacent->far[place] = far;
    args->adjacent->count = count;
}
