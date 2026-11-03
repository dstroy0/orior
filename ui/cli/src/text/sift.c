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
static unsigned long long s_existing[(2u * CLI_SIFT_SCORED) + 2u];

unsigned char cli_sift_on_fold(const CliSiftFoldCfg *args)
{
    // a capital and its small letter differ in the one bit 0x20
    return (args->fold && (args->byte >= 'A') && (args->byte <= 'Z')) ? (unsigned char)(args->byte | 0x20u)
                                                                      : args->byte;
}

void cli_sift_histogram(const CliSiftWindowLengthFoldHistogramCfg *args)
{
    const unsigned long long stride =
        ((args->length / CLI_SIFT_COUNTED) > 1ULL) ? (args->length / CLI_SIFT_COUNTED) : 1ULL;
    memor.set(&(MemoriaCfg){.dst = args->histogram, .val = 0, .bytes = sizeof(*args->histogram)});
    for (unsigned long long at = 0ULL; at < args->length; at += stride)
    {
        args->histogram->occurrences[EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->window[at],
                                                .fold = args->fold)] += 1ULL;
        args->histogram->total += 1ULL;
    }
}

void cli_sift_histogram_count(const CliSiftLengthHistogramCfg *args)
{
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        args->histogram->occurrences[args->bytes[at]] += 1ULL;
    }
    args->histogram->total += args->length;
}

void cli_sift_histogram_uncounted(const CliSiftLengthHistogramCfg *args)
{
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        args->histogram->occurrences[args->bytes[at]] -= 1ULL;
    }
    args->histogram->total -= args->length;
}

void cli_sift_histogram_fold(const CliSiftCfg *args)
{
    CliHistogram *folded = args->folded;
    *folded = *args->exact;
    for (unsigned int byte = 'A'; byte <= 'Z'; byte += 1u)
    {
        // a capital's count joins its small letter's, which stands at the byte with 0x20 set
        folded->occurrences[byte | 0x20u] += folded->occurrences[byte];
        folded->occurrences[byte] = 0ULL;
    }
}

typedef struct
{
    const CliHistogram *histogram;
    unsigned char byte;
    const unsigned char *window;
    const unsigned char *needle;
    unsigned long long offset;
    unsigned long long at;
    int fold;
    const CliHistogram *supplied;
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
    unsigned long long existing;
    const unsigned long long *candidates_read;
    unsigned int candidate_count;
    unsigned long long *left_out;
} SiftCtx;
// How rare a byte is in the census, larger the rarer: the bytes counted that are other bytes
EMBED_INLINE unsigned long long cli_magnitude(const SiftCtx *args)
{
    return args->histogram->total - args->histogram->occurrences[args->byte];
}

// whether the window agrees with the needle at `offset` from the alignment `at`
EMBED_INLINE int cli_agrees(const SiftCtx *args)
{
    int fold = args->fold;
    return EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->window[args->at + args->offset],
                      .fold = fold) ==
           EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->needle[args->offset], .fold = fold);
}

// whether `offset` is among the first `anchor_count` of `anchors_read`
EMBED_INLINE int cli_offset_among(const SiftCtx *args)
{
    for (unsigned int at = 0u; at < args->anchor_count; at += 1u)
    {
        if (args->anchors_read[at] == args->offset)
        {
            return 1;
        }
    }
    return 0;
}

// how rare the needle's byte at `offset` is in `histogram`, folded where `fold` is set
EMBED_INLINE unsigned long long cli_needle_magnitude(const SiftCtx *args)
{
    return EMBED_CALL(
        cli_magnitude, SiftCtx, .histogram = args->histogram,
        .byte = EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->needle[args->offset], .fold = args->fold));
}

