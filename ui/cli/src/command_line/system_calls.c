// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system_calls.c: the command line's calls of the system that the platform makes, each under the command line's own
// argument records (platform.h). The calls into the system are the platform's, and none of them is here
#include "cli_internal.h"

unsigned long long cli_wall_milliseconds(void)
{
    return platform_epoch_microseconds() / 1000ULL;
}

unsigned int cli_process_own(void)
{
    // a process id of the system is positive and fits an unsigned int
    return (unsigned int)platform_process_id();
}

int cli_file_size(const CliPathSizeCtx *args)
{
    return platform_file_size(args->path, args->size_pointer);
}

int cli_move(const CliErrMsgFromCtx *args)
{
    return platform_file_relocated(args->from_text, args->to);
}

int cli_file_append(const CliPathBytesCtx *args)
{
    return args->fresh ? platform_file_written(args->path, args->bytes, args->size)
                       : platform_file_appended(args->path, args->bytes, args->size);
}
