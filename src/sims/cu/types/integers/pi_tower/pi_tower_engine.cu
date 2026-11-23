// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pi_tower_engine.cu: the engine, depth and requests
#include "../../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "pi_tower_internal.h"

// 13 and 14 on the engine: position 0 and Bailey's, and the tower's deepest resolution requested. 15, a segment, is a
// request of its own (pi_tower_segment_request)
void pi_tower_engine(SimResults *results, int count, char **arguments, const PiTowerTurn &turn, unsigned int depth)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const unsigned long long lanes = pi_tower_bbp_lanes();
    unsigned int window = 0u;
    const PiWide tower_position = pi_tower_depth_position(pi_tower_unsigned(depth), &window);
    // a plan for each published position and one for the tower's, each its own run
    static PiTowerBbp s_plans[PI_TOWER_PUBLISHED_COUNT + 1u];
    PiTowerBbp *const plans = s_plans;
    const size_t plans_count = PI_TOWER_PUBLISHED_COUNT + 1u;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = s_plans, .bytes = sizeof(s_plans), .val = 0);
    int planned = lanes != 0ULL;
    unsigned long long declared = 0ULL;
    for (size_t at = 0u; (planned != 0) && (at < plans_count); at += 1u)
    {
        const PiWide position = (at < PI_TOWER_PUBLISHED_COUNT)
                                    ? pi_tower_unsigned(s_pi_tower_published[at].position - 1ULL)
                                    : tower_position;
        planned = pi_tower_bbp_plan(&plans[at], position, lanes, &error);
        const unsigned long long bytes = (planned != 0) ? pi_tower_bbp_bytes(&plans[at]) : 0ULL;
        declared = (bytes > declared) ? bytes : declared;
    }
    sim_check(results, planned, "engine: keymath encodes and the scheduler arranges the term, tail and pair programs");
    if ((planned == 0) || !sim_job_submit(results, "pi_tower", count, arguments, declared))
    {
        for (size_t at = 0u; at < plans_count; at += 1u)
        {
            pi_tower_bbp_free(&plans[at]);
        }
        return;
    }
    mmgr_line_put(&results->line, "  on the engine, pi's hex digits by BBP, each term a lane of the record machine\n");
    sim_flush(results);
    int published = 1;
    int finished = 1;
    for (size_t at = 0u; at < PI_TOWER_PUBLISHED_COUNT; at += 1u)
    {
        PiTowerBbpRun run;
        const int ran = pi_tower_bbp_sum(results, &plans[at], 0, &run);
        const unsigned int certified = (ran != 0) ? pi_tower_bbp_certified(&plans[at], run.sum) : 0u;
        pi_tower_bbp_print(results, &plans[at], &run, certified);
        const char *const expected = s_pi_tower_published[at].digits;
        finished = finished && ran && run.finished;
        // a published string is at max 23 digits. Its length fits an unsigned int
        const unsigned int expected_length =
            (unsigned int)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = expected, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        const int readable = published && ran && run.finished && ((4u * expected_length) <= certified);
        char hex[PI_TOWER_HEX_MAX];
        if (readable != 0)
        {
            pi_tower_bbp_hex(&plans[at], run.sum, expected_length, hex, sizeof(hex));
        }
        published = readable &&
                    (EMBED_CALL(memor.cmp, MemoriaCfg, .src = hex, .other = expected, .bytes = expected_length) == 0);
    }
    sim_check(results, finished, "engine: every run sums all its terms");
    sim_check(results, published,
              "on the engine, pi's first 64 bits are 0x243F6A8885A308D3 and its hex digits at Bailey's positions 10^6, "
              "10^6 + 1, 10^7 and 10^8 are his");
    PiTowerBbpRun run;
    PiTowerBbp *const tower = &plans[PI_TOWER_PUBLISHED_COUNT];
    const int ran = pi_tower_bbp_sum(results, tower, 0, &run);
    const unsigned int certified = (ran != 0) ? pi_tower_bbp_certified(tower, run.sum) : 0u;
    pi_tower_bbp_print(results, tower, &run, certified);
    const unsigned long long engine_cell = (ran != 0) ? pi_tower_bbp_cell(tower, run.sum, window) : 0ULL;
    const unsigned long long exact_cell =
        pi_tower_word(pi_tower_quotient(turn.multiplier, pi_tower_power_two(turn.precision - depth)));
    mmgr_line_put(&results->line, "    the cell at 2^");
    mmgr_line_decimal(&results->line, depth, 1u);
    mmgr_line_put(&results->line, ", its low 64 bits: engine 0x");
    mmgr_line_hex(&results->line, engine_cell, 16u);
    mmgr_line_put(&results->line, ", exact turn 0x");
    mmgr_line_hex(&results->line, exact_cell, 16u);
    mmgr_line_ch(&results->line, '\n');
    sim_check(results, ran && run.finished && (certified >= window) && (engine_cell == exact_cell),
              "the engine's cell at the deepest resolution requested, its low 64 bits, is the exact turn's");
    for (size_t at = 0u; at < plans_count; at += 1u)
    {
        pi_tower_bbp_free(&plans[at]);
    }
    sim_flush(results);
}

