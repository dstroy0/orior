// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// noise_detector_lines_summary.cu: line and plane noise summarized
#include "noise_detector_internal.h"
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"

static const char *const NOISE_LINE_NAMES[NOISE_LINE_TYPES] = {"row", "column", "plane"};

void noise_exact_wide(OriorExactInteger *value, const NoiseWide *wide)
{
    orior_exact_zero(value);
    value->limb[0] = (uint32_t)(wide->low & 0xFFFFFFFFULL);
    value->limb[1] = (uint32_t)(wide->low >> 32u);
    value->limb[2] = (uint32_t)(wide->high & 0xFFFFFFFFULL);
    value->limb[3] = (uint32_t)(wide->high >> 32u);
    value->sign = ((wide->low | wide->high) != 0ULL) ? 1 : 0;
}

// the lines' summed squares against their members' squares, in thousandths rounded down; 0 where nothing was summed
static int noise_line_per_mille(const NoiseWide *line_squares, unsigned long long member_squares,
                                unsigned long long *per_mille)
{
    if (member_squares == 0ULL)
    {
        return 0;
    }
    OriorExactInteger numerator;
    OriorExactInteger denominator;
    OriorExactInteger factor;
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    noise_exact_wide(&numerator, line_squares);
    noise_exact_word(&factor, 1000ULL);
    noise_exact_word(&denominator, member_squares);
    int ok = (orior_exact_multiply(&numerator, &factor, &numerator) == ORIOR_EXACT_OK) &&
             (orior_exact_divide(&numerator, &denominator, &quotient, &remainder) == ORIOR_EXACT_OK);
    for (unsigned int limb = 2u; ok && (limb < ORIOR_EXACT_LIMBS); limb += 1u)
    {
        ok = quotient.limb[limb] == 0u;
    }
    *per_mille = ok ? (((unsigned long long)quotient.limb[1] << 32u) | quotient.limb[0]) : 0ULL;
    return ok;
}

static void noise_lines_summary(const char *name, const NoiseLineCell cells[NOISE_LINE_CELLS])
{
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, name, 24u);
        platform_line_out_ended();
    }
    unsigned long long counted[NOISE_LINE_TYPES];
    for (unsigned int type = 0u; type < NOISE_LINE_TYPES; type += 1u)
    {
        NoiseLineCell pooled;
        mmgr_bytes_set(&pooled, 0, sizeof(pooled));
        for (unsigned int level_bin = NOISE_SUMMARY_FIRST_BIN; level_bin <= NOISE_SUMMARY_LAST_BIN; level_bin += 1u)
        {
            const NoiseLineCell *const cell = &cells[(type * NOISE_LEVEL_BINS) + level_bin];
            pooled.lines += cell->lines;
            pooled.member_squares += cell->member_squares;
            noise_wide_add(&pooled.line_squares, cell->line_squares.low, cell->line_squares.high);
        }
        counted[type] = pooled.lines;
        unsigned long long per_mille = 0ULL;
        if (noise_line_per_mille(&pooled.line_squares, pooled.member_squares, &per_mille))
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, per_mille, 9u);
            platform_line_out_ended();
        }
        else
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_put_right(output_line, "-", 9u);
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "   lines: ");
        mmgr_line_decimal(output_line, counted[NOISE_LINE_ROWS], 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, counted[NOISE_LINE_COLUMNS], 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, counted[NOISE_LINE_PLANES], 1u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_line_flushed();
}

// scale times numerator over denominator, signed and rounded toward zero; 0 where the denominator is 0 or the
// quotient's magnitude reaches 2^63
static int noise_exact_ratio(const OriorExactInteger *numerator, const OriorExactInteger *denominator,
                             unsigned long long scale, long long *ratio)
{
    if (denominator->sign == 0)
    {
        return 0;
    }
    OriorExactInteger scaled;
    OriorExactInteger factor;
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    noise_exact_word(&factor, scale);
    int ok = (orior_exact_multiply(numerator, &factor, &scaled) == ORIOR_EXACT_OK) &&
             (orior_exact_divide(&scaled, denominator, &quotient, &remainder) == ORIOR_EXACT_OK);
    for (unsigned int limb = 2u; ok && (limb < ORIOR_EXACT_LIMBS); limb += 1u)
    {
        ok = quotient.limb[limb] == 0u;
    }
    const unsigned long long magnitude = ok ? (((unsigned long long)quotient.limb[1] << 32u) | quotient.limb[0]) : 0ULL;
    ok = ok && (magnitude <= 0x7FFFFFFFFFFFFFFFULL);
    // the magnitude is below 2^63. It converts to long long exactly; the sign is stored apart
    *ratio = ok ? ((long long)magnitude * (long long)quotient.sign) : 0LL;
    return ok;
}

