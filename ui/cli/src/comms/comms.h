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
// A channel's name is words joined by slashes, as lsp/rust or db/rows. A grant is a name, or a name ending in /* for
// every channel under it, or * alone for every channel. A body is JSON, as its speaker wrote it (comms.c, comms_view.c)
#ifndef COMMS_H
#define COMMS_H
typedef struct CliCommsNameCfg CliCommsNameCfg;
typedef struct CliCommsChannelCfg CliCommsChannelCfg;
typedef struct CliCommsChannelErrMsgCfg CliCommsChannelErrMsgCfg;
typedef struct CliCommsProcessCfg CliCommsProcessCfg;
typedef struct CliCommsGrantCfg CliCommsGrantCfg;
typedef struct CliCommsKeyCfg CliCommsKeyCfg;
typedef struct CliCommsKeyProcessCfg CliCommsKeyProcessCfg;
typedef struct CliCommsChannelBytesCfg CliCommsChannelBytesCfg;
typedef struct CliCommsInnerCfg CliCommsInnerCfg;
typedef struct CliCommsCfg CliCommsCfg;
typedef struct CliCommsListenerCfg CliCommsListenerCfg;

#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// the bytes of a key written, its 32 bytes in hex and a 0 after; the most grants a part has to listen and to send
#define CLI_COMMS_KEY 65u
#define CLI_COMMS_GRANTS 16u

// A message received on `channel`, from the part named `from`, `size` bytes of its body: 1 where it was received, 0
// where the listener is gone, which lets it go, and -1 where the listener's queue had no room for it, which is not
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

// every part, by name, and every channel that has carried or refused a message, by name, as the view lists them,
// written into `out` (comms_view.c)
void cli_comms_parts(const CliCommsCfg *args);
void cli_comms_channels(const CliCommsCfg *args);

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

struct CliCommsCfg
{
    VerbaLinea *out;
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
    void (*parts)(const CliCommsCfg *args);
    void (*channels)(const CliCommsCfg *args);
} CliCommsNs;
EMBED_TABLE_LAYOUT(CliCommsNs, parts, channels);
EMBED_TABLE_STORAGE CliCommsNs cli_comms_table EMBED_UNUSED = {
    .parts = cli_comms_parts,
    .channels = cli_comms_channels,
};

#endif
