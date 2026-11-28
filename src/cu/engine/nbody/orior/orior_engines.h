// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_engines.h: the steering engines of each arm (orior.h includes the parts in order)
#ifndef ORIOR_ENGINES_H
#define ORIOR_ENGINES_H

#include "orior_descent.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* ---- the scan: the engine interface, the shared counters, the dispatch, one arm per set ---- */

    /**
     * @brief One implementation of the steering scan.
     *
     * @note `count` takes the corpus, the alignment count, the survivor flags, the needle byte under
     *       test and the offset that it sits at. It returns how many truthy alignments agree. A planner
     *       queries a scan engine for this operation alone.
     */
    typedef struct
    {
        const char *name; /**< What to print in a row. Never null. */
        size_t (*count)(const uint8_t *corpus, size_t alignments, const uint8_t *alive, uint8_t wanted, size_t offset);
    } OriorSteerEngine;

    /**
     * @brief Scans that any engine served since the last reset.
     *
     * A CORRECTNESS SUITE CANNOT FIND AN UNUSED IMPLEMENTATION. An engine that the build compiles and
     * the suite grades, but that nothing calls, makes no wrong answer. Every count stays the same, and
     * every test still passes. An engine graded against portable and benched fast can sit beside a
     * planner that runs its own scalar loop. Nothing in the suite reports it.
     *
     * These two counters make the wires testable. The claim is not that the engines agree, which the
     * differential already covers. The claim is that the engine that the machine carries truly RAN. A
     * test reads them after a planner run. It requires the wide count to be not zero wherever a wide
     * engine reports itself present.
     */
    extern uint64_t orior_steer_scan_calls;

    /** @brief Scans served by a vectorized engine since the last reset. */
    extern uint64_t orior_steer_wide_calls;

    /** @brief Sets both scan counters to zero. */
    void orior_steer_scan_counters_reset(void);

    /**
     * @brief The widest engine this machine carries, the one the planner calls.
     *
     * @return The engine. Never null, since the portable one is always present.
     * @note The call resolves it every time. A caller in a hot path caches the result, and does not
     *       query again, because the processor query costs more than a scan does.
     */
    const OriorSteerEngine *orior_steer_best_engine(void);

    /**
     * @brief The portable C11 engine, the reference that the suite grades every other engine against.
     *
     * @return A pointer to the engine. Never null, because it runs wherever a C11 compiler built it.
     */
    const OriorSteerEngine *orior_steer_portable_engine(void);

    /**
     * @brief The scan in portable C11.
     *
     * @param[in] corpus     Bytes under examination [BORROWS].
     * @param[in] alignments How many alignments the object has.
     * @param[in] alive      One flag for each alignment, not zero where it is truthy [BORROWS].
     * @param[in] wanted     The needle byte under test.
     * @param[in] offset     The needle offset that the probe sits at.
     * @return               How many truthy alignments agree.
     */
    size_t orior_steer_truthy_after_portable(const uint8_t *corpus, size_t alignments, const uint8_t *alive,
                                              uint8_t wanted, size_t offset);

#if defined(ORIOR_STEER_HAVE_AVX2) && ORIOR_STEER_HAVE_AVX2

    /**
     * @brief The AVX2 engine, which answers thirty-two alignments for each compare.
     *
     * @return A pointer to the engine, or NULL where this processor does not carry AVX2.
     * @note Queries the processor, and does not trust the build.
     */
    const OriorSteerEngine *orior_steer_avx2_engine(void);

    /** @brief The scan under AVX2. Same contract as the portable one, same count. */
    size_t orior_steer_truthy_after_avx2(const uint8_t *corpus, size_t alignments, const uint8_t *alive,
                                          uint8_t wanted, size_t offset);

#endif

#if defined(ORIOR_STEER_HAVE_AVX512) && ORIOR_STEER_HAVE_AVX512

    /**
     * @brief The AVX-512 engine, which answers sixty-four alignments for each compare.
     *
     * @return A pointer to the engine, or NULL where this processor does not carry AVX-512.
     * @note Queries the processor, and does not trust the build. No machine here runs it. The name
     *       that it carries reads avx512-unrun.
     */
    const OriorSteerEngine *orior_steer_avx512_engine(void);

    /** @brief The scan under AVX-512. Same contract as the portable one, same count. */
    size_t orior_steer_truthy_after_avx512(const uint8_t *corpus, size_t alignments, const uint8_t *alive,
                                            uint8_t wanted, size_t offset);

#endif

#if defined(ORIOR_STEER_HAVE_NEON) && ORIOR_STEER_HAVE_NEON

    /**
     * @brief The NEON engine, which answers sixteen alignments for each compare.
     *
     * @return A pointer to the engine. Never null on a build that reached it, because NEON is part of
     *         the base aarch64 architecture.
     */
    const OriorSteerEngine *orior_steer_neon_engine(void);

    /** @brief The scan under NEON. Same contract as the portable one, same count. */
    size_t orior_steer_truthy_after_neon(const uint8_t *corpus, size_t alignments, const uint8_t *alive,
                                          uint8_t wanted, size_t offset);

#endif

#if defined(ORIOR_STEER_HAVE_SVE) && ORIOR_STEER_HAVE_SVE

    /**
     * @brief The SVE engine, which answers one vector of alignments for each compare.
     *
     * @return A pointer to the engine, or NULL where the kernel does not report SVE. No machine here
     *         runs it. The name that it carries reads sve-unrun.
     * @note The test reads the kernel capability word, because ARM has no cpuid.
     */
    const OriorSteerEngine *orior_steer_sve_engine(void);

    /** @brief The scan under SVE. Same contract as the portable one, same count. */
    size_t orior_steer_truthy_after_sve(const uint8_t *corpus, size_t alignments, const uint8_t *alive, uint8_t wanted,
                                         size_t offset);

#endif

#if defined(ORIOR_STEER_HAVE_CUDA) && ORIOR_STEER_HAVE_CUDA

    /**
     * @brief Whether a usable CUDA device is present.
     *
     * @return 1 where at least one device answered, 0 where none did.
     * @note The call queries at run time. A binary built with CUDA still runs on a machine with no
     *       device. There, the arm reports itself absent, and does not fail inside a launch.
     */
    int orior_steer_cuda_available(void);

    /**
     * @brief The name and compute capability of the device this arm would use.
     *
     * @param[out] text Where the call writes the description [BORROWS].
     * @param[in]  capacity How many bytes `text` holds.
     * @return          1 where the call wrote a description, 0 where no device answered.
     */
    int orior_steer_cuda_describe(char *text, size_t capacity);

    /**
     * @brief The CUDA engine, one alignment for each thread, over the whole object.
     *
     * @return A pointer to the engine, or NULL where no device answered.
     * @note Not in orior_steer_best_engine. A descent calls the scan once for each candidate, and the
     *       survivor vector changes at each level. A host to device copy at each call would cost more
     *       than the scan saves on all but the largest objects. The suite grades the arm against
     *       portable, and the GPU build times it. A caller that already put the object on the device
     *       calls it directly.
     */
    const OriorSteerEngine *orior_steer_cuda_engine(void);

    /**
     * @brief The scan on a CUDA device. Same contract as the portable one, same count.
     *
     * @note Falls back to a host count where the device gives an error on the work. A driver that
     *       compares arms reads a count, and never a sentinel that it would read as a disagreement.
     */
    size_t orior_steer_truthy_after_cuda(const uint8_t *corpus, size_t alignments, const uint8_t *alive,
                                          uint8_t wanted, size_t offset);

#endif

#ifdef __cplusplus
}
#endif

#endif
