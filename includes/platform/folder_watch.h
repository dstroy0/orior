// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// folder_watch.h: what the watch of a folder shares with the file of each system that watches it
// (windows/windows_folder_watch.c, posix/linux/linux_folder_watch.c, posix/macos/macos_folder_watch.c): the record of
// one folder kept open, the walk of its tree, and the calls of each system
#ifndef FOLDER_WATCH_H
#define FOLDER_WATCH_H

#include "../../ui/cli/src/command_line/callback_cfg.h"
#include "../../ui/cli/src/command_line/cli_internal.h"

// how long the changes rest before the call, and how often a system without a change notice reads the folder. The max
// depth of the folders under a folder that the watch goes into
#define CLI_FOLDER_WATCH_QUIET_MS 150u
#define CLI_FOLDER_WATCH_READ_MS 2000u
#define CLI_FOLDER_WATCH_DEPTH 8u

// One folder kept open: its path, the call on a change, the thread, and whether it runs. Then whether it must stop,
// whether its thread ended, and the wake that the stop and the end go through. `notified` and `stopped` are the
// system's parts of the watch. Each is a handle or a descriptor that the system's file reads as its own. `stamp` is
// the fold of the files of a system that reads the folder
typedef struct
{
    char folder[CLI_PATH_MAX_LEN];
    void (*changed)(void);
    CliThread thread;
    int running;
    int stop;
    int ended;
    CliWake wake;
    unsigned long long notified;
    unsigned long long stopped;
    unsigned long long stamp;
} CliFolderWatch;

// the folder of `watch` and all under it walked: each folder put under the watch, and the stamp of its files
unsigned long long folder_watch_walked(CliFolderWatch *watch);

// the thread of `watch` ended, and its stopper woken
void folder_watch_ended(CliFolderWatch *watch);

// The calls of each system. The folder at `path` put under the watch of `watch`, where the system watches each
// folder apart. The thread of `watch`, which calls `changed` once the changes rest and ends at the stop. The system's
// parts of `watch` made: 1, or 0 where the system refuses. The stop given to the thread. The system's parts let go
// after the thread ends
void folder_watch_folder_put(CliFolderWatch *watch, const char *path);
void folder_watch_run(void *context);
int folder_watch_opened(CliFolderWatch *watch);
void folder_watch_stop_sent(CliFolderWatch *watch);
void folder_watch_closed(CliFolderWatch *watch);

#endif
