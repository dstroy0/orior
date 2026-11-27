// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Catalog: GAM-4-002
/**
 * @file grundy_period.cu
 * @brief The period detector on the Grundy sequence of each subtraction game, against the exact period of each row.
 *
 * @note The run is on the record machine, in exact integers.
 * @note Run bash examples/game_theory/build_game_theory.sh grundy_period [largest move] [max moves].
 * @note Every claim of reach in this work is empirical, over a set that nobody listed. A list of instances becomes a
 *       statement about a domain only through a controlled series inside it, with an outside answer on each row.
 * @note Impartial games remove the cost of the rows. A subtraction game with a finite move set has a Grundy sequence
 *       that is periodic past a start, and a proof shows it. An exact comparison computes the period. A row costs no
 *       data, and the answer of no row comes from the detector under test.
 * @note A record program steps each game one position a sweep: the mex of the values that its moves reach. A second
 *       program marks each pair a shift apart that differs past the bound. The sum of the marks of a game is 0 at its
 *       exact period. The detector reads the lags 1 to w of a window w, and scores each period to w / 2 against its own
 *       multiples. A period past half the window has one multiple inside it, and it is out of range.
 * @note The mark of a lag is n P / (N^2 sqrt(D_l D_r)), from five sums over the pairs of the lag. The device takes the
 *       sums. The host takes each mark as an interval at 2^-B by an integer square root. It doubles B until each
 *       decision and each printed digit is clear.
 * @note Each shuffle keeps every value of a row and moves the place that carries it, by the twister of CPython at the
 *       seeds 0 to 11. The tallest margin of the twelve is the floor of the row.
 */
#include "../../../includes/platform/platform.h"
#include "../period_detector.h"
#include "../subtraction_record.h"
#include "../twister_record.h"
#include "memoria_operor/memoria_operor.h"

#include "../../0_experimental/exact_zeta/zeta_shuffle.h"

/** @brief The positions of each game. The longest period of the sweep still repeats many times past its start. */
#define GRUNDY_POSITIONS 2048u

/** @brief The windows that the run reads each row at, and the last lag of the widest. */
#define GRUNDY_WINDOW_COUNT 5u
static const unsigned int GRUNDY_WINDOWS[GRUNDY_WINDOW_COUNT] = {16u, 24u, 32u, 48u, 64u};
#define GRUNDY_LAGS 64u

/** @brief The shuffles of each row. */
#define GRUNDY_DRAWS 12u

/* ---------------------------------------------------------------- the sweep */

/** @brief The next combination of size k of 1 to n, least first, in place. 0 past the last. */
static int grundy_next(unsigned int *chosen, unsigned int size, unsigned int largest)
{
    for (unsigned int slot = size; slot > 0u; slot -= 1u)
    {
        if (chosen[slot - 1u] < (largest - (size - slot)))
        {
            chosen[slot - 1u] += 1u;
            for (unsigned int after = slot; after < size; after += 1u)
            {
                chosen[after] = chosen[after - 1u] + 1u;
            }
            return 1;
        }
    }
    return 0;
}

/** @brief The device state of the run. */
typedef struct
{
    unsigned int *index;  /**< The index of a sweep. */
    unsigned int *out;    /**< The records of a sweep. */
    unsigned int *values; /**< The value of each place of each series, one limb a record, a zero record last. */
} GrundyDevice;

/** @brief The pool of the program. The detector takes two host cells of 51200000 bytes for its lanes. */
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 28u;

/**
 * @brief Runs the sweep.
 *
 * @param[in] argc The count of arguments.
 * @param[in] argv The largest move and the max moves, both optional [BORROWS].
 * @return         0 where every check holds, else 1.
 */
