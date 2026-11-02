// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vector_neon.c: the loops in NEON, eight units or two numbers a block
#include "vector.h"

#if ENGINE_TARGET_AARCH64

#include <arm_neon.h>

typedef struct
{
#if (ENGINE_TARGET_AARCH64)
    unsigned long long mask;
    uint16x8_t mask_uint16x8_t;
    uint16x8_t block;
#endif
} VectorNeonCtx;

// the place of the lowest bit set of a mask that is not 0
EMBED_INLINE unsigned int cli_lowest64(const VectorNeonCtx *args)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index = 0ul;
    _BitScanForward64(&index, args->mask);
    // the bit index of a non-zero 64 bit mask is 0 to 63
    return (unsigned int)index;
#else
    // the trailing count of a non-zero 64 bit mask is 0 to 63
    return (unsigned int)__builtin_ctzll(args->mask);
#endif
}

// Which of eight lanes are set, each a byte of 0xFF or 0 in the answer's eight bytes
EMBED_INLINE unsigned long long cli_lanes(const VectorNeonCtx *args)
{
    return vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(args->mask_uint16x8_t, 4)), 0);
}

EMBED_INLINE uint16x8_t cli_fold8(const VectorNeonCtx *args)
{
    const uint16x8_t upper = vcleq_u16(vsubq_u16(args->block, vdupq_n_u16(0x41u)), vdupq_n_u16(25u));
    uint16x8_t lowered = vorrq_u16(args->block, vandq_u16(upper, vdupq_n_u16(0x20u)));
    lowered = vbslq_u16(vceqq_u16(args->block, vdupq_n_u16(CLI_LONG_S)), vdupq_n_u16('s'), lowered);
    return vbslq_u16(vceqq_u16(args->block, vdupq_n_u16(CLI_KELVIN)), vdupq_n_u16('k'), lowered);
}

unsigned long long cli_vector_find_unit_neon(const CliVectorUnitCfg *args)
{
    const uint16x8_t wanted = vdupq_n_u16(args->unit);
    unsigned long long at = 0ull;
    while ((at + 8ull) <= args->length)
    {
        const unsigned long long found =
            EMBED_CALL(cli_lanes, VectorNeonCtx, .mask_uint16x8_t = vceqq_u16(vld1q_u16(args->text + at), wanted));
        if (found != 0ull)
        {
            return at + (EMBED_CALL(cli_lowest64, VectorNeonCtx, .mask = found) / 8u);
        }
        at += 8ull;
    }
    for (; at < args->length; at += 1ull)
    {
        if (args->text[at] == args->unit)
        {
            return at;
        }
    }
    return CLI_VECTOR_NONE;
}

unsigned long long cli_vector_common_neon(const CliVectorLengthCfg *args)
{
    unsigned long long at = 0ull;
    while ((at + 2ull) <= args->length)
    {
        // the numbers are read as the 64 bits they are
        const uint64x2_t same =
            vceqq_u64(vld1q_u64((const uint64_t *)args->a_u64s + at), vld1q_u64((const uint64_t *)args->b_u64s + at));
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
    while ((at < args->length) && (args->a_u64s[at] == args->b_u64s[at]))
    {
        at += 1ull;
    }
    return at;
}

int cli_vector_same_neon(const CliVectorLengthACfg *args)
{
    unsigned long long at = 0ull;
    while ((at + 8ull) <= args->length)
    {
        if (vminvq_u16(vceqq_u16(vld1q_u16(args->a + at), vld1q_u16(args->b + at))) != 0xffffu)
        {
            return 0;
        }
        at += 8ull;
    }
    return memor.cmp(&(MemoriaCfg){
               .src = args->a + at, .other = args->b + at, .bytes = (args->length - at) * sizeof(args->a[0])}) == 0;
}

unsigned long long cli_vector_find_neon(const CliVectorTextLengthCfg *args)
{
    const unsigned long long span = args->sought_length - 1ull;
    const uint16x8_t first = vdupq_n_u16(
        args->folded ? EMBED_CALL(cli_vector_unit.fold, CliVectorUnitCfg, .unit = args->sought[0]) : args->sought[0]);
    const uint16x8_t last =
        vdupq_n_u16(args->folded ? EMBED_CALL(cli_vector_unit.fold, CliVectorUnitCfg, .unit = args->sought[span])
                                 : args->sought[span]);
    unsigned long long at = 0ull;
    while ((at + span + 8ull) <= args->length)
    {
        uint16x8_t head = vld1q_u16(args->text + at);
        uint16x8_t tail = vld1q_u16(args->text + at + span);
        if (args->folded)
        {
            head = EMBED_CALL(cli_fold8, VectorNeonCtx, .block = head);
            tail = EMBED_CALL(cli_fold8, VectorNeonCtx, .block = tail);
        }
        unsigned long long found = EMBED_CALL(
            cli_lanes, VectorNeonCtx, .mask_uint16x8_t = vandq_u16(vceqq_u16(head, first), vceqq_u16(tail, last)));
        while (found != 0ull)
        {
            const unsigned int lane = EMBED_CALL(cli_lowest64, VectorNeonCtx, .mask = found) / 8u;
            const unsigned long long place = at + lane;
            if (EMBED_CALL(cli_vector_text_sought.stands_at, CliVectorTextSoughtCfg, .text = args->text,
                           .sought = args->sought, .sought_length = args->sought_length, .at = place,
                           .folded = args->folded))
            {
                return place;
            }
            found &= ~(0xffull << (lane * 8u));
        }
        at += 8ull;
    }
    return EMBED_CALL(cli_vector_text_length.plain_find, CliVectorTextLengthCfg, .text = args->text,
                      .length = args->length, .sought = args->sought, .sought_length = args->sought_length,
                      .folded = args->folded, .start = at);
}

#endif
