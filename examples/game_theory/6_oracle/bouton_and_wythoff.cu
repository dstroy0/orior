// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: GAM-6-005
/**
 * @file bouton_and_wythoff.cu
 * @brief Two games where somebody else settled which positions lose, read off grids made without either answer, and
 *        set against the published one.
 *
 * @note Run bash examples/game_theory/build_game_theory.sh bouton_and_wythoff [heap size].
 * @note A memoryless control shows one thing alone: an instrument does not invent structure. To show that it finds a
 *       structure that is there takes an answer from outside. These two games supply one for the price of the
 *       arithmetic: the answer of Bouton is from 1901, and that of Wythoff is from 1907.
 * @note The two are the same shape of game with opposite answers. The positions of Nim that lose carry a structure
 *       that a shift can find. Those of Wythoff sit on a line of irrational slope, and nothing on such a line repeats.
 * @note The grid of Nim comes from its closed form, the exclusive or of the heaps, and no other grid here does. It is
 *       a lane a cell on the record machine. The device counts its losses on each line along the first axis. The
 *       minimum excludant plays out the grid of Wythoff, one sweep an antidiagonal. A cell loses where no cell that
 *       loses lies before it in its row, its column or its diagonal. Each move from a cell reaches an earlier
 * antidiagonal.
 * @note The host sets the positions that lose against floor(n phi) and floor(n phi^2), from the exact integer square
 * root of 5 n^2. Each irrational value that it prints is an interval of exact integers.
 */
#include "../../../includes/platform/platform.h"
#include "../game_record.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

/** @brief The bits of the scale of each irrational value. */
#define WYTHOFF_BITS 192u

/** @brief The sides of the grids of Wythoff. */
#define WYTHOFF_SIDE_COUNT 3u
static const unsigned int WYTHOFF_SIDES[WYTHOFF_SIDE_COUNT] = {200u, 400u, 800u};

/**
 * @brief The cell of a grid of Nim: 1 where the exclusive or of its heaps is 0, the position that loses.
 *
 * @param[in]  heaps   The heaps.
 * @param[out] program The program [BORROWS].
 * @param[out] output  The step of the loss [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 * @note The lane is the cell. Its heap j is the digit j of the lane in base side, digit 0 first. Member 0 is one shared
 *       record that holds the side.
 */
