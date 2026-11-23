// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// record_tower_test_ruleset.cu: matches, rulesets, the operation stack, blocks, errors and main
#include "../../../../../../../src/platform/platform.h"
#include "record_tower_test_internal.h"

// The pool of the test. The code generator writes the PTX lane of a program of three thousand steps into a text of
// megabytes. While that text lasts, its claims take a cell of up to 16 MiB on the temporary tier
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 30u;

// t, z, y and x: a line of 64, a cube of 4, blocks with odd extents on every axis, a block on all four axes, and one
// sample, which no level lifts
static constexpr unsigned long long s_tower_test_extent[TOWER_TEST_BLOCKS][4] = {
    {1ull, 1ull, 1ull, 64ull}, {1ull, 4ull, 4ull, 4ull}, {2ull, 3ull, 4ull, 4ull}, {1ull, 3ull, 5ull, 7ull},
    {3ull, 1ull, 2ull, 9ull},  {2ull, 2ull, 2ull, 2ull}, {1ull, 1ull, 1ull, 1ull}};

// 1 where every block holds at max TOWER_TEST_LANES_MAX samples, and no line is longer than TOWER_TEST_LINE_MAX
static constexpr int tower_test_extents_held(void)
{
    int held = 1;
    for (unsigned int block = 0u; block < TOWER_TEST_BLOCKS; block += 1u)
    {
        const unsigned long long *const extent = s_tower_test_extent[block];
        held = held && ((extent[0] * extent[1] * extent[2] * extent[3]) <= TOWER_TEST_LANES_MAX);
        for (unsigned int axis = 0u; axis < 4u; axis += 1u)
        {
            held = held && (extent[axis] <= TOWER_TEST_LINE_MAX);
        }
    }
    return held;
}

static_assert(tower_test_extents_held() != 0,
              "every block of s_tower_test_extent holds at max TOWER_TEST_LANES_MAX samples, in lines of at max "
              "TOWER_TEST_LINE_MAX");

// the S transform: the highs less their even neighbor, the lows plus half the high beside them, floored
static const TowerLiftingStep s_tower_test_s_transform[2] = {{TOWER_BAND_HIGH, -1, 1u, {0}, {1}, 0u, 0u},
                                                             {TOWER_BAND_LOW, 1, 1u, {0}, {1}, 0u, 1u}};

// a four-tap predict, the highs less floor((9 (x_2j + x_(2j+2)) - (x_(2j-2) + x_(2j+4)) + 8) / 16), then the 5/3's
// update
static const TowerLiftingStep s_tower_test_four_tap[2] = {
    {TOWER_BAND_HIGH, -1, 4u, {-1, 0, 1, 2}, {-1, 9, 9, -1}, 8u, 4u}, {TOWER_BAND_LOW, 1, 2u, {-1, 0}, {1, 1}, 2u, 2u}};

static const TowerTestRuleset s_tower_test_ruleset[TOWER_TEST_RULESETS] = {
    {"the kernels' 5/3, no ruleset named", NULL, 0u, 1},
    {"the 5/3 named as its two steps", s_tower_test_five_three, 2u, 1},
    {"the S transform", s_tower_test_s_transform, 2u, 0},
    {"a four-tap predict and the 5/3's update", s_tower_test_four_tap, 2u, 0}};

#define TOWER_TEST_VALUES (TOWER_TEST_VOLUMES * TOWER_TEST_LANES_MAX)

// The arrays of one program's run on one block. The calls of this module share them, and they run one at a time
struct TowerTestRun
{
    long long sample_values[TOWER_TEST_VALUES];
    long long crystal[TOWER_TEST_VALUES];
    long long drawn[TOWER_TEST_VALUES];
    unsigned int atoms[TOWER_TEST_VALUES];
    unsigned int samples[TOWER_TEST_LANES_MAX];
    unsigned int sample_bits[TOWER_TEST_LANES_MAX];
    unsigned int crystal_bits[TOWER_TEST_LANES_MAX];
    unsigned int registers[TOWER_TEST_LANES_MAX];
    unsigned int middle[TOWER_TEST_LANES_MAX];
    TowerTestSteps steps;
};

