// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sift_x86.c: a window's alignments tested by its first two anchors in SSE2, sixteen a block, and in AVX2, thirty-two
#include "sift.h"

#if ENGINE_TARGET_X86_64

#include <immintrin.h>

#if defined(_MSC_VER) && !defined(__clang__)
#define CLI_AVX2_TARGET
#else
#define CLI_AVX2_TARGET __attribute__((target("avx2")))
#endif

// the place of the lowest bit set of a mask that is not 0
static unsigned int cli_lowest(unsigned int mask)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0ul;
    _BitScanForward(&index, mask);
    // the bit index of a non-zero 32 bit mask is 0 to 31
    return (unsigned int)index;
#else
    // the trailing count of a non-zero 32 bit mask is 0 to 31
    return (unsigned int)__builtin_ctz(mask);
#endif
}

// A block of sixteen bytes as a search without case reads them where `fold` is set: each capital to its small letter
static __m128i cli_fold16(__m128i block, int fold)
{
    if (!fold)
    {
        return block;
    }
    const __m128i past = _mm_sub_epi8(block, _mm_set1_epi8('A'));
    const __m128i upper = _mm_cmpeq_epi8(_mm_subs_epu8(past, _mm_set1_epi8(25)), _mm_setzero_si128());
    return _mm_or_si128(block, _mm_and_si128(upper, _mm_set1_epi8(0x20)));
}

CLI_AVX2_TARGET static __m256i cli_fold32(__m256i block, int fold)
{
    if (!fold)
    {
        return block;
    }
    const __m256i past = _mm256_sub_epi8(block, _mm256_set1_epi8('A'));
    const __m256i upper = _mm256_cmpeq_epi8(_mm256_subs_epu8(past, _mm256_set1_epi8(25)), _mm256_setzero_si256());
    return _mm256_or_si256(block, _mm256_and_si256(upper, _mm256_set1_epi8(0x20)));
}

// The window sixteen alignments at a time: the first two anchors in one compare each, the rest and the whole needle
// on each alignment both agree with
unsigned long long cli_sift_window_sse2(const unsigned char *window, unsigned long long length,
                                        const unsigned char *needle, unsigned long long needle_length,
                                        const unsigned long long *anchors, unsigned int anchor_count, int fold,
                                        unsigned long long base, void (*found)(void *context, unsigned long long at),
                                        void *context)
{
    const unsigned long long alignments = length - needle_length + 1ull;
    const unsigned long long first = anchors[0];
    const unsigned long long second = (anchor_count > 1u) ? anchors[1] : first;
    // a byte is set in each lane as the 8 bits it is
    const __m128i want_first = _mm_set1_epi8((char)cli_sift_folded(needle[first], fold));
    const __m128i want_second = _mm_set1_epi8((char)cli_sift_folded(needle[second], fold));
    const unsigned int tested = (anchor_count < 2u) ? anchor_count : 2u;
    unsigned long long count = 0ull;
    unsigned long long at = 0ull;
    while ((at + 16ull) <= alignments)
    {
        const __m128i a = cli_fold16(_mm_loadu_si128((const __m128i *)(const void *)(window + at + first)), fold);
        const __m128i b = cli_fold16(_mm_loadu_si128((const __m128i *)(const void *)(window + at + second)), fold);
        // the movemask of a block is its 16 bits
        unsigned int mask = (unsigned int)_mm_movemask_epi8(
            _mm_and_si128(_mm_cmpeq_epi8(a, want_first), _mm_cmpeq_epi8(b, want_second)));
        while (mask != 0u)
        {
            const unsigned long long here = at + cli_lowest(mask);
            if (cli_sift_holds(window, needle, needle_length, anchors, anchor_count, tested, here, fold))
            {
                found(context, base + here);
                count += 1ull;
            }
            mask &= mask - 1u;
        }
        at += 16ull;
    }
    return count + cli_sift_plain_window(window, length, needle, needle_length, anchors, anchor_count, fold, at, base,
                                         found, context);
}

// The window thirty-two alignments at a time, as the SSE2 loop does sixteen
CLI_AVX2_TARGET unsigned long long cli_sift_window_avx2(const unsigned char *window, unsigned long long length,
                                                        const unsigned char *needle, unsigned long long needle_length,
                                                        const unsigned long long *anchors, unsigned int anchor_count,
                                                        int fold, unsigned long long base,
                                                        void (*found)(void *context, unsigned long long at),
                                                        void *context)
{
    const unsigned long long alignments = length - needle_length + 1ull;
    const unsigned long long first = anchors[0];
    const unsigned long long second = (anchor_count > 1u) ? anchors[1] : first;
    // a byte is set in each lane as the 8 bits it is
    const __m256i want_first = _mm256_set1_epi8((char)cli_sift_folded(needle[first], fold));
    const __m256i want_second = _mm256_set1_epi8((char)cli_sift_folded(needle[second], fold));
    const unsigned int tested = (anchor_count < 2u) ? anchor_count : 2u;
    unsigned long long count = 0ull;
    unsigned long long at = 0ull;
    while ((at + 32ull) <= alignments)
    {
        const __m256i a = cli_fold32(_mm256_loadu_si256((const __m256i *)(const void *)(window + at + first)), fold);
        const __m256i b = cli_fold32(_mm256_loadu_si256((const __m256i *)(const void *)(window + at + second)), fold);
        // the movemask of a block is its 32 bits
        unsigned int mask = (unsigned int)_mm256_movemask_epi8(
            _mm256_and_si256(_mm256_cmpeq_epi8(a, want_first), _mm256_cmpeq_epi8(b, want_second)));
        while (mask != 0u)
        {
            const unsigned long long here = at + cli_lowest(mask);
            if (cli_sift_holds(window, needle, needle_length, anchors, anchor_count, tested, here, fold))
            {
                found(context, base + here);
                count += 1ull;
            }
            mask &= mask - 1u;
        }
        at += 32ull;
    }
    return count + cli_sift_plain_window(window, length, needle, needle_length, anchors, anchor_count, fold, at, base,
                                         found, context);
}

#endif
