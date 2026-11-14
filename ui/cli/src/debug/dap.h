// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dap.h: a client of a debug adapter, such as lldb-dap, gdb's or debugpy's, over its input and output: each message a
// Content-Length header and a JSON body, as the Debug Adapter Protocol frames them, the same framing a language server
// uses. A request is answered by a response that names its sequence number; an event goes to the function the adapter
// was started with. The adapter is a program started for the session, one listening at an address that the client
// connects to, or one that runs in impero itself, framed the same way. A request from the adapter, such as one to
// run the program in a terminal, is refused, and the adapter then runs it itself (dap.c, dap_start.c, dap_waiters.c)
#ifndef DAP_H
#define DAP_H
#include "../command_line/cli_internal.h"
#include "../servers/lsp.h"
#include "embed_dispatch_layout.h"

typedef struct CliDapProgramCfg CliDapProgramCfg;
typedef struct CliDapErrMsgCfg CliDapErrMsgCfg;
typedef struct CliDapEndsProgramCfg CliDapEndsProgramCfg;
typedef struct CliDapErrMsgCommandCfg CliDapErrMsgCommandCfg;
typedef struct CliDapErrMsgCommandSeqCfg CliDapErrMsgCommandSeqCfg;
typedef struct CliDapEmitCfg CliDapEmitCfg;
typedef struct CliDapMessageCfg CliDapMessageCfg;
typedef struct CliDapCfg CliDapCfg;

// the max adapters open at once, and the requests that wait on answers at once
#define CLI_DAPS_MAX 4u
#define CLI_DAP_WAITERS 16u

// A debug adapter a session connects to
typedef struct CliDap CliDap;

// an event from the adapter: its name and `size` bytes of its body as JSON
typedef void (*CliDapEventFn)(void *context, const char *event, const char *body, unsigned long long size);

typedef struct
{
    void *context;
    CliDap *dap;
    const char *request;
    unsigned long long size;
} CliDapServeCfg;

// an adapter that runs in orior: passed `size` bytes of a request, it sends the client a message with
// cli_dap_send and passes itself a message of its own with cli_dap_itself; and reports whether it goes on
typedef int (*CliDapServe)(const CliDapServeCfg *args);

// `program` started with `args`, `count` of them, in `folder`, its PATH `path`, as the adapter, `emit` taking its
// events, ending the session ending the program where `ends_program`; the adapter listening at `host`:`port` connected
// to, within 5 s; and an adapter that runs in impero started on a thread of its own, `serve` passed each request.
// Each referenced once for its starter, or NULL with why given. An adapter referenced once more, and a reference given
// back, the adapter ended with the last (dap_start.c, dap_within.c, dap.c)
CliDap *cli_dap_start(const CliDapProgramCfg *args);
CliDap *cli_dap_connect(const CliDapEndsProgramCfg *args);
CliDap *cli_dap_within(const CliDapEmitCfg *args);
void cli_dap_ref(const CliDapCfg *args);
void cli_dap_unref(const CliDapCfg *args);

// what an adapter in impero sends to the client, and passes itself, `size` bytes of a message's JSON: 1, or 0 where
// it could not, as where the client let it go (dap_within.c)
int cli_dap_send(const CliDapMessageCfg *args);
int cli_dap_itself(const CliDapMessageCfg *args);

// a request of `command` with `length` bytes of JSON arguments, an empty object for none, sent, its sequence number
// into `seq`; its answer waited for up to `wait` milliseconds and passed over in `answer`, whose message the caller
// gives back to g_cli_debug_owner, NULL where the request failed; and the two in turn. Each 1, or 0 with why given. The
// last line the adapter wrote to its errors, which reports why where it stops; and the adapter requested to end the
// program, or to let go of one it attached to, and itself, and ended where it does not (dap.c)
int cli_dap_on_send(const CliDapErrMsgCommandCfg *args);
int cli_dap_wait(const CliDapErrMsgCommandSeqCfg *args);
int cli_dap_request(const CliDapErrMsgCfg *args);
// whether the request of sequence number `seq` sent has its answer, without waiting and with the answer still waited
// for after: 1 where it succeeded, 0 with why given where it failed, -1 where it has none yet; and the request let go
// of, its answer passed over when it comes (dap_waiters.c)
int cli_dap_answer(const CliDapErrMsgCommandSeqCfg *args);
void cli_dap_forgotten(const CliDapErrMsgCommandSeqCfg *args);
void cli_dap_complaint(const CliDapCfg *args);
void cli_dap_stop(const CliDapCfg *args);

struct CliDapProgramCfg
{
    const char *program;
    const char *const *request;
    unsigned int count;
    const char *folder;
    const char *path;
    int ends_program;
    CliDapEventFn emit;
    void *context;
    VerbaLinea *err_msg;
};

