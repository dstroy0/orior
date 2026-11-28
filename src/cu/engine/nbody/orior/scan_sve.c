// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/*
 *
 * Every use falls under AGPL-3.0-or-later unless you have explicit permission. That permission is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file scan_sve.c
 * @brief The steering scan under SVE, at whatever vector length the part has.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * THE SAME QUESTION THAT AVX2 ANSWERS, WITH NO FIXED WIDTH. At one fixed needle offset, does
 * `corpus[at + offset]` equal `needle[offset]` for consecutive `at`? The needle byte broadcasts
 * once, and the corpus side is a window that moves at unit stride. The survivor mask enters as the
 * alignments that are still truthy. SVE answers one vector of those for each compare, and the
 * vector is as wide as the part.
 *
 * WHAT THIS ARM MUST NOT DO. It returns the count of the portable arm, or it has a defect. The
 * value is an integer count of alignments, and the two arms agree exactly or one is wrong. That
 * is the contract that OriorSteerEngine carries, the same one that the exact arms carry in
 * includes/exact_types/integers/arm.h.
 *
 * @note No part in this project has SVE. The Raspberry Pi 5 is a Cortex-A76, NEON only. This arm
 *       never ran. The build compiles it for armv8.2-a+sve, and
 *       utils/maint/engine/verify_arm_asm.sh reads its emitted instructions. That script confirms
 *       the predicated forms, which rules out a scalar fallback. It states nothing about behavior,
 *       thus the name reads sve-unrun.
 * @note SVE has no fixed vector length. svcntb() answers how many byte lanes this part carries, and
 *       the loop goes around a predicate. No lane count appears here, and the tail needs no
 *       separate scalar loop. The call svwhilelt_b8 covers only the lanes that exist, and the count
 *       comes from svcntp_b8 over the survivor predicate.
 * @note The test reads the hardware capability word that Linux exposes, and never runs an SVE
 *       instruction to see whether it faults. There is no cpuid on ARM: the features of a part come
 *       from the kernel. On any system but Linux, this arm reports itself absent, and does not
 *       guess.
 */

#include "orior.h"

#include <arm_sve.h>

#if defined(__linux__)
#include <sys/auxv.h>
/** @brief Set where this translation unit can query the kernel about SVE. */
#define ORIOR_STEER_SVE_CAN_DETECT 1
#ifndef HWCAP_SVE
/** @brief The SVE bit in AT_HWCAP, defined here where the running headers predate it. */
#define HWCAP_SVE (1 << 22)
#endif
#else
#define ORIOR_STEER_SVE_CAN_DETECT 0
#endif

/**
 * @brief Whether this part carries SVE.
 *
 * @return 1 where the kernel reports SVE, 0 where it does not, or the call cannot query it.
 */
static int steer_sve_present(void)
{
#if ORIOR_STEER_SVE_CAN_DETECT
    return ((getauxval(AT_HWCAP) & (unsigned long)HWCAP_SVE) != 0UL) ? 1 : 0;
#else
    return 0;
#endif
}

size_t orior_steer_truthy_after_sve(const uint8_t *corpus, size_t alignments, const uint8_t *alive, uint8_t wanted,
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

    const uint64_t total = (uint64_t)alignments;
    uint64_t located = 0u;

    for (uint64_t at = 0u; at < total; at += svcntb())
    {
        /* The predicate covers only the lanes that exist. The tail needs no separate loop and the
         * predicated load reads no byte past the alignment count. */
        const svbool_t live = svwhilelt_b8(at, total);
        const svuint8_t window = svld1_u8(live, corpus + at + offset);
        const svuint8_t located_bytes = svld1_u8(live, alive + at);

        const svbool_t agrees = svcmpeq_n_u8(live, window, wanted);

        /* An alive flag is zero or not zero. The alive lanes are the ones not equal to zero. A test
         * in this way, and not against one, keeps it correct if a caller ever stores a flag other
         * than one. */
        const svbool_t alive_mask = svcmpne_n_u8(live, located_bytes, 0u);

        located += (uint64_t)svcntp_b8(live, svand_b_z(live, agrees, alive_mask));
    }

    return (size_t)located;
}

const OriorSteerEngine *orior_steer_sve_engine(void)
{
    static const OriorSteerEngine engine = {"sve-unrun", orior_steer_truthy_after_sve};

    if (steer_sve_present() == 0)
    {
        return NULL;
    }
    return &engine;
}
