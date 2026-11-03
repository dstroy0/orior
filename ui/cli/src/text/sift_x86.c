// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sift_x86.c: a window's alignments tested by its first two anchors in SSE2, sixteen a block, and in AVX2, thirty-two
#include "../command_line/callback_cfg.h"
#include "sift.h"

#if ENGINE_TARGET_X86_64

#include <immintrin.h>

#if defined(_MSC_VER) && !defined(__clang__)
#define CLI_AVX2_TARGET
#else
#define CLI_AVX2_TARGET __attribute__((target("avx2")))
#endif

typedef struct
{
#if (ENGINE_TARGET_X86_64)
    unsigned int mask;
    __m128i block;
    int fold;
    __m256i block_m256i;
#endif
} SiftX86Ctx;
// the place of the lowest bit set of a mask that is not 0
EMBED_INLINE unsigned int cli_lowest(const SiftX86Ctx *args)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0UL;
    _BitScanForward(&index, args->mask);
    // the bit index of a non-zero 32 bit mask is 0 to 31
    return (unsigned int)index;
#else
    // the trailing count of a non-zero 32 bit mask is 0 to 31
    return (unsigned int)__builtin_ctz(args->mask);
#endif
}

// A block of sixteen bytes as a search without case reads them where `fold` is set: each capital to its small letter
EMBED_INLINE __m128i cli_fold16(const SiftX86Ctx *args)
{
    if (!args->fold)
    {
        return args->block;
    }
    const __m128i end = _mm_sub_epi8(args->block, _mm_set1_epi8('A'));
    const __m128i upper = _mm_cmpeq_epi8(_mm_subs_epu8(end, _mm_set1_epi8(25)), _mm_setzero_si128());
    return _mm_or_si128(args->block, _mm_and_si128(upper, _mm_set1_epi8(0x20)));
}

CLI_AVX2_TARGET static __m256i cli_fold32(const SiftX86Ctx *args)
{
    if (!args->fold)
    {
        return args->block_m256i;
    }
    const __m256i end = _mm256_sub_epi8(args->block_m256i, _mm256_set1_epi8('A'));
    const __m256i upper = _mm256_cmpeq_epi8(_mm256_subs_epu8(end, _mm256_set1_epi8(25)), _mm256_setzero_si256());
    return _mm256_or_si256(args->block_m256i, _mm256_and_si256(upper, _mm256_set1_epi8(0x20)));
}

// The window sixteen alignments at a time: the first two anchors in one compare each, the rest and the whole needle
// on each alignment both agree with
unsigned long long cli_sift_window_sse2(const CliSiftWindowLengthCfg *args)
{
    const unsigned char *window = args->window;
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    const unsigned long long *anchors = args->anchors;
    unsigned int anchor_count = args->anchor_count;
    int fold = args->fold;
    const unsigned long long alignments = args->length - needle_length + 1ULL;
    const unsigned long long first = anchors[0];
    const unsigned long long second = (anchor_count > 1u) ? anchors[1] : first;
    // a byte is set in each lane as the 8 bits it is
    const __m128i want_first =
        _mm_set1_epi8((char)EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = needle[first], .fold = fold));
    const __m128i want_second =
        _mm_set1_epi8((char)EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = needle[second], .fold = fold));
    const unsigned int tested = (anchor_count < 2u) ? anchor_count : 2u;
    unsigned long long count = 0ULL;
    unsigned long long at = 0ULL;
    while ((at + 16ULL) <= alignments)
    {
        const __m128i a =
            EMBED_CALL(cli_fold16, SiftX86Ctx,
                       .block = _mm_loadu_si128((const __m128i *)(const void *)(window + at + first)), .fold = fold);
        const __m128i b =
            EMBED_CALL(cli_fold16, SiftX86Ctx,
                       .block = _mm_loadu_si128((const __m128i *)(const void *)(window + at + second)), .fold = fold);
        // the movemask of a block is its 16 bits
        unsigned int mask = (unsigned int)_mm_movemask_epi8(
            _mm_and_si128(_mm_cmpeq_epi8(a, want_first), _mm_cmpeq_epi8(b, want_second)));
        while (mask != 0u)
        {
            const unsigned long long here = at + EMBED_CALL(cli_lowest, SiftX86Ctx, .mask = mask);
            if (EMBED_CALL(cli_sift_window_fold.matches, CliSiftWindowFoldCfg, .window = window, .needle = needle,
                           .needle_length = needle_length, .anchors = anchors, .anchor_count = anchor_count,
                           .from = tested, .at = here, .fold = fold))
            {
                EMBED_CALL(args->found, CliEachAtCfg, .context = args->context, .at = args->base + here);
                count += 1ULL;
            }
            mask &= mask - 1u;
        }
        at += 16ULL;
    }
    return count + EMBED_CALL(cli_sift_window.plain_window, CliSiftWindowCfg, .window = window, .length = args->length,
                              .needle = needle, .needle_length = needle_length, .anchors = anchors,
                              .anchor_count = anchor_count, .fold = fold, .start = at, .base = args->base,
                              .found = args->found, .context = args->context);
}

