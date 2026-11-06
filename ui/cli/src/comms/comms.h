// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms.h: the container's comms layer: every message between orior's parts goes through it, each on a named channel.
// A part registers and is given a key, drawn from the system's own source of secrets; the key names the channels the
// part may listen on and those it may send on, and the container knows the process each key was given to. A part
// grants the parts it starts their channels, never more than its own. A message on a channel its speaker's key does not
// name is refused and counted, and a listener receives only the channels its key names.
//
// The container tracks each channel's traffic: how many messages and bytes it has carried, how many of those bytes went
// through the transfer layer, how many messages it refused, who listens, and the last few messages, for a view of every
// part and what it sent.
//
// A channel's name is words joined by slashes, as lsp/rust or db/rows. A grant is a name, or a name ending in a slash
// and a star for every channel under it, or * alone for every channel. A body is JSON, as its speaker wrote it
// (comms.c, comms_view.c)
#ifndef COMMS_H
#define COMMS_H
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliCommsNameCfg CliCommsNameCfg;
typedef struct CliCommsChannelCfg CliCommsChannelCfg;
typedef struct CliCommsChannelErrMsgCfg CliCommsChannelErrMsgCfg;
typedef struct CliCommsProcessCfg CliCommsProcessCfg;
typedef struct CliCommsGrantCfg CliCommsGrantCfg;
typedef struct CliCommsKeyCfg CliCommsKeyCfg;
typedef struct CliCommsKeyProcessCfg CliCommsKeyProcessCfg;
typedef struct CliCommsChannelBytesCfg CliCommsChannelBytesCfg;
typedef struct CliCommsInnerCfg CliCommsInnerCfg;
typedef struct CliCommsPaneCfg CliCommsPaneCfg;
typedef struct CliCommsCommandCfg CliCommsCommandCfg;
typedef struct CliCommsListenerCfg CliCommsListenerCfg;

// the bytes of a key written, its 32 bytes in hex and a 0 after; the most grants a part has to listen and to send
#define CLI_COMMS_KEY 65u
#define CLI_COMMS_GRANTS 16u

// A message received on `channel`, from the part named `from`, `size` bytes of its body: 1 where it was received, 0
// where the listener is gone, which lets it go, and -1 where the listener's queue had no capacity for it, which is not
// counted received
typedef int (*CliCommsRecvFn)(const CliCommsRecvCfg *args);

// whether the grant `grant` names the channel `channel`
int cli_comms_grants(const CliCommsGrantCfg *args);
// whether the grant `inner` names only channels the grant `outer` names (comms.c)
int cli_comms_within(const CliCommsInnerCfg *args);

// a part named `name` registered, granted the channels `listen`, `listens` of them, and `send_grant`, `send_grants` of
// them, by the container, its key into `key`, sized for CLI_COMMS_KEY: given to the process `process` where
// `has_process`, else one of the container's own, whose key the door never takes; the same, its key taken by the door
// from any process that owns it, as the key is stored where only the reader can read it; and a part the part of
// `parent` starts, granted channels of the parent's own alone. Each 1, or 0 with why given
int cli_comms_register(const CliCommsNameCfg *args);
int cli_comms_register_persistent(const CliCommsNameCfg *args);
int cli_comms_register_under(const CliCommsNameCfg *args);

// whether the door takes the key of `key`'s part from any process that owns it; the part of `key` given the process
// `process`, once it has started; the part of `key` taken away, with every channel it listened on; the process it was
// registered for into `process`, and its name into `name`, sized for CLI_NAME_MAX_LEN: each 1, or 0 where the key is
// no part's or it was given none
int cli_comms_is_persistent(const CliCommsProcessCfg *args);
void cli_comms_bind(const CliCommsProcessCfg *args);
void cli_comms_leave(const CliCommsProcessCfg *args);
int cli_comms_process_get(const CliCommsKeyProcessCfg *args);
int cli_comms_name_get(const CliCommsKeyCfg *args);

// `key`'s part listening on `channel`, a channel or a grant its key must name, each message sent there from now on
// handed to `recv`: where `queued`, in the sender's call while the container owns its lock, so that it must neither
// wait nor send, as a queue a reader drains is filled; else once the container has let go of its lock, in the
// sender's thread, which preserves the order of what one thread sends. The listener's number into `listener`: 1, or 0
// with why given; and the listener let go
int cli_comms_listen(const CliCommsChannelCfg *args);
void cli_comms_unlisten(const CliCommsListenerCfg *args);

