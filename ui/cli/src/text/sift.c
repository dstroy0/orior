// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sift.c: a window's census, the anchors placed on a needle, and the needle found a window at a time
#include "sift.h"

// the most candidate offsets a level weighs, the most alignments a level scores its candidates on, every so many of
// the window's taken, and the most bytes of a window the census counts, every so many of them taken
#define CLI_SIFT_CANDIDATES 16u

#define CLI_SIFT_SCORED 4096u

#define CLI_SIFT_COUNTED 65536u

// the alignments a plan scores on, held under the lock while a plan is made
static unsigned long long s_standing[(2u * CLI_SIFT_SCORED) + 2u];
static CliLock s_lock = CLI_LOCK_INIT;

unsigned char cli_sift_folded(unsigned char byte, int fold)
{
    // a capital and its small letter differ in the one bit 0x20
    return (fold && (byte >= 'A') && (byte <= 'Z')) ? (unsigned char)(byte | 0x20u) : byte;
}

void cli_sift_census(const unsigned char *window, unsigned long long length, int fold, CliCensus *census)
{
    const unsigned long long stride = ((length / CLI_SIFT_COUNTED) > 1ull) ? (length / CLI_SIFT_COUNTED) : 1ull;
    memor.set(&(MemoriaCfg){.dst = census, .val = 0, .bytes = sizeof(*census)});
    for (unsigned long long at = 0ull; at < length; at += stride)
    {
        census->occurrences[cli_sift_folded(window[at], fold)] += 1ull;
        census->total += 1ull;
    }
}

void cli_sift_census_counted(CliCensus *census, const unsigned char *bytes, unsigned long long length)
{
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        census->occurrences[bytes[at]] += 1ull;
    }
    census->total += length;
}

void cli_sift_census_uncounted(CliCensus *census, const unsigned char *bytes, unsigned long long length)
{
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        census->occurrences[bytes[at]] -= 1ull;
    }
    census->total -= length;
}

void cli_sift_census_folded(const CliCensus *exact, CliCensus *folded)
{
    *folded = *exact;
    for (unsigned int byte = 'A'; byte <= 'Z'; byte += 1u)
    {
        // a capital's count joins its small letter's, which stands at the byte with 0x20 set
        folded->occurrences[byte | 0x20u] += folded->occurrences[byte];
        folded->occurrences[byte] = 0ull;
    }
}

// How rare a byte is in the census, larger the rarer: the bytes counted that are other bytes
static unsigned long long cli_magnitude(const CliCensus *census, unsigned char byte)
{
    return census->total - census->occurrences[byte];
}

// whether the window agrees with the needle at `offset` from the alignment `at`
static int cli_agrees(const unsigned char *window, const unsigned char *needle, unsigned long long offset,
                      unsigned long long at, int fold)
{
    return cli_sift_folded(window[at + offset], fold) == cli_sift_folded(needle[offset], fold);
}

