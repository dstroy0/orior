// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/*
 *
 * Every use falls under AGPL-3.0-or-later unless you have explicit permission. That permission is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file scan_neon.c
 * @brief The steering scan under NEON, sixteen alignments per compare.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * THE SAME QUESTION THAT AVX2 ANSWERS, ONE ARM OVER. At one fixed needle offset, does
 * `corpus[at + offset]` equal `needle[offset]` for consecutive `at`? The needle byte broadcasts
 * once, and the corpus side is a window that moves at unit stride. The survivor mask enters as the
 * alignments that are still truthy. A NEON register is 128 bits, and answers sixteen of those in
 * one compare.
 *
 * WHAT THIS ARM MUST NOT DO. It returns the count of the portable arm, or it has a defect. The
 * value is an integer count of alignments, and the two arms agree exactly or one is wrong. That
 * is the contract that OriorSteerEngine carries, the same one that the exact arms carry in
 * includes/exact_types/integers/arm.h.
 *
 * @note Written for the Raspberry Pi 5, a Cortex-A76 at aarch64. This arm runs, and its name has
 *       no unrun tag. NEON is mandatory in the base aarch64 architecture. A build that reached
 *       here can always run it, and there is nothing to detect.
 * @note There is no movemask on NEON. A byte compare sets a lane to all ones where it agrees, and
 *       the alive lanes enter the same way. The count comes from a shift of each survivor lane down
 *       to one, and an add of the sixteen lanes across the register. The across-vector add is an
 *       aarch64 form, the architecture that this arm targets.
 * @note The tail below sixteen ends scalar, in the same code that the portable arm runs.
 */

#include "orior.h"

#include <arm_neon.h>

/** @brief Alignments one NEON register answers at once. */
#define ORIOR_STEER_LANES 16u

size_t orior_steer_truthy_after_neon(const uint8_t *corpus, size_t alignments, const uint8_t *alive, uint8_t wanted,
                                      size_t offset)
{
    /* Counted before the argument check. A caller that passes nothing still records that this arm
     * was the one requested. The claim that the counters carry is which arm RAN, and not what it
     * returned. */
    orior_steer_scan_calls += 1u;
    orior_steer_wide_calls += 1u;

    if ((corpus == NULL) || (alive == NULL) || (alignments == 0u))
    {
        return 0u;
    }

    const uint8x16_t broadcast = vdupq_n_u8(wanted);
    const uint8x16_t zero = vdupq_n_u8(0u);
    size_t located = 0u;
    size_t at = 0u;

    for (; (at + ORIOR_STEER_LANES) <= alignments; at += ORIOR_STEER_LANES)
    {
        const uint8x16_t window = vld1q_u8(corpus + at + offset);
        const uint8x16_t located_bytes = vld1q_u8(alive + at);

        const uint8x16_t agrees = vceqq_u8(window, broadcast);

        /* An alive flag is zero or not zero. vceqq against zero marks the refuted lanes, and the
         * complement marks the alive ones. A complement, and not a compare against one, keeps this
         * correct if a caller ever stores a flag other than one. */
        const uint8x16_t alive_mask = vmvnq_u8(vceqq_u8(located_bytes, zero));

        /* A survivor lane is all ones. A shift of it down to one, and an add of the sixteen lanes,
         * give the count of the block. That count cannot exceed sixteen, thus it does not overflow
         * the byte that the across-vector add returns. */
        const uint8x16_t hits = vshrq_n_u8(vandq_u8(agrees, alive_mask), 7);
        located += (size_t)vaddvq_u8(hits);
    }

    for (; at < alignments; at += 1u)
    {
        if (alive[at] == 0u)
        {
            continue;
        }
        if (corpus[at + offset] == wanted)
        {
            located += 1u;
        }
    }
    return located;
}

const OriorSteerEngine *orior_steer_neon_engine(void)
{
    static const OriorSteerEngine engine = {"neon", orior_steer_truthy_after_neon};

    /* NEON is part of the base aarch64 architecture. A build that got here can always run it. */
    return &engine;
}