// A resolution past that of the exact turn, as (2, n). The sim prints exact every part that does not need the bits of
// pi. The engine sums the turn at depth n sweep by sweep, until every term is in or an interrupt stops the run
void pi_tower_deep(SimResults *results, int count, char **arguments, const PiWide &depth)
{
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    const PiWide word = pi_tower_unsigned(64ULL);
    const PiWide precision = pi_tower_product(
        pi_tower_quotient(pi_tower_sum(pi_tower_sum(pi_tower_product(depth, pi_tower_unsigned(3ULL)), word),
                                       pi_tower_unsigned(63ULL)),
                          word),
        word);
    const PiWide needed = pi_tower_sum(
        pi_tower_product(pi_tower_sum(precision, pi_tower_unsigned(PI_TOWER_GUARD)), pi_tower_unsigned(3ULL)), word);
    // the exact width a host turn would need, as a power of two of limbs
    unsigned int width = 0u;
    while (pi_tower_compare(pi_tower_product(pi_tower_power_two(width), pi_tower_unsigned(32ULL)), needed) < 0)
    {
        width += 1u;
    }
    mmgr_line_put(&results->line, "  resolution (2, n): 2^n cells, n = ");
    pi_tower_print_decimal(&results->line, depth);
    mmgr_line_put(&results->line, "\n    the turn's precision P = ");
    pi_tower_print_decimal(&results->line, precision);
    mmgr_line_put(&results->line, " bits; a host turn would need an exact width of ");
    pi_tower_print_decimal(&results->line, needed);
    mmgr_line_put(&results->line, " bits, SIM_EXACT_LIMBS = 2^");
    mmgr_line_decimal(&results->line, width, 1u);
    mmgr_line_ch(&results->line, '\n');
    sim_flush(results);
    unsigned int window = 0u;
    const PiWide position = pi_tower_depth_position(depth, &window);
    PiTowerBbp bbp;
    const unsigned long long lanes = pi_tower_bbp_lanes();
    const int planned = (lanes != 0ULL) && pi_tower_bbp_plan(&bbp, position, lanes, &error);
    sim_check(results, planned, "engine: keymath encodes and the scheduler arranges the term, tail and pair programs");
    if (planned == 0)
    {
        return;
    }
    if (!sim_job_submit(results, "pi_tower", count, arguments, pi_tower_bbp_bytes(&bbp)))
    {
        pi_tower_bbp_free(&bbp);
        return;
    }
    mmgr_line_put(&results->line,
                  "    the turn at depth n is alpha's bit n: its cell's low bits are the window from hex position ");
    pi_tower_print_decimal(&results->line, position);
    mmgr_line_put(&results->line, ", on the engine\n");
    sim_flush(results);
    g_pi_tower_stopped = 0;
    const PlatformSignalHandler was = platform_signal_handled(SIGINT, pi_tower_stop);
    PiTowerBbpRun run;
    const int ran = pi_tower_bbp_sum(results, &bbp, 1, &run);
    (void)platform_signal_handled(SIGINT, was);
    pi_tower_bbp_progress(results, &bbp, &run);
    const unsigned int certified = ((ran != 0) && (run.finished != 0)) ? pi_tower_bbp_certified(&bbp, run.sum) : 0u;
    pi_tower_bbp_print(results, &bbp, &run, certified);
    if ((run.finished != 0) && (certified >= window))
    {
        mmgr_line_put(&results->line, "    the cell at 2^n, its low 64 bits: 0x");
        mmgr_line_hex(&results->line, pi_tower_bbp_cell(&bbp, run.sum, window), 16u);
        mmgr_line_ch(&results->line, '\n');
    }
    else if (run.finished == 0)
    {
        mmgr_line_put(&results->line, "    stopped: no digit at depth n is printed until every term is summed\n");
    }
    sim_check(results, ran, "engine: every sweep the run made summed on the record machine");
    sim_check(results, (run.finished == 0) || (certified >= window),
              "a finished run certifies the window through depth n");
    pi_tower_bbp_free(&bbp);
    sim_flush(results);
}