void noise_planes_readings(const NoisePlanePool *pool, long long readings[NOISE_TWO_WAY_READINGS],
                           int valid[NOISE_TWO_WAY_READINGS])
{
    const unsigned long long height = pool->height;
    const unsigned long long width = pool->width;
    OriorExactInteger residual;
    OriorExactInteger factor;
    OriorExactInteger row_part;
    OriorExactInteger column_part;
    OriorExactInteger plane_part;
    OriorExactInteger counted;
    noise_exact_word(&factor, width - 1ULL);
    int ok = (orior_exact_subtract(&pool->spread, &pool->rows, &residual) == ORIOR_EXACT_OK) &&
             (orior_exact_subtract(&residual, &pool->columns, &residual) == ORIOR_EXACT_OK) &&
             (orior_exact_multiply(&pool->rows, &factor, &row_part) == ORIOR_EXACT_OK);
    noise_exact_word(&factor, height - 1ULL);
    ok = ok && (orior_exact_multiply(&pool->columns, &factor, &column_part) == ORIOR_EXACT_OK);
    noise_exact_word(&factor, (height - 1ULL) * (width - 1ULL));
    ok = ok && (orior_exact_multiply(&pool->plane_squares, &factor, &plane_part) == ORIOR_EXACT_OK) &&
         (orior_exact_subtract(&plane_part, &row_part, &plane_part) == ORIOR_EXACT_OK) &&
         (orior_exact_subtract(&plane_part, &column_part, &plane_part) == ORIOR_EXACT_OK) &&
         (orior_exact_add(&plane_part, &residual, &plane_part) == ORIOR_EXACT_OK) &&
         (orior_exact_add(&plane_part, &residual, &plane_part) == ORIOR_EXACT_OK);
    // the planes' voxels are the sample's frame pairs times its voxels, which the sample's bound bounds in a word
    noise_exact_word(&counted, height * width * pool->planes);
    ok = ok && (orior_exact_multiply(&counted, &factor, &counted) == ORIOR_EXACT_OK);
    const OriorExactInteger *const numerators[NOISE_TWO_WAY_READINGS] = {&row_part, &column_part, &plane_part,
                                                                         &residual};
    const OriorExactInteger *const denominators[NOISE_TWO_WAY_READINGS] = {&residual, &residual, &residual, &counted};
    const unsigned long long scales[NOISE_TWO_WAY_READINGS] = {1000ULL, 1000ULL, 1000ULL, 1ULL};
    for (unsigned int ratio = 0u; ratio < NOISE_TWO_WAY_READINGS; ratio += 1u)
    {
        readings[ratio] = 0LL;
        valid[ratio] = ok && noise_exact_ratio(numerators[ratio], denominators[ratio], scales[ratio], &readings[ratio]);
    }
}

static void noise_planes_summary(const NoisePlanePool *pool)
{
    long long readings[NOISE_TWO_WAY_READINGS];
    int valid[NOISE_TWO_WAY_READINGS];
    noise_planes_readings(pool, readings, valid);
    const char *const labels[NOISE_TWO_WAY_READINGS] = {"within each plane: rows", "columns", "plane", "independent"};
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "", 24u);
        platform_line_out_ended();
    }
    for (unsigned int ratio = 0u; ratio < NOISE_TWO_WAY_READINGS; ratio += 1u)
    {
        if (valid[ratio] != 0)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, labels[ratio]);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_signed(output_line, readings[ratio]);
            platform_line_out_ended();
        }
        else
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, labels[ratio]);
            mmgr_line_put(output_line, " -");
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_line_flushed();
}

