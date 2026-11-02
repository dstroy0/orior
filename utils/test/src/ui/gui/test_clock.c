// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_clock.c: the clock the window's modules read under the checks
#include "test_clock.h"

#undef cli_clock_milliseconds
unsigned long long cli_clock_milliseconds(void);
unsigned long long test_clock_milliseconds(void);

unsigned long long g_test_clock_ms = GUI_TEST_CLOCK_MACHINE;

unsigned long long test_clock_milliseconds(void)
{
    return (g_test_clock_ms != GUI_TEST_CLOCK_MACHINE) ? g_test_clock_ms : cli_clock_milliseconds();
}