// the anchors planned for `window`, ranked by `given`, a census kept with the text, where one is given, else by a count
// over the window
static unsigned int cli_plan_held(const CliCensus *given, const unsigned char *window, unsigned long long length,
                                  const unsigned char *needle, unsigned long long needle_length, int fold,
                                  unsigned long long *anchors)
{
    if ((needle_length == 0ull) || (needle_length > length))
    {
        return 0u;
    }
    CliCensus census;
    if (given != NULL)
    {
        census = *given;
    }
    else
    {
        cli_sift_census(window, length, fold, &census);
    }
    // The candidates: the needle's offsets, the rarest byte first, ties in the needle's order; a byte the needle
    // repeats tests at each of its offsets what it tests at another place, which the descent may want
    unsigned long long candidates[CLI_SIFT_CANDIDATES];
    unsigned int candidate_count = 0u;
    while (candidate_count < CLI_SIFT_CANDIDATES)
    {
        unsigned long long best = needle_length;
        for (unsigned long long offset = 0ull; offset < needle_length; offset += 1ull)
        {
            int chosen = 0;
            for (unsigned int at = 0u; !chosen && (at < candidate_count); at += 1u)
            {
                chosen = candidates[at] == offset;
            }
            if (!chosen && ((best == needle_length) || (cli_magnitude(&census, cli_sift_folded(needle[offset], fold)) >
                                                        cli_magnitude(&census, cli_sift_folded(needle[best], fold)))))
            {
                best = offset;
            }
        }
        if (best == needle_length)
        {
            break;
        }
        candidates[candidate_count] = best;
        candidate_count += 1u;
    }
    const unsigned long long alignments = length - needle_length + 1ull;
    const unsigned long long stride = ((alignments / CLI_SIFT_SCORED) > 1ull) ? (alignments / CLI_SIFT_SCORED) : 1ull;
    unsigned long long standing = 0ull;
    for (unsigned long long at = 0ull; (at < alignments) && (standing < (sizeof(s_standing) / sizeof(s_standing[0])));
         at += stride)
    {
        s_standing[standing] = at;
        standing += 1ull;
    }
    unsigned int placed = 0u;
    while (placed < CLI_SIFT_ANCHORS)
    {
        unsigned long long best_left = 0ull;
        unsigned long long best_offset = needle_length;
        for (unsigned int each = 0u; each < candidate_count; each += 1u)
        {
            const unsigned long long offset = candidates[each];
            int taken = 0;
            for (unsigned int at = 0u; !taken && (at < placed); at += 1u)
            {
                taken = anchors[at] == offset;
            }
            if (taken)
            {
                continue;
            }
            unsigned long long left = 0ull;
            for (unsigned long long at = 0ull; at < standing; at += 1ull)
            {
                left += cli_agrees(window, needle, offset, s_standing[at], fold) ? 1ull : 0ull;
            }
            // the least refuted left, the lesser offset among equals
            if ((best_offset == needle_length) || (left < best_left) || ((left == best_left) && (offset < best_offset)))
            {
                best_left = left;
                best_offset = offset;
            }
        }
        // A level that refutes nothing the levels above it did not ends the descent, and the first level is always
        // placed
        if ((best_offset == needle_length) || ((placed != 0u) && (best_left >= standing)))
        {
            break;
        }
        unsigned long long kept = 0ull;
        for (unsigned long long at = 0ull; at < standing; at += 1ull)
        {
            if (cli_agrees(window, needle, best_offset, s_standing[at], fold))
            {
                s_standing[kept] = s_standing[at];
                kept += 1ull;
            }
        }
        standing = kept;
        anchors[placed] = best_offset;
        placed += 1u;
    }
    return placed;
}

unsigned int cli_sift_plan(const unsigned char *window, unsigned long long length, const unsigned char *needle,
                           unsigned long long needle_length, int fold, unsigned long long *anchors)
{
    return cli_sift_plan_with(NULL, window, length, needle, needle_length, fold, anchors);
}

unsigned int cli_sift_plan_with(const CliCensus *census, const unsigned char *window, unsigned long long length,
                                const unsigned char *needle, unsigned long long needle_length, int fold,
                                unsigned long long *anchors)
{
    cli_lock_take(&s_lock);
    const unsigned int placed = cli_plan_held(census, window, length, needle, needle_length, fold, anchors);
    cli_lock_give(&s_lock);
    return placed;
}

// whether the whole needle stands at `at`
static int cli_stands_at(const unsigned char *window, const unsigned char *needle, unsigned long long needle_length,
                         unsigned long long at, int fold)
{
    for (unsigned long long step = 0ull; step < needle_length; step += 1ull)
    {
        if (cli_sift_folded(window[at + step], fold) != cli_sift_folded(needle[step], fold))
        {
            return 0;
        }
    }
    return 1;
}

int cli_sift_holds(const unsigned char *window, const unsigned char *needle, unsigned long long needle_length,
                   const unsigned long long *anchors, unsigned int anchor_count, unsigned int from,
                   unsigned long long at, int fold)
{
    for (unsigned int each = from; each < anchor_count; each += 1u)
    {
        if (!cli_agrees(window, needle, anchors[each], at, fold))
        {
            return 0;
        }
    }
    return cli_stands_at(window, needle, needle_length, at, fold);
}

unsigned long long cli_sift_plain_window(const unsigned char *window, unsigned long long length,
                                         const unsigned char *needle, unsigned long long needle_length,
                                         const unsigned long long *anchors, unsigned int anchor_count, int fold,
                                         unsigned long long start, unsigned long long base,
                                         void (*found)(void *context, unsigned long long at), void *context)
{
    const unsigned long long alignments = length - needle_length + 1ull;
    unsigned long long count = 0ull;
    for (unsigned long long at = start; at < alignments; at += 1ull)
    {
        if (cli_sift_holds(window, needle, needle_length, anchors, anchor_count, 0u, at, fold))
        {
            found(context, base + at);
            count += 1ull;
        }
    }
    return count;
}

