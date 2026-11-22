// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../../../src/cu/engine/nbody/max_tree/max_tree.h"
#include "../../../../src/platform/platform.h"

typedef struct
{
    unsigned int present;
    unsigned int alone;
} ScoreSlidePresence;

static ScoreSlidePresence score_slide_results(const unsigned int *partition, unsigned int keys,
                                              unsigned char *ever_alone)
{
    ScoreSlidePresence presence = {0u, 0u};
    for (unsigned int key = 0u; key < keys; key += 1u)
    {
        unsigned int sharers = 0u;
        for (unsigned int other = 0u; other < keys; other += 1u)
        {
            sharers += (partition[other] == partition[key]) ? 1u : 0u;
        }
        const unsigned int present = (partition[key] != MAX_TREE_ABSENT) ? 1u : 0u;
        const unsigned int alone = present * ((sharers == 1u) ? 1u : 0u);
        presence.present += present;
        presence.alone += alone;
        if (ever_alone != NULL)
        {
            // alone is 0 or 1. It narrows to unsigned char exactly
            ever_alone[key] |= (unsigned char)alone;
        }
    }
    return presence;
}

typedef struct
{
    unsigned int frames;
    unsigned int keys;
    unsigned int present_at_threshold;
    unsigned int alone_at_threshold;
    unsigned int separable;
    unsigned int best_alone;
    unsigned long long labelings;
} ScoreSlideResults;

typedef struct
{
    unsigned int *levels;
    unsigned int count;
} ScoreSlideLevels;

