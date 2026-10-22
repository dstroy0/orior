// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.h: what the window calls of orior, declared by it `extern "C"`: the command line run on the words the
// program was started with, and the window opened on the tree it names; every call of the page, the tree's answered
// over the link where the tree is on another machine and here where it is not, and the window's own answered here,
// each by name with its arguments and its answer as JSON; a terminal's key to the comms layer; the processes the window
// started; and all of it ended as the window ends (window.c)
#ifndef WINDOW_H
#define WINDOW_H

#include "../command_line/cli_internal.h"
#include "serve.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // the calls answered at once, the bytes one's answer is begun in before it grows, and the most bytes of why one
    // failed
#define CLI_WINDOW_ROOMS 8u
#define CLI_WINDOW_ANSWER_FIRST (64ull << 10u)
#define CLI_WINDOW_SAID (1ull << 16u)

    // the command line run on `count` words: the code to exit with, 1 in `window` where the words open the window
    // instead, the window to open kept for cli_window_begun
    int cli_window_command_line(const char *const *words, unsigned int count, int *window);

    // the window opened as the command line left it, or on the tree found where it left none: orior_cli run on the
    // machine where it is in a Flatpak's sandbox, every process it starts held under it, the tree's side made with
    // what it tells sent to `emit`, its door opened, the command line let reach it, the tree watched, and the link
    // opened where the tree is on another machine, what it tells sent to `emit` too; and every server, session,
    // link and process of the window's ended, as it ends
    void cli_window_begun(CliServeEmit emit, void *context);
    void cli_window_ended(void);

    // the call `name` of `size` bytes of JSON `args` answered, the window's own where `own` is 1, else the tree's:
    // the room it was answered in; its answer, or why it failed, at `text`, `size` bytes, 1 where it is an answer;
    // and the room given back
    unsigned int cli_window_called(int own, const char *name, const char *args, unsigned long long size);
    int cli_window_answer(unsigned int room, const char **text, unsigned long long *size);
    void cli_window_given(unsigned int room);

    // a terminal's part of the comms layer: a key that listens and speaks on `runs/*`, into `key`, which holds
    // CLI_COMMS_KEY, and the door's address into `address`, which holds CLI_PATH_LONGEST: 1, or 0 where there is
    // no door; the part bound to the shell's process; and the part let go as the shell ends
    int cli_window_terminal_key(char *key, char *address);
    void cli_window_terminal_bound(const char *key, unsigned int process);
    void cli_window_terminal_left(const char *key);

#ifdef __cplusplus
}
#endif

#endif