struct CliDapErrMsgCfg
{
    VerbaLinea *err_msg;
    CliDap *dap;
    const char *command;
    const char *arguments;
    unsigned long long length;
    unsigned long long wait;
    CliLspAnswer *answer;
};

struct CliDapEndsProgramCfg
{
    int ends_program;
    CliDapEventFn emit;
    void *context;
    VerbaLinea *err_msg;
    const char *host;
    unsigned int port;
};

struct CliDapErrMsgCommandCfg
{
    VerbaLinea *err_msg;
    CliDap *dap;
    const char *command;
    const char *arguments;
    unsigned long long length;
    unsigned long long *seq_pointer;
};

struct CliDapErrMsgCommandSeqCfg
{
    VerbaLinea *err_msg;
    CliDap *dap;
    const char *command;
    unsigned long long seq;
    unsigned long long wait;
    CliLspAnswer *answer;
};

struct CliDapEmitCfg
{
    CliDapEventFn emit;
    void *context;
    VerbaLinea *err_msg;
    CliDapServe serve;
    void *serve_context;
};

struct CliDapMessageCfg
{
    CliDap *dap;
    const char *message;
    unsigned long long size;
};

struct CliDapCfg
{
    CliDap *dap;
    VerbaLinea *out;
};

typedef struct
{
    CliDap *(*start)(const CliDapProgramCfg *args);
} CliDapProgramNs;
EMBED_TABLE_LAYOUT(CliDapProgramNs, start);
EMBED_TABLE_STORAGE CliDapProgramNs cli_dap_program EMBED_UNUSED = {
    .start = cli_dap_start,
};

typedef struct
{
    CliDap *(*connect)(const CliDapEndsProgramCfg *args);
} CliDapEndsProgramNs;
EMBED_TABLE_LAYOUT(CliDapEndsProgramNs, connect);
EMBED_TABLE_STORAGE CliDapEndsProgramNs cli_dap_ends_program EMBED_UNUSED = {
    .connect = cli_dap_connect,
};

typedef struct
{
    CliDap *(*within)(const CliDapEmitCfg *args);
} CliDapEmitNs;
EMBED_TABLE_LAYOUT(CliDapEmitNs, within);
EMBED_TABLE_STORAGE CliDapEmitNs cli_dap_emit EMBED_UNUSED = {
    .within = cli_dap_within,
};

typedef struct
{
    void (*ref)(const CliDapCfg *args);
    void (*unref)(const CliDapCfg *args);
    void (*complaint)(const CliDapCfg *args);
    void (*stop)(const CliDapCfg *args);
} CliDapNsTable;
EMBED_TABLE_LAYOUT(CliDapNsTable, ref, unref, complaint, stop);
EMBED_TABLE_STORAGE CliDapNsTable cli_dap EMBED_UNUSED = {
    .ref = cli_dap_ref,
    .unref = cli_dap_unref,
    .complaint = cli_dap_complaint,
    .stop = cli_dap_stop,
};

typedef struct
{
    int (*send)(const CliDapMessageCfg *args);
    int (*itself)(const CliDapMessageCfg *args);
} CliDapMessageNs;
EMBED_TABLE_LAYOUT(CliDapMessageNs, send, itself);
EMBED_TABLE_STORAGE CliDapMessageNs cli_dap_message EMBED_UNUSED = {
    .send = cli_dap_send,
    .itself = cli_dap_itself,
};

typedef struct
{
    int (*send)(const CliDapErrMsgCommandCfg *args);
} CliDapErrMsgCommandNs;
EMBED_TABLE_LAYOUT(CliDapErrMsgCommandNs, send);
EMBED_TABLE_STORAGE CliDapErrMsgCommandNs cli_dap_err_msg_command EMBED_UNUSED = {
    .send = cli_dap_on_send,
};

typedef struct
{
    int (*wait)(const CliDapErrMsgCommandSeqCfg *args);
    int (*answer)(const CliDapErrMsgCommandSeqCfg *args);
    void (*forgotten)(const CliDapErrMsgCommandSeqCfg *args);
} CliDapErrMsgCommandSeqNs;
EMBED_TABLE_LAYOUT(CliDapErrMsgCommandSeqNs, wait, answer, forgotten);
EMBED_TABLE_STORAGE CliDapErrMsgCommandSeqNs cli_dap_err_msg_command_seq EMBED_UNUSED = {
    .wait = cli_dap_wait,
    .answer = cli_dap_answer,
    .forgotten = cli_dap_forgotten,
};

typedef struct
{
    int (*request)(const CliDapErrMsgCfg *args);
} CliDapErrMsgNs;
EMBED_TABLE_LAYOUT(CliDapErrMsgNs, request);
EMBED_TABLE_STORAGE CliDapErrMsgNs cli_dap_err_msg EMBED_UNUSED = {
    .request = cli_dap_request,
};

#endif
