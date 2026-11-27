// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_container.c: the command line's calls on macOS: the system's processes listed through proc_listpids, this
// program's path and the words it was started with as the system saved them, and a file copied with the reader's
// rights raised by osascript
#if defined(__APPLE__)
// libproc and the saved words of the program are the system's, outside strict C11
#define _DARWIN_C_SOURCE
#endif
#include "../../../../ui/cli/src/command_line/callback_cfg.h"
#include "../../../../ui/cli/src/comms/container.h"
#include "../../../../ui/cli/src/text/text.h"
#include "../posix_systems.h"

#if defined(__APPLE__)
#include <crt_externs.h>
#include <libproc.h>
#include <mach-o/dyld.h>

unsigned int cli_processes_list(const CliCapacityProcessesCtx *args)
{
    unsigned int count = 0u;
    static int s_ids[8192];
    const int list_entries = proc_listpids(PROC_ALL_PIDS, 0u, s_ids, (int)sizeof(s_ids));
    for (int at = 0; (list_entries > 0) && (at < (list_entries / (int)sizeof(int))) && (count < args->capacity_u32);
         at += 1)
    {
        struct proc_bsdinfo info;
        if (proc_pidinfo(s_ids[at], PROC_PIDTBSDINFO, 0u, &info, (int)sizeof(info)) == (int)sizeof(info))
        {
            // an id the system lists is positive
            args->processes[count].id = (unsigned int)s_ids[at];
            args->processes[count].parent = info.pbi_ppid;
            count += 1u;
        }
    }
    return count;
}

int cli_program_own(const CliPathCapacityCtx *args)
{
    // a capacity is far below what a embed_u32 counts
    embed_u32 size = (embed_u32)args->capacity_bytes;
    return _NSGetExecutablePath(args->path_out, &size) == 0;
}

unsigned int cli_arguments_own_read(const char **words, unsigned int capacity)
{
    unsigned int reserved = 0u;
    const int count = *_NSGetArgc();
    char **const supplied = *_NSGetArgv();
    for (int at = 1; (at < count) && (reserved < capacity); at += 1)
    {
        words[reserved] = supplied[at];
        reserved += 1u;
    }
    return reserved;
}

int cli_raised_platform(const CliErrMsgFromCtx *args)
{
    // the script `do shell script "/bin/cp '<from>' '<to>'" with administrator privileges`, each path quoted for the
    // shell and the script's quotes escaped
    static char s_script[(4u * CLI_PATH_MAX_LEN) + 128u];
    VerbaLinea script = {s_script, sizeof(s_script), 0ULL};
    mmgr_line_put(&script, "do shell script \"/bin/cp ");
    const char *const paths[2] = {args->from_text, args->to};
    for (unsigned int path = 0u; path < 2u; path += 1u)
    {
        mmgr_line_put(&script, (path == 0u) ? "'" : " '");
        const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = paths[path], .cap = CLI_POOL_BYTES});
        for (unsigned long long at = 0ULL; at < whole;)
        {
            const unsigned long long stop = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                       .text = paths[path], .at = at, .size = whole, .set = "'\"");
            mmgr_line_put_n(&script, paths[path] + at, (embed_word)(stop - at));
            if (stop < whole)
            {
                mmgr_line_put(&script, (paths[path][stop] == '\'') ? "'\\\\''" : "\\\"");
            }
            at = stop + 1ULL;
        }
        mmgr_line_ch(&script, '\'');
    }
    mmgr_line_put(&script, "\" with administrator privileges");
    if (mmgr_line_finish(&script) == 0ULL)
    {
        return 0;
    }
    const char *const words[4] = {"/usr/bin/osascript", "-e", s_script, NULL};
    PlatformProgram child;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &child, .words = words,
                    .folder = NULL, .output = QUERY_OUTPUT_NONE) ||
        (EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &child) != 0LL))
    {
        mmgr_line_put(args->err_msg, "the system's authorization did not write ");
        mmgr_line_put(args->err_msg, args->to);
        return 0;
    }
    return 1;
}
#endif
