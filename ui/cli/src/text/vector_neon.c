// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vector_neon.c: the loops in NEON, eight units or two numbers a block
#include "vector.h"

#if ENGINE_TARGET_AARCH64

#include <arm_neon.h>

// the place of the lowest bit set of a mask that is not 0
static unsigned int cli_lowest64(unsigned long long mask)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0ul;
    _BitScanForward64(&index, mask);
    // the bit index of a non-zero 64 bit mask is 0 to 63
    return (unsigned int)index;
#else
    // the trailing count of a non-zero 64 bit mask is 0 to 63
    return (unsigned int)__builtin_ctzll(mask);
#endif
}

// Which of eight lanes are set, each a byte of 0xFF or 0 in the answer's eight bytes
static unsigned long long cli_lanes(uint16x8_t mask)
{
    return vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(mask, 4)), 0);
}

static uint16x8_t cli_fold8(uint16x8_t block)
{
    const uint16x8_t upper = vcleq_u16(vsubq_u16(block, vdupq_n_u16(0x41u)), vdupq_n_u16(25u));
    uint16x8_t lowered = vorrq_u16(block, vandq_u16(upper, vdupq_n_u16(0x20u)));
    lowered = vbslq_u16(vceqq_u16(block, vdupq_n_u16(CLI_LONG_S)), vdupq_n_u16('s'), lowered);
    return vbslq_u16(vceqq_u16(block, vdupq_n_u16(CLI_KELVIN)), vdupq_n_u16('k'), lowered);
}

unsigned long long cli_vector_find_unit_neon(const unsigned short *text, unsigned long long length, unsigned short unit)
{
    const uint16x8_t wanted = vdupq_n_u16(unit);
    unsigned long long at = 0ull;
    while ((at + 8ull) <= length)
    {
        const unsigned long long found = cli_lanes(vceqq_u16(vld1q_u16(text + at), wanted));
        if (found != 0ull)
        {
            return at + (cli_lowest64(found) / 8u);
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

unsigned long long cli_vector_common_neon(const unsigned long long *a, const unsigned long long *b,
                                          unsigned long long length)
{
    unsigned long long at = 0ull;
    while ((at + 2ull) <= length)
    {
        // the numbers are read as the 64 bits they are
        const uint64x2_t same = vceqq_u64(vld1q_u64((const uint64_t *)a + at), vld1q_u64((const uint64_t *)b + at));
        if (vgetq_lane_u64(same, 0) != 0xffffffffffffffffull)
        {
            return at;
        }
        if (vgetq_lane_u64(same, 1) != 0xffffffffffffffffull)
        {
            return at + 1ull;
        }
        at += 2ull;
    }
    while ((at < length) && (a[at] == b[at]))
    {
        at += 1ull;
    }
    return at;
}

int cli_vector_same_neon(const unsigned short *a, const unsigned short *b, unsigned long long length)
{
    unsigned long long at = 0ull;
    while ((at + 8ull) <= length)
    {
        if (vminvq_u16(vceqq_u16(vld1q_u16(a + at), vld1q_u16(b + at))) != 0xffffu)
        {
            return 0;
        }
        at += 8ull;
    }
    return memor.cmp(&(MemoriaCfg){.src = a + at, .other = b + at, .bytes = (length - at) * sizeof(a[0])}) == 0;
}

unsigned long long cli_vector_find_neon(const unsigned short *text, unsigned long long length,
                                        const unsigned short *sought, unsigned long long sought_length, int folded)
{
    const unsigned long long span = sought_length - 1ull;
    const uint16x8_t first = vdupq_n_u16(folded ? cli_vector_fold(sought[0]) : sought[0]);
    const uint16x8_t last = vdupq_n_u16(folded ? cli_vector_fold(sought[span]) : sought[span]);
    unsigned long long at = 0ull;
    while ((at + span + 8ull) <= length)
    {
        uint16x8_t head = vld1q_u16(text + at);
        uint16x8_t tail = vld1q_u16(text + at + span);
        if (folded)
        {
            head = cli_fold8(head);
            tail = cli_fold8(tail);
        }
        unsigned long long found = cli_lanes(vandq_u16(vceqq_u16(head, first), vceqq_u16(tail, last)));
        while (found != 0ull)
        {
            const unsigned int lane = cli_lowest64(found) / 8u;
            const unsigned long long place = at + lane;
            if (cli_vector_stands_at(text, sought, sought_length, place, folded))
            {
                return place;
            }
            found &= ~(0xffull << (lane * 8u));
        }
        at += 8ull;
    }
    return cli_vector_plain_find(text, length, sought, sought_length, folded, at);
}

#endif
