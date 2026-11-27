// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: GAM-2-002
/**
 * @file board_width.cu
 * @brief The width of a board, read back off its flattening by the period detector.
 *
 * @note Run bash examples/game_theory/build_game_theory.sh board_width [games].
 * @note A board flattened row by row repeats at its width. The detector reads the final positions of games of Reversi
 *       concatenated, row by row and column by column. The null is the same pieces on the same squares in a drawn
 *       arrangement. It keeps the count of each color and the occupied squares, and it moves which color sits where.
 * @note A two-heap subtraction grid carries two axes of structure. One is the width it is laid out at, and one is the
 *       Grundy period of its game along each row. The detector returns one of them, and the run counts which. The same
 * grids shuffled return the width at the chance rate.
 * @note The games of Reversi run on the record machine, a game a lane, and the twister on the device draws each move.
 *       The Grundy sequences run on the record machine too, and each cell of a grid is the exclusive or of two values.
 */
#include "../../../src/platform/platform.h"
#include "../period_detector.h"
#include "../reversi_record.h"
#include "../subtraction_record.h"
#include "memoria_operor/memoria_operor.h"

/** @brief The widths that the two-heap grids are laid out at. */
#define WIDTH_COUNT 5u
static const unsigned int WIDTHS[WIDTH_COUNT] = {12u, 16u, 20u, 24u, 32u};

/** @brief The positions of the sequence that names the own period of a game. */
#define WIDTH_SEQUENCE 512u

/** @brief The games of the null of the grids, the first of the sweep. */
#define WIDTH_SHUFFLED_GAMES 20u

/** @brief The labels of the readings of a grid, in the order of their first sight. */
#define WIDTH_LABELS 4u
static const char *const WIDTH_LABEL_TEXT[WIDTH_LABELS] = {"the width",
                                                           "the width off by one, which is diagonal content",
                                                           "the game's own Grundy period", "a combination of the two"};

/** @brief The buffers of the detector on one set of series. */
typedef struct
{
    GameProgram lag;                   /**< The lag program. */
    unsigned int steps[DETECTOR_SUMS]; /**< Its steps. */
    unsigned int *values;              /**< The values on the device [OWNS]. */
    unsigned int *index;               /**< The index of a sweep on the device [OWNS]. */
    unsigned int *out;                 /**< The records of a sweep [OWNS]. */
    unsigned int *host_index;          /**< The index on the host [OWNS]. */
    unsigned int *sums;                /**< The sums of a sweep [OWNS]. */
} WidthDetector;

/**
 * @brief Reads series of value records on the device with the detector.
 *
 * @param[in,out] results   The results [BORROWS].
 * @param[in]     detector  The buffers [BORROWS].
 * @param[in]     series    The series, each `positions` values, then one zero record [BORROWS].
 * @param[in]     count     The series.
 * @param[in]     positions The values of a series.
 * @param[in]     longest   The window.
 * @param[out]    readings  The reading of each [BORROWS].
 * @return                  1 where every step ran.
 */
static int width_read(SimResults *results, WidthDetector *detector, unsigned long long count,
                      unsigned long long positions, unsigned int longest, DetectorReading *readings)
{
    DetectorLagSums *const sums = (DetectorLagSums *)platform_cell_cleared(
        (size_t)(count * (longest + 1ULL)), sizeof(DetectorLagSums), platform_place_g_program_owner, 0);
    int ok = sums != NULL;
    const DetectorSweep sweep = {&detector->lag, detector->steps,      detector->values, count,         positions,
                                 longest,        detector->host_index, detector->index,  detector->out, detector->sums};
    ok = ok && detector_sweep(results, &sweep, sums);
    unsigned long long undecided = 0ULL;
    for (unsigned long long one = 0ULL; ok && (one < count); one += 1ULL)
    {
        ok = detector_detect(&sums[one * (longest + 1ULL)], positions, longest, &readings[one]);
        undecided += (ok && !readings[one].decided) ? 1ULL : 0ULL;
    }
    sim_check(results, ok && (undecided == 0ULL), "every reading is decided at the precision that it asked for");
    platform_cell_released(sums, platform_place_g_program_owner);
    return ok;
}

