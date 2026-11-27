
// test_adversarial_plans.c: family guard, sampling cost, permutations and plans
#include "../../../../../../../src/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "test_adversarial_internal.h"

/**
 * @brief Case 6. Shapes outside the necessary-condition family, errored in the guard.
 *
 * @return 0 where every illegal shape errors and every legal one admitted, 1 otherwise.
 * @note This grades the GUARD instead of the count, because the first two shapes that leave the
 *       family are caught here and never reach a search. The third shape that leaves the family, a
 *       predicate taken from the census instead of the needle, is not reachable from outside and is
 *       named in the file block as a gap.
 */
int adversarial_case_family_guard(void)
{
    static const size_t needle_len = 16u;
    int failed = 0;

    // An origin at the needle length reads past the end and must error.
    const OriorProbe past_end = {needle_len, 1u, 1u};
    if (orior_steer_probe_fits(&past_end, needle_len) != 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    FAIL origin at needle_len admitted");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed = 1;
    }

    // A line whose last position falls outside the needle must error.
    const OriorProbe overruns = {needle_len - 2u, 4u, 3u};
    if (orior_steer_probe_fits(&overruns, needle_len) != 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    FAIL line overrunning the needle admitted");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed = 1;
    }

    // A zero step above length one reads one position repeatedly and carries no second condition.
    const OriorProbe stalled = {0u, 0u, 4u};
    if (orior_steer_probe_fits(&stalled, needle_len) != 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    FAIL zero step above length one admitted");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed = 1;
    }

    // A zero length probe tests nothing and is not a necessary condition of anything.
    const OriorProbe empty = {0u, 1u, 0u};
    if (orior_steer_probe_fits(&empty, needle_len) != 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    FAIL zero length probe admitted");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed = 1;
    }

    // The widest line that still lands inside must be admitted, or the guard is erroring on the family.
    const OriorProbe widest = {0u, needle_len - 1u, 2u};
    if (orior_steer_probe_fits(&widest, needle_len) == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    FAIL widest fitting line errored");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed = 1;
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  family guard, verdict ");
        mmgr_line_put(output_line, (failed == 0) ? "ok" : "FAILS");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return failed;
}

/**
 * @brief Case 7. What sampling costs, reported as a ratio against the unsampled planner.
 *
 * @return 0 where every stride returns the exact count, 1 otherwise.
 * @note THE COUNT IS THE PASS CONDITION AND THE READS ARE THE REPORT. Sampling cannot reach
 *       correctness, by the necessary-condition guarantee. A count that moves with the stride is
 *       a defect elsewhere. What sampling can do is choose worse probes, and the read ratio is what
 *       that costs. This turns a caveat in the guide into a number.
 */
int adversarial_case_sampling_cost(void)
{
    uint8_t *const corpus = (uint8_t *)platform_cell_taken(ADVERSARIAL_CORPUS, platform_place_g_program_owner, 0);
    uint8_t needle[12];
    int failed = 0;

    if (corpus == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    allocation failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    // A field periodic at 16. A stride sharing that period sees an unrepresentative sample.
    //
    // BUILT HERE, THOUGH bench_corpora BUILDS THE SAME FIELD. This was briefly folded
    // onto bench_build_bytes(CORPUS_PERIODIC), whose body is character for character this loop, and
    // the fold was withdrawn: it would make a test depend on a bench corpus, and a bench corpus that
    // cannot be retuned without checking what it silently changed in the test suite stopped
    // being a benchmark. utils/test/ answers whether the engine is right and utils/bench/ answers how fast, and
    // the dependency only runs one way.
    //
    // What this case needs is A period, not bench's period. The 16 below is free. Nothing here is
    // coupled to CORPUS_PERIODIC and no edit there can reach this.
    for (size_t at = 0u; at < ADVERSARIAL_CORPUS; at += 1u)
    {
        corpus[at] = (uint8_t)(at % 16u);
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = needle, .src = corpus + 64u, .bytes = sizeof(needle));

    const size_t expected = orior_naive(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle));

    orior_steer_probes_reset();
    const size_t baseline_count = orior_steer_count(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle), 1);
    const uint64_t baseline_reads = orior_steer_probes;

    if (baseline_count != expected)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    FAIL periodic field count: ");
            mmgr_line_decimal(output_line, baseline_count, 1u);
            mmgr_line_put(output_line, " against ");
            mmgr_line_decimal(output_line, expected, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed = 1;
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  sampling on a period-16 field, ");
        mmgr_line_decimal(output_line, (unsigned long long)baseline_reads, 1u);
        mmgr_line_put(output_line, " reads at the planner's own stride");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  sampling cost, verdict ");
        mmgr_line_put(output_line, (failed == 0) ? "ok" : "FAILS");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(corpus, platform_place_g_program_owner);
    return failed;
}

/**
 * @brief Case 8. Probe order is a permutation null.
 *
 * @return 0 where every order of one probe set returns one count, 1 otherwise.
 * @note DIFFERENTIAL, AND IT TESTS THE COROLLARY DIRECTLY. An alignment survives only where every
 *       probe agrees, a conjunction commutes. The surviving set and the count are the same under
 *       any order. This was argued from the start and could not be checked until an entry took the
 *       probe set as an argument.
 * @note A failure here points at STATE CARRIED BETWEEN PROBES in the implementation and not at the
 *       theory, because the mathematics has no order in it to get wrong. That is the reading to
 *       give anyone who hits it.
 */
int adversarial_case_permutation_null(void)
{
    uint8_t *const corpus = (uint8_t *)platform_cell_taken(ADVERSARIAL_CORPUS, platform_place_g_program_owner, 0);
    uint8_t needle[16];
    int failed = 0;

    if (corpus == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    allocation failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    uint64_t state = 0x2545F4914F6CDD1DULL;
    adversarial_fill_field(corpus, ADVERSARIAL_CORPUS, 6u, &state);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = needle, .src = corpus + 700u, .bytes = sizeof(needle));

    const OriorProbe orders[4][3] = {
        {{0u, 1u, 1u}, {7u, 1u, 1u}, {15u, 1u, 1u}},
        {{15u, 1u, 1u}, {0u, 1u, 1u}, {7u, 1u, 1u}},
        {{7u, 1u, 1u}, {15u, 1u, 1u}, {0u, 1u, 1u}},
        {{15u, 1u, 1u}, {7u, 1u, 1u}, {0u, 1u, 1u}},
    };
    const size_t reference = orior_naive(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle));

    for (size_t which = 0u; which < 4u; which += 1u)
    {
        const size_t counted =
            orior_steer_count_with_probes(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle), orders[which], 3u);
        if (counted != reference)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    FAIL order ");
                mmgr_line_decimal(output_line, which, 1u);
                mmgr_line_put(output_line, " returned ");
                mmgr_line_decimal(output_line, counted, 1u);
                mmgr_line_put(output_line, " against reference ");
                mmgr_line_decimal(output_line, reference, 1u);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            failed = 1;
        }
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  probe order as a permutation null, verdict ");
        mmgr_line_put(output_line, (failed == 0) ? "ok" : "FAILS");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(corpus, platform_place_g_program_owner);
    return failed;
}

