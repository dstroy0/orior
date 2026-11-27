// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "score_sample.h"
#include "../../../../includes/platform/platform.h"

#include "answer_key.h"
#include "bodies.h"
#include "../../../../src/cu/engine/nbody/box_history/box_history.h"
#include "../../../../src/cu/engine/nbody/climb_machine/climb_machine.h"
#include "coherence.h"
#include "../../../../src/cu/engine/nbody/contact_side/contact_side.h"
#include "../../../../src/cu/engine/analysis/cycle/cycle.h"
#include "../../../../src/cu/engine/nbody/division/division.h"
#include "../../../../src/cu/engine/engine.h"
#include "../../../../src/cu/engine/analysis/golden_bands/golden_bands.h"
#include "group_objects.h"
#include "../../../../src/cu/engine/nbody/heaviest_matching/heaviest_matching.h"
#include "link_objects.h"
#include "../../../../src/cu/engine/nbody/marginal/marginal.h"
#include "../../../../src/cu/engine/nbody/max_tree/max_tree.h"
#include "relate_frames.h"
#include "run_log.h"
#include "track.h"
#include "../../../../src/cu/engine/nbody/velocity/velocity.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

#define TOWER_ROUNDS 64u

static int score_flattened_slice(const char *sample, const TreeFrame *frames, unsigned int frame_count,
                                 const TreeRules *rules, unsigned long long *first, unsigned long long *bodies)
{
    unsigned int named_at = rules->flattened_samples;
    for (unsigned int named = 0u; named < rules->flattened_samples; named += 1u)
    {
        named_at = ((named_at == rules->flattened_samples) &&
                    (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = rules->flattened_names[named],
                                .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = sample, .ci = EMBED_FALSE)))
                       ? named
                       : named_at;
    }
    if (named_at == rules->flattened_samples)
    {
        fprintf(stderr, "  %s: not among the samples flattened.iapx contains; flatten it first\n", sample);
        return 0;
    }
    *first = rules->flattened_start[named_at];
    *bodies = rules->flattened_start[named_at + 1u] - *first;
    unsigned long long leaves = 0ull;
    for (unsigned int frame = 0u; frame < frame_count; frame += 1u)
    {
        leaves += (unsigned long long)frames[frame].leaf_count;
    }
    if ((leaves != *bodies) || (*bodies > 0xFFFFFFFFull))
    {
        fprintf(stderr, "  %s: the tracker contains %llu bodies and flattened.iapx %llu; flatten it again\n", sample,
                leaves, *bodies);
        return 0;
    }
    return 1;
}