// a whole number of decimal digits, from the text up to the first character that is not one; 0 where there are none
static int pi_tower_digits(const char **text, PiWide *value)
{
    const PiWide ten = pi_tower_unsigned(10ULL);
    *value = pi_tower_unsigned(0ULL);
    const char *const digits = *text;
    const size_t length = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = digits, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                     .other = "0123456789");
    // each digit is a step of the value: the value times 10, and the digit added
    for (size_t at = 0u; at < length; at += 1u)
    {
        // one decimal digit, 0 to 9
        *value = pi_tower_sum(pi_tower_product(*value, ten), pi_tower_unsigned((unsigned long long)(digits[at] - '0')));
    }
    *text = &digits[length];
    return length != 0u;
}

// n from the request, whole or as base^exponent. The base and its operation stay apart until they make n. The answer
// is 0 where the text is neither, n is 0, or n outgrows the exact width
int pi_tower_request(const char *text, PiWide *value)
{
    if (text == NULL)
    {
        return 0;
    }
    g_pi_tower_error = 0;
    const char *walk = text;
    PiWide base;
    if (pi_tower_digits(&walk, &base) == 0)
    {
        return 0;
    }
    *value = base;
    if (*walk == '^')
    {
        walk += 1;
        PiWide exponent;
        if ((pi_tower_digits(&walk, &exponent) == 0) ||
            (pi_tower_compare(exponent, pi_tower_unsigned(ORIOR_EXACT_BITS)) > 0))
        {
            return 0;
        }
        // base^exponent by a square for each bit of the exponent, from the top
        *value = pi_tower_unsigned(1ULL);
        for (unsigned int bit = pi_tower_bit_length(exponent); (bit > 0u) && (g_pi_tower_error == 0); bit -= 1u)
        {
            *value = pi_tower_product(*value, *value);
            if (((exponent.limb[(bit - 1u) / 32u] >> ((bit - 1u) % 32u)) & 1u) != 0u)
            {
                *value = pi_tower_product(*value, base);
            }
        }
    }
    return (*walk == '\0') && (value->sign != 0) && (g_pi_tower_error == 0);
}

// a resolution from the request, from 1 to 2^20
int pi_tower_resolution(const char *text, unsigned int *bits)
{
    PiWide value;
    if ((pi_tower_request(text, &value) == 0) || (pi_tower_compare(value, pi_tower_unsigned(PI_TOWER_EXACT_MAX)) > 0))
    {
        return 0;
    }
    // at max 2^20. An unsigned int stores it
    *bits = (unsigned int)pi_tower_word(value);
    return 1;
}