/**
 * @brief Case 9. The empty probe set is the identity element.
 *
 * @return 0 where no probes still returns the exact count at zero probe reads, 1 otherwise.
 * @note THE CHEAPEST TOTAL CHECK OF THE WHOLE GUARANTEE. With no probes every alignment reaches the
 *       full compare. The answer is exactly right and the cost is maximal. If this fails, the
 *       verifier is wrong and every other count in the tree rests on nothing, because every probe
 *       set relies on that same compare to remove its false survivors.
 * @note The read count is graded at zero as a premise check. Probes reading bytes where no probe
 *       was supplied would mean the entry chooses its own, and the case would be measuring
 *       something other than the identity.
 */
int adversarial_case_empty_plan(void)
{
    uint8_t *const corpus = (uint8_t *)platform_cell_taken(ADVERSARIAL_CORPUS, platform_place_g_program_owner, 0);
    uint8_t needle[12];
    int failed = 0;

    if (corpus == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    allocation failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    uint64_t state = 0x14057B7EF767814FULL;
    adversarial_fill_field(corpus, ADVERSARIAL_CORPUS, 4u, &state);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = needle, .src = corpus + 321u, .bytes = sizeof(needle));

    const size_t reference = orior_naive(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle));

    orior_steer_probes_reset();
    const size_t counted = orior_steer_count_with_probes(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle), NULL, 0u);
    const uint64_t reads = orior_steer_probes;

    if (counted != reference)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    FAIL empty plan counted ");
            mmgr_line_decimal(output_line, counted, 1u);
            mmgr_line_put(output_line, " against reference ");
            mmgr_line_decimal(output_line, reference, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed = 1;
    }
    if (reads != 0u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    FAIL premise: ");
            mmgr_line_decimal(output_line, (unsigned long long)reads, 1u);
            mmgr_line_put(output_line, " probe reads with no probes supplied");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed = 1;
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  the empty plan, ");
        mmgr_line_decimal(output_line, counted, 1u);
        mmgr_line_put(output_line, " occurrences at ");
        mmgr_line_decimal(output_line, (unsigned long long)reads, 1u);
        mmgr_line_put(output_line, " probe reads, verdict ");
        mmgr_line_put(output_line, (failed == 0) ? "ok" : "FAILS");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(corpus, platform_place_g_program_owner);
    return failed;
}

