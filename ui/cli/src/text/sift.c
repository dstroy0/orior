// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sift.c: a window's census, the anchors placed on a needle, and the needle found a window at a time
#include "sift.h"
#include "../command_line/callback_cfg.h"

// the most candidate offsets a level weighs, the most alignments a level scores its candidates on, every so many of
// the window's taken, and the most bytes of a window the census counts, every so many of them taken
#define CLI_SIFT_CANDIDATES 16u

#define CLI_SIFT_SCORED 4096u

#define CLI_SIFT_COUNTED 65536u

// the alignments a plan scores on, guarded by the lock while a plan is made
static unsigned long long s_standing[(2u * CLI_SIFT_SCORED) + 2u];
static CliLock s_lock = CLI_LOCK_INIT;

unsigned char cli_sift_on_fold(const CliSiftFoldCfg *args)
{
    // a capital and its small letter differ in the one bit 0x20
    return (args->fold && (args->byte >= 'A') && (args->byte <= 'Z')) ? (unsigned char)(args->byte | 0x20u)
                                                                      : args->byte;
}

void cli_sift_census(const CliSiftWindowLengthFoldCensusCfg *args)
{
    const unsigned long long stride =
        ((args->length / CLI_SIFT_COUNTED) > 1ull) ? (args->length / CLI_SIFT_COUNTED) : 1ull;
    memor.set(&(MemoriaCfg){.dst = args->census, .val = 0, .bytes = sizeof(*args->census)});
    for (unsigned long long at = 0ull; at < args->length; at += stride)
    {
        args->census->occurrences[EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->window[at],
                                             .fold = args->fold)] += 1ull;
        args->census->total += 1ull;
    }
}

void cli_sift_census_count(const CliSiftLengthCensusCfg *args)
{
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        args->census->occurrences[args->bytes[at]] += 1ull;
    }
    args->census->total += args->length;
}

void cli_sift_census_uncounted(const CliSiftLengthCensusCfg *args)
{
    for (unsigned long long at = 0ull; at < args->length; at += 1ull)
    {
        args->census->occurrences[args->bytes[at]] -= 1ull;
    }
    args->census->total -= args->length;
}

void cli_sift_census_fold(const CliSiftCfg *args)
{
    CliCensus *folded = args->folded;
    *folded = *args->exact;
    for (unsigned int byte = 'A'; byte <= 'Z'; byte += 1u)
    {
        // a capital's count joins its small letter's, which stands at the byte with 0x20 set
        folded->occurrences[byte | 0x20u] += folded->occurrences[byte];
        folded->occurrences[byte] = 0ull;
    }
}

typedef struct
{
    const CliCensus *census;
    unsigned char byte;
    const unsigned char *window;
    const unsigned char *needle;
    unsigned long long offset;
    unsigned long long at;
    int fold;
    const CliCensus *given;
    unsigned long long length;
    unsigned long long needle_length;
    unsigned long long *anchors;
    unsigned int path;
    const unsigned long long *anchors_read;
    unsigned int anchor_count;
    unsigned long long base;
    void (*found)(const CliEachAtCfg *args);
    void *context;
    const unsigned char *text;
} SiftCtx;
// How rare a byte is in the census, larger the rarer: the bytes counted that are other bytes
EMBED_INLINE unsigned long long cli_magnitude(const SiftCtx *args)
{
    return args->census->total - args->census->occurrences[args->byte];
}

// whether the window agrees with the needle at `offset` from the alignment `at`
EMBED_INLINE int cli_agrees(const SiftCtx *args)
{
    int fold = args->fold;
    return EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->window[args->at + args->offset],
                      .fold = fold) ==
           EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->needle[args->offset], .fold = fold);
}

