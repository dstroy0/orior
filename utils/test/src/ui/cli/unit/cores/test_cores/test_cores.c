// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_cores.c: at least one core is reserved and at least one is left for work
#include "ui/cli/src/machine/cores.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// `count` less `less`, one at the least
static unsigned int cores_left(unsigned int count, unsigned int less)
{
    return (count > less + 1u) ? count - less : 1u;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
    EMBED_CALL(cli_cores.reserve, CliCoresCfg, .reserved = CLI_CORES_RESERVED_MIN);
}

void test_reserving_zero_reserves_one(void)
{
    EMBED_CALL(cli_cores.reserve, CliCoresCfg, .reserved = 0u);
    TEST_ASSERT_EQUAL_UINT(1u, cli_cores_on_reserve());
}

void test_reserving_one_leaves_the_remainder_for_work(void)
{
    EMBED_CALL(cli_cores.reserve, CliCoresCfg, .reserved = 0u);
    TEST_ASSERT_EQUAL_UINT(cores_left(cli_cores_machine(), 1u), cli_cores_for_work());
}

void test_reserving_more_than_the_machine_has_leaves_one_for_work(void)
{
    EMBED_CALL(cli_cores.reserve, CliCoresCfg, .reserved = cli_cores_machine() + 4u);
    TEST_ASSERT_EQUAL_UINT(1u, cli_cores_for_work());
}

void test_reserving_two_leaves_the_remainder_for_work(void)
{
    EMBED_CALL(cli_cores.reserve, CliCoresCfg, .reserved = 2u);
    TEST_ASSERT_EQUAL_UINT(cores_left(cli_cores_machine(), 2u), cli_cores_for_work());
}
