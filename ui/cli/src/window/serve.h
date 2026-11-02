// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve.h: the tree's side of the window: every command the page calls that reads or changes the tree, runs its jobs,
// its tests and its profiles, or communicates with its language servers and its debugger, called by name with its
// arguments as JSON and answered as JSON, with what each emits as it goes sent as an event by name. The window calls it
// in its own process for a tree on this machine, and over a link for a tree on another, where `orior_cli serve` answers
// each line of its input with a line of its output (serve*.c)
#ifndef SERVE_H
#define SERVE_H
typedef struct CliServeErrMsgCfg CliServeErrMsgCfg;
typedef struct CliServeRootCfg CliServeRootCfg;
typedef struct CliServeEmitCfg CliServeEmitCfg;
typedef struct CliServeErrMsgAddressCfg CliServeErrMsgAddressCfg;
typedef struct CliServeRootLinkCfg CliServeRootLinkCfg;
typedef struct CliServeErrMsgRootCfg CliServeErrMsgRootCfg;
typedef struct CliServeCfg CliServeCfg;

#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// Where what the server emits goes: the event's name and `size` bytes of its body as JSON
typedef void (*CliServeEmit)(void *context, const char *event, const char *body, unsigned long long size);

// the server made for the tree at `root`, none where it is NULL, retaining the runs started over a link in the folder
// `retaining` where it is not NULL; what it emits sent to `emit` from now on, through the comms layer, the server a
// part that sends each event on the channel of its name and the window a part that listens on every channel; and the
// window opened to a command line run on its tree, a persistent key that sends on `command` and listens on `answer/*`
// stored in orior's home with the door and the tree: 1, or 0 with why given
void cli_serve_begin(const CliServeRootCfg *args);
void cli_serve_emit_set(const CliServeEmitCfg *args);
int cli_serve_open_to_command_line(const CliServeErrMsgAddressCfg *args);

// the comms layer's door to the container's processes opened, once, its address into `address`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 with why given; the tree open into `root`, sized for CLI_PATH_MAX_LEN: 1, or 0 with why
// given, and the same with nothing given; every language server, debug session, kernel and database connection ended,
// as the program ends; and the tree open watched for changes made outside the window, each batch reported as
// tree-changed, the servers and the tree's check receiving of the files on a thread of their own
int cli_serve_open_door(const CliServeErrMsgAddressCfg *args);
int cli_serve_root(const CliServeErrMsgRootCfg *args);
int cli_serve_root_now(const CliServeErrMsgRootCfg *args);
void cli_serve_stop_all(void);
void cli_serve_watch(void);

// the call `name` answered from `size` bytes of its arguments as a JSON object, each found by its own name or by the
// page's, as the page writes names of more than one word, `with_errors` as `withErrors`: its answer as JSON into
// `out`, 1, or 0 with why it cannot given (serve_call*.c)
int cli_serve_call(const CliServeErrMsgCfg *args);

// the window answered over the standard input and output: each line of input a call, `{id, name, args}`, each
// answered on a thread of the server's by a line `{id, ok}` or `{id, error}` once it is ready, and what the server
// emits sent as lines `{event, body}` as it comes; ended, every server and session with it, once the input ends and
// the calls made before are answered (serve_lines.c)
void cli_serve(const CliServeRootCfg *args);
// Where a server's lines come from and go over a link: a line read into `line`, sized for `room`, as much of it as
// fits and a 0 after, as snprintf writes: its whole length, `room` or more where it was cut, or QRY_MEMBER_UNREAD once
// the input ends; `size` bytes written whole; and their context
typedef struct
{
    unsigned long long (*read)(const CliEachLineRoomCfg *args);
    void (*write)(const CliEachBytesSizeCfg *args);
    void *context;
} CliServeLink;
// the same over `link` in place of the standard input and output (serve_lines.c)
void cli_serve_over(const CliServeRootLinkCfg *args);

