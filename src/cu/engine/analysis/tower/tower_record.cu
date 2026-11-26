// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tower_record.cu: the tower as record floors
#include "memoria_operor/memoria_operor.h"
#include "tower_internal.h"

// the ruleset tower_forward_kernel and tower_inverse_kernel run: the highs less floor((x_2j + x_(2j+2)) / 2), then the
// lows plus floor((d_(j-1) + d_j + 2) / 4)
static const TowerLiftingStep s_tower_five_three[2] = {{TOWER_BAND_HIGH, -1, 2u, {0, 1}, {1, 1}, 0u, 1u},
                                                       {TOWER_BAND_LOW, 1, 2u, {-1, 0}, {1, 1}, 2u, 2u}};

static unsigned int tower_record_append(TowerRecordBuilder *builder, EngineRecordOperation operation, unsigned int left,
                                        unsigned int right)
{
    if (builder->steps != NULL)
    {
        builder->steps[builder->count] = EngineRecordStep{operation, left, right, 0u};
    }
    builder->count += 1ULL;
    // the code writes a program only once 2^32 - 1 bounds its count. A counted name past that errors unread
    return (unsigned int)(builder->count - 1ULL);
}

static unsigned int tower_record_constant(TowerRecordBuilder *builder, unsigned long long value)
{
    for (unsigned long long at = 0ULL; at < builder->constants.count; at += 1ULL)
    {
        // Explicit cast reads the element of the array as the constant that the builder put there
        const TowerConstant *const constant = (const TowerConstant *)platform_array_at(&builder->constants, at);
        if (constant->value == value)
        {
            return constant->step;
        }
    }
    // a constant step stores its value's low 32 bits in `left` and its high 32 in `right`
    const unsigned int step_index = tower_record_append(
        builder, ENGINE_RECORD_CONSTANT, (unsigned int)(value & 0xFFFFFFFFULL), (unsigned int)(value >> 32u));
    const TowerConstant added = {value, step_index};
    builder->failed |= (platform_array_appended(&builder->constants, &added) == 0) ? 1 : 0;
    return step_index;
}

// floor(v / 2^shift), toward minus infinity as tower_floor_shift: v's residue, the and with 2^shift - 1, is never
// negative, and v less it divides exactly
static unsigned int tower_record_floor_shift(TowerRecordBuilder *builder, unsigned int value, unsigned int shift)
{
    if (shift == 0u)
    {
        return value;
    }
    const unsigned int mask = tower_record_constant(builder, (1ULL << shift) - 1ULL);
    const unsigned int residue = tower_record_append(builder, ENGINE_RECORD_AND, value, mask);
    const unsigned int difference = tower_record_append(builder, ENGINE_RECORD_DIFFERENCE, value, residue);
    return tower_record_append(builder, ENGINE_RECORD_EXACT_QUOTIENT, difference,
                               tower_record_constant(builder, 1ULL << shift));
}

// One lifting step over a band. Each value moves by sign * floor((rounding + sum_k weight_k * other[a + offset_k]) /
// 2^shift). An index past either end of the other band takes that end, and `undo` turns the sign
static void tower_record_lifting_step(TowerRecordBuilder *builder, const TowerLiftingStep &rule, int undo,
                                      unsigned int *target, unsigned long long target_count, const unsigned int *other,
                                      unsigned long long other_count)
{
    // both bands of a lifted line contain at least one value, and far fewer than 2^31
    const long long last = (long long)other_count - 1LL;
    for (unsigned long long at = 0ULL; at < target_count; at += 1ULL)
    {
        unsigned int sum = 0u;
        int summed = 0;
        if (rule.rounding != 0u)
        {
            sum = tower_record_constant(builder, rule.rounding);
            summed = 1;
        }
        for (unsigned int tap = 0u; tap < rule.taps; tap += 1u)
        {
            const long long wanted = (long long)at + (long long)rule.offset[tap];
            const long long taken = (wanted < 0LL) ? 0LL : ((wanted > last) ? last : wanted);
            unsigned int term = other[(size_t)taken];
            // a weight is never INT_MIN. Its magnitude is an int
            const unsigned int magnitude =
                (unsigned int)((rule.weight[tap] < 0) ? -rule.weight[tap] : rule.weight[tap]);
            if (magnitude != 1u)
            {
                term = tower_record_append(builder, ENGINE_RECORD_PRODUCT, term,
                                           tower_record_constant(builder, magnitude));
            }
            if (summed == 0)
            {
                sum = (rule.weight[tap] > 0) ? term
                                             : tower_record_append(builder, ENGINE_RECORD_DIFFERENCE,
                                                                   tower_record_constant(builder, 0ULL), term);
                summed = 1;
            }
            else
            {
                sum = tower_record_append(
                    builder, (rule.weight[tap] > 0) ? ENGINE_RECORD_SUM : ENGINE_RECORD_DIFFERENCE, sum, term);
            }
        }
        const unsigned int moved = tower_record_floor_shift(builder, sum, rule.shift);
        const int adds = (rule.sign > 0) != (undo != 0);
        target[at] =
            tower_record_append(builder, adds ? ENGINE_RECORD_SUM : ENGINE_RECORD_DIFFERENCE, target[at], moved);
    }
}

