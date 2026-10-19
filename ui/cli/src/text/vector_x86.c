// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vector_x86.c: the loops in SSE2, eight units or two numbers a block, and in AVX2, sixteen units or four numbers
#include "vector.h"

#if ENGINE_TARGET_X86_64

#include <immintrin.h>

#if !defined(_MSC_VER) || defined(__clang__)
#include <cpuid.h>
#endif

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

int cli_vector_avx2_present(void)
{
#if defined(_MSC_VER) && !defined(__clang__)
    int leaves[4];
    __cpuid(leaves, 0);
    if (leaves[0] < 7)
    {
        return 0;
    }
    __cpuid(leaves, 1);
    // leaf 1 ecx is read as its bit pattern
    const unsigned int features = (unsigned int)leaves[2];
    if ((((features >> 27u) & 1u) == 0u) || ((_xgetbv(0u) & 6ull) != 6ull))
    {
        return 0;
    }
    __cpuidex(leaves, 7, 0);
    // leaf 7 ebx is read as its bit pattern
    return ((((unsigned int)leaves[1]) >> 5u) & 1u) != 0u;
#else
    unsigned int eax = 0u;
    unsigned int ebx = 0u;
    unsigned int features = 0u;
    unsigned int edx = 0u;
    if ((__get_cpuid(1u, &eax, &ebx, &features, &edx) == 0) || (((features >> 27u) & 1u) == 0u))
    {
        return 0;
    }
    unsigned int low = 0u;
    unsigned int high = 0u;
    __asm__ __volatile__("xgetbv" : "=a"(low), "=d"(high) : "c"(0u));
    if ((low & 6u) != 6u)
    {
        return 0;
    }
    unsigned int extended = 0u;
    unsigned int ecx = 0u;
    if (__get_cpuid_count(7u, 0u, &eax, &extended, &ecx, &edx) == 0)
    {
        return 0;
    }
    return (((extended >> 5u) & 1u) != 0u) ? 1 : 0;
#endif
}

// Each unit of a block of eight folded: A to Z to a to z, `ſ` to s and the Kelvin sign to k
static __m128i cli_fold8(__m128i block)
{
    const __m128i past = _mm_sub_epi16(block, _mm_set1_epi16(0x41));
    const __m128i upper = _mm_cmpeq_epi16(_mm_subs_epu16(past, _mm_set1_epi16(25)), _mm_setzero_si128());
    __m128i lowered = _mm_or_si128(block, _mm_and_si128(upper, _mm_set1_epi16(0x20)));
    const __m128i long_s = _mm_cmpeq_epi16(block, _mm_set1_epi16(CLI_LONG_S));
    const __m128i kelvin = _mm_cmpeq_epi16(block, _mm_set1_epi16(CLI_KELVIN));
    lowered = _mm_or_si128(_mm_andnot_si128(long_s, lowered), _mm_and_si128(long_s, _mm_set1_epi16('s')));
    return _mm_or_si128(_mm_andnot_si128(kelvin, lowered), _mm_and_si128(kelvin, _mm_set1_epi16('k')));
}

CLI_AVX2_TARGET static __m256i cli_fold16(__m256i block)
{
    const __m256i past = _mm256_sub_epi16(block, _mm256_set1_epi16(0x41));
    const __m256i upper = _mm256_cmpeq_epi16(_mm256_subs_epu16(past, _mm256_set1_epi16(25)), _mm256_setzero_si256());
    __m256i lowered = _mm256_or_si256(block, _mm256_and_si256(upper, _mm256_set1_epi16(0x20)));
    const __m256i long_s = _mm256_cmpeq_epi16(block, _mm256_set1_epi16(CLI_LONG_S));
    const __m256i kelvin = _mm256_cmpeq_epi16(block, _mm256_set1_epi16(CLI_KELVIN));
    lowered = _mm256_blendv_epi8(lowered, _mm256_set1_epi16('s'), long_s);
    return _mm256_blendv_epi8(lowered, _mm256_set1_epi16('k'), kelvin);
}

