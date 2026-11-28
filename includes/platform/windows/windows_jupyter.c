// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_jupyter.c: a Jupyter kernel on Windows. Its data is under APPDATA and PROGRAMDATA, and an environment's
// python is at the environment's top. An event the kernel inherits interrupts it, and its own children end before it
#include "../../../ui/cli/src/notebooks/jupyter_internal.h"

#if defined(_WIN32)

unsigned int cli_jupyter_reader_folders(const CliJupyterSystemCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "APPDATA", .value_out = home,
                    .capacity = sizeof(home)))
    {
        return args->count;
    }
    return EMBED_CALL(cli_jupyter_system.folder_append, CliJupyterSystemCtx, .folders = args->folders,
                      .count = args->count, .folder = home, .name = "jupyter");
}

unsigned int cli_jupyter_shared_folders(const CliJupyterSystemCtx *args)
{
    char shared[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PROGRAMDATA",
                    .value_out = shared, .capacity = sizeof(shared)))
    {
        return args->count;
    }
    return EMBED_CALL(cli_jupyter_system.folder_append, CliJupyterSystemCtx, .folders = args->folders,
                      .count = args->count, .folder = shared, .name = "jupyter");
}

void cli_jupyter_status(const CliJupyterSystemCtx *args)
{
    mmgr_line_put(args->err_msg, "exit code: ");
    mmgr_line_decimal_signed(args->err_msg, args->code);
}

const char *cli_jupyter_env_python(void)
{
    return "python.exe";
}

// the event the kernel inherits, its number in JPY_INTERRUPT_EVENT
unsigned int cli_jupyter_interrupt_setting(const CliJupyterSystemCtx *args)
{
    CliKernel *const kernel = args->kernel;
    kernel->interrupt_event = cli_event_inheritable();
    VerbaLinea number = {kernel->event_number, sizeof(kernel->event_number), 0ULL};
    mmgr_line_decimal(&number, kernel->interrupt_event, 1u);
    mmgr_line_finish(&number);
    kernel->settings[args->count] = (PlatformSetting){"JPY_INTERRUPT_EVENT", kernel->event_number};
    return args->count + 1u;
}

// the kernel's whole tree, as a kernel's own children would outlive it
void cli_jupyter_children_end(const CliJupyterSystemCtx *args)
{
    EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = args->kernel->spawned.id);
}

int cli_jupyter_kernel_interrupt(const CliJupyterSystemCtx *args)
{
    if (!EMBED_CALL(cli_event_internal.event_set, CliEventCtx, .event = args->kernel->interrupt_event))
    {
        mmgr_line_put(args->err_msg, "the kernel's interrupt event could not be signaled");
        return 0;
    }
    return 1;
}
#endif