// `size` bytes of `body` sent on `channel` as the part of `key`, how many listeners received it into `delivered`: 1, or
// 0 with why given; and `bytes` of a body on `channel` counted as gone through the transfer layer
int cli_comms_send(const CliCommsChannelErrMsgCfg *args);
void cli_comms_transfer(const CliCommsChannelBytesCfg *args);

struct CliCommsNameCfg
{
    const char *name;
    int has_process;
    unsigned int process;
    const char *const *listen;
    unsigned int listens;
    const char *const *send_grant;
    unsigned int send_grants;
    char *key_out;
    VerbaLinea *err_msg;
    const char *parent;
};

struct CliCommsChannelCfg
{
    const char *channel;
    VerbaLinea *err_msg;
    const char *key;
    int queued;
    CliCommsRecvFn recv;
    void *context;
    unsigned int *listener;
};

struct CliCommsChannelErrMsgCfg
{
    const char *channel;
    VerbaLinea *err_msg;
    const char *key;
    const char *body;
    unsigned long long size;
    unsigned long long *delivered;
};

struct CliCommsProcessCfg
{
    unsigned int process;
    const char *key;
};

struct CliCommsGrantCfg
{
    const char *grant;
    const char *channel;
};

struct CliCommsKeyCfg
{
    const char *key;
    char *name_out;
};

struct CliCommsKeyProcessCfg
{
    const char *key;
    unsigned int *process_pointer;
};

struct CliCommsChannelBytesCfg
{
    const char *channel;
    unsigned long long bytes;
};

struct CliCommsInnerCfg
{
    const char *inner;
    const char *outer;
};

// The Comms pane as the window reads it (comms_view.c): one of four views, Channels, Requests, Network and Report,
// the Channels view current until another is chosen. The Channels view lists a group row Parts with its count, a row
// each part registered by name, a group row Channels with its count, a row each channel that carried or refused a
// message by name, and under a channel opened a row each of its last messages, the oldest first. A part's row carries
// its name, its counts sent and refused as its status, its process and its grants as its detail, and whether it had
// a message refused; a channel's carries its name, its messages and refusals as its status, its bytes, those through
// the transfer layer and the parts that listen as its detail, whether it refused one, whether it is open and whether
// it is the channel last opened or closed; a message's carries its sender as its name and its body as its status.
// Every text is a pointer into the pane's one text cell and a length. The rows are listed again only where the comms
// layer's stamp moved since they were last listed, or where they are read with `force`
#define CLI_COMMS_VIEW_CHANNELS 0u
#define CLI_COMMS_VIEW_REQUESTS 1u
#define CLI_COMMS_VIEW_NETWORK 2u
#define CLI_COMMS_VIEW_REPORT 3u
#define CLI_COMMS_VIEWS 4u
// the head's actions: the view `at` chosen, CLI_COMMS_CHOOSE and on; the layer listed again; and an action of the view
// shown, CLI_COMMS_FORWARD and on; and the milliseconds between two listings while the Channels view is in sight
#define CLI_COMMS_CHOOSE 1u
#define CLI_COMMS_READ 5u
#define CLI_COMMS_FORWARD 64u
#define CLI_COMMS_REFRESH_MS 1000ULL
// a row's kind: a group's, a part's, a channel's, a stored message's
#define CLI_COMMS_ROW_GROUP 0u
#define CLI_COMMS_ROW_PART 1u
#define CLI_COMMS_ROW_CHANNEL 2u
#define CLI_COMMS_ROW_MESSAGE 3u
typedef struct
{
    unsigned int kind;
    const char *name;
    unsigned long long name_length;
    const char *status;
    unsigned long long status_length;
    const char *detail;
    unsigned long long detail_length;
    int refused;
    int open;
    int current;
} CliCommsRow;

