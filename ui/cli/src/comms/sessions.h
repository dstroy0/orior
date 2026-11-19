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
// CLI_REACH_DONE or CLI_REACH_REFUSED, then its text. One request is in flight on a session at a time (sessions.c)
#ifndef SESSIONS_H
#define SESSIONS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliSessionsCfg CliSessionsCfg;

// the bytes a frame's body takes at most, and the milliseconds the sender waits for every host's answer
#define CLI_SESSIONS_FRAME (1u << 16u)
#define CLI_SESSIONS_WAIT_MS 30000ULL

// A session joined to `address`, `user@host[:folder]`, `docker:container:folder` or `wsl:distribution:folder`, the
// folder the tree the far window is open on, none for the one window open there: 1, or 0 with why given. The same
// with the program and its `words` that run the far end given, ended by a NULL, `address` the host's name as it is
// reported
int cli_sessions_join(const CliSessionsCfg *args);
int cli_sessions_join_words(const CliSessionsCfg *args);
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
// The far end: requests read from this process's input until it ends, each sent to the window open on `root`, or on
// the one tree open where `root` is NULL, its answer written to the output. The exit code, 0
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
    int (*relay)(const CliSessionsCfg *args);
} CliSessionsNs;
EMBED_TABLE_LAYOUT(CliSessionsNs, join, join_words, leave, list, send, step, refused, relay);
EMBED_TABLE_STORAGE CliSessionsNs cli_sessions EMBED_UNUSED = {
    .join = cli_sessions_join,
    .join_words = cli_sessions_join_words,
    .leave = cli_sessions_leave,
    .list = cli_sessions_list,
    .send = cli_sessions_send,
    .step = cli_sessions_step,
    .refused = cli_sessions_refused,
    .relay = cli_sessions_relay,
};
#endif
