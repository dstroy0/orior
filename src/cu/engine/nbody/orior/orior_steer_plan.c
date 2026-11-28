// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// orior_steer_plan.c: descent, the recursive plan and the probes
#include "orior_internal.h"

/** @brief Shared entry the two planners differ only in their candidate set. */
static size_t steer_descend(size_t *offsets, size_t count, const uint8_t *corpus, size_t corpus_len,
                            const uint8_t *needle, size_t needle_len, uint8_t *survivors, size_t survivors_length,
                            size_t sample_stride, int spawning, int force_full_depth, const OriorField *any,
                            int resume)
{
    // A field of any symbol type supplies its own extents and its own validity, and the byte
    // pointers go unread. The call checks it on its own, and does not cast the field into the byte
    // pointers to satisfy a null test. That cast would pass the guard and mean nothing.
    if (any != NULL)
    {
        if ((offsets == NULL) || (survivors == NULL) || (count == 0u) || (any->same == NULL) ||
            (any->alignments == 0u) || (any->needle_len == 0u))
        {
            return 0u;
        }
        needle_len = any->needle_len;
        corpus_len = (any->alignments + any->needle_len) - 1u;
    }
    else if ((offsets == NULL) || (corpus == NULL) || (needle == NULL) || (survivors == NULL) || (count == 0u) ||
             (needle_len == 0u) || (needle_len > corpus_len))
    {
        return 0u;
    }
    if (count > ORIOR_STEER_ANCHORS)
    {
        return 0u;
    }

    const size_t alignments = (corpus_len - needle_len) + 1u;
    if (survivors_length < alignments)
    {
        // FAILS CLOSED. The kernel allocates nothing. A buffer that does not reach the alignment
        // count gives an error. The call never works around it with a plan on part of the field.
        return 0u;
    }

    const size_t stride = (sample_stride == 0u) ? 1u : sample_stride;

    // Every alignment starts truthy, and a probe can only take one down. That direction makes the
    // descent safe to stop at any level: the set shrinks and never grows back.
    //
    // ON RESUME THE SURVIVORS ARE THE INPUT, AND THE CALL DOES NOT RESET THEM. A caller composes a
    // recursive spawn: it runs one descent, then runs the next over the survivors that the last one
    // left. The child reads only what the parent left truthy, and its cost is the survivor count
    // and not the whole field. The engine cannot check that a survivor set that comes in is a valid
    // superset of the true occurrences. That obligation belongs to the caller. A set from an
    // earlier descent on this field meets it.
    //
    // A lone survivor is still not an answer. It passed only the probes placed so far. The caller
    // checks it against the conditions not yet tested, with a full compare, before it calls it
    // found.
    if (resume == 0)
    {
        for (size_t at = 0u; at < alignments; at += 1u)
        {
            survivors[at] = 1u;
        }
    }

    size_t chosen[ORIOR_STEER_ANCHORS];
    size_t placed = 0u;

    // THE BOUNDED DESCENT. One coarm for each level, at max `count` levels, which the guard above
    // bounds at or under ORIOR_STEER_ANCHORS. The corpus picks the offset of a level. Through the
    // destroy rule below, it can end the descent early, unless `force_full_depth` is not zero. The
    // program has the ceiling before it starts. This loop ends for the same reason that a for loop
    // over a fixed array does.
    while (placed < count)
    {
        size_t best_offset = 0u;
        size_t best_located = (size_t)-1;
        int found = 0;

        const size_t candidates = spawning ? needle_len : count;
        for (size_t which = 0u; which < candidates; which += 1u)
        {
            const size_t offset = spawning ? which : offsets[which];
            if (offset >= needle_len)
            {
                continue;
            }

            int already = 0;
            for (size_t seen = 0u; seen < placed; seen += 1u)
            {
                if (chosen[seen] == offset)
                {
                    already = 1;
                    break;
                }
            }
            if (already != 0)
            {
                continue;
            }

            const size_t located =
                steer_truthy_after(corpus, corpus_len, needle, needle_len, survivors, offset, stride, any);
            // Strictly fewer survivors wins. A tie retains the earlier candidate, and
            // the descent is therefore deterministic on identical input.
            if ((found == 0) || (located < best_located))
            {
                best_located = located;
                best_offset = offset;
                found = 1;
            }
        }

        if (found == 0)
        {
            break;
        }

        // DESTROY WHAT DOES NOT PRUNE. A probe that leaves the truthy population as it found it
        // rejects nothing that an earlier probe did not already reject. To place it would read a
        // byte for each alignment and buy nothing. The descent stops, and destroys every level
        // below it. The call returns `placed`. The caller learns how many probes survived, and
        // never gets dead ones to evaluate.
        //
        // This is the general form of what orior_anchors_for does in one special case. That
        // function returns a single anchor on a periodic corpus. At a period, every anchor tests
        // the same congruence, and the ones after the first are pure cost. Here the call MEASURES
        // the judgment for each level against the field, and does not infer it from a period. That
        // also catches fields whose redundancy no period search would name.
        if ((force_full_depth == 0) && (best_located >= steer_truthy_total(survivors, alignments, stride)))
        {
            break;
        }

        chosen[placed] = best_offset;
        placed += 1u;
        steer_make_falsy(corpus, corpus_len, needle, needle_len, survivors, best_offset, any);
    }

    for (size_t index = 0u; index < placed; index += 1u)
    {
        offsets[index] = chosen[index];
    }
    return placed;
}

