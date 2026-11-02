// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds_near.c: the names nearest one that is not known, by the fewest edits of one letter that turn one into the
// other, near enough to be what was meant, the three nearest retained, each once. A name of more than
// CLI_BUILD_NEAR_MAX_LEN letters is near none
#include "../text/text.h"
#include "builds.h"

// the most letters of a name measured against another
#define CLI_BUILD_NEAR_MAX_LEN 1024u

unsigned long long cli_build_distance(const CliBuildsACfg *args)
{
    unsigned int letters[CLI_BUILD_NEAR_MAX_LEN];
    unsigned long long row[CLI_BUILD_NEAR_MAX_LEN + 1u];
    unsigned int count = 0u;
    for (unsigned long long at = 0ull; at < args->b_size; count += 1u)
    {
        if (count == CLI_BUILD_NEAR_MAX_LEN)
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

void cli_build_near_begin(const CliBuildsSizeCfg *args)
{
    const char *name = args->name;
    args->near->name = name;
    args->near->name_size = args->size;
    args->near->count = 0u;
}

typedef struct
{
    const CliBuildNear *near;
    unsigned long long far;
    const char *one;
    unsigned long long size;
    unsigned int at;
} BuildsNearCtx;
// whether the pair `far`, `one` sorts before the near name at `at`, as a distance and then a text
EMBED_INLINE int help_before(const BuildsNearCtx *args)
{
    if (args->far != args->near->far[args->at])
    {
        return args->far < args->near->far[args->at];
    }
    const unsigned long long least =
        (args->size < args->near->near_size[args->at]) ? args->size : args->near->near_size[args->at];
    const int order =
        memor.cmp(&(MemoriaCfg){.src = args->one, .other = args->near->near[args->at], .bytes = (size_t)least});
    return (order != 0) ? (order < 0) : (args->size < args->near->near_size[args->at]);
}

void cli_build_near_offer(const CliBuildsSizeNearCfg *args)
{
    if ((args->size == args->near->name_size) &&
        (memor.cmp(&(MemoriaCfg){.src = args->one, .other = args->near->name, .bytes = (size_t)args->size}) == 0))
    {
        return;
    }
    const unsigned long long letters = EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg,
                                                  .text = args->near->name, .size = args->near->name_size);
    const unsigned long long most = ((letters / 4u) > 2u) ? (letters / 4u) : 2u;
    const unsigned long long far = EMBED_CALL(cli_builds_a.build_distance, CliBuildsACfg, .a = args->near->name,
                                              .a_size = args->near->name_size, .b = args->one, .b_size = args->size);
    if (far > most)
    {
        return;
    }
    unsigned int place = args->near->count;
    for (unsigned int at = 0u; at < args->near->count; at += 1u)
    {
        if ((far == args->near->far[at]) && (args->size == args->near->near_size[at]) &&
            (memor.cmp(&(MemoriaCfg){.src = args->one, .other = args->near->near[at], .bytes = (size_t)args->size}) ==
             0))
        {
            return;
        }
        if ((place == args->near->count) && EMBED_CALL(help_before, BuildsNearCtx, .near = args->near, .far = far,
                                                       .one = args->one, .size = args->size, .at = at))
        {
            place = at;
        }
    }
    if (place == 3u)
    {
        return;
    }
    const unsigned int count = (args->near->count < 3u) ? args->near->count + 1u : 3u;
    for (unsigned int at = count - 1u; at > place; at -= 1u)
    {
        args->near->near[at] = args->near->near[at - 1u];
        args->near->near_size[at] = args->near->near_size[at - 1u];
        args->near->far[at] = args->near->far[at - 1u];
    }
    args->near->near[place] = args->one;
    args->near->near_size[place] = args->size;
    args->near->far[place] = far;
    args->near->count = count;
}