// the anchors planned for `window`, ranked by `given`, a census stored with the text, where one is given, else by a
// count over the window
EMBED_INLINE unsigned int cli_plan_anchors_place(const SiftCtx *args)
{
    const unsigned char *window = args->window;
    unsigned long long length = args->length;
    int fold = args->fold;
    if ((args->needle_length == 0ull) || (args->needle_length > length))
    {
        return 0u;
    }
    CliCensus census;
    if (args->given != NULL)
    {
        census = *args->given;
    }
    else
    {
        EMBED_CALL(cli_sift_window_length_fold_census.census, CliSiftWindowLengthFoldCensusCfg, .window = window,
                   .length = length, .fold = fold, .census = &census);
    }
    // The candidates: the needle's offsets, the rarest byte first, ties in the needle's order; a byte the needle
    // repeats tests at each of its offsets what it tests at another place, which the descent may want
    unsigned long long candidates[CLI_SIFT_CANDIDATES];
    unsigned int candidate_count = 0u;
    while (candidate_count < CLI_SIFT_CANDIDATES)
    {
        unsigned long long best = args->needle_length;
        for (unsigned long long offset = 0ull; offset < args->needle_length; offset += 1ull)
        {
            int chosen = 0;
            for (unsigned int at = 0u; !chosen && (at < candidate_count); at += 1u)
            {
                chosen = candidates[at] == offset;
            }
            if (!chosen && ((best == args->needle_length) ||
                            (EMBED_CALL(cli_magnitude, SiftCtx, .census = &census,
                                        .byte = EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg,
                                                           .byte = args->needle[offset], .fold = fold)) >
                             EMBED_CALL(cli_magnitude, SiftCtx, .census = &census,
                                        .byte = EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg,
                                                           .byte = args->needle[best], .fold = fold)))))
            {
                best = offset;
            }
        }
        if (best == args->needle_length)
        {
            break;
        }
        candidates[candidate_count] = best;
        candidate_count += 1u;
    }
    const unsigned long long alignments = length - args->needle_length + 1ull;
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
        unsigned long long best_offset = args->needle_length;
        for (unsigned int each = 0u; each < candidate_count; each += 1u)
        {
            const unsigned long long offset = candidates[each];
            int taken = 0;
            for (unsigned int at = 0u; !taken && (at < placed); at += 1u)
            {
                taken = args->anchors[at] == offset;
            }
            if (taken)
            {
                continue;
            }
            unsigned long long left = 0ull;
            for (unsigned long long at = 0ull; at < standing; at += 1ull)
            {
                left += EMBED_CALL(cli_agrees, SiftCtx, .window = window, .needle = args->needle, .offset = offset,
                                   .at = s_standing[at], .fold = fold)
                            ? 1ull
                            : 0ull;
            }
            // the least refuted left, the lesser offset among equals
            if ((best_offset == args->needle_length) || (left < best_left) ||
                ((left == best_left) && (offset < best_offset)))
            {
                best_left = left;
                best_offset = offset;
            }
        }
        // A level that refutes nothing the levels above it did not ends the descent, and the first level is always
        // placed
        if ((best_offset == args->needle_length) || ((placed != 0u) && (best_left >= standing)))
        {
            break;
        }
        unsigned long long count = 0ull;
        for (unsigned long long at = 0ull; at < standing; at += 1ull)
        {
            if (EMBED_CALL(cli_agrees, SiftCtx, .window = window, .needle = args->needle, .offset = best_offset,
                           .at = s_standing[at], .fold = fold))
            {
                s_standing[count] = s_standing[at];
                count += 1ull;
            }
        }
        standing = count;
        args->anchors[placed] = best_offset;
        placed += 1u;
    }
    return placed;
}

unsigned int cli_sift_plan(const CliSiftWindowLengthFoldCfg *args)
{
    return EMBED_CALL(cli_sift_window_length_fold.plan_with, CliSiftWindowLengthFoldCfg, .census_read = NULL,
                      .window = args->window, .length = args->length, .needle = args->needle,
                      .needle_length = args->needle_length, .fold = args->fold, .anchors_out = args->anchors_out);
}

unsigned int cli_sift_plan_with(const CliSiftWindowLengthFoldCfg *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const unsigned int placed = EMBED_CALL(
        cli_plan_anchors_place, SiftCtx, .given = args->census_read, .window = args->window, .length = args->length,
        .needle = args->needle, .needle_length = args->needle_length, .fold = args->fold, .anchors = args->anchors_out);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return placed;
}

