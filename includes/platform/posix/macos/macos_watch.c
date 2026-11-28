// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_watch.c: the tree's changes on macOS, which the system's watch does not report here. The start refuses, and
// the tree is read every two seconds instead (watch.c)
#include "../../../../ui/cli/src/command_line/cli_internal.h"

#if defined(__APPLE__)

int cli_watch_system_start(const CliRootContextCtx *args)
{
    (void)args;
    return 0;
}

void cli_watch_system_stop(void)
{
}
#endif