// The static layout's three shared variances against the independent part s, per mille and signed (each 0 where
// nothing is shared), then s in thousandths of a lane unit squared. With each type's excess A its squared sums less T,
// D_plane = K_plane - K_row - K_column: var_plane = (A_plane - A_row - A_column) / D_plane, var_row = A_row / K_row -
// var_plane, var_column = A_column / K_column - var_plane, and s = T / M - var_row - var_column - var_plane, each
// carried over the common denominator L = M K_row K_column D_plane so nothing is rounded before the last division.
void noise_static_readings(const NoiseStaticPool *still, long long readings[NOISE_STATIC_READINGS],
                           int valid[NOISE_STATIC_READINGS])
{
    OriorExactInteger squares;
    OriorExactInteger members;
    OriorExactInteger row_pairs;
    OriorExactInteger column_pairs;
    OriorExactInteger plane_pairs;
    OriorExactInteger row_excess;
    OriorExactInteger column_excess;
    OriorExactInteger plane_excess;
    noise_exact_word(&squares, still->squares);
    noise_exact_word(&members, still->members);
    noise_exact_word(&row_pairs, still->row_pairs);
    noise_exact_word(&column_pairs, still->column_pairs);
    noise_exact_word(&plane_pairs, still->plane_pairs);
    noise_exact_wide(&row_excess, &still->row_squares);
    noise_exact_wide(&column_excess, &still->column_squares);
    noise_exact_wide(&plane_excess, &still->plane_squares);
    // plane_pairs becomes D_plane, and plane_excess the plane's excess less the rows' and the columns'
    int ok = (orior_exact_subtract(&row_excess, &squares, &row_excess) == ORIOR_EXACT_OK) &&
             (orior_exact_subtract(&column_excess, &squares, &column_excess) == ORIOR_EXACT_OK) &&
             (orior_exact_subtract(&plane_excess, &squares, &plane_excess) == ORIOR_EXACT_OK) &&
             (orior_exact_subtract(&plane_excess, &row_excess, &plane_excess) == ORIOR_EXACT_OK) &&
             (orior_exact_subtract(&plane_excess, &column_excess, &plane_excess) == ORIOR_EXACT_OK) &&
             (orior_exact_subtract(&plane_pairs, &row_pairs, &plane_pairs) == ORIOR_EXACT_OK) &&
             (orior_exact_subtract(&plane_pairs, &column_pairs, &plane_pairs) == ORIOR_EXACT_OK);
    // var_plane L, then A_row M K_column D_plane and A_column M K_row D_plane, then s L and L
    OriorExactInteger plane_part;
    OriorExactInteger row_part;
    OriorExactInteger column_part;
    OriorExactInteger independent;
    OriorExactInteger common;
    ok = ok && (orior_exact_multiply(&plane_excess, &members, &plane_part) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&plane_part, &row_pairs, &plane_part) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&plane_part, &column_pairs, &plane_part) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&row_excess, &members, &row_part) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&row_part, &column_pairs, &row_part) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&row_part, &plane_pairs, &row_part) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&column_excess, &members, &column_part) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&column_part, &row_pairs, &column_part) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&column_part, &plane_pairs, &column_part) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&row_pairs, &column_pairs, &common) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&common, &plane_pairs, &common) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&common, &squares, &independent) == ORIOR_EXACT_OK) &&
         (orior_exact_subtract(&independent, &row_part, &independent) == ORIOR_EXACT_OK) &&
         (orior_exact_subtract(&independent, &column_part, &independent) == ORIOR_EXACT_OK) &&
         (orior_exact_add(&independent, &plane_part, &independent) == ORIOR_EXACT_OK) &&
         (orior_exact_multiply(&common, &members, &common) == ORIOR_EXACT_OK) &&
         (orior_exact_subtract(&row_part, &plane_part, &row_part) == ORIOR_EXACT_OK) &&
         (orior_exact_subtract(&column_part, &plane_part, &column_part) == ORIOR_EXACT_OK);
    const OriorExactInteger *const numerators[NOISE_STATIC_READINGS] = {&row_part, &column_part, &plane_part,
                                                                        &independent};
    const OriorExactInteger *const denominators[NOISE_STATIC_READINGS] = {&independent, &independent, &independent,
                                                                          &common};
    for (unsigned int measurement = 0u; measurement < NOISE_STATIC_READINGS; measurement += 1u)
    {
        readings[measurement] = 0LL;
        valid[measurement] = ok && noise_exact_ratio(numerators[measurement], denominators[measurement], 1000ULL,
                                                     &readings[measurement]);
    }
}

