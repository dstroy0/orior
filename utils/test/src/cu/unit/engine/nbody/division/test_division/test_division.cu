// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_division.cu: the division program's writer, a static helper of division.cu reached through its wrapper, and the program it writes:
// a voxel size of zero refused, every step written, the scale constants divided by the common unit and the outputs at their steps
#include "engine/nbody/division/division_wrap.cu"
#include "unity.h"

static EngineRecordStep s_program[DIVISION_STEPS];
static unsigned int s_outputs[DIVISION_OUTPUTS];

// the program written for voxels of these sizes, its fields numbered from one: its answer
static long program_written(unsigned long long across, unsigned long long along, unsigned long long deep)
{
    DivisionRequest request;
    memset(&request, 0, sizeof(request));
    request.mass_field = 1u;
    for (unsigned int axis = 0u; axis < ENGINE_AXES; axis += 1u)
    {
        request.sum_field[axis] = 2u + axis;
    }
    for (unsigned int moment = 0u; moment < DIVISION_MOMENTS; moment += 1u)
    {
        request.moment_field[moment] = 5u + moment;
    }
    request.voxel_pm[0] = across;
    request.voxel_pm[1] = along;
    request.voxel_pm[2] = deep;
    return division_program(&request, s_program, s_outputs);
}

void setUp(void)
{
    memset(s_program, 0, sizeof(s_program));
    memset(s_outputs, 0, sizeof(s_outputs));
}

void tearDown(void)
{
}

void test_a_step_is_written_at_the_next_place_and_its_place_returned(void)
{
    DivisionWriter writer = {s_program, 5u};
    TEST_ASSERT_EQUAL_UINT(5u, division_put(&writer, ENGINE_RECORD_SUM, 1u, 2u, 3u));
    TEST_ASSERT_EQUAL_UINT(6u, writer.written);
    TEST_ASSERT_EQUAL_INT((int)ENGINE_RECORD_SUM, (int)s_program[5].operation);
    TEST_ASSERT_EQUAL_UINT(1u, s_program[5].left);
    TEST_ASSERT_EQUAL_UINT(2u, s_program[5].right);
    TEST_ASSERT_EQUAL_UINT(3u, s_program[5].member);
}

void test_a_step_past_the_program_is_counted_and_not_written(void)
{
    EngineRecordStep untouched[DIVISION_STEPS + 1u];
    memset(untouched, 0x5a, sizeof(untouched));
    DivisionWriter writer = {untouched, DIVISION_STEPS};
    TEST_ASSERT_EQUAL_UINT(DIVISION_STEPS, division_put(&writer, ENGINE_RECORD_SUM, 1u, 2u, 3u));
    TEST_ASSERT_EQUAL_UINT(DIVISION_STEPS + 1u, writer.written);
    TEST_ASSERT_EQUAL_UINT(0x5a5a5a5au, untouched[DIVISION_STEPS].left);
}

void test_a_constant_is_written_as_its_low_and_high_words(void)
{
    DivisionWriter writer = {s_program, 0u};
    TEST_ASSERT_EQUAL_UINT(0u, division_constant(&writer, 0x1122334455667788ull));
    TEST_ASSERT_EQUAL_INT((int)ENGINE_RECORD_CONSTANT, (int)s_program[0].operation);
    TEST_ASSERT_EQUAL_HEX32(0x55667788u, s_program[0].left);
    TEST_ASSERT_EQUAL_HEX32(0x11223344u, s_program[0].right);
}

void test_a_voxel_size_of_zero_on_any_axis_is_refused(void)
{
    TEST_ASSERT_EQUAL_INT64(DIVISION_ERROR, program_written(0ull, 100ull, 100ull));
    TEST_ASSERT_EQUAL_INT64(DIVISION_ERROR, program_written(100ull, 0ull, 100ull));
    TEST_ASSERT_EQUAL_INT64(DIVISION_ERROR, program_written(100ull, 100ull, 0ull));
}

void test_the_program_fills_every_step(void)
{
    TEST_ASSERT_EQUAL_INT64((long)DIVISION_STEPS, program_written(100ull, 100ull, 200ull));
}

void test_the_scale_constants_are_the_voxel_sizes_over_their_common_unit(void)
{
    (void)program_written(100ull, 100ull, 200ull);
    // the first axis's constant follows the eleven steps of the mass test, and each axis takes a constant and three sums
    TEST_ASSERT_EQUAL_INT((int)ENGINE_RECORD_CONSTANT, (int)s_program[11].operation);
    TEST_ASSERT_EQUAL_UINT(1u, s_program[11].left);
    TEST_ASSERT_EQUAL_INT((int)ENGINE_RECORD_CONSTANT, (int)s_program[15].operation);
    TEST_ASSERT_EQUAL_UINT(1u, s_program[15].left);
    TEST_ASSERT_EQUAL_INT((int)ENGINE_RECORD_CONSTANT, (int)s_program[19].operation);
    TEST_ASSERT_EQUAL_UINT(2u, s_program[19].left);
}

void test_the_mass_output_is_one_less_the_difference_of_the_mass_bands(void)
{
    (void)program_written(100ull, 100ull, 200ull);
    const EngineRecordStep *const by_mass = &s_program[s_outputs[DIVISION_BY_MASS]];
    TEST_ASSERT_EQUAL_INT((int)ENGINE_RECORD_DIFFERENCE, (int)by_mass->operation);
    TEST_ASSERT_EQUAL_INT((int)ENGINE_RECORD_CONSTANT, (int)s_program[by_mass->left].operation);
    TEST_ASSERT_EQUAL_UINT(1u, s_program[by_mass->left].left);
    TEST_ASSERT_EQUAL_INT((int)ENGINE_RECORD_ABSOLUTE, (int)s_program[by_mass->right].operation);
}

void test_the_last_step_is_the_product_of_the_mass_and_axis_outputs(void)
{
    (void)program_written(100ull, 100ull, 200ull);
    TEST_ASSERT_EQUAL_UINT(DIVISION_STEPS - 1u, s_outputs[DIVISION_BY_BOTH]);
    const EngineRecordStep *const both = &s_program[DIVISION_STEPS - 1u];
    TEST_ASSERT_EQUAL_INT((int)ENGINE_RECORD_PRODUCT, (int)both->operation);
    TEST_ASSERT_EQUAL_UINT(s_outputs[DIVISION_BY_MASS], both->left);
    TEST_ASSERT_EQUAL_UINT(s_outputs[DIVISION_BY_AXIS], both->right);
}