// The kernels' arrays of one block: the source, the crystal and the rebuilt samples of every volume. Also the width of
// each coefficient's field for F . T^-1. The calls of this module share them
struct TowerTestBlock
{
    unsigned short source[TOWER_TEST_VALUES];
    int crystal[TOWER_TEST_VALUES];
    unsigned short rebuilt[TOWER_TEST_VALUES];
    unsigned int crystal_bits[TOWER_TEST_LANES_MAX];
};

static TowerTestRun s_tower_test_run;
static TowerTestBlock s_tower_test_block;

// The run's samples set for `lanes` positions: each sample is its position, and each field is TOWER_TEST_SAMPLE_BITS
// or 32 bits wide
static void tower_test_run_begun(TowerTestRun *run, unsigned int lanes)
{
    for (unsigned int position = 0u; position < lanes; position += 1u)
    {
        run->samples[position] = position;
        run->sample_bits[position] = TOWER_TEST_SAMPLE_BITS;
        run->crystal_bits[position] = 32u;
    }
}

// how many volumes' records in `out` hold, at the `lanes` registers named, the values expected of them
static unsigned int tower_test_matches(const TowerTestLoaded *loaded, const unsigned int *out,
                                       const unsigned int *registers, unsigned int lanes, const long long *expected,
                                       unsigned int volumes)
{
    unsigned int matched = 0u;
    for (unsigned int volume = 0u; volume < volumes; volume += 1u)
    {
        const unsigned int *const record = &out[(size_t)volume * loaded->layout.out_limbs];
        int equal = 1;
        for (unsigned int position = 0u; position < lanes; position += 1u)
        {
            equal = equal && (tower_test_take(record, &loaded->layout.step_table[registers[position]]) ==
                              expected[((size_t)volume * lanes) + position]);
        }
        matched += (equal != 0) ? 1u : 0u;
    }
    return matched;
}

// A loaded program run over the run's atoms, and the volumes whose records hold `expected` at `registers`. The
// records are ephemeral: the call takes them on the temporary tier and gives them back before it returns. `ran` is 1
// where the device's records equal the host's
static unsigned int tower_test_checked(TowerTestLoaded *loaded, const TowerTestRun *run, const unsigned int *registers,
                                       unsigned int lanes, const long long *expected, int *ran)
{
    const unsigned int volumes = TOWER_TEST_VOLUMES;
    unsigned int *const out = tower_test_words((unsigned long long)volumes * loaded->layout.out_limbs);
    *ran = (out != NULL) && tower_test_run(loaded, run->atoms, volumes, out);
    const unsigned int matched =
        (*ran != 0) ? tower_test_matches(loaded, out, registers, lanes, expected, volumes) : 0u;
    platform_cell_released(out, platform_place_g_program_owner);
    return matched;
}