// The candidates into `anchors`, at most CLI_SIFT_CANDIDATES: the needle's offsets, the byte rarest in `histogram`
// first, ties in the needle's order; a byte the needle repeats tests at each of its offsets what it tests at another
// place, which the descent may want. Their count
EMBED_INLINE unsigned int cli_candidates_ranked(const SiftCtx *args)
{
    unsigned long long *const candidates = args->anchors;
    unsigned int candidate_count = 0u;
    int going = 1;
    while (going && (candidate_count < CLI_SIFT_CANDIDATES))
    {
        unsigned long long best = args->needle_length;
        for (unsigned long long offset = 0ULL; offset < args->needle_length; offset += 1ULL)
        {
            if (!EMBED_CALL(cli_offset_among, SiftCtx, .anchors_read = candidates, .anchor_count = candidate_count,
                            .offset = offset) &&
                ((best == args->needle_length) ||
                 (EMBED_CALL(cli_needle_magnitude, SiftCtx, .histogram = args->histogram, .needle = args->needle,
                             .fold = args->fold, .offset = offset) >
                  EMBED_CALL(cli_needle_magnitude, SiftCtx, .histogram = args->histogram, .needle = args->needle,
                             .fold = args->fold, .offset = best))))
            {
                best = offset;
            }
        }
        going = best != args->needle_length;
        if (going)
        {
            candidates[candidate_count] = best;
            candidate_count += 1u;
        }
    }
    return candidate_count;
}

// the alignments the descent scores on, every so many of those a window of `length` holds for a needle of
// `needle_length`, into s_existing as far as it holds them: their count
EMBED_INLINE unsigned long long cli_alignments_sampled(const SiftCtx *args)
{
    const unsigned long long alignments = args->length - args->needle_length + 1ULL;
    const unsigned long long stride = ((alignments / CLI_SIFT_SCORED) > 1ULL) ? (alignments / CLI_SIFT_SCORED) : 1ULL;
    unsigned long long existing = 0ULL;
    unsigned long long at = 0ULL;
    while ((at < alignments) && (existing < (sizeof(s_existing) / sizeof(s_existing[0]))))
    {
        s_existing[existing] = at;
        existing += 1ULL;
        at += stride;
    }
    return existing;
}

// how many of the first `existing` alignments in s_existing agree with the needle at `offset`, those an anchor there
// leaves
EMBED_INLINE unsigned long long cli_agreeing(const SiftCtx *args)
{
    unsigned long long left = 0ULL;
    for (unsigned long long at = 0ULL; at < args->existing; at += 1ULL)
    {
        left += EMBED_CALL(cli_agrees, SiftCtx, .window = args->window, .needle = args->needle, .offset = args->offset,
                           .at = s_existing[at], .fold = args->fold)
                    ? 1ULL
                    : 0ULL;
    }
    return left;
}

// the candidate of the first `candidate_count` of `candidates_read`, not among the `anchor_count` placed in
// `anchors_read`, that leaves the least of the `existing` alignments, the lesser offset among equals, the count it
// leaves into `left_out`: `needle_length` where every candidate is placed
EMBED_INLINE unsigned long long cli_candidate_best(const SiftCtx *args)
{
    unsigned long long best_left = 0ULL;
    unsigned long long best_offset = args->needle_length;
    for (unsigned int each = 0u; each < args->candidate_count; each += 1u)
    {
        const unsigned long long offset = args->candidates_read[each];
        if (EMBED_CALL(cli_offset_among, SiftCtx, .anchors_read = args->anchors_read,
                       .anchor_count = args->anchor_count, .offset = offset))
        {
            continue;
        }
        const unsigned long long left =
            EMBED_CALL(cli_agreeing, SiftCtx, .window = args->window, .needle = args->needle, .offset = offset,
                       .fold = args->fold, .existing = args->existing);
        // the least refuted left, the lesser offset among equals
        if ((best_offset == args->needle_length) || (left < best_left) ||
            ((left == best_left) && (offset < best_offset)))
        {
            best_left = left;
            best_offset = offset;
        }
    }
    *args->left_out = best_left;
    return best_offset;
}

// the first `existing` alignments in s_existing that agree with the needle at `offset` moved to its start: their count
EMBED_INLINE unsigned long long cli_alignments_kept(const SiftCtx *args)
{
    unsigned long long count = 0ULL;
    for (unsigned long long at = 0ULL; at < args->existing; at += 1ULL)
    {
        if (EMBED_CALL(cli_agrees, SiftCtx, .window = args->window, .needle = args->needle, .offset = args->offset,
                       .at = s_existing[at], .fold = args->fold))
        {
            s_existing[count] = s_existing[at];
            count += 1ULL;
        }
    }
    return count;
}

