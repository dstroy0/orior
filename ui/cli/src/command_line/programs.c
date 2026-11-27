// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// programs.c: the processes that a run starts, through the platform's programs. Each call here gives its arguments to
// one platform call (platform.h), and so no call of this file holds a system's code. The folders of the PATH split
// and join at the system's list separator
#include "cli_internal.h"

// the start that the arguments of a call ask for, with its input piped where `input_piped`, at `priority`
static PlatformStart cli_start_of(const CliWordsErrMsgCtx *args, unsigned int priority, int input_piped)
{
    const PlatformStart start = {.words = args->words,
                                 .folder = args->folder,
                                 .settings = args->settings,
                                 .setting_count = args->count,
                                 .priority = priority,
                                 .detached = args->detached,
                                 .input_piped = input_piped,
                                 .inherited_handle = args->inherited_handle,
                                 .err_msg = args->err_msg};
    return start;
}

int cli_spawn(const CliWordsErrMsgCtx *args)
{
    const PlatformStart start = cli_start_of(args, PLATFORM_PRIORITY_WORK, 0);
    return platform_program_started(&start, args->spawned);
}

int cli_spawn_as(const CliWordsErrMsgCtx *args)
{
    const PlatformStart start = cli_start_of(args, args->priority, 0);
    return platform_program_started(&start, args->spawned);
}

int cli_spawn_feed(const CliWordsErrMsgCtx *args)
{
    const PlatformStart start = cli_start_of(args, PLATFORM_PRIORITY_WORK, 1);
    return platform_program_started(&start, args->spawned);
}

int cli_spawn_piped_as(const CliWordsErrMsgCtx *args)
{
    const PlatformStart start = cli_start_of(args, args->priority, 1);
    return platform_program_started(&start, args->spawned);
}

int cli_spawn_write(const CliPipeHandleCtx *args)
{
    return platform_program_written(args->pipe_handle, args->bytes, args->size);
}

int cli_spawn_terminal(const CliWordsErrMsgSpawnedCtx *args)
{
    const PlatformStart start = {.words = args->words,
                                 .folder = args->folder,
                                 .settings = args->settings,
                                 .setting_count = args->count,
                                 .err_msg = args->err_msg};
    return platform_program_terminal(&start, args->cols, args->rows, args->spawned);
}

int cli_spawn_terminal_resize(const CliColsCtx *args)
{
    return platform_program_terminal_sized(args->spawned_read, args->cols, args->rows);
}

void cli_spawn_terminal_hung_up(const CliColsCtx *args)
{
    platform_program_terminal_hung_up(args->spawned_read);
}

void cli_spawn_terminal_close(const CliSpawnedCtx *args)
{
    platform_program_terminal_closed(args->spawned);
}

int cli_spawn_share(const CliWordsErrMsgSettingsCtx *args)
{
    const PlatformStart start = {
        .words = args->words, .settings = args->settings, .setting_count = args->count, .err_msg = args->err_msg};
    return platform_program_shared(&start, args->code);
}

void cli_spawn_input_close(const CliSpawnedCtx *args)
{
    platform_program_input_closed(args->spawned);
}

unsigned long long cli_spawn_read(const CliCapacityPipeHandleCtx *args)
{
    return platform_program_read(args->pipe_handle, args->piece, args->capacity);
}

unsigned long long cli_spawn_waiting(const CliCapacityPipeHandleCtx *args)
{
    return platform_program_waiting(args->pipe_handle);
}

long long cli_spawn_wait(const CliSpawnedCtx *args)
{
    return platform_program_waited(args->spawned);
}

int cli_spawn_poll(const CliSpawnedCtx *args)
{
    return platform_program_polled(args->spawned, args->code);
}

void cli_spawn_group_end(const CliGroupCtx *args)
{
    platform_program_group_ended(args->group);
}

void cli_spawn_close(const CliSpawnedCtx *args)
{
    platform_program_closed(args->spawned);
}

void cli_spawn_tree_end(const CliIdCtx *args)
{
    platform_program_tree_ended(args->id_u64);
}

void cli_spawn_kill(const CliColsCtx *args)
{
    platform_program_killed(args->spawned_read);
}

int cli_spawn_interrupt(const CliColsCtx *args)
{
    return platform_program_interrupted(args->spawned_read);
}

// the processes started outside the container, at max a static count of them
#define CLI_OUTSIDE_MAX 1024u
static unsigned int s_outside[CLI_OUTSIDE_MAX];
static unsigned int s_outsides;

int cli_started_outside(const CliWordsIdCtx *args)
{
    // None of the starter's standard handles goes to the process, and so no reader of the starter's output waits on a
    // process that outlives it
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = mmgr_line_over(why, sizeof(why));
    const PlatformStart start = {
        .words = args->words, .output = PLATFORM_OUTPUT_NONE, .windowless = 1, .outlives = 1, .err_msg = &err_msg};
    PlatformProgram started;
    if (!platform_program_started(&start, &started))
    {
        return 0;
    }
    // a process id that the system gives fits the 32 bits of an unsigned int
    *args->id = (unsigned int)started.id;
    if (s_outsides < CLI_OUTSIDE_MAX)
    {
        s_outside[s_outsides] = *args->id;
        s_outsides += 1u;
    }
    platform_program_closed(&started);
    return 1;
}

unsigned int cli_started_outside_listed(const unsigned int **ids)
{
    *ids = s_outside;
    return s_outsides;
}

unsigned int cli_path_folders(const CliCapacityFoldersCtx *args)
{
    // the PATH read into a cell of this call's own, and so two threads that read it share nothing
    char *const path = g_platform_guard->temporary_buf_alloc(QUERY_ENVIRONMENT_MAX_LEN, g_cli_platform_owner);
    if (path == NULL)
    {
        return 0u;
    }
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "PATH", .value_out = path,
                    .capacity = QUERY_ENVIRONMENT_MAX_LEN))
    {
        g_platform_guard->persistent_buf_release(path, g_cli_platform_owner);
        return 0u;
    }
    const char separators[2] = {g_platform_list_separator, '\0'};
    unsigned int count = 0u;
    const char *from = path;
    for (;;)
    {
        // the bytes of the cell after `from`, which is inside the cell
        const size_t left = QUERY_ENVIRONMENT_MAX_LEN - (size_t)(from - path);
        const size_t length =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = from, .cap = left, .other = separators);
        if ((length != 0u) && (count < args->capacity_u32))
        {
            VerbaLinea line = mmgr_line_over(args->folders[count], CLI_PATH_MAX_LEN);
            mmgr_line_put_n(&line, from, length);
            count += (mmgr_line_finish(&line) != 0u) ? 1u : 0u;
        }
        if (from[length] == '\0')
        {
            break;
        }
        from += length + 1u;
    }
    g_platform_guard->persistent_buf_release(path, g_cli_platform_owner);
    return count;
}

void cli_path_join(const CliPathCountSizeCtx *args)
{
    const char separators[2] = {g_platform_list_separator, '\0'};
    VerbaLinea line = mmgr_line_over(args->path_out, args->size);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(&line, (at != 0u) ? separators : "");
        mmgr_line_put(&line, args->folders_callback[at]);
    }
    (void)mmgr_line_finish(&line);
}
