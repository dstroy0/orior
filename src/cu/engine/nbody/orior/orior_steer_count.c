// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_steer_count.c: sweeping the probes and counting
#include "memoria_operor/memoria_operor.h"
#include "orior_internal.h"

/**
 * @brief The sweep itself, which the public entry names.
 *
 * @param[out] probes           Chosen probes, in evaluation order [BORROWS].
 * @param[in]  wanted           How many to place. At max ORIOR_STEER_ANCHORS.
 * @param[in]  corpus           Bytes the search will run over [BORROWS].
 * @param[in]  corpus_len       How many.
 * @param[in]  needle           Bytes to find [BORROWS].
 * @param[in]  needle_len       How many.
 * @param[in]  max_length       Longest eye to consider.
 * @param[out] survivors        Which alignments the probes left truthy, one byte each [BORROWS].
 * @param[in]  survivors_length How many. Must reach the alignment count.
 * @param[in]  sample_stride    Plan on every Nth alignment. The call reads 0 as 1.
 * @return                      The probes that the call placed.
 * @note Static and positional, which is where a long parameter list can live. The public surface
 *       takes one pointer to a const argument structure. This is the backend that it names, and
 *       every check that the contract states occurs here and not in the entry.
 */
static size_t steer_sweep_probes(OriorProbe *probes, size_t wanted, const uint8_t *corpus, size_t corpus_len,
                                 const uint8_t *needle, size_t needle_len, size_t max_length, uint8_t *survivors,
                                 size_t survivors_length, size_t sample_stride)
{
    if ((probes == NULL) || (corpus == NULL) || (needle == NULL) || (survivors == NULL) || (wanted == 0u) ||
        (wanted > ORIOR_STEER_ANCHORS) || (needle_len == 0u) || (needle_len > corpus_len) || (max_length == 0u))
    {
        return 0u;
    }

    const size_t alignments = (corpus_len - needle_len) + 1u;
    if (survivors_length < alignments)
    {
        return 0u;
    }

    const size_t stride = (sample_stride == 0u) ? 1u : sample_stride;
    for (size_t at = 0u; at < alignments; at += 1u)
    {
        survivors[at] = 1u;
    }

    size_t placed = 0u;

    // THREE BOUNDED LOOPS INSIDE A BOUNDED DESCENT. Origins run to needle_len, steps run to
    // needle_len, lengths run to max_length, and the descent runs to `wanted`. Every bound is an
    // argument or a compile time constant, and the call reads none of them from the corpus. This
    // search has its full extent before the call examines the first byte.
    while (placed < wanted)
    {
        OriorProbe best = {0u, 1u, 1u};
        size_t best_located = 0u;
        int found = 0;

        for (size_t origin = 0u; origin < needle_len; origin += 1u)
        {
            for (size_t length = 1u; length <= max_length; length += 1u)
            {
                const size_t step_limit = (length == 1u) ? 2u : (needle_len + 1u);
                for (size_t step = 1u; step < step_limit; step += 1u)
                {
                    const OriorProbe candidate = {origin, step, length};
                    if (orior_steer_probe_fits(&candidate, needle_len) == 0)
                    {
                        continue;
                    }

                    const size_t located =
                        steer_truthy_after_probe(corpus, corpus_len, needle, needle_len, survivors, &candidate, stride);
                    if ((found == 0) || (located < best_located))
                    {
                        best_located = located;
                        best = candidate;
                        found = 1;
                    }
                }
            }
        }

        if (found == 0)
        {
            break;
        }
        if (best_located >= steer_truthy_total(survivors, alignments, stride))
        {
            // Destroyed for the same reason a coarm is. It prunes nothing and would only read.
            break;
        }

        probes[placed] = best;
        placed += 1u;
        steer_make_falsy_probe(corpus, corpus_len, needle, needle_len, survivors, &best);
    }
    return placed;
}

size_t orior_steer_sweep_probes(const OriorSteerSweep *args)
{
    if (args == NULL)
    {
        return 0u;
    }

    // The entry tests only what the backend cannot: whether it got arguments at all. The call
    // steer_sweep_probes checks all else that the contract states, against the values that it will
    // use. No check exists in two places, where the two could drift apart.
    return steer_sweep_probes(args->probes, args->count, args->corpus, args->corpus_len, args->needle, args->needle_len,
                              args->max_length, args->survivors, args->survivors_length, args->sample_stride);
}

uint64_t orior_steer_probes = 0u;

void orior_steer_probes_reset(void)
{
    orior_steer_probes = 0u;
}

