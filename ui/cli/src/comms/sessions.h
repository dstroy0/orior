// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sessions.h: the windows on other hosts reached as text. A session is a process started over a link, the far end,
// `imp sessions relay`, its input and output piped back: a door command written to it is sent to the window open on
// that host, as the command line sends one, and the window's answer comes back on the output. One window holds every
// session and sends one command to all of them at once, each host's answer read apart and reported under the host's
// name. Nothing but text goes over the link: no display, no pixels, no keys.
//
// A frame on either pipe is a 4-byte little-endian length and that many bytes. A request's bytes are its id, menu,
// command and words, each ended by a 0, as the door's own are; an answer's are the id ended by a 0, then
// CLI_REACH_DONE or CLI_REACH_REFUSED, then its text. One request is in flight on a session at a time.
//
// With the git option a session carries the tree too. This window's tree is served over git's own protocol from a
// file server started here, git daemon on a port of the loopback, and over ssh that port is led to the far host. The
// far end clones the tree into the address's folder where none is there, or names this server as the remote
// `session` where one is, and `imp sessions pull [ref]` run there fetches a state of this tree and checks it out:
// every host holds the commit named, nothing is sent twice and no diff is made (sessions.c)
#ifndef SESSIONS_H
#define SESSIONS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliSessionsCfg CliSessionsCfg;

// the bytes a frame's body takes at most, and the milliseconds the sender waits for every host's answer
#define CLI_SESSIONS_FRAME (1u << 16u)
#define CLI_SESSIONS_WAIT_MS 30000ULL

// A session joined to `address`, `user@host[:folder]`, `docker:container:folder` or `wsl:distribution:folder`, the
// folder the tree the far window is open on, none for the one window open there, with `count` `request` words after
// it, `--git` among them where the tree at `root` is served to the host and the folder takes its clone: 1, or 0 with
// why given. The same with the program and its `words` that run the far end given, ended by a NULL, `address` the
// host's name as it is reported
int cli_sessions_join(const CliSessionsCfg *args);
int cli_sessions_join_words(const CliSessionsCfg *args);
// the tree at `root` served from this machine, the file server started where it is not up, its address as the far
// host reaches it over the `address` way, CLI_LINK_SSH and the rest, into `out`: 1, or 0 with why given
int cli_sessions_served(const CliSessionsCfg *args);
// On the far host: the state `request[0]` of the served tree, HEAD where `count` is 0, fetched from the remote
// `session` of the tree at `root` and checked out, the commit's hash into `out`: 1, or 0 with what git reported
int cli_sessions_pull(const CliSessionsCfg *args);
// the session on `address` ended and forgotten: 1, or 0 with why given where there is none
int cli_sessions_leave(const CliSessionsCfg *args);
// every session's address on a line of `out`, ` ended` after one whose far end ended; `no sessions` where there is none
void cli_sessions_list(const CliSessionsCfg *args);
// `command` of `menu` with `count` `request` words sent to every session: 1, or 0 with why given where there is no
// session or a session took no request. The answer gathers as the window steps
int cli_sessions_send(const CliSessionsCfg *args);
// whether a sent command's answer is still gathered or a list, join or leave waits to be answered; and the answer
// once it is whole or CLI_SESSIONS_WAIT_MS passed, into `out` with `done` 1 where every host answered that it ran:
// 1, or 0 where nothing waits or the answer is not whole yet. Each host's line is its address, `: ` and its text,
// ` refused: ` and its text where the far window refused, or ` gave no answer`
int cli_sessions_pending(void);
int cli_sessions_step(const CliSessionsCfg *args);
// a refusal waiting to be answered, its text `err_msg`'s: for a join, leave or send the window refused before it began
void cli_sessions_refused(const CliSessionsCfg *args);
// every session ended and forgotten
void cli_sessions_close(void);
// The far end: requests read from this process's input until it ends, each run as impero runs a command line, on the
// tree `root` where one is named, its answer written to the output. Where `address` names a served tree, the tree is
// cloned into `root` first, or the server named as its remote `session`: 1 as the input ends, or 0 with what git
// reported where the tree could not be made ready
int cli_sessions_relay(const CliSessionsCfg *args);

struct CliSessionsCfg
{
    const char *address;
    const char *root;
    const char *menu;
    const char *command;
    const char *const *request;
    const char *const *words;
    unsigned int count;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    int *done;
};

typedef struct
{
    int (*join)(const CliSessionsCfg *args);
    int (*join_words)(const CliSessionsCfg *args);
    int (*leave)(const CliSessionsCfg *args);
    void (*list)(const CliSessionsCfg *args);
    int (*send)(const CliSessionsCfg *args);
    int (*step)(const CliSessionsCfg *args);
    void (*refused)(const CliSessionsCfg *args);
    int (*served)(const CliSessionsCfg *args);
    int (*pull)(const CliSessionsCfg *args);
    int (*relay)(const CliSessionsCfg *args);
} CliSessionsNs;
EMBED_TABLE_LAYOUT(CliSessionsNs, join, join_words, leave, list, send, step, refused, served, pull, relay);
EMBED_TABLE_STORAGE CliSessionsNs cli_sessions EMBED_UNUSED = {
    .join = cli_sessions_join,
    .join_words = cli_sessions_join_words,
    .leave = cli_sessions_leave,
    .list = cli_sessions_list,
    .send = cli_sessions_send,
    .step = cli_sessions_step,
    .refused = cli_sessions_refused,
    .served = cli_sessions_served,
    .pull = cli_sessions_pull,
    .relay = cli_sessions_relay,
};
#endif