// One ruleset on one block: T against the kernels or the oracle, and T^-1 on that crystal against the source. Then T
// read through T^-1 on drawn crystals. `source` and `kernel_crystal` hold the volumes' samples and the kernels'
// crystal, `lanes` a volume
static void tower_test_ruleset(SimResults *results, const unsigned long long extent[4], const TowerTestRuleset *ruleset,
                               const unsigned short *source, const int *kernel_crystal, unsigned int lanes)
{
    VerbaLinea *const line = &results->line;
    TowerTestRun *const run = &s_tower_test_run;
    const unsigned int volumes = TOWER_TEST_VOLUMES;
    const unsigned long long values = (unsigned long long)volumes * lanes;
    const TowerLiftingStep *const rules = (ruleset->rules != NULL) ? ruleset->rules : s_tower_test_five_three;
    const unsigned int rule_count = (ruleset->rules != NULL) ? ruleset->rule_count : 2u;
    tower_test_run_begun(run, lanes);
    for (unsigned long long at = 0ull; at < values; at += 1ull)
    {
        run->sample_values[at] = (long long)source[at];
        run->crystal[at] = (ruleset->kernels != 0) ? (long long)kernel_crystal[at] : (long long)source[at];
        run->atoms[at] = source[at];
    }
    for (unsigned int volume = 0u; (ruleset->kernels == 0) && (volume < volumes); volume += 1u)
    {
        tower_test_oracle(extent, rules, rule_count, &run->crystal[(size_t)volume * lanes], 0);
    }

    // T: a field for each sample, 16 bits unsigned, then the lift
    TowerTestLoaded lift = {};
    int ok = tower_test_fields(lanes, ENGINE_RECORD_FIELD, &run->steps) &&
             tower_test_build(extent, ruleset, 0, run->samples, lanes, run->registers, &run->steps) &&
             tower_test_load(&run->steps, run->registers, lanes, run->sample_bits, lanes, &lift);
    int ran = 0;
    const unsigned int lift_equal =
        (ok != 0) ? tower_test_checked(&lift, run, run->registers, lanes, run->crystal, &ran) : 0u;
    sim_check(results, ran,
              "T is emitted as sized, encodes, arranges and loads, and the device's records equal the host's");
    sim_check(results, lift_equal == volumes,
              (ruleset->kernels != 0) ? "T's record floors equal tower_lift's crystal at every coefficient"
                                      : "T's record floors equal the oracle's crystal at every coefficient");
    // each coefficient's field for T^-1: its register's width and a sign bit
    unsigned int widest = 0u;
    for (unsigned int position = 0u; (ok != 0) && (position < lanes); position += 1u)
    {
        run->crystal_bits[position] = lift.key.term[run->registers[position]].bits + 1u;
        widest = ((run->crystal_bits[position] - 1u) > widest) ? (run->crystal_bits[position] - 1u) : widest;
    }
    const unsigned int lift_limbs = lift.layout.file_limbs;
    const unsigned int lift_count = run->steps.count - lanes;
    tower_test_free(&lift);

    // T^-1: a signed field for each coefficient, then the lower, on T's crystal
    TowerTestLoaded lower = {};
    ok = tower_test_fields(lanes, ENGINE_RECORD_FIELD_SIGNED, &run->steps) &&
         tower_test_build(extent, ruleset, 1, run->samples, lanes, run->registers, &run->steps) &&
         tower_test_load(&run->steps, run->registers, lanes, run->crystal_bits, lanes, &lower);
    for (unsigned long long at = 0ull; at < values; at += 1ull)
    {
        // a coefficient's two's complement word, whose low field bits the signed field reads
        run->atoms[at] = (unsigned int)run->crystal[at];
    }
    ran = 0;
    const unsigned int lower_equal =
        (ok != 0) ? tower_test_checked(&lower, run, run->registers, lanes, run->sample_values, &ran) : 0u;
    sim_check(results, ran,
              "T^-1 is emitted as sized, encodes, arranges and loads, and the device's records equal the host's");
    sim_check(results, lower_equal == volumes, "T^-1's record floors return the source at every sample");
    const unsigned int lower_limbs = lower.layout.file_limbs;
    const unsigned int lower_count = run->steps.count - lanes;
    tower_test_free(&lower);

    // T read through T^-1 as one stack, on crystals drawn anywhere in the fields' widths
    TowerTestLoaded round = {};
    ok = tower_test_fields(lanes, ENGINE_RECORD_FIELD_SIGNED, &run->steps) &&
         tower_test_build(extent, ruleset, 1, run->samples, lanes, run->middle, &run->steps) &&
         tower_test_build(extent, ruleset, 0, run->middle, lanes, run->registers, &run->steps) &&
         tower_test_load(&run->steps, run->registers, lanes, run->crystal_bits, lanes, &round);
    for (unsigned long long at = 0ull; at < values; at += 1ull)
    {
        run->atoms[at] = tower_test_random();
        run->drawn[at] = tower_test_signed(run->atoms[at], run->crystal_bits[at % lanes]);
    }
    ran = 0;
    const unsigned int round_equal =
        (ok != 0) ? tower_test_checked(&round, run, run->registers, lanes, run->drawn, &ran) : 0u;
    sim_check(results, ran,
              "T . T^-1 encodes, arranges and loads as one program, and the device's records equal the host's");
    sim_check(results, round_equal == volumes, "T . T^-1 returns every drawn crystal exactly, as one stack");
    const unsigned int round_limbs = round.layout.file_limbs;
    tower_test_free(&round);

    mmgr_line_put(line, "    ");
    mmgr_line_put(line, ruleset->name);
    mmgr_line_put(line, ": T ");
    mmgr_line_decimal(line, lift_count, 1u);
    mmgr_line_put(line, " steps in a file of ");
    mmgr_line_decimal(line, lift_limbs, 1u);
    mmgr_line_put(line, " limbs, the widest coefficient ");
    mmgr_line_decimal(line, widest, 1u);
    mmgr_line_put(line, " bits; T^-1 ");
    mmgr_line_decimal(line, lower_count, 1u);
    mmgr_line_put(line, " steps, ");
    mmgr_line_decimal(line, lower_limbs, 1u);
    mmgr_line_put(line, " limbs; T . T^-1 ");
    mmgr_line_decimal(line, round_limbs, 1u);
    mmgr_line_put(line, " limbs\n      of ");
    mmgr_line_decimal(line, volumes, 1u);
    mmgr_line_put(line,
                  (ruleset->kernels != 0) ? " volumes, T equals tower_lift on " : " volumes, T equals the oracle on ");
    mmgr_line_decimal(line, lift_equal, 1u);
    mmgr_line_put(line, ", T^-1 returns the source on ");
    mmgr_line_decimal(line, lower_equal, 1u);
    mmgr_line_put(line, ", T . T^-1 returns ");
    mmgr_line_decimal(line, round_equal, 1u);
    mmgr_line_put(line, " drawn crystals\n");
    sim_flush(results);
}

