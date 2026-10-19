// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve.h: the tree's side of the window: every command the page calls that reads or changes the tree, runs its jobs,
// its tests and its profiles, or speaks to its language servers and its debugger, called by name with its arguments
// as JSON and answered as JSON, with what each tells as it goes sent as an event by name. The window calls it in its
// own process for a tree on this machine, and over a link for a tree on another, where `orior-cli serve` answers each
// line of its input with a line of its output (serve*.c)
#ifndef SERVE_H
#define SERVE_H

#include "../command_line/cli_internal.h"

// Where what the server tells goes: the event's name and `size` bytes of its body as JSON
typedef void (*CliServeEmit)(void *context, const char *event, const char *body, unsigned long long size);

// the server made for the tree at `root`, none where it is NULL, keeping the runs started over a link in the folder
// `keeping` where it is not NULL; what it tells sent to `emit` from now on, through the comms layer, the server a part
// that speaks each event on the channel of its name and the window a part that listens on every channel; and the
// window opened to a command line run on its tree, a held key that speaks on `command` and listens on `answer/*` kept
// in orior's home with the door and the tree: 1, or 0 with why said
void cli_serve_begun(const char *root, const char *keeping);
void cli_serve_tell_to(CliServeEmit emit, void *context);
int cli_serve_open_to_command_line(CliLine *said);

// the comms layer's door to the container's processes opened, once, its address into `address`, which holds
// CLI_PATH_LONGEST: 1, or 0 with why said; the tree open into `root`, which holds CLI_PATH_LONGEST: 1, or 0 with why
// said, and the same with nothing said; every language server, debug session, kernel and database connection ended,
// as the program ends; and the tree open watched for changes made outside the window, each batch told as
// tree-changed, the servers and the tree's check hearing of the files on a thread of their own
int cli_serve_open_door(char *address, CliLine *said);
int cli_serve_root(char *root, CliLine *said);
int cli_serve_root_now(char *root);
void cli_serve_stop_all(void);
void cli_serve_watch(void);

// the call `name` answered from `size` bytes of its arguments as a JSON object, each found by its own name or by the
// page's, as the page writes names of more than one word, `with_errors` as `withErrors`: its answer as JSON into
// `out`, 1, or 0 with why it cannot said (serve_call*.c)
int cli_serve_call(const char *name, const char *args, unsigned long long size, CliLine *out, CliLine *said);

// the window answered over the standard input and output: each line of input a call, `{id, name, args}`, each
// answered on a thread of the server's by a line `{id, ok}` or `{id, error}` once it is ready, and what the server
// tells sent as lines `{event, body}` as it comes; ended, every server and session with it, once the input ends and
// the calls made before are answered (serve_lines.c)
void cli_serve(const char *root);
// Where a server's lines come from and go over a link: a line read into `line`, which holds `room`, as much of it as
// fits and a 0 after, as snprintf writes: its whole length, `room` or more where it was cut, or QRY_MEMBER_UNREAD once
// the input ends; `size` bytes written whole; and their context
typedef struct
{
    unsigned long long (*read)(void *context, char *line, unsigned long long room);
    void (*write)(void *context, const char *bytes, unsigned long long size);
    void *context;
} CliServeLink;
// the same over `link` in place of the standard input and output (serve_lines.c)
void cli_serve_over(const char *root, const CliServeLink *link);

// the kept run `base` run as its job file says, every line it says added to its said file as it comes, and stopped
// where its stop file is made; `orior-cli keep-run` runs this, a process of its own that outlives the server that
// started it: the code to exit with (serve_kept_run.c)
int cli_serve_keep_run(const char *base);

#endif
