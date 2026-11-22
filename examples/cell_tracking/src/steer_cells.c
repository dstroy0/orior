// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../src/cu/engine/nbody/orior/orior.h"
#include "../../../src/platform/platform.h"

typedef struct
{
    unsigned int level;
    uint64_t occurrences;
    uint64_t magnitude;
} SteerLevel;

static int steer_order(const void *left, const void *right)
{
    const SteerLevel *a = (const SteerLevel *)left;
    const SteerLevel *b = (const SteerLevel *)right;
    if ((a->occurrences == 0u) != (b->occurrences == 0u))
    {
        return (a->occurrences == 0u) ? 1 : -1;
    }
    if (a->occurrences < b->occurrences)
    {
        return -1;
    }
    return (a->occurrences > b->occurrences) ? 1 : 0;
}

int program_main(int argc, char **argv)
{
    if (argc < 3)
    {
        (void)fprintf(stderr, "  usage: steer_cells VOLUME_TXT SHARE\n"
                              "         VOLUME_TXT  rows of \"z y x level\", the whole field\n"
                              "         SHARE       parts per thousand of the field to admit as cells\n");
        return 2;
    }

    FILE *handle = fopen(argv[1], "r");
    if (handle == NULL)
    {
        (void)fprintf(stderr, "  could not read %s\n", argv[1]);
        return 1;
    }

    size_t capacity = 1u << 16;
    size_t count = 0u;
    unsigned int *levels = platform_cell_taken(capacity * sizeof(*levels), platform_place_g_program_owner, 0);
    unsigned int *zs = platform_cell_taken(capacity * sizeof(*zs), platform_place_g_program_owner, 0);
    unsigned int *ys = platform_cell_taken(capacity * sizeof(*ys), platform_place_g_program_owner, 0);
    unsigned int *xs = platform_cell_taken(capacity * sizeof(*xs), platform_place_g_program_owner, 0);
    if ((levels == NULL) || (zs == NULL) || (ys == NULL) || (xs == NULL))
    {
        return 1;
    }

    unsigned int z = 0u;
    unsigned int y = 0u;
    unsigned int x = 0u;
    unsigned int value = 0u;
    while (fscanf(handle, "%u %u %u %u", &z, &y, &x, &value) == 4)
    {
        if (count == capacity)
        {
            capacity *= 2u;
            levels = platform_cell_grown(levels, (capacity / 2u) * sizeof(*levels), capacity * sizeof(*levels),
                                         platform_place_g_program_owner, 0);
            zs = platform_cell_grown(zs, (capacity / 2u) * sizeof(*zs), capacity * sizeof(*zs),
                                     platform_place_g_program_owner, 0);
            ys = platform_cell_grown(ys, (capacity / 2u) * sizeof(*ys), capacity * sizeof(*ys),
                                     platform_place_g_program_owner, 0);
            xs = platform_cell_grown(xs, (capacity / 2u) * sizeof(*xs), capacity * sizeof(*xs),
                                     platform_place_g_program_owner, 0);
            if ((levels == NULL) || (zs == NULL) || (ys == NULL) || (xs == NULL))
            {
                return 1;
            }
        }
        zs[count] = z;
        ys[count] = y;
        xs[count] = x;
        levels[count] = value;
        count++;
    }
    fclose(handle);

    uint8_t *field = platform_cell_taken(count, platform_place_g_program_owner, 0);
    if (field == NULL)
    {
        return 1;
    }
    for (size_t at = 0u; at < count; at++)
    {
        field[at] = (uint8_t)(levels[at] & 0xFFu);
    }

    OriorFieldCensus census;
    orior_field_census(field, count, &census);

    SteerLevel ranked[ORIOR_STEER_SYMBOLS];
    for (unsigned int level = 0u; level < ORIOR_STEER_SYMBOLS; level++)
    {
        ranked[level].level = level;
        ranked[level].occurrences = census.occurrences[level];
        ranked[level].magnitude = orior_steer_magnitude(&census, (uint8_t)level);
    }
    qsort(ranked, ORIOR_STEER_SYMBOLS, sizeof(*ranked), steer_order);

    const unsigned long share = strtoul(argv[2], NULL, 10);
    const uint64_t budget = (census.total * (uint64_t)share) / 1000u;

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  field ");
        mmgr_line_decimal(output_line, (unsigned long long)census.total, 1u);
        mmgr_line_put(output_line, " voxels, ");
        mmgr_line_decimal(output_line, census.distinct, 1u);
        mmgr_line_put(output_line, " distinct levels, admitting ");
        mmgr_line_decimal(output_line, (unsigned long long)budget, 1u);
        mmgr_line_put(output_line, " voxels (");
        mmgr_line_decimal(output_line, share, 1u);
        mmgr_line_put(output_line, " per mille)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n  rank  level  occurrences  magnitude");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    uint64_t spent = 0u;
    unsigned char admitted[ORIOR_STEER_SYMBOLS];
    memset(admitted, 0, sizeof(admitted));
    unsigned int taken = 0u;
    for (unsigned int at = 0u; at < ORIOR_STEER_SYMBOLS; at++)
    {
        if (ranked[at].occurrences == 0u)
        {
            break;
        }
        if ((spent + ranked[at].occurrences) > budget)
        {
            break;
        }
        admitted[ranked[at].level] = 1u;
        spent += ranked[at].occurrences;
        if (taken < 8u)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                mmgr_line_decimal(&linea_piece_line, (uint64_t)(taken), 1u);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 5u);
            }
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                mmgr_line_decimal(&linea_piece_line, (uint64_t)(ranked[at].level), 1u);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 6u);
            }
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                mmgr_line_decimal(&linea_piece_line, (uint64_t)((unsigned long long)ranked[at].occurrences), 1u);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 12u);
            }
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, (unsigned long long)ranked[at].magnitude, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        taken++;
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n  ");
        mmgr_line_decimal(output_line, taken, 1u);
        mmgr_line_put(output_line, " levels admitted, ");
        mmgr_line_decimal(output_line, (unsigned long long)spent, 1u);
        mmgr_line_put(output_line, " voxels, ");
        mmgr_line_fixed(output_line, (double)(census.total ? (1000.0 * (double)spent / (double)census.total) : 0.0), 3u,
                        0u, EMBED_FALSE);
        mmgr_line_put(output_line, " per mille of the field");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    FILE *out = fopen("steered.txt", "w");
    if (out != NULL)
    {
        for (size_t at = 0u; at < count; at++)
        {
            if (admitted[field[at]])
            {
                (void)fprintf(out, "%u %u %u %u\n", zs[at], ys[at], xs[at], levels[at]);
            }
        }
        fclose(out);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  wrote steered.txt");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    return 0;
}