// a step appended to `steps`, and its place there into `place`: 1, or 0 where the steps are full
static int tower_test_step(TowerTestSteps *steps, EngineRecordOperation operation, unsigned int left,
                           unsigned int right, unsigned int *place)
{
    if (steps->count >= TOWER_TEST_STEPS_MAX)
    {
        return 0;
    }
    const EngineRecordStep step = {operation, left, right, 0u};
    *place = steps->count;
    steps->step[steps->count] = step;
    steps->count += 1u;
    return 1;
}

// F read through T^-1 as one stack, on the kernels' crystal: the block's sum, and its energy along x where x has
// neighbors, against F on tower_lower's samples. Each of the three holds `lanes` positions of each volume
static void tower_test_operation_stack(SimResults *results, const unsigned long long extent[4],
                                       const int *kernel_crystal, const unsigned short *rebuilt,
                                       const unsigned int *crystal_bits, unsigned int lanes)
{
    TowerTestRun *const run = &s_tower_test_run;
    const unsigned int volumes = TOWER_TEST_VOLUMES;
    tower_test_run_begun(run, lanes);
    int ok = tower_test_fields(lanes, ENGINE_RECORD_FIELD_SIGNED, &run->steps) &&
             tower_test_build(extent, &s_tower_test_ruleset[0], 1, run->samples, lanes, run->registers, &run->steps);
    const unsigned int *const read_back = run->registers;
    unsigned int sum = read_back[0];
    unsigned int energy = 0u;
    int energy_ok = 0;
    for (unsigned int position = 0u; (ok != 0) && (position < lanes); position += 1u)
    {
        if (position > 0u)
        {
            ok = tower_test_step(&run->steps, ENGINE_RECORD_SUM, sum, read_back[position], &sum);
        }
        if ((ok != 0) && ((position % extent[3]) < (extent[3] - 1ull)))
        {
            unsigned int difference = 0u;
            unsigned int square = 0u;
            ok = tower_test_step(&run->steps, ENGINE_RECORD_DIFFERENCE, read_back[position + 1u], read_back[position],
                                 &difference) &&
                 tower_test_step(&run->steps, ENGINE_RECORD_PRODUCT, difference, difference, &square);
            if ((ok != 0) && (energy_ok != 0))
            {
                ok = tower_test_step(&run->steps, ENGINE_RECORD_SUM, energy, square, &energy);
            }
            else
            {
                energy = square;
                energy_ok = 1;
            }
        }
    }
    const unsigned int outputs[2] = {sum, energy};
    TowerTestLoaded operation = {};
    ok = ok && tower_test_load(&run->steps, outputs, (energy_ok != 0) ? 2u : 1u, crystal_bits, lanes, &operation);
    const unsigned long long values = (unsigned long long)volumes * lanes;
    for (unsigned long long at = 0ull; at < values; at += 1ull)
    {
        // a coefficient's two's complement word, whose low field bits the signed field reads
        run->atoms[at] = (unsigned int)kernel_crystal[at];
    }
    // the records are ephemeral: the call takes them on the temporary tier and gives them back before it returns
    unsigned int *const out =
        (ok != 0) ? tower_test_words((unsigned long long)volumes * operation.layout.out_limbs) : NULL;
    ok = ok && (out != NULL) && tower_test_run(&operation, run->atoms, volumes, out);
    sim_check(results, ok,
              "F . T^-1 encodes, arranges and loads as one program, and the device's records equal the host's");
    unsigned int matched = 0u;
    for (unsigned int volume = 0u; (ok != 0) && (volume < volumes); volume += 1u)
    {
        const unsigned int *const record = &out[(size_t)volume * operation.layout.out_limbs];
        long long host_sum = 0ll;
        long long host_energy = 0ll;
        tower_test_operation(extent, &rebuilt[(size_t)volume * lanes], lanes, &host_sum, &host_energy);
        int equal = tower_test_take(record, &operation.layout.step_table[sum]) == host_sum;
        if (energy_ok != 0)
        {
            equal = equal && (tower_test_take(record, &operation.layout.step_table[energy]) == host_energy);
        }
        matched += (equal != 0) ? 1u : 0u;
    }
    platform_cell_released(out, platform_place_g_program_owner);
    sim_check(results, matched == volumes,
              "F . T^-1 equals F taken on tower_lower's samples: the block's sum and its energy along x");
    mmgr_line_put(&results->line, "    F . T^-1, the block's sum and its energy along x: ");
    mmgr_line_decimal(&results->line, run->steps.count, 1u);
    mmgr_line_put(&results->line, " steps in one program, a file of ");
    mmgr_line_decimal(&results->line, operation.layout.file_limbs, 1u);
    mmgr_line_put(&results->line, " limbs; equal to F on tower_lower's samples on ");
    mmgr_line_decimal(&results->line, matched, 1u);
    mmgr_line_put(&results->line, " of ");
    mmgr_line_decimal(&results->line, volumes, 1u);
    mmgr_line_put(&results->line, " volumes\n");
    tower_test_free(&operation);
}