// whether the whole needle stands at `at`
EMBED_INLINE int cli_stands_at(const SiftCtx *args)
{
    int fold = args->fold;
    for (unsigned long long step = 0ull; step < args->needle_length; step += 1ull)
    {
        if (EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->window[args->at + step], .fold = fold) !=
            EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->needle[step], .fold = fold))
        {
            return 0;
        }
    }
    return 1;
}

int cli_sift_matches(const CliSiftWindowFoldCfg *args)
{
    for (unsigned int each = args->from; each < args->anchor_count; each += 1u)
    {
        if (!EMBED_CALL(cli_agrees, SiftCtx, .window = args->window, .needle = args->needle,
                        .offset = args->anchors[each], .at = args->at, .fold = args->fold))
        {
            return 0;
        }
    }
    return EMBED_CALL(cli_stands_at, SiftCtx, .window = args->window, .needle = args->needle,
                      .needle_length = args->needle_length, .at = args->at, .fold = args->fold);
}

unsigned long long cli_sift_plain_window(const CliSiftWindowCfg *args)
{
    const unsigned char *window = args->window;
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    const unsigned long long *anchors = args->anchors;
    unsigned int anchor_count = args->anchor_count;
    int fold = args->fold;
    const unsigned long long alignments = args->length - needle_length + 1ull;
    unsigned long long count = 0ull;
    for (unsigned long long at = args->start; at < alignments; at += 1ull)
    {
        if (EMBED_CALL(cli_sift_window_fold.matches, CliSiftWindowFoldCfg, .window = window, .needle = needle,
                       .needle_length = needle_length, .anchors = anchors, .anchor_count = anchor_count, .from = 0u,
                       .at = at, .fold = fold))
        {
            EMBED_CALL(args->found, CliEachAtCfg, .context = args->context, .at = args->base + at);
            count += 1ull;
        }
    }
    return count;
}

// The offsets the needle stands at in one window, under the anchors placed for it
EMBED_INLINE unsigned long long cli_window_on(const SiftCtx *args)
{
#if ENGINE_TARGET_X86_64
    if ((args->anchor_count != 0u) && (args->path == CLI_VECTOR_AVX2) && cli_vector_avx2_present())
    {
        return EMBED_CALL(cli_sift_window_length.window_avx2, CliSiftWindowLengthCfg, .window = args->window,
                          .length = args->length, .needle = args->needle, .needle_length = args->needle_length,
                          .anchors = args->anchors_read, .anchor_count = args->anchor_count, .fold = args->fold,
                          .base = args->base, .found = args->found, .context = args->context);
    }
    if ((args->anchor_count != 0u) && ((args->path == CLI_VECTOR_SSE2) || (args->path == CLI_VECTOR_AVX2)))
    {
        return EMBED_CALL(cli_sift_window_length.window_sse2, CliSiftWindowLengthCfg, .window = args->window,
                          .length = args->length, .needle = args->needle, .needle_length = args->needle_length,
                          .anchors = args->anchors_read, .anchor_count = args->anchor_count, .fold = args->fold,
                          .base = args->base, .found = args->found, .context = args->context);
    }
#else
#endif
    return EMBED_CALL(cli_sift_window.plain_window, CliSiftWindowCfg, .window = args->window, .length = args->length,
                      .needle = args->needle, .needle_length = args->needle_length, .anchors = args->anchors_read,
                      .anchor_count = args->anchor_count, .fold = args->fold, .start = 0ull, .base = args->base,
                      .found = args->found, .context = args->context);
}