static void noise_static_summary(const NoiseStaticPool *still)
{
    long long readings[NOISE_STATIC_READINGS];
    int valid[NOISE_STATIC_READINGS];
    noise_static_readings(still, readings, valid);
    const char *const labels[NOISE_STATIC_READINGS] = {"rows", "columns", "plane", "independent"};
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "", 24u);
        mmgr_line_put(output_line, " static, means up to ");
        mmgr_line_decimal(output_line, still->ceiling, 1u);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, still->members, 1u);
        mmgr_line_put(output_line, " voxel-frame pairs:");
        platform_line_out_ended();
    }
    for (unsigned int measurement = 0u; measurement < NOISE_STATIC_READINGS; measurement += 1u)
    {
        if (valid[measurement] != 0)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, labels[measurement]);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_signed(output_line, readings[measurement]);
            platform_line_out_ended();
        }
        else
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, " ");
            mmgr_line_put(output_line, labels[measurement]);
            mmgr_line_put(output_line, " -");
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_line_flushed();
}

static int noise_lines_rows(PlatformFile *table, const char *name, const NoiseLineCell cells[NOISE_LINE_CELLS])
{
    int ok = 1;
    for (unsigned int type = 0u; ok && (type < NOISE_LINE_TYPES); type += 1u)
    {
        for (unsigned int level_bin = 0u; ok && (level_bin < NOISE_LEVEL_BINS); level_bin += 1u)
        {
            const NoiseLineCell *const cell = &cells[(type * NOISE_LEVEL_BINS) + level_bin];
            if (cell->lines == 0ULL)
            {
                continue;
            }
            unsigned long long formed_mark_1;
            ok =
                (formed_mark_1 = platform_file_mark(table), mmgr_line_put(platform_file_line(table), name),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_put(platform_file_line(table), NOISE_LINE_NAMES[type]),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), level_bin << (NOISE_LEVEL_BIN_SHIFT - 1u), 1u),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), cell->lines, 1u),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), cell->members, 1u),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), cell->member_squares, 1u),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), cell->line_squares.high, 1u),
                 mmgr_line_put(platform_file_line(table), "\t"),
                 mmgr_line_decimal(platform_file_line(table), cell->line_squares.low, 1u),
                 mmgr_line_put(platform_file_line(table), "\n"), platform_file_written_since(table, formed_mark_1)) > 0;
        }
    }
    return ok;
}