// the oracle's 5/3 read against the kernels on every volume: T equal to tower_lift's crystal, and T^-1 on it the
// source. The answer is the count of volumes where both hold
static unsigned int tower_test_oracle_equal(const unsigned long long extent[4], const unsigned short *source,
                                            const int *crystal, unsigned int lanes, unsigned int volumes)
{
    long long block[TOWER_TEST_LANES_MAX];
    unsigned int oracle_equal = 0u;
    for (unsigned int volume = 0u; volume < volumes; volume += 1u)
    {
        const size_t first = (size_t)volume * lanes;
        for (unsigned int position = 0u; position < lanes; position += 1u)
        {
            block[position] = (long long)source[first + position];
        }
        tower_test_oracle(extent, s_tower_test_five_three, 2u, block, 0);
        int equal = 1;
        for (unsigned int position = 0u; position < lanes; position += 1u)
        {
            equal = equal && (block[position] == (long long)crystal[first + position]);
        }
        tower_test_oracle(extent, s_tower_test_five_three, 2u, block, 1);
        for (unsigned int position = 0u; position < lanes; position += 1u)
        {
            equal = equal && (block[position] == (long long)source[first + position]);
        }
        oracle_equal += (equal != 0) ? 1u : 0u;
    }
    return oracle_equal;
}

// The 5/3's T emitted for the block, and the width of each coefficient's field for T^-1 into `crystal_bits`: its
// register's width and a sign bit. 1, or 0 where the program did not build or load
static int tower_test_widths(const unsigned long long extent[4], unsigned int lanes, unsigned int *crystal_bits)
{
    TowerTestRun *const run = &s_tower_test_run;
    tower_test_run_begun(run, lanes);
    TowerTestLoaded lift = {};
    const int ok =
        tower_test_fields(lanes, ENGINE_RECORD_FIELD, &run->steps) &&
        tower_test_build(extent, &s_tower_test_ruleset[0], 0, run->samples, lanes, run->registers, &run->steps) &&
        tower_test_load(&run->steps, run->registers, lanes, run->sample_bits, lanes, &lift);
    for (unsigned int position = 0u; (ok != 0) && (position < lanes); position += 1u)
    {
        crystal_bits[position] = lift.key.term[run->registers[position]].bits + 1u;
    }
    if (ok != 0)
    {
        tower_test_free(&lift);
    }
    return ok;
}

