// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "residual_survey.h"
#include "../../../../platform/platform.h"

#include "../../engine_config.h"
#include "../golden_bands/golden_bands.h"
#include "../../runtime/radix_keys/radix_keys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int g_survey = 0u;

static unsigned long long s_survey_frames = 0ULL;

static unsigned long long s_survey_positive = 0ULL;

static unsigned long long s_survey_voxels = 0ULL;

static unsigned int s_survey_reached = 0u;

static unsigned long long s_survey_distinct_sum = 0ULL;

static unsigned long long s_survey_distinct_least = 0xFFFFFFFFFFFFFFFFULL;

static unsigned long long s_survey_distinct_max = 0ULL;

static unsigned long long s_survey_rungs[DAMP_BANDS];

void survey_residual(const unsigned int *residual, size_t voxels)
{
    unsigned int reached = 0u;
    size_t positive = 0u;
    for (size_t voxel = 0u; voxel < voxels; voxel += 1u)
    {
        const unsigned int *const limbs = &residual[voxel * ENGINE_RESIDUAL_LIMBS];
        const unsigned int negative = (unsigned int)((limbs[ENGINE_RESIDUAL_LIMBS - 1u] >> 31u) != 0u);
        positive += (size_t)(negative == 0u);
        for (unsigned int limb = 0u; (negative == 0u) && (limb < ENGINE_RESIDUAL_LIMBS); limb += 1u)
        {
            reached = ((limbs[limb] != 0u) && ((limb + 1u) > reached)) ? (limb + 1u) : reached;
        }
    }
    s_survey_frames += 1ULL;
    s_survey_voxels += (unsigned long long)voxels;
    s_survey_positive += (unsigned long long)positive;
    s_survey_reached = (reached > s_survey_reached) ? reached : s_survey_reached;
    if ((reached == 0u) || (positive == 0u))
    {
        return;
    }
    unsigned long long *const keys = (unsigned long long *)platform_cell_taken(
        (unsigned long long)(positive * sizeof(unsigned long long)), (unsigned int)platform_place_g_engine_owner, 0);
    if (keys == NULL)
    {
        return;
    }
    size_t key_count = 0u;
    for (size_t voxel = 0u; voxel < voxels; voxel += 1u)
    {
        const unsigned int *const limbs = &residual[voxel * ENGINE_RESIDUAL_LIMBS];
        if ((limbs[ENGINE_RESIDUAL_LIMBS - 1u] >> 31u) != 0u)
        {
            continue;
        }
        const unsigned int high = (reached >= 2u) ? limbs[reached - 1u] : 0u;
        const unsigned int low = limbs[(reached >= 2u) ? (reached - 2u) : (reached - 1u)];
        keys[key_count] = ((unsigned long long)high << 32u) | (unsigned long long)low;
        key_count += 1u;
    }
    radix_sort_keys(keys, key_count);
    unsigned long long distinct = (key_count != 0u) ? 1ULL : 0ULL;
    for (size_t at = 1u; at < key_count; at += 1u)
    {
        distinct += (unsigned long long)(keys[at] != keys[at - 1u]);
    }
    s_survey_distinct_sum += distinct;
    s_survey_distinct_least = (distinct < s_survey_distinct_least) ? distinct : s_survey_distinct_least;
    s_survey_distinct_max = (distinct > s_survey_distinct_max) ? distinct : s_survey_distinct_max;
    for (size_t at = 0u; at < key_count; at += 1u)
    {
        s_survey_rungs[band_of(keys[at])] += 1ULL;
    }
    platform_cell_released(keys, (unsigned int)platform_place_g_engine_owner);
}

void survey_report(void)
{
    if (s_survey_frames == 0ULL)
    {
        return;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n  RESIDUAL SURVEY over ");
        mmgr_line_decimal(output_line, s_survey_frames, 1u);
        mmgr_line_put(output_line, " frames, ");
        mmgr_line_decimal(output_line, s_survey_voxels, 1u);
        mmgr_line_put(output_line, " voxels:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const unsigned long long positive_tenths = (1000ULL * s_survey_positive) / s_survey_voxels;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    positive voxels               ");
        mmgr_line_decimal(output_line, s_survey_positive, 1u);
        mmgr_line_put(output_line, "  (");
        mmgr_line_decimal(output_line, positive_tenths / 10ULL, 1u);
        mmgr_line_put(output_line, ".");
        mmgr_line_decimal(output_line, positive_tenths % 10ULL, 1u);
        mmgr_line_ch(output_line, '%');
        mmgr_line_put(output_line, " of all)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    limbs the residual reaches    ");
        mmgr_line_decimal(output_line, s_survey_reached, 1u);
        mmgr_line_put(output_line, " of ");
        mmgr_line_decimal(output_line, ENGINE_RESIDUAL_LIMBS, 1u);
        mmgr_line_put(output_line, "  (");
        mmgr_line_decimal(output_line, s_survey_reached * 32u, 1u);
        mmgr_line_put(output_line, " bits of 288)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    distinct levels a frame contains least ");
        mmgr_line_decimal(output_line, s_survey_distinct_least, 1u);
        mmgr_line_put(output_line, ", max ");
        mmgr_line_decimal(output_line, s_survey_distinct_max, 1u);
        mmgr_line_put(output_line, ", mean ");
        mmgr_line_decimal(output_line, s_survey_distinct_sum / s_survey_frames, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned int occupied = 0u;
    for (unsigned int band = 0u; band < DAMP_BANDS; band += 1u)
    {
        occupied += (unsigned int)(s_survey_rungs[band] != 0ULL);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    golden rungs occupied         ");
        mmgr_line_decimal(output_line, occupied, 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "    voxels by rung, where a count banded flood would step:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int band = 0u; band < DAMP_BANDS; band += 1u)
    {
        if (s_survey_rungs[band] == 0ULL)
        {
            continue;
        }
        const unsigned long long hundredths = (10000ULL * s_survey_rungs[band]) / s_survey_positive;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "      rung ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                mmgr_line_decimal(&linea_piece_line, (uint64_t)(band), 1u);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 3u);
            }
            mmgr_line_put(output_line, " from ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                mmgr_line_decimal(&linea_piece_line, (uint64_t)(band_floor(band)), 1u);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 12u);
            }
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, s_survey_rungs[band], 12u);
            mmgr_line_put(output_line, " voxels  ");
            mmgr_line_decimal_columns(output_line, hundredths / 100ULL, 3u);
            mmgr_line_put(output_line, ".");
            mmgr_line_decimal(output_line, hundredths % 100ULL, 2u);
            mmgr_line_ch(output_line, '%');
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
}