// the pane begun, `woken` waking the view's thread as the Requests view's answer comes in; and while the pane is
// `shown`, the Channels view listed again where the layer's stamp moved or where `force`, and the view shown stepped
// on at `now_ms`: 1 where either changed what the pane draws
void cli_comms_pane_begin(const CliCommsPaneCfg *args);
int cli_comms_pane_step(const CliCommsPaneCfg *args);
// the millisecond the Channels view next looks at the layer while `shown`, 0 for none
unsigned long long cli_comms_pane_deadline_ms(const CliCommsPaneCfg *args);
// how many rows the Channels view has; the row `at` into `row`: 1, or 0 past the last; the channel of the row `at`
// opened under it, or closed where it is open; and the row of the channel last opened or closed, -1 for none
unsigned int cli_comms_pane_rows(const CliCommsPaneCfg *args);
int cli_comms_pane_row(const CliCommsPaneCfg *args);
void cli_comms_pane_toggle(const CliCommsPaneCfg *args);
int cli_comms_pane_current(const CliCommsPaneCfg *args);
// the view shown; the view `view` chosen; the count of parts and channels as a sentence; and the text of the head's
// action `at`, a view's choice or the layer's read again, each with its bytes into `length_out`
unsigned int cli_comms_pane_view(const CliCommsPaneCfg *args);
void cli_comms_pane_choose(const CliCommsPaneCfg *args);
const char *cli_comms_pane_summary(const CliCommsPaneCfg *args);
const char *cli_comms_pane_label(const CliCommsPaneCfg *args);
struct CliCommsPaneCfg
{
    void (*woken)(void);
    const char *root;
    unsigned int at;
    CliCommsRow *row;
    int shown;
    int force;
    unsigned long long now_ms;
    unsigned int view;
    unsigned long long *length_out;
};

// The Comms commands of the catalog run (comms_command.c): send-request, copy-as-curl and private-environment on the
// request the shown view's cursor stands in, network, network-off, report, remote-rejoin and remote-copy-address. A
// command that shows a view of the pane chooses it and answers 1 in `shown_out`; one whose text goes to the system's
// clipboard writes it into `clipboard`; one that opens a file writes its path in the tree into `open_out`. The run
// answers 1 where `name` is one of them, else 0 and nothing written
int cli_comms_command_run(const CliCommsCommandCfg *args);
struct CliCommsCommandCfg
{
    const char *name;
    const char *const *words;
    unsigned int count;
    VerbaLinea *clipboard;
    VerbaLinea *open_out;
    int *shown_out;
};

struct CliCommsListenerCfg
{
    unsigned int listener_value;
};

typedef struct
{
    int (*grants)(const CliCommsGrantCfg *args);
} CliCommsGrantNs;
EMBED_TABLE_LAYOUT(CliCommsGrantNs, grants);
EMBED_TABLE_STORAGE CliCommsGrantNs cli_comms_grant EMBED_UNUSED = {
    .grants = cli_comms_grants,
};

typedef struct
{
    int (*within)(const CliCommsInnerCfg *args);
} CliCommsInnerNs;
EMBED_TABLE_LAYOUT(CliCommsInnerNs, within);
EMBED_TABLE_STORAGE CliCommsInnerNs cli_comms_inner EMBED_UNUSED = {
    .within = cli_comms_within,
};

typedef struct
{
    int (*comms_register)(const CliCommsNameCfg *args);
    int (*register_persistent)(const CliCommsNameCfg *args);
    int (*register_under)(const CliCommsNameCfg *args);
} CliCommsNameNs;
EMBED_TABLE_LAYOUT(CliCommsNameNs, comms_register, register_persistent, register_under);
EMBED_TABLE_STORAGE CliCommsNameNs cli_comms_name EMBED_UNUSED = {
    .comms_register = cli_comms_register,
    .register_persistent = cli_comms_register_persistent,
    .register_under = cli_comms_register_under,
};

typedef struct
{
    int (*is_persistent)(const CliCommsProcessCfg *args);
    void (*bind)(const CliCommsProcessCfg *args);
    void (*leave)(const CliCommsProcessCfg *args);
} CliCommsProcessNs;
EMBED_TABLE_LAYOUT(CliCommsProcessNs, is_persistent, bind, leave);
EMBED_TABLE_STORAGE CliCommsProcessNs cli_comms_process EMBED_UNUSED = {
    .is_persistent = cli_comms_is_persistent,
    .bind = cli_comms_bind,
    .leave = cli_comms_leave,
};

