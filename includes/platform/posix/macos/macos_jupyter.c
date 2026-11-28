// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_jupyter.c: the reader's Jupyter data on macOS, under ~/Library
#include "../../../../ui/cli/src/notebooks/jupyter_internal.h"

#if defined(__APPLE__)

unsigned int cli_jupyter_reader_folders(const CliJupyterSystemCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    char under[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME", .value_out = home,
                    .capacity = sizeof(home)))
    {
        return args->count;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "Library", .joined = under);
    return EMBED_CALL(cli_jupyter_system.folder_append, CliJupyterSystemCtx, .folders = args->folders,
                      .count = args->count, .folder = under, .name = "Jupyter");
}
#endif
