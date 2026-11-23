// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// goodstein_main.cu: bumps, printing, the lemma and main
#include "goodstein_internal.h"

// the numeric step's bump: value in hereditary base b with every b made b + 1, 0 where it leaves 64 bits
static int goodstein_bump(unsigned long long value, unsigned long long base, unsigned long long *out)
{
    unsigned long long total = 0ULL;
    unsigned long long position = 0ULL;
    unsigned long long rest = value;
    while (rest != 0ULL)
    {
        const unsigned long long digit = rest % base;
        if (digit != 0ULL)
        {
            unsigned long long exponent = 0ULL;
            if ((goodstein_bump(position, base, &exponent) == 0) ||
                (goodstein_add_term(&total, digit, base + 1ULL, exponent) == 0))
            {
                return 0;
            }
        }
        rest /= base;
        position += 1ULL;
    }
    *out = total;
    return 1;
}

static void goodstein_print_term(VerbaLinea *line, GoodsteinForm exponent, unsigned long long coefficient,
                                 unsigned int *capacity);

// the term of k written in the base `origin` printed with `coefficient`, its tree on the stack while it prints
static void goodstein_print_tree(VerbaLinea *line, unsigned long long k, unsigned long long origin,
                                 unsigned long long coefficient, unsigned int *capacity)
{
    GoodsteinNotation *const tree = goodstein_taken();
    goodstein_of(k, origin, tree);
    goodstein_print_term(line, goodstein_form(tree), coefficient, capacity);
    goodstein_given();
}

// the form as an ordinal in Cantor normal form, w for omega, cut at the capacity left
static void goodstein_print(VerbaLinea *line, GoodsteinForm form, unsigned int *capacity)
{
    if (form.count == 0u)
    {
        mmgr_line_ch(line, '0');
        return;
    }
    for (unsigned int at = 0u; at < form.count; at += 1u + form.blocks[at].exponent_blocks)
    {
        if (*capacity == 0u)
        {
            mmgr_line_put(line, " ...");
            return;
        }
        if (at != 0u)
        {
            mmgr_line_put(line, " + ");
        }
        const GoodsteinBlock *const block = &form.blocks[at];
        if (block->origin == 0ULL)
        {
            goodstein_print_term(line, goodstein_exponent(form, at), block->coefficient, capacity);
            continue;
        }
        mmgr_line_ch(line, '[');
        goodstein_print_tree(line, block->high, block->origin, block->coefficient, capacity);
        if (block->high != block->low)
        {
            mmgr_line_put(line, " + ... + ");
            goodstein_print_tree(line, block->low, block->origin, block->coefficient, capacity);
        }
        mmgr_line_put(line, "; ");
        mmgr_line_decimal(line, block->high - block->low + 1ULL, 1u);
        mmgr_line_put(line, " terms]");
    }
}

static void goodstein_print_term(VerbaLinea *line, GoodsteinForm exponent, unsigned long long coefficient,
                                 unsigned int *capacity)
{
    *capacity = (*capacity > 8u) ? (*capacity - 8u) : 0u;
    if (exponent.count == 0u)
    {
        mmgr_line_decimal(line, coefficient, 1u);
        return;
    }
    // an exponent of one term with no exponent of its own is a finite power
    const int power_finite = (exponent.count == 1u) && (exponent.blocks[0].origin == 0ULL);
    const int power_one = power_finite && (exponent.blocks[0].coefficient == 1ULL);
    mmgr_line_ch(line, 'w');
    if (power_finite != 0)
    {
        if (power_one == 0)
        {
            mmgr_line_ch(line, '^');
            mmgr_line_decimal(line, exponent.blocks[0].coefficient, 1u);
        }
    }
    else
    {
        mmgr_line_put(line, "^(");
        goodstein_print(line, exponent, capacity);
        mmgr_line_ch(line, ')');
    }
    if (coefficient != 1ULL)
    {
        mmgr_line_ch(line, '.');
        mmgr_line_decimal(line, coefficient, 1u);
    }
}

// The forms that main holds over its run: the form, the form before a step, a tower and the tower below it. Then the
// two trees that a comparison reads
static GoodsteinNotation s_form;
static GoodsteinNotation s_before;
static GoodsteinNotation s_tower;
static GoodsteinNotation s_below;
static GoodsteinNotation s_left;
static GoodsteinNotation s_right;