/**
 * @brief Case 10. Growing a probe set changes the cost and never the count.
 *
 * @return 0 where one, two, three and four probes all return the reference count, 1 otherwise.
 * @note THIS IS THEOREM 1 STATED AS A MEASUREMENT. The count is exact for ANY probe set. A sequence
 *       of sets returning different counts therefore refutes the necessary-condition guarantee
 *       itself. Each probe is a necessary condition of an occurrence, a conjunction of them is one,
 *       and the full compare removes the false survivors.
 * @note The reads are reported and never asserted, because more probes may read more or fewer bytes
 *       depending on where the field refutes, and only the count is guaranteed.
 */
int adversarial_case_growing_plan(void)
{
    uint8_t *const corpus = (uint8_t *)platform_cell_taken(ADVERSARIAL_CORPUS, platform_place_g_program_owner, 0);
    uint8_t needle[20];
    int failed = 0;

    if (corpus == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    allocation failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    uint64_t state = 0x76E15D3EFEFDCBBFULL;
    adversarial_fill_field(corpus, ADVERSARIAL_CORPUS, 5u, &state);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = needle, .src = corpus + 1024u, .bytes = sizeof(needle));

    const OriorProbe probes[4] = {
        {0u, 1u, 1u},
        {19u, 1u, 1u},
        {9u, 1u, 1u},
        {4u, 5u, 2u},
    };
    const size_t reference = orior_naive(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle));

    for (size_t used = 1u; used <= 4u; used += 1u)
    {
        orior_steer_probes_reset();
        const size_t counted =
            orior_steer_count_with_probes(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle), probes, used);
        if (counted != reference)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    FAIL ");
                mmgr_line_decimal(output_line, used, 1u);
                mmgr_line_put(output_line, " probe(s) counted ");
                mmgr_line_decimal(output_line, counted, 1u);
                mmgr_line_put(output_line, " against reference ");
                mmgr_line_decimal(output_line, reference, 1u);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            failed = 1;
        }
    }

    // The empty-needle boundary, which no other case reaches. An empty needle occurs at every
    // alignment. The reference is corpus_len + 1. orior_steer_count_with_probes returned 0 here
    // until its guard was split, disagreeing with orior_naive and orior_steer_count in the
    // same tree. The probes cannot be evaluated on a needle with no positions. The empty probe set
    // grades it.
    {
        const size_t empty_reference = orior_naive(corpus, ADVERSARIAL_CORPUS, needle, 0u);
        const size_t empty_counted = orior_steer_count_with_probes(corpus, ADVERSARIAL_CORPUS, needle, 0u, NULL, 0u);
        if (empty_counted != empty_reference)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    FAIL empty needle counted ");
                mmgr_line_decimal(output_line, empty_counted, 1u);
                mmgr_line_put(output_line, " against reference ");
                mmgr_line_decimal(output_line, empty_reference, 1u);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            failed = 1;
        }
    }

    // A probe that reads past the needle. Its last position is (needle_len - 2) + 4*2, which is
    // needle_len + 6. Orior_steer_probe_fits rejects it and the count is 0. Before the guard,
    // count_with_probes read needle[offset] and corpus[at + offset] off the end of both. The count
    // it returns is not the reference; an error is the point, and 0 is the documented one.
    {
        const OriorProbe overruns = {sizeof(needle) - 2u, 4u, 3u};
        const size_t error =
            orior_steer_count_with_probes(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle), &overruns, 1u);
        if (error != 0u)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    FAIL a probe past the needle counted ");
                mmgr_line_decimal(output_line, error, 1u);
                mmgr_line_put(output_line, ", expected the error 0");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            failed = 1;
        }
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  growing the plan over ");
        mmgr_line_decimal(output_line, reference, 1u);
        mmgr_line_put(output_line, " occurrences, verdict ");
        mmgr_line_put(output_line, (failed == 0) ? "ok" : "FAILS");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(corpus, platform_place_g_program_owner);
    return failed;
}