/** @brief Sends value records to the detector: one limb a value, then a zero record. */
static int width_send(SimResults *results, WidthDetector *detector, const unsigned int *values,
                      unsigned long long count)
{
    return sim_status_check(results, cudaMemcpy(detector->values, values, count * 4ULL, cudaMemcpyHostToDevice),
                            "values sent") &&
           sim_status_check(results, cudaMemset(detector->values + count, 0, 4ULL), "zero record set");
}

/** @brief The next combination of size k of 1 to n, least first, in place. 0 past the last. */
static int width_next(unsigned int *chosen, unsigned int size, unsigned int largest)
{
    for (unsigned int place = size; place > 0u; place -= 1u)
    {
        if (chosen[place - 1u] < (largest - (size - place)))
        {
            chosen[place - 1u] += 1u;
            for (unsigned int after = place; after < size; after += 1u)
            {
                chosen[after] = chosen[after - 1u] + 1u;
            }
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Runs both halves.
 *
 * @param[in] argc The count of arguments.
 * @param[in] argv The games of Reversi of each player, optional [BORROWS].
 * @return         0 where every check holds, else 1.
 */
int program_main(int argc, char **argv)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    VerbaLinea *const line = &results.line;
    // the games are a small whole number
    const unsigned int games = (argc > 1) ? (unsigned int)strtoul(argv[1], NULL, 10) : 120u;
    sim_check(&results, (games >= 1u) && (games <= 4096u), "the games are from 1 to 4096");
    if (results.failures != 0ULL)
    {
        return sim_close(&results, "board width");
    }
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const unsigned long long board_values = (unsigned long long)games * REVERSI_SQUARES;
    const unsigned long long declared = (twister_bytes(2ULL * games) * 2ULL) + (2ULL * games * 64ULL) +
                                        subtraction_bytes(64ULL, WIDTH_SEQUENCE, DETECTOR_SWEEP_LANES) +
                                        (DETECTOR_SWEEP_LANES * 6ULL * 4ULL) + ((board_values + 1ULL) * 4ULL) +
                                        (128ULL * 1024ULL * 4ULL);
    int ok = sim_job_submit(&results, "board_width", argc, argv, declared);
    WidthDetector detector;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &detector, .bytes = sizeof(detector), .val = 0);
    const unsigned long long value_room =
        ((board_values > (128ULL * 1024ULL)) ? board_values : (128ULL * 1024ULL)) + 1ULL;
    ok =
        ok && detector_make_lag(2u, &detector.lag, detector.steps, &error) &&
        sim_status_check(&results, cudaMalloc((void **)&detector.values, value_room * 4ULL), "values") &&
        sim_status_check(&results, cudaMalloc((void **)&detector.index, DETECTOR_SWEEP_LANES * 2ULL * 4ULL), "index") &&
        sim_status_check(
            &results, cudaMalloc((void **)&detector.out, DETECTOR_SWEEP_LANES * detector.lag.layout.out_limbs * 4ULL),
            "out");
    detector.host_index = (unsigned int *)platform_cell_taken((size_t)(DETECTOR_SWEEP_LANES * 2ULL * 4ULL),
                                                              platform_place_g_program_owner, 0);
    detector.sums = (unsigned int *)platform_cell_taken((size_t)(DETECTOR_SWEEP_LANES * 2ULL * 4ULL),
                                                        platform_place_g_program_owner, 0);
    unsigned int *const values =
        (unsigned int *)platform_cell_taken((size_t)(value_room * 4ULL), platform_place_g_program_owner, 0);
    ok = ok && (detector.host_index != NULL) && (detector.sums != NULL) && (values != NULL);
    // Reversi: each player plays `games` games at the seeds 0 to games - 1, the same player for both colors
    const unsigned int total = 2u * games;
    unsigned int *const players =
        (unsigned int *)platform_cell_taken(total * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const seeds =
        (unsigned int *)platform_cell_taken(total * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned char *const finals =
        (unsigned char *)platform_cell_taken((size_t)total * REVERSI_SQUARES, platform_place_g_program_owner, 0);
    ok = ok && (players != NULL) && (seeds != NULL) && (finals != NULL);
    for (unsigned int game = 0u; ok && (game < total); game += 1u)
    {
        players[game] = (game < games) ? REVERSI_RANDOM : REVERSI_CORNER;
        seeds[game] = game % games;
    }
    ok = ok && reversi_play(&results, players, seeds, total, finals);
    sim_check(&results, ok, "every game of Reversi plays to its end on the device");
    // the null of each board: its colors over its occupied squares, in the order of random.Random(seed).shuffle at the
    // seed of the game
    unsigned int *const lengths =
        (unsigned int *)platform_cell_taken(total * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const orders = (unsigned int *)platform_cell_taken(
        (size_t)total * REVERSI_SQUARES * sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = ok && (lengths != NULL) && (orders != NULL);
    for (unsigned int game = 0u; ok && (game < total); game += 1u)
    {
        lengths[game] = 0u;
        for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
        {
            lengths[game] += (finals[((unsigned long long)game * REVERSI_SQUARES) + square] != 0u) ? 1u : 0u;
        }
    }
    TwisterSet scatter;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &scatter, .bytes = sizeof(scatter), .val = 0);
    ok = ok && twister_open(&results, &scatter, seeds, total) &&
         twister_shuffle_each(&results, &scatter, lengths, REVERSI_SQUARES, orders);
    twister_close(&scatter);
    char text[200];
    snprintf(text, sizeof(text), "Reversi, %u final positions concatenated, true width %u\n", games, REVERSI_SIDE);
    mmgr_line_put(line, text);
    static const char *const PLAYER_NAMES[2] = {"random", "corner"};
    static const char *const READING_NAMES[3] = {"row major", "column major", "scattered null"};
    for (unsigned int player = 0u; ok && (player < 2u); player += 1u)
    {
        for (unsigned int reading = 0u; ok && (reading < 3u); reading += 1u)
        {
            unsigned long long at = 0ULL;
            for (unsigned int seed = 0u; seed < games; seed += 1u)
            {
                const unsigned int game = (player * games) + seed;
                const unsigned char *const board = finals + ((unsigned long long)game * REVERSI_SQUARES);
                unsigned int cells[REVERSI_SQUARES];
                unsigned int rank[REVERSI_SQUARES];
                unsigned int occupied = 0u;
                for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
                {
                    rank[square] = occupied;
                    if (board[square] != 0u)
                    {
                        cells[occupied] = square;
                        occupied += 1u;
                    }
                }
                for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
                {
                    unsigned int value = board[square];
                    if (reading == 1u)
                    {
                        value = board[((square % REVERSI_SIDE) * REVERSI_SIDE) + (square / REVERSI_SIDE)];
                    }
                    else if ((reading == 2u) && (board[square] != 0u))
                    {
                        const unsigned int *const order = orders + ((unsigned long long)game * REVERSI_SQUARES);
                        value = board[cells[order[rank[square]]]];
                    }
                    values[at] = value;
                    at += 1ULL;
                }
            }
            DetectorReading found;
            ok = width_send(&results, &detector, values, at) && width_read(&results, &detector, 1ULL, at, 24u, &found);
            char period_text[16];
            char margin_text[32];
            snprintf(period_text, sizeof(period_text), "%u", found.period);
            snprintf(margin_text, sizeof(margin_text), "none");
            if (found.period == 0u)
            {
                snprintf(period_text, sizeof(period_text), "None");
            }
            else
            {
                long long scaled = 0;
                const int clear = game_round(&found.margin, 64u, 4u, &scaled);
                sim_check(&results, clear, "every printed margin rounds alike at both ends of its interval");
                // the margin lies between -2 and 2: its digits fit the text
                snprintf(margin_text, sizeof(margin_text), "%s%lld.%04lld", (scaled < 0) ? "-" : "",
                         ((scaled < 0) ? -scaled : scaled) / 10000LL, ((scaled < 0) ? -scaled : scaled) % 10000LL);
            }
            snprintf(text, sizeof(text), "  %-7s %-16s period %-5s margin %s\n", PLAYER_NAMES[player],
                     READING_NAMES[reading], period_text, margin_text);
            mmgr_line_put(line, text);
        }
    }
    // the two-heap grids: every move set of 1 to 3 moves from 1 to 7, in the order of itertools.combinations
    mmgr_line_put(line, "\ntwo-heap subtraction grids, arranged at a known width\n");
    unsigned int masks[64];
    unsigned int swept = 0u;
    for (unsigned int size = 1u; size <= 3u; size += 1u)
    {
        unsigned int chosen[3];
        for (unsigned int place = 0u; place < size; place += 1u)
        {
            chosen[place] = place + 1u;
        }
        do
        {
            masks[swept] = 0u;
            for (unsigned int place = 0u; place < size; place += 1u)
            {
                masks[swept] |= 1u << (chosen[place] - 1u);
            }
            swept += 1u;
        } while (width_next(chosen, size, 7u));
    }
    SubtractionGames heaps;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &heaps, .bytes = sizeof(heaps), .val = 0);
    unsigned int own[64];
    ok = ok && subtraction_open(&results, &heaps, masks, swept, 7u, 3u, WIDTH_SEQUENCE, DETECTOR_SWEEP_LANES) &&
         subtraction_periods(&results, &heaps, own);
    // the null of the grids: the order that random.Random(width).shuffle gives a grid of each width
    unsigned int width_seeds[WIDTH_COUNT];
    unsigned int width_lengths[WIDTH_COUNT];
    for (unsigned int width = 0u; width < WIDTH_COUNT; width += 1u)
    {
        width_seeds[width] = WIDTHS[width];
        width_lengths[width] = WIDTHS[width] * WIDTHS[width];
    }
    const unsigned int widest_cells = WIDTHS[WIDTH_COUNT - 1u] * WIDTHS[WIDTH_COUNT - 1u];
    unsigned int *const grid_orders = (unsigned int *)platform_cell_taken(
        (size_t)WIDTH_COUNT * widest_cells * sizeof(unsigned int), platform_place_g_program_owner, 0);
    TwisterSet grids;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &grids, .bytes = sizeof(grids), .val = 0);
    ok = ok && (grid_orders != NULL) && twister_open(&results, &grids, width_seeds, WIDTH_COUNT) &&
         twister_shuffle_each(&results, &grids, width_lengths, widest_cells, grid_orders);
    twister_close(&grids);
    long long tally[WIDTH_LABELS] = {0, 0, 0, 0};
    unsigned int first_seen[WIDTH_LABELS] = {0u, 0u, 0u, 0u};
    unsigned int seen_count = 0u;
    unsigned int hits = 0u;
    unsigned int *const pairs = (unsigned int *)platform_cell_taken(
        (size_t)(swept + WIDTH_SHUFFLED_GAMES) * widest_cells * 2u * 4u, platform_place_g_program_owner, 0);
    DetectorReading *const readings = (DetectorReading *)platform_cell_cleared(
        swept + WIDTH_SHUFFLED_GAMES, sizeof(DetectorReading), platform_place_g_program_owner, 0);
    ok = ok && (pairs != NULL) && (readings != NULL);
    for (unsigned int width = 0u; ok && (width < WIDTH_COUNT); width += 1u)
    {
        const unsigned int side = WIDTHS[width];
        const unsigned int cells = side * side;
        // each grid, then the first 20 grids shuffled: a cell is the exclusive or of the values at its row and column
        unsigned long long at = 0ULL;
        for (unsigned int one = 0u; one < (swept + WIDTH_SHUFFLED_GAMES); one += 1u)
        {
            const unsigned int game = (one < swept) ? one : (one - swept);
            for (unsigned int cell = 0u; cell < cells; cell += 1u)
            {
                const unsigned int from =
                    (one < swept) ? cell : grid_orders[((unsigned long long)width * widest_cells) + cell];
                const unsigned int left = from / side;
                const unsigned int right = from % side;
                pairs[2ULL * at] = (left * swept) + game;
                pairs[(2ULL * at) + 1ULL] = (right * swept) + game;
                at += 1ULL;
            }
        }
        ok = subtraction_write(&results, &heaps, pairs, at, 1, detector.values) &&
             sim_status_check(&results, cudaMemset(detector.values + at, 0, 4ULL), "zero record set") &&
             width_read(&results, &detector, swept + WIDTH_SHUFFLED_GAMES, cells, 2u * side, readings);
        for (unsigned int game = 0u; ok && (game < swept); game += 1u)
        {
            const unsigned int period = readings[game].period;
            unsigned int label = 3u;
            if (period == side)
            {
                label = 0u;
            }
            else if ((period == (side - 1u)) || (period == (side + 1u)))
            {
                label = 1u;
            }
            else if ((own[game] != 0u) && (period != 0u) &&
                     (((period % own[game]) == 0u) || ((own[game] % period) == 0u)))
            {
                label = 2u;
            }
            if (tally[label] == 0)
            {
                first_seen[label] = seen_count;
                seen_count += 1u;
            }
            tally[label] += 1;
        }
        for (unsigned int game = 0u; ok && (game < WIDTH_SHUFFLED_GAMES); game += 1u)
        {
            hits += (readings[swept + game].period == side) ? 1u : 0u;
        }
    }
    if (ok)
    {
        // most common first, and of equal counts the first seen first, as Counter.most_common orders them
        unsigned char shown[WIDTH_LABELS] = {0u, 0u, 0u, 0u};
        const long long counted = (long long)swept * WIDTH_COUNT;
        for (unsigned int rank = 0u; rank < seen_count; rank += 1u)
        {
            unsigned int best = WIDTH_LABELS;
            for (unsigned int label = 0u; label < WIDTH_LABELS; label += 1u)
            {
                if ((shown[label] != 0u) || (tally[label] == 0))
                {
                    continue;
                }
                if ((best == WIDTH_LABELS) || (tally[label] > tally[best]) ||
                    ((tally[label] == tally[best]) && (first_seen[label] < first_seen[best])))
                {
                    best = label;
                }
            }
            shown[best] = 1u;
            snprintf(text, sizeof(text), "  %-46s %3lld of %lld\n", WIDTH_LABEL_TEXT[best], tally[best], counted);
            mmgr_line_put(line, text);
        }
        snprintf(text, sizeof(text), "  the same grids shuffled return the width on %u of %u, the chance rate\n", hits,
                 WIDTH_SHUFFLED_GAMES * WIDTH_COUNT);
        mmgr_line_put(line, text);
    }
    sim_check(&results, ok, "both halves were computed and read");
    sim_flush(&results);
    subtraction_close(&heaps);
    game_release(&detector.lag);
    cudaFree(detector.values);
    cudaFree(detector.index);
    cudaFree(detector.out);
    cycle_record_sum_release();
    platform_cell_released(detector.host_index, platform_place_g_program_owner);
    platform_cell_released(detector.sums, platform_place_g_program_owner);
    platform_cell_released(values, platform_place_g_program_owner);
    platform_cell_released(players, platform_place_g_program_owner);
    platform_cell_released(seeds, platform_place_g_program_owner);
    platform_cell_released(finals, platform_place_g_program_owner);
    platform_cell_released(lengths, platform_place_g_program_owner);
    platform_cell_released(orders, platform_place_g_program_owner);
    platform_cell_released(grid_orders, platform_place_g_program_owner);
    platform_cell_released(pairs, platform_place_g_program_owner);
    platform_cell_released(readings, platform_place_g_program_owner);
    return sim_close(&results, "board width");
}