// 1. the bump preserves the tree
static void goodstein_lemma(SimResults *results)
{
    unsigned long long tested = 0ULL;
    unsigned long long terminated = 0ULL;
    for (unsigned long long base = 2ULL; base <= GOODSTEIN_LEMMA_BASE_MAX; base += 1ULL)
    {
        for (unsigned long long value = 0ULL; value < GOODSTEIN_LEMMA_BELOW; value += 1ULL)
        {
            unsigned long long bumped = 0ULL;
            if (goodstein_bump(value, base, &bumped) == 0)
            {
                continue;
            }
            GoodsteinBudget budget = {GOODSTEIN_COMPARE_TERMS, 0};
            goodstein_of(bumped, base + 1ULL, &s_left);
            goodstein_of(value, base, &s_right);
            const int order = goodstein_compare(goodstein_form(&s_left), goodstein_form(&s_right), &budget);
            tested += 1ULL;
            terminated += ((order == 0) && (budget.exhausted == 0)) ? 1ULL : 0ULL;
        }
    }
    mmgr_line_put(&results->line, "  the bump preserves the tree: ");
    mmgr_line_decimal(&results->line, terminated, 1u);
    mmgr_line_put(&results->line, " of ");
    mmgr_line_decimal(&results->line, tested, 1u);
    mmgr_line_put(&results->line, " values below ");
    mmgr_line_decimal(&results->line, GOODSTEIN_LEMMA_BELOW, 1u);
    mmgr_line_put(&results->line, " at bases 2 to 6 whose bump fits 64 bits\n");
    sim_check(results, (tested != 0ULL) && (terminated == tested),
              "every n bumped to base b + 1 has the hereditary tree n had in base b");
}

// 2. 2^^k is omega^^k
static void goodstein_towers(SimResults *results)
{
    unsigned long long value = 1ULL;
    int same = 1;
    int climbs = 1;
    s_below.count = 0u;
    for (unsigned int height = 1u; height <= GOODSTEIN_TOWER; height += 1u)
    {
        // 2^^height from 2^^(height - 1): 1, 2, 4, 16, 65536
        unsigned long long next = 0ULL;
        same = same && (goodstein_power(2ULL, value, &next) != 0);
        value = next;
        goodstein_tower(height, &s_tower);
        goodstein_of(value, 2ULL, &s_left);
        GoodsteinBudget budget = {GOODSTEIN_COMPARE_TERMS, 0};
        same = same && (goodstein_compare(goodstein_form(&s_left), goodstein_form(&s_tower), &budget) == 0) &&
               (budget.exhausted == 0);
        if (height > 1u)
        {
            GoodsteinBudget climb = {GOODSTEIN_COMPARE_TERMS, 0};
            climbs = climbs && (goodstein_compare(goodstein_form(&s_below), goodstein_form(&s_tower), &climb) < 0) &&
                     (climb.exhausted == 0);
        }
        goodstein_copied(&s_below, &s_tower);
        unsigned int capacity = GOODSTEIN_PRINT_CAPACITY;
        mmgr_line_put(&results->line, "  2^^");
        mmgr_line_decimal(&results->line, height, 1u);
        mmgr_line_put(&results->line, " = ");
        mmgr_line_decimal(&results->line, value, 1u);
        mmgr_line_put(&results->line, " in hereditary base 2 is ");
        goodstein_print(&results->line, goodstein_form(&s_left), &capacity);
        mmgr_line_ch(&results->line, '\n');
    }
    sim_check(results, same, "2^^k in hereditary base 2 is omega^^k, for k = 1 to 4");
    sim_check(results, climbs, "omega < omega^omega < omega^omega^omega < omega^omega^omega^omega");
}

// 3. the starts that finish
static void goodstein_small(SimResults *results)
{
    const unsigned long long expected[3] = {1ULL, 3ULL, 5ULL};
    int finish = 1;
    int values = 1;
    for (unsigned long long start = 1ULL; start <= 3ULL; start += 1ULL)
    {
        goodstein_of(start, 2ULL, &s_form);
        unsigned long long numeric = start;
        unsigned long long base = 2ULL;
        unsigned long long steps = 0ULL;
        while ((s_form.count != 0u) && (steps < GOODSTEIN_SMALL_STEPS_MAX))
        {
            unsigned long long bumped = 0ULL;
            values = values && (goodstein_bump(numeric, base, &bumped) != 0);
            numeric = bumped - 1ULL;
            base += 1ULL;
            values = values && (goodstein_decrement(&s_form, base) != 0);
            unsigned long long value = 0ULL;
            values = values && (goodstein_value(goodstein_form(&s_form), base, &value) != 0) && (value == numeric);
            steps += 1ULL;
        }
        finish = finish && (s_form.count == 0u) && (numeric == 0ULL) && (steps == expected[start - 1ULL]);
        mmgr_line_put(&results->line, "  start ");
        mmgr_line_decimal(&results->line, start, 1u);
        mmgr_line_put(&results->line, ": 0 after ");
        mmgr_line_decimal(&results->line, steps, 1u);
        mmgr_line_put(&results->line, " steps\n");
    }
    sim_check(results, finish, "the starts 1, 2 and 3 reach 0 in 1, 3 and 5 steps");
    sim_check(results, values, "every step of the starts 1, 2 and 3 equals the numeric sequence");
}

