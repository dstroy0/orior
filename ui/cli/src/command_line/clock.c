// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// clock.c: the command line's monotonic clock in microseconds and in milliseconds, each read from the one clock of the
// platform, embed_clock_ns (the files of the platform)
#include "cli_internal.h"

unsigned long long cli_clock_microseconds(void)
{
    return (unsigned long long)embed_clock_ns() / 1000ULL;
}

unsigned long long cli_clock_milliseconds(void)
{
    return (unsigned long long)embed_clock_ns() / 1000000ULL;
}
