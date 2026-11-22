// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file reversi_record.h
 * @brief Reversi on the record machine: a board a record, a game a lane, and each move a pair of sweeps.
 *
 * @note A square is a point, and the piece on it is its value: 0 empty, 1 black and 2 white. A board record holds the
 *       64 squares row by row, 3 bits a square, and then the color to move. Each output is one bit wider than its
 *       register. A value of 2 bits then fills its 3 bits, and the output of a move is the input of the next.
 * @note A move turns over each run of the other color that the new piece and a piece of its own bracket. That holds
 *       in each of the eight directions. The count sweep finds, for each square, the pieces that a move there turns. It
 * counts the legal moves, the fewest pieces turned and the moves that tie at it, and whether a corner is legal.
 * @note The random player takes the legal move a draw names, in row order. The corner player takes the first legal
 *       corner. Where none is legal, it takes the move a draw names among those that turn the fewest pieces. The draws
 *       are those of the twister on the device, one generator a game, as random.Random(seed) draws them.
 * @note A player with no legal move passes. The game ends after two passes in a row.
 */
#ifndef REVERSI_RECORD_H
#define REVERSI_RECORD_H

#include "twister_record.h"
#include "../../src/platform/platform.h"

/** @brief The side of the board, the squares, and the bits of a square in a record. */
#define REVERSI_SIDE 8u
#define REVERSI_SQUARES 64u
#define REVERSI_SQUARE_BITS 3u

/** @brief The place of the color to move in a record, and the limbs of a record. */
#define REVERSI_COLOR_OFFSET (REVERSI_SQUARES * REVERSI_SQUARE_BITS)
#define REVERSI_LIMBS 7u

/** @brief The players. */
enum
{
    REVERSI_RANDOM = 0, /**< Any legal move, drawn uniformly. */
    REVERSI_CORNER = 1  /**< A corner where one is legal, else the fewest pieces turned. */
};

/** @brief The registers of a board as a program reads it. */
typedef struct
{
    unsigned int cell[REVERSI_SQUARES];   /**< The piece on each square. */
    unsigned int color;                   /**< The color to move. */
    unsigned int other;                   /**< The other color. */
    unsigned int run[REVERSI_SQUARES][8]; /**< The pieces a move turns in each direction. */
    unsigned int turned[REVERSI_SQUARES]; /**< The pieces a move turns in all. */
    unsigned int legal[REVERSI_SQUARES];  /**< 1 where the move is legal. */
    unsigned int tied[REVERSI_SQUARES];   /**< 1 where it is legal and turns the fewest. */
    unsigned int moves;                   /**< The legal moves. */
    unsigned int fewest;                  /**< The fewest pieces a legal move turns. */
    unsigned int ties;                    /**< The legal moves that turn the fewest. */
    unsigned int corner;                  /**< 1 where a corner is legal. */
} ReversiBoard;