static int score_print_match(const char *sample, TreeFrame *frames, unsigned int frame_count, const TreeRules *rules)
{
    unsigned long long first = 0ull;
    unsigned long long bodies = 0ull;
    if (score_flattened_slice(sample, frames, frame_count, rules, &first, &bodies) == 0)
    {
        return 0;
    }
    unsigned long long capacity = 0ull;
    for (unsigned int frame = 0u; frame < frame_count; frame += 1u)
    {
        capacity +=
            (frames[frame].forward != NULL)
                ? ((unsigned long long)frames[frame].leaf_count +
                   ((frames[frame].triple_start != NULL) ? (unsigned long long)frames[frame].triple_count : 0ull))
                : 0ull;
    }
    if (capacity > 0x3FFFFFFFull)
    {
        fprintf(stderr, "  %s: %llu candidate pairs are more than one sweep's index can store\n", sample, capacity);
        return 0;
    }
    unsigned int *const frame_first = (unsigned int *)platform_cell_taken(
        ((size_t)frame_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const pair_start = (unsigned int *)platform_cell_taken(
        ((size_t)frame_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const index = (unsigned int *)platform_cell_taken(((size_t)capacity + 1u) * 2u * sizeof(unsigned int),
                                                                    platform_place_g_program_owner, 0);
    unsigned int *const before = (unsigned int *)platform_cell_taken(((size_t)capacity + 1u) * sizeof(unsigned int),
                                                                     platform_place_g_program_owner, 0);
    unsigned int *const after = (unsigned int *)platform_cell_taken(((size_t)capacity + 1u) * sizeof(unsigned int),
                                                                    platform_place_g_program_owner, 0);
    unsigned int *const weight = (unsigned int *)platform_cell_taken(((size_t)capacity + 1u) * sizeof(unsigned int),
                                                                     platform_place_g_program_owner, 0);
    unsigned char *const chosen =
        (unsigned char *)platform_cell_taken((size_t)capacity + 1u, platform_place_g_program_owner, 0);
    int ok = (frame_first != NULL) && (pair_start != NULL) && (index != NULL) && (before != NULL) && (after != NULL) &&
             (weight != NULL) && (chosen != NULL);
    unsigned int made = 0u;
    if (ok)
    {
        frame_first[0] = 0u;
    }
    for (unsigned int frame = 0u; ok && (frame < frame_count); frame += 1u)
    {
        frame_first[frame + 1u] = frame_first[frame] + frames[frame].leaf_count;
        pair_start[frame] = made;
        const TreeFrame *const tree = &frames[frame];
        for (unsigned int leaf = 0u; (tree->forward != NULL) && (leaf < tree->leaf_count); leaf += 1u)
        {
            const unsigned int leaf_first = made;
            const unsigned int triples_first = (tree->triple_start != NULL) ? tree->triple_start[leaf] : 0u;
            const unsigned int triples_end = (tree->triple_start != NULL) ? tree->triple_start[leaf + 1u] : 0u;
            for (unsigned int triple = triples_first; triple <= triples_end; triple += 1u)
            {
                const int other = (triple < triples_end) ? (int)tree->triple_after[triple] : tree->forward[leaf];
                int fresh = (other >= 0) ? 1 : 0;
                for (unsigned int seen = leaf_first; fresh && (seen < made); seen += 1u)
                {
                    fresh = (after[seen] != (unsigned int)other) ? 1 : 0;
                }
                if (fresh != 0)
                {
                    before[made] = leaf;
                    after[made] = (unsigned int)other;
                    index[2u * made] = frame_first[frame] + leaf;
                    index[(2u * made) + 1u] = frame_first[frame + 1u] + (unsigned int)other;
                    made += 1u;
                }
            }
        }
    }
    pair_start[frame_count] = made;
    unsigned long long sweep = 0ull;
    const unsigned int *const sample_prints = &rules->prints[first * rules->print_limbs];
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const EngineRecordSweep run = {
        rules->print_pair, {sample_prints, sample_prints}, {bodies, bodies}, index, made, weight, &sweep, &error};
    ok = ok && ((made == 0u) || (engine_record_sweep(&run) == (long)made));
    if (error.type != ENGINE_ERROR_NONE)
    {
        track_error_report("print match sweep", &error);
    }
    unsigned int matched = 0u;
    unsigned int changed = 0u;
    unsigned int queried = 0u;
    for (unsigned int frame = 0u; ok && (frame < frame_count); frame += 1u)
    {
        TreeFrame *const tree = &frames[frame];
        const unsigned int start = pair_start[frame];
        const unsigned int count = pair_start[frame + 1u] - start;
        if (tree->forward == NULL)
        {
            continue;
        }
        const HeaviestMatchingRequest match = {&before[start], &after[start],    &weight[start],
                                               count,          tree->leaf_count, frames[frame + 1u].leaf_count,
                                               &chosen[start]};
        ok = (count == 0u) || (heaviest_matching_run(&match) >= 0L);
        unsigned int cursor = start;
        for (unsigned int leaf = 0u; ok && (leaf < tree->leaf_count); leaf += 1u)
        {
            int landed = -1;
            while ((cursor < pair_start[frame + 1u]) && (before[cursor] == leaf))
            {
                landed = (chosen[cursor] != 0u) ? (int)after[cursor] : landed;
                cursor += 1u;
            }
            matched += (landed >= 0) ? 1u : 0u;
            changed += (landed != tree->forward[leaf]) ? 1u : 0u;
            tree->forward[leaf] = landed;
        }
        queried += tree->leaf_count;
    }
    if (ok)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, sample);
        mmgr_line_put(output_line, ": the print match swept ");
        mmgr_line_decimal(output_line, made, 1u);
        mmgr_line_put(output_line, " pairs at once in ");
        mmgr_line_decimal(output_line, sweep, 1u);
        mmgr_line_put(output_line, " us; ");
        mmgr_line_decimal(output_line, matched, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, queried, 1u);
        mmgr_line_put(output_line, " leaves matched one to one, ");
        mmgr_line_decimal(output_line, changed, 1u);
        mmgr_line_put(output_line, " forwards changed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        fprintf(stderr, "  %s: the print match errored\n", sample);
    }
    platform_cell_released(frame_first, platform_place_g_program_owner);
    platform_cell_released(pair_start, platform_place_g_program_owner);
    platform_cell_released(index, platform_place_g_program_owner);
    platform_cell_released(before, platform_place_g_program_owner);
    platform_cell_released(after, platform_place_g_program_owner);
    platform_cell_released(weight, platform_place_g_program_owner);
    platform_cell_released(chosen, platform_place_g_program_owner);
    return ok;
}

static int score_field_truthy(const unsigned int *record, unsigned int offset, unsigned int bits)
{
    unsigned int set_bits = 0u;
    for (unsigned int bit = 0u; bit < bits; bit += 1u)
    {
        const unsigned int at = offset + bit;
        set_bits |= (record[at / 32u] >> (at % 32u)) & 1u;
    }
    return (set_bits != 0u) ? 1 : 0;
}

static int score_velocity_sweep(const char *sample, const char *pass, const TreeRules *rules,
                                const unsigned int *magnitudes, unsigned long long bodies, const unsigned int *lags,
                                const unsigned int *index, unsigned int lanes)
{
    const unsigned int out_limbs = cycle_record_out_limbs(rules->velocity_record);
    const size_t words = (size_t)lanes * out_limbs;
    unsigned int *const device =
        (unsigned int *)platform_cell_taken((words + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const host =
        (unsigned int *)platform_cell_taken((words + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned long long sweep = 0ull;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const EngineRecordSweep run = {rules->velocity_record,
                                   {magnitudes, magnitudes, lags},
                                   {bodies, bodies, bodies},
                                   index,
                                   lanes,
                                   device,
                                   &sweep,
                                   &error};
    const EngineRecordSweep host_run = {
        NULL, {magnitudes, magnitudes, lags}, {bodies, bodies, bodies}, index, lanes, host, NULL, &error};
    const int ok = (device != NULL) && (host != NULL) &&
                   ((lanes == 0u) || ((engine_record_sweep(&run) == (long)lanes) &&
                                      (engine_record_host(rules->velocity_encode_request, &host_run) == (long)lanes)));
    if (error.type != ENGINE_ERROR_NONE)
    {
        track_error_report("velocity sweep", &error);
    }
    const unsigned int *const offset = rules->velocity_encode_request->output_offset;
    const unsigned int *const bits = rules->velocity_encode_request->output_bits;
    unsigned int agree[ENGINE_AXES + 1u] = {0u, 0u, 0u, 0u};
    unsigned int differ = 0u;
    for (unsigned int lane = 0u; ok && (lane < lanes); lane += 1u)
    {
        const unsigned int *const record = &device[(size_t)lane * out_limbs];
        for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
        {
            agree[axis] +=
                (unsigned int)score_field_truthy(record, offset[VELOCITY_AGREES + axis], bits[VELOCITY_AGREES + axis]);
        }
        agree[ENGINE_AXES] +=
            (unsigned int)score_field_truthy(record, offset[VELOCITY_ALL_AGREE], bits[VELOCITY_ALL_AGREE]);
        differ += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = record, .other = &host[(size_t)lane * out_limbs],
                              .bytes = out_limbs * sizeof(unsigned int)) != 0)
                      ? 1u
                      : 0u;
    }
    if (ok)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, sample);
        mmgr_line_put(output_line, ": velocity ");
        mmgr_line_put(output_line, pass);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, lanes, 1u);
        mmgr_line_put(output_line, " lanes in ");
        mmgr_line_decimal(output_line, sweep, 1u);
        mmgr_line_put(output_line, " us: the climbed lag is beside the dipole's change on ");
        mmgr_line_decimal(output_line, agree[ENGINE_AXES], 1u);
        mmgr_line_put(output_line, " (z ");
        mmgr_line_decimal(output_line, agree[0], 1u);
        mmgr_line_put(output_line, ", y ");
        mmgr_line_decimal(output_line, agree[1], 1u);
        mmgr_line_put(output_line, ", x ");
        mmgr_line_decimal(output_line, agree[2], 1u);
        mmgr_line_put(output_line, "); ");
        mmgr_line_decimal(output_line, differ, 1u);
        mmgr_line_put(output_line, " differ from the host");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        fprintf(stderr, "  %s: the velocity %s errored\n", sample, pass);
    }
    platform_cell_released(device, platform_place_g_program_owner);
    platform_cell_released(host, platform_place_g_program_owner);
    return (ok && (differ == 0u)) ? 1 : 0;
}

static int score_velocity(const char *sample, const TreeFrame *frames, unsigned int frame_count, const TreeRules *rules)
{
    unsigned long long first = 0ull;
    unsigned long long bodies = 0ull;
    if (score_flattened_slice(sample, frames, frame_count, rules, &first, &bodies) == 0)
    {
        return 0;
    }
    unsigned int *const frame_first = (unsigned int *)platform_cell_taken(
        ((size_t)frame_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const lags = (unsigned int *)platform_cell_taken(
        ((size_t)bodies * ENGINE_AXES + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const checked = (unsigned int *)platform_cell_taken(
        ((size_t)bodies * VELOCITY_MEMBERS + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const predicted = (unsigned int *)platform_cell_taken(
        ((size_t)bodies * VELOCITY_MEMBERS + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    int ok = (frame_first != NULL) && (lags != NULL) && (checked != NULL) && (predicted != NULL);
    unsigned int checks = 0u;
    unsigned int predictions = 0u;
    if (ok)
    {
        frame_first[0] = 0u;
    }
    for (unsigned int frame = 0u; ok && (frame < frame_count); frame += 1u)
    {
        frame_first[frame + 1u] = frame_first[frame] + frames[frame].leaf_count;
    }
    for (unsigned int frame = 0u; ok && (frame < frame_count); frame += 1u)
    {
        const TreeFrame *const tree = &frames[frame];
        for (unsigned int leaf = 0u; leaf < tree->leaf_count; leaf += 1u)
        {
            const unsigned int body = frame_first[frame] + leaf;
            for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
            {
                lags[(ENGINE_AXES * body) + axis] =
                    (tree->forward_lag != NULL) ? (unsigned int)tree->forward_lag[(ENGINE_AXES * leaf) + axis] : 0u;
            }
            if ((tree->forward_lag != NULL) && (tree->forward != NULL) && (tree->forward[leaf] >= 0))
            {
                checked[VELOCITY_MEMBERS * checks] = body;
                checked[(VELOCITY_MEMBERS * checks) + 1u] = frame_first[frame + 1u] + (unsigned int)tree->forward[leaf];
                checked[(VELOCITY_MEMBERS * checks) + 2u] = body;
                checks += 1u;
            }
            if ((tree->forward_lag != NULL) && (tree->backward != NULL) && (tree->backward[leaf] >= 0))
            {
                predicted[VELOCITY_MEMBERS * predictions] =
                    frame_first[frame - 1u] + (unsigned int)tree->backward[leaf];
                predicted[(VELOCITY_MEMBERS * predictions) + 1u] = body;
                predicted[(VELOCITY_MEMBERS * predictions) + 2u] = body;
                predictions += 1u;
            }
        }
    }
    const unsigned int *const magnitudes = &rules->flattened[first * rules->flattened_limbs];
    ok = ok && (score_velocity_sweep(sample, "check, each body and its climbed forward", rules, magnitudes, bodies,
                                     lags, checked, checks) != 0);
    ok = ok && (score_velocity_sweep(sample, "prediction, each body's last change against its next climb", rules,
                                     magnitudes, bodies, lags, predicted, predictions) != 0);
    platform_cell_released(frame_first, platform_place_g_program_owner);
    platform_cell_released(lags, platform_place_g_program_owner);
    platform_cell_released(checked, platform_place_g_program_owner);
    platform_cell_released(predicted, platform_place_g_program_owner);
    return ok;
}

static unsigned long long score_division_capacity(const TreeFrame *frames, unsigned int frame_count,
                                                  unsigned int **start)
{
    unsigned long long capacity = 0ull;
    for (unsigned int frame = 1u; frame < frame_count; frame += 1u)
    {
        const TreeFrame *const later = &frames[frame];
        const unsigned int parents = frames[frame - 1u].leaf_count;
        start[frame] = (later->backward != NULL)
                           ? (unsigned int *)platform_cell_cleared((size_t)parents + 2u, sizeof(unsigned int),
                                                                   platform_place_g_program_owner, 0)
                           : NULL;
        for (unsigned int leaf = 0u; (start[frame] != NULL) && (leaf < later->leaf_count); leaf += 1u)
        {
            const int parent = later->backward[leaf];
            start[frame][(parent >= 0) ? ((unsigned int)parent + 1u) : (parents + 1u)] += (parent >= 0) ? 1u : 0u;
        }
        for (unsigned int parent = 0u; (start[frame] != NULL) && (parent < parents); parent += 1u)
        {
            const unsigned long long children = (unsigned long long)start[frame][parent + 1u];
            capacity += (children * (children - ((children != 0ull) ? 1ull : 0ull))) / 2ull;
            start[frame][parent + 1u] += start[frame][parent];
        }
    }
    return capacity;
}

static unsigned int *score_key_successors(const AnswerKey *key)
{
    // node_count widens from unsigned int to size_t; node_count + 1 cannot wrap before calloc sees it
    unsigned int *const successors = (unsigned int *)platform_cell_cleared(
        (size_t)key->node_count + 1u, 3u * sizeof(unsigned int), platform_place_g_program_owner, 0);
    for (unsigned int edge = 0u; (successors != NULL) && (edge < key->edge_count); edge += 1u)
    {
        const long source = node_index_of(key, key->edge_ends[2u * edge]);
        const long target = node_index_of(key, key->edge_ends[(2u * edge) + 1u]);
        if ((source >= 0L) && (target >= 0L))
        {
            unsigned int *const node_successors = &successors[3L * source];
            // target is a node index, non-negative and below node_count. It narrows from long to unsigned int exactly
            node_successors[(node_successors[0] < 2u) ? (node_successors[0] + 1u) : 0u] =
                (node_successors[0] < 2u) ? (unsigned int)target : node_successors[0];
            node_successors[0] += 1u;
        }
    }
    return successors;
}

static int score_division(const char *sample, const TreeFrame *frames, unsigned int frame_count, const TreeRules *rules,
                          const AnswerKey *key, const int *node_leaf, const int *tree_index_of_time,
                          unsigned int volume_frames)
{
    unsigned long long first = 0ull;
    unsigned long long bodies = 0ull;
    if (score_flattened_slice(sample, frames, frame_count, rules, &first, &bodies) == 0)
    {
        return 0;
    }
    unsigned int *const frame_first = (unsigned int *)platform_cell_taken(
        ((size_t)frame_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int **const start = (unsigned int **)platform_cell_cleared(
        (size_t)frame_count + 1u, sizeof(unsigned int *), platform_place_g_program_owner, 0);
    int ok = (frame_first != NULL) && (start != NULL);
    const unsigned long long capacity = ok ? score_division_capacity(frames, frame_count, start) : 0ull;
    if (ok && (capacity > 0x3FFFFFFFull))
    {
        fprintf(stderr, "  %s: %llu division triples are more than one sweep's index can store\n", sample, capacity);
        ok = 0;
    }
    unsigned int *const index =
        ok ? (unsigned int *)platform_cell_taken(((size_t)capacity * DIVISION_MEMBERS + 1u) * sizeof(unsigned int),
                                                 platform_place_g_program_owner, 0)
           : NULL;
    unsigned int *const children =
        ok ? (unsigned int *)platform_cell_taken(((size_t)bodies + 1u) * sizeof(unsigned int),
                                                 platform_place_g_program_owner, 0)
           : NULL;
    ok = ok && (index != NULL) && (children != NULL);
    if (ok)
    {
        frame_first[0] = 0u;
    }
    for (unsigned int frame = 0u; ok && (frame < frame_count); frame += 1u)
    {
        frame_first[frame + 1u] = frame_first[frame] + frames[frame].leaf_count;
    }
    unsigned int lanes = 0u;
    for (unsigned int frame = 1u; ok && (frame < frame_count); frame += 1u)
    {
        const TreeFrame *const later = &frames[frame];
        const unsigned int parents = frames[frame - 1u].leaf_count;
        unsigned int *const cursor = start[frame];
        if (cursor == NULL)
        {
            continue;
        }
        for (unsigned int leaf = 0u; leaf < later->leaf_count; leaf += 1u)
        {
            const int parent = later->backward[leaf];
            if (parent >= 0)
            {
                children[cursor[parent]] = leaf;
                cursor[parent] += 1u;
            }
        }
        unsigned int from = 0u;
        for (unsigned int parent = 0u; parent < parents; parent += 1u)
        {
            const unsigned int end = cursor[parent];
            for (unsigned int one = from; one < end; one += 1u)
            {
                for (unsigned int other = one + 1u; other < end; other += 1u)
                {
                    index[DIVISION_MEMBERS * lanes] = frame_first[frame - 1u] + parent;
                    index[(DIVISION_MEMBERS * lanes) + 1u] = frame_first[frame] + children[one];
                    index[(DIVISION_MEMBERS * lanes) + 2u] = frame_first[frame] + children[other];
                    lanes += 1u;
                }
            }
            from = end;
        }
    }
    const unsigned int *const magnitudes = &rules->flattened[first * rules->flattened_limbs];
    const unsigned int out_limbs = cycle_record_out_limbs(rules->division_record);
    const size_t words = (size_t)lanes * out_limbs;
    unsigned int *const device =
        ok ? (unsigned int *)platform_cell_taken((words + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0)
           : NULL;
    unsigned int *const host =
        ok ? (unsigned int *)platform_cell_taken((words + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0)
           : NULL;
    unsigned long long sweep = 0ull;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const EngineRecordSweep run = {rules->division_record,
                                   {magnitudes, magnitudes, magnitudes},
                                   {bodies, bodies, bodies},
                                   index,
                                   lanes,
                                   device,
                                   &sweep,
                                   &error};
    const EngineRecordSweep host_run = {
        NULL, {magnitudes, magnitudes, magnitudes}, {bodies, bodies, bodies}, index, lanes, host, NULL, &error};
    ok = ok && (device != NULL) && (host != NULL) &&
         ((lanes == 0u) || ((engine_record_sweep(&run) == (long)lanes) &&
                            (engine_record_host(rules->division_encode_request, &host_run) == (long)lanes)));
    if (error.type != ENGINE_ERROR_NONE)
    {
        track_error_report("division sweep", &error);
    }
    const unsigned int *const offset = rules->division_encode_request->output_offset;
    const unsigned int *const bits = rules->division_encode_request->output_bits;
    unsigned int output_true[DIVISION_OUTPUTS] = {0u, 0u, 0u};
    unsigned int differ = 0u;
    for (unsigned int lane = 0u; ok && (lane < lanes); lane += 1u)
    {
        const unsigned int *const record = &device[(size_t)lane * out_limbs];
        for (unsigned int output = 0u; output < DIVISION_OUTPUTS; output += 1u)
        {
            output_true[output] += (unsigned int)score_field_truthy(record, offset[output], bits[output]);
        }
        differ += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = record, .other = &host[(size_t)lane * out_limbs],
                              .bytes = out_limbs * sizeof(unsigned int)) != 0)
                      ? 1u
                      : 0u;
    }
    unsigned int *const successors = ok ? score_key_successors(key) : NULL;
    ok = ok && (successors != NULL);
    unsigned int truths = 0u;
    unsigned int resolved = 0u;
    unsigned int among = 0u;
    unsigned int truth_true[DIVISION_OUTPUTS] = {0u, 0u, 0u};
    unsigned int lost_time = 0u;
    unsigned int lost_off_leaf = 0u;
    unsigned int lost_one_leaf = 0u;
    unsigned int back_to_parent = 0u;
    for (unsigned int node = 0u; ok && (node < key->node_count); node += 1u)
    {
        const unsigned int *const node_successors = &successors[3u * node];
        if (node_successors[0] != 2u)
        {
            continue;
        }
        truths += 1u;
        const int time = key->node_coordinates[(size_t)node * 4u];
        const int child_time = key->node_coordinates[(size_t)node_successors[1] * 4u];
        const int sibling_time = key->node_coordinates[(size_t)node_successors[2] * 4u];
        const int parent_leaf = node_leaf[node];
        const int child_leaf = node_leaf[node_successors[1]];
        const int sibling_leaf = node_leaf[node_successors[2]];
        const int timed = (time >= 0) && ((unsigned int)time + 1u < volume_frames) && (child_time == time + 1) &&
                          (sibling_time == time + 1) && (tree_index_of_time[time] >= 0);
        lost_time += (timed == 0) ? 1u : 0u;
        lost_off_leaf += ((timed != 0) && ((parent_leaf < 0) || (child_leaf < 0) || (sibling_leaf < 0))) ? 1u : 0u;
        lost_one_leaf +=
            ((timed != 0) && (parent_leaf >= 0) && (child_leaf >= 0) && (child_leaf == sibling_leaf)) ? 1u : 0u;
        if ((timed != 0) && (parent_leaf >= 0) && (child_leaf >= 0) && (sibling_leaf >= 0) &&
            (child_leaf != sibling_leaf))
        {
            const TreeFrame *const later = &frames[(unsigned int)tree_index_of_time[time] + 1u];
            const int child_back = (later->backward != NULL) ? later->backward[child_leaf] : -1;
            const int sibling_back = (later->backward != NULL) ? later->backward[sibling_leaf] : -1;
            back_to_parent += (unsigned int)(child_back == parent_leaf) + (unsigned int)(sibling_back == parent_leaf);
        }
        if ((time < 0) || ((unsigned int)time + 1u >= volume_frames) || (child_time != time + 1) ||
            (sibling_time != time + 1) || (parent_leaf < 0) || (child_leaf < 0) || (sibling_leaf < 0) ||
            (child_leaf == sibling_leaf) || (tree_index_of_time[time] < 0))
        {
            continue;
        }
        resolved += 1u;
        const unsigned int frame = (unsigned int)tree_index_of_time[time];
        const unsigned int parent_body = frame_first[frame] + (unsigned int)parent_leaf;
        const unsigned int child_body = frame_first[frame + 1u] + (unsigned int)child_leaf;
        const unsigned int sibling_body = frame_first[frame + 1u] + (unsigned int)sibling_leaf;
        for (unsigned int lane = 0u; lane < lanes; lane += 1u)
        {
            const unsigned int *const triple = &index[DIVISION_MEMBERS * lane];
            const int same =
                (triple[0] == parent_body) && (((triple[1] == child_body) && (triple[2] == sibling_body)) ||
                                               ((triple[1] == sibling_body) && (triple[2] == child_body)));
            if (same)
            {
                among += 1u;
                for (unsigned int output = 0u; output < DIVISION_OUTPUTS; output += 1u)
                {
                    truth_true[output] += (unsigned int)score_field_truthy(&device[(size_t)lane * out_limbs],
                                                                           offset[output], bits[output]);
                }
            }
        }
    }
    if (ok)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, sample);
            mmgr_line_put(output_line, ": division, ");
            mmgr_line_decimal(output_line, lanes, 1u);
            mmgr_line_put(output_line, " triples (a parent and two pieces whose climbs land back on it) in ");
            mmgr_line_decimal(output_line, sweep, 1u);
            mmgr_line_put(output_line, " us: mass conserved on ");
            mmgr_line_decimal(output_line, output_true[DIVISION_BY_MASS], 1u);
            mmgr_line_put(output_line, ", split along the parent's long axis on ");
            mmgr_line_decimal(output_line, output_true[DIVISION_BY_AXIS], 1u);
            mmgr_line_put(output_line, ", both on ");
            mmgr_line_decimal(output_line, output_true[DIVISION_BY_BOTH], 1u);
            mmgr_line_put(output_line, "; ");
            mmgr_line_decimal(output_line, differ, 1u);
            mmgr_line_put(output_line, " differ from the host");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, sample);
            mmgr_line_put(output_line, ": the key contains ");
            mmgr_line_decimal(output_line, truths, 1u);
            mmgr_line_put(output_line, " divisions, ");
            mmgr_line_decimal(output_line, resolved, 1u);
            mmgr_line_put(output_line, " on three separate leaves, ");
            mmgr_line_decimal(output_line, among, 1u);
            mmgr_line_put(output_line, " among the triples: mass on ");
            mmgr_line_decimal(output_line, truth_true[DIVISION_BY_MASS], 1u);
            mmgr_line_put(output_line, ", axis on ");
            mmgr_line_decimal(output_line, truth_true[DIVISION_BY_AXIS], 1u);
            mmgr_line_put(output_line, ", both on ");
            mmgr_line_decimal(output_line, truth_true[DIVISION_BY_BOTH], 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, sample);
            mmgr_line_put(output_line, ": of the key's divisions, ");
            mmgr_line_decimal(output_line, lost_time, 1u);
            mmgr_line_put(output_line, " fall outside consecutive tracked frames, ");
            mmgr_line_decimal(output_line, lost_off_leaf, 1u);
            mmgr_line_put(output_line, " have a node on no leaf, ");
            mmgr_line_decimal(output_line, lost_one_leaf, 1u);
            mmgr_line_put(output_line, " have both children on one leaf; on three separate leaves, ");
            mmgr_line_decimal(output_line, back_to_parent, 1u);
            mmgr_line_put(output_line, " of the ");
            mmgr_line_decimal(output_line, 2u * resolved, 1u);
            mmgr_line_put(output_line, " children climb back to their parent");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    else
    {
        fprintf(stderr, "  %s: the division sweep errored\n", sample);
    }
    for (unsigned int frame = 0u; (start != NULL) && (frame < frame_count); frame += 1u)
    {
        platform_cell_released(start[frame], platform_place_g_program_owner);
    }
    platform_cell_released(start, platform_place_g_program_owner);
    platform_cell_released(frame_first, platform_place_g_program_owner);
    platform_cell_released(index, platform_place_g_program_owner);
    platform_cell_released(children, platform_place_g_program_owner);
    platform_cell_released(device, platform_place_g_program_owner);
    platform_cell_released(host, platform_place_g_program_owner);
    platform_cell_released(successors, platform_place_g_program_owner);
    return (ok && (differ == 0u)) ? 1 : 0;
}

static int score_limbs_order(const unsigned int *left, const unsigned int *right)
{
    for (unsigned int limb = MARGINAL_LIMBS; limb > 0u; limb -= 1u)
    {
        if (left[limb - 1u] != right[limb - 1u])
        {
            return (left[limb - 1u] < right[limb - 1u]) ? -1 : 1;
        }
    }
    return 0;
}

typedef struct
{
    MarginalQuestion *questions;
    MarginalSource *sources;
    MarginalOption *options;
    unsigned int question_count;
    unsigned int source_count;
    unsigned int option_count;
    size_t source_capacity;
    size_t option_capacity;
    unsigned int *queried_leaf;
    unsigned int *queried_frame;
} ScoreMarginalSet;

static int score_marginal_capacity(ScoreMarginalSet *set, size_t sources, size_t options)
{
    if ((set->source_count + sources) > set->source_capacity)
    {
        const size_t capacity = (set->source_count + sources) * 2u;
        MarginalSource *const grown =
            (MarginalSource *)platform_cell_grown(set->sources, set->source_capacity * sizeof(MarginalSource),
                                                  capacity * sizeof(MarginalSource), platform_place_g_program_owner, 0);
        set->sources = (grown != NULL) ? grown : set->sources;
        set->source_capacity = (grown != NULL) ? capacity : set->source_capacity;
        if (grown == NULL)
        {
            return 0;
        }
    }
    if ((set->option_count + options) > set->option_capacity)
    {
        const size_t capacity = (set->option_count + options) * 2u;
        MarginalOption *const grown =
            (MarginalOption *)platform_cell_grown(set->options, set->option_capacity * sizeof(MarginalOption),
                                                  capacity * sizeof(MarginalOption), platform_place_g_program_owner, 0);
        set->options = (grown != NULL) ? grown : set->options;
        set->option_capacity = (grown != NULL) ? capacity : set->option_capacity;
        if (grown == NULL)
        {
            return 0;
        }
    }
    return 1;
}

static unsigned int score_best_null(const TreeFrame *tree, unsigned int leaf)
{
    unsigned int best = 0u;
    for (unsigned int draw = 0u; draw < tree->null_count; draw += 1u)
    {
        const unsigned int drawn = tree->null_final_score[((size_t)draw * ((size_t)tree->leaf_count + 1u)) + leaf];
        best = (drawn > best) ? drawn : best;
    }
    return best;
}

typedef struct
{
    const unsigned int *start;
    const unsigned int *target;
    const unsigned int *weight;
    unsigned int count;
} ScoreChoices;

static ScoreChoices score_choices_of(const TreeFrame *frames, const ScoreChoices *stars, unsigned int frame)
{
    const TreeFrame *const tree = &frames[frame];
    const ScoreChoices triples = {tree->triple_start, tree->triple_after, tree->triple_shared, tree->triple_count};
    return ((stars != NULL) && (stars[frame].start != NULL)) ? stars[frame] : triples;
}

static int score_marginal_query(ScoreMarginalSet *set, const TreeFrame *tree, const ScoreChoices *choices,
                                const unsigned int *target_start, const unsigned int *target_sources, unsigned int leaf,
                                int alone)
{
    unsigned int local_leaf[MARGINAL_SOURCES_MAX];
    unsigned int local_target[MARGINAL_TARGETS_MAX];
    unsigned int leaves = 1u;
    unsigned int targets = 0u;
    local_leaf[0] = leaf;
    for (unsigned int triple = choices->start[leaf]; (alone == 0) && (triple < choices->start[leaf + 1u]); triple += 1u)
    {
        const unsigned int target = choices->target[triple];
        for (unsigned int at = target_start[target]; at < target_start[target + 1u]; at += 1u)
        {
            const unsigned int other = target_sources[at];
            int fresh = 1;
            for (unsigned int seen = 0u; seen < leaves; seen += 1u)
            {
                fresh = fresh && (local_leaf[seen] != other);
            }
            if (fresh && (leaves == MARGINAL_SOURCES_MAX))
            {
                return 0;
            }
            local_leaf[leaves] = fresh ? other : local_leaf[leaves];
            leaves += fresh ? 1u : 0u;
        }
    }
    size_t wanted = 0u;
    for (unsigned int local = 0u; local < leaves; local += 1u)
    {
        wanted += (size_t)(choices->start[local_leaf[local] + 1u] - choices->start[local_leaf[local]]);
    }
    if (score_marginal_capacity(set, leaves, wanted) == 0)
    {
        return -1;
    }
    const unsigned int source_first = set->source_count;
    const unsigned int option_mark = set->option_count;
    for (unsigned int local = 0u; local < leaves; local += 1u)
    {
        const unsigned int own = local_leaf[local];
        MarginalSource *const source = &set->sources[set->source_count + local];
        source->option_first = set->option_count;
        source->options = 0u;
        source->absent = score_best_null(tree, own);
        for (unsigned int triple = choices->start[own]; triple < choices->start[own + 1u]; triple += 1u)
        {
            const unsigned int target = choices->target[triple];
            unsigned int target_at = targets;
            for (unsigned int seen = 0u; seen < targets; seen += 1u)
            {
                target_at = (local_target[seen] == target) ? seen : target_at;
            }
            if ((target_at == targets) && (targets == MARGINAL_TARGETS_MAX))
            {
                set->option_count = option_mark;
                return 0;
            }
            local_target[target_at] = target;
            targets += (target_at == targets) ? 1u : 0u;
            set->options[set->option_count].target = target_at;
            set->options[set->option_count].weight = choices->weight[triple];
            set->option_count += 1u;
            source->options += 1u;
        }
    }
    const MarginalRequest probe = {set->questions,    0u,   set->sources, source_first + leaves, set->options,
                                   set->option_count, NULL, NULL};
    const MarginalQuestion question = {source_first, leaves};
    if (marginal_arrangements(&probe, &question) == MARGINAL_ERROR)
    {
        set->option_count = option_mark;
        return 0;
    }
    set->questions[set->question_count] = question;
    set->source_count += leaves;
    return 1;
}

typedef struct
{
    ScoreChoices *choices;
    unsigned int *start;
    unsigned int *target;
    unsigned int *weight;
} ScoreStars;

static void score_stars_release(ScoreStars *stars)
{
    platform_cell_released(stars->choices, platform_place_g_program_owner);
    platform_cell_released(stars->start, platform_place_g_program_owner);
    platform_cell_released(stars->target, platform_place_g_program_owner);
    platform_cell_released(stars->weight, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = stars, .bytes = sizeof(*stars), .val = 0);
}

static unsigned int score_box_cell(const ClimbMachineBox *box, unsigned int boxed, const int *lag)
{
    unsigned int found = 0xFFFFFFFFu;
    for (unsigned int cell = box->cell_first[boxed]; cell < box->cell_first[boxed + 1u]; cell += 1u)
    {
        const int *const shift = &box->cell_shift[3u * cell];
        const int same = (shift[0] == lag[0]) && (shift[1] == lag[1]) && (shift[2] == lag[2]);
        found = ((found == 0xFFFFFFFFu) && same) ? cell : found;
    }
    return found;
}

static int score_box(const char *sample, const TreeFrame *frames, unsigned int frame_count, ClimbMachine *machine,
                     const int *tree_index_of_time, unsigned int volume_frames, ScoreStars *stars,
                     ClimbMachineBox *box_out)
{
    ClimbMachineBox box;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = box_out, .bytes = sizeof(*box_out), .val = 0);
    if (climb_machine_box(machine, &box) == 0)
    {
        fprintf(stderr,
                "  %s: the box errored: %u climbers, %u cells; %u cells met more than %u bodies, %u cells"
                " counted fewer positive voxels than labeled ones\n",
                sample, box.climbers, box.cells, box.crowded, CLIMB_MACHINE_BOX_TARGETS, box.broken);
        return 0;
    }
    size_t starts = 0u;
    for (unsigned int frame = 0u; frame < frame_count; frame += 1u)
    {
        starts += (size_t)frames[frame].leaf_count + 1u;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = stars, .bytes = sizeof(*stars), .val = 0);
    stars->choices = (ScoreChoices *)platform_cell_cleared((size_t)frame_count + 1u, sizeof(ScoreChoices),
                                                           platform_place_g_program_owner, 0);
    stars->start =
        (unsigned int *)platform_cell_cleared(starts + 1u, sizeof(unsigned int), platform_place_g_program_owner, 0);
    stars->target = (unsigned int *)platform_cell_taken(((size_t)box.entries + 1u) * sizeof(unsigned int),
                                                        platform_place_g_program_owner, 0);
    stars->weight = (unsigned int *)platform_cell_taken(((size_t)box.entries + 1u) * sizeof(unsigned int),
                                                        platform_place_g_program_owner, 0);
    int ok = (stars->choices != NULL) && (stars->start != NULL) && (stars->target != NULL) && (stars->weight != NULL);
    unsigned int *const frame_offset = (unsigned int *)platform_cell_cleared(
        (size_t)frame_count + 1u, sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = ok && (frame_offset != NULL);
    for (unsigned int frame = 0u; ok && (frame < frame_count); frame += 1u)
    {
        frame_offset[frame + 1u] = frame_offset[frame] + frames[frame].leaf_count + 1u;
    }
    unsigned int placed = 0u;
    unsigned int frame_base = 0u;
    unsigned int drift_queried = 0u;
    unsigned int drift_differ = 0u;
    unsigned int starred = 0u;
    for (unsigned int boxed = 0u; ok && (boxed < box.climbers); boxed += 1u)
    {
        const unsigned int earlier = box.earlier[boxed];
        const unsigned int later = box.later[boxed];
        if ((later != earlier + 1u) || (earlier >= volume_frames) || (tree_index_of_time[earlier] < 0))
        {
            continue;
        }
        const unsigned int frame = (unsigned int)tree_index_of_time[earlier];
        const TreeFrame *const tree = &frames[frame];
        const unsigned int leaf = box.leaf[boxed];
        if (((frame + 1u) >= frame_count) || (frames[frame + 1u].time != later) || (tree->forward_lag == NULL) ||
            (leaf >= tree->leaf_count))
        {
            continue;
        }
        unsigned int *const start = &stars->start[frame_offset[frame]];
        if (leaf == 0u)
        {
            frame_base = placed;
            stars->choices[frame].start = start;
            stars->choices[frame].target = &stars->target[frame_base];
            stars->choices[frame].weight = &stars->weight[frame_base];
        }
        start[leaf] = placed - frame_base;
        const unsigned int climbed = score_box_cell(&box, boxed, &tree->forward_lag[3u * leaf]);
        ok = (climbed != 0xFFFFFFFFu);
        for (unsigned int entry = ok ? box.entry_first[climbed] : 0u; ok && (entry < box.entry_first[climbed + 1u]);
             entry += 1u)
        {
            if (box.target[entry] == CLIMB_MACHINE_BOX_NONE)
            {
                continue;
            }
            stars->target[placed] = box.target[entry];
            stars->weight[placed] = box.count[entry];
            placed += 1u;
        }
        start[leaf + 1u] = placed - frame_base;
        stars->choices[frame].count = placed - frame_base;
        starred += 1u;
        const unsigned int drifted = score_box_cell(&box, boxed, tree->lag_to_next);
        if ((drifted == 0xFFFFFFFFu) || (tree->triple_start == NULL))
        {
            continue;
        }
        drift_queried += 1u;
        unsigned int labeled = 0u;
        unsigned int agreed = 0u;
        for (unsigned int entry = box.entry_first[drifted]; entry < box.entry_first[drifted + 1u]; entry += 1u)
        {
            if (box.target[entry] == CLIMB_MACHINE_BOX_NONE)
            {
                continue;
            }
            labeled += 1u;
            for (unsigned int triple = tree->triple_start[leaf]; triple < tree->triple_start[leaf + 1u]; triple += 1u)
            {
                agreed += ((tree->triple_after[triple] == box.target[entry]) &&
                           (tree->triple_shared[triple] == box.count[entry]))
                              ? 1u
                              : 0u;
            }
        }
        const unsigned int triples = tree->triple_start[leaf + 1u] - tree->triple_start[leaf];
        drift_differ += ((labeled != triples) || (agreed != triples)) ? 1u : 0u;
    }
    platform_cell_released(frame_offset, platform_place_g_program_owner);
    if (ok)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, sample);
            mmgr_line_put(output_line, ": the box C, ");
            mmgr_line_decimal(output_line, box.climbers, 1u);
            mmgr_line_put(output_line, " climbers (every forward and null pair), ");
            mmgr_line_decimal(output_line, box.cells, 1u);
            mmgr_line_put(output_line, " cells (the 27 shifts around each climbed lag and the drift), ");
            mmgr_line_decimal(output_line, box.entries, 1u);
            mmgr_line_put(output_line, " entries (");
            mmgr_line_decimal(output_line, box.crowded, 1u);
            mmgr_line_put(output_line, " cells met more bodies than the fast table and were merged exactly), in ");
            mmgr_line_decimal(output_line, box.microseconds, 1u);
            mmgr_line_put(output_line, " us");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, sample);
            mmgr_line_put(output_line, ": at the climbed lag C sums to the climb's retained count on all but ");
            mmgr_line_decimal(output_line, box.final_score_differ, 1u);
            mmgr_line_put(output_line, "; the climbed lag is highest in its neighborhood on all but ");
            mmgr_line_decimal(output_line, box.not_highest, 1u);
            mmgr_line_put(output_line, "; at the drift C equals the overlap triples on all but ");
            mmgr_line_decimal(output_line, drift_differ, 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal(output_line, drift_queried, 1u);
            mmgr_line_put(output_line, " bodies; T* arranged for ");
            mmgr_line_decimal(output_line, starred, 1u);
            mmgr_line_put(output_line, " bodies, ");
            mmgr_line_decimal(output_line, placed, 1u);
            mmgr_line_put(output_line, " options");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    else
    {
        fprintf(stderr, "  %s: a climbed lag fell outside its own box\n", sample);
        score_stars_release(stars);
    }
    *box_out = ok ? box : *box_out;
    return (ok && (box.final_score_differ == 0u) && (drift_differ == 0u)) ? 1 : 0;
}

static_assert(CLIMB_MACHINE_EXTENT_FIELDS == BOX_HISTORY_EXTENT_FIELDS,
              "score_sample: the climb machine's extents are the box history's boxes");

static void score_box_history_series(const char *role, const TreeFrame *tree, unsigned int leaf,
                                     const unsigned int *counts, unsigned int windows)
{
    // leaf widens from unsigned int to size_t before the multiply; leaf * 6 cannot wrap in 32 bits
    const unsigned int *const extent = &tree->extents[(size_t)leaf * BOX_HISTORY_EXTENT_FIELDS];
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, role);
        mmgr_line_put(output_line, " leaf ");
        mmgr_line_decimal(output_line, leaf, 1u);
        mmgr_line_put(output_line, " (");
        mmgr_line_decimal(output_line, tree->sizes[leaf], 1u);
        mmgr_line_put(output_line, " voxels, box ");
        mmgr_line_decimal(output_line, extent[0], 1u);
        mmgr_line_put(output_line, "..");
        mmgr_line_decimal(output_line, extent[3], 1u);
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal(output_line, extent[1], 1u);
        mmgr_line_put(output_line, "..");
        mmgr_line_decimal(output_line, extent[4], 1u);
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal(output_line, extent[2], 1u);
        mmgr_line_put(output_line, "..");
        mmgr_line_decimal(output_line, extent[5], 1u);
        mmgr_line_put(output_line, "):");
        platform_line_out_ended();
    }
    for (unsigned int window = 0u; window < windows; window += 1u)
    {
        unsigned long long density = 0ull;
        for (unsigned int bit = 0u; bit < ENGINE_HISTORY_BITS; bit += 1u)
        {
            // the count widens from unsigned int to unsigned long long. Shifting it by a history bit stays in 64
            // bits
            density += (unsigned long long)counts[(window * ENGINE_HISTORY_BITS) + bit] << bit;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, density, 1u);
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, ";");
        platform_line_out_ended();
    }
}

static int score_box_history(const char *set, const char *sample, const TreeFrame *frames, unsigned int frame_count,
                             const AnswerKey *key, const int *node_leaf, const int *tree_index_of_time,
                             unsigned int volume_frames)
{
    char path[ENGINE_PATH_CAPACITY];
    EngineHistory history;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &history, .bytes = sizeof(history), .val = 0);
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    if ((engine_sample_path(path, sizeof(path), set, sample, ".oapx") == 0) ||
        (engine_entropy_history_read(path, &history, &error) != 0L))
    {
        fprintf(stderr, "  %s: no entropy history (.oapx) to gather the boxes from\n", sample);
        if (error.type != ENGINE_ERROR_NONE)
        {
            track_error_report("entropy history read", &error);
        }
        return 0;
    }
    unsigned int *const frame_first = (unsigned int *)platform_cell_taken(
        ((size_t)frame_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    int steps_succeeded = (frame_first != NULL);
    unsigned int bodies = 0u;
    for (unsigned int frame = 0u; steps_succeeded && (frame < frame_count); frame += 1u)
    {
        frame_first[frame] = bodies;
        bodies += frames[frame].leaf_count;
        steps_succeeded = (frames[frame].extents != NULL) || (frames[frame].leaf_count == 0u);
    }
    const unsigned int windows = (unsigned int)history.windows;
    unsigned int *const extents = steps_succeeded
                                      ? (unsigned int *)platform_cell_taken(
                                            ((size_t)bodies + 1u) * BOX_HISTORY_EXTENT_FIELDS * sizeof(unsigned int),
                                            platform_place_g_program_owner, 0)
                                      : NULL;
    unsigned int *const counts = steps_succeeded
                                     ? (unsigned int *)platform_cell_taken(
                                           ((size_t)bodies * windows + 1u) * ENGINE_HISTORY_BITS * sizeof(unsigned int),
                                           platform_place_g_program_owner, 0)
                                     : NULL;
    steps_succeeded = steps_succeeded && (extents != NULL) && (counts != NULL);
    for (unsigned int frame = 0u; steps_succeeded && (frame < frame_count); frame += 1u)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &extents[(size_t)frame_first[frame] * BOX_HISTORY_EXTENT_FIELDS],
                   .src = frames[frame].extents,
                   .bytes = (size_t)frames[frame].leaf_count * BOX_HISTORY_EXTENT_FIELDS * sizeof(unsigned int));
    }
    if (steps_succeeded)
    {
        frame_first[frame_count] = bodies;
    }
    unsigned long long disagreements = 0ull;
    unsigned long long microseconds = 0ull;
    const BoxHistoryRequest request = {&history, bodies, extents, counts, &disagreements, &microseconds};
    steps_succeeded = steps_succeeded && (box_history_gather(&request) == 0L);
    if (steps_succeeded)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, sample);
        mmgr_line_put(output_line, ": the box history, ");
        mmgr_line_decimal(output_line, bodies, 1u);
        mmgr_line_put(output_line, " bodies' boxes gathered from ");
        mmgr_line_decimal(output_line, windows, 1u);
        mmgr_line_put(output_line, " windows of ");
        mmgr_line_decimal(output_line, ENGINE_HISTORY_WINDOW, 1u);
        mmgr_line_put(output_line, " transitions, ");
        mmgr_line_decimal(output_line, ENGINE_HISTORY_BITS, 1u);
        mmgr_line_put(output_line, " bits, by 8 corners of each window's running-sum key, in ");
        mmgr_line_decimal(output_line, microseconds, 1u);
        mmgr_line_put(output_line, " us; ");
        mmgr_line_decimal(output_line, disagreements, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, (unsigned long long)bodies * windows * ENGINE_HISTORY_BITS, 1u);
        mmgr_line_put(output_line, " differ from walking every box");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned int *const successors = steps_succeeded ? score_key_successors(key) : NULL;
    steps_succeeded = steps_succeeded && (successors != NULL);
    for (unsigned int node = 0u; steps_succeeded && (node < key->node_count); node += 1u)
    {
        const unsigned int *const node_successors = &successors[3u * node];
        const int time = key->node_coordinates[(size_t)node * 4u];
        if ((node_successors[0] != 2u) || (time < 0) || (((unsigned int)time + 1u) >= volume_frames) ||
            (tree_index_of_time[time] < 0) || (node_leaf[node] < 0))
        {
            continue;
        }
        const unsigned int frame = (unsigned int)tree_index_of_time[time];
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    division at frame ");
            mmgr_line_decimal_signed(output_line, time);
            mmgr_line_put(output_line, ", the transition to ");
            mmgr_line_decimal_signed(output_line, time + 1);
            mmgr_line_put(output_line, " in window ");
            mmgr_line_decimal(output_line, (unsigned int)time / ENGINE_HISTORY_WINDOW, 1u);
            mmgr_line_put(output_line, " (density, the flips times 2^bit, a window each):");
            platform_line_out_ended();
        }
        score_box_history_series(
            "parent", &frames[frame], (unsigned int)node_leaf[node],
            &counts[(size_t)(frame_first[frame] + (unsigned int)node_leaf[node]) * windows * ENGINE_HISTORY_BITS],
            windows);
        for (unsigned int child = 1u; child <= 2u; child += 1u)
        {
            const int child_time = key->node_coordinates[(size_t)node_successors[child] * 4u];
            const int child_leaf = node_leaf[node_successors[child]];
            const int child_frame =
                ((child_time >= 0) && ((unsigned int)child_time < volume_frames)) ? tree_index_of_time[child_time] : -1;
            if ((child_leaf < 0) || (child_frame < 0))
            {
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, " child on no leaf;");
                    platform_line_out_ended();
                }
                continue;
            }
            score_box_history_series(
                "child", &frames[child_frame], (unsigned int)child_leaf,
                &counts[(size_t)(frame_first[child_frame] + (unsigned int)child_leaf) * windows * ENGINE_HISTORY_BITS],
                windows);
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    unsigned int *const leaf_node_count =
        steps_succeeded ? (unsigned int *)platform_cell_cleared((size_t)bodies + 1u, sizeof(unsigned int),
                                                                platform_place_g_program_owner, 0)
                        : NULL;
    steps_succeeded = steps_succeeded && (leaf_node_count != NULL);
    unsigned int placed_nodes = 0u;
    for (unsigned int node = 0u; steps_succeeded && (node < key->node_count); node += 1u)
    {
        const int time = key->node_coordinates[(size_t)node * 4u];
        if ((time < 0) || ((unsigned int)time >= volume_frames) || (tree_index_of_time[time] < 0) ||
            (node_leaf[node] < 0))
        {
            continue;
        }
        leaf_node_count[frame_first[tree_index_of_time[time]] + (unsigned int)node_leaf[node]] += 1u;
        placed_nodes += 1u;
    }
    unsigned int shared_nodes = 0u;
    unsigned int shared_leaves = 0u;
    unsigned int max_nodes_on_leaf = 0u;
    unsigned int max_nodes_leaf_voxels = 0u;
    for (unsigned int frame = 0u; steps_succeeded && (frame < frame_count); frame += 1u)
    {
        for (unsigned int leaf = 0u; leaf < frames[frame].leaf_count; leaf += 1u)
        {
            const unsigned int nodes_on_leaf = leaf_node_count[frame_first[frame] + leaf];
            shared_nodes += (nodes_on_leaf >= 2u) ? nodes_on_leaf : 0u;
            shared_leaves += (nodes_on_leaf >= 2u) ? 1u : 0u;
            max_nodes_leaf_voxels =
                (nodes_on_leaf > max_nodes_on_leaf) ? frames[frame].sizes[leaf] : max_nodes_leaf_voxels;
            max_nodes_on_leaf = (nodes_on_leaf > max_nodes_on_leaf) ? nodes_on_leaf : max_nodes_on_leaf;
        }
    }
    if (steps_succeeded)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, sample);
        mmgr_line_put(output_line, ": of ");
        mmgr_line_decimal(output_line, placed_nodes, 1u);
        mmgr_line_put(output_line, " key nodes on a leaf, ");
        mmgr_line_decimal(output_line, shared_nodes, 1u);
        mmgr_line_put(output_line, " share their leaf with another key node, on ");
        mmgr_line_decimal(output_line, shared_leaves, 1u);
        mmgr_line_put(output_line, " leaves; the max on one leaf is ");
        mmgr_line_decimal(output_line, max_nodes_on_leaf, 1u);
        mmgr_line_put(output_line, ", a leaf of ");
        mmgr_line_decimal(output_line, max_nodes_leaf_voxels, 1u);
        mmgr_line_put(output_line, " voxels");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(leaf_node_count, platform_place_g_program_owner);
    if (steps_succeeded == 0)
    {
        fprintf(stderr, "  %s: the box history errored\n", sample);
    }
    platform_cell_released(successors, platform_place_g_program_owner);
    platform_cell_released(counts, platform_place_g_program_owner);
    platform_cell_released(extents, platform_place_g_program_owner);
    platform_cell_released(frame_first, platform_place_g_program_owner);
    engine_entropy_history_release(&history);
    return (steps_succeeded && (disagreements == 0ull)) ? 1 : 0;
}

static const unsigned long long *score_core_fields(const ClimbMachineCore *core, unsigned int climber,
                                                   unsigned int width)
{
    const unsigned int width_first = core->width_first[climber];
    const unsigned int width_count = core->width_first[climber + 1u] - width_first;
    return (width < width_count) ? &core->fields[(size_t)(width_first + width) * CLIMB_MACHINE_CORE_FIELDS] : NULL;
}

static int score_core(const char *sample, ClimbMachine *machine, const ClimbMachineBox *box)
{
    ClimbMachineCore core;
    if (climb_machine_core(machine, &core) == 0)
    {
        fprintf(stderr, "  %s: the core errored\n", sample);
        return 0;
    }
    unsigned int max_widths = 0u;
    for (unsigned int climber = 0u; climber < core.climbers; climber += 1u)
    {
        const unsigned int width_count = core.width_first[climber + 1u] - core.width_first[climber];
        max_widths = (width_count > max_widths) ? width_count : max_widths;
    }
    const size_t capacity = (size_t)max_widths + 1u;
    unsigned long long *const climbers_at = (unsigned long long *)platform_cell_cleared(
        capacity, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    unsigned long long *const mass_at = (unsigned long long *)platform_cell_cleared(
        capacity, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    unsigned long long *const pairs_at = (unsigned long long *)platform_cell_cleared(
        capacity, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    unsigned long long *const within_half_voxel_at = (unsigned long long *)platform_cell_cleared(
        capacity, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    if ((climbers_at == NULL) || (mass_at == NULL) || (pairs_at == NULL) || (within_half_voxel_at == NULL))
    {
        platform_cell_released(climbers_at, platform_place_g_program_owner);
        platform_cell_released(mass_at, platform_place_g_program_owner);
        platform_cell_released(pairs_at, platform_place_g_program_owner);
        platform_cell_released(within_half_voxel_at, platform_place_g_program_owner);
        return 0;
    }
    unsigned int proved = 0u;
    unsigned int differing_climbers = 0u;
    unsigned int unboxed = 0u;
    unsigned int box_cursor = 0u;
    unsigned int pair_start = 0u;
    unsigned int earlier_count = 0u;
    for (unsigned int climber = 0u; climber < core.climbers; climber += 1u)
    {
        const unsigned int widths = core.width_first[climber + 1u] - core.width_first[climber];
        for (unsigned int width = 0u; width < widths; width += 1u)
        {
            climbers_at[width] += 1ull;
            mass_at[width] += score_core_fields(&core, climber, width)[0];
        }
        const int new_pair = (climber == 0u) || (core.earlier[climber] != core.earlier[climber - 1u]) ||
                             (core.later[climber] != core.later[climber - 1u]);
        if (new_pair)
        {
            pair_start = climber;
            earlier_count = 0u;
            while (((pair_start + earlier_count) < core.climbers) && (core.side[pair_start + earlier_count] == 0u) &&
                   (core.earlier[pair_start + earlier_count] == core.earlier[climber]) &&
                   (core.later[pair_start + earlier_count] == core.later[climber]))
            {
                earlier_count += 1u;
            }
        }
        const unsigned int match = core.match[climber];
        if ((core.side[climber] != 0u) || (match == CLIMB_MACHINE_BOX_NONE))
        {
            continue;
        }
        unsigned int box_place = box_cursor;
        while ((box != NULL) && (box_place < box->climbers) &&
               ((box->earlier[box_place] != core.earlier[climber]) || (box->later[box_place] != core.later[climber]) ||
                (box->leaf[box_place] != core.leaf[climber])))
        {
            box_place += 1u;
        }
        if ((box != NULL) && (box_place < box->climbers))
        {
            box_cursor = box_place;
            const unsigned int cell = box->cell_first[box_place] + CLIMB_MACHINE_BOX_CENTER;
            unsigned long long box_count_at_match = 0ull;
            for (unsigned int entry = box->entry_first[cell]; entry < box->entry_first[cell + 1u]; entry += 1u)
            {
                box_count_at_match += (box->target[entry] == match) ? box->count[entry] : 0ull;
            }
            const unsigned long long *const width_zero_fields = score_core_fields(&core, climber, 0u);
            differing_climbers +=
                (box_count_at_match != ((width_zero_fields != NULL) ? width_zero_fields[0] : 0ull)) ? 1u : 0u;
            proved += 1u;
        }
        else
        {
            unboxed += 1u;
        }
        const unsigned int partner = pair_start + earlier_count + match;
        if ((partner >= core.climbers) || (core.side[partner] != 1u) || (core.leaf[partner] != match) ||
            (core.earlier[partner] != core.earlier[climber]) || (core.later[partner] != core.later[climber]) ||
            (core.match[partner] != core.leaf[climber]))
        {
            continue;
        }
        const int *const lag = &core.lag[ENGINE_AXES * climber];
        for (unsigned int width = 0u; width < max_widths; width += 1u)
        {
            const unsigned long long *const forward = score_core_fields(&core, climber, width);
            const unsigned long long *const backward = score_core_fields(&core, partner, width);
            if ((forward == NULL) || (backward == NULL))
            {
                break;
            }
            pairs_at[width] += 1ull;
            const long long mass_product = (long long)(forward[0] * backward[0]);
            unsigned int within_half_voxel = 1u;
            for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
            {
                const long long displacement_scaled =
                    (long long)(forward[0] * backward[1u + axis]) - (long long)(backward[0] * forward[1u + axis]);
                const long long lag_error_doubled = 2ll * (displacement_scaled - ((long long)lag[axis] * mass_product));
                within_half_voxel *=
                    (((lag_error_doubled < 0ll) ? -lag_error_doubled : lag_error_doubled) <= mass_product) ? 1u : 0u;
            }
            within_half_voxel_at[width] += within_half_voxel;
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, sample);
        mmgr_line_put(output_line, ": the core, ");
        mmgr_line_decimal(output_line, core.climbers, 1u);
        mmgr_line_put(output_line, " climbers on adjacent frames (both sides), ");
        mmgr_line_decimal(output_line, core.widths, 1u);
        mmgr_line_put(output_line, " widths retained, in ");
        mmgr_line_decimal(output_line, core.microseconds, 1u);
        mmgr_line_put(output_line, " us; the width-0 core mass equals C at the climbed lag on all but ");
        mmgr_line_decimal(output_line, differing_climbers, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, proved, 1u);
        mmgr_line_put(output_line, " forward climbers (");
        mmgr_line_decimal(output_line, unboxed, 1u);
        mmgr_line_put(output_line, " not in a box)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int width = 0u; width < max_widths; width += 1u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    width ");
        mmgr_line_decimal(output_line, width, 1u);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, climbers_at[width], 1u);
        mmgr_line_put(output_line, " climbers reach it, core mass ");
        mmgr_line_decimal(output_line, mass_at[width], 1u);
        mmgr_line_put(output_line, "; ");
        mmgr_line_decimal(output_line, pairs_at[width], 1u);
        mmgr_line_put(output_line, " mutual pairs contain both cores, the core displacement within half a voxel of the "
                                   "climbed lag on every axis for ");
        mmgr_line_decimal(output_line, within_half_voxel_at[width], 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_cell_released(climbers_at, platform_place_g_program_owner);
    platform_cell_released(mass_at, platform_place_g_program_owner);
    platform_cell_released(pairs_at, platform_place_g_program_owner);
    platform_cell_released(within_half_voxel_at, platform_place_g_program_owner);
    return (differing_climbers == 0u) ? 1 : 0;
}

static int score_marginal(const char *sample, const TreeFrame *frames, unsigned int frame_count, const AnswerKey *key,
                          const int *node_leaf, const int *tree_index_of_time, unsigned int volume_frames,
                          const ScoreChoices *stars)
{
    ScoreMarginalSet set;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &set, .bytes = sizeof(set), .val = 0);
    unsigned int leaves = 0u;
    for (unsigned int frame = 0u; frame < frame_count; frame += 1u)
    {
        leaves += frames[frame].leaf_count;
    }
    set.questions = (MarginalQuestion *)platform_cell_taken(((size_t)leaves + 1u) * sizeof(MarginalQuestion),
                                                            platform_place_g_program_owner, 0);
    set.queried_leaf = (unsigned int *)platform_cell_taken(((size_t)leaves + 1u) * sizeof(unsigned int),
                                                           platform_place_g_program_owner, 0);
    set.queried_frame = (unsigned int *)platform_cell_taken(((size_t)leaves + 1u) * sizeof(unsigned int),
                                                            platform_place_g_program_owner, 0);
    unsigned int **const question_of = (unsigned int **)platform_cell_cleared(
        (size_t)frame_count + 1u, sizeof(unsigned int *), platform_place_g_program_owner, 0);
    int ok =
        (set.questions != NULL) && (set.queried_leaf != NULL) && (set.queried_frame != NULL) && (question_of != NULL);
    unsigned int in_context = 0u;
    unsigned int alone = 0u;
    unsigned int error = 0u;
    for (unsigned int frame = 0u; ok && (frame < frame_count); frame += 1u)
    {
        const TreeFrame *const tree = &frames[frame];
        const ScoreChoices choices = score_choices_of(frames, stars, frame);
        if ((choices.start == NULL) || (tree->forward == NULL) || ((frame + 1u) >= frame_count))
        {
            continue;
        }
        if (tree->null_final_score == NULL)
        {
            fprintf(stderr, "  %s: the marginal needs each body's null reading; run with --null or a floor\n", sample);
            ok = 0;
            break;
        }
        const unsigned int targets = frames[frame + 1u].leaf_count;
        unsigned int *const target_start = (unsigned int *)platform_cell_cleared(
            (size_t)targets + 2u, sizeof(unsigned int), platform_place_g_program_owner, 0);
        unsigned int *const target_sources = (unsigned int *)platform_cell_taken(
            ((size_t)choices.count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
        question_of[frame] = (unsigned int *)platform_cell_taken(((size_t)tree->leaf_count + 1u) * sizeof(unsigned int),
                                                                 platform_place_g_program_owner, 0);
        ok = (target_start != NULL) && (target_sources != NULL) && (question_of[frame] != NULL);
        for (unsigned int triple = 0u; ok && (triple < choices.count); triple += 1u)
        {
            target_start[choices.target[triple] + 1u] += 1u;
        }
        for (unsigned int target = 0u; ok && (target < targets); target += 1u)
        {
            target_start[target + 1u] += target_start[target];
        }
        for (unsigned int leaf = 0u; ok && (leaf < tree->leaf_count); leaf += 1u)
        {
            for (unsigned int triple = choices.start[leaf]; triple < choices.start[leaf + 1u]; triple += 1u)
            {
                const unsigned int target = choices.target[triple];
                target_sources[target_start[target]] = leaf;
                target_start[target] += 1u;
            }
        }
        for (unsigned int target = targets; ok && (target > 0u); target -= 1u)
        {
            target_start[target] = target_start[target - 1u];
        }
        if (ok)
        {
            target_start[0] = 0u;
        }
        for (unsigned int leaf = 0u; ok && (leaf < tree->leaf_count); leaf += 1u)
        {
            question_of[frame][leaf] = 0xFFFFFFFFu;
            if (choices.start[leaf + 1u] == choices.start[leaf])
            {
                continue;
            }
            int made = score_marginal_query(&set, tree, &choices, target_start, target_sources, leaf, 0);
            in_context += (made > 0) ? 1u : 0u;
            if (made == 0)
            {
                made = score_marginal_query(&set, tree, &choices, target_start, target_sources, leaf, 1);
                alone += (made > 0) ? 1u : 0u;
                error += (made == 0) ? 1u : 0u;
            }
            ok = (made >= 0);
            if (made > 0)
            {
                question_of[frame][leaf] = set.question_count;
                set.queried_leaf[set.question_count] = leaf;
                set.queried_frame[set.question_count] = frame;
                set.question_count += 1u;
            }
        }
        platform_cell_released(target_start, platform_place_g_program_owner);
        platform_cell_released(target_sources, platform_place_g_program_owner);
    }
    const size_t question_words = (size_t)set.question_count * MARGINAL_SUMS * MARGINAL_LIMBS;
    const size_t option_words = (size_t)set.option_count * MARGINAL_LIMBS;
    unsigned int *const device_questions =
        ok ? (unsigned int *)platform_cell_taken((question_words + 1u) * sizeof(unsigned int),
                                                 platform_place_g_program_owner, 0)
           : NULL;
    unsigned int *const device_options =
        ok ? (unsigned int *)platform_cell_taken((option_words + 1u) * sizeof(unsigned int),
                                                 platform_place_g_program_owner, 0)
           : NULL;
    unsigned int *const host_questions =
        ok ? (unsigned int *)platform_cell_taken((question_words + 1u) * sizeof(unsigned int),
                                                 platform_place_g_program_owner, 0)
           : NULL;
    unsigned int *const host_options =
        ok ? (unsigned int *)platform_cell_taken((option_words + 1u) * sizeof(unsigned int),
                                                 platform_place_g_program_owner, 0)
           : NULL;
    ok = ok && (device_questions != NULL) && (device_options != NULL) && (host_questions != NULL) &&
         (host_options != NULL);
    const MarginalRequest device = {set.questions, set.question_count, set.sources,      set.source_count,
                                    set.options,   set.option_count,   device_questions, device_options};
    const MarginalRequest host = {set.questions, set.question_count, set.sources,    set.source_count,
                                  set.options,   set.option_count,   host_questions, host_options};
    const unsigned long long started = engine_clock_microseconds();
    ok = ok && (marginal_run(&device) == (long)set.question_count);
    const unsigned long long swept = engine_clock_microseconds() - started;
    ok = ok && (marginal_run_host(&host) == (long)set.question_count);
    const int same = ok &&
                     (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device_questions, .other = host_questions,
                                 .bytes = question_words * sizeof(unsigned int)) == 0) &&
                     (EMBED_CALL(memor.cmp, MemoriaCfg, .src = device_options, .other = host_options,
                                 .bytes = option_words * sizeof(unsigned int)) == 0);
    unsigned int edges = 0u;
    unsigned int by_context = 0u;
    unsigned int by_absent = 0u;
    unsigned int by_weight = 0u;
    unsigned int by_climb = 0u;
    for (unsigned int edge = 0u; ok && (edge < key->edge_count); edge += 1u)
    {
        const long source = node_index_of(key, key->edge_ends[2u * edge]);
        const long target = node_index_of(key, key->edge_ends[(2u * edge) + 1u]);
        if ((source < 0L) || (target < 0L))
        {
            continue;
        }
        const int time = key->node_coordinates[(size_t)source * 4u];
        const int next_time = key->node_coordinates[(size_t)target * 4u];
        if ((time < 0) || (next_time != time + 1) || ((unsigned int)next_time >= volume_frames) ||
            (tree_index_of_time[time] < 0) || (node_leaf[source] < 0) || (node_leaf[target] < 0))
        {
            continue;
        }
        const unsigned int frame = (unsigned int)tree_index_of_time[time];
        const unsigned int leaf = (unsigned int)node_leaf[source];
        const unsigned int truth = (unsigned int)node_leaf[target];
        const unsigned int question = (question_of[frame] != NULL) ? question_of[frame][leaf] : 0xFFFFFFFFu;
        if (question == 0xFFFFFFFFu)
        {
            continue;
        }
        edges += 1u;
        const TreeFrame *const tree = &frames[frame];
        const ScoreChoices choices = score_choices_of(frames, stars, frame);
        const MarginalSource *const own = &set.sources[set.questions[question].source_first];
        const unsigned int *best =
            &device_questions[((size_t)question * MARGINAL_SUMS + MARGINAL_ABSENT) * MARGINAL_LIMBS];
        int context_leaf = -1;
        unsigned int heaviest = 0u;
        int weight_leaf = -1;
        for (unsigned int option = 0u; option < own->options; option += 1u)
        {
            const unsigned int triple = choices.start[leaf] + option;
            const unsigned int *const sum = &device_options[(size_t)(own->option_first + option) * MARGINAL_LIMBS];
            const int ahead = (score_limbs_order(sum, best) > 0);
            best = ahead ? sum : best;
            context_leaf = ahead ? (int)choices.target[triple] : context_leaf;
            weight_leaf = (choices.weight[triple] > heaviest) ? (int)choices.target[triple] : weight_leaf;
            heaviest = (choices.weight[triple] > heaviest) ? choices.weight[triple] : heaviest;
        }
        by_context += (context_leaf == (int)truth) ? 1u : 0u;
        by_absent += (context_leaf < 0) ? 1u : 0u;
        by_weight += (weight_leaf == (int)truth) ? 1u : 0u;
        by_climb += (tree->forward[leaf] == (int)truth) ? 1u : 0u;
    }
    if (ok)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, sample);
            mmgr_line_put(output_line, ": marginal, ");
            mmgr_line_decimal(output_line, set.question_count, 1u);
            mmgr_line_put(output_line, " bodies queried (");
            mmgr_line_decimal(output_line, in_context, 1u);
            mmgr_line_put(output_line, " in their context, ");
            mmgr_line_decimal(output_line, alone, 1u);
            mmgr_line_put(output_line, " alone, ");
            mmgr_line_decimal(output_line, error, 1u);
            mmgr_line_put(output_line, " too wide to query), ");
            mmgr_line_decimal(output_line, set.option_count, 1u);
            mmgr_line_put(output_line, " options, in ");
            mmgr_line_decimal(output_line, swept, 1u);
            mmgr_line_put(output_line, " us; device ");
            mmgr_line_put(output_line, same ? "equals" : "DIFFERS FROM");
            mmgr_line_put(output_line, " the host");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, sample);
            mmgr_line_put(output_line, ": of ");
            mmgr_line_decimal(output_line, edges, 1u);
            mmgr_line_put(output_line, " key edges queried, the max probable in context lands on the truth for ");
            mmgr_line_decimal(output_line, by_context, 1u);
            mmgr_line_put(output_line, " (no link is the max probable for ");
            mmgr_line_decimal(output_line, by_absent, 1u);
            mmgr_line_put(output_line, "), the heaviest candidate alone for ");
            mmgr_line_decimal(output_line, by_weight, 1u);
            mmgr_line_put(output_line, ", the climb's forward for ");
            mmgr_line_decimal(output_line, by_climb, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    else
    {
        fprintf(stderr, "  %s: the marginal errored\n", sample);
    }
    for (unsigned int frame = 0u; (question_of != NULL) && (frame < frame_count); frame += 1u)
    {
        platform_cell_released(question_of[frame], platform_place_g_program_owner);
    }
    platform_cell_released(question_of, platform_place_g_program_owner);
    platform_cell_released(set.questions, platform_place_g_program_owner);
    platform_cell_released(set.sources, platform_place_g_program_owner);
    platform_cell_released(set.options, platform_place_g_program_owner);
    platform_cell_released(set.queried_leaf, platform_place_g_program_owner);
    platform_cell_released(set.queried_frame, platform_place_g_program_owner);
    platform_cell_released(device_questions, platform_place_g_program_owner);
    platform_cell_released(device_options, platform_place_g_program_owner);
    platform_cell_released(host_questions, platform_place_g_program_owner);
    platform_cell_released(host_options, platform_place_g_program_owner);
    return (ok && same) ? 1 : 0;
}

static int score_contact_side(const char *sample, const TreeFrame *frames, unsigned int frame_count,
                              const TreeRules *rules)
{
    unsigned long long first = 0ull;
    unsigned long long bodies = 0ull;
    if (score_flattened_slice(sample, frames, frame_count, rules, &first, &bodies) == 0)
    {
        return 0;
    }
    size_t capacity = 0u;
    for (unsigned int frame = 0u; frame < frame_count; frame += 1u)
    {
        capacity += (size_t)frames[frame].joined_count;
    }
    unsigned int *const frame_first = (unsigned int *)platform_cell_taken(
        ((size_t)frame_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const pairs = (unsigned int *)platform_cell_taken(
        ((capacity * 2u * CONTACT_SIDE_MEMBERS) + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const sides = (unsigned int *)platform_cell_taken(
        ((capacity * CONTACT_SIDE_MEMBERS) + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    int ok = (frame_first != NULL) && (pairs != NULL) && (sides != NULL) && ((capacity * 2u) <= 0x3FFFFFFFu);
    if (ok)
    {
        frame_first[0] = 0u;
    }
    for (unsigned int frame = 0u; ok && (frame < frame_count); frame += 1u)
    {
        frame_first[frame + 1u] = frame_first[frame] + frames[frame].leaf_count;
    }
    unsigned int contacts = 0u;
    unsigned int landed_together = 0u;
    for (unsigned int frame = 0u; ok && ((frame + 1u) < frame_count); frame += 1u)
    {
        const TreeFrame *const tree = &frames[frame];
        for (unsigned int pair = 0u; (tree->forward != NULL) && (pair < tree->joined_count); pair += 1u)
        {
            const unsigned int one = tree->joined[2u * pair];
            const unsigned int other = tree->joined[(2u * pair) + 1u];
            const int one_after = tree->forward[one];
            const int other_after = tree->forward[other];
            if ((one_after < 0) || (other_after < 0))
            {
                continue;
            }
            unsigned int *const lane = &pairs[(size_t)contacts * 2u * CONTACT_SIDE_MEMBERS];
            lane[0] = frame_first[frame] + one;
            lane[1] = frame_first[frame] + other;
            lane[2] = frame_first[frame + 1u] + (unsigned int)one_after;
            lane[3] = frame_first[frame + 1u] + (unsigned int)other_after;
            sides[(size_t)contacts * CONTACT_SIDE_MEMBERS] = 2u * contacts;
            sides[((size_t)contacts * CONTACT_SIDE_MEMBERS) + 1u] = (2u * contacts) + 1u;
            landed_together += (one_after == other_after) ? 1u : 0u;
            contacts += 1u;
        }
    }
    const unsigned int *const magnitudes = &rules->flattened[first * rules->flattened_limbs];
    const unsigned int apart_limbs = cycle_record_out_limbs(rules->contact_difference_record);
    const unsigned int retained_limbs = cycle_record_out_limbs(rules->contact_retained_record);
    const unsigned int differences = 2u * contacts;
    const size_t apart_words = (size_t)differences * apart_limbs;
    const size_t retained_words = (size_t)contacts * retained_limbs;
    unsigned int *const apart_device =
        ok ? (unsigned int *)platform_cell_taken((apart_words + 1u) * sizeof(unsigned int),
                                                 platform_place_g_program_owner, 0)
           : NULL;
    unsigned int *const apart_host = ok ? (unsigned int *)platform_cell_taken((apart_words + 1u) * sizeof(unsigned int),
                                                                              platform_place_g_program_owner, 0)
                                        : NULL;
    unsigned int *const retained_device =
        ok ? (unsigned int *)platform_cell_taken((retained_words + 1u) * sizeof(unsigned int),
                                                 platform_place_g_program_owner, 0)
           : NULL;
    unsigned int *const retained_host =
        ok ? (unsigned int *)platform_cell_taken((retained_words + 1u) * sizeof(unsigned int),
                                                 platform_place_g_program_owner, 0)
           : NULL;
    ok = ok && (apart_device != NULL) && (apart_host != NULL) && (retained_device != NULL) && (retained_host != NULL);
    unsigned long long apart_sweep = 0ull;
    unsigned long long retained_sweep = 0ull;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const EngineRecordSweep apart_run = {rules->contact_difference_record,
                                         {magnitudes, magnitudes},
                                         {bodies, bodies},
                                         pairs,
                                         differences,
                                         apart_device,
                                         &apart_sweep,
                                         &error};
    const EngineRecordSweep apart_host_run = {
        NULL, {magnitudes, magnitudes}, {bodies, bodies}, pairs, differences, apart_host, NULL, &error};
    const EngineRecordSweep retained_run = {rules->contact_retained_record,
                                            {apart_device, apart_device},
                                            {differences, differences},
                                            sides,
                                            contacts,
                                            retained_device,
                                            &retained_sweep,
                                            &error};
    const EngineRecordSweep retained_host_run = {
        NULL, {apart_device, apart_device}, {differences, differences}, sides, contacts, retained_host, NULL, &error};
    ok = ok && ((contacts == 0u) ||
                ((engine_record_sweep(&apart_run) == (long)differences) &&
                 (engine_record_host(rules->contact_difference_encode_request, &apart_host_run) == (long)differences) &&
                 (engine_record_sweep(&retained_run) == (long)contacts) &&
                 (engine_record_host(rules->contact_retained_encode_request, &retained_host_run) == (long)contacts)));
    if (error.type != ENGINE_ERROR_NONE)
    {
        track_error_report("contact side sweep", &error);
    }
    const unsigned int differ = ok ? ((EMBED_CALL(memor.cmp, MemoriaCfg, .src = apart_device, .other = apart_host,
                                                  .bytes = apart_words * sizeof(unsigned int)) != 0)
                                          ? 1u
                                          : 0u)
                                   : 0u;
    const unsigned int *const offset = ok ? rules->contact_retained_encode_request->output_offset : NULL;
    const unsigned int *const bits = ok ? rules->contact_retained_encode_request->output_bits : NULL;
    unsigned int retained = 0u;
    unsigned int crossed = 0u;
    unsigned int retained_differ = 0u;
    for (unsigned int contact = 0u; ok && (contact < contacts); contact += 1u)
    {
        const unsigned int *const record = &retained_device[(size_t)contact * retained_limbs];
        retained += (unsigned int)score_field_truthy(record, offset[CONTACT_SIDE_SAME], bits[CONTACT_SIDE_SAME]);
        crossed += (unsigned int)score_field_truthy(record, offset[CONTACT_SIDE_CROSSED], bits[CONTACT_SIDE_CROSSED]);
        retained_differ +=
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = record, .other = &retained_host[(size_t)contact * retained_limbs],
                        .bytes = retained_limbs * sizeof(unsigned int)) != 0)
                ? 1u
                : 0u;
    }
    if (ok)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, sample);
        mmgr_line_put(output_line, ": contact side, ");
        mmgr_line_decimal(output_line, contacts, 1u);
        mmgr_line_put(output_line, " touching pairs whose bodies both climb onward (");
        mmgr_line_decimal(output_line, differences, 1u);
        mmgr_line_put(output_line, " differences in ");
        mmgr_line_decimal(output_line, apart_sweep, 1u);
        mmgr_line_put(output_line, " us, the verdicts in ");
        mmgr_line_decimal(output_line, retained_sweep, 1u);
        mmgr_line_put(output_line, " us): the centroid difference retains its side on ");
        mmgr_line_decimal(output_line, retained, 1u);
        mmgr_line_put(output_line, ", crosses on ");
        mmgr_line_decimal(output_line, crossed, 1u);
        mmgr_line_put(output_line, ", meets on ");
        mmgr_line_decimal(output_line, contacts - retained - crossed, 1u);
        mmgr_line_put(output_line, " (");
        mmgr_line_decimal(output_line, landed_together, 1u);
        mmgr_line_put(output_line, " of them landed on one body); the differences ");
        mmgr_line_put(output_line, (differ == 0u) ? "equal" : "DIFFER FROM");
        mmgr_line_put(output_line, " the host, ");
        mmgr_line_decimal(output_line, retained_differ, 1u);
        mmgr_line_put(output_line, " verdicts differ from it");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        fprintf(stderr, "  %s: the contact side sweeps errored\n", sample);
    }
    platform_cell_released(frame_first, platform_place_g_program_owner);
    platform_cell_released(pairs, platform_place_g_program_owner);
    platform_cell_released(sides, platform_place_g_program_owner);
    platform_cell_released(apart_device, platform_place_g_program_owner);
    platform_cell_released(apart_host, platform_place_g_program_owner);
    platform_cell_released(retained_device, platform_place_g_program_owner);
    platform_cell_released(retained_host, platform_place_g_program_owner);
    return (ok && (differ == 0u) && (retained_differ == 0u)) ? 1 : 0;
}

static unsigned long long s_web_logged[5] = {0ull, 0ull, 0ull, 0ull, 0ull};

static int score_truth_load(const char *source, const char *sample, AnswerKey *key)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = key, .bytes = sizeof(*key), .val = 0);
    if (source == NULL)
    {
        return 1;
    }
    char path[ENGINE_PATH_CAPACITY];
    const int written = snprintf(path, sizeof(path), "%s/%s.geff", source, sample);
    if ((written <= 0) || ((size_t)written >= sizeof(path)))
    {
        fprintf(stderr, "  %s: %s/%s.geff is longer than a path may be\n", sample, source, sample);
        return 0;
    }
    EngineGeff geff;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &geff, .bytes = sizeof(geff), .val = 0);
    if (engine_geff_read(path, &geff) != 0L)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, sample);
            mmgr_line_put(output_line, ": no answer key at ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, "; run unscored");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    AnswerKeyTruth truth;
    truth.nodes = geff.nodes;
    truth.edges = geff.edges;
    truth.node_identity = geff.node_identity;
    truth.node_place = geff.node_place;
    truth.edge_ends = geff.edge_ends;
    const int ok = answer_key_build(&truth, key);
    engine_geff_release(&geff);
    if (ok == 0)
    {
        fprintf(stderr, "  %s: the answer key at %s is not valid: ids past 63 bits, places off the grid or ids twice\n",
                sample, path);
    }
    return ok;
}

int score_sample(const char *set, const char *source, const char *sample, const TreeRules *rules, EdgeResults *results)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = results, .bytes = sizeof(*results), .val = 0);
    AnswerKey key;
    if (score_truth_load(source, sample, &key) == 0)
    {
        return 0;
    }
    unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
    unsigned short *volume = NULL;
    EngineSignum volume_root;
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    if ((engine_iapx_load(set, sample, extent, &volume, &volume_root, NULL, &error) != 0L) ||
        (extent[0] > 0xFFFFFFFFull) || (extent[1] > 0xFFFFFFFFull) || (extent[2] > 0xFFFFFFFFull) ||
        (extent[3] > 0xFFFFFFFFull))
    {
        fprintf(stderr, "  %s: its .iapx in %s did not load and prove\n", sample, set);
        if (error.type != ENGINE_ERROR_NONE)
        {
            track_error_report("iapx load", &error);
        }
        platform_cell_released(volume, platform_place_g_program_owner);
        release_answer_key(&key);
        return 0;
    }
    const unsigned int header[4] = {(unsigned int)extent[0], (unsigned int)extent[1], (unsigned int)extent[2],
                                    (unsigned int)extent[3]};
    const unsigned int volume_frames = header[0];

    unsigned char *const reached =
        (unsigned char *)platform_cell_cleared((size_t)volume_frames + 1u, 1u, platform_place_g_program_owner, 0);
    int *const tree_index_of_time =
        (int *)platform_cell_taken(((size_t)volume_frames + 1u) * sizeof(int), platform_place_g_program_owner, 0);
    if ((reached == NULL) || (tree_index_of_time == NULL))
    {
        platform_cell_released(volume, platform_place_g_program_owner);
        platform_cell_released(reached, platform_place_g_program_owner);
        platform_cell_released(tree_index_of_time, platform_place_g_program_owner);
        release_answer_key(&key);
        return 0;
    }
    for (unsigned int time = 0u; time < volume_frames; time += 1u)
    {
        reached[time] = 1u;
    }
    unsigned int frame_count = 0u;
    for (unsigned int time = 0u; time < volume_frames; time += 1u)
    {
        tree_index_of_time[time] = (reached[time] != 0u) ? (int)frame_count : -1;
        frame_count += (reached[time] != 0u) ? 1u : 0u;
    }
    TreeFrame *const frames = (TreeFrame *)platform_cell_cleared((size_t)frame_count + 1u, sizeof(TreeFrame),
                                                                 platform_place_g_program_owner, 0);
    if (frames != NULL)
    {
        unsigned int written = 0u;
        for (unsigned int time = 0u; time < volume_frames; time += 1u)
        {
            if (reached[time] != 0u)
            {
                frames[written].time = time;
                written += 1u;
            }
        }
    }

    EngineBuffers buffers;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &buffers, .bytes = sizeof(buffers), .val = 0);
    buffers.unit_sweep = rules->unit_sweep;
    buffers.depth = header[1];
    buffers.height = header[2];
    buffers.width = header[3];
    const size_t voxels = (size_t)buffers.depth * buffers.height * buffers.width;
    buffers.peak_capacity = ((buffers.depth + 1u) / 2u) * ((buffers.height + 1u) / 2u) * ((buffers.width + 1u) / 2u);
    buffers.pair_capacity = 1u << 18u;
    buffers.overlap_capacity = 1u << 16u;
    buffers.volume =
        (unsigned short *)platform_cell_taken(voxels * sizeof(unsigned short), platform_place_g_program_owner, 0);
    buffers.peak_indices = (unsigned int *)platform_cell_taken((size_t)buffers.peak_capacity * sizeof(unsigned int),
                                                               platform_place_g_program_owner, 0);
    buffers.sizes = (unsigned int *)platform_cell_taken((size_t)buffers.peak_capacity * sizeof(unsigned int),
                                                        platform_place_g_program_owner, 0);
    buffers.sums = (unsigned long long *)platform_cell_taken(
        (size_t)buffers.peak_capacity * 3u * sizeof(unsigned long long), platform_place_g_program_owner, 0);
    buffers.peak_limbs = (unsigned int *)platform_cell_taken((size_t)buffers.peak_capacity * ENGINE_RESIDUAL_LIMBS *
                                                                 sizeof(unsigned int),
                                                             platform_place_g_program_owner, 0);
    buffers.bodies = (EngineBody *)platform_cell_taken((size_t)buffers.peak_capacity * sizeof(EngineBody),
                                                       platform_place_g_program_owner, 0);
    buffers.adjacency = (unsigned int *)platform_cell_taken((size_t)buffers.pair_capacity * 2u * sizeof(unsigned int),
                                                            platform_place_g_program_owner, 0);
    buffers.joined = (unsigned int *)platform_cell_taken((size_t)buffers.pair_capacity * 2u * sizeof(unsigned int),
                                                         platform_place_g_program_owner, 0);
    buffers.overlap_before = (unsigned int *)platform_cell_taken(
        (size_t)buffers.overlap_capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    buffers.overlap_after = (unsigned int *)platform_cell_taken((size_t)buffers.overlap_capacity * sizeof(unsigned int),
                                                                platform_place_g_program_owner, 0);
    buffers.overlap_shared = (unsigned int *)platform_cell_taken(
        (size_t)buffers.overlap_capacity * sizeof(unsigned int), platform_place_g_program_owner, 0);
    for (unsigned int target_at = 0u; target_at < 2u; target_at += 1u)
    {
        buffers.labels[target_at] =
            (unsigned int *)platform_cell_taken(voxels * sizeof(unsigned int), platform_place_g_program_owner, 0);
        buffers.positive[target_at] = (unsigned long long *)platform_cell_taken(
            ((voxels + 63u) / 64u) * sizeof(unsigned long long), platform_place_g_program_owner, 0);
        if ((rules->climb != 0) || (rules->cast != 0) || (rules->parallax != 0))
        {
            buffers.leaf_at_peak[target_at] =
                (int *)platform_cell_taken(voxels * sizeof(int), platform_place_g_program_owner, 0);
            buffers.leaf_start[target_at] = (unsigned int *)platform_cell_taken(
                ((size_t)buffers.peak_capacity + 2u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
            buffers.leaf_voxels[target_at] =
                (unsigned int *)platform_cell_taken(voxels * sizeof(unsigned int), platform_place_g_program_owner, 0);
            for (size_t voxel = 0u; (buffers.leaf_at_peak[target_at] != NULL) && (voxel < voxels); voxel += 1u)
            {
                buffers.leaf_at_peak[target_at][voxel] = -1;
            }
        }
    }

    int *const node_leaf =
        (int *)platform_cell_taken(((size_t)key.node_count + 1u) * sizeof(int), platform_place_g_program_owner, 0);
    if (node_leaf != NULL)
    {
        for (unsigned int node = 0u; node < key.node_count; node += 1u)
        {
            node_leaf[node] = -1;
        }
    }

    int ok = (frames != NULL) && (node_leaf != NULL) && (buffers.volume != NULL) && (buffers.peak_indices != NULL) &&
             (buffers.sizes != NULL) && (buffers.sums != NULL) && (buffers.peak_limbs != NULL) &&
             (buffers.adjacency != NULL) && (buffers.joined != NULL) && (buffers.overlap_before != NULL) &&
             (buffers.overlap_after != NULL) && (buffers.overlap_shared != NULL) && (buffers.labels[0] != NULL) &&
             (buffers.labels[1] != NULL) && (buffers.positive[0] != NULL) && (buffers.positive[1] != NULL) &&
             ((rules->climb == 0) || ((buffers.leaf_at_peak[0] != NULL) && (buffers.leaf_at_peak[1] != NULL) &&
                                      (buffers.leaf_start[0] != NULL) && (buffers.leaf_start[1] != NULL) &&
                                      (buffers.leaf_voxels[0] != NULL) && (buffers.leaf_voxels[1] != NULL)));
    const int machine_wanted = (rules->pick != 0) || (rules->climb == 1) || (rules->climb == 3);
    const unsigned long long started = engine_clock_microseconds() / 1000ull;
    StageClock clocks;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &clocks, .bytes = sizeof(clocks), .val = 0);
    const bool object = rules->object;
    size_t frame_runs_capacity = 0u;
    unsigned int *frame_runs = NULL;
    size_t object_run_capacity = 0u;
    size_t object_run_count = 0u;
    unsigned int *object_runs = NULL;
    size_t object_leaf_capacity = 0u;
    size_t object_leaf_count = 0u;
    unsigned int *object_leaf_runs = NULL;
    unsigned int *const object_first_run = (unsigned int *)platform_cell_cleared(
        ((size_t)frame_count + 1u) * object, sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const object_leaf_code =
        (unsigned int *)platform_cell_cleared(voxels * object, sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = ok && (!object || (object_first_run && object_leaf_code));
    for (unsigned int frame = 0u; (ok != 0) && (frame < frame_count); frame += 1u)
    {
        unsigned long long mark = engine_clock_microseconds();
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = buffers.volume, .src = &volume[(size_t)frames[frame].time * voxels],
                   .bytes = voxels * sizeof(unsigned short));
        clocks.read += engine_clock_microseconds() - mark;
        mark = engine_clock_microseconds();
        ok = (ok != 0) && (track_frame_bodies(&buffers, 1u, &frames[frame]) != 0);
        clocks.bodies += engine_clock_microseconds() - mark;
        mark = engine_clock_microseconds();
        if ((ok != 0) && machine_wanted && (buffers.machine == NULL))
        {
            unsigned long long padded = 1ull;
            const unsigned int extents[3] = {buffers.depth, buffers.height, buffers.width};
            for (unsigned int axis = 0u; axis < 3u; axis += 1u)
            {
                unsigned long long power = 1ull;
                while (power < (2ull * (unsigned long long)extents[axis]) - 1ull)
                {
                    power <<= 1u;
                }
                padded *= power;
            }
            ClimbMachineExtent machine_extent;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &machine_extent, .bytes = sizeof(machine_extent), .val = 0);
            machine_extent.depth = buffers.depth;
            machine_extent.height = buffers.height;
            machine_extent.width = buffers.width;
            machine_extent.peak_capacity = buffers.peak_capacity;
            machine_extent.frames = frame_count;
            machine_extent.weight_z = AXIS_WEIGHTS[0];
            machine_extent.reserve_bytes = (unsigned int)(((padded * 16ull) >> 20u) + 768ull);
            buffers.machine = climb_machine_open(&machine_extent);
            climb_machine_land_by_mass(buffers.machine, (unsigned int)((rules->mass != 0) || (rules->spiral != 0u)));
            ok = (ok != 0) && (climb_machine_spiral(buffers.machine, rules->spiral) != 0);
            ok = (buffers.machine != NULL) ? 1 : 0;
        }
        if ((ok != 0) && ((rules->climb >= 2) || (rules->cast != 0) || (rules->parallax != 0)))
        {
            track_group_voxels(&buffers, 1u, buffers.labels[1], &frames[frame]);
        }
        if ((ok != 0) && (buffers.machine != NULL))
        {
            ClimbMachineFrame stored;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &stored, .bytes = sizeof(stored), .val = 0);
            stored.frame = frames[frame].time;
            stored.leaf_count = frames[frame].leaf_count;
            stored.labels = buffers.labels[1];
            stored.positive = buffers.positive[1];
            unsigned int *contact_start = NULL;
            unsigned int *contacts = NULL;
            if ((rules->climb == 1) || (rules->climb == 3))
            {
                stored.peaks = frames[frame].peaks;
                ok = (rules->sticky == 0) || (frame_contacts(&frames[frame], &contact_start, &contacts) != 0);
                stored.contact_start = contact_start;
                stored.contacts = contacts;
            }
            ok = (ok != 0) && (climb_machine_store(buffers.machine, &stored) != 0);
            platform_cell_released(contact_start, platform_place_g_program_owner);
            platform_cell_released(contacts, platform_place_g_program_owner);
            if ((ok != 0) && (rules->box_history != 0))
            {
                // leaf_count widens from unsigned int to size_t. (leaf_count + 1) * 6 cannot wrap before malloc sees
                // it
                frames[frame].extents = (unsigned int *)platform_cell_taken(
                    ((size_t)frames[frame].leaf_count + 1u) * CLIMB_MACHINE_EXTENT_FIELDS * sizeof(unsigned int),
                    platform_place_g_program_owner, 0);
                const ClimbMachineExtentsRequest extents_request = {frames[frame].time, frames[frame].extents};
                ok = (frames[frame].extents != NULL) && (climb_machine_extents(buffers.machine, &extents_request) != 0);
            }
        }
        clocks.store += engine_clock_microseconds() - mark;
        mark = engine_clock_microseconds();
        if (ok == 0)
        {
            break;
        }
        if (object)
        {
            const unsigned long long *const positive = buffers.positive[1];
            size_t positives = 0u;
            for (size_t word = 0u; word < (voxels + 63u) / 64u; word += 1u)
            {
                positives += engine_word_population(positive[word]);
            }
            ok = ok && engine_object_reserve(&frame_runs, &frame_runs_capacity, positives + 2u, 3u) &&
                 engine_object_reserve(&object_runs, &object_run_capacity, object_run_count + positives, 2u) &&
                 engine_object_reserve(&object_leaf_runs, &object_leaf_capacity,
                                       object_leaf_count + frames[frame].leaf_count, 2u);
        }
        if (object && ok)
        {
            const TreeFrame *const tree = &frames[frame];
            const unsigned int *const labels = buffers.labels[1];
            const unsigned long long *const positive = buffers.positive[1];
            unsigned int *const runs = frame_runs;
            for (unsigned int leaf = 0u; leaf < tree->leaf_count; leaf += 1u)
            {
                object_leaf_code[tree->peaks[leaf]] = leaf + 1u;
            }
            // three words a run while the frame is walked: its first voxel, its leaf, and its length less one. Entry
            // 0 takes the writes of voxels outside every leaf. A run stays in its row and is at max a row long
            runs[0] = 0u;
            runs[1] = 0u;
            runs[2] = 0u;
            size_t count = 0u;
            size_t voxel = 0u;
            for (unsigned int row = 0u; row < buffers.depth * buffers.height; row += 1u)
            {
                unsigned int previous = 0u;
                for (unsigned int column = 0u; column < buffers.width; column += 1u)
                {
                    const unsigned int live = (unsigned int)((positive[voxel >> 6u] >> (voxel & 63u)) & 1ULL);
                    const unsigned int code = live * object_leaf_code[labels[voxel] * live];
                    const unsigned int coded = !!code;
                    const unsigned int begins = coded & (unsigned int)(code != previous);
                    runs[3u * (count + 1u)] = (unsigned int)voxel;
                    count += begins;
                    const size_t target_at = count * coded;
                    runs[(3u * target_at) + 1u] = code - coded;
                    runs[(3u * target_at) + 2u] = (unsigned int)voxel - runs[3u * target_at];
                    previous = code;
                    voxel += 1u;
                }
            }
            unsigned int *const leaf_runs = &object_leaf_runs[2u * object_leaf_count];
            for (unsigned int leaf = 0u; leaf < tree->leaf_count; leaf += 1u)
            {
                object_leaf_code[tree->peaks[leaf]] = 0u;
                leaf_runs[2u * leaf] = 0u;
                leaf_runs[(2u * leaf) + 1u] = 0u;
            }
            for (size_t target_at = 1u; target_at <= count; target_at += 1u)
            {
                leaf_runs[(2u * runs[(3u * target_at) + 1u]) + 1u] += 1u;
            }
            unsigned int placed = (unsigned int)object_run_count;
            for (unsigned int leaf = 0u; leaf < tree->leaf_count; leaf += 1u)
            {
                leaf_runs[2u * leaf] = placed;
                placed += leaf_runs[(2u * leaf) + 1u];
            }
            // two words a run in the object: its first voxel in the frame, and its length
            for (size_t target_at = 1u; target_at <= count; target_at += 1u)
            {
                unsigned int *const cursor = &leaf_runs[2u * runs[(3u * target_at) + 1u]];
                object_runs[2u * (size_t)*cursor] = runs[3u * target_at];
                object_runs[(2u * (size_t)*cursor) + 1u] = runs[(3u * target_at) + 2u] + 1u;
                *cursor += 1u;
            }
            for (unsigned int leaf = 0u; leaf < tree->leaf_count; leaf += 1u)
            {
                leaf_runs[2u * leaf] -= leaf_runs[(2u * leaf) + 1u];
            }
            object_first_run[frame] = (unsigned int)object_run_count;
            object_run_count += count;
            object_leaf_count += tree->leaf_count;
        }
        if (rules->export_directory != NULL)
        {
            TreeFrame *const tree = &frames[frame];
            platform_cell_released(tree->moments, platform_place_g_program_owner);
            tree->moments = (unsigned long long *)platform_cell_cleared(
                (size_t)tree->leaf_count * 6u + 1u, sizeof(unsigned long long), platform_place_g_program_owner, 0);
            tree->exposed = (unsigned int *)platform_cell_cleared((size_t)tree->leaf_count + 1u, sizeof(unsigned int),
                                                                  platform_place_g_program_owner, 0);
            tree->contact_faces = (unsigned int *)platform_cell_cleared(
                (size_t)tree->joined_count + 1u, sizeof(unsigned int), platform_place_g_program_owner, 0);
            ok = (tree->moments != NULL) && (tree->exposed != NULL) && (tree->contact_faces != NULL);
            const unsigned int plane = buffers.height * buffers.width;
            for (size_t voxel = 0u; (ok != 0) && (voxel < voxels); voxel += 1u)
            {
                if (((buffers.positive[1][voxel / 64u] >> (voxel % 64u)) & 1ULL) == 0ULL)
                {
                    continue;
                }
                const int leaf = engine_leaf_of_peak(tree->peaks, tree->leaf_count, buffers.labels[1][voxel]);
                if (leaf < 0)
                {
                    continue;
                }
                const unsigned long long z = (unsigned long long)(voxel / plane);
                const unsigned long long y = (unsigned long long)((voxel % plane) / buffers.width);
                const unsigned long long x = (unsigned long long)((voxel % plane) % buffers.width);
                unsigned long long *const moment = &tree->moments[(size_t)(unsigned int)leaf * 6u];
                moment[0] += z * z;
                moment[1] += y * y;
                moment[2] += x * x;
                moment[3] += z * y;
                moment[4] += z * x;
                moment[5] += y * x;
                const long neighbors[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
                for (unsigned int face = 0u; face < 6u; face += 1u)
                {
                    const long near_z = (long)z + neighbors[face][0];
                    const long near_y = (long)y + neighbors[face][1];
                    const long near_x = (long)x + neighbors[face][2];
                    if ((near_z < 0L) || (near_z >= (long)buffers.depth) || (near_y < 0L) ||
                        (near_y >= (long)buffers.height) || (near_x < 0L) || (near_x >= (long)buffers.width))
                    {
                        tree->exposed[(unsigned int)leaf] += 1u;
                        continue;
                    }
                    const size_t near =
                        (size_t)((near_z * (long)buffers.height + near_y) * (long)buffers.width + near_x);
                    if (((buffers.positive[1][near / 64u] >> (near % 64u)) & 1ULL) == 0ULL)
                    {
                        tree->exposed[(unsigned int)leaf] += 1u;
                        continue;
                    }
                    const int other = engine_leaf_of_peak(tree->peaks, tree->leaf_count, buffers.labels[1][near]);
                    if ((other < 0) || (other <= leaf))
                    {
                        tree->exposed[(unsigned int)leaf] += (other < 0) ? 1u : 0u;
                        continue;
                    }
                    const unsigned long long sought =
                        ((unsigned long long)(unsigned int)leaf << 32u) | (unsigned int)other;
                    unsigned int low = 0u;
                    unsigned int high = tree->joined_count;
                    while (low < high)
                    {
                        const unsigned int middle = low + (high - low) / 2u;
                        const unsigned long long key =
                            ((unsigned long long)tree->joined[2u * middle] << 32u) | tree->joined[2u * middle + 1u];
                        if (key < sought)
                        {
                            low = middle + 1u;
                        }
                        else
                        {
                            high = middle;
                        }
                    }
                    if ((low < tree->joined_count) && (tree->joined[2u * low] == (unsigned int)leaf) &&
                        (tree->joined[2u * low + 1u] == (unsigned int)other))
                    {
                        tree->contact_faces[low] += 1u;
                    }
                }
            }
        }
        for (unsigned int node = 0u; node < key.node_count; node += 1u)
        {
            const int *const place = &key.node_coordinates[(size_t)node * 4u];
            if ((place[0] < 0) || ((unsigned int)place[0] != frames[frame].time))
            {
                continue;
            }
            const size_t voxel =
                ((size_t)place[1] * buffers.height + (size_t)place[2]) * buffers.width + (size_t)place[3];
            node_leaf[node] =
                engine_leaf_of_peak(frames[frame].peaks, frames[frame].leaf_count, buffers.labels[1][voxel]);
        }
        clocks.ties += engine_clock_microseconds() - mark;
        if ((frame > 0u) && (frames[frame].time == frames[frame - 1u].time + 1u))
        {
            ok = relate_frames(&buffers, &frames[frame - 1u], &frames[frame], rules, &clocks);
        }
        unsigned int *const labels = buffers.labels[0];
        buffers.labels[0] = buffers.labels[1];
        buffers.labels[1] = labels;
        unsigned long long *const positive = buffers.positive[0];
        buffers.positive[0] = buffers.positive[1];
        buffers.positive[1] = positive;
        int *const leaf_at_peak = buffers.leaf_at_peak[0];
        buffers.leaf_at_peak[0] = buffers.leaf_at_peak[1];
        buffers.leaf_at_peak[1] = leaf_at_peak;
        unsigned int *const leaf_start = buffers.leaf_start[0];
        buffers.leaf_start[0] = buffers.leaf_start[1];
        buffers.leaf_start[1] = leaf_start;
        unsigned int *const leaf_voxels = buffers.leaf_voxels[0];
        buffers.leaf_voxels[0] = buffers.leaf_voxels[1];
        buffers.leaf_voxels[1] = leaf_voxels;
    }
    unsigned int floors = 0u;
    unsigned long long cloud[ENGINE_HISTORY_WINDOWS_MAX * ENGINE_HISTORY_WINDOWS_MAX];
    if ((ok != 0) && (rules->null_draws != 0u) && (rules->floor_entropy != 0))
    {
        char cloud_path[ENGINE_PATH_CAPACITY];
        ok = engine_sample_path(cloud_path, sizeof(cloud_path), set, sample, ".oapx") &&
             (engine_entropy_cloud(cloud_path, &floors, cloud, &error) == 0L);
        if (ok == 0)
        {
            fprintf(stderr, "  %s: the floor's cloud could not be read from %s\n", sample, cloud_path);
            if (error.type != ENGINE_ERROR_NONE)
            {
                track_error_report("entropy cloud read", &error);
            }
        }
    }
    const unsigned int draw_capacity = (floors != 0u) ? floors : rules->null_draws;
    int **null_scratch = NULL;
    unsigned int null_scratch_count = 0u;
    for (unsigned int frame = 0u;
         (ok != 0) && (rules->null_draws != 0u) && (buffers.machine != NULL) && (frame < frame_count); frame += 1u)
    {
        TreeFrame *const tree = &frames[frame];
        if (tree->forward_final_score == NULL)
        {
            continue;
        }
        tree->null_final_score = (unsigned int *)platform_cell_cleared(
            ((size_t)tree->leaf_count + 1u) * draw_capacity, sizeof(unsigned int), platform_place_g_program_owner, 0);
        int **const grown = (int **)platform_cell_grown(
            null_scratch, (size_t)null_scratch_count * sizeof(int *),
            ((size_t)null_scratch_count + (4u * draw_capacity)) * sizeof(int *), platform_place_g_program_owner, 0);
        ok = (tree->null_final_score != NULL) && (grown != NULL);
        null_scratch = (grown != NULL) ? grown : null_scratch;
        unsigned int aimed[ENGINE_HISTORY_WINDOWS_MAX];
        const unsigned int own_floor =
            (floors != 0u) ? (((tree->time / ENGINE_HISTORY_WINDOW) < floors) ? (tree->time / ENGINE_HISTORY_WINDOW)
                                                                              : (floors - 1u))
                           : 0u;
        for (unsigned int floor = 0u; floor < floors; floor += 1u)
        {
            aimed[floor] = floor;
        }
        for (unsigned int at = 1u; at < floors; at += 1u)
        {
            const unsigned int moving = aimed[at];
            unsigned int target_at = at;
            while ((target_at > 0u) &&
                   (cloud[(own_floor * floors) + aimed[target_at - 1u]] > cloud[(own_floor * floors) + moving]))
            {
                aimed[target_at] = aimed[target_at - 1u];
                target_at -= 1u;
            }
            aimed[target_at] = moving;
        }
        for (unsigned int draw = 0u; (ok != 0) && (draw < draw_capacity); draw += 1u)
        {
            unsigned int other = (frame + (frame_count / 2u) + draw) % frame_count;
            if (floors != 0u)
            {
                const unsigned int floor = aimed[draw];
                const unsigned int place = tree->time - (own_floor * ENGINE_HISTORY_WINDOW);
                const unsigned int line = (floor * ENGINE_HISTORY_WINDOW) + place;
                if ((floor == own_floor) || (line >= volume_frames) || (tree_index_of_time[line] < 0))
                {
                    continue;
                }
                other = (unsigned int)tree_index_of_time[line];
            }
            const unsigned int apart = (other > frame) ? (other - frame) : (frame - other);
            if ((frame_count < 8u) || (apart <= 2u) || ((frame_count - apart) <= 2u))
            {
                continue;
            }
            const TreeFrame *const null_frame = &frames[other];
            ClimbMachinePair pair;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &pair, .bytes = sizeof(pair), .val = 0);
            pair.earlier = tree->time;
            pair.later = null_frame->time;
            for (unsigned int axis = 0u; axis < 3u; axis += 1u)
            {
                pair.lag[axis] = tree->lag_to_next[axis];
            }
            pair.earlier_leaves = tree->leaf_count;
            pair.earlier_peaks = tree->peaks;
            pair.later_leaves = null_frame->leaf_count;
            pair.later_peaks = null_frame->peaks;
            pair.forward_lags = (int *)platform_cell_taken(((size_t)tree->leaf_count + 1u) * 3u * sizeof(int),
                                                           platform_place_g_program_owner, 0);
            pair.forward = (int *)platform_cell_taken(((size_t)tree->leaf_count + 1u) * sizeof(int),
                                                      platform_place_g_program_owner, 0);
            pair.backward_lags = (int *)platform_cell_taken(((size_t)null_frame->leaf_count + 1u) * 3u * sizeof(int),
                                                            platform_place_g_program_owner, 0);
            pair.backward = (int *)platform_cell_taken(((size_t)null_frame->leaf_count + 1u) * sizeof(int),
                                                       platform_place_g_program_owner, 0);
            pair.forward_final_score =
                &tree->null_final_score[(size_t)tree->null_count * ((size_t)tree->leaf_count + 1u)];
            null_scratch[null_scratch_count] = pair.forward_lags;
            null_scratch[null_scratch_count + 1u] = pair.forward;
            null_scratch[null_scratch_count + 2u] = pair.backward_lags;
            null_scratch[null_scratch_count + 3u] = pair.backward;
            null_scratch_count += 4u;
            ok = (pair.forward_lags != NULL) && (pair.forward != NULL) && (pair.backward_lags != NULL) &&
                 (pair.backward != NULL) && (climb_machine_pend(buffers.machine, &pair) != 0);
            tree->null_count += (ok != 0) ? 1u : 0u;
        }
    }
    int **arm_scratch = NULL;
    unsigned int arm_scratch_count = 0u;
    for (unsigned int frame = 0u;
         (ok != 0) && (rules->arms != 0u) && (buffers.machine != NULL) && (frame < frame_count); frame += 1u)
    {
        TreeFrame *const tree = &frames[frame];
        tree->arm_forward = (int *)platform_cell_taken(((size_t)tree->leaf_count + 1u) * rules->arms * sizeof(int),
                                                       platform_place_g_program_owner, 0);
        int **const grown = (int **)platform_cell_grown(
            arm_scratch, (size_t)arm_scratch_count * sizeof(int *),
            ((size_t)arm_scratch_count + (3u * rules->arms)) * sizeof(int *), platform_place_g_program_owner, 0);
        ok = (tree->arm_forward != NULL) && (grown != NULL);
        arm_scratch = (grown != NULL) ? grown : arm_scratch;
        for (size_t target_at = 0u; (ok != 0) && (target_at < ((size_t)tree->leaf_count + 1u) * rules->arms);
             target_at += 1u)
        {
            tree->arm_forward[target_at] = CLIMB_MACHINE_NO_LEAF;
        }
        for (unsigned int arm = 0u; (ok != 0) && (arm < rules->arms); arm += 1u)
        {
            const unsigned int gap = arm + 2u;
            if ((frame + gap) >= frame_count)
            {
                continue;
            }
            const TreeFrame *const far = &frames[frame + gap];
            ClimbMachinePair pair;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &pair, .bytes = sizeof(pair), .val = 0);
            pair.earlier = tree->time;
            pair.later = far->time;
            for (unsigned int axis = 0u; axis < 3u; axis += 1u)
            {
                pair.lag[axis] = (int)((long long)tree->lag_to_next[axis] * (long long)gap);
            }
            pair.earlier_leaves = tree->leaf_count;
            pair.earlier_peaks = tree->peaks;
            pair.later_leaves = far->leaf_count;
            pair.later_peaks = far->peaks;
            pair.forward_lags = (int *)platform_cell_taken(((size_t)tree->leaf_count + 1u) * 3u * sizeof(int),
                                                           platform_place_g_program_owner, 0);
            pair.backward_lags = (int *)platform_cell_taken(((size_t)far->leaf_count + 1u) * 3u * sizeof(int),
                                                            platform_place_g_program_owner, 0);
            pair.backward = (int *)platform_cell_taken(((size_t)far->leaf_count + 1u) * sizeof(int),
                                                       platform_place_g_program_owner, 0);
            pair.forward = &tree->arm_forward[(size_t)arm * ((size_t)tree->leaf_count + 1u)];
            arm_scratch[arm_scratch_count] = pair.forward_lags;
            arm_scratch[arm_scratch_count + 1u] = pair.backward_lags;
            arm_scratch[arm_scratch_count + 2u] = pair.backward;
            arm_scratch_count += 3u;
            ok = (pair.forward_lags != NULL) && (pair.backward_lags != NULL) && (pair.backward != NULL) &&
                 (climb_machine_pend(buffers.machine, &pair) != 0);
            tree->arm_count += (ok != 0) ? 1u : 0u;
        }
    }
    const unsigned long long climbing = engine_clock_microseconds();
    ok = (ok != 0) && ((buffers.machine == NULL) || (climb_machine_run(buffers.machine) != 0));
    clocks.climb += engine_clock_microseconds() - climbing;
    for (unsigned int scratch = 0u; scratch < null_scratch_count; scratch += 1u)
    {
        platform_cell_released(null_scratch[scratch], platform_place_g_program_owner);
    }
    platform_cell_released(null_scratch, platform_place_g_program_owner);
    for (unsigned int scratch = 0u; scratch < arm_scratch_count; scratch += 1u)
    {
        platform_cell_released(arm_scratch[scratch], platform_place_g_program_owner);
    }
    platform_cell_released(arm_scratch, platform_place_g_program_owner);
    for (unsigned int frame = 0u; (ok != 0) && (rules->climb == 3) && (frame < frame_count); frame += 1u)
    {
        const TreeFrame *const tree = &frames[frame];
        for (unsigned int leaf = 0u; (tree->check_forward_lag != NULL) && (leaf < tree->leaf_count); leaf += 1u)
        {
            if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &tree->check_forward_lag[3u * leaf],
                           .other = &tree->forward_lag[3u * leaf], .bytes = 3u * sizeof(int)) != 0)
            {
                fprintf(stderr, "    climb disagrees: earlier frame %u leaf %u\n", tree->time, leaf);
            }
        }
        for (unsigned int leaf = 0u; (tree->check_backward_lag != NULL) && (leaf < tree->leaf_count); leaf += 1u)
        {
            if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &tree->check_backward_lag[3u * leaf],
                           .other = &tree->backward_lag[3u * leaf], .bytes = 3u * sizeof(int)) != 0)
            {
                fprintf(stderr, "    climb disagrees: later frame %u leaf %u\n", tree->time, leaf);
            }
        }
    }
    ok = (ok != 0) && ((rules->velocity == 0) || (score_velocity(sample, frames, frame_count, rules) != 0));
    ok = (ok != 0) && ((rules->division == 0) || (score_division(sample, frames, frame_count, rules, &key, node_leaf,
                                                                 tree_index_of_time, volume_frames) != 0));
    ok = (ok != 0) && ((rules->contact_side == 0) || (score_contact_side(sample, frames, frame_count, rules) != 0));
    ScoreStars stars;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &stars, .bytes = sizeof(stars), .val = 0);
    ClimbMachineBox box;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &box, .bytes = sizeof(box), .val = 0);
    ok = (ok != 0) && ((rules->box == 0) || (buffers.machine == NULL) ||
                       (score_box(sample, frames, frame_count, buffers.machine, tree_index_of_time, volume_frames,
                                  &stars, &box) != 0));
    ok = (ok != 0) && ((rules->core == 0) || (buffers.machine == NULL) ||
                       (score_core(sample, buffers.machine, (rules->box != 0) ? &box : NULL) != 0));
    ok =
        (ok != 0) && ((rules->box_history == 0) || (score_box_history(set, sample, frames, frame_count, &key, node_leaf,
                                                                      tree_index_of_time, volume_frames) != 0));
    ok = (ok != 0) &&
         ((rules->marginal == 0) || (score_marginal(sample, frames, frame_count, &key, node_leaf, tree_index_of_time,
                                                    volume_frames, stars.choices) != 0));
    score_stars_release(&stars);
    ok = (ok != 0) && ((rules->print_match == 0) || (score_print_match(sample, frames, frame_count, rules) != 0));
    const unsigned long long engines_done = engine_clock_microseconds() / 1000ull;

    for (unsigned int frame = 0u; (ok != 0) && (rules->tower != 0) && (frame < frame_count); frame += 1u)
    {
        frames[frame].majority_target = (unsigned int *)platform_cell_taken(
            ((size_t)frames[frame].leaf_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
        frames[frame].majority_count = (unsigned int *)platform_cell_cleared(
            (size_t)frames[frame].leaf_count + 1u, sizeof(unsigned int), platform_place_g_program_owner, 0);
        frames[frame].majority_agreed_rounds = (unsigned int *)platform_cell_cleared(
            (size_t)frames[frame].leaf_count + 1u, sizeof(unsigned int), platform_place_g_program_owner, 0);
        ok = (frames[frame].majority_target != NULL) && (frames[frame].majority_count != NULL) &&
             (frames[frame].majority_agreed_rounds != NULL);
        for (unsigned int leaf = 0u; (ok != 0) && (leaf < frames[frame].leaf_count); leaf += 1u)
        {
            frames[frame].majority_target[leaf] = 0xFFFFFFFFu;
        }
    }
    unsigned int **const was_target = (unsigned int **)platform_cell_cleared(
        (size_t)frame_count + 1u, sizeof(unsigned int *), platform_place_g_program_owner, 0);
    unsigned int *const was_count = (unsigned int *)platform_cell_cleared(
        (size_t)frame_count + 1u, sizeof(unsigned int), platform_place_g_program_owner, 0);
    const unsigned int rounds =
        ((rules->tower != 0) && (was_target != NULL) && (was_count != NULL)) ? TOWER_ROUNDS : 1u;
    unsigned int turned = 0u;
    unsigned int moving = 1u;
    for (unsigned int round = 0u; (ok != 0) && (round < rounds) && (moving != 0u); round += 1u)
    {
        turned = round + 1u;
        for (unsigned int frame = 0u; (round != 0u) && (frame < frame_count); frame += 1u)
        {
            platform_cell_released(frames[frame].link_start, platform_place_g_program_owner);
            platform_cell_released(frames[frame].link_target, platform_place_g_program_owner);
            platform_cell_released(frames[frame].pool_start, platform_place_g_program_owner);
            platform_cell_released(frames[frame].pool_target, platform_place_g_program_owner);
            platform_cell_released(frames[frame].pool_weight, platform_place_g_program_owner);
            platform_cell_released(frames[frame].pool_cost, platform_place_g_program_owner);
            frames[frame].link_start = NULL;
            frames[frame].link_target = NULL;
            frames[frame].pool_start = NULL;
            frames[frame].pool_target = NULL;
            frames[frame].pool_weight = NULL;
            frames[frame].pool_cost = NULL;
        }
        const unsigned int grouping_passes = (rules->agree != 0) ? 2u : 1u;
        for (unsigned int pass = 0u; (ok != 0) && (pass < grouping_passes); pass += 1u)
        {
            for (unsigned int frame = 0u; (ok != 0) && (frame < frame_count); frame += 1u)
            {
                const TreeFrame *const previous = (frames[frame].backward != NULL) ? &frames[frame - 1u] : NULL;
                const TreeFrame *const next =
                    ((pass != 0u) && ((frame + 1u) < frame_count)) ? &frames[frame + 1u] : NULL;
                ok = group_objects(&frames[frame], previous, next, buffers.height, buffers.width, rules);
            }
        }
        for (unsigned int frame = 0u; (ok != 0) && (frame + 1u < frame_count); frame += 1u)
        {
            if (frames[frame].forward != NULL)
            {
                const TreeFrame *const before = (frame > 0u) ? &frames[frame - 1u] : NULL;
                const TreeFrame *const after = ((frame + 2u) < frame_count) ? &frames[frame + 2u] : NULL;
                ok = (rules->unbound != 0)
                         ? link_objects_unbound(&frames[frame], &frames[frame + 1u], before, after, rules)
                         : link_objects(&frames[frame], &frames[frame + 1u], rules);
            }
        }
        for (unsigned int frame = 0u; (ok != 0) && (rounds > 1u) && ((frame + 1u) < frame_count); frame += 1u)
        {
            TreeFrame *const tree = &frames[frame];
            const TreeFrame *const ahead = &frames[frame + 1u];
            if ((tree->link_start == NULL) || (tree->majority_target == NULL) || (ahead->member_start == NULL))
            {
                continue;
            }
            for (unsigned int leaf = 0u; leaf < tree->leaf_count; leaf += 1u)
            {
                const unsigned int object = tree->object_of[leaf];
                const unsigned int links = tree->link_start[object + 1u] - tree->link_start[object];
                unsigned int target = 0xFFFFFFFFu;
                if (links == 1u)
                {
                    const unsigned int went = tree->link_target[tree->link_start[object]];
                    unsigned long long widest = 0ull;
                    for (unsigned int member = ahead->member_start[went]; member < ahead->member_start[went + 1u];
                         member += 1u)
                    {
                        const unsigned int other = ahead->members[member];
                        const unsigned long long size =
                            band_or_count((unsigned int)rules->mass_band, ahead->sizes[other]);
                        const unsigned int bigger = (unsigned int)((target == 0xFFFFFFFFu) || (size > widest));
                        widest = (bigger != 0u) ? size : widest;
                        target = (bigger != 0u) ? other : target;
                    }
                }
                const unsigned int empty = (unsigned int)(tree->majority_count[leaf] == 0u);
                const unsigned int agrees = (unsigned int)(target == tree->majority_target[leaf]);
                tree->majority_target[leaf] = (empty != 0u) ? target : tree->majority_target[leaf];
                tree->majority_count[leaf] = ((empty != 0u) || (agrees != 0u)) ? (tree->majority_count[leaf] + 1u)
                                                                               : (tree->majority_count[leaf] - 1u);
                tree->majority_agreed_rounds[leaf] += (unsigned int)(agrees != 0u);
            }
        }
        moving = 0u;
        for (unsigned int frame = 0u; (ok != 0) && (rounds > 1u) && (frame < frame_count); frame += 1u)
        {
            const unsigned int links =
                (frames[frame].link_start != NULL) ? frames[frame].link_start[frames[frame].object_count] : 0u;
            const unsigned int same =
                (unsigned int)((links == was_count[frame]) && (was_target[frame] != NULL) &&
                               ((links == 0u) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = was_target[frame],
                                                             .other = frames[frame].link_target,
                                                             .bytes = (size_t)links * sizeof(unsigned int)) == 0)));
            moving += (unsigned int)(same == 0u);
            unsigned int *const retained =
                (links != 0u) ? (unsigned int *)platform_cell_taken((size_t)links * sizeof(unsigned int),
                                                                    platform_place_g_program_owner, 0)
                              : NULL;
            if (retained != NULL)
            {
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = retained, .src = frames[frame].link_target,
                           .bytes = (size_t)links * sizeof(unsigned int));
            }
            platform_cell_released(was_target[frame], platform_place_g_program_owner);
            was_target[frame] = retained;
            was_count[frame] = links;
        }
    }
    for (unsigned int frame = 0u; (was_target != NULL) && (frame < frame_count); frame += 1u)
    {
        platform_cell_released(was_target[frame], platform_place_g_program_owner);
    }
    platform_cell_released(was_target, platform_place_g_program_owner);
    platform_cell_released(was_count, platform_place_g_program_owner);
    if ((rules->tower != 0) && (rules->edges != NULL))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    tower: ");
        mmgr_line_decimal(output_line, turned, 1u);
        mmgr_line_put(output_line, " rounds, ");
        mmgr_line_put(output_line, (moving == 0u) ? "settled" : "still moving when the rounds ran out");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const unsigned int refocused =
        ((ok != 0) && (rules->focus != 0)) ? focus_links(frames, frame_count, (unsigned int)rules->mass_band) : 0u;
    if ((rules->focus != 0) && (rules->edges != NULL))
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    focus moved ");
        mmgr_line_decimal(output_line, refocused, 1u);
        mmgr_line_put(output_line, " links");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    NodeIndex index;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &index, .bytes = sizeof(index), .val = 0);
    index.frame_count = frame_count;
    index.frames = frames;
    index.node_offset = (unsigned int *)platform_cell_cleared((size_t)frame_count + 1u, sizeof(unsigned int),
                                                              platform_place_g_program_owner, 0);
    ok = (ok != 0) && (index.node_offset != NULL);
    for (unsigned int frame = 0u; (ok != 0) && (frame < frame_count); frame += 1u)
    {
        index.node_offset[frame + 1u] = index.node_offset[frame] + frames[frame].object_count;
    }
    index.node_count = (ok != 0) ? index.node_offset[frame_count] : 0u;
    index.node_frame = (unsigned int *)platform_cell_taken(((size_t)index.node_count + 1u) * sizeof(unsigned int),
                                                           platform_place_g_program_owner, 0);
    unsigned int *const parent = (unsigned int *)platform_cell_taken(
        ((size_t)index.node_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = (ok != 0) && (index.node_frame != NULL) && (parent != NULL);
    for (unsigned int frame = 0u; (ok != 0) && (frame < frame_count); frame += 1u)
    {
        for (unsigned int node = index.node_offset[frame]; node < index.node_offset[frame + 1u]; node += 1u)
        {
            index.node_frame[node] = frame;
            parent[node] = node;
        }
    }

    unsigned int rejoined = 0u;
    unsigned int visited_capacity = 64u;
    unsigned int *visited = (unsigned int *)platform_cell_taken((size_t)visited_capacity * 2u * sizeof(unsigned int),
                                                                platform_place_g_program_owner, 0);
    ok = (ok != 0) && (visited != NULL);
    for (unsigned int frame = 0u; (ok != 0) && (rules->resolve != 0) && (frame < frame_count); frame += 1u)
    {
        if (frames[frame].link_start == NULL)
        {
            continue;
        }
        const unsigned int next_offset = index.node_offset[frame + 1u];
        for (unsigned int object = 0u; (ok != 0) && (object < frames[frame].object_count); object += 1u)
        {
            const unsigned int *const children = &frames[frame].link_target[frames[frame].link_start[object]];
            const unsigned int child_count = frames[frame].link_start[object + 1u] - frames[frame].link_start[object];
            if (child_count < 2u)
            {
                continue;
            }
            for (unsigned int first_child = 0u; (ok != 0) && (first_child < child_count); first_child += 1u)
            {
                for (unsigned int second_child = first_child + 1u; (ok != 0) && (second_child < child_count);
                     second_child += 1u)
                {
                    const unsigned int first = next_offset + children[first_child];
                    const unsigned int second = next_offset + children[second_child];
                    if (engine_find_root(parent, first) == engine_find_root(parent, second))
                    {
                        continue;
                    }
                    unsigned int left = first;
                    unsigned int right = second;
                    visited[0] = left;
                    visited[1] = right;
                    unsigned int steps = 1u;
                    while (left != right)
                    {
                        const unsigned int *left_next = NULL;
                        const unsigned int *right_next = NULL;
                        unsigned int left_count = 0u;
                        unsigned int right_count = 0u;
                        links_of_node(&index, left, &left_next, &left_count);
                        links_of_node(&index, right, &right_next, &right_count);
                        if ((left_count != 1u) || (right_count != 1u))
                        {
                            break;
                        }
                        left = index.node_offset[index.node_frame[left] + 1u] + left_next[0];
                        right = index.node_offset[index.node_frame[right] + 1u] + right_next[0];
                        if (steps == visited_capacity)
                        {
                            visited_capacity *= 2u;
                            unsigned int *const grown = (unsigned int *)platform_cell_grown(
                                visited, (size_t)visited_capacity * sizeof(unsigned int),
                                (size_t)visited_capacity * 2u * sizeof(unsigned int), platform_place_g_program_owner,
                                0);
                            if (grown == NULL)
                            {
                                ok = 0;
                                break;
                            }
                            visited = grown;
                        }
                        visited[2u * steps] = left;
                        visited[2u * steps + 1u] = right;
                        steps += 1u;
                    }
                    if ((ok != 0) && (left == right))
                    {
                        rejoined += 1u;
                        for (unsigned int step = 0u; step + 1u < steps; step += 1u)
                        {
                            const unsigned int one = engine_find_root(parent, visited[2u * step]);
                            const unsigned int other = engine_find_root(parent, visited[2u * step + 1u]);
                            if (one != other)
                            {
                                parent[(one > other) ? one : other] = (one < other) ? one : other;
                            }
                        }
                    }
                }
            }
        }
    }
    platform_cell_released(visited, platform_place_g_program_owner);

    unsigned int *const unified_of = (unsigned int *)platform_cell_taken(
        ((size_t)index.node_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const rank_of_root = (unsigned int *)platform_cell_taken(
        ((size_t)index.node_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = (ok != 0) && (unified_of != NULL) && (rank_of_root != NULL);
    unsigned int unified_count = 0u;
    unsigned int *const unified_first = (unsigned int *)platform_cell_cleared(
        (size_t)frame_count + 1u, sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = (ok != 0) && (unified_first != NULL);
    for (unsigned int frame = 0u; (ok != 0) && (frame < frame_count); frame += 1u)
    {
        unified_first[frame] = unified_count;
        for (unsigned int node = index.node_offset[frame]; node < index.node_offset[frame + 1u]; node += 1u)
        {
            if (engine_find_root(parent, node) == node)
            {
                rank_of_root[node] = unified_count;
                unified_count += 1u;
            }
        }
    }
    if (ok != 0)
    {
        unified_first[frame_count] = unified_count;
    }
    for (unsigned int node = 0u; (ok != 0) && (node < index.node_count); node += 1u)
    {
        unified_of[node] = rank_of_root[engine_find_root(parent, node)];
    }
    unsigned int link_total = 0u;
    for (unsigned int frame = 0u; (ok != 0) && (frame < frame_count); frame += 1u)
    {
        if (frames[frame].link_start != NULL)
        {
            link_total += frames[frame].link_start[frames[frame].object_count];
        }
    }
    unsigned long long *const unified_link = (unsigned long long *)platform_cell_taken(
        ((size_t)link_total + 1u) * sizeof(unsigned long long), platform_place_g_program_owner, 0);
    ok = (ok != 0) && (unified_link != NULL);
    unsigned int unified_links = 0u;
    for (unsigned int frame = 0u; (ok != 0) && (frame < frame_count); frame += 1u)
    {
        if (frames[frame].link_start == NULL)
        {
            continue;
        }
        for (unsigned int object = 0u; object < frames[frame].object_count; object += 1u)
        {
            const unsigned int source = unified_of[index.node_offset[frame] + object];
            for (unsigned int link = frames[frame].link_start[object]; link < frames[frame].link_start[object + 1u];
                 link += 1u)
            {
                const unsigned int child = unified_of[index.node_offset[frame + 1u] + frames[frame].link_target[link]];
                unified_link[unified_links] = ((unsigned long long)source << 32u) | child;
                unified_links += 1u;
            }
        }
    }
    unified_links = (ok != 0) ? engine_sort_unique(unified_link, unified_links) : 0u;
    unsigned int *const unified_start = (unsigned int *)platform_cell_cleared(
        (size_t)unified_count + 2u, sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = (ok != 0) && (unified_start != NULL);
    for (unsigned int link = 0u; (ok != 0) && (link < unified_links); link += 1u)
    {
        unified_start[(unsigned int)(unified_link[link] >> 32u) + 1u] += 1u;
    }
    for (unsigned int unified = 0u; (ok != 0) && (unified < unified_count); unified += 1u)
    {
        unified_start[unified + 1u] += unified_start[unified];
    }

    unsigned int *const successors = (unsigned int *)platform_cell_cleared(
        (size_t)key.node_count + 1u, sizeof(unsigned int), platform_place_g_program_owner, 0);
    signed char *const edge_status =
        (signed char *)platform_cell_taken((size_t)key.edge_count + 1u, platform_place_g_program_owner, 0);
    ok = (ok != 0) && (successors != NULL) && (edge_status != NULL);
    for (unsigned int edge = 0u; (ok != 0) && (edge < key.edge_count); edge += 1u)
    {
        edge_status[edge] = -1;
    }
    for (unsigned int edge = 0u; (ok != 0) && (edge < key.edge_count); edge += 1u)
    {
        const long source = node_index_of(&key, key.edge_ends[2u * edge]);
        if (source >= 0L)
        {
            successors[(unsigned long)source] += 1u;
        }
    }
    for (unsigned int edge = 0u; (ok != 0) && (edge < key.edge_count); edge += 1u)
    {
        const long source = node_index_of(&key, key.edge_ends[2u * edge]);
        const long target = node_index_of(&key, key.edge_ends[2u * edge + 1u]);
        if ((source < 0L) || (target < 0L))
        {
            continue;
        }
        const int source_time = key.node_coordinates[(size_t)source * 4u];
        const int target_time = key.node_coordinates[(size_t)target * 4u];
        if ((source_time < 0) || (target_time < 0) || ((unsigned int)source_time >= volume_frames) ||
            ((unsigned int)target_time >= volume_frames))
        {
            continue;
        }
        const int source_frame = tree_index_of_time[source_time];
        const int target_frame = tree_index_of_time[target_time];
        const int source_leaf = node_leaf[source];
        const int target_leaf = node_leaf[target];
        if ((source_leaf < 0) || (target_leaf < 0))
        {
            results->missed += 1ULL;
            edge_status[edge] = 4;
            continue;
        }
        const unsigned int source_unified =
            unified_of[index.node_offset[source_frame] + frames[source_frame].object_of[source_leaf]];
        const unsigned int target_unified =
            unified_of[index.node_offset[target_frame] + frames[target_frame].object_of[target_leaf]];
        const unsigned int made = unified_start[source_unified + 1u] - unified_start[source_unified];
        if (made == 0u)
        {
            results->unlinked += 1ULL;
            edge_status[edge] = 3;
            continue;
        }
        int has_target = 0;
        for (unsigned int link = unified_start[source_unified]; link < unified_start[source_unified + 1u]; link += 1u)
        {
            if ((unsigned int)(unified_link[link] & 0xFFFFFFFFULL) == target_unified)
            {
                has_target = 1;
            }
        }
        if ((has_target != 0) && ((made == 1u) || (made == successors[source])))
        {
            results->correct += 1ULL;
            edge_status[edge] = 0;
        }
        else if (has_target != 0)
        {
            results->branched += 1ULL;
            edge_status[edge] = 1;
        }
        else
        {
            results->wrong += 1ULL;
            edge_status[edge] = 2;
        }
    }
    unsigned int *edge_near_start = NULL;
    unsigned int *edge_nearby = NULL;
    unsigned int *edge_later_near_start = NULL;
    unsigned int *edge_later_nearby = NULL;
    unsigned int *edge_seen = NULL;
    unsigned int edge_mark = 0u;
    unsigned int edge_pair_from = 0xFFFFFFFFu;
    unsigned int edge_pair_to = 0xFFFFFFFFu;
    for (unsigned int edge = 0u; (ok != 0) && (rules->edges != NULL) && (edge < key.edge_count); edge += 1u)
    {
        const int status = edge_status[edge];
        if ((status < 0) || (status == 4))
        {
            continue;
        }
        const long source = node_index_of(&key, key.edge_ends[2u * edge]);
        const long target = node_index_of(&key, key.edge_ends[2u * edge + 1u]);
        const int *const from = &key.node_coordinates[(size_t)source * 4u];
        const int *const to = &key.node_coordinates[(size_t)target * 4u];
        const TreeFrame *const tree = &frames[tree_index_of_time[from[0]]];
        const unsigned int leaf = (unsigned int)node_leaf[source];
        const int *const carried = (tree->forward_lag != NULL) ? &tree->forward_lag[3u * leaf] : tree->lag_to_next;
        const unsigned int final_score = (tree->forward_final_score != NULL) ? tree->forward_final_score[leaf] : 0u;
        unsigned int null_at_least = 0u;
        unsigned int null_best = 0u;
        for (unsigned int draw = 0u; (tree->null_final_score != NULL) && (draw < tree->null_count); draw += 1u)
        {
            const unsigned int drawn = tree->null_final_score[((size_t)draw * ((size_t)tree->leaf_count + 1u)) + leaf];
            null_at_least += (drawn >= final_score) ? 1u : 0u;
            null_best = (drawn > null_best) ? drawn : null_best;
        }
        const TreeFrame *const next_tree = &frames[tree_index_of_time[to[0]]];
        if ((edge_pair_from != tree_index_of_time[from[0]]) || (edge_pair_to != tree_index_of_time[to[0]]))
        {
            platform_cell_released(edge_near_start, platform_place_g_program_owner);
            platform_cell_released(edge_nearby, platform_place_g_program_owner);
            platform_cell_released(edge_later_near_start, platform_place_g_program_owner);
            platform_cell_released(edge_later_nearby, platform_place_g_program_owner);
            platform_cell_released(edge_seen, platform_place_g_program_owner);
            edge_near_start = NULL;
            edge_nearby = NULL;
            edge_later_near_start = NULL;
            edge_later_nearby = NULL;
            edge_seen = NULL;
            const int joined = (frame_contacts(tree, &edge_near_start, &edge_nearby) != 0) &&
                               (frame_contacts(next_tree, &edge_later_near_start, &edge_later_nearby) != 0);
            edge_seen = (unsigned int *)platform_cell_cleared((size_t)tree->leaf_count + 2u, sizeof(unsigned int),
                                                              platform_place_g_program_owner, 0);
            if ((joined == 0) || (edge_seen == NULL))
            {
                platform_cell_released(edge_near_start, platform_place_g_program_owner);
                platform_cell_released(edge_nearby, platform_place_g_program_owner);
                platform_cell_released(edge_later_near_start, platform_place_g_program_owner);
                platform_cell_released(edge_later_nearby, platform_place_g_program_owner);
                platform_cell_released(edge_seen, platform_place_g_program_owner);
                edge_near_start = NULL;
                edge_nearby = NULL;
                edge_later_near_start = NULL;
                edge_later_nearby = NULL;
                edge_seen = NULL;
            }
            edge_mark = 0u;
            edge_pair_from = tree_index_of_time[from[0]];
            edge_pair_to = tree_index_of_time[to[0]];
        }
        const int truth_leaf = node_leaf[target];
        const int chosen_leaf = (tree->forward != NULL) ? tree->forward[leaf] : -1;
        const unsigned int pairs_first = (tree->triple_start != NULL) ? tree->triple_start[leaf] : 0u;
        const unsigned int pairs_end = (tree->triple_start != NULL) ? tree->triple_start[leaf + 1u] : 0u;
        unsigned int truth_shared = 0u;
        unsigned int chosen_shared = 0u;
        unsigned int best_shared = 0u;
        int best_leaf = -1;
        for (unsigned int pair = pairs_first; pair < pairs_end; pair += 1u)
        {
            const int after = (int)tree->triple_after[pair];
            const unsigned int shared = tree->triple_shared[pair];
            truth_shared = (after == truth_leaf) ? shared : truth_shared;
            chosen_shared = (after == chosen_leaf) ? shared : chosen_shared;
            best_leaf = (shared > best_shared) ? after : best_leaf;
            best_shared = (shared > best_shared) ? shared : best_shared;
        }
        const int truth_back =
            ((truth_leaf >= 0) && (next_tree->backward != NULL)) ? next_tree->backward[truth_leaf] : -1;
        const unsigned int truth_mutual =
            (unsigned int)((truth_back >= 0) && (tree->object_of[truth_back] == tree->object_of[leaf]));
        const unsigned int one_object =
            (unsigned int)((truth_leaf >= 0) && (chosen_leaf >= 0) &&
                           (next_tree->object_of[truth_leaf] == next_tree->object_of[chosen_leaf]));
        const unsigned int own_object = tree->object_of[leaf];
        const unsigned int truth_object = (truth_leaf >= 0) ? next_tree->object_of[truth_leaf] : 0xFFFFFFFFu;
        const unsigned int object_links =
            (tree->link_start != NULL) ? (tree->link_start[own_object + 1u] - tree->link_start[own_object]) : 0u;
        unsigned int object_links_truth = 0u;
        for (unsigned int link = 0u; (tree->link_start != NULL) && (link < object_links); link += 1u)
        {
            object_links_truth +=
                (unsigned int)(tree->link_target[tree->link_start[own_object] + link] == truth_object);
        }
        const unsigned int linked_object = ((tree->link_start != NULL) && (object_links != 0u))
                                               ? tree->link_target[tree->link_start[own_object]]
                                               : 0xFFFFFFFFu;
        unsigned long long truth_weight = 0ULL;
        unsigned long long linked_weight = 0ULL;
        const unsigned int members = tree->member_start[own_object + 1u] - tree->member_start[own_object];
        for (unsigned int member = tree->member_start[own_object]; member < tree->member_start[own_object + 1u];
             member += 1u)
        {
            const unsigned int other = tree->members[member];
            const unsigned int other_first = (tree->triple_start != NULL) ? tree->triple_start[other] : 0u;
            const unsigned int other_end = (tree->triple_start != NULL) ? tree->triple_start[other + 1u] : 0u;
            for (unsigned int pair = other_first; pair < other_end; pair += 1u)
            {
                const unsigned int after_object = next_tree->object_of[tree->triple_after[pair]];
                truth_weight += (after_object == truth_object) ? tree->triple_shared[pair] : 0u;
                linked_weight += (after_object == linked_object) ? tree->triple_shared[pair] : 0u;
            }
        }
        unsigned long long truth_web = 0ULL;
        unsigned long long linked_web = 0ULL;
        if ((edge_near_start != NULL) && (edge_later_near_start != NULL) && (edge_seen != NULL))
        {
            edge_mark += 1u;
            truth_web = (truth_object != 0xFFFFFFFFu)
                            ? web_count(tree, own_object, next_tree, truth_object, edge_near_start, edge_nearby,
                                        edge_later_near_start, edge_later_nearby, edge_seen, edge_mark)
                            : 0ULL;
            edge_mark += 1u;
            linked_web = (linked_object != 0xFFFFFFFFu)
                             ? web_count(tree, own_object, next_tree, linked_object, edge_near_start, edge_nearby,
                                         edge_later_near_start, edge_later_nearby, edge_seen, edge_mark)
                             : 0ULL;
        }
        const unsigned int own_unified = unified_of[index.node_offset[tree_index_of_time[from[0]]] + own_object];
        const unsigned int truth_unified =
            (truth_leaf >= 0) ? unified_of[index.node_offset[tree_index_of_time[to[0]]] + truth_object] : 0xFFFFFFFFu;
        const unsigned int unified_links = unified_start[own_unified + 1u] - unified_start[own_unified];
        unsigned int unified_matches_truth = 0u;
        for (unsigned int link = unified_start[own_unified]; link < unified_start[own_unified + 1u]; link += 1u)
        {
            unified_matches_truth +=
                (unsigned int)((unsigned int)(unified_link[link] & 0xFFFFFFFFULL) == truth_unified);
        }
        fprintf(rules->edges, "%s\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%u\t%u\t%u\t%u\t%u\t%u\t", sample,
                from[0], EDGE_STATUS_NAMES[status], to[1] - from[1], to[2] - from[2], to[3] - from[3],
                tree->lag_to_next[0], tree->lag_to_next[1], tree->lag_to_next[2], carried[0], carried[1], carried[2],
                tree->sizes[leaf], (tree->forward != NULL) ? 1u : 0u, final_score, tree->null_count, null_at_least,
                null_best);
        for (unsigned int draw = 0u; (tree->null_final_score != NULL) && (draw < tree->null_count); draw += 1u)
        {
            fprintf(rules->edges, "%s%u", (draw == 0u) ? "" : ",",
                    tree->null_final_score[((size_t)draw * ((size_t)tree->leaf_count + 1u)) + leaf]);
        }
        const int pool_present = (rules->pool != NULL) && (tree->pool_start != NULL);
        const unsigned int pool_first = (pool_present != 0) ? tree->pool_start[own_object] : 0u;
        const unsigned int pool_end = (pool_present != 0) ? tree->pool_start[own_object + 1u] : 0u;
        for (unsigned int target_at = pool_first; target_at < pool_end; target_at += 1u)
        {
            const unsigned int candidate = tree->pool_target[target_at];
            unsigned int candidate_voxels = 0u;
            for (unsigned int member = next_tree->member_start[candidate];
                 member < next_tree->member_start[candidate + 1u]; member += 1u)
            {
                candidate_voxels += next_tree->sizes[next_tree->members[member]];
            }
            unsigned int linked = 0u;
            for (unsigned int link = tree->link_start[own_object]; link < tree->link_start[own_object + 1u]; link += 1u)
            {
                linked += (unsigned int)(tree->link_target[link] == candidate);
            }
            fprintf(rules->pool, "%s\t%d\t%s\t%u\t%u\t%u\t%u\t%u\t%llu\t%llu\t%u\t%u\t%llu\t%u\n", sample, from[0],
                    EDGE_STATUS_NAMES[status], own_object, candidate, (unsigned int)(candidate == truth_object), linked,
                    tree->pool_weight[target_at], tree->pool_cost[target_at] >> LINK_MAGNITUDE_BITS,
                    tree->pool_cost[target_at] & LINK_MAGNITUDE_MASK, candidate_voxels,
                    next_tree->member_start[candidate + 1u] - next_tree->member_start[candidate],
                    (unsigned long long)tree->sizes[leaf], members);
        }
        const TreeFrame *const beyond =
            ((tree_index_of_time[from[0]] + 2u) < frame_count) ? &frames[tree_index_of_time[from[0]] + 2u] : NULL;
        const int arm_leaf =
            ((tree->arm_forward != NULL) && (tree->arm_count != 0u)) ? tree->arm_forward[leaf] : CLIMB_MACHINE_NO_LEAF;
        const unsigned int arm_object =
            ((arm_leaf >= 0) && (beyond != NULL)) ? beyond->object_of[(unsigned int)arm_leaf] : 0xFFFFFFFFu;
        unsigned int truth_onward = 0xFFFFFFFFu;
        for (unsigned int link = 0u;
             (truth_leaf >= 0) && (next_tree->link_start != NULL) && (beyond != NULL) &&
             (link < (next_tree->link_start[truth_object + 1u] - next_tree->link_start[truth_object]));
             link += 1u)
        {
            truth_onward = next_tree->link_target[next_tree->link_start[truth_object] + link];
        }
        unsigned int linked_onward = 0xFFFFFFFFu;
        for (unsigned int link = 0u;
             (linked_object != 0xFFFFFFFFu) && (next_tree->link_start != NULL) && (beyond != NULL) &&
             (link < (next_tree->link_start[linked_object + 1u] - next_tree->link_start[linked_object]));
             link += 1u)
        {
            linked_onward = next_tree->link_target[next_tree->link_start[linked_object] + link];
        }
        fprintf(rules->edges, "\t%u\t%u\t%u", arm_object, truth_onward, linked_onward);
        fprintf(rules->edges, "\t%d\t%d\t%d\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%llu\t%llu\t%llu\t%llu\n",
                truth_leaf, chosen_leaf, best_leaf, truth_shared, chosen_shared, best_shared, truth_mutual, one_object,
                object_links, object_links_truth, unified_links, unified_matches_truth, members, truth_weight,
                linked_weight, truth_web, linked_web);
    }
    ok = ok && ((rules->nodes == NULL) || assign_bodies(frames, frame_count, (unsigned int)rules->mass_band));
    for (unsigned int frame = 0u; (ok != 0) && (rules->nodes != NULL) && (frame < frame_count); frame += 1u)
    {
        const TreeFrame *const tree = &frames[frame];
        const size_t plane = (size_t)buffers.height * buffers.width;
        for (unsigned int leaf = 0u; (tree->peaks != NULL) && (leaf < tree->leaf_count); leaf += 1u)
        {
            const unsigned int peak = tree->peaks[leaf];
            unsigned long long departure = 0ull;
            for (unsigned int axis = 0u; (tree->forward_lag != NULL) && (axis < 3u); axis += 1u)
            {
                const long long apart =
                    (long long)tree->forward_lag[(3u * leaf) + axis] - (long long)tree->lag_to_next[axis];
                departure += (unsigned long long)(apart * apart) * (unsigned long long)AXIS_WEIGHTS[axis];
            }
            const unsigned int object = tree->object_of[leaf];
            const unsigned int object_links =
                (tree->link_start != NULL) ? (tree->link_start[object + 1u] - tree->link_start[object]) : 0u;
            const int object_link = ((tree->link_start != NULL) && (object_links != 0u))
                                        ? (int)tree->link_target[tree->link_start[object]]
                                        : -1;
            const unsigned int final_score_here =
                (tree->forward_final_score != NULL) ? tree->forward_final_score[leaf] : 0u;
            unsigned int null_at_least = 0u;
            unsigned int null_best = 0u;
            for (unsigned int draw = 0u; (tree->null_final_score != NULL) && (draw < tree->null_count); draw += 1u)
            {
                const unsigned int drawn =
                    tree->null_final_score[((size_t)draw * ((size_t)tree->leaf_count + 1u)) + leaf];
                null_at_least += (unsigned int)(drawn >= final_score_here);
                null_best = (drawn > null_best) ? drawn : null_best;
            }
            fprintf(rules->nodes, "%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%d\t%llu\t%u\t%d\t%u\t%u\t%u\t%u\t%d\t%u\t%d",
                    sample, tree->time, leaf, (unsigned int)(peak / plane),
                    (unsigned int)((peak % plane) / buffers.width), (unsigned int)((peak % plane) % buffers.width),
                    tree->sizes[leaf], object,
                    (tree->member_start != NULL) ? (tree->member_start[object + 1u] - tree->member_start[object]) : 0u,
                    (tree->forward != NULL) ? tree->forward[leaf] : -1, departure,
                    (tree->forward_final_score != NULL) ? tree->forward_final_score[leaf] : 0u, object_link,
                    object_links, tree->null_count, null_at_least, null_best,
                    (tree->majority_target != NULL) ? (int)tree->majority_target[leaf] : -1,
                    (tree->majority_agreed_rounds != NULL) ? tree->majority_agreed_rounds[leaf] : 0u,
                    (tree->backward != NULL) ? tree->backward[leaf] : -1);
            fprintf(rules->nodes, "\t%u\t%u\t%d\t%u", tree->body_id[leaf], tree->body_state[leaf],
                    (tree->body_parent[leaf] == BODY_NONE) ? -1 : (int)tree->body_parent[leaf],
                    (tree->touches != NULL) ? tree->touches[leaf] : 0u);
            const int split_from =
                (((tree->body_state[leaf] & BODY_SPLIT) != 0u) && (tree->backward != NULL)) ? tree->backward[leaf] : -1;
            fprintf(rules->nodes, "\t%d\n", split_from);
        }
    }
    if (ok && (object || (rules->coherence != NULL) || (rules->vis_index != NULL) || (rules->export_directory != NULL)))
    {
        CoherenceInputs inputs;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &inputs, .bytes = sizeof(inputs), .val = 0);
        inputs.sample = sample;
        inputs.volume = volume;
        inputs.key = &key;
        inputs.frames = frames;
        inputs.frame_count = frame_count;
        inputs.node_offset = index.node_offset;
        inputs.unified_of = unified_of;
        inputs.unified_count = unified_count;
        inputs.node_leaf = node_leaf;
        inputs.tree_index_of_time = tree_index_of_time;
        inputs.volume_frames = volume_frames;
        inputs.edge_status = edge_status;
        inputs.unified_start = unified_start;
        inputs.unified_link = unified_link;
        inputs.vis_directory = rules->vis_directory;
        inputs.vis_index = rules->vis_index;
        inputs.unified_first = unified_first;
        if ((rules->coherence != NULL) || (rules->vis_index != NULL))
        {
            ok = read_coherence(&buffers, &inputs, rules->coherence);
        }
        if ((ok != 0) && (rules->export_directory != NULL))
        {
            ok = export_capacity(&buffers, &inputs, rules->export_directory);
        }
        if (ok && object)
        {
            object_first_run[frame_count] = (unsigned int)object_run_count;
            ok = export_object(&buffers, &inputs, object_runs, object_first_run, object_leaf_runs, rules);
        }
    }
    platform_cell_released(frame_runs, platform_place_g_program_owner);
    platform_cell_released(object_runs, platform_place_g_program_owner);
    platform_cell_released(object_leaf_runs, platform_place_g_program_owner);
    platform_cell_released(object_first_run, platform_place_g_program_owner);
    platform_cell_released(object_leaf_code, platform_place_g_program_owner);
    const unsigned long long finished = engine_clock_microseconds() / 1000ull;
    unsigned int object_total = 0u;
    unsigned int leaf_total = 0u;
    unsigned int largest_object = 0u;
    for (unsigned int frame = 0u; (ok != 0) && (frame < frame_count); frame += 1u)
    {
        const TreeFrame *const tree = &frames[frame];
        object_total += tree->object_count;
        leaf_total += tree->leaf_count;
        for (unsigned int object = 0u; (tree->member_start != NULL) && (object < tree->object_count); object += 1u)
        {
            const unsigned int members = tree->member_start[object + 1u] - tree->member_start[object];
            largest_object = (members > largest_object) ? members : largest_object;
        }
    }
    if (ok != 0)
    {
        const unsigned long long scored =
            results->correct + results->branched + results->wrong + results->unlinked + results->missed;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            {
                char linea_piece[24u + 1u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                (void)mmgr_line_put_max(&linea_piece_line, sample, 24u);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 24u);
            }
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                mmgr_line_decimal(&linea_piece_line, (uint64_t)(scored), 1u);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 6u);
            }
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, results->correct, 1u);
            mmgr_line_put(output_line, "/");
            mmgr_line_decimal(output_line, results->branched, 1u);
            mmgr_line_put(output_line, "/");
            mmgr_line_decimal(output_line, results->wrong, 1u);
            mmgr_line_put(output_line, "/");
            mmgr_line_decimal(output_line, results->unlinked, 1u);
            mmgr_line_put(output_line, "/");
            mmgr_line_decimal(output_line, results->missed, 1u);
            mmgr_line_put(output_line, "   ");
            mmgr_line_decimal(output_line, frame_count, 1u);
            mmgr_line_put(output_line, " frames, ");
            mmgr_line_decimal(output_line, object_total, 1u);
            mmgr_line_put(output_line, " objects of ");
            mmgr_line_decimal(output_line, leaf_total, 1u);
            mmgr_line_put(output_line, " leaves, largest ");
            mmgr_line_decimal(output_line, largest_object, 1u);
            mmgr_line_put(output_line, ", rejoined ");
            mmgr_line_decimal(output_line, rejoined, 1u);
            mmgr_line_put(output_line, ", engines ");
            mmgr_line_decimal(output_line, engines_done - started, 1u);
            mmgr_line_put(output_line, " ms (read ");
            mmgr_line_decimal(output_line, clocks.read / 1000ULL, 1u);
            mmgr_line_put(output_line, ", bodies ");
            mmgr_line_decimal(output_line, clocks.bodies / 1000ULL, 1u);
            mmgr_line_put(output_line, ", store ");
            mmgr_line_decimal(output_line, clocks.store / 1000ULL, 1u);
            mmgr_line_put(output_line, ", ties ");
            mmgr_line_decimal(output_line, clocks.ties / 1000ULL, 1u);
            mmgr_line_put(output_line, ", motion ");
            mmgr_line_decimal(output_line, clocks.motion / 1000ULL, 1u);
            mmgr_line_put(output_line, ", landing ");
            mmgr_line_decimal(output_line, clocks.landing / 1000ULL, 1u);
            mmgr_line_put(output_line, ", overlap ");
            mmgr_line_decimal(output_line, clocks.overlap / 1000ULL, 1u);
            mmgr_line_put(output_line, ", climb ");
            mmgr_line_decimal(output_line, clocks.climb / 1000ULL, 1u);
            mmgr_line_put(output_line, "), tree ");
            mmgr_line_decimal(output_line, finished - engines_done, 1u);
            mmgr_line_put(output_line, " ms");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_line_flushed();
        FILE *const log = log_open();
        if (log != NULL)
        {
            char when[32];
            log_when(when, sizeof(when));
            fprintf(log,
                    "%s\tsample\t%s\t%s\t%u\t%u\t%u\t%u\t%u\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu"
                    "\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu"
                    "\t%llu\t%llu\t%llu\t%u\t%llu\t%llu\t%llu\n",
                    when, log_rules(), sample, frame_count, object_total, leaf_total, largest_object, rejoined, scored,
                    results->correct, results->branched, results->wrong, results->unlinked, results->missed,
                    clocks.read / 1000ULL, clocks.bodies / 1000ULL, clocks.store / 1000ULL, clocks.ties / 1000ULL,
                    clocks.motion / 1000ULL, clocks.landing / 1000ULL, clocks.overlap / 1000ULL, clocks.climb / 1000ULL,
                    engines_done - started, finished - engines_done, g_web_queried - s_web_logged[0],
                    g_web_moved - s_web_logged[1], g_web_capped - s_web_logged[2], WEB_MEMBERS,
                    g_damp_leaves - s_web_logged[3], g_damp_landings - s_web_logged[4],
                    (unsigned long long)DAMP_DEVIATIONS);
            fclose(log);
            s_web_logged[0] = g_web_queried;
            s_web_logged[1] = g_web_moved;
            s_web_logged[2] = g_web_capped;
            s_web_logged[3] = g_damp_leaves;
            s_web_logged[4] = g_damp_landings;
        }
    }

    platform_cell_released(edge_status, platform_place_g_program_owner);
    platform_cell_released(unified_first, platform_place_g_program_owner);
    platform_cell_released(successors, platform_place_g_program_owner);
    platform_cell_released(unified_start, platform_place_g_program_owner);
    platform_cell_released(unified_link, platform_place_g_program_owner);
    platform_cell_released(rank_of_root, platform_place_g_program_owner);
    platform_cell_released(unified_of, platform_place_g_program_owner);
    platform_cell_released(parent, platform_place_g_program_owner);
    platform_cell_released(index.node_frame, platform_place_g_program_owner);
    platform_cell_released(index.node_offset, platform_place_g_program_owner);
    for (unsigned int frame = 0u; (frames != NULL) && (frame < frame_count); frame += 1u)
    {
        TreeFrame *const tree = &frames[frame];
        platform_cell_released(tree->peaks, platform_place_g_program_owner);
        platform_cell_released(tree->sizes, platform_place_g_program_owner);
        platform_cell_released(tree->sums, platform_place_g_program_owner);
        platform_cell_released(tree->moments, platform_place_g_program_owner);
        platform_cell_released(tree->exposed, platform_place_g_program_owner);
        platform_cell_released(tree->touches, platform_place_g_program_owner);
        platform_cell_released(tree->body_id, platform_place_g_program_owner);
        platform_cell_released(tree->body_parent, platform_place_g_program_owner);
        platform_cell_released(tree->body_state, platform_place_g_program_owner);
        platform_cell_released(tree->contact_faces, platform_place_g_program_owner);
        platform_cell_released(tree->extents, platform_place_g_program_owner);
        platform_cell_released(tree->joined, platform_place_g_program_owner);
        platform_cell_released(tree->forward, platform_place_g_program_owner);
        platform_cell_released(tree->forward_lag, platform_place_g_program_owner);
        platform_cell_released(tree->backward_lag, platform_place_g_program_owner);
        platform_cell_released(tree->check_forward_lag, platform_place_g_program_owner);
        platform_cell_released(tree->check_backward_lag, platform_place_g_program_owner);
        platform_cell_released(tree->forward_final_score, platform_place_g_program_owner);
        platform_cell_released(tree->null_final_score, platform_place_g_program_owner);
        platform_cell_released(tree->arm_forward, platform_place_g_program_owner);
        platform_cell_released(tree->backward, platform_place_g_program_owner);
        platform_cell_released(tree->triple_start, platform_place_g_program_owner);
        platform_cell_released(tree->triple_after, platform_place_g_program_owner);
        platform_cell_released(tree->triple_shared, platform_place_g_program_owner);
        platform_cell_released(tree->triple_still, platform_place_g_program_owner);
        platform_cell_released(tree->pool_start, platform_place_g_program_owner);
        platform_cell_released(tree->pool_target, platform_place_g_program_owner);
        platform_cell_released(tree->pool_weight, platform_place_g_program_owner);
        platform_cell_released(tree->pool_cost, platform_place_g_program_owner);
        platform_cell_released(tree->object_of, platform_place_g_program_owner);
        platform_cell_released(tree->member_start, platform_place_g_program_owner);
        platform_cell_released(tree->members, platform_place_g_program_owner);
        platform_cell_released(tree->link_start, platform_place_g_program_owner);
        platform_cell_released(tree->link_target, platform_place_g_program_owner);
    }
    platform_cell_released(frames, platform_place_g_program_owner);
    platform_cell_released(node_leaf, platform_place_g_program_owner);
    platform_cell_released(tree_index_of_time, platform_place_g_program_owner);
    platform_cell_released(reached, platform_place_g_program_owner);
    platform_cell_released(volume, platform_place_g_program_owner);
    platform_cell_released(buffers.volume, platform_place_g_program_owner);
    platform_cell_released(buffers.peak_indices, platform_place_g_program_owner);
    platform_cell_released(buffers.sizes, platform_place_g_program_owner);
    platform_cell_released(buffers.sums, platform_place_g_program_owner);
    platform_cell_released(buffers.peak_limbs, platform_place_g_program_owner);
    platform_cell_released(buffers.bodies, platform_place_g_program_owner);
    platform_cell_released(buffers.adjacency, platform_place_g_program_owner);
    platform_cell_released(buffers.joined, platform_place_g_program_owner);
    platform_cell_released(buffers.overlap_before, platform_place_g_program_owner);
    platform_cell_released(buffers.overlap_after, platform_place_g_program_owner);
    platform_cell_released(buffers.overlap_shared, platform_place_g_program_owner);
    for (unsigned int target_at = 0u; target_at < 2u; target_at += 1u)
    {
        platform_cell_released(buffers.labels[target_at], platform_place_g_program_owner);
        platform_cell_released(buffers.positive[target_at], platform_place_g_program_owner);
        platform_cell_released(buffers.leaf_at_peak[target_at], platform_place_g_program_owner);
        platform_cell_released(buffers.leaf_start[target_at], platform_place_g_program_owner);
        platform_cell_released(buffers.leaf_voxels[target_at], platform_place_g_program_owner);
    }
    climb_machine_close(buffers.machine);
    release_answer_key(&key);
    return ok;
}