unsigned long long cli_sift_horspool(const CliSiftLengthCfg *args)
{
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    int fold = args->fold;
    if ((needle_length == 0ull) || (needle_length > args->length))
    {
        return 0ull;
    }
    // how far the needle moves on when the byte under its last stands nowhere in it but its last: its length; and for
    // each byte it contains before its last, the distance from that byte's last place to the end
    unsigned long long shift[256];
    for (unsigned int byte = 0u; byte < 256u; byte += 1u)
    {
        shift[byte] = needle_length;
    }
    for (unsigned long long at = 0ull; (at + 1ull) < needle_length; at += 1ull)
    {
        shift[EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = needle[at], .fold = fold)] =
            needle_length - 1ull - at;
    }
    const unsigned char last =
        EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = needle[needle_length - 1ull], .fold = fold);
    unsigned long long count = 0ull;
    unsigned long long at = 0ull;
    while ((at + needle_length) <= args->length)
    {
        const unsigned char under =
            EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->text[at + needle_length - 1ull], .fold = fold);
        if ((under == last) && EMBED_CALL(cli_stands_at, SiftCtx, .window = args->text, .needle = needle,
                                          .needle_length = needle_length, .at = at, .fold = fold))
        {
            EMBED_CALL(args->found, CliEachAtCfg, .context = args->context, .at = at);
            count += 1ull;
        }
        at += shift[under];
    }
    return count;
}

// every offset `needle` stands at in `text` on `path`, each window planned by `census` where one is given
EMBED_INLINE unsigned long long cli_find_all_census(const SiftCtx *args)
{
    const CliCensus *census = args->census;
    unsigned long long length = args->length;
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    int fold = args->fold;
    if ((needle_length == 0ull) || (needle_length > length))
    {
        return 0ull;
    }
    // a text within one window is too small for a plan to pay for itself: the needle's own shifts find it faster
    if (length < CLI_SIFT_WINDOW)
    {
        return EMBED_CALL(cli_sift_length.horspool, CliSiftLengthCfg, .text = args->text, .length = length,
                          .needle = needle, .needle_length = needle_length, .fold = fold, .found = args->found,
                          .context = args->context);
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
            EMBED_CALL(cli_sift_window_length_fold.plan_with, CliSiftWindowLengthFoldCfg, .census_read = census,
                       .window = args->text + start, .length = end - start, .needle = needle,
                       .needle_length = needle_length, .fold = fold, .anchors_out = anchors);
        count += EMBED_CALL(cli_window_on, SiftCtx, .path = args->path, .window = args->text + start,
                            .length = end - start, .needle = needle, .needle_length = needle_length,
                            .anchors_read = anchors, .anchor_count = placed, .fold = fold, .base = start,
                            .found = args->found, .context = args->context);
    }
    return count;
}

unsigned long long cli_sift_find_all_on(const CliSiftLengthCfg *args)
{
    unsigned int path = args->path;
    const unsigned char *text = args->text;
    unsigned long long length = args->length;
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    int fold = args->fold;
    void (*found)(const CliEachAtCfg *args) = args->found;
    void *context = args->context;
    return EMBED_CALL(cli_find_all_census, SiftCtx, .path = path, .census = NULL, .text = text, .length = length,
                      .needle = needle, .needle_length = needle_length, .fold = fold, .found = found,
                      .context = context);
}

unsigned long long cli_sift_find_all(const CliSiftLengthCfg *args)
{
    const unsigned char *text = args->text;
    unsigned long long length = args->length;
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    int fold = args->fold;
    void (*found)(const CliEachAtCfg *args) = args->found;
    void *context = args->context;
    return EMBED_CALL(cli_find_all_census, SiftCtx, .path = cli_vector_best(), .census = NULL, .text = text,
                      .length = length, .needle = needle, .needle_length = needle_length, .fold = fold, .found = found,
                      .context = context);
}

unsigned long long cli_sift_find_all_with(const CliSiftLengthFoldCfg *args)
{
    const CliCensus *census = args->census_read;
    const unsigned char *text = args->text;
    unsigned long long length = args->length;
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    int fold = args->fold;
    void (*found)(const CliEachAtCfg *args) = args->found;
    void *context = args->context;
    return EMBED_CALL(cli_find_all_census, SiftCtx, .path = cli_vector_best(), .census = census, .text = text,
                      .length = length, .needle = needle, .needle_length = needle_length, .fold = fold, .found = found,
                      .context = context);
}
