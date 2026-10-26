// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve.h: the tree's side of the window: every command the page calls that reads or changes the tree, runs its jobs,
// its tests and its profiles, or communicates with its language servers and its debugger, called by name with its
// arguments as JSON and answered as JSON, with what each emits as it goes sent as an event by name. The window calls it
// in its own process for a tree on this machine, and over a link for a tree on another, where `orior_cli serve` answers
// each line of its input with a line of its output (serve*.c)
#ifndef SERVE_H
#define SERVE_H

#include "../command_line/cli_internal.h"

// Where what the server emits goes: the event's name and `size` bytes of its body as JSON
typedef void (*CliServeEmit)(void *context, const char *event, const char *body, unsigned long long size);

// the server made for the tree at `root`, none where it is NULL, retaining the runs started over a link in the folder
// `retaining` where it is not NULL; what it emits sent to `emit` from now on, through the comms layer, the server a
// part that sends each event on the channel of its name and the window a part that listens on every channel; and the
// window opened to a command line run on its tree, a persistent key that sends on `command` and listens on `answer/*`
// stored in orior's home with the door and the tree: 1, or 0 with why given
void cli_serve_begun(const char *root, const char *detached_dir);
void cli_serve_emit_set(CliServeEmit emit, void *context);
int cli_serve_open_to_command_line(VerbaLinea *err_msg);

// the comms layer's door to the container's processes opened, once, its address into `address`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 with why given; the tree open into `root`, sized for CLI_PATH_MAX_LEN: 1, or 0 with why
// given, and the same with nothing given; every language server, debug session, kernel and database connection ended,
// as the program ends; and the tree open watched for changes made outside the window, each batch reported as
// tree-changed, the servers and the tree's check receiving of the files on a thread of their own
int cli_serve_open_door(char *address, VerbaLinea *err_msg);
int cli_serve_root(char *root, VerbaLinea *err_msg);
int cli_serve_root_now(char *root);
void cli_serve_stop_all(void);
void cli_serve_watch(void);

// the call `name` answered from `size` bytes of its arguments as a JSON object, each found by its own name or by the
// page's, as the page writes names of more than one word, `with_errors` as `withErrors`: its answer as JSON into
// `out`, 1, or 0 with why it cannot given (serve_call*.c)
int cli_serve_call(const char *name, const char *args, unsigned long long size, VerbaLinea *out, VerbaLinea *err_msg);

// the window answered over the standard input and output: each line of input a call, `{id, name, args}`, each
// answered on a thread of the server's by a line `{id, ok}` or `{id, error}` once it is ready, and what the server
// emits sent as lines `{event, body}` as it comes; ended, every server and session with it, once the input ends and
// the calls made before are answered (serve_lines.c)
void cli_serve(const char *root);
// Where a server's lines come from and go over a link: a line read into `line`, sized for `room`, as much of it as
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

// the detached run `base` run as its job file specifies, every line it prints added to its output file as it comes, and
// stopped where its stop file is made; `orior_cli detached-run` runs this, a process of its own that outlives the
// server that started it: the code to exit with (serve_detached_run.c)
int cli_serve_detached_run(const char *base);

#endif
