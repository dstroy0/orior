// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_jupyter.c: a Jupyter kernel on the POSIX systems. Its shared data is under /usr/local/share and /usr/share, an
// environment's python is in its bin, and a signal interrupts the kernel. The reader's own data is where each system
// keeps it (linux/linux_jupyter.c, macos/macos_jupyter.c)
#include "../../../ui/cli/src/notebooks/jupyter_internal.h"

#if !defined(_WIN32)

unsigned int cli_jupyter_shared_folders(const CliJupyterSystemCtx *args)
{
    const unsigned int count =
        EMBED_CALL(cli_jupyter_system.folder_append, CliJupyterSystemCtx, .folders = args->folders,
                   .count = args->count, .folder = "/usr/local/share", .name = "jupyter");
    return EMBED_CALL(cli_jupyter_system.folder_append, CliJupyterSystemCtx, .folders = args->folders, .count = count,
                      .folder = "/usr/share", .name = "jupyter");
}

void cli_jupyter_status(const CliJupyterSystemCtx *args)
{
    if (args->code < 0LL)
    {
        mmgr_line_put(args->err_msg, "signal");
        return;
    }
    mmgr_line_put(args->err_msg, "exit status: ");
    mmgr_line_decimal_signed(args->err_msg, args->code);
}

const char *cli_jupyter_env_python(void)
{
    return "bin/python3";
}

unsigned int cli_jupyter_interrupt_setting(const CliJupyterSystemCtx *args)
{
    // a signal interrupts the kernel, and no event goes to it
    return args->count;
}

void cli_jupyter_children_end(const CliJupyterSystemCtx *args)
{
    // the kill that follows ends the kernel's process alone
    (void)args;
}

int cli_jupyter_kernel_interrupt(const CliJupyterSystemCtx *args)
{
    // the call ignores what kill reports of the signal, as the reader learns of no failure of it
    (void)EMBED_CALL(cli_cols_internal.spawn_interrupt, CliColsCtx, .spawned_read = &args->kernel->spawned);
    return 1;
}
#endif