size_t orior_steer_plan_recursive(const OriorSteerDescent *args)
{
    if (args == NULL)
    {
        return 0u;
    }

    // The reorder case. `spawning` is 0, and the candidates are the offsets that the caller
    // already placed. The call reads force_full_depth from the argument here too. A caller that
    // checks that a stop equals a continue must be able to force the reorder descent, as well as
    // the spawn descent.
    return steer_descend(args->offsets, args->count, args->corpus, args->corpus_len, args->needle, args->needle_len,
                         args->survivors, args->survivors_length, args->sample_stride, 0, args->force_full_depth,
                         args->any, args->resume);
}

size_t orior_steer_spawn_coarms(const OriorSteerDescent *args)
{
    if (args == NULL)
    {
        return 0u;
    }

    // The spawn case. `spawning` is 1, and the candidates are every position in the needle.
    return steer_descend(args->offsets, args->count, args->corpus, args->corpus_len, args->needle, args->needle_len,
                         args->survivors, args->survivors_length, args->sample_stride, 1, args->force_full_depth,
                         args->any, args->resume);
}

int orior_steer_probe_fits(const OriorProbe *probe, size_t needle_len)
{
    if ((probe == NULL) || (probe->length == 0u) || (needle_len == 0u))
    {
        return 0;
    }
    if (probe->origin >= needle_len)
    {
        return 0;
    }
    if (probe->length == 1u)
    {
        return 1;
    }
    if (probe->step == 0u)
    {
        // A line of length greater than one with no step reads one position again and again. That
        // is an arm in the shape of an eye. It gives an error here, and never collapses in silence.
        return 0;
    }

    // The last position is `origin + step*(length-1)`. The call forms it by division against the
    // capacity that stays, which gives an error on a step and length whose product would wrap
    // size_t. Else it would wrap into a position that passes a bounds test.
    const size_t range = needle_len - 1u - probe->origin;
    return ((probe->length - 1u) <= (range / probe->step)) ? 1 : 0;
}

/** @brief Whether one alignment agrees with the needle at every position a probe reads. */
static int steer_probe_agrees(const uint8_t *corpus, const uint8_t *needle, const OriorProbe *probe, size_t at)
{
    for (size_t step = 0u; step < probe->length; step += 1u)
    {
        const size_t offset = probe->origin + (step * probe->step);
        if (corpus[at + offset] != needle[offset])
        {
            return 0;
        }
    }
    return 1;
}

/** @brief Truthy alignments that stay if the call places `probe`, in the sampled population. */
size_t steer_truthy_after_probe(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                                const uint8_t *alive, const OriorProbe *probe, size_t stride)
{
    const size_t alignments = (corpus_len - needle_len) + 1u;
    size_t located = 0u;

    for (size_t at = 0u; at < alignments; at += stride)
    {
        if (alive[at] == 0u)
        {
            continue;
        }
        if (steer_probe_agrees(corpus, needle, probe, at) != 0)
        {
            located += 1u;
        }
    }
    return located;
}

/** @brief Turns falsy every alignment a probe rejects, over the whole population. */
void steer_make_falsy_probe(const uint8_t *corpus, size_t corpus_len, const uint8_t *needle, size_t needle_len,
                            uint8_t *alive, const OriorProbe *probe)
{
    const size_t alignments = (corpus_len - needle_len) + 1u;

    for (size_t at = 0u; at < alignments; at += 1u)
    {
        if (alive[at] == 0u)
        {
            continue;
        }
        if (steer_probe_agrees(corpus, needle, probe, at) == 0)
        {
            alive[at] = 0u;
        }
    }
}
