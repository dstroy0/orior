// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_sea.c: Node's single executable on macOS, where a node older than 25.5 needs postject, which impero does not
// run
#include "../../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../../ui/cli/src/programs/sea.h"

#if defined(__APPLE__)

int cli_sea_older_node(const CliSeaSystemCfg *args)
{
    mmgr_line_put(args->err_msg, "on macOS, Node ");
    mmgr_line_decimal(args->err_msg, args->major, 1u);
    mmgr_line_ch(args->err_msg, '.');
    mmgr_line_decimal(args->err_msg, args->minor, 1u);
    mmgr_line_put(args->err_msg, " needs postject to add the blob; Node 25.5 builds it itself, with --build-sea");
    return 0;
}
#endif
