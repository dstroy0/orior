// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window.h: what the window calls of orior, declared by it `extern "C"`: the command line run on the words the
// program was started with, and the window opened on the tree it names; every call of the page, the tree's answered
// over the link where the tree is on another machine and here where it is not, and the window's own answered here,
// each by name with its arguments and its answer as JSON; the web view's browser process; the window's place, its
// page's first script and its title bar; a panic filed; and all of it ended as the window ends (window.c)
#ifndef WINDOW_H
#define WINDOW_H

#include "../command_line/cli_internal.h"
#include "serve.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // the calls answered at once, the bytes one's answer is written in, and the most bytes of why one failed
#define CLI_WINDOW_ROOMS 8u
#define CLI_WINDOW_ANSWER_ROOM (4ull << 20u)
#define CLI_WINDOW_ERR_MSG_MAX_LEN (1ull << 16u)

    // the command line run on the words the program was started with, as the system keeps them: the code to exit with,
    // 1 in `window` where the words open the window instead, the window to open recorded for cli_window_begun
    int cli_window_command_line(int *window);

    // the window opened as the command line left it, or on the tree found where it left none: orior_cli run on the
    // machine where it is in a Flatpak's sandbox, every process it starts contained under it, the tree's side made with
    // what it emits sent to `emit`, its door opened, the command line let reach it, the tree watched, and the link
    // opened where the tree is on another machine, what it emits sent to `emit` too; and every server, session,
    // link and process of the window's ended, as it ends
    void cli_window_begun(CliServeEmit emit, void *context);
    void cli_window_ended(void);

    // the call `name` of `size` bytes of JSON `args` answered: a name of the window's own here, whatever `own` is,
    // and any other name the tree's where `own` is 0, refused where it is 1: the room it was answered in; its answer,
    // or why it failed, at `text`, `size` bytes, 1 where it is an answer; and the room given back. `own` is the
    // lane the window called on: its own calls run in order, the rest at once
    unsigned int cli_window_called(int own, const char *name, const char *args, unsigned long long size);
    int cli_window_answer(unsigned int room, const char **text, unsigned long long *size);
    void cli_window_given(unsigned int room);

    // the web view's browser process named, for what the window holds to count it with every process under it
    // (window_call_memory.c)
    void cli_window_browser_known(unsigned int pid);

    // the page's first script, which puts what orior's folder keeps into the page's storage before the page's own
    // scripts run, on the first load of the main window's page in a run, in place of every entry the page kept before,
    // and where the folder keeps nothing yet tells the page to write all it holds there: `size` bytes of it, NULL where
    // its cell could not be taken; its cell given back once it is read; and whether the reader asked for the system's
    // own title bar (window_call_home.c)
    const char *cli_window_kept_script(unsigned long long *size);
    void cli_window_kept_script_given(void);
    int cli_window_title_bar_system(void);

    // Where a file a page window asks for is handed: the answer's status, its media type, and `size` bytes of its body,
    // each valid while the call runs
    typedef void (*CliWindowServed)(void *context, unsigned int status, const char *media_type,
                                    const unsigned char *bytes, unsigned long long size);

    // the file a page window asks for by `size` bytes of its address's path, its escapes read, handed to `served`: the
    // file under the tree, read where it lies or over the link where the tree is on another machine, or why it is not
    // served, refused outside the tree (window_call_view.c)
    void cli_window_view_served(const char *path, unsigned long long size, CliWindowServed served, void *context);

    // a panic of the window's filed as an error is, `message_size` bytes of its message and `detail_size` of its
    // backtrace, its category by where the backtrace first enters orior's own code (window_call_report.c)
    void cli_window_panicked(const char *message, unsigned long long message_size, const char *detail,
                             unsigned long long detail_size);

    // A place on the screen in the system's own pixels, and whether the window filled its monitor there
    typedef struct
    {
        int x;
        int y;
        unsigned int width;
        unsigned int height;
        int maximized;
    } CliWindowPlace;

    // A monitor's area, in the system's own pixels
    typedef struct
    {
        int x;
        int y;
        unsigned int width;
        unsigned int height;
    } CliWindowArea;

    // the least size the window is dragged to, and a place kept is read back at, in pixels at a scale of one
#define CLI_PLACE_LEAST_WIDTH 960u
#define CLI_PLACE_LEAST_HEIGHT 600u

    // the place kept for the tree open, fitted to the monitors `areas`, `count` of them, the system's main one first,
    // into `place`: 1, or 0 where none is kept, it is below the least size or there is no monitor; and `place` kept for
    // the tree open, beside the places of the other trees, as its window closes, nothing kept where no tree is open, and
    // why it could not be written on the errors (window_place.c)
    int cli_window_place_kept(const CliWindowArea *areas, unsigned int count, CliWindowPlace *place);
    void cli_window_place_keep(const CliWindowPlace *place);

#ifdef __cplusplus
}
#endif

#endif
