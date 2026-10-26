// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_watch.c: off Windows the system does not report the tree's changes here; the tree is read every two seconds
// instead (watch.c)
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
int cli_watch_system_start(const char *root, CliChangeFn emit, void *context)
{
    (void)root;
    (void)emit;
    (void)context;
    return 0;
}

void cli_watch_system_stop(void)
{
}
#endif