// the detached run `base` run as its job file specifies, every line it prints added to its output file as it comes, and
// stopped where its stop file is made; `orior_cli detached-run` runs this, a process of its own that outlives the
// server that started it: the code to exit with (serve_detached_run.c)
int cli_serve_detached_run(const CliServeCfg *args);

struct CliServeErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *name;
    const char *request;
    unsigned long long size;
    VerbaLinea *out;
};

struct CliServeRootCfg
{
    const char *root;
    const char *detached_dir;
};

struct CliServeEmitCfg
{
    CliServeEmit emit;
    void *context;
};

struct CliServeErrMsgAddressCfg
{
    VerbaLinea *err_msg;
    char *address;
};

struct CliServeRootLinkCfg
{
    const char *root;
    const CliServeLink *link;
};

struct CliServeErrMsgRootCfg
{
    VerbaLinea *err_msg;
    char *root_out;
};

struct CliServeCfg
{
    const char *base;
};

typedef struct
{
    void (*begin)(const CliServeRootCfg *args);
    void (*serve)(const CliServeRootCfg *args);
} CliServeRootNs;
EMBED_TABLE_LAYOUT(CliServeRootNs, begin, serve);
EMBED_TABLE_STORAGE CliServeRootNs cli_serve_root_table EMBED_UNUSED = {
    .begin = cli_serve_begin,
    .serve = cli_serve,
};

typedef struct
{
    void (*emit_set)(const CliServeEmitCfg *args);
} CliServeEmitNs;
EMBED_TABLE_LAYOUT(CliServeEmitNs, emit_set);
EMBED_TABLE_STORAGE CliServeEmitNs cli_serve_emit EMBED_UNUSED = {
    .emit_set = cli_serve_emit_set,
};

typedef struct
{
    int (*open_to_command_line)(const CliServeErrMsgAddressCfg *args);
    int (*open_door)(const CliServeErrMsgAddressCfg *args);
} CliServeErrMsgAddressNs;
EMBED_TABLE_LAYOUT(CliServeErrMsgAddressNs, open_to_command_line, open_door);
EMBED_TABLE_STORAGE CliServeErrMsgAddressNs cli_serve_err_msg_address EMBED_UNUSED = {
    .open_to_command_line = cli_serve_open_to_command_line,
    .open_door = cli_serve_open_door,
};

typedef struct
{
    int (*root)(const CliServeErrMsgRootCfg *args);
    int (*root_now)(const CliServeErrMsgRootCfg *args);
} CliServeErrMsgRootNs;
EMBED_TABLE_LAYOUT(CliServeErrMsgRootNs, root, root_now);
EMBED_TABLE_STORAGE CliServeErrMsgRootNs cli_serve_err_msg_root EMBED_UNUSED = {
    .root = cli_serve_root,
    .root_now = cli_serve_root_now,
};

typedef struct
{
    int (*call)(const CliServeErrMsgCfg *args);
} CliServeErrMsgNs;
EMBED_TABLE_LAYOUT(CliServeErrMsgNs, call);
EMBED_TABLE_STORAGE CliServeErrMsgNs cli_serve_err_msg EMBED_UNUSED = {
    .call = cli_serve_call,
};

typedef struct
{
    void (*over)(const CliServeRootLinkCfg *args);
} CliServeRootLinkNs;
EMBED_TABLE_LAYOUT(CliServeRootLinkNs, over);
EMBED_TABLE_STORAGE CliServeRootLinkNs cli_serve_root_link EMBED_UNUSED = {
    .over = cli_serve_over,
};

typedef struct
{
    int (*detached_run)(const CliServeCfg *args);
} CliServeNs;
EMBED_TABLE_LAYOUT(CliServeNs, detached_run);
EMBED_TABLE_STORAGE CliServeNs cli_serve_table EMBED_UNUSED = {
    .detached_run = cli_serve_detached_run,
};

#endif
