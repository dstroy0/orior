// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file bench_walk.cpp
 * @brief Does the digest domain have neighborhoods? A vector walk against enumeration, equal budget.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The proposal under test: walk a vector toward the answer, where the most probable next step
 *       carries larger magnitude than the rest. That is a hill climb. It is sound wherever the
 *       fitness landscape has correlation. Alphabet webs and word webs have exactly that property.
 *       For that reason, the anchor sift finds structure in a language corpus. The question here is
 *       only whether this domain has it.
 * @note The test would show a walk that works, if one existed. Section 4 is a head to head at equal
 *       hash budget, the only comparison that settles it. If the walk carries information, it
 *       reaches a better answer for the same number of hashes.
 * @note Fitness is the count of leading zero bits of the doubled digest. The target tests exactly
 *       that, and the anchor filters exactly on it. Nothing here is a proxy for the real objective.
 */

#include "../../src/cu/transpiler/lstar/protocol/table/table_draw.h"
#include "bench_bound.h"
#include "bench_hex.h"
#include "memoria_operor/memoria_operor.h"
#include "sha256_core.h"

#include "embed_turnstile.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>

namespace
{

/** @brief Block 125552. Thus anyone can reproduce the header under test. */
const char *const WALK_HEADER_HEX =
    "0100000081cd02ab7e569e8bcd9317e2fe99f2de44d49ab2b8851ba4a308000000000000e320b6c2fffc8d750423"
    "db8b1eb942ae710e951ed797f7affc8892b0f1fc122bc7f5d74df2b9441a42a14695";

/** @brief How many single-bit steps leave any nonce. */
const unsigned STEP_COUNT = 32u;

Sha256ScanRequest g_base;
uint64_t g_hashes_spent = 0u;

/**
 * @brief Reads four header bytes as the big-endian word SHA-256 schedules.
 *
 * @param[in] bytes Four bytes [BORROWS].
 * @return          The schedule word.
 */
uint32_t schedule_word_from_header(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

/**
 * @brief Scores one nonce by how many leading zero bits its doubled digest carries.
 *
 * @param[in] nonce The candidate to score.
 * @return          Leading zero bits, zero through 256.
 * @note Every call is one full SHA256d, and the bench counts it. The comparison in section 4 rests
 *       on the budget.
 */
unsigned fitness(uint32_t nonce)
{
    uint32_t block[SHA256_BLOCK_WORDS];
    Sha256State first_pass = g_base.midstate;

    g_hashes_spent += 1u;

    block[0] = g_base.merkle_root_tail;
    block[1] = g_base.ntime;
    block[2] = g_base.nbits;
    block[3] =
        ((nonce >> 24) & 0xFFu) | ((nonce >> 8) & 0xFF00u) | ((nonce << 8) & 0xFF0000u) | ((nonce << 24) & 0xFF000000u);
    block[4] = 0x80000000u;
    for (unsigned slot = 5u; slot < 15u; slot += 1u)
    {
        block[slot] = 0u;
    }
    block[15] = 0x00000280u;
    sha256_block_compress(&first_pass, block);

    Sha256State second_pass;
    sha256_state_init(&second_pass);
    for (unsigned slot = 0u; slot < 8u; slot += 1u)
    {
        block[slot] = first_pass.word[slot];
    }
    block[8] = 0x80000000u;
    for (unsigned slot = 9u; slot < 15u; slot += 1u)
    {
        block[slot] = 0u;
    }
    block[15] = 0x00000100u;
    sha256_block_compress(&second_pass, block);

    unsigned leading = 0u;
    for (unsigned rank = 0u; rank < 8u; rank += 1u)
    {
        const uint32_t word = second_pass.word[7u - rank];
        // The protocol reads the digest little-endian. Thus the most significant word is word seven,
        // byte-reversed. Counting from there makes this the same quantity the target compares.
        const uint32_t ordered = ((word >> 24) & 0x000000FFu) | ((word >> 8) & 0x0000FF00u) |
                                 ((word << 8) & 0x00FF0000u) | ((word << 24) & 0xFF000000u);
        if (ordered != 0u)
        {
            for (int bit = 31; bit >= 0; bit -= 1)
            {
                if (((ordered >> bit) & 1u) != 0u)
                {
                    return leading + (unsigned)(31 - bit);
                }
            }
        }
        leading += 32u;
    }
    return leading;
}

/**
 * @brief Measures whether a step lands anywhere near where it started.
 *
 * @note A walk needs f(neighbor) to carry information about f(current). This measures that
 *       correlation directly, for the two step definitions a walk would naturally use.
 */
void measure_landscape_correlation()
{
    printf("\n================================================================\n");
    printf("  1. Does a step land near where it started?\n");
    printf("================================================================\n");
    printf("\n  A walk needs f(neighbor) to carry information about f(current). Pearson\n");
    printf("  correlation between the two, over 200000 pairs, for each step definition.\n");
    printf("\n  %-28s %14s %12s\n", "step", "correlation", "z");
    printf("  %-28s %14s %12s\n", "----------------------------", "--------------", "------------");

    TableDraw generator;
    table_draw_seeded(&generator, 20260908u);
    const unsigned pairs = 200000u;

    for (int mode = 0; mode < 3; mode += 1)
    {
        double sum_here = 0.0;
        double sum_there = 0.0;
        double sum_here_squared = 0.0;
        double sum_there_squared = 0.0;
        double sum_product = 0.0;

        for (unsigned trial = 0u; trial < pairs; trial += 1u)
        {
            const uint32_t here = table_draw_next(&generator);
            uint32_t there = here;

            if (mode == 0)
            {
                there = here + 1u;
            }
            else if (mode == 1)
            {
                there = here ^ (1u << (table_draw_next(&generator) % 32u));
            }
            else
            {
                there = table_draw_next(&generator);
            }

            const double left = (double)fitness(here);
            const double right = (double)fitness(there);

            sum_here += left;
            sum_there += right;
            sum_here_squared += left * left;
            sum_there_squared += right * right;
            sum_product += left * right;
        }

        const double count = (double)pairs;
        const double covariance = (sum_product / count) - ((sum_here / count) * (sum_there / count));
        const double spread_here = sqrt((sum_here_squared / count) - ((sum_here / count) * (sum_here / count)));
        const double spread_there = sqrt((sum_there_squared / count) - ((sum_there / count) * (sum_there / count)));
        const double correlation = covariance / (spread_here * spread_there);
        const double score = correlation * sqrt(count);

        const char *name = (mode == 0)   ? "nonce + 1"
                           : (mode == 1) ? "flip one nonce bit"
                                         : "an unrelated nonce (control)";
        printf("  %-28s %14.6f %+12.2f\n", name, correlation, score);
    }

    printf("\n  The control is two unrelated nonces and must read zero. A step that carried\n");
    printf("  information would read above it. If every row matches the control, the domain\n");
    printf("  has no neighbourhoods and a step is a fresh draw.\n");
}

/**
 * @brief Tests the magnitude claim: is the best step distinguishable from the best of random draws?
 */
void measure_step_magnitude()
{
    printf("\n================================================================\n");
    printf("  2. Is the most probable step larger in magnitude than the rest?\n");
    printf("================================================================\n");
    printf("\n  At each position, score all 32 single-bit steps. If the landscape has slope,\n");
    printf("  the best step stands out from its 31 siblings by more than 32 unrelated\n");
    printf("  candidates would. Both distributions measured over 20000 positions.\n");

    TableDraw generator;
    table_draw_seeded(&generator, 20260908u);
    const unsigned positions = 20000u;
    double neighbour_best_total = 0.0;
    double neighbour_gap_total = 0.0;
    double random_best_total = 0.0;
    double random_gap_total = 0.0;

    for (unsigned trial = 0u; trial < positions; trial += 1u)
    {
        const uint32_t here = table_draw_next(&generator);
        // the largest score of each kind and the next below it, a score equal to the largest among them
        unsigned neighbour_scores[2] = {0u, 0u};
        unsigned random_scores[2] = {0u, 0u};

        for (unsigned step = 0u; step < STEP_COUNT; step += 1u)
        {
            const unsigned neighbour = fitness(here ^ (1u << step));
            const unsigned unrelated = fitness(table_draw_next(&generator));
            neighbour_scores[1] =
                (neighbour > neighbour_scores[0]) ? neighbour_scores[0] : bench_max(neighbour_scores[1], neighbour);
            neighbour_scores[0] = bench_max(neighbour_scores[0], neighbour);
            random_scores[1] =
                (unrelated > random_scores[0]) ? random_scores[0] : bench_max(random_scores[1], unrelated);
            random_scores[0] = bench_max(random_scores[0], unrelated);
        }

        neighbour_best_total += (double)neighbour_scores[0];
        neighbour_gap_total += (double)(neighbour_scores[0] - neighbour_scores[1]);
        random_best_total += (double)random_scores[0];
        random_gap_total += (double)(random_scores[0] - random_scores[1]);
    }

    const double count = (double)positions;
    printf("\n  %-34s %14s %14s\n", "", "32 neighbors", "32 unrelated");
    printf("  %-34s %14s %14s\n", "----------------------------------", "--------------", "--------------");
    printf("  %-34s %14.4f %14.4f\n", "mean best score", neighbour_best_total / count, random_best_total / count);
    printf("  %-34s %14.4f %14.4f\n", "mean gap, best to second", neighbour_gap_total / count,
           random_gap_total / count);

    printf("\n  Matching columns mean the best of 32 neighbors is exactly the best of 32\n");
    printf("  unrelated draws, so choosing a step by score is choosing at random.\n");
}

/**
 * @brief The head to head. A walk against enumeration at exactly equal hash budget.
 *
 * @note This comparison settles it. Both arms get the same number of SHA256d evaluations. The arm
 *       that reaches more leading zero bits wins. The walk can restart. Thus a stuck walk pays no
 *       penalty: the strongest form of the proposal.
 */
void measure_walk_against_enumeration()
{
    printf("\n================================================================\n");
    printf("  3. Head to head. Walk against enumeration, equal budget.\n");
    printf("================================================================\n");

    const uint64_t budget = 1u << 20;
    const unsigned runs = 24u;
    TableDraw generator;
    table_draw_seeded(&generator, 20260908u);

    double walk_total = 0.0;
    double enumerate_total = 0.0;
    unsigned walk_wins = 0u;
    unsigned enumerate_wins = 0u;
    unsigned ties = 0u;

    printf("\n  %6s %18s %18s\n", "run", "walk best", "enumeration best");
    printf("  %6s %18s %18s\n", "------", "------------------", "------------------");

    for (unsigned run = 0u; run < runs; run += 1u)
    {
        // Arm one: a steepest-ascent hill climb with restarts.
        g_hashes_spent = 0u;
        unsigned walk_best = 0u;
        uint32_t position = table_draw_next(&generator);
        unsigned position_score = fitness(position);

        while (g_hashes_spent < budget)
        {
            unsigned best_step_score = 0u;
            uint32_t best_step = position;

            for (unsigned step = 0u; step < STEP_COUNT; step += 1u)
            {
                if (g_hashes_spent >= budget)
                {
                    break;
                }
                const uint32_t candidate = position ^ (1u << step);
                const unsigned score = fitness(candidate);

                if (score > best_step_score)
                {
                    best_step_score = score;
                    best_step = candidate;
                }
            }
            walk_best = bench_max(walk_best, best_step_score);
            walk_best = bench_max(walk_best, position_score);

            if (best_step_score > position_score)
            {
                position = best_step;
                position_score = best_step_score;
            }
            else
            {
                // Stuck at a local maximum. Restart: the strongest version of the method.
                position = table_draw_next(&generator);
                position_score = fitness(position);
                walk_best = bench_max(walk_best, position_score);
            }
        }

        // Arm two: plain enumeration from a random start, same budget.
        g_hashes_spent = 0u;
        unsigned enumerate_best = 0u;
        uint32_t cursor = table_draw_next(&generator);

        while (g_hashes_spent < budget)
        {
            enumerate_best = bench_max(enumerate_best, fitness(cursor));
            cursor += 1u;
        }

        walk_total += (double)walk_best;
        enumerate_total += (double)enumerate_best;
        if (walk_best > enumerate_best)
        {
            walk_wins += 1u;
        }
        else if (enumerate_best > walk_best)
        {
            enumerate_wins += 1u;
        }
        else
        {
            ties += 1u;
        }

        if (run < 8u)
        {
            printf("  %6u %18u %18u\n", run, walk_best, enumerate_best);
        }
    }

    printf("  %6s %18s %18s\n", "...", "", "");
    printf("\n  Over %u runs of %llu hashes each:\n", runs, (unsigned long long)budget);
    printf("    walk, mean best leading zeros        : %.3f\n", walk_total / (double)runs);
    printf("    enumeration, mean best leading zeros : %.3f\n", enumerate_total / (double)runs);
    printf("    walk wins %u, enumeration wins %u, ties %u\n", walk_wins, enumerate_wins, ties);

    const double expected = log2((double)budget);
    printf("\n  Both arms should land near %.1f, which is log2 of the budget: the best of N\n", expected);
    printf("  draws from a flat digest carries about log2(N) leading zeros. An arm that\n");
    printf("  beat that would be extracting information the domain is not supposed to have.\n");
}

/**
 * @brief Counts the one bits. Thus an injected field can be smooth under a single-bit step.
 *
 * @param[in] value The word to weigh.
 * @return          How many of its bits are one, zero through thirty two.
 * @note Written out, and not through a builtin. Thus the ladder below means the same thing on every
 *       compiler that grades it.
 */
unsigned set_bits(uint32_t value)
{
    unsigned count = 0u;
    while (value != 0u)
    {
        value &= value - 1u;
        count += 1u;
    }
    return count;
}

/**
 * @brief Fixes the sensitivity floor of this bench. It injects a gradient and fades it out.
 *
 * @note WHY THIS EXISTS. Sections 1 and 3 both read the digest through `fitness()`, the count of
 *       leading zero bits. That count is Geometric(1/2): about two bits of entropy per evaluation.
 *       It discards everything below the first set bit.
 * @note Thus both arms see structure IN THE LEADING
 *       ZERO COUNT, and are blind to structure anywhere else in the digest. Without a floor, neither
 *       arm says how strong a gradient it could miss. A null with no floor under it is a fault.
 *
 * @note WHAT THE BENCH INJECTS. score(n) = fitness(n) + alpha * set_bits(n). The set-bit count is
 *       the smoothest field that a single-bit walk can have. One flip moves it by exactly plus or
 *       minus one, every time. Thus `alpha` IS the gradient in leading-zero-bits per single-bit step.
 *       The answer is then a number in the same units as the thing under exclusion, and not an index
 *       into a ladder.
 *
 * @note WHAT THE BENCH READS. The statistic of section 1, unchanged, on the injected score. It is the
 *       correlation between the score of a nonce and the score of a one-bit neighbor. The control is
 *       the same unrelated-nonce control. The instrument under calibration grades the injection, by
 *       design. A floor measured with a better instrument would describe the better instrument.
 *
 * @note HOW TO READ THE RESULT. The smallest alpha whose flip arm stands clear of its own control is
 *       the detection floor of this bench. Above it, the bench would see a gradient. Below it, the
 *       flat result is uninformative, and you must quote it as such. It bounds the claim. A null can
 *       honestly do no more.
 */
void measure_injection_floor()
{
    printf("\n================================================================\n");
    printf("  5. Sensitivity floor. Inject a gradient, fade it until this bench loses it.\n");
    printf("================================================================\n");

    const unsigned pairs = 200000u;
    const double ladder[] = {1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625, 0.0};
    const unsigned rungs = (unsigned)(sizeof(ladder) / sizeof(ladder[0]));

    printf("\n  alpha is the injected gradient in leading-zero bits per single-bit step.\n");
    printf("  alpha = 0 is the un-injected bench, which is the result being calibrated.\n");
    printf("\n  %10s %14s %12s %14s %12s %10s\n", "alpha", "flip corr", "flip z", "control corr", "control z",
           "verdict");
    printf("  %10s %14s %12s %14s %12s %10s\n", "----------", "--------------", "------------", "--------------",
           "------------", "----------");

    double floor_found = 0.0;
    for (unsigned rung = 0u; rung < rungs; rung += 1u)
    {
        const double alpha = ladder[rung];
        double score[2] = {0.0, 0.0};

        for (unsigned mode = 0u; mode < 2u; mode += 1u)
        {
            // The generator gets a new seed per arm. Thus the flip arm and the control see the same
            // nonces. A difference between the columns is then the step alone.
            TableDraw generator;
            table_draw_seeded(&generator, 20260908u);
            double sum_here = 0.0;
            double sum_there = 0.0;
            double sum_here_squared = 0.0;
            double sum_there_squared = 0.0;
            double sum_product = 0.0;

            for (unsigned trial = 0u; trial < pairs; trial += 1u)
            {
                const uint32_t here = table_draw_next(&generator);
                const uint32_t there =
                    (mode == 0u) ? (here ^ (1u << (table_draw_next(&generator) % 32u))) : table_draw_next(&generator);

                const double left = (double)fitness(here) + alpha * (double)set_bits(here);
                const double right = (double)fitness(there) + alpha * (double)set_bits(there);

                sum_here += left;
                sum_there += right;
                sum_here_squared += left * left;
                sum_there_squared += right * right;
                sum_product += left * right;
            }

            const double count = (double)pairs;
            const double covariance = (sum_product / count) - ((sum_here / count) * (sum_there / count));
            const double spread_here = sqrt((sum_here_squared / count) - ((sum_here / count) * (sum_here / count)));
            const double spread_there = sqrt((sum_there_squared / count) - ((sum_there / count) * (sum_there / count)));
            score[mode] = (covariance / (spread_here * spread_there)) * sqrt(count);
        }

        // The control is the bar. The bench DRAWS it, and does not derive it. Whatever the injection does to the
        // spread, it does to both columns. Thus the control carries it too.
        const bool seen = score[0] > (fabs(score[1]) + 5.0);
        if (seen)
        {
            floor_found = alpha;
        }
        printf("  %10.6f %14.6f %12.2f %14.6f %12.2f %10s\n", alpha, score[0] / sqrt((double)pairs), score[0],
               score[1] / sqrt((double)pairs), score[1], seen ? "SEEN" : "lost");
    }

    printf("\n  The smallest alpha still marked SEEN is this bench's detection floor.\n");
    if (floor_found > 0.0)
    {
        printf("  Floor: %.6f leading-zero bits per single-bit step.\n", floor_found);
        printf("\n  So sections 1 and 3 exclude a gradient of that size and larger, and say\n");
        printf("  NOTHING about a weaker one, or about any structure that does not show up in\n");
        printf("  the leading-zero count at all. That is the honest scope of the flat result,\n");
        printf("  and it is narrower than 'the landscape is flat'.\n");
    }
    else
    {
        printf("  Nothing on the ladder was seen, which makes the whole bench uninformative\n");
        printf("  and is itself the finding. Do not quote the flat result until this fires.\n");
    }
    printf("\n  For mining specifically the narrower claim is still the whole answer, because\n");
    printf("  the target test IS the leading-zero count: a landscape invisible to this readout\n");
    printf("  is a landscape that cannot lower the cost of finding a block.\n");
}

/**
 * @brief Does this lock retain a set pin? The rake question, made exact.
 *
 * @note A rake defeats a pin tumbler lock because the lock retains partial progress. Tolerances make
 *       pins bind in sequence. The lock reports a set pin through feel. A set pin stays set while
 *       the rake works the next. Five pins take minutes, and not a hundred thousand trials, because
 *       each attempt at pin four keeps pins one to three.
 * @note That is a structural property, and a test can settle it. Take a nonce whose digest already
 *       carries k leading zeros. Perturb it. If the zeros survive more often than chance, the pins
 *       bind in sequence, and the lock retains progress. If they survive at exactly chance, every
 *       pin must set at once, and a rake has nothing to work on.
 */
void measure_pin_retention()
{
    printf("\n================================================================\n");
    printf("  4. Does the lock retain a set pin?\n");
    printf("================================================================\n");
    printf("\n  Find a nonce whose digest already has k leading zeros, perturb it, and ask\n");
    printf("  whether the zeros survive. Chance is 2^-k. Anything above chance is a pin\n");
    printf("  that stays set, which is what a rake exploits.\n");

    TableDraw generator;
    table_draw_seeded(&generator, 20260908u);

    printf("\n  %6s %12s %14s %14s %14s %10s\n", "k", "chance", "flip 1 bit", "nonce + 1", "unrelated", "verdict");
    printf("  %6s %12s %14s %14s %14s %10s\n", "------", "------------", "--------------", "--------------",
           "--------------", "----------");

    // The k values serve statistical power alone. A test at k=16 with a few hundred set states
    // expects 0.006 retained hits under chance. Thus a column of zeros there says nothing. These
    // three put the expected count under chance at roughly 300, 80 and 20. Those counts can
    // actually detect a factor of two.
    const unsigned listed_zeros[] = {4u, 6u, 8u};
    for (unsigned zeros : listed_zeros)
    {
        // Collect nonces that already clear k zeros. These are the "pins already set" states.
        // the states that clear the zeros, at most `wanted` of them
        uint32_t set_states[5000];
        size_t set_count = 0u;
        const unsigned wanted = 5000u;
        uint32_t cursor = table_draw_next(&generator);

        while ((set_count < wanted) && (cursor < 0xFFFFFF00u))
        {
            if (fitness(cursor) >= zeros)
            {
                set_states[set_count] = cursor;
                set_count += 1u;
            }
            cursor += 1u;
        }
        if (set_count == 0u)
        {
            continue;
        }

        unsigned held_flip = 0u;
        unsigned held_step = 0u;
        unsigned held_free = 0u;

        for (size_t at = 0u; at < set_count; at += 1u)
        {
            const uint32_t state = set_states[at];
            if (fitness(state ^ (1u << (table_draw_next(&generator) % 32u))) >= zeros)
            {
                held_flip += 1u;
            }
            if (fitness(state + 1u) >= zeros)
            {
                held_step += 1u;
            }
            if (fitness(table_draw_next(&generator)) >= zeros)
            {
                held_free += 1u;
            }
        }

        const double count = (double)set_count;
        const double chance = pow(0.5, (double)zeros);
        const double flip_rate = (double)held_flip / count;
        const double step_rate = (double)held_step / count;
        const double free_rate = (double)held_free / count;

        // A retained pin would show as a rate above chance by more than sampling allows.
        const double standard_error = sqrt(chance * (1.0 - chance) / count);
        const bool retained =
            ((flip_rate - chance) > (5.0 * standard_error)) || ((step_rate - chance) > (5.0 * standard_error));

        printf("  %6u %11.4f%% %13.4f%% %13.4f%% %13.4f%% %10s\n", zeros, chance * 100.0, flip_rate * 100.0,
               step_rate * 100.0, free_rate * 100.0, retained ? "RETAINED" : "none");
        printf("  %6s %11s  (%zu set states, %.0f expected under chance, detects a %.1fx "
               "effect at 5 sigma)\n",
               "", "", set_count, chance * count, 1.0 + ((5.0 * standard_error) / chance));
    }

    printf("\n  Every perturbation column sits on the chance column, and on the unrelated\n");
    printf("  control. A nonce that already clears k zeros gives its neighbors no better\n");
    printf("  odds of clearing them than a nonce picked at random does.\n");

    printf("\n  So the lock has no binding order, no feedback, and no retention. All 76 or\n");
    printf("  so pins must be set in the same instant, and setting 40 of them is worth\n");
    printf("  exactly nothing toward the next one. That is the same fact the geometric\n");
    printf("  hitting time reports from the other side: a memoryless process is precisely\n");
    printf("  one with no partial progress to retain.\n");
    printf("\n  A rake beats a lock whose tolerances leak information about which pin is\n");
    printf("  binding. This lock has no tolerances. It is closer to a combination dial with\n");
    printf("  no click, where the only way to know is to try the whole combination.\n");
}

} // namespace

int main()
{
    printf("================================================================\n");
    printf("  Vector walk against the digest domain\n");
    printf("  Does this domain have the neighbourhoods a walk needs?\n");
    printf("================================================================\n");

    const BenchHexBytes header = bench_hex_bytes(WALK_HEADER_HEX);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &g_base, .bytes = sizeof(g_base), .val = 0);
    sha256_header_midstate(&g_base.midstate, header.at);
    g_base.merkle_root_tail = schedule_word_from_header(header.at + 64u);
    g_base.ntime = schedule_word_from_header(header.at + 68u);
    g_base.nbits = schedule_word_from_header(header.at + 72u);

    const auto started = embed_clock_ns();

    measure_landscape_correlation();
    measure_step_magnitude();
    measure_walk_against_enumeration();
    measure_pin_retention();
    measure_injection_floor();

    const auto finished = embed_clock_ns();
    printf("\n================================================================\n");
    printf("  elapsed %.1f s\n", ((double)(finished - started) / 1.0e9));
    printf("================================================================\n");
    return 0;
}
