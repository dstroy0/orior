// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

#include "../../../includes/exact_types/integers/arm.h"
#include "../../../src/platform/platform.h"
#include "../../../includes/exact_types/integers/exact_integer.h"

#define SHIFT_TEXT 64u

typedef struct
{
    OriorExactInteger *positions;
    uint64_t *values;
    size_t count;
} CellFrame;

static int shift_from_unsigned(unsigned long long value, OriorExactInteger *result)
{
    char text[SHIFT_TEXT];
    const int written = snprintf(text, sizeof(text), "%llu", value);
    if ((written <= 0) || ((size_t)written >= sizeof(text)))
    {
        return 0;
    }
    return orior_exact_from_decimal(text, (size_t)written, ORIOR_EXACT_DIGITS, result) == ORIOR_EXACT_OK;
}

static int shift_read_frame(const char *path, unsigned long long side_y, unsigned long long side_x, CellFrame *frame)
{
    FILE *handle = fopen(path, "r");
    if (handle == NULL)
    {
        return 0;
    }

    size_t capacity = 1024u;
    frame->positions = platform_cell_taken(capacity * sizeof(*frame->positions), platform_place_g_program_owner, 0);
    frame->values = platform_cell_taken(capacity * sizeof(*frame->values), platform_place_g_program_owner, 0);
    frame->count = 0u;
    if ((frame->positions == NULL) || (frame->values == NULL))
    {
        fclose(handle);
        return 0;
    }

    unsigned long long z = 0u;
    unsigned long long y = 0u;
    unsigned long long x = 0u;
    unsigned long long value = 0u;
    while (fscanf(handle, "%llu %llu %llu %llu", &z, &y, &x, &value) == 4)
    {
        if (frame->count == capacity)
        {
            capacity *= 2u;
            OriorExactInteger *grown =
                platform_cell_grown(frame->positions, frame->count * sizeof(*frame->positions),
                                    capacity * sizeof(*grown), platform_place_g_program_owner, 0);
            uint64_t *grown_values =
                platform_cell_grown(frame->values, frame->count * sizeof(*frame->values),
                                    capacity * sizeof(*grown_values), platform_place_g_program_owner, 0);
            if ((grown == NULL) || (grown_values == NULL))
            {
                fclose(handle);
                return 0;
            }
            frame->positions = grown;
            frame->values = grown_values;
        }
        const unsigned long long index = (z * side_y * side_x) + (y * side_x) + x;
        if (!shift_from_unsigned(index, &frame->positions[frame->count]))
        {
            fclose(handle);
            return 0;
        }
        frame->values[frame->count] = value;
        frame->count++;
    }
    fclose(handle);
    return 1;
}

static int shift_order(const void *left, const void *right)
{
    return orior_exact_compare((const OriorExactInteger *)left, (const OriorExactInteger *)right);
}

int program_main(int argc, char **argv)
{
    if (argc < 6)
    {
        (void)fprintf(stderr, "  usage: cell_shift FRAME_A FRAME_B SIDE_Y SIDE_X MAX_STEP [engine]\n"
                              "         FRAME_A, FRAME_B  text files of \"z y x value\" rows\n"
                              "         SIDE_Y, SIDE_X    volume extents, for the linearization\n"
                              "         MAX_STEP          largest displacement to sweep, in voxels\n"
                              "         engine            portable or cuda, default whichever is present\n");
        return 2;
    }

    const unsigned long long side_y = strtoull(argv[3], NULL, 10);
    const unsigned long long side_x = strtoull(argv[4], NULL, 10);
    const long max_step = strtol(argv[5], NULL, 10);

    CellFrame first;
    CellFrame second;
    if (!shift_read_frame(argv[1], side_y, side_x, &first))
    {
        (void)fprintf(stderr, "  could not read %s\n", argv[1]);
        return 1;
    }
    if (!shift_read_frame(argv[2], side_y, side_x, &second))
    {
        (void)fprintf(stderr, "  could not read %s\n", argv[2]);
        return 1;
    }

    const size_t total = first.count + second.count;
    OriorExactInteger *positions = platform_cell_taken(total * sizeof(*positions), platform_place_g_program_owner, 0);
    uint64_t *values = platform_cell_taken(total * sizeof(*values), platform_place_g_program_owner, 0);
    if ((positions == NULL) || (values == NULL))
    {
        (void)fprintf(stderr, "  out of memory at %zu positions\n", total);
        return 1;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = positions, .src = first.positions,
               .bytes = first.count * sizeof(*positions));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = values, .src = first.values, .bytes = first.count * sizeof(*values));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = positions + first.count, .src = second.positions,
               .bytes = second.count * sizeof(*positions));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = values + first.count, .src = second.values,
               .bytes = second.count * sizeof(*values));
    qsort(positions, total, sizeof(*positions), shift_order);

    const OriorExactArm *engine = orior_exact_portable_arm();
#if defined(ORIOR_EXACT_HAVE_CUDA) && ORIOR_EXACT_HAVE_CUDA
    if ((argc < 7) || (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = argv[6], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = "portable", .ci = EMBED_FALSE)))
    {
        const OriorExactArm *device = orior_exact_cuda_arm();
        if (device != NULL)
        {
            engine = device;
        }
    }
#endif

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_decimal(output_line, total, 1u);
        mmgr_line_put(output_line, " positions from two frames, engine ");
        mmgr_line_put(output_line, engine->name);
        mmgr_line_put(output_line, ", sweeping to ");
        mmgr_line_decimal_signed(output_line, max_step);
        mmgr_line_put(output_line, " voxels\n");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "dz", 10u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_columns(output_line, "lag", 14u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, "agreement");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    size_t best = 0u;
    long best_dz = 0;
    for (long dz = 0; dz <= max_step; dz++)
    {
        OriorExactInteger lag;
        const unsigned long long step = (unsigned long long)dz * side_y * side_x;
        if (!shift_from_unsigned(step, &lag))
        {
            continue;
        }
        const size_t agreed = engine->agreement(positions, values, total, &lag);
        if (agreed == (size_t)-1)
        {
            (void)fprintf(stderr, "  the engine errored on the work at dz %ld\n", dz);
            break;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                const int64_t linea_value = (int64_t)(dz);
                mmgr_line_decimal_signed(&linea_piece_line, linea_value);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 10u);
            }
            mmgr_line_put(output_line, " ");
            {
                char linea_piece[24u];
                VerbaLinea linea_piece_line = mmgr_line_over(linea_piece, sizeof linea_piece);
                mmgr_line_decimal(&linea_piece_line, (uint64_t)(step), 1u);
                (void)mmgr_line_finish(&linea_piece_line);
                mmgr_line_put_columns(output_line, linea_piece, 14u);
            }
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, agreed, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if ((dz > 0) && (agreed > best))
        {
            best = agreed;
            best_dz = dz;
        }
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n  peak away from zero: dz ");
        mmgr_line_decimal_signed(output_line, best_dz);
        mmgr_line_put(output_line, " at ");
        mmgr_line_decimal(output_line, best, 1u);
        mmgr_line_put(output_line, " agreements.");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  Lag zero counts every position against itself and is not a displacement.");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
