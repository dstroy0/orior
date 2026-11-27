// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_tessera_run.c: tessera_run on POSIX: the interrupts left to the command, the processors named, the records
// and the life of the parent. The command's start, its time and its end are the platform's (platform.h)
#if !defined(_WIN32)
// flock and pwrite are outside strict C11. The macro comes before the first header, which fixes the set of calls
// that every later header declares
#define _GNU_SOURCE
#endif
#include "../../../src/cu/engine/runtime/daemon/tessera_run_internal.h"
#include "../platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#if !(defined(_WIN32))

void run_unmeasured_noted(void)
{
    // the note names a job object, which only Windows has, and so POSIX notes nothing
}

void run_interrupts_passed(void)
{
    // an interrupt or a hangup from the terminal reaches the command, which ends. This process waits for it, then
    // releases its job or writes its last record
    (void)platform_signal_handled(SIGINT, SIG_IGN);
    (void)platform_signal_handled(SIGQUIT, SIG_IGN);
    (void)platform_signal_handled(SIGHUP, SIG_IGN);
}

int run_processors_named(unsigned long long processors)
{
    char text[24];
    VerbaLinea formed_line_1 = mmgr_line_over(text, sizeof(text));
    mmgr_line_decimal(&formed_line_1, processors, 1u);
    (void)mmgr_line_finish(&formed_line_1);
    const int written = (int)formed_line_1.at;
    return (written > 0) && platform_environment_set("TESSERA_RUN_PROCESSORS", text);
}

// the parent's record, locked for this process's life and let go with it: how its child finds that it lives. The
// descriptor closes on exec: no child takes the lock with it
int run_record_make(RunChannel *channel, const char *path)
{
    channel->record = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (channel->record < 0)
    {
        return 0;
    }
    if (flock(channel->record, LOCK_EX | LOCK_NB) != 0)
    {
        close(channel->record);
        return 0;
    }
    return 1;
}

void run_record_put(RunChannel *channel, const char *line, size_t length)
{
    const ssize_t written = pwrite(channel->record, line, length, 0);
    (void)written;
}

void run_record_close(RunChannel *channel)
{
    close(channel->record);
}

// 1 while the parent lives. A Windows parent (this child runs in WSL) shares its record only for reading, and an open
// for writing errors while it lives. A Linux parent takes a lock on it. The parent lets either go when it ends
int run_parent_alive(const char *parent)
{
    const int opened = open(parent, O_WRONLY | O_CLOEXEC);
    if (opened < 0)
    {
        return errno != ENOENT;
    }
    const int locked = flock(opened, LOCK_EX | LOCK_NB) == 0;
    close(opened);
    return !locked;
}
#endif