// One level along one line of `length` values, or its inverse. Lifting splits the line into its evens, the lows, and
// its odds, the highs. It runs the ruleset's steps in order, and puts the lows first and the highs after them, as
// tower_forward_kernel places them. Lowering reads the two bands back from there. It runs the steps last first with
// each sign turned, and interleaves the bands again, as tower_inverse_kernel does.
static void tower_record_line(TowerRecordBuilder *builder, const TowerLiftingStep *rules, unsigned int rule_count,
                              unsigned int *registers, unsigned long long line, unsigned long long stride,
                              unsigned long long length, int inverse)
{
    const unsigned long long lows = (length + 1ULL) / 2ULL;
    const unsigned long long highs = length / 2ULL;
    // a band's k-th value sits at 2k or 2k + 1 along the interleaved line, and at k or lows + k along the lifted one.
    // The two bands share one cell, the highs after the lows
    unsigned int *const low = (unsigned int *)platform_cell_taken(
        (lows + highs) * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (low == NULL)
    {
        builder->failed = 1;
        return;
    }
    unsigned int *const high = &low[lows];
    for (unsigned long long k = 0ULL; k < lows; k += 1ULL)
    {
        low[(size_t)k] = registers[(size_t)(line + (((inverse != 0) ? k : (2ULL * k)) * stride))];
    }
    for (unsigned long long k = 0ULL; k < highs; k += 1ULL)
    {
        high[(size_t)k] = registers[(size_t)(line + (((inverse != 0) ? (lows + k) : ((2ULL * k) + 1ULL)) * stride))];
    }
    for (unsigned int walked = 0u; walked < rule_count; walked += 1u)
    {
        const TowerLiftingStep &rule = rules[(inverse != 0) ? (rule_count - 1u - walked) : walked];
        if (rule.target == TOWER_BAND_HIGH)
        {
            tower_record_lifting_step(builder, rule, inverse, high, highs, low, lows);
        }
        else
        {
            tower_record_lifting_step(builder, rule, inverse, low, lows, high, highs);
        }
    }
    for (unsigned long long k = 0ULL; k < lows; k += 1ULL)
    {
        registers[(size_t)(line + (((inverse != 0) ? (2ULL * k) : k) * stride))] = low[(size_t)k];
    }
    for (unsigned long long k = 0ULL; k < highs; k += 1ULL)
    {
        registers[(size_t)(line + (((inverse != 0) ? ((2ULL * k) + 1ULL) : (lows + k)) * stride))] = high[(size_t)k];
    }
    platform_cell_released(low, platform_place_g_program_owner);
}

// T, or T^-1 run from the last level and axis back, over a block's registers in place. These are the levels and axes
// that tower_lift and tower_lower walk, each axis whose active extent is 2 or more. The lines along an axis of a level
// go in t, z, y, x order, and each line's first position comes from its index
static void tower_record_build(TowerRecordBuilder *builder, const TowerLiftingStep *rules, unsigned int rule_count,
                               const TowerFloors &floors, unsigned int *registers, int inverse)
{
    const unsigned int levels = floors.count;
    for (unsigned int walked = 0u; walked < levels; walked += 1u)
    {
        const TowerStep &step = floors.step[(inverse != 0) ? (levels - 1u - walked) : walked];
        for (unsigned int turn = 0u; turn < 4u; turn += 1u)
        {
            const unsigned int axis = (inverse != 0) ? (3u - turn) : turn;
            if (step.extent[axis] < 2ULL)
            {
                continue;
            }
            unsigned long long across[4];
            unsigned long long lines = 1ULL;
            for (unsigned int each = 0u; each < 4u; each += 1u)
            {
                across[each] = (each == axis) ? 1ULL : step.extent[each];
                lines *= across[each];
            }
            for (unsigned long long index = 0ULL; index < lines; index += 1ULL)
            {
                unsigned long long rest = index;
                unsigned long long line = 0ULL;
                for (unsigned int each = 4u; each > 0u; each -= 1u)
                {
                    line += (rest % across[each - 1u]) * step.stride[each - 1u];
                    rest /= across[each - 1u];
                }
                tower_record_line(builder, rules, rule_count, registers, line, step.stride[axis], step.extent[axis],
                                  inverse);
            }
        }
    }
}

static long tower_record_run(const TowerRecordRequest *request, int inverse)
{
    if ((request == NULL) || (request->error == NULL))
    {
        return TOWER_ERROR;
    }
    EngineError *const error = request->error;
    if (!TOWER_CHECK((request->in_registers != NULL) && (request->out_registers != NULL) && (request->count != NULL),
                     request, error, ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    const unsigned long long lanes = tower_lanes(request->extent);
    if (!TOWER_CHECK((lanes != 0ULL) && (lanes <= 0xFFFFFFFFULL), request->extent, error, ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    // no ruleset named is the kernels' 5/3
    const int named_rules = (request->rules != NULL) || (request->rule_count != 0u);
    const TowerLiftingStep *const rules = (named_rules != 0) ? request->rules : s_tower_five_three;
    const unsigned int rule_count = (named_rules != 0) ? request->rule_count : 2u;
    if (!TOWER_CHECK((rules != NULL) && (rule_count != 0u), request, error, ENGINE_ERROR_REQUEST))
    {
        return TOWER_ERROR;
    }
    for (unsigned int at = 0u; at < rule_count; at += 1u)
    {
        const TowerLiftingStep *const rule = &rules[at];
        int ok = ((rule->target == TOWER_BAND_LOW) || (rule->target == TOWER_BAND_HIGH)) &&
                 ((rule->sign == 1) || (rule->sign == -1)) && (rule->taps >= 1u) &&
                 (rule->taps <= TOWER_RULE_TAPS_MAX) && (rule->shift <= TOWER_RULE_SHIFT_MAX);
        for (unsigned int tap = 0u; (ok != 0) && (tap < rule->taps); tap += 1u)
        {
            // a weight's magnitude is an int. INT_MIN errors with 0
            ok = (rule->weight[tap] != 0) && (rule->weight[tap] != INT_MIN);
        }
        if (!TOWER_CHECK(ok, rule, error, ENGINE_ERROR_REQUEST))
        {
            return TOWER_ERROR;
        }
    }
    const unsigned int start = *request->count;
    for (unsigned long long lane = 0ULL; lane < lanes; lane += 1ULL)
    {
        // a value the block reads is an earlier step's register
        if (!TOWER_CHECK(request->in_registers[lane] < start, &request->in_registers[lane], error,
                         ENGINE_ERROR_REQUEST))
        {
            return TOWER_ERROR;
        }
    }
    // the registers of the counted build and of the written one, each a copy of the block's registers in a cell
    unsigned int *const named = (unsigned int *)platform_cell_taken(
        lanes * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int *const registers = (unsigned int *)platform_cell_taken(
        lanes * sizeof(unsigned int), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (!TOWER_CHECK((named != NULL) && (registers != NULL), request, error, ENGINE_ERROR_RESOURCE))
    {
        platform_cell_released(named, platform_place_g_program_owner);
        platform_cell_released(registers, platform_place_g_program_owner);
        return TOWER_ERROR;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = named, .src = request->in_registers,
               .bytes = (size_t)lanes * sizeof(unsigned int));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = registers, .src = request->in_registers,
               .bytes = (size_t)lanes * sizeof(unsigned int));
    TowerFloors floors;
    tower_floors(request->extent, &floors);
    // counted first. A program that its step names or the caller's capacity cannot fit errors before any step goes in
    TowerRecordBuilder counted;
    counted.steps = NULL;
    counted.count = start;
    counted.failed = 0;
    platform_array_begun(&counted.constants, sizeof(TowerConstant), platform_place_g_program_owner,
                         PLATFORM_TIER_TEMPORARY);
    tower_record_build(&counted, rules, rule_count, floors, named, inverse);
    platform_array_ended(&counted.constants);
    int ok =
        TOWER_CHECK(counted.failed == 0, &counted, error, ENGINE_ERROR_RESOURCE) &&
        TOWER_CHECK((counted.count <= 0xFFFFFFFFULL) &&
                        ((request->steps == NULL) || (counted.count <= (unsigned long long)request->step_capacity)),
                    request, error, ENGINE_ERROR_REQUEST);
    if (ok && (request->steps != NULL))
    {
        TowerRecordBuilder written;
        written.steps = request->steps;
        written.count = start;
        written.failed = 0;
        platform_array_begun(&written.constants, sizeof(TowerConstant), platform_place_g_program_owner,
                             PLATFORM_TIER_TEMPORARY);
        tower_record_build(&written, rules, rule_count, floors, registers, inverse);
        platform_array_ended(&written.constants);
        ok = TOWER_CHECK(written.failed == 0, &written, error, ENGINE_ERROR_RESOURCE);
    }
    if (ok)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request->out_registers, .src = named,
                   .bytes = (size_t)lanes * sizeof(unsigned int));
        // the check above bounds the count by 2^32 - 1. It narrows to the program's next step exactly
        *request->count = (unsigned int)counted.count;
    }
    platform_cell_released(named, platform_place_g_program_owner);
    platform_cell_released(registers, platform_place_g_program_owner);
    return ok ? 0L : TOWER_ERROR;
}

extern "C" long tower_record_lift(const TowerRecordRequest *request)
{
    return tower_record_run(request, 0);
}

extern "C" long tower_record_lower(const TowerRecordRequest *request)
{
    return tower_record_run(request, 1);
}