unsigned long long cli_vector_find_unit_sse2(const unsigned short *text, unsigned long long length, unsigned short unit)
{
    // a unit is set in each of a block's lanes as the 16 bits it is
    const __m128i wanted = _mm_set1_epi16((short)unit);
    unsigned long long at = 0ull;
    while ((at + 8ull) <= length)
    {
        const __m128i block = _mm_loadu_si128((const __m128i *)(const void *)(text + at));
        // the movemask of a block is its 16 bits
        const unsigned int mask = (unsigned int)_mm_movemask_epi8(_mm_cmpeq_epi16(block, wanted));
        if (mask != 0u)
        {
            return at + (cli_lowest(mask) / 2u);
        }
        at += 8ull;
    }
    for (; at < length; at += 1ull)
    {
        if (text[at] == unit)
        {
            return at;
        }
    }
    return CLI_VECTOR_NONE;
}

CLI_AVX2_TARGET unsigned long long cli_vector_find_unit_avx2(const unsigned short *text, unsigned long long length,
                                                             unsigned short unit)
{
    // a unit is set in each of a block's lanes as the 16 bits it is
    const __m256i wanted = _mm256_set1_epi16((short)unit);
    unsigned long long at = 0ull;
    while ((at + 16ull) <= length)
    {
        const __m256i block = _mm256_loadu_si256((const __m256i *)(const void *)(text + at));
        // the movemask of a block is its 32 bits
        const unsigned int mask = (unsigned int)_mm256_movemask_epi8(_mm256_cmpeq_epi16(block, wanted));
        if (mask != 0u)
        {
            return at + (cli_lowest(mask) / 2u);
        }
        at += 16ull;
    }
    const unsigned long long found = cli_vector_find_unit_sse2(text + at, length - at, unit);
    return (found == CLI_VECTOR_NONE) ? found : (at + found);
}

// Two numbers at a time, each the same where all its eight bytes are, then the plain loop
unsigned long long cli_vector_common_sse2(const unsigned long long *a, const unsigned long long *b,
                                          unsigned long long length)
{
    unsigned long long at = 0ull;
    while ((at + 2ull) <= length)
    {
        const __m128i one = _mm_loadu_si128((const __m128i *)(const void *)(a + at));
        const __m128i other = _mm_loadu_si128((const __m128i *)(const void *)(b + at));
        // the movemask of a block is its 16 bits
        const unsigned int same = (unsigned int)_mm_movemask_epi8(_mm_cmpeq_epi8(one, other));
        if (same != 0xffffu)
        {
            return at + (cli_lowest(~same) / 8u);
        }
        at += 2ull;
    }
    while ((at < length) && (a[at] == b[at]))
    {
        at += 1ull;
    }
    return at;
}

CLI_AVX2_TARGET unsigned long long cli_vector_common_avx2(const unsigned long long *a, const unsigned long long *b,
                                                          unsigned long long length)
{
    unsigned long long at = 0ull;
    while ((at + 4ull) <= length)
    {
        const __m256i one = _mm256_loadu_si256((const __m256i *)(const void *)(a + at));
        const __m256i other = _mm256_loadu_si256((const __m256i *)(const void *)(b + at));
        // the movemask of a block is its 32 bits
        const unsigned int same = (unsigned int)_mm256_movemask_epi8(_mm256_cmpeq_epi8(one, other));
        if (same != 0xffffffffu)
        {
            return at + (cli_lowest(~same) / 8u);
        }
        at += 4ull;
    }
    return at + cli_vector_common_sse2(a + at, b + at, length - at);
}

int cli_vector_same_sse2(const unsigned short *a, const unsigned short *b, unsigned long long length)
{
    unsigned long long at = 0ull;
    while ((at + 8ull) <= length)
    {
        const __m128i one = _mm_loadu_si128((const __m128i *)(const void *)(a + at));
        const __m128i other = _mm_loadu_si128((const __m128i *)(const void *)(b + at));
        if (_mm_movemask_epi8(_mm_cmpeq_epi16(one, other)) != 0xffff)
        {
            return 0;
        }
        at += 8ull;
    }
    return memor.cmp(&(MemoriaCfg){.src = a + at, .other = b + at, .bytes = (length - at) * sizeof(a[0])}) == 0;
}