extern "C" long noise_lines_set(const NoiseSetRequest *request)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return NOISE_DETECTOR_ERROR;
    }
    EngineError *const error = request->error;
    char path[ENGINE_PATH_CAPACITY];
    VerbaLinea formed_line_1;
    const int written = (request->set != NULL) ? (formed_line_1 = mmgr_line_over(path, sizeof(path)),
                                                  mmgr_line_put(&formed_line_1, request->set),
                                                  mmgr_line_put(&formed_line_1, "/noise_lines.tsv"),
                                                  (void)mmgr_line_finish(&formed_line_1), (int)formed_line_1.at)
                                               : -1;
    const int named = (written > 0) && ((size_t)written < sizeof(path));
    PlatformFile *const table = named ? platform_file_open(path, "wb") : NULL;
    char plane_path[ENGINE_PATH_CAPACITY];
    VerbaLinea formed_line_2;
    const int plane_written = (request->set != NULL) ? (formed_line_2 = mmgr_line_over(plane_path, sizeof(plane_path)),
                                                        mmgr_line_put(&formed_line_2, request->set),
                                                        mmgr_line_put(&formed_line_2, "/noise_planes.tsv"),
                                                        (void)mmgr_line_finish(&formed_line_2), (int)formed_line_2.at)
                                                     : -1;
    const int plane_named = (plane_written > 0) && ((size_t)plane_written < sizeof(plane_path));
    PlatformFile *const plane_table = plane_named ? platform_file_open(plane_path, "wb") : NULL;
    unsigned long long formed_mark_3;
    unsigned long long formed_mark_2;
    int ok =
        NOISE_DETECTOR_CHECK(named && plane_named && (request->samples != NULL) && (request->load != NULL), request,
                             error, ENGINE_ERROR_REQUEST) &&
        NOISE_DETECTOR_IO(table != NULL, path, error) &&
        NOISE_DETECTOR_IO(
            (formed_mark_2 = platform_file_mark(table),
             mmgr_line_put(
                 platform_file_line(table),
                 "sample\tline\tlowest_mean\tlines\tmembers\tmember_squares\tline_squares_high\tline_squares_low\n"),
             platform_file_written_since(table, formed_mark_2)) > 0,
            table, error) &&
        NOISE_DETECTOR_IO(plane_table != NULL, plane_path, error) &&
        NOISE_DETECTOR_IO(
            (formed_mark_3 = platform_file_mark(plane_table),
             mmgr_line_put(platform_file_line(plane_table),
                           "sample\tframe\tz\tmembers\tlevel\tplane_sum\tplane_squares\trow_squares\tcolumn_"
                           "squares\tstatic_members\tstatic_sum\tstatic_squares\tstatic_level\n"),
             platform_file_written_since(plane_table, formed_mark_3)) > 0,
            plane_table, error);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  lines: each frame difference summed along a row (x), a column (y) and a whole z "
                                   "plane, the sum squared, in bins of 8 by the line's mean");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  the lines' squared sums against their members' squares summed, per mille, means 40 to 199 "
                      "(independent noise gives 1000; an offset shared along the line raises it):");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  then every plane as a two-way layout, rows by columns: rows 1 + W var_row / s, columns 1 + H "
                      "var_col / s and the plane 1 + N var_plane / s, per mille (1000 where nothing is shared), and s, "
                      "the independent variance of a voxel's frame difference, in lane units squared");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  then the static voxels alone (the dimmest quarter by mean of those that change, never touch "
                      "either end of the lane, and have a mean square frame difference within 4 times their mean plus "
                      "32) as an unbalanced layout: var_row, var_col and var_plane against s, per mille (0 where "
                      "nothing is shared), and s in thousandths of a lane unit squared");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, "", 24u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "rows", 9u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "columns", 9u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put_right(output_line, "planes", 9u);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    NoiseLineCell *const cells = (NoiseLineCell *)malloc((size_t)NOISE_LINE_CELLS * sizeof(NoiseLineCell));
    ok = ok && NOISE_DETECTOR_CHECK(cells != NULL, &cells, error, ENGINE_ERROR_RESOURCE);
    // four exact integers, stored apart from the stack
    NoisePlanePool *const pool = (NoisePlanePool *)malloc(sizeof(NoisePlanePool));
    ok = ok && NOISE_DETECTOR_CHECK(pool != NULL, &pool, error, ENGINE_ERROR_RESOURCE);
    NoiseStaticPool still;
    mmgr_bytes_set(&still, 0, sizeof(still));
    const unsigned long long began = engine_clock_microseconds();
    for (unsigned int sample = 0u; ok && (sample < request->count); sample += 1u)
    {
        const char *const name = request->samples[sample];
        unsigned long long extent[4] = {0ULL, 0ULL, 0ULL, 0ULL};
        unsigned short *volume = NULL;
        ok = NOISE_DETECTOR_CHECK(request->load(request->set, name, extent, &volume, error) == 0L, name, error,
                                  ENGINE_ERROR_REQUEST) &&
             (noise_lines_sample(volume, extent, cells, pool, &still, plane_table, name, error) == 0L);
        free(volume);
        ok = ok && NOISE_DETECTOR_IO(noise_lines_rows(table, name, cells) != 0, table, error);
        if (ok != 0)
        {
            noise_lines_summary(name, cells);
            noise_planes_summary(pool);
            noise_static_summary(&still);
        }
    }
    free(cells);
    free(pool);
    if (table != NULL)
    {
        ok = NOISE_DETECTOR_IO(platform_file_close(table) == 0, path, error) && ok;
    }
    if (plane_table != NULL)
    {
        ok = NOISE_DETECTOR_IO(platform_file_close(plane_table) == 0, plane_path, error) && ok;
    }
    if (ok != 0)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  lines: ");
        mmgr_line_decimal(output_line, request->count, 1u);
        mmgr_line_put(output_line, " samples in ");
        mmgr_line_decimal(output_line, (engine_clock_microseconds() - began) / 1000ULL, 1u);
        mmgr_line_put(output_line, " ms; every sum is in ");
        mmgr_line_put(output_line, path);
        mmgr_line_put(output_line, ", and every plane's in ");
        mmgr_line_put(output_line, plane_path);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (ok != 0) ? 0L : NOISE_DETECTOR_ERROR;
}