static int score_slide_frame(const AnswerKey *key, unsigned int time, const EngineBuffers *buffers,
                             ScoreSlideResults *results, ScoreSlideLevels *levels)
{
    levels->levels = NULL;
    levels->count = 0u;
    unsigned int *const key_voxels = (unsigned int *)platform_cell_taken(
        ((size_t)key->node_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int keys = 0u;
    for (unsigned int node = 0u; (key_voxels != NULL) && (node < key->node_count); node += 1u)
    {
        const int *const place = &key->node_coordinates[(size_t)node * 4u];
        // the time re-signs to unsigned int only after it is checked non-negative
        if ((place[0] < 0) || ((unsigned int)place[0] != time))
        {
            continue;
        }
        // the key contains every place on the grid. Each coordinate is non-negative and re-signs to unsigned int
        // exactly
        key_voxels[keys] = ((((unsigned int)place[1] * buffers->height) + (unsigned int)place[2]) * buffers->width) +
                           (unsigned int)place[3];
        keys += 1u;
    }
    if ((key_voxels == NULL) || (keys == 0u))
    {
        platform_cell_released(key_voxels, platform_place_g_program_owner);
        return (key_voxels != NULL) ? 1 : 0;
    }
    const unsigned int step_capacity = (2u * keys) + 2u;
    MaxTreeSlideStep *const steps = (MaxTreeSlideStep *)platform_cell_taken(
        (size_t)step_capacity * sizeof(MaxTreeSlideStep), platform_place_g_program_owner, 0);
    unsigned int *const partitions = (unsigned int *)platform_cell_taken(
        (size_t)step_capacity * keys * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned char *const ever_alone =
        (unsigned char *)platform_cell_cleared(keys, 1u, platform_place_g_program_owner, 0);
    unsigned int step_count = 0u;
    unsigned int top = 0u;
    unsigned int threshold = 0u;
    unsigned int labelings = 0u;
    EngineError error;
    memset(&error, 0, sizeof(error));
    const MaxTreeSlideRequest request = {.probe_voxels = key_voxels,
                                         .probe_count = keys,
                                         .steps = steps,
                                         .partitions = partitions,
                                         .step_capacity = step_capacity,
                                         .step_count = &step_count,
                                         .top_level = &top,
                                         .threshold = &threshold,
                                         .labelings = &labelings,
                                         .error = &error};
    const int steps_succeeded =
        (steps != NULL) && (partitions != NULL) && (ever_alone != NULL) && (max_tree_slide(&request) == 0L);
    if ((steps != NULL) && (partitions != NULL) && (ever_alone != NULL) && !steps_succeeded)
    {
        track_error_report("slide", &error);
    }
    unsigned int present_at_threshold = 0u;
    unsigned int alone_at_threshold = 0u;
    unsigned int best_alone = 0u;
    for (unsigned int step = 0u; steps_succeeded && (step < step_count); step += 1u)
    {
        const ScoreSlidePresence presence = score_slide_results(&partitions[(size_t)step * keys], keys, ever_alone);
        present_at_threshold = (steps[step].level >= threshold) ? presence.present : present_at_threshold;
        alone_at_threshold = (steps[step].level >= threshold) ? presence.alone : alone_at_threshold;
        best_alone = (presence.alone > best_alone) ? presence.alone : best_alone;
    }
    unsigned int separable = 0u;
    for (unsigned int each = 0u; steps_succeeded && (each < keys); each += 1u)
    {
        separable += (unsigned int)ever_alone[each];
    }
    if (steps_succeeded)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    slide t=");
            mmgr_line_decimal(output_line, time, 1u);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal(output_line, keys, 1u);
            mmgr_line_put(output_line, " key cells; the threshold at level ");
            mmgr_line_decimal(output_line, threshold, 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal(output_line, top, 1u);
            mmgr_line_put(output_line, " captures ");
            mmgr_line_decimal(output_line, present_at_threshold, 1u);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, alone_at_threshold, 1u);
            mmgr_line_put(output_line, " alone; alone at some level ");
            mmgr_line_decimal(output_line, separable, 1u);
            mmgr_line_put(output_line, ", at the best single level ");
            mmgr_line_decimal(output_line, best_alone, 1u);
            mmgr_line_put(output_line, "; level:components:present:alone");
            platform_line_out_ended();
        }
        for (unsigned int step = 0u; step < step_count; step += 1u)
        {
            const ScoreSlidePresence presence = score_slide_results(&partitions[(size_t)step * keys], keys, NULL);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, " ");
                mmgr_line_decimal(output_line, steps[step].level, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, steps[step].components, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, presence.present, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, presence.alone, 1u);
                platform_line_out_ended();
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        results->frames += 1u;
        results->keys += keys;
        results->present_at_threshold += present_at_threshold;
        results->alone_at_threshold += alone_at_threshold;
        results->separable += separable;
        results->best_alone += best_alone;
        results->labelings += labelings;
        levels->levels = (unsigned int *)platform_cell_taken(((size_t)step_count + 1u) * sizeof(unsigned int),
                                                             platform_place_g_program_owner, 0);
    }
    for (unsigned int step = 0u; (levels->levels != NULL) && (step < step_count); step += 1u)
    {
        levels->levels[step] = steps[step].level;
    }
    if (levels->levels != NULL)
    {
        levels->levels[step_count] = threshold;
        levels->count = step_count + 1u;
    }
    platform_cell_released(key_voxels, platform_place_g_program_owner);
    platform_cell_released(steps, platform_place_g_program_owner);
    platform_cell_released(partitions, platform_place_g_program_owner);
    platform_cell_released(ever_alone, platform_place_g_program_owner);
    return steps_succeeded && (levels->levels != NULL);
}

typedef struct
{
    unsigned int pairs;
    unsigned long long evaluations;
    unsigned long long components_at_threshold;
    unsigned long long one_at_threshold;
    unsigned long long mutual_at_threshold;
    unsigned long long best_mutual;
    unsigned long long disappear_at_threshold;
    unsigned long long appear_at_threshold;
    unsigned long long divisions_at_threshold;
    unsigned long long merges_at_threshold;
    unsigned long long key_divisions;
    unsigned long long parted_at_threshold;
    unsigned long long caught_at_threshold;
    unsigned long long best_parted;
    unsigned long long best_caught;
    unsigned long long best_present;
    unsigned long long best_apart;
    unsigned int mutual_differ;
    unsigned long long chosen_components;
    unsigned long long chosen_later_components;
    unsigned long long chosen_mutual;
    unsigned long long chosen_cells_present;
    unsigned long long chosen_cells_alone;
    unsigned long long chosen_parted;
    unsigned long long chosen_caught;
    unsigned long long threshold_cells_present;
    unsigned long long threshold_cells_alone;
    unsigned long long key_cells;
} ScoreOverlapResults;

static int score_code_descending(const void *left, const void *right)
{
    const unsigned int one = *(const unsigned int *)left;
    const unsigned int other = *(const unsigned int *)right;
    return (one < other) ? 1 : ((one > other) ? -1 : 0);
}

static int score_peak_levels(const EngineBuffers *buffers, const TreeFrame *frame, ScoreSlideLevels *levels)
{
    levels->count = 0u;
    // leaf_count widens from unsigned int to size_t; leaf_count + 1 cannot wrap before malloc sees it
    levels->levels = (unsigned int *)platform_cell_taken(((size_t)frame->leaf_count + 1u) * sizeof(unsigned int),
                                                         platform_place_g_program_owner, 0);
    MaxTreeSlideStep no_step;
    unsigned int step_count = 0u;
    unsigned int threshold = 0u;
    EngineError error;
    memset(&error, 0, sizeof(error));
    const MaxTreeSlideRequest request = {
        .steps = &no_step, .step_capacity = 1u, .step_count = &step_count, .threshold = &threshold, .error = &error};
    if ((levels->levels == NULL) || (max_tree_slide(&request) != 0L))
    {
        if (levels->levels != NULL)
        {
            track_error_report("peak levels", &error);
        }
        platform_cell_released(levels->levels, platform_place_g_program_owner);
        levels->levels = NULL;
        return 0;
    }
    for (unsigned int leaf = 0u; leaf < frame->leaf_count; leaf += 1u)
    {
        levels->levels[leaf] = buffers->bodies[leaf].code;
    }
    qsort(levels->levels, frame->leaf_count, sizeof(unsigned int), score_code_descending);
    for (unsigned int leaf = 0u; leaf < frame->leaf_count; leaf += 1u)
    {
        if ((levels->count == 0u) || (levels->levels[levels->count - 1u] != levels->levels[leaf]))
        {
            levels->levels[levels->count] = levels->levels[leaf];
            levels->count += 1u;
        }
    }
    levels->levels[levels->count] = threshold;
    levels->count += 1u;
    return 1;
}

typedef struct
{
    const AnswerKey *key;
    const unsigned int *successors;
    unsigned int height;
    unsigned int width;
} ScoreKeyDivisions;

static unsigned int score_key_voxel(const ScoreKeyDivisions *divisions, unsigned int node)
{
    const int *const place = &divisions->key->node_coordinates[(size_t)node * 4u];
    // the key contains every place on the grid. Each coordinate is non-negative and re-signs to unsigned int exactly
    return ((((unsigned int)place[1] * divisions->height) + (unsigned int)place[2]) * divisions->width) +
           (unsigned int)place[3];
}

static unsigned int *score_key_division_voxels(const ScoreKeyDivisions *divisions, unsigned int earlier_time,
                                               unsigned int later_time, unsigned int *count)
{
    const AnswerKey *const key = divisions->key;
    // node_count widens from unsigned int to size_t; (node_count + 1) * 3 cannot wrap before malloc sees it
    unsigned int *const voxels = (unsigned int *)platform_cell_taken(
        ((size_t)key->node_count + 1u) * 3u * sizeof(unsigned int), platform_place_g_program_owner, 0);
    *count = 0u;
    for (unsigned int node = 0u; (voxels != NULL) && (node < key->node_count); node += 1u)
    {
        const unsigned int *const successors = &divisions->successors[(size_t)node * 3u];
        const int *const place = &key->node_coordinates[(size_t)node * 4u];
        const int *const one = &key->node_coordinates[(size_t)successors[1] * 4u];
        const int *const other = &key->node_coordinates[(size_t)successors[2] * 4u];
        // each time re-signs to unsigned int only after it is checked non-negative
        const int ok = (successors[0] == 2u) && (place[0] >= 0) && ((unsigned int)place[0] == earlier_time) &&
                       (one[0] >= 0) && ((unsigned int)one[0] == later_time) && (other[0] >= 0) &&
                       ((unsigned int)other[0] == later_time);
        if (ok)
        {
            voxels[(3u * *count)] = score_key_voxel(divisions, node);
            voxels[(3u * *count) + 1u] = score_key_voxel(divisions, successors[1]);
            voxels[(3u * *count) + 2u] = score_key_voxel(divisions, successors[2]);
            *count += 1u;
        }
    }
    return voxels;
}

static unsigned int *score_key_cell_voxels(const ScoreKeyDivisions *divisions, unsigned int time, unsigned int *count)
{
    const AnswerKey *const key = divisions->key;
    // node_count widens from unsigned int to size_t; node_count + 1 cannot wrap before malloc sees it
    unsigned int *const voxels = (unsigned int *)platform_cell_taken(
        ((size_t)key->node_count + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    *count = 0u;
    for (unsigned int node = 0u; (voxels != NULL) && (node < key->node_count); node += 1u)
    {
        const int *const place = &key->node_coordinates[(size_t)node * 4u];
        // the time re-signs to unsigned int only after it is checked non-negative
        if ((place[0] >= 0) && ((unsigned int)place[0] == time))
        {
            voxels[*count] = score_key_voxel(divisions, node);
            *count += 1u;
        }
    }
    return voxels;
}

typedef struct
{
    unsigned int key_present;
    unsigned int key_apart;
    unsigned int key_parted;
    unsigned int key_caught;
    unsigned int key_cells_present;
    unsigned int key_cells_alone;
} ScoreOverlapGrade;

static ScoreOverlapGrade score_overlap_grade(const MaxTreeOverlapProbe *probes, const unsigned char *links_present,
                                             unsigned int key_divisions, unsigned int key_cells)
{
    ScoreOverlapGrade grade = {0u, 0u, 0u, 0u, 0u, 0u};
    const MaxTreeOverlapProbe *const parents = probes;
    const MaxTreeOverlapProbe *const cells = &probes[key_divisions];
    const MaxTreeOverlapProbe *const daughters = &probes[key_divisions + key_cells];
    for (unsigned int division = 0u; division < key_divisions; division += 1u)
    {
        const MaxTreeOverlapProbe *const parent = &parents[division];
        const MaxTreeOverlapProbe *const one = &daughters[division];
        const MaxTreeOverlapProbe *const other = &daughters[key_divisions + division];
        const unsigned int present = (unsigned int)((parent->root != MAX_TREE_ABSENT) &&
                                                    (one->root != MAX_TREE_ABSENT) && (other->root != MAX_TREE_ABSENT));
        const unsigned int apart = present & (unsigned int)(one->root != other->root);
        const unsigned int parted = apart & (unsigned int)(links_present[2u * division] != 0u) &
                                    (unsigned int)(links_present[(2u * division) + 1u] != 0u);
        const unsigned int caught = parted & (unsigned int)((parent->degree == 2u) && (parent->backs == 2u));
        grade.key_present += present;
        grade.key_apart += apart;
        grade.key_parted += parted;
        grade.key_caught += caught;
    }
    for (unsigned int cell = 0u; cell < key_cells; cell += 1u)
    {
        unsigned int sharers = 0u;
        for (unsigned int other = 0u; other < key_cells; other += 1u)
        {
            sharers += (cells[other].root == cells[cell].root) ? 1u : 0u;
        }
        const unsigned int present = (unsigned int)(cells[cell].root != MAX_TREE_ABSENT);
        grade.key_cells_present += present;
        grade.key_cells_alone += present * (unsigned int)(sharers == 1u);
    }
    return grade;
}

static int score_overlap_pair(const TreeFrame *earlier, const TreeFrame *later, const ScoreSlideLevels *earlier_levels,
                              const ScoreSlideLevels *later_levels, const ScoreKeyDivisions *divisions, int every_step,
                              ScoreOverlapResults *results)
{
    unsigned int key_divisions = 0u;
    unsigned int key_cells = 0u;
    unsigned int *const division_voxels =
        score_key_division_voxels(divisions, earlier->time, later->time, &key_divisions);
    unsigned int *const key_cell_voxels = score_key_cell_voxels(divisions, earlier->time, &key_cells);
    const unsigned int step_capacity = earlier_levels->count + later_levels->count;
    if ((division_voxels == NULL) || (key_cell_voxels == NULL) || (step_capacity == 0u))
    {
        platform_cell_released(division_voxels, platform_place_g_program_owner);
        platform_cell_released(key_cell_voxels, platform_place_g_program_owner);
        return (division_voxels != NULL) && (key_cell_voxels != NULL);
    }
    const unsigned int earlier_probes = key_divisions + key_cells;
    const unsigned int later_probes = 2u * key_divisions;
    const size_t probes_a_step = (size_t)earlier_probes + later_probes;
    const unsigned int link_count = 2u * key_divisions;
    unsigned int *const earlier_voxels = (unsigned int *)platform_cell_taken(
        ((size_t)earlier_probes + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const later_voxels = (unsigned int *)platform_cell_taken(
        ((size_t)later_probes + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const links = (unsigned int *)platform_cell_taken(
        ((size_t)link_count + 1u) * 2u * sizeof(unsigned int), platform_place_g_program_owner, 0);
    MaxTreeOverlapStep *const steps = (MaxTreeOverlapStep *)platform_cell_taken(
        (size_t)step_capacity * sizeof(MaxTreeOverlapStep), platform_place_g_program_owner, 0);
    MaxTreeOverlapProbe *const probes = (MaxTreeOverlapProbe *)platform_cell_taken(
        ((size_t)step_capacity * probes_a_step + 1u) * sizeof(MaxTreeOverlapProbe), platform_place_g_program_owner, 0);
    unsigned char *const links_present = (unsigned char *)platform_cell_taken(((size_t)step_capacity * link_count) + 1u,
                                                                              platform_place_g_program_owner, 0);
    const int buffers_ok = (earlier_voxels != NULL) && (later_voxels != NULL) && (links != NULL) && (steps != NULL) &&
                           (probes != NULL) && (links_present != NULL);
    for (unsigned int division = 0u; buffers_ok && (division < key_divisions); division += 1u)
    {
        earlier_voxels[division] = division_voxels[3u * division];
        later_voxels[division] = division_voxels[(3u * division) + 1u];
        later_voxels[key_divisions + division] = division_voxels[(3u * division) + 2u];
        links[4u * division] = division;
        links[(4u * division) + 1u] = division;
        links[(4u * division) + 2u] = division;
        links[(4u * division) + 3u] = key_divisions + division;
    }
    for (unsigned int cell = 0u; buffers_ok && (cell < key_cells); cell += 1u)
    {
        earlier_voxels[key_divisions + cell] = key_cell_voxels[cell];
    }
    unsigned int step_count = 0u;
    EngineError error;
    memset(&error, 0, sizeof(error));
    MaxTreeOverlapRequest request;
    memset(&request, 0, sizeof(request));
    for (unsigned int axis = 0u; axis < 3u; axis += 1u)
    {
        request.lag[axis] = earlier->lag_to_next[axis];
    }
    request.earlier_levels = earlier_levels->levels;
    request.earlier_level_count = earlier_levels->count;
    request.later_levels = later_levels->levels;
    request.later_level_count = later_levels->count;
    request.probe_voxels[0] = earlier_voxels;
    request.probe_counts[0] = earlier_probes;
    request.probe_voxels[1] = later_voxels;
    request.probe_counts[1] = later_probes;
    request.probe_links = links;
    request.link_count = link_count;
    request.probes = probes;
    request.links_present = links_present;
    request.steps = steps;
    request.step_capacity = step_capacity;
    request.step_count = &step_count;
    request.error = &error;
    const int steps_succeeded = buffers_ok && (max_tree_overlap(&request) == 0L);
    if (buffers_ok && !steps_succeeded)
    {
        track_error_report("overlap", &error);
    }
    if (steps_succeeded && every_step)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    overlap t=");
            mmgr_line_decimal(output_line, earlier->time, 1u);
            mmgr_line_put(output_line, "->");
            mmgr_line_decimal(output_line, later->time, 1u);
            mmgr_line_put(output_line, " at the drift ");
            mmgr_line_decimal_signed(output_line, request.lag[0]);
            mmgr_line_put(output_line, ",");
            mmgr_line_decimal_signed(output_line, request.lag[1]);
            mmgr_line_put(output_line, ",");
            mmgr_line_decimal_signed(output_line, request.lag[2]);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, key_divisions, 1u);
            mmgr_line_put(
                output_line,
                " key divisions; origin:level:earlier code:later code:earlier components:later components:earlier "
                "one:later one:mutual:disappear:appear:divisions:merges:key present:key apart:key parted:key caught");
            platform_line_out_ended();
        }
        for (unsigned int step = 0u; step < step_count; step += 1u)
        {
            const MaxTreeOverlapStep *const at = &steps[step];
            const ScoreOverlapGrade grade = score_overlap_grade(
                &probes[step * probes_a_step], &links_present[(size_t)step * link_count], key_divisions, key_cells);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, " ");
                mmgr_line_decimal(output_line, at->origin, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->level, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->earlier_code, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->later_code, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->earlier_components, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->later_components, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->earlier_one, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->later_one, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->earlier_mutual, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->earlier_unpaired, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->later_unpaired, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->earlier_forked, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, at->later_forked, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, grade.key_present, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, grade.key_apart, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, grade.key_parted, 1u);
                mmgr_line_put(output_line, ":");
                mmgr_line_decimal(output_line, grade.key_caught, 1u);
                platform_line_out_ended();
            }
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    if (steps_succeeded)
    {
        unsigned int best_mutual = 0u;
        unsigned int best_parted = 0u;
        unsigned int best_caught = 0u;
        unsigned int best_present = 0u;
        unsigned int best_apart = 0u;
        unsigned int chosen = 0u;
        for (unsigned int step = 0u; step < step_count; step += 1u)
        {
            const MaxTreeOverlapStep *const at = &steps[step];
            const ScoreOverlapGrade grade = score_overlap_grade(
                &probes[step * probes_a_step], &links_present[(size_t)step * link_count], key_divisions, key_cells);
            chosen = (at->earlier_mutual > steps[chosen].earlier_mutual) ? step : chosen;
            best_present = (grade.key_present > best_present) ? grade.key_present : best_present;
            best_apart = (grade.key_apart > best_apart) ? grade.key_apart : best_apart;
            best_mutual = (at->earlier_mutual > best_mutual) ? at->earlier_mutual : best_mutual;
            best_parted = (grade.key_parted > best_parted) ? grade.key_parted : best_parted;
            best_caught = (grade.key_caught > best_caught) ? grade.key_caught : best_caught;
            results->mutual_differ += (at->earlier_mutual != at->later_mutual) ? 1u : 0u;
        }
        const MaxTreeOverlapStep *const pick = &steps[chosen];
        const ScoreOverlapGrade picked = score_overlap_grade(
            &probes[chosen * probes_a_step], &links_present[(size_t)chosen * link_count], key_divisions, key_cells);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    max mutual t=");
            mmgr_line_decimal(output_line, earlier->time, 1u);
            mmgr_line_put(output_line, "->");
            mmgr_line_decimal(output_line, later->time, 1u);
            mmgr_line_put(output_line, ": origin ");
            mmgr_line_decimal(output_line, pick->origin, 1u);
            mmgr_line_put(output_line, " level ");
            mmgr_line_decimal(output_line, pick->level, 1u);
            mmgr_line_put(output_line, ", codes ");
            mmgr_line_decimal(output_line, pick->earlier_code, 1u);
            mmgr_line_put(output_line, "/");
            mmgr_line_decimal(output_line, pick->later_code, 1u);
            mmgr_line_put(output_line, ", ");
            mmgr_line_decimal(output_line, pick->earlier_components, 1u);
            mmgr_line_put(output_line, "/");
            mmgr_line_decimal(output_line, pick->later_components, 1u);
            mmgr_line_put(output_line, " components, ");
            mmgr_line_decimal(output_line, pick->earlier_mutual, 1u);
            mmgr_line_put(output_line, " mutual, ");
            mmgr_line_decimal(output_line, picked.key_cells_present, 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal(output_line, key_cells, 1u);
            mmgr_line_put(output_line, " key cells present and ");
            mmgr_line_decimal(output_line, picked.key_cells_alone, 1u);
            mmgr_line_put(output_line, " alone, ");
            mmgr_line_decimal(output_line, picked.key_parted, 1u);
            mmgr_line_put(output_line, " of ");
            mmgr_line_decimal(output_line, key_divisions, 1u);
            mmgr_line_put(output_line, " key divisions parted and ");
            mmgr_line_decimal(output_line, picked.key_caught, 1u);
            mmgr_line_put(output_line, " caught");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        results->chosen_components += pick->earlier_components;
        results->chosen_later_components += pick->later_components;
        results->chosen_mutual += pick->earlier_mutual;
        results->chosen_cells_present += picked.key_cells_present;
        results->chosen_cells_alone += picked.key_cells_alone;
        results->chosen_parted += picked.key_parted;
        results->chosen_caught += picked.key_caught;
        results->key_cells += key_cells;
        results->pairs += 1u;
        results->evaluations += step_count;
        results->best_mutual += best_mutual;
        results->key_divisions += key_divisions;
        results->best_parted += best_parted;
        results->best_caught += best_caught;
        results->best_present += best_present;
        results->best_apart += best_apart;
        if (earlier_levels->count != 0u)
        {
            const unsigned int threshold_index = earlier_levels->count - 1u;
            const MaxTreeOverlapStep *const threshold_step = &steps[threshold_index];
            const ScoreOverlapGrade at_threshold =
                score_overlap_grade(&probes[threshold_index * probes_a_step],
                                    &links_present[(size_t)threshold_index * link_count], key_divisions, key_cells);
            results->components_at_threshold += threshold_step->earlier_components;
            results->one_at_threshold += threshold_step->earlier_one;
            results->mutual_at_threshold += threshold_step->earlier_mutual;
            results->disappear_at_threshold += threshold_step->earlier_unpaired;
            results->appear_at_threshold += threshold_step->later_unpaired;
            results->divisions_at_threshold += threshold_step->earlier_forked;
            results->merges_at_threshold += threshold_step->later_forked;
            results->parted_at_threshold += at_threshold.key_parted;
            results->caught_at_threshold += at_threshold.key_caught;
            results->threshold_cells_present += at_threshold.key_cells_present;
            results->threshold_cells_alone += at_threshold.key_cells_alone;
        }
    }
    platform_cell_released(earlier_voxels, platform_place_g_program_owner);
    platform_cell_released(later_voxels, platform_place_g_program_owner);
    platform_cell_released(links, platform_place_g_program_owner);
    platform_cell_released(steps, platform_place_g_program_owner);
    platform_cell_released(probes, platform_place_g_program_owner);
    platform_cell_released(links_present, platform_place_g_program_owner);
    platform_cell_released(division_voxels, platform_place_g_program_owner);
    platform_cell_released(key_cell_voxels, platform_place_g_program_owner);
    return steps_succeeded;
}

ScoreSlideResults slide;
memset(&slide, 0, sizeof(slide));
ScoreSlideLevels slide_levels[2];
memset(slide_levels, 0, sizeof(slide_levels));
ScoreOverlapResults overlap;
memset(&overlap, 0, sizeof(overlap));
ok = ok && ((rules->overlap == 0) || (max_tree_retain_frames() != 0));
unsigned int *const key_successors = (rules->overlap != 0) ? score_key_successors(&key) : NULL;
ok = ok && ((rules->overlap == 0) || (key_successors != NULL));
const ScoreKeyDivisions key_divisions = {&key, key_successors, buffers.height, buffers.width};

platform_cell_released(slide_levels[1].levels, platform_place_g_program_owner);
slide_levels[1].levels = NULL;
slide_levels[1].count = 0u;
ok = (ok != 0) &&
     ((rules->slide == 0) || (score_slide_frame(&key, frames[frame].time, &buffers, &slide, &slide_levels[1]) != 0));
ok = (ok != 0) && ((rules->overlap != 2) || (score_peak_levels(&buffers, &frames[frame], &slide_levels[1]) != 0));

ok = (ok != 0) && ((rules->overlap == 0) ||
                   (score_overlap_pair(&frames[frame - 1u], &frames[frame], &slide_levels[0], &slide_levels[1],
                                       &key_divisions, rules->overlap == 1, &overlap) != 0));

platform_cell_released(slide_levels[0].levels, platform_place_g_program_owner);
slide_levels[0] = slide_levels[1];
slide_levels[1].levels = NULL;
slide_levels[1].count = 0u;

if ((ok != 0) && (rules->slide != 0))
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  ");
    mmgr_line_put(output_line, sample);
    mmgr_line_put(output_line, ": the slide, ");
    mmgr_line_decimal(output_line, slide.frames, 1u);
    mmgr_line_put(output_line, " frames with key cells, ");
    mmgr_line_decimal(output_line, slide.keys, 1u);
    mmgr_line_put(output_line, " key cells: the threshold captures ");
    mmgr_line_decimal(output_line, slide.present_at_threshold, 1u);
    mmgr_line_put(output_line, ", ");
    mmgr_line_decimal(output_line, slide.alone_at_threshold, 1u);
    mmgr_line_put(output_line, " of them alone; alone at some level ");
    mmgr_line_decimal(output_line, slide.separable, 1u);
    mmgr_line_put(output_line, "; at each frame's best single level ");
    mmgr_line_decimal(output_line, slide.best_alone, 1u);
    mmgr_line_put(output_line, "; ");
    mmgr_line_decimal(output_line, slide.labelings, 1u);
    mmgr_line_put(output_line, " labelings, each proved against the tree's count");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}
if ((ok != 0) && (rules->overlap != 0))
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, sample);
        mmgr_line_put(output_line, ": the overlap at the same residual, ");
        mmgr_line_decimal(output_line, overlap.pairs, 1u);
        mmgr_line_put(output_line, " frame pairs, ");
        mmgr_line_decimal(output_line, overlap.evaluations, 1u);
        mmgr_line_put(output_line, " thresholds: at the earlier frame's threshold ");
        mmgr_line_decimal(output_line, overlap.components_at_threshold, 1u);
        mmgr_line_put(output_line, " components, ");
        mmgr_line_decimal(output_line, overlap.one_at_threshold, 1u);
        mmgr_line_put(output_line, " of them overlap exactly one, ");
        mmgr_line_decimal(output_line, overlap.mutual_at_threshold, 1u);
        mmgr_line_put(output_line, " mutual; the best mutual over each pair's thresholds ");
        mmgr_line_decimal(output_line, overlap.best_mutual, 1u);
        mmgr_line_put(output_line, "; ");
        mmgr_line_decimal(output_line, overlap.mutual_differ, 1u);
        mmgr_line_put(output_line, " thresholds where mutual counted from the later side differs");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, sample);
        mmgr_line_put(output_line, ": the event census at the earlier frame's threshold: ");
        mmgr_line_decimal(output_line, overlap.mutual_at_threshold, 1u);
        mmgr_line_put(output_line, " moves (1->1), ");
        mmgr_line_decimal(output_line, overlap.divisions_at_threshold, 1u);
        mmgr_line_put(output_line, " divisions (1->2), ");
        mmgr_line_decimal(output_line, overlap.merges_at_threshold, 1u);
        mmgr_line_put(output_line, " merges (2->1), ");
        mmgr_line_decimal(output_line, overlap.disappear_at_threshold, 1u);
        mmgr_line_put(output_line, " disappear (1->0), ");
        mmgr_line_decimal(output_line, overlap.appear_at_threshold, 1u);
        mmgr_line_put(output_line, " appear (0->1); the key contains ");
        mmgr_line_decimal(output_line, overlap.key_divisions, 1u);
        mmgr_line_put(output_line, " divisions over these pairs: at the threshold ");
        mmgr_line_decimal(output_line, overlap.parted_at_threshold, 1u);
        mmgr_line_put(output_line,
                      " parted (the parent's component overlaps the two daughters' distinct components), ");
        mmgr_line_decimal(output_line, overlap.caught_at_threshold, 1u);
        mmgr_line_put(output_line, " caught (and it is 1->2); at each pair's best sampled threshold ");
        mmgr_line_decimal(output_line, overlap.best_present, 1u);
        mmgr_line_put(output_line, " present (parent and both daughters above the threshold), ");
        mmgr_line_decimal(output_line, overlap.best_apart, 1u);
        mmgr_line_put(output_line, " apart (the daughters in two components), ");
        mmgr_line_decimal(output_line, overlap.best_parted, 1u);
        mmgr_line_put(output_line, " parted, ");
        mmgr_line_decimal(output_line, overlap.best_caught, 1u);
        mmgr_line_put(output_line, " caught");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, sample);
        mmgr_line_put(output_line,
                      ": at each pair's max-mutual threshold (the first sampled threshold with the max mutual): ");
        mmgr_line_decimal(output_line, overlap.chosen_components, 1u);
        mmgr_line_put(output_line, " earlier and ");
        mmgr_line_decimal(output_line, overlap.chosen_later_components, 1u);
        mmgr_line_put(output_line, " later components, ");
        mmgr_line_decimal(output_line, overlap.chosen_mutual, 1u);
        mmgr_line_put(output_line, " mutual; ");
        mmgr_line_decimal(output_line, overlap.chosen_cells_present, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, overlap.key_cells, 1u);
        mmgr_line_put(output_line, " key cells present, ");
        mmgr_line_decimal(output_line, overlap.chosen_cells_alone, 1u);
        mmgr_line_put(output_line, " alone (at max-components threshold: ");
        mmgr_line_decimal(output_line, overlap.threshold_cells_present, 1u);
        mmgr_line_put(output_line, " present, ");
        mmgr_line_decimal(output_line, overlap.threshold_cells_alone, 1u);
        mmgr_line_put(output_line, " alone); ");
        mmgr_line_decimal(output_line, overlap.chosen_parted, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, overlap.key_divisions, 1u);
        mmgr_line_put(output_line, " key divisions parted, ");
        mmgr_line_decimal(output_line, overlap.chosen_caught, 1u);
        mmgr_line_put(output_line, " caught");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}
platform_cell_released(slide_levels[0].levels, platform_place_g_program_owner);
platform_cell_released(slide_levels[1].levels, platform_place_g_program_owner);
platform_cell_released(key_successors, platform_place_g_program_owner);

int slide;
int overlap;

else if (strcmp(flag, "--slide") == 0)
{
    rules.slide = 1;
}
else if (strcmp(flag, "--overlap") == 0)
{
    rules.slide = 1;
    rules.overlap = 1;
}
else if (strcmp(flag, "--overlap-peaks") == 0)
{
    rules.overlap = 2;
}