// the anchors planned for `window`, ranked by `supplied`, a census stored with the text, where one is given, else by a
// count over the window
EMBED_INLINE unsigned int cli_plan_anchors_position(const SiftCtx *args)
{
    const unsigned char *window = args->window;
    unsigned long long length = args->length;
    int fold = args->fold;
    if ((args->needle_length == 0ULL) || (args->needle_length > length))
    {
        return 0u;
    }
    CliHistogram histogram;
    if (args->supplied != NULL)
    {
        histogram = *args->supplied;
    }
    else
    {
        EMBED_CALL(cli_sift_window_length_fold_histogram.histogram, CliSiftWindowLengthFoldHistogramCfg,
                   .window = window, .length = length, .fold = fold, .histogram = &histogram);
    }
    unsigned long long candidates[CLI_SIFT_CANDIDATES];
    const unsigned int candidate_count =
        EMBED_CALL(cli_candidates_ranked, SiftCtx, .histogram = &histogram, .needle = args->needle,
                   .needle_length = args->needle_length, .fold = fold, .anchors = candidates);
    unsigned long long existing =
        EMBED_CALL(cli_alignments_sampled, SiftCtx, .length = length, .needle_length = args->needle_length);
    unsigned int positioned = 0u;
    int going = 1;
    while (going && (positioned < CLI_SIFT_ANCHORS))
    {
        unsigned long long best_left = 0ULL;
        const unsigned long long best_offset = EMBED_CALL(
            cli_candidate_best, SiftCtx, .candidates_read = candidates, .candidate_count = candidate_count,
            .anchors_read = args->anchors, .anchor_count = positioned, .window = window, .needle = args->needle,
            .needle_length = args->needle_length, .fold = fold, .existing = existing, .left_out = &best_left);
        // A level that refutes nothing the levels above it did not ends the descent, and the first level is always
        // placed
        going = (best_offset != args->needle_length) && ((positioned == 0u) || (best_left < existing));
        if (going)
        {
            existing = EMBED_CALL(cli_alignments_kept, SiftCtx, .window = window, .needle = args->needle,
                                  .offset = best_offset, .fold = fold, .existing = existing);
            args->anchors[positioned] = best_offset;
            positioned += 1u;
        }
    }
    return positioned;
}

unsigned int cli_sift_plan(const CliSiftWindowLengthFoldCfg *args)
{
    return EMBED_CALL(cli_sift_window_length_fold.plan_with, CliSiftWindowLengthFoldCfg, .histogram_read = NULL,
                      .window = args->window, .length = args->length, .needle = args->needle,
                      .needle_length = args->needle_length, .fold = args->fold, .anchors_out = args->anchors_out);
}

unsigned int cli_sift_plan_with(const CliSiftWindowLengthFoldCfg *args)
{
    const unsigned int positioned =
        EMBED_CALL(cli_plan_anchors_position, SiftCtx, .supplied = args->histogram_read, .window = args->window,
                   .length = args->length, .needle = args->needle, .needle_length = args->needle_length,
                   .fold = args->fold, .anchors = args->anchors_out);
    return positioned;
}

// whether the whole needle stands at `at`
EMBED_INLINE int cli_exists_at(const SiftCtx *args)
{
    int fold = args->fold;
    for (unsigned long long step = 0ULL; step < args->needle_length; step += 1ULL)
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
    return EMBED_CALL(cli_exists_at, SiftCtx, .window = args->window, .needle = args->needle,
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
    const unsigned long long alignments = args->length - needle_length + 1ULL;
    unsigned long long count = 0ULL;
    for (unsigned long long at = args->start; at < alignments; at += 1ULL)
    {
        if (EMBED_CALL(cli_sift_window_fold.matches, CliSiftWindowFoldCfg, .window = window, .needle = needle,
                       .needle_length = needle_length, .anchors = anchors, .anchor_count = anchor_count, .from = 0u,
                       .at = at, .fold = fold))
        {
            EMBED_CALL(args->found, CliEachAtCfg, .context = args->context, .at = args->base + at);
            count += 1ULL;
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
                      .anchor_count = args->anchor_count, .fold = args->fold, .start = 0ULL, .base = args->base,
                      .found = args->found, .context = args->context);
}

unsigned long long cli_sift_horspool(const CliSiftLengthCfg *args)
{
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    int fold = args->fold;
    if ((needle_length == 0ULL) || (needle_length > args->length))
    {
        return 0ULL;
    }
    // how far the needle moves on when the byte under its last stands nowhere in it but its last: its length; and for
    // each byte it contains before its last, the distance from that byte's last place to the end
    unsigned long long shift[256];
    for (unsigned int byte = 0u; byte < 256u; byte += 1u)
    {
        shift[byte] = needle_length;
    }
    for (unsigned long long at = 0ULL; (at + 1ULL) < needle_length; at += 1ULL)
    {
        shift[EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = needle[at], .fold = fold)] =
            needle_length - 1ULL - at;
    }
    const unsigned char last =
        EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = needle[needle_length - 1ULL], .fold = fold);
    unsigned long long count = 0ULL;
    unsigned long long at = 0ULL;
    while ((at + needle_length) <= args->length)
    {
        const unsigned char under =
            EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = args->text[at + needle_length - 1ULL], .fold = fold);
        if ((under == last) && EMBED_CALL(cli_exists_at, SiftCtx, .window = args->text, .needle = needle,
                                          .needle_length = needle_length, .at = at, .fold = fold))
        {
            EMBED_CALL(args->found, CliEachAtCfg, .context = args->context, .at = at);
            count += 1ULL;
        }
        at += shift[under];
    }
    return count;
}