// 4. a tower start, run for GOODSTEIN_STEPS steps
static void goodstein_run(SimResults *results, unsigned long long start, int *falls, int *agrees,
                          unsigned long long *compared)
{
    goodstein_of(start, 2ULL, &s_form);
    unsigned int capacity = GOODSTEIN_PRINT_CAPACITY;
    mmgr_line_put(&results->line, "  start ");
    mmgr_line_decimal(&results->line, start, 1u);
    mmgr_line_put(&results->line, ", the ordinal ");
    goodstein_print(&results->line, goodstein_form(&s_form), &capacity);
    mmgr_line_ch(&results->line, '\n');
    unsigned long long numeric = start;
    int numeric_fits = 1;
    unsigned long long base = 2ULL;
    unsigned long long steps = 0ULL;
    unsigned long long fell = 0ULL;
    unsigned long long matched = 0ULL;
    int ok = 1;
    while ((s_form.count != 0u) && (steps < GOODSTEIN_STEPS) && (ok != 0))
    {
        goodstein_copied(&s_before, &s_form);
        if (numeric_fits != 0)
        {
            unsigned long long bumped = 0ULL;
            numeric_fits = goodstein_bump(numeric, base, &bumped);
            numeric = bumped - 1ULL;
        }
        base += 1ULL;
        ok = goodstein_decrement(&s_form, base);
        GoodsteinBudget budget = {GOODSTEIN_COMPARE_TERMS, 0};
        fell += ((ok != 0) && (goodstein_compare(goodstein_form(&s_form), goodstein_form(&s_before), &budget) < 0) &&
                 (budget.exhausted == 0))
                    ? 1ULL
                    : 0ULL;
        unsigned long long value = 0ULL;
        const int value_fits = goodstein_value(goodstein_form(&s_form), base, &value);
        if (numeric_fits != value_fits)
        {
            *agrees = 0;
        }
        else if (numeric_fits != 0)
        {
            matched += 1ULL;
            *agrees = *agrees && (value == numeric);
        }
        steps += 1ULL;
    }
    *falls = *falls && (ok != 0) && (fell == steps) && (steps == GOODSTEIN_STEPS);
    *compared += matched;
    capacity = GOODSTEIN_PRINT_CAPACITY;
    mmgr_line_put(&results->line, "    after ");
    mmgr_line_decimal(&results->line, steps, 1u);
    mmgr_line_put(&results->line, " steps, base ");
    mmgr_line_decimal(&results->line, base, 1u);
    mmgr_line_put(&results->line, ", ");
    mmgr_line_decimal(&results->line, goodstein_terms(goodstein_form(&s_form)), 1u);
    mmgr_line_put(&results->line, " blocks; the ordinal fell on ");
    mmgr_line_decimal(&results->line, fell, 1u);
    mmgr_line_put(&results->line, "; the value equals the numeric sequence's on ");
    mmgr_line_decimal(&results->line, matched, 1u);
    mmgr_line_put(&results->line, " steps, where it fits 64 bits\n    now ");
    goodstein_print(&results->line, goodstein_form(&s_form), &capacity);
    mmgr_line_ch(&results->line, '\n');
    sim_flush(results);
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    goodstein_lemma(&results);
    goodstein_towers(&results);
    goodstein_small(&results);
    sim_flush(&results);
    const unsigned long long starts[3] = {4ULL, 16ULL, 65536ULL};
    int falls = 1;
    int agrees = 1;
    unsigned long long compared[3] = {0ULL, 0ULL, 0ULL};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        goodstein_run(&results, starts[at], &falls, &agrees, &compared[at]);
    }
    sim_check(&results, falls,
              "the starts 4, 16 and 65536 run every step, and each step's ordinal falls below the last");
    sim_check(&results, agrees && (compared[0] != 0ULL) && (compared[1] != 0ULL),
              "every value equals the numeric sequence's wherever that fits 64 bits, and fits exactly then");
    return sim_close(&results, "goodstein");
}