int program_main(int argc, char **argv)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    VerbaLinea *const line = &results.line;
    // The arguments are small whole numbers.
    const unsigned int largest = (argc > 1) ? (unsigned int)strtoul(argv[1], NULL, 10) : 10u;
    const unsigned int most = (argc > 2) ? (unsigned int)strtoul(argv[2], NULL, 10) : 4u;
    sim_check(&results, (largest >= 1u) && (largest <= 32u) && (most >= 1u) && (most <= largest),
              "the largest move and the max moves name a sweep");
    if (results.failures != 0ULL)
    {
        return sim_close(&results, "grundy period");
    }
    const unsigned int width = exact_record_bits_of(most);
    // the move sets, in the order of itertools.combinations, each as a mask of its moves
    unsigned int games = 0u;
    unsigned int *masks = NULL;
    for (unsigned int size = 1u; size <= most; size += 1u)
    {
        unsigned int chosen[32];
        for (unsigned int slot = 0u; slot < size; slot += 1u)
        {
            chosen[slot] = slot + 1u;
        }
        do
        {
            unsigned int *const grown = (unsigned int *)platform_cell_grown(masks, (size_t)games * sizeof(unsigned int),
                                                                            (games + 1u) * sizeof(unsigned int),
                                                                            platform_place_g_program_owner, 0);
            if (grown == NULL)
            {
                platform_cell_released(masks, platform_place_g_program_owner);
                return sim_close(&results, "grundy period");
            }
            masks = grown;
            masks[games] = 0u;
            for (unsigned int slot = 0u; slot < size; slot += 1u)
            {
                masks[games] |= 1u << (chosen[slot] - 1u);
            }
            games += 1u;
        } while (grundy_next(chosen, size, largest));
    }
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    GameProgram lag;
    unsigned int lag_steps[DETECTOR_SUMS];
    int ok = detector_make_lag(width, &lag, lag_steps, &error);
    sim_check(&results, ok, "the lag program imprints, arranges and loads");
    const unsigned long long whole = GRUNDY_POSITIONS;
    const unsigned long long series_max = games + ((unsigned long long)GRUNDY_DRAWS * games);
    const unsigned long long lag_lanes = DETECTOR_SWEEP_LANES;
    const unsigned long long out_limbs_max = (lag.layout.out_limbs > 1u) ? lag.layout.out_limbs : 1u;
    const unsigned long long declared = subtraction_bytes(games, whole, lag_lanes) + (lag_lanes * 2ULL * 4ULL) +
                                        (lag_lanes * out_limbs_max * 4ULL) + (((series_max * whole) + 1ULL) * 4ULL) +
                                        twister_bytes(GRUNDY_DRAWS);
    ok = ok && sim_job_submit(&results, "grundy_period", argc, argv, declared);
    GrundyDevice device;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &device, .bytes = sizeof(device), .val = 0);
    ok = ok && sim_status_check(&results, cudaMalloc((void **)&device.index, lag_lanes * 2ULL * 4ULL), "index") &&
         sim_status_check(&results, cudaMalloc((void **)&device.out, lag_lanes * out_limbs_max * 4ULL), "out") &&
         sim_status_check(&results, cudaMalloc((void **)&device.values, ((series_max * whole) + 1ULL) * 4ULL),
                          "values") &&
         sim_status_check(&results, cudaMemset(device.values, 0, ((series_max * whole) + 1ULL) * 4ULL),
                          "values cleared");
    unsigned int *const host_index =
        (unsigned int *)platform_cell_taken((size_t)(lag_lanes * 2ULL * 4ULL), platform_place_g_program_owner, 0);
    unsigned int *const period =
        (unsigned int *)platform_cell_cleared(games + 1u, sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const sums = (unsigned int *)platform_cell_taken(
        (size_t)((series_max + 1ULL) * (GRUNDY_LAGS + 1ULL) * 2ULL * 4ULL), platform_place_g_program_owner, 0);
    // the Grundy sequence of every game and its exact period, on the record machine
    SubtractionGames heaps;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &heaps, .bytes = sizeof(heaps), .val = 0);
    ok = ok && (host_index != NULL) && (period != NULL) && (sums != NULL) &&
         subtraction_open(&results, &heaps, masks, games, largest, most, GRUNDY_POSITIONS, lag_lanes) &&
         subtraction_periods(&results, &heaps, period);
    unsigned int settled = 0u;
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        settled += (period[game] != 0u) ? 1u : 0u;
    }
    unsigned char present[(GRUNDY_POSITIONS / 4u) + 1u];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = present, .bytes = sizeof(present), .val = 0);
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        present[period[game]] = 1u;
    }
    if (ok)
    {
        mmgr_line_decimal(line, games, 1u);
        mmgr_line_put(line, " move sets, exact period found for ");
        mmgr_line_decimal(line, settled, 1u);
        mmgr_line_put(line, " of them\nperiods present: [");
        int first = 1;
        for (unsigned int shift = 1u; shift <= (GRUNDY_POSITIONS / 4u); shift += 1u)
        {
            if (present[shift] != 0u)
            {
                mmgr_line_put(line, first ? "" : ", ");
                mmgr_line_decimal(line, shift, 1u);
                first = 0;
            }
        }
        mmgr_line_put(line, "]\n\n");
        sim_flush(&results);
    }
    // the rows that the widest window reads, and their shuffles at the seeds 0 to 11 by the twister of CPython
    const unsigned int widest = GRUNDY_WINDOWS[GRUNDY_WINDOW_COUNT - 1u];
    unsigned int rows = 0u;
    unsigned int *const row_game =
        (unsigned int *)platform_cell_taken((games + 1u) * sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = ok && (row_game != NULL);
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        if ((period[game] != 0u) && (period[game] <= (widest / 2u)))
        {
            row_game[rows] = game;
            rows += 1u;
        }
    }
    // series 0 to games - 1 are the games themselves, then the 12 shuffles of each row, row by row. The twister on the
    // device draws each order. The order depends on the seed and the length alone, and one generator a seed serves
    // every row.
    const unsigned long long series = games + ((unsigned long long)rows * GRUNDY_DRAWS);
    unsigned int seeds[GRUNDY_DRAWS];
    for (unsigned int draw = 0u; draw < GRUNDY_DRAWS; draw += 1u)
    {
        seeds[draw] = draw;
    }
    unsigned int *const orders = (unsigned int *)platform_cell_taken(
        (size_t)(GRUNDY_DRAWS * whole * sizeof(unsigned int)), platform_place_g_program_owner, 0);
    TwisterSet twister;
    ok = ok && (orders != NULL) && twister_open(&results, &twister, seeds, GRUNDY_DRAWS) &&
         twister_shuffle(&results, &twister, GRUNDY_POSITIONS, orders);
    twister_close(&twister);
    // the twister of the host, as a second route: every order of the device equals its order
    unsigned int *const order =
        (unsigned int *)platform_cell_taken(whole * sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = ok && (order != NULL);
    unsigned long long mismatches = 0ULL;
    for (unsigned int draw = 0u; ok && (draw < GRUNDY_DRAWS); draw += 1u)
    {
        for (unsigned int place = 0u; place < GRUNDY_POSITIONS; place += 1u)
        {
            order[place] = place;
        }
        ZetaTwister host;
        twister_seed(&host, draw);
        for (unsigned int place = GRUNDY_POSITIONS - 1u; place > 0u; place -= 1u)
        {
            const unsigned int other = twister_below(&host, place + 1u);
            const unsigned int held_place = order[place];
            order[place] = order[other];
            order[other] = held_place;
        }
        for (unsigned int place = 0u; place < GRUNDY_POSITIONS; place += 1u)
        {
            mismatches += (order[place] != orders[(draw * whole) + place]) ? 1ULL : 0ULL;
        }
    }
    sim_check(&results, ok && (mismatches == 0ULL),
              "the twister of the device draws every order that the twister of the host draws");
    unsigned long long written = 0ULL;
    for (unsigned long long one = 0ULL; ok && (one < series); one += 1ULL)
    {
        // the game of a series and the seed of a shuffle are small counts
        unsigned int game = (unsigned int)one;
        unsigned int draw = GRUNDY_DRAWS;
        if (one >= games)
        {
            const unsigned long long rank = one - games;
            game = row_game[rank / GRUNDY_DRAWS];
            draw = (unsigned int)(rank % GRUNDY_DRAWS);
        }
        for (unsigned int place = 0u; place < GRUNDY_POSITIONS; place += 1u)
        {
            const unsigned int from = (draw == GRUNDY_DRAWS) ? place : orders[(draw * whole) + place];
            host_index[written] = (from * games) + game;
            written += 1ULL;
        }
        // the gather writes a batch of series at a time
        if ((written >= lag_lanes) || ((one + 1ULL) == series))
        {
            const unsigned long long first_series = (one + 1ULL) - (written / whole);
            ok = subtraction_write(&results, &heaps, host_index, written, 0, device.values + (first_series * whole));
            written = 0ULL;
        }
    }
    sim_check(&results, ok, "the gather writes every series and shuffle");
    // the lag sums of every series at lags 0 to 64
    DetectorLagSums *const lag_sums = (DetectorLagSums *)platform_cell_cleared(
        (size_t)(series * (GRUNDY_LAGS + 1ULL)), sizeof(DetectorLagSums), platform_place_g_program_owner, 0);
    ok = ok && (lag_sums != NULL);
    const DetectorSweep sweep = {&lag,        lag_steps,  device.values, series,     whole,
                                 GRUNDY_LAGS, host_index, device.index,  device.out, sums};
    ok = ok && detector_sweep(&results, &sweep, lag_sums);
    sim_check(&results, ok, "the lag program sweeps and sums every series on the device");
    // the detector at every window on every row in range, and its floor from the shuffles at the widest
    DetectorReading *const readings = (DetectorReading *)platform_cell_cleared(
        (size_t)games * GRUNDY_WINDOW_COUNT, sizeof(DetectorReading), platform_place_g_program_owner, 0);
    ok = ok && (readings != NULL);
    unsigned long long undecided = 0ULL;
    if (ok)
    {
        mmgr_line_put(line, "window   in range    exact out of range\n");
    }
    for (unsigned int window = 0u; ok && (window < GRUNDY_WINDOW_COUNT); window += 1u)
    {
        const unsigned int reach = GRUNDY_WINDOWS[window];
        unsigned int inside = 0u;
        unsigned int hit = 0u;
        for (unsigned int game = 0u; ok && (game < games); game += 1u)
        {
            if ((period[game] == 0u) || (period[game] > (reach / 2u)))
            {
                continue;
            }
            inside += 1u;
            DetectorReading *const reading = &readings[(game * GRUNDY_WINDOW_COUNT) + window];
            ok = detector_detect(&lag_sums[(unsigned long long)game * (GRUNDY_LAGS + 1ULL)], GRUNDY_POSITIONS, reach,
                                 reading);
            undecided += (ok && !reading->decided) ? 1ULL : 0ULL;
            hit += (ok && (reading->period == period[game])) ? 1u : 0u;
        }
        char text[96];
        snprintf(text, sizeof(text), "%-8u %8u %8u %8u\n", reach, inside, hit, settled - inside);
        mmgr_line_put(line, text);
    }
    GameInterval *const live =
        (GameInterval *)platform_cell_cleared(rows + 1u, sizeof(GameInterval), platform_place_g_program_owner, 0);
    GameInterval *const floors =
        (GameInterval *)platform_cell_cleared(rows + 1u, sizeof(GameInterval), platform_place_g_program_owner, 0);
    GameInterval *const ratios =
        (GameInterval *)platform_cell_cleared(rows + 1u, sizeof(GameInterval), platform_place_g_program_owner, 0);
    ok = ok && (live != NULL) && (floors != NULL) && (ratios != NULL);
    unsigned long long ratio_count = 0ULL;
    unsigned long long uncleared = 0ULL;
    unsigned long long unclear_orders = 0ULL;
    OriorExactInteger unit;
    ok = ok && game_power(2, 64u, &unit);
    for (unsigned int row = 0u; ok && (row < rows); row += 1u)
    {
        live[row] = readings[(row_game[row] * GRUNDY_WINDOW_COUNT) + (GRUNDY_WINDOW_COUNT - 1u)].margin;
        orior_exact_zero(&floors[row].low);
        orior_exact_zero(&floors[row].high);
        for (unsigned int draw = 0u; ok && (draw < GRUNDY_DRAWS); draw += 1u)
        {
            DetectorReading shuffled;
            const unsigned long long one = games + ((unsigned long long)row * GRUNDY_DRAWS) + draw;
            ok = detector_detect(&lag_sums[one * (GRUNDY_LAGS + 1ULL)], GRUNDY_POSITIONS, widest, &shuffled);
            undecided += (ok && !shuffled.decided) ? 1ULL : 0ULL;
            if (ok && (shuffled.period != 0u))
            {
                if (orior_exact_compare(&shuffled.margin.low, &floors[row].low) > 0)
                {
                    floors[row].low = shuffled.margin.low;
                }
                if (orior_exact_compare(&shuffled.margin.high, &floors[row].high) > 0)
                {
                    floors[row].high = shuffled.margin.high;
                }
            }
        }
        // the floor is above 0 where its low end is, and an interval across 0 leaves the comparison unclear
        if (ok && (floors[row].low.sign > 0))
        {
            OriorExactInteger work;
            ok = game_multiply(&live[row].low, &unit, &work) &&
                 game_divide(&work, &floors[row].high, 0, &ratios[ratio_count].low) &&
                 game_multiply(&live[row].high, &unit, &work) &&
                 game_divide(&work, &floors[row].low, 1, &ratios[ratio_count].high);
            ratio_count += 1ULL;
        }
        else if (ok && (floors[row].high.sign > 0))
        {
            unclear_orders += 1ULL;
        }
        // the margin did not clear its floor where its ceiling is at most the low end of the floor. Where they overlap
        // the order is unclear
        if (ok && (orior_exact_compare(&live[row].high, &floors[row].low) <= 0))
        {
            uncleared += 1ULL;
        }
        else if (ok && (orior_exact_compare(&live[row].low, &floors[row].high) <= 0))
        {
            unclear_orders += 1ULL;
        }
    }
    unsigned long long unclear_digits = 0ULL;
    if (ok && (rows > 0u))
    {
        char text[96];
        snprintf(text, sizeof(text), "\nat window %u, %u rows\n", widest, rows);
        mmgr_line_put(line, text);
        GameInterval *const sets[3] = {live, floors, ratios};
        const unsigned long long counts[3] = {rows, rows, ratio_count};
        static const char *const LABELS[3] = {"  live margin   min ", "  shuffle floor min ", "  ratio         min "};
        static const char *const PARTS[3] = {"", "  median ", "  max "};
        for (int set = 0; ok && (set < 3); set += 1)
        {
            GameInterval found[3];
            ok = (counts[set] > 0ULL) && detector_spread(sets[set], counts[set], found);
            const unsigned int decimals = (set == 2) ? 2u : 3u;
            mmgr_line_put(line, LABELS[set]);
            for (int part = 0; ok && (part < ((set == 2) ? 2 : 3)); part += 1)
            {
                long long scaled = 0;
                unclear_digits += game_round(&found[part], 64u, decimals, &scaled) ? 0ULL : 1ULL;
                mmgr_line_put(line, PARTS[part]);
                game_put_decimal(line, scaled, decimals);
            }
            mmgr_line_ch(line, '\n');
        }
        snprintf(text, sizeof(text), "  rows where the live margin did not clear its own floor: %llu\n", uncleared);
        mmgr_line_put(line, text);
    }
    // window invariance: a true period is one number at every window
    unsigned int short_rows = 0u;
    unsigned int stable = 0u;
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        if ((period[game] == 0u) || (period[game] > (GRUNDY_WINDOWS[0] / 2u)))
        {
            continue;
        }
        short_rows += 1u;
        int same = 1;
        for (unsigned int window = 0u; window < GRUNDY_WINDOW_COUNT; window += 1u)
        {
            same = same && (readings[(game * GRUNDY_WINDOW_COUNT) + window].period == period[game]);
        }
        stable += same ? 1u : 0u;
    }
    if (ok)
    {
        char text[192];
        snprintf(text, sizeof(text),
                 "\nrows with a period readable at every window: %u, of which the answer was the same number at all %u "
                 "windows: %u\n",
                 short_rows, GRUNDY_WINDOW_COUNT, stable);
        mmgr_line_put(line, text);
    }
    sim_check(&results, ok, "every row was computed and read");
    sim_check(&results, settled == games, "every move set has an exact period inside its sequence");
    sim_check(&results, undecided == 0ULL, "every reading is decided at the precision that it asked for");
    sim_check(&results, unclear_orders == 0ULL, "every margin and floor are in a clear order");
    sim_check(&results, unclear_digits == 0ULL, "every printed value rounds alike at both ends of its interval");
    sim_check(&results, stable == short_rows, "a true period is one number at every window");
    sim_flush(&results);
    cudaFree(device.index);
    cudaFree(device.out);
    cudaFree(device.values);
    subtraction_close(&heaps);
    game_release(&lag);
    cycle_record_sum_release();
    platform_cell_released(masks, platform_place_g_program_owner);
    platform_cell_released(host_index, platform_place_g_program_owner);
    platform_cell_released(period, platform_place_g_program_owner);
    platform_cell_released(sums, platform_place_g_program_owner);
    platform_cell_released(row_game, platform_place_g_program_owner);
    platform_cell_released(order, platform_place_g_program_owner);
    platform_cell_released(orders, platform_place_g_program_owner);
    platform_cell_released(lag_sums, platform_place_g_program_owner);
    platform_cell_released(readings, platform_place_g_program_owner);
    platform_cell_released(live, platform_place_g_program_owner);
    platform_cell_released(floors, platform_place_g_program_owner);
    platform_cell_released(ratios, platform_place_g_program_owner);
    return sim_close(&results, "grundy period");
}