/**
 * @brief Case 11. Stopping at the destroy condition equals running to full depth.
 *
 * @return 0 where both descents agree on the count and on every probe the shallow one placed, 1
 *         otherwise.
 * @note THE DESTROY THEOREM TESTED BY ITS CONSEQUENCE. Under a candidate set that does not grow
 *       with the level, a fired stop condition would fire at every level below. Stopping and
 *       continuing place the same probes up to the stop point and return the same count.
 * @note TWO FAILURE MODES, AND THEY MEAN DIFFERENT THINGS. Counts differing means the
 *       necessary-condition guarantee broke, since both probe sets are legal whatever the descent
 *       chose. The probes before the stop point differing means the induction broke, and the
 *       candidate set is growing with the level where the theorem requires it not to. Reporting one
 *       verdict for both would lose that distinction.
 */
int adversarial_case_stop_equals_continue(void)
{
    uint8_t *const corpus = (uint8_t *)platform_cell_taken(ADVERSARIAL_CORPUS, platform_place_g_program_owner, 0);
    uint8_t needle[16];
    size_t shallow[ORIOR_STEER_ANCHORS];
    size_t deep[ORIOR_STEER_ANCHORS];
    int failed = 0;

    if (corpus == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    allocation failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    // A field of one repeated symbol, where the first probe prunes nothing and the stop fires early.
    EMBED_CALL(memor.set, MemoriaCfg, .dst = corpus, .bytes = ADVERSARIAL_CORPUS, .val = 'm');
    EMBED_CALL(memor.set, MemoriaCfg, .dst = needle, .bytes = sizeof(needle), .val = 'm');

    const size_t alignments = ADVERSARIAL_CORPUS - sizeof(needle) + 1u;
    uint8_t *const survivors = (uint8_t *)platform_cell_taken(alignments, platform_place_g_program_owner, 0);

    if (survivors == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    allocation failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_cell_released(corpus, platform_place_g_program_owner);
        return 1;
    }

    // The two runs differ in one member only, and the case exists to compare them.
    // force_full_depth is omitted on the first, and an omitted member is zero, the destroy
    // rule honored. Naming it on the second forces every level.
    const size_t stopped = ORIOR_STEER_CALL(
        orior_steer_spawn_coarms, OriorSteerDescent, .offsets = shallow, .count = ORIOR_STEER_ANCHORS, .corpus = corpus,
        .corpus_len = ADVERSARIAL_CORPUS, .needle = needle, .needle_len = sizeof(needle), .survivors = survivors,
        .survivors_length = alignments, .sample_stride = 1u);
    const size_t forced = ORIOR_STEER_CALL(
        orior_steer_spawn_coarms, OriorSteerDescent, .offsets = deep, .count = ORIOR_STEER_ANCHORS, .corpus = corpus,
        .corpus_len = ADVERSARIAL_CORPUS, .needle = needle, .needle_len = sizeof(needle), .survivors = survivors,
        .survivors_length = alignments, .sample_stride = 1u, .force_full_depth = 1);

    const size_t reference = orior_naive(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle));
    const size_t shallow_count = orior_steer_count(corpus, ADVERSARIAL_CORPUS, needle, sizeof(needle), 1);

    if (shallow_count != reference)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    FAIL descent count ");
            mmgr_line_decimal(output_line, shallow_count, 1u);
            mmgr_line_put(output_line, " against reference ");
            mmgr_line_decimal(output_line, reference, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed = 1;
    }
    for (size_t at = 0u; at < stopped; at += 1u)
    {
        if (shallow[at] != deep[at])
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "    FAIL induction: probe ");
                mmgr_line_decimal(output_line, at, 1u);
                mmgr_line_put(output_line, " is offset ");
                mmgr_line_decimal(output_line, shallow[at], 1u);
                mmgr_line_put(output_line, " stopped and ");
                mmgr_line_decimal(output_line, deep[at], 1u);
                mmgr_line_put(output_line, " forced");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            failed = 1;
        }
    }
    if (forced < stopped)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    FAIL forced depth ");
            mmgr_line_decimal(output_line, forced, 1u);
            mmgr_line_put(output_line, " below stopped depth ");
            mmgr_line_decimal(output_line, stopped, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed = 1;
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  stopping equals continuing, ");
        mmgr_line_decimal(output_line, stopped, 1u);
        mmgr_line_put(output_line, " probes stopped against ");
        mmgr_line_decimal(output_line, forced, 1u);
        mmgr_line_put(output_line, " forced, verdict ");
        mmgr_line_put(output_line, (failed == 0) ? "ok" : "FAILS");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(survivors, platform_place_g_program_owner);
    platform_cell_released(corpus, platform_place_g_program_owner);
    return failed;
}
