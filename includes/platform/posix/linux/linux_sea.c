// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// linux_sea.c: Node's single executable on Linux, where a copy of node takes the blob as a note whatever its version
#include "../../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../../ui/cli/src/programs/sea.h"

#if defined(__linux__)

int cli_sea_older_node(const CliSeaSystemCfg *args)
{
    // a Linux copy of node takes the blob as a note, whatever node's version
    (void)args;
    return 1;
}
#endif
