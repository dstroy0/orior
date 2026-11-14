// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_arith.c: the arithmetic on a double's bits: the floor, ceiling and rounding at whole values, halves and
// negatives, the absolute value and the remainder, the root against its square, the sine and cosine at the axes and
// against their identity, the exponential and the logarithm each other's inverse, and the power
#include "includes/arith/arith.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_floor_and_ceiling_cut_at_whole_values(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(2.0, arith_floor(2.7));
    TEST_ASSERT_EQUAL_DOUBLE(-3.0, arith_floor(-2.3));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, arith_floor(2.0));
    TEST_ASSERT_EQUAL_DOUBLE(-2.0, arith_floor(-2.0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, arith_floor(0.4));
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, arith_floor(-0.4));
    TEST_ASSERT_EQUAL_DOUBLE(3.0, arith_ceil(2.1));
    TEST_ASSERT_EQUAL_DOUBLE(-2.0, arith_ceil(-2.9));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, arith_ceil(0.1));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, arith_ceil(-0.1));
    TEST_ASSERT_EQUAL_DOUBLE(123456789012345.0, arith_floor(123456789012345.5));
    TEST_ASSERT_EQUAL_DOUBLE(9007199254740993.0, arith_floor(9007199254740993.0));
    TEST_ASSERT_EQUAL_FLOAT(4.0F, arith_floor_f32(4.9F));
    TEST_ASSERT_EQUAL_FLOAT(5.0F, arith_ceil_f32(4.1F));
}

void test_round_takes_a_half_away_from_zero(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(3.0, arith_round(2.5));
    TEST_ASSERT_EQUAL_DOUBLE(-3.0, arith_round(-2.5));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, arith_round(2.4999));
    TEST_ASSERT_EQUAL_DOUBLE(-2.0, arith_round(-2.4999));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, arith_round(0.25));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, arith_round(0.5));
    TEST_ASSERT_EQUAL_INT64(-3LL, arith_round_whole(-2.5));
    TEST_ASSERT_EQUAL_INT64(7LL, arith_round_whole(6.75));
    TEST_ASSERT_EQUAL_FLOAT(13.0F, arith_round_f32(12.5F));
}

void test_absolute_value_least_most_and_remainder(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(2.5, arith_abs(-2.5));
    TEST_ASSERT_EQUAL_DOUBLE(2.5, arith_abs(2.5));
    TEST_ASSERT_EQUAL_FLOAT(1.25F, arith_abs_f32(-1.25F));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, arith_min(1.0, 2.0));
    TEST_ASSERT_EQUAL_DOUBLE(2.0, arith_max(1.0, 2.0));
    TEST_ASSERT_EQUAL_FLOAT(0.0F, arith_min_f32(0.0F, 0.5F));
    TEST_ASSERT_EQUAL_FLOAT(1.0F, arith_max_f32(0.3F, 1.0F));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.5, arith_remainder(7.5, 3.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, arith_remainder(14.0, 4.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.25, arith_remainder(1000000.25, 1.0));
}

void test_root_squares_back_to_its_value(void)
{
    const double values[6] = {0.0, 1.0, 2.0, 0.0001, 12345.678, 1.0e12};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        const double root = arith_root(values[at]);
        TEST_ASSERT_DOUBLE_WITHIN(values[at] * 1e-14 + 1e-300, values[at], root * root);
    }
    TEST_ASSERT_EQUAL_DOUBLE(3.0, arith_root(9.0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, arith_root(-4.0));
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 1.4142135F, arith_root_f32(2.0F));
}

void test_sine_and_cosine_at_the_axes_and_their_identity(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, arith_sin(0.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1.0, arith_cos(0.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 1.0, arith_sin(ARITH_PI / 2.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 0.0, arith_cos(ARITH_PI / 2.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 0.0, arith_sin(ARITH_PI));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, -1.0, arith_cos(ARITH_PI));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, -1.0, arith_sin(3.0 * ARITH_PI / 2.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 0.5, arith_sin(ARITH_PI / 6.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 0.5, arith_cos(ARITH_PI / 3.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, -0.5, arith_sin(-ARITH_PI / 6.0));
    for (int step = -40; step <= 40; step += 1)
    {
        const double angle = (double)step * 0.37;
        const double sine = arith_sin(angle);
        const double cosine = arith_cos(angle);
        TEST_ASSERT_DOUBLE_WITHIN(1e-13, 1.0, (sine * sine) + (cosine * cosine));
        TEST_ASSERT_DOUBLE_WITHIN(1e-13, arith_sin(angle + ARITH_PI / 2.0), cosine);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.8414710F, arith_sin_f32(1.0F));
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.5403023F, arith_cos_f32(1.0F));
}

void test_exponential_and_logarithm_invert_each_other(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1.0, arith_exp(0.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 2.718281828459045, arith_exp(1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-16, 0.36787944117144233, arith_exp(-1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, arith_log(1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 0.6931471805599453, arith_log(2.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 2.302585092994046, arith_log(10.0));
    const double values[6] = {0.001, 0.5, 1.5, 10.0, 1234.5, 1.0e10};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        TEST_ASSERT_DOUBLE_WITHIN(values[at] * 1e-13, values[at], arith_exp(arith_log(values[at])));
    }
    for (int step = -20; step <= 20; step += 1)
    {
        const double power = (double)step * 1.7;
        TEST_ASSERT_DOUBLE_WITHIN(1e-13, power, arith_log(arith_exp(power)));
    }
    TEST_ASSERT_EQUAL_DOUBLE(0.0, arith_exp(-800.0));
    TEST_ASSERT_TRUE(arith_exp(800.0) > 1.0e308);
    TEST_ASSERT_TRUE(arith_log(0.0) < -1.0e308);
}

void test_hypotenuse_arc_tangent_and_arc_cosine(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 5.0, arith_hypot(3.0, 4.0));
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 13.0F, arith_hypot_f32(5.0F, -12.0F));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 0.0, arith_atan(0.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, ARITH_PI / 4.0, arith_atan(1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, -ARITH_PI / 4.0, arith_atan(-1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, ARITH_PI / 6.0, arith_atan(1.0 / 1.7320508075688772));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, ARITH_PI / 3.0, arith_atan(1.7320508075688772));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.4711276743037347, arith_atan(10.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 0.0, arith_acos(1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, ARITH_PI / 2.0, arith_acos(0.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, ARITH_PI, arith_acos(-1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, ARITH_PI / 3.0, arith_acos(0.5));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 2.0 * ARITH_PI / 3.0, arith_acos(-0.5));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, ARITH_PI, arith_acos(-7.0));
    for (int step = -10; step <= 10; step += 1)
    {
        const double cosine = (double)step * 0.099;
        TEST_ASSERT_DOUBLE_WITHIN(1e-12, cosine, arith_cos(arith_acos(cosine)));
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 1.0471976F, arith_acos_f32(0.5F));
    TEST_ASSERT_EQUAL_INT64(3LL, arith_round_whole_f32(2.5F));
}

void test_power_and_finite(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 8.0, arith_pow(2.0, 3.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 0.125, arith_pow(2.0, -3.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 3.0, arith_pow(9.0, 0.5));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, arith_pow(0.0, 0.45));
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.7297400F, arith_pow_f32(0.5F, 0.4545454F));
    TEST_ASSERT_TRUE(arith_finite(1.0));
    TEST_ASSERT_TRUE(arith_finite(-1.0e308));
    TEST_ASSERT_FALSE(arith_finite(arith_value(0x7FF0000000000000ULL)));
    TEST_ASSERT_FALSE(arith_finite(arith_value(0x7FF8000000000000ULL)));
}