typedef struct
{
    int (*process_get)(const CliCommsKeyProcessCfg *args);
} CliCommsKeyProcessNs;
EMBED_TABLE_LAYOUT(CliCommsKeyProcessNs, process_get);
EMBED_TABLE_STORAGE CliCommsKeyProcessNs cli_comms_key_process EMBED_UNUSED = {
    .process_get = cli_comms_process_get,
};

typedef struct
{
    int (*name_get)(const CliCommsKeyCfg *args);
} CliCommsKeyNs;
EMBED_TABLE_LAYOUT(CliCommsKeyNs, name_get);
EMBED_TABLE_STORAGE CliCommsKeyNs cli_comms_key EMBED_UNUSED = {
    .name_get = cli_comms_name_get,
};

typedef struct
{
    int (*listen)(const CliCommsChannelCfg *args);
} CliCommsChannelNs;
EMBED_TABLE_LAYOUT(CliCommsChannelNs, listen);
EMBED_TABLE_STORAGE CliCommsChannelNs cli_comms_channel EMBED_UNUSED = {
    .listen = cli_comms_listen,
};

typedef struct
{
    void (*unlisten)(const CliCommsListenerCfg *args);
} CliCommsListenerNs;
EMBED_TABLE_LAYOUT(CliCommsListenerNs, unlisten);
EMBED_TABLE_STORAGE CliCommsListenerNs cli_comms_listener EMBED_UNUSED = {
    .unlisten = cli_comms_unlisten,
};

typedef struct
{
    int (*send)(const CliCommsChannelErrMsgCfg *args);
} CliCommsChannelErrMsgNs;
EMBED_TABLE_LAYOUT(CliCommsChannelErrMsgNs, send);
EMBED_TABLE_STORAGE CliCommsChannelErrMsgNs cli_comms_channel_err_msg EMBED_UNUSED = {
    .send = cli_comms_send,
};

typedef struct
{
    void (*transfer)(const CliCommsChannelBytesCfg *args);
} CliCommsChannelBytesNs;
EMBED_TABLE_LAYOUT(CliCommsChannelBytesNs, transfer);
EMBED_TABLE_STORAGE CliCommsChannelBytesNs cli_comms_channel_bytes EMBED_UNUSED = {
    .transfer = cli_comms_transfer,
};

typedef struct
{
    void (*begin)(const CliCommsPaneCfg *args);
    int (*step)(const CliCommsPaneCfg *args);
    unsigned long long (*deadline_ms)(const CliCommsPaneCfg *args);
    unsigned int (*rows)(const CliCommsPaneCfg *args);
    int (*row)(const CliCommsPaneCfg *args);
    void (*toggle)(const CliCommsPaneCfg *args);
    int (*current)(const CliCommsPaneCfg *args);
    unsigned int (*view)(const CliCommsPaneCfg *args);
    void (*choose)(const CliCommsPaneCfg *args);
    const char *(*summary)(const CliCommsPaneCfg *args);
    const char *(*label)(const CliCommsPaneCfg *args);
} CliCommsPaneNs;
EMBED_TABLE_LAYOUT(CliCommsPaneNs, begin, step, deadline_ms, rows, row, toggle, current, view, choose, summary, label);
EMBED_TABLE_STORAGE CliCommsPaneNs cli_comms_pane EMBED_UNUSED = {
    .begin = cli_comms_pane_begin,
    .step = cli_comms_pane_step,
    .deadline_ms = cli_comms_pane_deadline_ms,
    .rows = cli_comms_pane_rows,
    .row = cli_comms_pane_row,
    .toggle = cli_comms_pane_toggle,
    .current = cli_comms_pane_current,
    .view = cli_comms_pane_view,
    .choose = cli_comms_pane_choose,
    .summary = cli_comms_pane_summary,
    .label = cli_comms_pane_label,
};

typedef struct
{
    int (*run)(const CliCommsCommandCfg *args);
} CliCommsCommandNs;
EMBED_TABLE_LAYOUT(CliCommsCommandNs, run);
EMBED_TABLE_STORAGE CliCommsCommandNs cli_comms_command EMBED_UNUSED = {
    .run = cli_comms_command_run,
};

#endif