// every offset `needle` stands at in `text` on `path`, each window planned by `histogram` where one is given
EMBED_INLINE unsigned long long cli_find_all_histogram(const SiftCtx *args)
{
    const CliHistogram *histogram = args->histogram;
    unsigned long long length = args->length;
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    int fold = args->fold;
    if ((needle_length == 0ULL) || (needle_length > length))
    {
        return 0ULL;
    }
    // a text within one window is too small for a plan to pay for itself: the needle's own shifts find it faster
    if (length < CLI_SIFT_WINDOW)
    {
        return EMBED_CALL(cli_sift_length.horspool, CliSiftLengthCfg, .text = args->text, .length = length,
                          .needle = needle, .needle_length = needle_length, .fold = fold, .found = args->found,
                          .context = args->context);
    }
    const unsigned long long reach =
        (CLI_SIFT_WINDOW > (needle_length * 2ULL)) ? CLI_SIFT_WINDOW : (needle_length * 2ULL);
    const unsigned long long step = reach - (needle_length - 1ULL);
    unsigned long long count = 0ULL;
    for (unsigned long long start = 0ULL; (start + needle_length) <= length; start += step)
    {
        const unsigned long long end =
            ((start + step + needle_length - 1ULL) < length) ? (start + step + needle_length - 1ULL) : length;
        unsigned long long anchors[CLI_SIFT_ANCHORS];
        const unsigned int positioned =
            EMBED_CALL(cli_sift_window_length_fold.plan_with, CliSiftWindowLengthFoldCfg, .histogram_read = histogram,
                       .window = args->text + start, .length = end - start, .needle = needle,
                       .needle_length = needle_length, .fold = fold, .anchors_out = anchors);
        count += EMBED_CALL(cli_window_on, SiftCtx, .path = args->path, .window = args->text + start,
                            .length = end - start, .needle = needle, .needle_length = needle_length,
                            .anchors_read = anchors, .anchor_count = positioned, .fold = fold, .base = start,
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
    void (*found)(const CliEachAtCfg *) = args->found;
    void *context = args->context;
    return EMBED_CALL(cli_find_all_histogram, SiftCtx, .path = path, .histogram = NULL, .text = text, .length = length,
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
    void (*found)(const CliEachAtCfg *) = args->found;
    void *context = args->context;
    return EMBED_CALL(cli_find_all_histogram, SiftCtx, .path = cli_vector_best(), .histogram = NULL, .text = text,
                      .length = length, .needle = needle, .needle_length = needle_length, .fold = fold, .found = found,
                      .context = context);
}

unsigned long long cli_sift_find_all_with(const CliSiftLengthFoldCfg *args)
{
    const CliHistogram *histogram = args->histogram_read;
    const unsigned char *text = args->text;
    unsigned long long length = args->length;
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    int fold = args->fold;
    void (*found)(const CliEachAtCfg *) = args->found;
    void *context = args->context;
    return EMBED_CALL(cli_find_all_histogram, SiftCtx, .path = cli_vector_best(), .histogram = histogram, .text = text,
                      .length = length, .needle = needle, .needle_length = needle_length, .fold = fold, .found = found,
                      .context = context);
}
