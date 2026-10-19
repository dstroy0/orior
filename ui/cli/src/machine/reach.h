// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach.h: the windows open, as the command line finds them: each window keeps a file under windows/ in orior's home,
// readable by the reader alone, naming its process, the tree it has open, its comms door and a held key of the door's
// that grants the channel `command` to speak on and `answer/*` to listen on. A command line run on a tree with a window
// open says the command there, and the window runs it as the menu runs it and answers on `answer/` and the command's
// id. A file whose window is gone is taken away by the first command line that finds the door shut (reach.c)
#ifndef REACH_H
#define REACH_H

#include "../command_line/cli_internal.h"

// What the window answered: whether the command ran, and what it said
typedef struct
{
    int done;
    char said[CLI_SAID_LONGEST];
} CliReachAnswer;

// what a window keeps for the command line, its process `process`, the tree `root` it has open, none where it is NULL,
// its door's `address` and `key`, kept in `home` in a file the reader alone can read; the same of this window in
// orior's home; and this window's file taken away, as it closes. Each 1, or 0 with why said
int cli_reach_keep_in(const char *home, unsigned int process, const char *root, const char *address, const char *key,
                      CliLine *said);
int cli_reach_keep(const char *root, const char *address, const char *key, CliLine *said);
void cli_reach_forget(void);

// `command` of `menu`, with `args`, `count` of them, said to a window open on `root` among those kept in `home`, or,
// with no tree named, to the one window open where there is only one, its answer into `answer`: 1, 0 with why said,
// or -1 where no window is open there; and the same in orior's home
int cli_reach_ask_in(const char *home, const char *root, const char *menu, const char *command, const char *const *args,
                     unsigned int count, CliReachAnswer *answer, CliLine *said);
int cli_reach_ask(const char *root, const char *menu, const char *command, const char *const *args, unsigned int count,
                  CliReachAnswer *answer, CliLine *said);

#endif