// The offsets the needle stands at in one window, under the anchors placed for it
static unsigned long long cli_window_on(unsigned int path, const unsigned char *window, unsigned long long length,
                                        const unsigned char *needle, unsigned long long needle_length,
                                        const unsigned long long *anchors, unsigned int anchor_count, int fold,
                                        unsigned long long base, void (*found)(void *context, unsigned long long at),
                                        void *context)
{
#if ENGINE_TARGET_X86_64
    if ((anchor_count != 0u) && (path == CLI_VECTOR_AVX2) && cli_vector_avx2_present())
    {
        return cli_sift_window_avx2(window, length, needle, needle_length, anchors, anchor_count, fold, base, found,
                                    context);
    }
    if ((anchor_count != 0u) && ((path == CLI_VECTOR_SSE2) || (path == CLI_VECTOR_AVX2)))
    {
        return cli_sift_window_sse2(window, length, needle, needle_length, anchors, anchor_count, fold, base, found,
                                    context);
    }
#else
    (void)path;
#endif
    return cli_sift_plain_window(window, length, needle, needle_length, anchors, anchor_count, fold, 0ull, base, found,
                                 context);
}

unsigned long long cli_sift_horspool(const unsigned char *text, unsigned long long length, const unsigned char *needle,
                                     unsigned long long needle_length, int fold,
                                     void (*found)(void *context, unsigned long long at), void *context)
{
    if ((needle_length == 0ull) || (needle_length > length))
    {
        return 0ull;
    }
    // how far the needle moves on when the byte under its last stands nowhere in it but its last: its length; and for
    // each byte it holds before its last, the distance from that byte's last place to the end
    unsigned long long shift[256];
    for (unsigned int byte = 0u; byte < 256u; byte += 1u)
    {
        shift[byte] = needle_length;
    }
    for (unsigned long long at = 0ull; (at + 1ull) < needle_length; at += 1ull)
    {
        shift[cli_sift_folded(needle[at], fold)] = needle_length - 1ull - at;
    }
    const unsigned char last = cli_sift_folded(needle[needle_length - 1ull], fold);
    unsigned long long count = 0ull;
    unsigned long long at = 0ull;
    while ((at + needle_length) <= length)
    {
        const unsigned char under = cli_sift_folded(text[at + needle_length - 1ull], fold);
        if ((under == last) && cli_stands_at(text, needle, needle_length, at, fold))
        {
            found(context, at);
            count += 1ull;
        }
        at += shift[under];
    }
    return count;
}

// every offset `needle` stands at in `text` on `path`, each window planned by `census` where one is given
static unsigned long long cli_find_all_census(unsigned int path, const CliCensus *census, const unsigned char *text,
                                              unsigned long long length, const unsigned char *needle,
                                              unsigned long long needle_length, int fold,
                                              void (*found)(void *context, unsigned long long at), void *context)
{
    if ((needle_length == 0ull) || (needle_length > length))
    {
        return 0ull;
    }
    // a text within one window is too small for a plan to pay for itself: the needle's own shifts find it faster
    if (length < CLI_SIFT_WINDOW)
    {
        return cli_sift_horspool(text, length, needle, needle_length, fold, found, context);
    }
    const unsigned long long reach =
        (CLI_SIFT_WINDOW > (needle_length * 2ull)) ? CLI_SIFT_WINDOW : (needle_length * 2ull);
    const unsigned long long step = reach - (needle_length - 1ull);
    unsigned long long count = 0ull;
    for (unsigned long long start = 0ull; (start + needle_length) <= length; start += step)
    {
        const unsigned long long end =
            ((start + step + needle_length - 1ull) < length) ? (start + step + needle_length - 1ull) : length;
        unsigned long long anchors[CLI_SIFT_ANCHORS];
        const unsigned int placed =
            cli_sift_plan_with(census, text + start, end - start, needle, needle_length, fold, anchors);
        count += cli_window_on(path, text + start, end - start, needle, needle_length, anchors, placed, fold, start,
                               found, context);
    }
    return count;
}

unsigned long long cli_sift_find_all_on(unsigned int path, const unsigned char *text, unsigned long long length,
                                        const unsigned char *needle, unsigned long long needle_length, int fold,
                                        void (*found)(void *context, unsigned long long at), void *context)
{
    return cli_find_all_census(path, NULL, text, length, needle, needle_length, fold, found, context);
}

unsigned long long cli_sift_find_all(const unsigned char *text, unsigned long long length, const unsigned char *needle,
                                     unsigned long long needle_length, int fold,
                                     void (*found)(void *context, unsigned long long at), void *context)
{
    return cli_find_all_census(cli_vector_best(), NULL, text, length, needle, needle_length, fold, found, context);
}

unsigned long long cli_sift_find_all_with(const CliCensus *census, const unsigned char *text, unsigned long long length,
                                          const unsigned char *needle, unsigned long long needle_length, int fold,
                                          void (*found)(void *context, unsigned long long at), void *context)
{
    return cli_find_all_census(cli_vector_best(), census, text, length, needle, needle_length, fold, found, context);
}