static void tower_test_block(SimResults *results, const unsigned long long extent[4], unsigned short *device_volume)
{
    VerbaLinea *const line = &results->line;
    TowerTestBlock *const block = &s_tower_test_block;
    // every block here holds at max TOWER_TEST_LANES_MAX samples
    const unsigned int lanes = (unsigned int)(extent[0] * extent[1] * extent[2] * extent[3]);
    const unsigned int volumes = TOWER_TEST_VOLUMES;
    mmgr_line_put(line, "  t x z x y x x = ");
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        mmgr_line_decimal(line, extent[axis], 1u);
        mmgr_line_put(line, (axis < 3u) ? " x " : "\n");
    }

    // the kernels' crystal and rebuilt samples of every volume
    for (unsigned int volume = 0u; volume < volumes; volume += 1u)
    {
        for (unsigned int position = 0u; position < lanes; position += 1u)
        {
            block->source[((size_t)volume * lanes) + position] = tower_test_sample(volume, position);
        }
    }
    int kernels = 1;
    for (unsigned int volume = 0u; (kernels != 0) && (volume < volumes); volume += 1u)
    {
        const size_t at = (size_t)volume * lanes;
        kernels = tower_test_kernels(extent, &block->source[at], device_volume, &block->crystal[at],
                                     &block->rebuilt[at], lanes);
    }
    sim_check(results, kernels, "tower_lift and tower_lower run every volume, and tower_lower rebuilds the source");

    // the oracle is read against the kernels once, on the 5/3, before it grades the rulesets they do not run
    const unsigned int oracle_equal =
        (kernels != 0) ? tower_test_oracle_equal(extent, block->source, block->crystal, lanes, volumes) : 0u;
    sim_check(results, oracle_equal == volumes, "the oracle's 5/3 equals tower_lift's crystal and returns the source");
    mmgr_line_put(line, "    the oracle's 5/3 equals tower_lift and returns the source on ");
    mmgr_line_decimal(line, oracle_equal, 1u);
    mmgr_line_put(line, " of ");
    mmgr_line_decimal(line, volumes, 1u);
    mmgr_line_put(line, " volumes\n");

    for (unsigned int ruleset = 0u; (kernels != 0) && (ruleset < TOWER_TEST_RULESETS); ruleset += 1u)
    {
        tower_test_ruleset(results, extent, &s_tower_test_ruleset[ruleset], block->source, block->crystal, lanes);
    }

    // F . T^-1 reads the kernels' crystal through fields as wide as the 5/3's T makes each coefficient
    const int widths = (kernels != 0) && tower_test_widths(extent, lanes, block->crystal_bits);
    if (widths != 0)
    {
        tower_test_operation_stack(results, extent, block->crystal, block->rebuilt, block->crystal_bits, lanes);
    }
    sim_check(results, widths, "the 5/3's widths are read for F . T^-1");
    sim_flush(results);
}