size_t orior_steer_count_with_probes(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                                     const OriorProbe *probes, size_t count)
{
    if ((corpus == NULL) || (needle == NULL) || (needle_len > corpus_len))
    {
        return 0u;
    }
    // An empty needle occurs at every alignment. The engines orior_naive and orior_steer_count both
    // report `corpus_len + 1` for it, and the reference fixes that answer. This returns the same
    // before the loop, and does not read `needle[offset]` from a needle with no positions. A return
    // of 0 here would disagree with the reference and with the two count entries beside it.
    if (needle_len == 0u)
    {
        return corpus_len + 1u;
    }
    if ((probes == NULL) && (count != 0u))
    {
        return 0u;
    }

    // Every probe reads within the needle, or the whole call gives an error. A probe whose origin
    // plus its stepped reach lands at or past needle_len would read `needle[offset]`, and
    // `corpus[at + offset]`, out of bounds. The test orior_steer_probe_fits is the one that the
    // sweep applies before it emits a probe. The call checks it here once, before the alignment
    // loop, because a caller can give this public entry a probe that the sweep never made. A probe
    // that does not fit gets 0, the error that a null probe array with a count that is not zero
    // gets above.
    for (size_t index = 0u; index < count; index += 1u)
    {
        if (orior_steer_probe_fits(&probes[index], needle_len) == 0)
        {
            return 0u;
        }
    }

    size_t found = 0u;
    for (size_t at = 0u; (at + needle_len) <= corpus_len; at += 1u)
    {
        size_t index = 0u;
        while (index < count)
        {
            int agrees = 1;
            for (size_t step = 0u; step < probes[index].length; step += 1u)
            {
                const size_t offset = probes[index].origin + (step * probes[index].step);
                orior_steer_probes += 1u;
                if (corpus[at + offset] != needle[offset])
                {
                    agrees = 0;
                    break;
                }
            }
            if (agrees == 0)
            {
                break;
            }
            index += 1u;
        }
        if (index == count)
        {
            if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = corpus + at, .other = needle, .bytes = needle_len) == 0)
            {
                found += 1u;
            }
        }
    }
    return found;
}

size_t orior_steer_count(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                         int steered)
{
    if ((corpus == NULL) || (needle == NULL) || (needle_len > corpus_len))
    {
        return 0u;
    }

    size_t found = 0u;

    // An empty needle has no symbol to probe. The reference engine reports it at every alignment,
    // and this one answers the same. It does not invent a different answer to one question.
    if (needle_len == 0u)
    {
        return corpus_len + 1u;
    }

    size_t offsets[ORIOR_STEER_ANCHORS];
    // the placement that the search uses. The steered route probes where the engines that it
    // compares with do, and only the order of evaluation belongs to this file
    choose_offsets(offsets, ORIOR_STEER_ANCHORS, needle_len);

    if (steered != 0)
    {
        // THE ENGINE TURNED ONTO ITSELF. One pass over the corpus that it will search makes the
        // census. The census decides the order that the call then probes this same corpus in.
        OriorFieldCensus census;
        orior_field_census(corpus, corpus_len, &census);
        orior_steer_probe_order(offsets, ORIOR_STEER_ANCHORS, &census, needle, needle_len);
    }

    for (size_t at = 0u; (at + needle_len) <= corpus_len; at += 1u)
    {
        size_t index = 0u;
        while (index < ORIOR_STEER_ANCHORS)
        {
            orior_steer_probes += 1u;
            if (corpus[at + offsets[index]] != needle[offsets[index]])
            {
                break;
            }
            index += 1u;
        }
        if (index == ORIOR_STEER_ANCHORS)
        {
            if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = corpus + at, .other = needle, .bytes = needle_len) == 0)
            {
                found += 1u;
            }
        }
    }
    return found;
}

/* ---- the scan: shared counters and the arm dispatch; each arm is its own scan_<set>.c ---- */

uint64_t orior_steer_scan_calls = 0u;
uint64_t orior_steer_wide_calls = 0u;

void orior_steer_scan_counters_reset(void)
{
    orior_steer_scan_calls = 0u;
    orior_steer_wide_calls = 0u;
}

const OriorSteerEngine *orior_steer_best_engine(void)
{
    // Widest first, and each arm queries the processor before the call takes it. Build macros that
    // exclude each other gate the x86 arms and the ARM arms. At max the ladder of one architecture
    // compiles in, and the rest fold away. A present arm that returns NULL means that the build
    // carried it but the part that runs does not. The call then tries the next arm down.
#if defined(ORIOR_STEER_HAVE_AVX512) && ORIOR_STEER_HAVE_AVX512
    {
        const OriorSteerEngine *wide = orior_steer_avx512_engine();
        if (wide != NULL)
        {
            return wide;
        }
    }
#endif
#if defined(ORIOR_STEER_HAVE_AVX2) && ORIOR_STEER_HAVE_AVX2
    {
        const OriorSteerEngine *wide = orior_steer_avx2_engine();
        if (wide != NULL)
        {
            return wide;
        }
    }
#endif
#if defined(ORIOR_STEER_HAVE_SVE) && ORIOR_STEER_HAVE_SVE
    {
        const OriorSteerEngine *wide = orior_steer_sve_engine();
        if (wide != NULL)
        {
            return wide;
        }
    }
#endif
#if defined(ORIOR_STEER_HAVE_NEON) && ORIOR_STEER_HAVE_NEON
    {
        const OriorSteerEngine *wide = orior_steer_neon_engine();
        if (wide != NULL)
        {
            return wide;
        }
    }
#endif
    return orior_steer_portable_engine();
}
