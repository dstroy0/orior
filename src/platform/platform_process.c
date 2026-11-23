// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_process.c: the entry of every program, which the platform owns. The process stage comes first: main places
// the pool of the program before program_main runs, and ends the stage after program_main returns. A program defines
// program_main and no main, and so no program can skip the order or run it in another order
#include "platform_internal.h"

// the bytes of the pool where the program declares none, the fewest that the platform places
EMBED_WEAK_ZERO(const unsigned long long, g_program_pool_bytes)

int main(int argc, char **argv)
{
    if (platform_open(g_program_pool_bytes) == 0)
    {
        static const char refused[] = "  FAILED: the platform places the pool of the program\n";
        platform_terminal_write(1, refused, sizeof(refused) - 1u);
        return 1;
    }
    const int code = program_main(argc, argv);
    platform_close();
    return code;
}