// the emitter's errors: each leaves the count, and any step already in the capacity, as it was
static void tower_test_errors(SimResults *results)
{
    const unsigned long long pair[4] = {1ull, 1ull, 1ull, 2ull};
    const unsigned long long empty[4] = {1ull, 0ull, 1ull, 2ull};
    const unsigned int fields[2] = {0u, 1u};
    unsigned int out[2] = {0u, 0u};
    EngineRecordStep steps_out[3];
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        steps_out[at] = EngineRecordStep{ENGINE_RECORD_CONSTANT, 7u, 0u, 0u};
    }
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    unsigned int count = 1u;
    TowerRecordRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.extent, .src = pair, .bytes = sizeof(request.extent));
    request.in_registers = fields;
    request.out_registers = out;
    request.count = &count;
    request.error = &error;
    // register 1 is not earlier than the count of 1
    const int later =
        (tower_record_lift(&request) == TOWER_ERROR) && (count == 1u) && (error.type == ENGINE_ERROR_REQUEST);
    sim_check(results, later, "a register that is not an earlier step errors, and the count is left as it was");
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    count = 2u;
    request.steps = steps_out;
    request.step_capacity = 3u;
    int unchanged = 1;
    const int small =
        (tower_record_lower(&request) == TOWER_ERROR) && (count == 2u) && (error.type == ENGINE_ERROR_REQUEST);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        unchanged = unchanged && (steps_out[at].operation == ENGINE_RECORD_CONSTANT) && (steps_out[at].left == 7u);
    }
    sim_check(results, small && unchanged, "a capacity too small errors before a step is written");
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.extent, .src = empty, .bytes = sizeof(request.extent));
    request.steps = NULL;
    const int nothing =
        (tower_record_lift(&request) == TOWER_ERROR) && (count == 2u) && (error.type == ENGINE_ERROR_REQUEST);
    sim_check(results, nothing, "an extent with no samples errors");
    // a step with a zero weight, then one with no taps, then one whose sign is neither 1 nor -1
    TowerLiftingStep malformed[3] = {{TOWER_BAND_HIGH, -1, 1u, {0}, {0}, 0u, 0u},
                                     {TOWER_BAND_HIGH, -1, 0u, {0}, {1}, 0u, 0u},
                                     {TOWER_BAND_LOW, 2, 1u, {0}, {1}, 0u, 0u}};
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.extent, .src = pair, .bytes = sizeof(request.extent));
    int errored = 1;
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
        request.rules = &malformed[at];
        request.rule_count = 1u;
        errored = errored && (tower_record_lift(&request) == TOWER_ERROR) && (count == 2u) &&
                  (error.type == ENGINE_ERROR_REQUEST);
    }
    sim_check(results, errored, "a step with a zero weight, no taps, or a sign other than 1 or -1 errors");
}

int program_main(int count, char **arguments)
{
    char line_buffer[SIM_LINE_CAPACITY];
    SimResults results;
    sim_open(&results, line_buffer);
    mmgr_line_put(&results.line, "  the two towers as record floors focused from a ruleset, read against tower_lift and"
                                 " tower_lower's kernels and a host oracle\n");
    int ok = sim_job_submit(&results, "record_tower_test", count, arguments, TOWER_TEST_DECLARED);
    unsigned long long max = 1ull;
    for (unsigned int block = 0u; block < TOWER_TEST_BLOCKS; block += 1u)
    {
        const unsigned long long *const extent = s_tower_test_extent[block];
        const unsigned long long lanes = extent[0] * extent[1] * extent[2] * extent[3];
        max = (lanes > max) ? lanes : max;
    }
    unsigned short *device_volume = NULL;
    ok = ok && sim_status_check(&results, cudaMalloc((void **)&device_volume, (size_t)max * sizeof(unsigned short)),
                                "the kernels' volume");
    // a sweep whose frame passes the stack limit grows it, and the local memory that reserves is reserved until the
    // limit is set back. Cycle sets it back once each sweep ends. The limit after every block is the limit before them
    size_t stack_before = 0u;
    ok = ok &&
         sim_status_check(&results, cudaDeviceGetLimit(&stack_before, cudaLimitStackSize), "the stack limit, before");
    for (unsigned int block = 0u; (ok != 0) && (block < TOWER_TEST_BLOCKS); block += 1u)
    {
        tower_test_block(&results, s_tower_test_extent[block], device_volume);
    }
    size_t stack_after = 0u;
    ok = ok &&
         sim_status_check(&results, cudaDeviceGetLimit(&stack_after, cudaLimitStackSize), "the stack limit, after");
    sim_check(&results, (ok != 0) && (stack_after == stack_before),
              "every sweep gives its stack back: the limit after the blocks is the limit before them");
    mmgr_line_put(&results.line, "  the stack limit: ");
    mmgr_line_decimal(&results.line, stack_before, 1u);
    mmgr_line_put(&results.line, " bytes a thread before the blocks, ");
    mmgr_line_decimal(&results.line, stack_after, 1u);
    mmgr_line_put(&results.line, " after them\n");
    sim_flush(&results);
    tower_test_errors(&results);
    cudaFree(device_volume);
    return sim_close(&results, "record tower test");
}
