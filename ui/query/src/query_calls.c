// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_calls.c: the query's calls of the system that the platform makes, each under the query's own argument records
// (platform.h). The calls into the system are the platform's, and none of them is here
#include "../../../includes/platform/platform.h"
#include "query_internal.h"

void query_pause(const QueryMillisecondsCtx *args)
{
    platform_paused(args->milliseconds);
}

// the platform's output modes, in the order of the query's: none, piped, own, piped alone
static const unsigned int s_query_outputs[4] = {PLATFORM_OUTPUT_NONE, PLATFORM_OUTPUT_JOINED, PLATFORM_OUTPUT_OWN,
                                                PLATFORM_OUTPUT_ALONE};

int query_process_start(const QueryStartedCtx *args)
{
    // a mode that the query does not name sends the output and the errors to nothing
    const unsigned int mode = ((args->output >= 0) && (args->output < 4)) ? (unsigned int)args->output : 0u;
    char why[QUERY_PATH_MAX_LEN];
    VerbaLinea err_msg = mmgr_line_over(why, sizeof(why));
    const PlatformStart start = {.words = args->words,
                                 .folder = args->folder,
                                 .output = s_query_outputs[mode],
                                 .windowless = 1,
                                 .err_msg = &err_msg};
    return platform_program_started(&start, args->started);
}

unsigned long long query_process_read(const QueryProcessPieceCtx *args)
{
    return platform_program_read(args->process->out, args->piece, args->capacity_u64);
}

long long query_process_wait(const QueryProcessCtx *args)
{
    PlatformProgram *const process = args->process_out;
    if (process->process == 0ULL)
    {
        return -1LL;
    }
    const long long code = platform_program_waited(process);
    platform_program_closed(process);
    return code;
}

unsigned long long query_now(void)
{
    return platform_epoch_seconds();
}

int query_is_folder(const QueryPathSecondsCtx *args)
{
    return platform_folder_is(args->path);
}

// what one walk of a folder hands to the platform: the query's own call of each entry and its context
typedef struct
{
    void (*each)(const CliEachNameIsFolderCfg *args);
    void *context;
} QueryCallsEach;

// one entry of the folder, which the platform heard, passed on to the query's own call
static void query_calls_entry(void *walk, const char *name, int folder)
{
    // Explicit cast takes the context that query_folder_each gave as the walk that it holds
    const QueryCallsEach *const each = (const QueryCallsEach *)walk;
    EMBED_CALL(each->each, CliEachNameIsFolderCfg, .context = each->context, .name = name, .is_folder = folder);
}

int query_folder_each(const QueryFolderEachCtx *args)
{
    QueryCallsEach each = {args->each, args->context_bytess};
    return platform_folder_listed(args->folder, query_calls_entry, &each);
}

int query_folder_init(const QueryFolderContextCtx *args)
{
    // a folder of the query is the reader's to change, and the reader's group's to read and to search
    return platform_folders_made(args->folder, 0750u);
}

int query_environment_set(const QueryNameCtx *args)
{
    return platform_environment_set(args->name, args->value);
}

int query_environment_read(const QueryNameValueCtx *args)
{
    return platform_environment(args->name, args->value_out, args->capacity) != 0ULL;
}

int query_environment_write(const QueryNameOutCtx *args)
{
    char s_value[QUERY_ENVIRONMENT_MAX_LEN];
    const int read = EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = args->name,
                                .value_out = s_value, .capacity = sizeof(s_value));
    if (read)
    {
        mmgr_line_put(args->out, s_value);
    }
    return read && mmgr_line_ok(args->out);
}