CLI_AVX2_TARGET int cli_vector_same_avx2(const unsigned short *a, const unsigned short *b, unsigned long long length)
{
    unsigned long long at = 0ull;
    while ((at + 16ull) <= length)
    {
        const __m256i one = _mm256_loadu_si256((const __m256i *)(const void *)(a + at));
        const __m256i other = _mm256_loadu_si256((const __m256i *)(const void *)(b + at));
        if (_mm256_movemask_epi8(_mm256_cmpeq_epi16(one, other)) != -1)
        {
            return 0;
        }
        at += 16ull;
    }
    return cli_vector_same_sse2(a + at, b + at, length - at);
}

// Each place in a block of eight where the first unit sought stands and the last stands where it would, folded where
// `folded` is 1, checked whole; past the last whole block, the plain loop
unsigned long long cli_vector_find_sse2(const unsigned short *text, unsigned long long length,
                                        const unsigned short *sought, unsigned long long sought_length, int folded)
{
    const unsigned long long span = sought_length - 1ull;
    const unsigned short head_unit = folded ? cli_vector_fold(sought[0]) : sought[0];
    const unsigned short tail_unit = folded ? cli_vector_fold(sought[span]) : sought[span];
    // a unit is set in each lane as the 16 bits it is
    const __m128i first = _mm_set1_epi16((short)head_unit);
    const __m128i last = _mm_set1_epi16((short)tail_unit);
    unsigned long long at = 0ull;
    while ((at + span + 8ull) <= length)
    {
        __m128i head = _mm_loadu_si128((const __m128i *)(const void *)(text + at));
        __m128i tail = _mm_loadu_si128((const __m128i *)(const void *)(text + at + span));
        if (folded)
        {
            head = cli_fold8(head);
            tail = cli_fold8(tail);
        }
        const __m128i both = _mm_and_si128(_mm_cmpeq_epi16(head, first), _mm_cmpeq_epi16(tail, last));
        // the movemask of a block is its 16 bits, one of each lane's two kept
        unsigned int mask = ((unsigned int)_mm_movemask_epi8(both)) & 0x5555u;
        while (mask != 0u)
        {
            const unsigned long long place = at + (cli_lowest(mask) / 2u);
            if (cli_vector_stands_at(text, sought, sought_length, place, folded))
            {
                return place;
            }
            mask &= mask - 1u;
        }
        at += 8ull;
    }
    return cli_vector_plain_find(text, length, sought, sought_length, folded, at);
}

CLI_AVX2_TARGET unsigned long long cli_vector_find_avx2(const unsigned short *text, unsigned long long length,
                                                        const unsigned short *sought, unsigned long long sought_length,
                                                        int folded)
{
    const unsigned long long span = sought_length - 1ull;
    const unsigned short head_unit = folded ? cli_vector_fold(sought[0]) : sought[0];
    const unsigned short tail_unit = folded ? cli_vector_fold(sought[span]) : sought[span];
    // a unit is set in each lane as the 16 bits it is
    const __m256i first = _mm256_set1_epi16((short)head_unit);
    const __m256i last = _mm256_set1_epi16((short)tail_unit);
    unsigned long long at = 0ull;
    while ((at + span + 16ull) <= length)
    {
        __m256i head = _mm256_loadu_si256((const __m256i *)(const void *)(text + at));
        __m256i tail = _mm256_loadu_si256((const __m256i *)(const void *)(text + at + span));
        if (folded)
        {
            head = cli_fold16(head);
            tail = cli_fold16(tail);
        }
        const __m256i both = _mm256_and_si256(_mm256_cmpeq_epi16(head, first), _mm256_cmpeq_epi16(tail, last));
        // the movemask of a block is its 32 bits, one of each lane's two kept
        unsigned int mask = ((unsigned int)_mm256_movemask_epi8(both)) & 0x55555555u;
        while (mask != 0u)
        {
            const unsigned long long place = at + (cli_lowest(mask) / 2u);
            if (cli_vector_stands_at(text, sought, sought_length, place, folded))
            {
                return place;
            }
            mask &= mask - 1u;
        }
        at += 16ull;
    }
    const unsigned long long found = cli_vector_find_sse2(text + at, length - at, sought, sought_length, folded);
    return (found == CLI_VECTOR_NONE) ? found : (at + found);
}

#endif