static int nim_make(unsigned int heaps, GameProgram *program, unsigned int *output, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int size = exact_record_read_unsigned(&built, exact_record_member_field(&built, 32u, 0u), 0u);
    unsigned int rest = exact_record_lane_below(&built, 32u);
    unsigned int total = exact_record_constant(&built, 0u);
    for (unsigned int heap = 0u; heap < heaps; heap += 1u)
    {
        total = exact_record_step(&built, ENGINE_RECORD_XOR, total, exact_record_remainder(&built, rest, size), 0u);
        rest = exact_record_quotient(&built, rest, size);
    }
    const unsigned int loss = exact_record_equal(&built, total, exact_record_constant(&built, 0u));
    *output = loss;
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {1u, 0u, 0u};
    const int ok = game_load(&built, 1u, &loss, 1u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/**
 * @brief The cell of a grid of Wythoff on an antidiagonal.
 *
 * @param[out] program The program [BORROWS].
 * @param[out] output  The step of the loss [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 * @note It is 1 where no cell that loses lies before it on its row, its column or its diagonal.
 * @note Members 0, 1 and 2 are the flags of the row, the column and the diagonal of the cell. A flag is 1 where its
 *       line holds a cell that loses on an earlier antidiagonal.
 */
static int wythoff_make_cell(GameProgram *program, unsigned int *output, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int field = exact_record_member_field(&built, 2u, 0u);
    const unsigned int one = exact_record_constant(&built, 1u);
    unsigned int free_lines = one;
    for (unsigned int member = 0u; member < 3u; member += 1u)
    {
        const unsigned int flag = exact_record_read_unsigned(&built, field, member);
        free_lines = exact_record_product(&built, free_lines, exact_record_difference(&built, one, flag));
    }
    const unsigned int loss = exact_record_narrow(&built, free_lines, 1u);
    *output = loss;
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {1u, 1u, 1u};
    const int ok = game_load(&built, 3u, &loss, 1u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/**
 * @brief The flag of a line after an antidiagonal: 1 where it was 1, or where the cell of the line on the antidiagonal
 *        loses.
 *
 * @param[out] program The program [BORROWS].
 * @param[out] output  The step of the flag [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 * @note Member 0 is the flag before. Member 1 is the cell of the line on the antidiagonal, or a record of 0 where the
 *       line meets no cell there.
 */
static int wythoff_make_flag(GameProgram *program, unsigned int *output, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    const unsigned int field = exact_record_member_field(&built, 2u, 0u);
    const unsigned int before = exact_record_read_unsigned(&built, field, 0u);
    const unsigned int cell = exact_record_read_unsigned(&built, field, 1u);
    const unsigned int flag = exact_record_narrow(&built, exact_record_max(&built, before, cell), 1u);
    *output = flag;
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {1u, 1u, 0u};
    const int ok = game_load(&built, 2u, &flag, 1u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/**
 * @brief A ratio of exact integers rounded to `places` decimals, half to even, as the format of Python rounds an exact
 *        tie.
 *
 * @param[in]  numerator   The numerator, not negative [BORROWS].
 * @param[in]  denominator The denominator, above 0 [BORROWS].
 * @param[in]  places      The decimals.
 * @param[out] scaled      The ratio times 10^places, rounded [BORROWS].
 * @return                 1 where it is clean.
 */
static int game_round_ratio(const OriorExactInteger *numerator, const OriorExactInteger *denominator,
                            unsigned int places, long long *scaled)
{
    OriorExactInteger ten_power;
    OriorExactInteger work;
    OriorExactInteger quotient;
    OriorExactInteger remainder;
    int ok = game_power(10, places, &ten_power) && game_multiply(numerator, &ten_power, &work) &&
             (orior_exact_divide(&work, denominator, &quotient, &remainder) == ORIOR_EXACT_OK);
    OriorExactInteger twice;
    OriorExactInteger two;
    game_exact(&two, 2);
    ok = ok && game_multiply(&remainder, &two, &twice);
    // the quotient is a small whole number, in the low two limbs
    long long value = (long long)(((unsigned long long)quotient.limb[1] << 32u) | quotient.limb[0]);
    if (ok)
    {
        const int order = orior_exact_compare(&twice, denominator);
        value += ((order > 0) || ((order == 0) && ((value % 2LL) == 1LL))) ? 1LL : 0LL;
    }
    *scaled = value;
    return ok;
}

/**
 * @brief The golden ratio phi = (1 + sqrt 5) / 2 at 2^-bits, between its floor and its ceiling.
 *
 * @param[in]  bits The scale.
 * @param[out] phi  The interval [BORROWS].
 * @return          1 where it is clean.
 */
static int wythoff_phi(unsigned int bits, GameInterval *phi)
{
    OriorExactInteger unit;
    OriorExactInteger five;
    OriorExactInteger root;
    OriorExactInteger two;
    OriorExactInteger one;
    game_exact(&five, 5);
    game_exact(&two, 2);
    game_exact(&one, 1);
    // sqrt(5) 2^bits lies in (root, root + 1), since 5 4^bits is not a square
    int ok = game_power(4, bits, &unit) && game_multiply(&unit, &five, &root) && game_root(&root, &root) &&
             game_power(2, bits, &unit);
    OriorExactInteger low;
    OriorExactInteger high;
    ok = ok && game_add(&unit, &root, &low) && game_divide(&low, &two, 0, &phi->low) && game_add(&low, &one, &high) &&
         game_divide(&high, &two, 1, &phi->high);
    return ok;
}

/**
 * @brief floor(n phi), from the exact square root of 5 n^2: floor((n + floor(sqrt(5 n^2))) / 2).
 *
 * @param[in] number n.
 * @return           The floor, or -1 where the exact arithmetic fails.
 * @note 5 n^2 is not a square for n above 0. Thus sqrt(5 n^2) lies strictly between its floor k and k + 1. Half of
 *       n + sqrt(5 n^2) then has the floor of half of n + k.
 */
static long long wythoff_floor(long long number)
{
    OriorExactInteger value;
    OriorExactInteger root;
    game_exact(&value, 5LL * number * number);
    if (!game_root(&value, &root))
    {
        return -1LL;
    }
    // the root of 5 n^2 is a small whole number, in the low two limbs
    const long long whole = (long long)(((unsigned long long)root.limb[1] << 32u) | root.limb[0]);
    return (number + whole) / 2LL;
}

/**
 * @brief The relative error |a / m - phi| / phi in the format %.2e of Python, from intervals of exact integers.
 *
 * @param[in]  top    a [BORROWS].
 * @param[in]  bottom m [BORROWS].
 * @param[in]  phi    phi at 2^-WYTHOFF_BITS [BORROWS].
 * @param[out] text   The text [BORROWS].
 * @param[in]  room   Bytes of the text.
 * @return            1 where both ends of the interval give the same text.
 * @note The error is |a - m phi| / (m phi). Its digits are the mantissa of three digits that the error times a power
 *       of ten rounds to.
 */
static int wythoff_error_text(const OriorExactInteger *top, const OriorExactInteger *bottom, const GameInterval *phi,
                              char *text, size_t room)
{
    OriorExactInteger unit;
    int ok = game_power(2, WYTHOFF_BITS, &unit);
    // a 2^bits against m phi 2^bits, low and high
    OriorExactInteger scaled_top;
    OriorExactInteger near_low;
    OriorExactInteger near_high;
    ok = ok && game_multiply(top, &unit, &scaled_top) && game_multiply(bottom, &phi->low, &near_low) &&
         game_multiply(bottom, &phi->high, &near_high);
    // the gap |a 2^bits - m phi 2^bits| as an interval. The estimate is below phi here, and the gap is m phi - a
    OriorExactInteger gap_low;
    OriorExactInteger gap_high;
    ok = ok && game_subtract(&near_low, &scaled_top, &gap_low) && game_subtract(&near_high, &scaled_top, &gap_high);
    char found[2][32];
    for (int end = 0; ok && (end < 2); end += 1)
    {
        // the error at this end: the gap over m phi, each at its own outer bound
        const OriorExactInteger *const gap = (end == 0) ? &gap_low : &gap_high;
        const OriorExactInteger *const under = (end == 0) ? &near_high : &near_low;
        long long mantissa = 0;
        int exponent = 0;
        for (int shift = 1; ok && (shift < 40); shift += 1)
        {
            OriorExactInteger power;
            OriorExactInteger lifted;
            ok = game_power(10, (unsigned int)shift, &power) && game_multiply(gap, &power, &lifted);
            long long scaled = 0;
            ok = ok && game_round_ratio(&lifted, under, 0u, &scaled);
            if (ok && (scaled >= 100LL))
            {
                mantissa = scaled;
                exponent = 2 - shift;
                if (mantissa >= 1000LL)
                {
                    mantissa /= 10LL;
                    exponent += 1;
                }
                break;
            }
        }
        snprintf(found[end], sizeof(found[end]), "%lld.%02llde%c%02d", mantissa / 100LL, mantissa % 100LL,
                 (exponent < 0) ? '-' : '+', (exponent < 0) ? -exponent : exponent);
    }
    snprintf(text, room, "%s", found[0]);
    return ok && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = found[0], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = found[1], .ci = EMBED_FALSE));
}

/**
 * @brief Writes an interval at 2^-WYTHOFF_BITS with its decimals, and counts it where its ends round apart.
 *
 * @param[in,out] line    The line [BORROWS].
 * @param[in]     value   The interval [BORROWS].
 * @param[in]     places  The decimals.
 * @param[in,out] unclear The count of values whose ends round apart [BORROWS].
 */
static void wythoff_put(VerbaLinea *line, const GameInterval *value, unsigned int places, unsigned long long *unclear)
{
    long long scaled = 0;
    *unclear += game_round(value, WYTHOFF_BITS, places, &scaled) ? 0ULL : 1ULL;
    game_put_decimal(line, scaled, places);
}

/**
 * @brief Plays out the grid of Wythoff of one side on the device, one antidiagonal a pair of sweeps.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in]     cell    The cell program [BORROWS].
 * @param[in]     flag    The flag program [BORROWS].
 * @param[in]     side    The side.
 * @param[out]    lost    1 at each cell that loses, row by row [BORROWS]. side^2 of them.
 * @return                1 where every sweep ran.
 * @note The flags are rows 0 to side - 1, then columns, then diagonals l - r + side - 1. Each sweep writes the cells
 *       of antidiagonal d at their own place in the order of the antidiagonals, and a zero record follows the last.
 */
static int wythoff_play(SimResults *results, const GameProgram *cell, const GameProgram *flag, unsigned int side,
                        unsigned char *lost)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const unsigned long long cells = (unsigned long long)side * side;
    const unsigned long long flags = (4ULL * side) - 1ULL;
    unsigned int *device_cells = NULL;
    unsigned int *device_flags[2] = {NULL, NULL};
    unsigned int *device_index = NULL;
    int ok = sim_status_check(results, cudaMalloc((void **)&device_cells, (cells + 1ULL) * 4ULL), "cells") &&
             sim_status_check(results, cudaMalloc((void **)&device_flags[0], flags * 4ULL), "flags") &&
             sim_status_check(results, cudaMalloc((void **)&device_flags[1], flags * 4ULL), "flags") &&
             sim_status_check(results, cudaMalloc((void **)&device_index, flags * 3ULL * 4ULL), "index") &&
             sim_status_check(results, cudaMemset(device_cells, 0, (cells + 1ULL) * 4ULL), "cells cleared") &&
             sim_status_check(results, cudaMemset(device_flags[0], 0, flags * 4ULL), "flags cleared");
    unsigned int *const index =
        (unsigned int *)platform_cell_taken((size_t)(flags * 3ULL * 4ULL), platform_place_g_program_owner, 0);
    unsigned long long *const start = (unsigned long long *)platform_cell_taken(
        (2ULL * side) * sizeof(unsigned long long), platform_place_g_program_owner, 0);
    ok = ok && (index != NULL) && (start != NULL);
    unsigned long long place = 0ULL;
    for (unsigned int diagonal = 0u; ok && (diagonal < ((2u * side) - 1u)); diagonal += 1u)
    {
        start[diagonal] = place;
        const unsigned int first = (diagonal < side) ? 0u : (diagonal - side + 1u);
        const unsigned int last = (diagonal < side) ? diagonal : (side - 1u);
        place += (unsigned long long)(last - first) + 1ULL;
    }
    // the cells are below 2^20, and every place fits a limb
    const unsigned int zero_record = (unsigned int)cells;
    int current = 0;
    for (unsigned int diagonal = 0u; ok && (diagonal < ((2u * side) - 1u)); diagonal += 1u)
    {
        const unsigned int first = (diagonal < side) ? 0u : (diagonal - side + 1u);
        const unsigned int last = (diagonal < side) ? diagonal : (side - 1u);
        const unsigned int lanes = (last - first) + 1u;
        for (unsigned int row = first; row <= last; row += 1u)
        {
            const unsigned int column = diagonal - row;
            const unsigned int lane = row - first;
            index[(3u * lane) + 0u] = row;
            index[(3u * lane) + 1u] = side + column;
            index[(3u * lane) + 2u] = (2u * side) + ((row + side) - 1u - column);
        }
        ok = sim_status_check(results, cudaMemcpy(device_index, index, lanes * 3ULL * 4ULL, cudaMemcpyHostToDevice),
                              "cell index sent");
        const CycleRecordRunRequest run_cells = {cell->record,
                                                 {device_flags[current], device_flags[current], device_flags[current]},
                                                 {flags, flags, flags},
                                                 device_index,
                                                 lanes,
                                                 device_cells + start[diagonal],
                                                 &error};
        ok = ok && (cycle_record_run(&run_cells) == (long)lanes);
        // each line meets the antidiagonal at one cell at most
        for (unsigned int entry = 0u; ok && (entry < flags); entry += 1u)
        {
            unsigned int row = side;
            if (entry < side)
            {
                row = entry;
            }
            else if (entry < (2u * side))
            {
                const unsigned int column = entry - side;
                row = (diagonal >= column) ? (diagonal - column) : side;
            }
            else
            {
                // the diagonal l - r + side - 1 meets l + r = d where l = (d + entry - 3 side + 1) / 2 is whole
                const long long twice = (long long)diagonal + (long long)(entry - (2u * side)) - (long long)side + 1LL;
                row = ((twice >= 0LL) && ((twice % 2LL) == 0LL)) ? (unsigned int)(twice / 2LL) : side;
            }
            const int inside = (row >= first) && (row <= last);
            index[(2u * entry) + 0u] = entry;
            // the place of the cell on the antidiagonal is below the cells, a limb
            index[(2u * entry) + 1u] = inside ? (unsigned int)(start[diagonal] + (row - first)) : zero_record;
        }
        ok = ok &&
             sim_status_check(results, cudaMemcpy(device_index, index, flags * 2ULL * 4ULL, cudaMemcpyHostToDevice),
                              "flag index sent");
        const CycleRecordRunRequest run_flags = {flag->record,
                                                 {device_flags[current], device_cells, NULL},
                                                 {flags, cells + 1ULL, 0ULL},
                                                 device_index,
                                                 flags,
                                                 device_flags[1 - current],
                                                 &error};
        ok = ok && (cycle_record_run(&run_flags) == (long)flags);
        current = 1 - current;
    }
    unsigned int *const read =
        (unsigned int *)platform_cell_taken((size_t)(cells * 4ULL), platform_place_g_program_owner, 0);
    ok = ok && (read != NULL) &&
         sim_status_check(results, cudaMemcpy(read, device_cells, cells * 4ULL, cudaMemcpyDeviceToHost), "cells read");
    for (unsigned int diagonal = 0u; ok && (diagonal < ((2u * side) - 1u)); diagonal += 1u)
    {
        const unsigned int first = (diagonal < side) ? 0u : (diagonal - side + 1u);
        const unsigned int last = (diagonal < side) ? diagonal : (side - 1u);
        for (unsigned int row = first; row <= last; row += 1u)
        {
            // the loss is in bits 0 and 1 of its record, 0 or 1
            lost[((unsigned long long)row * side) + (diagonal - row)] =
                (unsigned char)(read[start[diagonal] + (row - first)] & 3u);
        }
    }
    cudaFree(device_cells);
    cudaFree(device_flags[0]);
    cudaFree(device_flags[1]);
    cudaFree(device_index);
    platform_cell_released(index, platform_place_g_program_owner);
    platform_cell_released(start, platform_place_g_program_owner);
    platform_cell_released(read, platform_place_g_program_owner);
    return ok;
}

/**
 * @brief Runs both games.
 *
 * @param[in] argc The count of arguments.
 * @param[in] argv The heap size, optional [BORROWS].
 * @return         0 where every check holds, else 1.
 */
int program_main(int argc, char **argv)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    VerbaLinea *const line = &results.line;
    // the heap size is a small whole number
    const unsigned int heap = (argc > 1) ? (unsigned int)strtoul(argv[1], NULL, 10) : 64u;
    sim_check(&results, (heap >= 2u) && (heap <= 64u), "the heap size is from 2 to 64");
    if (results.failures != 0ULL)
    {
        return sim_close(&results, "bouton and wythoff");
    }
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const unsigned int largest_side = WYTHOFF_SIDES[WYTHOFF_SIDE_COUNT - 1u];
    const unsigned long long nim_cells = 32ULL * 32ULL * 32ULL * 32ULL;
    const unsigned long long declared =
        (nim_cells * 12ULL) + ((unsigned long long)largest_side * largest_side * 4ULL) + (64ULL * largest_side * 4ULL);
    int ok = sim_job_submit(&results, "bouton_and_wythoff", argc, argv, declared);
    // Nim: the losses on each line along the first axis
    mmgr_line_put(line, "Nim, losing positions per axis line (Bouton 1901)\n");
    unsigned long long unclear = 0ULL;
    const unsigned int nim_heaps[3] = {2u, 3u, 4u};
    for (unsigned int shape = 0u; ok && (shape < 3u); shape += 1u)
    {
        const unsigned int heaps = nim_heaps[shape];
        const unsigned int side = (shape == 2u) ? ((heap < 32u) ? heap : 32u) : heap;
        unsigned long long cells = 1ULL;
        for (unsigned int at = 0u; at < heaps; at += 1u)
        {
            cells *= side;
        }
        GameProgram program;
        unsigned int loss = 0u;
        ok = nim_make(heaps, &program, &loss, &error);
        unsigned int *device_shared = NULL;
        unsigned int *device_index = NULL;
        unsigned int *device_out = NULL;
        unsigned int *const sums = (unsigned int *)platform_cell_taken((size_t)((cells / side) + 1ULL) * 2ULL * 4ULL,
                                                                       platform_place_g_program_owner, 0);
        ok = ok && (sums != NULL) && sim_status_check(&results, cudaMalloc((void **)&device_shared, 4ULL), "side") &&
             sim_status_check(&results, cudaMalloc((void **)&device_index, cells * 4ULL), "index") &&
             sim_status_check(&results, cudaMalloc((void **)&device_out, cells * program.layout.out_limbs * 4ULL),
                              "grid") &&
             sim_status_check(&results, cudaMemcpy(device_shared, &side, 4ULL, cudaMemcpyHostToDevice), "side sent") &&
             sim_status_check(&results, cudaMemset(device_index, 0, cells * 4ULL), "index set");
        const CycleRecordRunRequest run = {
            program.record, {device_shared, NULL, NULL}, {1ULL, 0ULL, 0ULL}, device_index, cells, device_out, &error};
        ok = ok && (cycle_record_run(&run) == (long)cells) &&
             game_sum(&program, loss, device_out, cells, side, sums, &error);
        unsigned long long seen_low = 0ULL;
        unsigned long long seen_high = 0ULL;
        unsigned long long losses = 0ULL;
        for (unsigned long long group = 0ULL; ok && (group < (cells / side)); group += 1ULL)
        {
            const long long count = game_sum_value(sums, group);
            losses += (unsigned long long)count;
            // a line of side cells holds at most side losses, and side is at most 64
            if (count < 64LL)
            {
                seen_low |= 1ULL << (unsigned int)count;
            }
            else
            {
                seen_high |= 1ULL << (unsigned int)(count - 64LL);
            }
        }
        if (ok)
        {
            OriorExactInteger top;
            OriorExactInteger bottom;
            sim_exact_unsigned(&top, losses);
            sim_exact_unsigned(&bottom, cells);
            long long density = 0;
            ok = game_round_ratio(&top, &bottom, 5u, &density);
            char text[160];
            snprintf(text, sizeof(text), "  %u heaps of %-3u  %6llu lines  counts seen [", heaps, side, cells / side);
            mmgr_line_put(line, text);
            int first = 1;
            for (unsigned int count = 0u; count < 128u; count += 1u)
            {
                const int present = (count < 64u) ? (((seen_low >> count) & 1ULL) != 0ULL)
                                                  : (((seen_high >> (count - 64u)) & 1ULL) != 0ULL);
                if (present)
                {
                    snprintf(text, sizeof(text), "%s%u", first ? "" : ", ", count);
                    mmgr_line_put(line, text);
                    first = 0;
                }
            }
            mmgr_line_put(line, "]  loss density ");
            game_put_decimal(line, density, 5u);
            mmgr_line_ch(line, '\n');
            sim_check(&results, (seen_low == 2ULL) && (seen_high == 0ULL),
                      "every line of a grid of Nim holds exactly one loss");
        }
        cudaFree(device_shared);
        cudaFree(device_index);
        cudaFree(device_out);
        platform_cell_released(sums, platform_place_g_program_owner);
        game_release(&program);
    }
    if (ok)
    {
        // ((heap - 1) / heap)^(heap - 1), exact
        OriorExactInteger top;
        OriorExactInteger bottom;
        long long chance = 0;
        ok = game_power((long long)heap - 1LL, heap - 1u, &top) && game_power(heap, heap - 1u, &bottom) &&
             game_round_ratio(&top, &bottom, 3u, &chance);
        char text[200];
        snprintf(text, sizeof(text),
                 "  a set of the same density scattered over the same cells puts exactly one in a line\n  of %u with "
                 "probability ",
                 heap);
        mmgr_line_put(line, text);
        game_put_decimal(line, chance, 3u);
        snprintf(text, sizeof(text), ". All-ones over %u lines is not what a scatter gives\n", heap * heap);
        mmgr_line_put(line, text);
    }
    // Wythoff: the played-out grid against the closed form of 1907
    mmgr_line_put(line, "\nWythoff, losing positions (Wythoff 1907)\n");
    GameProgram cell;
    GameProgram flag;
    unsigned int cell_output = 0u;
    unsigned int flag_output = 0u;
    ok = ok && wythoff_make_cell(&cell, &cell_output, &error) && wythoff_make_flag(&flag, &flag_output, &error);
    GameInterval phi;
    ok = ok && wythoff_phi(WYTHOFF_BITS, &phi);
    unsigned char *const lost = (unsigned char *)platform_cell_taken(
        (size_t)((unsigned long long)largest_side * largest_side), platform_place_g_program_owner, 0);
    unsigned int *const firsts = (unsigned int *)platform_cell_taken((size_t)largest_side * sizeof(unsigned int),
                                                                     platform_place_g_program_owner, 0);
    ok = ok && (lost != NULL) && (firsts != NULL);
    unsigned int first_count = 0u;
    for (unsigned int shape = 0u; ok && (shape < WYTHOFF_SIDE_COUNT); shape += 1u)
    {
        const unsigned int side = WYTHOFF_SIDES[shape];
        ok = wythoff_play(&results, &cell, &flag, side, lost);
        unsigned int count = 0u;
        int agree = 1;
        unsigned int last_left = 0u;
        char gaps_text[160];
        size_t used = 0u;
        gaps_text[0] = '\0';
        first_count = 0u;
        for (unsigned int left = 0u; ok && (left < side); left += 1u)
        {
            for (unsigned int right = left + 1u; right < side; right += 1u)
            {
                if (lost[((unsigned long long)left * side) + right] == 0u)
                {
                    continue;
                }
                count += 1u;
                const long long low = wythoff_floor(count);
                agree = agree && (low == (long long)left) && ((low + (long long)count) == (long long)right);
                if (count <= 12u)
                {
                    used += (size_t)snprintf(gaps_text + used, sizeof(gaps_text) - used, "%s%u",
                                             (count > 1u) ? ", " : "", right - left);
                }
                last_left = left;
                firsts[first_count] = left;
                first_count += 1u;
            }
        }
        if (ok)
        {
            char text[200];
            snprintf(text, sizeof(text),
                     "  %3u square  %3u positions  played-out grid equals the 1907 closed form: %s\n", side, count,
                     agree ? "True" : "False");
            mmgr_line_put(line, text);
            snprintf(text, sizeof(text), "              second minus first, first twelve: [%s]\n", gaps_text);
            mmgr_line_put(line, text);
            // a_n / n at 2^-bits, an exact ratio, against phi
            OriorExactInteger top;
            OriorExactInteger bottom;
            OriorExactInteger unit;
            GameInterval estimate;
            sim_exact_unsigned(&top, last_left);
            sim_exact_unsigned(&bottom, count);
            ok = game_power(2, WYTHOFF_BITS, &unit) && game_multiply(&top, &unit, &estimate.low) &&
                 game_divide(&estimate.low, &bottom, 0, &estimate.low) && game_multiply(&top, &unit, &estimate.high) &&
                 game_divide(&estimate.high, &bottom, 1, &estimate.high);
            char error_text[32];
            unclear += (ok && wythoff_error_text(&top, &bottom, &phi, error_text, sizeof(error_text))) ? 0ULL : 1ULL;
            mmgr_line_put(line, "              a_n / n = ");
            wythoff_put(line, &estimate, 6u, &unclear);
            mmgr_line_put(line, " against phi = ");
            wythoff_put(line, &phi, 6u, &unclear);
            mmgr_line_put(line, ", relative error ");
            mmgr_line_put(line, error_text);
            mmgr_line_ch(line, '\n');
            sim_check(&results, agree, "the played-out grid of Wythoff equals the closed form of 1907");
        }
    }
    // the gaps between successive first coordinates on the widest grid, which the loop above left in `firsts`
    unsigned long long ones = 0ULL;
    unsigned long long twos = 0ULL;
    unsigned long long others = 0ULL;
    unsigned long long gap_set = 0ULL;
    for (unsigned int at = 1u; ok && (at < first_count); at += 1u)
    {
        const unsigned int gap = firsts[at] - firsts[at - 1u];
        ones += (gap == 1u) ? 1ULL : 0ULL;
        twos += (gap == 2u) ? 1ULL : 0ULL;
        others += ((gap != 1u) && (gap != 2u)) ? 1ULL : 0ULL;
        gap_set |= (gap < 64u) ? (1ULL << gap) : 0ULL;
    }
    if (ok)
    {
        OriorExactInteger top;
        OriorExactInteger bottom;
        sim_exact_unsigned(&top, twos);
        sim_exact_unsigned(&bottom, ones);
        long long ratio = 0;
        ok = game_round_ratio(&top, &bottom, 5u, &ratio);
        mmgr_line_put(line, "  gaps between successive first coordinates take [");
        int first_gap = 1;
        for (unsigned int gap = 0u; gap < 64u; gap += 1u)
        {
            if (((gap_set >> gap) & 1ULL) != 0ULL)
            {
                mmgr_line_put(line, first_gap ? "" : ", ");
                mmgr_line_decimal(line, gap, 1u);
                first_gap = 0;
            }
        }
        mmgr_line_put(line, "], in ratio ");
        game_put_decimal(line, ratio, 5u);
        mmgr_line_put(line, " against phi ");
        wythoff_put(line, &phi, 5u, &unclear);
        mmgr_line_put(line, "\n  that word is Sturmian and has no period. What the period detector does with it is\n"
                            "  examples/game_theory/6_oracle/sturmian_convergents.cu, and it is not nothing.\n");
        sim_check(&results, others == 0ULL, "the gaps between successive first coordinates are 1 and 2 alone");
    }
    sim_check(&results, ok, "both games were computed and read");
    sim_check(&results, unclear == 0ULL, "every printed value rounds alike at both ends of its interval");
    sim_flush(&results);
    game_release(&cell);
    game_release(&flag);
    platform_cell_released(lost, platform_place_g_program_owner);
    platform_cell_released(firsts, platform_place_g_program_owner);
    return sim_close(&results, "bouton and wythoff");
}
