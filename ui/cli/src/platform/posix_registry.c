// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_registry.c: off Windows there is no registry: the PATH is the one orior_cli started with, and a folder added to
// the reader's PATH goes into their shell's profile instead
#include "../command_line/cli_internal.h"

#if !defined(_WIN32)
int cli_registry_machine_path(const CliOutCtx *args)
{
    (void)args;
    return 0;
}

int cli_registry_user_value(const CliOutCapacityCtx *args)
{
    (void)args;
    return 0;
}

int cli_registry_set_user_value(const CliErrMsgNameCtx *args)
{
    mmgr_line_put(args->err_msg, args->name);
    mmgr_line_put(args->err_msg, " is stored in the registry, which only Windows has");
    return 0;
}

void cli_registry_announce(void)
{
    // this system has no registry whose change is announced
}

void cli_registry_expand(const CliOutCtx *args)
{
    memor.cpy(&(MemoriaCfg){.dst = args->out,
                            .src = args->text,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}) + 1u});
}
#endif
