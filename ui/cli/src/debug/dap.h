// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap.h: a client of a debug adapter, such as lldb-dap, gdb's or debugpy's, over its input and output: each message a
// Content-Length header and a JSON body, as the Debug Adapter Protocol frames them, the same framing a language server
// uses. A request is answered by a response that names its sequence number; an event goes to the function the adapter
// was started with. The adapter is a program started for the session, one listening at an address that the client
// connects to, or one that runs in orior_cli itself, framed the same way. A request from the adapter, such as one to run
// the program in a terminal, is refused, and the adapter then runs it itself (dap.c, dap_start.c,
// dap_waiters.c)
#ifndef DAP_H
#define DAP_H

#include "../command_line/cli_internal.h"

// the most adapters held at once, and the requests that wait on answers at once
#define CLI_DAPS_MOST 4u
#define CLI_DAP_WAITERS 16u

// A debug adapter a session speaks to
typedef struct CliDap CliDap;

// an event from the adapter: its name and `size` bytes of its body as JSON
typedef void (*CliDapHeard)(void *context, const char *event, const char *body, unsigned long long size);

// an adapter that runs in orior: handed `size` bytes of a request, it sends the client a message with
// cli_dap_told and hands itself a message of its own with cli_dap_itself; and says whether it goes on
typedef int (*CliDapServe)(void *context, CliDap *dap, const char *request, unsigned long long size);

// `program` started with `args`, `count` of them, in `folder`, its PATH `path`, as the adapter, `heard` taking its
// events, ending the session ending the program where `ends_program`; the adapter listening at `host`:`port` connected
// to, within 5 s; and an adapter that runs in orior_cli started on a thread of its own, `serve` handed each request. Each
// held once for its starter, or NULL with why said. An adapter held once more, and a hold given back, the adapter ended
// with the last (dap_start.c, dap_within.c, dap.c)
CliDap *cli_dap_started(const char *program, const char *const *args, unsigned int count, const char *folder,
                        const char *path, int ends_program, CliDapHeard heard, void *context, CliLine *said);
CliDap *cli_dap_connected(const char *host, unsigned int port, int ends_program, CliDapHeard heard, void *context,
                          CliLine *said);
CliDap *cli_dap_within(CliDapServe serve, void *serve_context, CliDapHeard heard, void *context, CliLine *said);
void cli_dap_held(CliDap *dap);
void cli_dap_given(CliDap *dap);

// what an adapter in orior_cli says to the client, and hands itself, `size` bytes of a message's JSON: 1, or 0 where it
// could not, as where the client let it go (dap_within.c)
int cli_dap_told(CliDap *dap, const char *message, unsigned long long size);
int cli_dap_itself(CliDap *dap, const char *message, unsigned long long size);

// a request of `command` with `length` bytes of JSON arguments, an empty object for none, sent, its sequence number
// into `seq`; its answer waited for up to `wait` milliseconds, its body written into `body`; and the two in turn. Each
// 1, or 0 with why said. The last line the adapter wrote to its errors, which says why where it stops; and the adapter
// asked to end the program, or to let go of one it attached to, and itself, and ended where it does not (dap.c)
int cli_dap_sent(CliDap *dap, const char *command, const char *arguments, unsigned long long length,
                 unsigned long long *seq, CliLine *said);
int cli_dap_waited(CliDap *dap, const char *command, unsigned long long seq, unsigned long long wait, CliLine *body,
                   CliLine *said);
int cli_dap_requested(CliDap *dap, const char *command, const char *arguments, unsigned long long length,
                      unsigned long long wait, CliLine *body, CliLine *said);
// whether the request of sequence number `seq` sent has its answer, without waiting and with the answer still waited
// for after: 1 where it succeeded, 0 with why said where it failed, -1 where it has none yet; and the request let go
// of, its answer passed over when it comes (dap_waiters.c)
int cli_dap_answered(CliDap *dap, unsigned long long seq, CliLine *said);
void cli_dap_forgotten(CliDap *dap, unsigned long long seq);
void cli_dap_complaint(CliDap *dap, CliLine *out);
void cli_dap_stopped(CliDap *dap);

#endif