// The window thirty-two alignments at a time, as the SSE2 loop does sixteen
CLI_AVX2_TARGET unsigned long long cli_sift_window_avx2(const CliSiftWindowLengthCfg *args)
{
    const unsigned char *window = args->window;
    const unsigned char *needle = args->needle;
    unsigned long long needle_length = args->needle_length;
    const unsigned long long *anchors = args->anchors;
    unsigned int anchor_count = args->anchor_count;
    int fold = args->fold;
    const unsigned long long alignments = args->length - needle_length + 1ULL;
    const unsigned long long first = anchors[0];
    const unsigned long long second = (anchor_count > 1u) ? anchors[1] : first;
    // a byte is set in each lane as the 8 bits it is
    const __m256i want_first =
        _mm256_set1_epi8((char)EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = needle[first], .fold = fold));
    const __m256i want_second =
        _mm256_set1_epi8((char)EMBED_CALL(cli_sift_fold.fold, CliSiftFoldCfg, .byte = needle[second], .fold = fold));
    const unsigned int tested = (anchor_count < 2u) ? anchor_count : 2u;
    unsigned long long count = 0ULL;
    unsigned long long at = 0ULL;
    while ((at + 32ULL) <= alignments)
    {
        const __m256i a = EMBED_CALL(
            cli_fold32, SiftX86Ctx,
            .block_m256i = _mm256_loadu_si256((const __m256i *)(const void *)(window + at + first)), .fold = fold);
        const __m256i b = EMBED_CALL(
            cli_fold32, SiftX86Ctx,
            .block_m256i = _mm256_loadu_si256((const __m256i *)(const void *)(window + at + second)), .fold = fold);
        // the movemask of a block is its 32 bits
        unsigned int mask = (unsigned int)_mm256_movemask_epi8(
            _mm256_and_si256(_mm256_cmpeq_epi8(a, want_first), _mm256_cmpeq_epi8(b, want_second)));
        while (mask != 0u)
        {
            const unsigned long long here = at + EMBED_CALL(cli_lowest, SiftX86Ctx, .mask = mask);
            if (EMBED_CALL(cli_sift_window_fold.matches, CliSiftWindowFoldCfg, .window = window, .needle = needle,
                           .needle_length = needle_length, .anchors = anchors, .anchor_count = anchor_count,
                           .from = tested, .at = here, .fold = fold))
            {
                EMBED_CALL(args->found, CliEachAtCfg, .context = args->context, .at = args->base + here);
                count += 1ULL;
            }
            mask &= mask - 1u;
        }
        at += 32ULL;
    }
    return count + EMBED_CALL(cli_sift_window.plain_window, CliSiftWindowCfg, .window = window, .length = args->length,
                              .needle = needle, .needle_length = needle_length, .anchors = anchors,
                              .anchor_count = anchor_count, .fold = fold, .start = at, .base = args->base,
                              .found = args->found, .context = args->context);
}

#endif