/** @brief The eight directions, row step and column step. */
static const int REVERSI_STEPS[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

/** @brief The corners, in row order. */
static const unsigned int REVERSI_CORNERS[4] = {0u, 7u, 56u, 63u};

/**
 * @brief Reads a board of member 0 and finds every legal move of the color to move.
 *
 * @param[in,out] built The program [BORROWS].
 * @param[out]    board The registers [BORROWS].
 * @note The run in a direction is the count of pieces of the other color before a piece of the color to move. No
 *       other square lies between. A move is legal on an empty square where some run is above 0.
 */
static inline void reversi_read(ExactRecordProgram *built, ReversiBoard *board)
{
    for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
    {
        const unsigned int field = exact_record_member_field(built, REVERSI_SQUARE_BITS, square * REVERSI_SQUARE_BITS);
        board->cell[square] = exact_record_read_unsigned(built, field, 0u);
    }
    board->color = exact_record_read_unsigned(
        built, exact_record_member_field(built, REVERSI_SQUARE_BITS, REVERSI_COLOR_OFFSET), 0u);
    board->other = exact_record_difference(built, exact_record_constant(built, 3u), board->color);
    const unsigned int zero = exact_record_constant(built, 0u);
    const unsigned int one = exact_record_constant(built, 1u);
    board->moves = zero;
    unsigned int corners = zero;
    for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
    {
        const int row = (int)(square / REVERSI_SIDE);
        const int column = (int)(square % REVERSI_SIDE);
        unsigned int total = zero;
        for (unsigned int direction = 0u; direction < 8u; direction += 1u)
        {
            unsigned int unbroken = one;
            unsigned int run = zero;
            for (int distance = 1; distance < (int)REVERSI_SIDE; distance += 1)
            {
                const int at_row = row + (REVERSI_STEPS[direction][0] * distance);
                const int at_column = column + (REVERSI_STEPS[direction][1] * distance);
                if ((at_row < 0) || (at_row >= (int)REVERSI_SIDE) || (at_column < 0) ||
                    (at_column >= (int)REVERSI_SIDE))
                {
                    break;
                }
                // the place on the board is a small count
                const unsigned int cell = board->cell[(unsigned int)((at_row * (int)REVERSI_SIDE) + at_column)];
                const unsigned int own = exact_record_equal(built, cell, board->color);
                // a piece of its own after distance - 1 pieces of the other color closes a run of that length
                run = exact_record_sum(
                    built, run,
                    exact_record_product(built, exact_record_product(built, unbroken, own),
                                         exact_record_constant(built, (unsigned long long)(distance - 1))));
                unbroken = exact_record_product(built, unbroken, exact_record_equal(built, cell, board->other));
            }
            board->run[square][direction] = run;
            total = exact_record_sum(built, total, run);
        }
        const unsigned int empty = exact_record_equal(built, board->cell[square], zero);
        board->turned[square] = exact_record_product(built, total, empty);
        board->legal[square] = exact_record_above(built, board->turned[square], zero);
        board->moves = exact_record_sum(built, board->moves, board->legal[square]);
    }
    for (unsigned int corner = 0u; corner < 4u; corner += 1u)
    {
        corners = exact_record_sum(built, corners, board->legal[REVERSI_CORNERS[corner]]);
    }
    board->corner = exact_record_above(built, corners, zero);
    // the fewest pieces turned: an illegal square counts as more than any move can turn
    const unsigned int beyond = exact_record_constant(built, 100u);
    board->fewest = beyond;
    for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
    {
        board->fewest = exact_record_min(
            built, board->fewest, exact_record_select(built, board->legal[square], board->turned[square], beyond));
    }
    board->ties = zero;
    for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
    {
        board->tied[square] = exact_record_product(built, board->legal[square],
                                                   exact_record_equal(built, board->turned[square], board->fewest));
        board->ties = exact_record_sum(built, board->ties, board->tied[square]);
    }
}

/**
 * @brief The count of a board: its legal moves, those that tie at the fewest pieces turned, and whether a corner is
 *        legal.
 *
 * @param[out] program The program [BORROWS].
 * @param[out] steps   The steps of the moves, the ties and the corner [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 */
static inline int reversi_make_count(GameProgram *program, unsigned int steps[3], EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    static ReversiBoard board;
    reversi_read(&built, &board);
    steps[0] = board.moves;
    steps[1] = board.ties;
    steps[2] = board.corner;
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {REVERSI_LIMBS, 0u, 0u};
    const int ok = game_load(&built, 1u, steps, 3u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/**
 * @brief The choice of a board: the square of the chosen move, or 64 where the color to move has no legal move.
 *
 * @param[out] program The program [BORROWS].
 * @param[out] output  The step of the square [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 * @note Member 1 is the choice of the lane: the draw in bits 0 to 7 and the player in bit 8. The random player takes
 *       the legal move that the draw names, in row order. The corner player takes the first legal corner. Where no
 * corner is legal, it takes the move that the draw names among those that turn the fewest.
 */
static inline int reversi_make_choose(GameProgram *program, unsigned int *output, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    static ReversiBoard board;
    reversi_read(&built, &board);
    const unsigned int draw = exact_record_read_unsigned(&built, exact_record_member_field(&built, 8u, 0u), 1u);
    const unsigned int player = exact_record_read_unsigned(&built, exact_record_member_field(&built, 1u, 8u), 1u);
    const unsigned int zero = exact_record_constant(&built, 0u);
    const unsigned int one = exact_record_constant(&built, 1u);
    const unsigned int random_player = exact_record_difference(&built, one, player);
    const unsigned int no_corner = exact_record_difference(&built, one, board.corner);
    unsigned int legal_before = zero;
    unsigned int tied_before = zero;
    unsigned int corner_before = zero;
    // the square of the chosen move, and 64 where there is none
    unsigned int square_chosen = exact_record_constant(&built, REVERSI_SQUARES);
    for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
    {
        const unsigned int nth_legal =
            exact_record_product(&built, board.legal[square], exact_record_equal(&built, legal_before, draw));
        const unsigned int nth_tied =
            exact_record_product(&built, board.tied[square], exact_record_equal(&built, tied_before, draw));
        legal_before = exact_record_sum(&built, legal_before, board.legal[square]);
        tied_before = exact_record_sum(&built, tied_before, board.tied[square]);
        unsigned int first_corner = zero;
        for (unsigned int corner = 0u; corner < 4u; corner += 1u)
        {
            if (REVERSI_CORNERS[corner] == square)
            {
                first_corner =
                    exact_record_product(&built, board.legal[square], exact_record_equal(&built, corner_before, zero));
                corner_before = exact_record_sum(&built, corner_before, board.legal[square]);
            }
        }
        const unsigned int by_corner =
            exact_record_sum(&built, exact_record_product(&built, board.corner, first_corner),
                             exact_record_product(&built, no_corner, nth_tied));
        const unsigned int chosen = exact_record_sum(&built, exact_record_product(&built, random_player, nth_legal),
                                                     exact_record_product(&built, player, by_corner));
        square_chosen = exact_record_select(&built, chosen, exact_record_constant(&built, square), square_chosen);
    }
    *output = exact_record_narrow(&built, square_chosen, 7u);
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {REVERSI_LIMBS, 1u, 0u};
    const int ok = game_load(&built, 2u, output, 1u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/**
 * @brief The move of a board, made on the chosen square, with the turn passed to the other color.
 *
 * @note The piece goes on the chosen square, and the pieces that it brackets turn.
 *
 * @param[out] program The program [BORROWS].
 * @param[out] error   The error [BORROWS].
 * @return             1 where it loaded.
 * @note Member 1 is the record of the chosen square, 64 where there is none. A board with no move stays as it is, and
 *       only the color to move changes.
 * @note A square turns where the chosen move brackets it. It lies at distance m in a direction whose run from the move
 *       is at least m. The run of each square in each direction feeds the squares that it reaches at once, and the
 *       program holds no run past that.
 */
static inline int reversi_make_flip(GameProgram *program, EngineError *error)
{
    ExactRecordProgram built;
    exact_record_open(&built);
    unsigned int cell[REVERSI_SQUARES];
    for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
    {
        const unsigned int field = exact_record_member_field(&built, REVERSI_SQUARE_BITS, square * REVERSI_SQUARE_BITS);
        cell[square] = exact_record_read_unsigned(&built, field, 0u);
    }
    const unsigned int color = exact_record_read_unsigned(
        &built, exact_record_member_field(&built, REVERSI_SQUARE_BITS, REVERSI_COLOR_OFFSET), 0u);
    const unsigned int other = exact_record_difference(&built, exact_record_constant(&built, 3u), color);
    const unsigned int square_chosen =
        exact_record_read_unsigned(&built, exact_record_member_field(&built, 8u, 0u), 1u);
    const unsigned int zero = exact_record_constant(&built, 0u);
    const unsigned int one = exact_record_constant(&built, 1u);
    unsigned int hits[REVERSI_SQUARES];
    for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
    {
        hits[square] = exact_record_equal(&built, square_chosen, exact_record_constant(&built, square));
    }
    for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
    {
        const unsigned int here = exact_record_equal(&built, square_chosen, exact_record_constant(&built, square));
        const int row = (int)(square / REVERSI_SIDE);
        const int column = (int)(square % REVERSI_SIDE);
        for (unsigned int direction = 0u; direction < 8u; direction += 1u)
        {
            unsigned int unbroken = one;
            unsigned int run = zero;
            int reach = 0;
            for (int distance = 1; distance < (int)REVERSI_SIDE; distance += 1)
            {
                const int at_row = row + (REVERSI_STEPS[direction][0] * distance);
                const int at_column = column + (REVERSI_STEPS[direction][1] * distance);
                if ((at_row < 0) || (at_row >= (int)REVERSI_SIDE) || (at_column < 0) ||
                    (at_column >= (int)REVERSI_SIDE))
                {
                    break;
                }
                // the place on the board is a small count
                const unsigned int target = cell[(unsigned int)((at_row * (int)REVERSI_SIDE) + at_column)];
                const unsigned int own = exact_record_equal(&built, target, color);
                run = exact_record_sum(
                    &built, run,
                    exact_record_product(&built, exact_record_product(&built, unbroken, own),
                                         exact_record_constant(&built, (unsigned long long)(distance - 1))));
                unbroken = exact_record_product(&built, unbroken, exact_record_equal(&built, target, other));
                reach = distance;
            }
            const unsigned int brackets = exact_record_product(&built, here, run);
            for (int distance = 1; distance <= reach; distance += 1)
            {
                // the place on the board is a small count
                const unsigned int target =
                    (unsigned int)(((row + (REVERSI_STEPS[direction][0] * distance)) * (int)REVERSI_SIDE) + column +
                                   (REVERSI_STEPS[direction][1] * distance));
                const unsigned int reaches = exact_record_above(
                    &built, brackets, exact_record_constant(&built, (unsigned long long)(distance - 1)));
                hits[target] = exact_record_sum(&built, hits[target], reaches);
            }
        }
    }
    unsigned int outputs[REVERSI_SQUARES + 1u];
    for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
    {
        const unsigned int hit = exact_record_above(&built, hits[square], zero);
        outputs[square] = exact_record_narrow(&built, exact_record_select(&built, hit, color, cell[square]), 2u);
    }
    outputs[REVERSI_SQUARES] = exact_record_narrow(&built, other, 2u);
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {REVERSI_LIMBS, 1u, 0u};
    const int ok = game_load(&built, 2u, outputs, REVERSI_SQUARES + 1u, in_limbs, program, error);
    exact_record_close(&built);
    return ok;
}

/**
 * @brief Plays games of Reversi to the end on the device, one lane a game, each game with its own player and seed.
 *
 * @param[in,out] results The results [BORROWS].
 * @param[in]     players The player of each game, the same for both colors [BORROWS].
 * @param[in]     seeds   The seed of each game [BORROWS].
 * @param[in]     games   The games.
 * @param[out]    finals  The final board of each game, 64 squares a game, row by row [BORROWS].
 * @return                1 where every sweep ran.
 * @note Black moves first from the four center pieces. A game ends after two passes in a row. The run sweeps every
 *       game until each game ends. The board of a game that ends does not change after it, since neither color has a
 *       move on it.
 */
static inline int reversi_play(SimResults *results, const unsigned int *players, const unsigned int *seeds,
                               unsigned int games, unsigned char *finals)
{
    EngineError error;
    memset(&error, 0, sizeof(error));
    GameProgram count;
    GameProgram choose;
    GameProgram flip;
    memset(&count, 0, sizeof(count));
    memset(&choose, 0, sizeof(choose));
    memset(&flip, 0, sizeof(flip));
    unsigned int count_steps[3];
    unsigned int choose_step = 0u;
    int ok = reversi_make_count(&count, count_steps, &error) && reversi_make_choose(&choose, &choose_step, &error) &&
             reversi_make_flip(&flip, &error);
    sim_check(results, ok, "the three record programs of Reversi imprint, arrange and load");
    if (!ok)
    {
        mmgr_line_put(&results->line, "  the load stopped at module ");
        mmgr_line_decimal(&results->line, (unsigned long long)error.module, 1u);
        mmgr_line_put(&results->line, ", site ");
        mmgr_line_decimal(&results->line, (unsigned long long)error.site, 1u);
        mmgr_line_ch(&results->line, '\n');
    }
    TwisterSet twister;
    memset(&twister, 0, sizeof(twister));
    ok = ok && twister_open(results, &twister, seeds, games);
    unsigned int *boards[2] = {NULL, NULL};
    unsigned int *choices = NULL;
    unsigned int *chosen = NULL;
    unsigned int *index = NULL;
    unsigned int *index_one = NULL;
    unsigned int *counted = NULL;
    const unsigned long long record_bytes = REVERSI_LIMBS * 4ULL;
    ok = ok && sim_status_check(results, cudaMalloc((void **)&boards[0], games * record_bytes), "boards") &&
         sim_status_check(results, cudaMalloc((void **)&boards[1], games * record_bytes), "boards") &&
         sim_status_check(results, cudaMalloc((void **)&choices, games * 4ULL), "choices") &&
         sim_status_check(results, cudaMalloc((void **)&chosen, games * choose.layout.out_limbs * 4ULL), "chosen") &&
         sim_status_check(results, cudaMalloc((void **)&index, games * 2ULL * 4ULL), "index") &&
         sim_status_check(results, cudaMalloc((void **)&index_one, games * 4ULL), "index of one member") &&
         sim_status_check(results, cudaMalloc((void **)&counted, games * count.layout.out_limbs * 4ULL), "counts");
    unsigned int *const host = (unsigned int *)platform_cell_cleared(
        (size_t)games * REVERSI_LIMBS, sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const host_index = (unsigned int *)platform_cell_taken((size_t)games * 2u * sizeof(unsigned int),
                                                                         platform_place_g_program_owner, 0);
    unsigned int *const host_counted = (unsigned int *)platform_cell_taken((size_t)games * count.layout.out_limbs * 4u,
                                                                           platform_place_g_program_owner, 0);
    unsigned int *const host_choices =
        (unsigned int *)platform_cell_cleared(games, sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const passes =
        (unsigned int *)platform_cell_cleared(games, sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const which =
        (unsigned int *)platform_cell_taken((size_t)games * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const bounds =
        (unsigned int *)platform_cell_taken((size_t)games * sizeof(unsigned int), platform_place_g_program_owner, 0);
    unsigned int *const draws =
        (unsigned int *)platform_cell_taken((size_t)games * sizeof(unsigned int), platform_place_g_program_owner, 0);
    ok = ok && (host != NULL) && (host_index != NULL) && (host_counted != NULL) && (host_choices != NULL) &&
         (passes != NULL) && (which != NULL) && (bounds != NULL) && (draws != NULL);
    // the opening: white on d4 and e5, black on e4 and d5, and black to move
    const unsigned int opening[4][2] = {{27u, 2u}, {28u, 1u}, {35u, 1u}, {36u, 2u}};
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        unsigned int *const record = host + ((unsigned long long)game * REVERSI_LIMBS);
        for (unsigned int piece = 0u; piece < 4u; piece += 1u)
        {
            const unsigned int bit = opening[piece][0] * REVERSI_SQUARE_BITS;
            record[bit / 32u] |= opening[piece][1] << (bit % 32u);
            // a field of 3 bits at a place past bit 29 of a limb runs into the next limb
            if ((bit % 32u) > 29u)
            {
                record[(bit / 32u) + 1u] |= opening[piece][1] >> (32u - (bit % 32u));
            }
        }
        record[REVERSI_COLOR_OFFSET / 32u] |= 1u << (REVERSI_COLOR_OFFSET % 32u);
        host_index[(2u * game) + 0u] = game;
        host_index[(2u * game) + 1u] = game;
    }
    ok = ok &&
         sim_status_check(results, cudaMemcpy(boards[0], host, games * record_bytes, cudaMemcpyHostToDevice),
                          "openings sent") &&
         sim_status_check(results, cudaMemcpy(index, host_index, games * 2ULL * 4ULL, cudaMemcpyHostToDevice),
                          "index sent");
    // the index of the count, which reads one member: one entry a lane, the game of the lane
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        host_index[game] = game;
    }
    ok = ok && sim_status_check(results, cudaMemcpy(index_one, host_index, games * 4ULL, cudaMemcpyHostToDevice),
                                "index of one member sent");
    const unsigned int moves_offset = count.layout.step_table[count_steps[0]].out_offset;
    const unsigned int ties_offset = count.layout.step_table[count_steps[1]].out_offset;
    const unsigned int corner_offset = count.layout.step_table[count_steps[2]].out_offset;
    const unsigned int count_limbs = count.layout.out_limbs;
    int current = 0;
    unsigned int finished = 0u;
    for (unsigned int turn = 0u; ok && (finished < games); turn += 1u)
    {
        const CycleRecordRunRequest run_count = {
            count.record, {boards[current], NULL, NULL}, {games, 0ULL, 0ULL}, index_one, games, counted, &error};
        ok = (cycle_record_run(&run_count) == (long)games) &&
             sim_status_check(results,
                              cudaMemcpy(host_counted, counted, games * count_limbs * 4ULL, cudaMemcpyDeviceToHost),
                              "counts read");
        unsigned long long drawing = 0ULL;
        for (unsigned int game = 0u; ok && (game < games); game += 1u)
        {
            const unsigned int *const record = host_counted + ((unsigned long long)game * count_limbs);
            // each count is below 2^7, a field inside one limb or across two
            const unsigned long long low = record[moves_offset / 32u];
            const unsigned long long high =
                ((moves_offset / 32u) + 1u < count_limbs) ? record[(moves_offset / 32u) + 1u] : 0u;
            const unsigned int moves = (unsigned int)((((high << 32u) | low) >> (moves_offset % 32u)) & 127u);
            const unsigned long long tie_low = record[ties_offset / 32u];
            const unsigned long long tie_high =
                ((ties_offset / 32u) + 1u < count_limbs) ? record[(ties_offset / 32u) + 1u] : 0u;
            const unsigned int ties = (unsigned int)((((tie_high << 32u) | tie_low) >> (ties_offset % 32u)) & 127u);
            const unsigned int corner = (record[corner_offset / 32u] >> (corner_offset % 32u)) & 1u;
            if (passes[game] >= 2u)
            {
                continue;
            }
            if (moves == 0u)
            {
                passes[game] += 1u;
                finished += (passes[game] == 2u) ? 1u : 0u;
                continue;
            }
            passes[game] = 0u;
            const int draws_now = (players[game] == REVERSI_RANDOM) || (corner == 0u);
            if (draws_now)
            {
                which[drawing] = game;
                bounds[drawing] = (players[game] == REVERSI_RANDOM) ? moves : ties;
                drawing += 1ULL;
            }
            host_choices[game] = players[game] << 8u;
        }
        ok = ok && twister_below(results, &twister, which, drawing, bounds, draws);
        for (unsigned long long at = 0ULL; ok && (at < drawing); at += 1ULL)
        {
            host_choices[which[at]] = (players[which[at]] << 8u) | draws[at];
        }
        const CycleRecordRunRequest run_choose = {
            choose.record, {boards[current], choices, NULL}, {games, games, 0ULL}, index, games, chosen, &error};
        const CycleRecordRunRequest run_flip = {
            flip.record, {boards[current], chosen, NULL}, {games, games, 0ULL}, index, games, boards[1 - current],
            &error};
        ok = ok &&
             sim_status_check(results, cudaMemcpy(choices, host_choices, games * 4ULL, cudaMemcpyHostToDevice),
                              "choices sent") &&
             (cycle_record_run(&run_choose) == (long)games) && (cycle_record_run(&run_flip) == (long)games);
        current = 1 - current;
        if (turn >= 200u)
        {
            sim_check(results, 0, "every game of Reversi ends inside 200 turns");
            ok = 0;
        }
    }
    ok =
        ok && sim_status_check(results, cudaMemcpy(host, boards[current], games * record_bytes, cudaMemcpyDeviceToHost),
                               "finals read");
    for (unsigned int game = 0u; ok && (game < games); game += 1u)
    {
        const unsigned int *const record = host + ((unsigned long long)game * REVERSI_LIMBS);
        for (unsigned int square = 0u; square < REVERSI_SQUARES; square += 1u)
        {
            const unsigned int bit = square * REVERSI_SQUARE_BITS;
            unsigned long long word = record[bit / 32u];
            if (((bit / 32u) + 1u) < REVERSI_LIMBS)
            {
                word |= (unsigned long long)record[(bit / 32u) + 1u] << 32u;
            }
            // a square holds 0, 1 or 2
            finals[((unsigned long long)game * REVERSI_SQUARES) + square] = (unsigned char)((word >> (bit % 32u)) & 3u);
        }
    }
    twister_close(&twister);
    game_release(&count);
    game_release(&choose);
    game_release(&flip);
    cudaFree(boards[0]);
    cudaFree(boards[1]);
    cudaFree(choices);
    cudaFree(chosen);
    cudaFree(index);
    cudaFree(index_one);
    cudaFree(counted);
    platform_cell_released(host, platform_place_g_program_owner);
    platform_cell_released(host_index, platform_place_g_program_owner);
    platform_cell_released(host_counted, platform_place_g_program_owner);
    platform_cell_released(host_choices, platform_place_g_program_owner);
    platform_cell_released(passes, platform_place_g_program_owner);
    platform_cell_released(which, platform_place_g_program_owner);
    platform_cell_released(bounds, platform_place_g_program_owner);
    platform_cell_released(draws, platform_place_g_program_owner);
    return ok;
}

#endif
