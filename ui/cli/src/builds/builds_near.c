// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds_near.c: the names nearest one that is not known, by the fewest edits of one letter that turn one into the
// other, near enough to be what was meant, the three nearest kept, each once. A name of more than
// CLI_BUILD_NEAR_LONGEST letters is near none
#include "../text/text.h"
#include "builds.h"

// the most letters of a name measured against another
#define CLI_BUILD_NEAR_LONGEST 1024u

unsigned long long cli_build_distance(const char *a, unsigned long long a_size, const char *b,
                                      unsigned long long b_size)
{
    unsigned int letters[CLI_BUILD_NEAR_LONGEST];
    unsigned long long row[CLI_BUILD_NEAR_LONGEST + 1u];
    unsigned int count = 0u;
    for (unsigned long long at = 0ull; at < b_size; count += 1u)
    {
        if (count == CLI_BUILD_NEAR_LONGEST)
        {
            return ~0ull;
        }
        unsigned long long length = 0ull;
        letters[count] = cli_text_letter(b, b_size, at, &length);
        at += length;
    }
    for (unsigned int at = 0u; at <= count; at += 1u)
    {
        row[at] = at;
    }
    unsigned long long index = 0ull;
    for (unsigned long long at = 0ull; at < a_size; index += 1u)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = cli_text_letter(a, a_size, at, &length);
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

void cli_build_near_begun(CliBuildNear *near, const char *name, unsigned long long size)
{
    near->name = name;
    near->name_size = size;
    near->count = 0u;
}

// whether the pair `far`, `one` sorts before the near name at `at`, as a distance and then a text
static int help_before(const CliBuildNear *near, unsigned long long far, const char *one, unsigned long long size,
                       unsigned int at)
{
    if (far != near->far[at])
    {
        return far < near->far[at];
    }
    const unsigned long long least = (size < near->near_size[at]) ? size : near->near_size[at];
    const int order = memor.cmp(&(MemoriaCfg){.src = one, .other = near->near[at], .bytes = (size_t)least});
    return (order != 0) ? (order < 0) : (size < near->near_size[at]);
}

void cli_build_near_offered(CliBuildNear *near, const char *one, unsigned long long size)
{
    if ((size == near->name_size) &&
        (memor.cmp(&(MemoriaCfg){.src = one, .other = near->name, .bytes = (size_t)size}) == 0))
    {
        return;
    }
    const unsigned long long letters = cli_text_letters(near->name, near->name_size);
    const unsigned long long most = ((letters / 4u) > 2u) ? (letters / 4u) : 2u;
    const unsigned long long far = cli_build_distance(near->name, near->name_size, one, size);
    if (far > most)
    {
        return;
    }
    unsigned int place = near->count;
    for (unsigned int at = 0u; at < near->count; at += 1u)
    {
        if ((far == near->far[at]) && (size == near->near_size[at]) &&
            (memor.cmp(&(MemoriaCfg){.src = one, .other = near->near[at], .bytes = (size_t)size}) == 0))
        {
            return;
        }
        if ((place == near->count) && help_before(near, far, one, size, at))
        {
            place = at;
        }
    }
    if (place == 3u)
    {
        return;
    }
    const unsigned int kept = (near->count < 3u) ? near->count + 1u : 3u;
    for (unsigned int at = kept - 1u; at > place; at -= 1u)
    {
        near->near[at] = near->near[at - 1u];
        near->near_size[at] = near->near_size[at - 1u];
        near->far[at] = near->far[at - 1u];
    }
    near->near[place] = one;
    near->near_size[place] = size;
    near->far[place] = far;
    near->count = kept;
}
