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

#include "../command_line/cli_internal.h"

// the bytes of a key written, its 32 bytes in hex and a 0 after; the most grants a part has to listen and to send
#define CLI_COMMS_KEY 65u
#define CLI_COMMS_GRANTS 16u

// A message received on `channel`, from the part named `from`, `size` bytes of its body: 1 where it was received, 0
// where the listener is gone, which lets it go, and -1 where the listener's queue had no room for it, which is not
// counted received
typedef int (*CliCommsRecvFn)(void *context, const char *channel, const char *from, const char *body,
                              unsigned long long size);

// whether the grant `grant` names the channel `channel`
int cli_comms_grants(const char *grant, const char *channel);
// whether the grant `inner` names only channels the grant `outer` names (comms.c)
int cli_comms_within(const char *inner, const char *outer);

// a part named `name` registered, granted the channels `listen`, `listens` of them, and `send_grant`, `send_grants` of
// them, by the container, its key into `key`, sized for CLI_COMMS_KEY: given to the process `process` where
// `has_process`, else one of the container's own, whose key the door never takes; the same, its key taken by the door
// from any process that owns it, as the key is stored where only the reader can read it; and a part the part of
// `parent` starts, granted channels of the parent's own alone. Each 1, or 0 with why given
int cli_comms_register(const char *name, int has_process, unsigned int process, const char *const *listen,
                       unsigned int listens, const char *const *send_grant, unsigned int send_grants, char *key,
                       VerbaLinea *err_msg);
int cli_comms_register_persistent(const char *name, const char *const *listen, unsigned int listens,
                                  const char *const *send_grant, unsigned int send_grants, char *key,
                                  VerbaLinea *err_msg);
int cli_comms_register_under(const char *parent, const char *name, int has_process, unsigned int process,
                             const char *const *listen, unsigned int listens, const char *const *send_grant,
                             unsigned int send_grants, char *key, VerbaLinea *err_msg);

// whether the door takes the key of `key`'s part from any process that owns it; the part of `key` given the process
// `process`, once it has started; the part of `key` taken away, with every channel it listened on; the process it was
// registered for into `process`, and its name into `name`, sized for CLI_NAME_MAX_LEN: each 1, or 0 where the key is
// no part's or it was given none
int cli_comms_is_persistent(const char *key);
void cli_comms_bind(const char *key, unsigned int process);
void cli_comms_leave(const char *key);
int cli_comms_process_of(const char *key, unsigned int *process);
int cli_comms_name_of(const char *key, char *name);

// `key`'s part listening on `channel`, a channel or a grant its key must name, each message sent there from now on
// handed to `recv`: where `queued`, in the sender's call while the container owns its lock, so that it must neither
// wait nor send, as a queue a reader drains is filled; else once the container has let go of its lock, in the
// sender's thread, which preserves the order of what one thread sends. The listener's number into `listener`: 1, or 0
// with why given; and the listener let go
int cli_comms_listen(const char *key, const char *channel, int queued, CliCommsRecvFn recv, void *context,
                     unsigned int *listener, VerbaLinea *err_msg);
void cli_comms_unlisten(unsigned int listener);

// `size` bytes of `body` sent on `channel` as the part of `key`, how many listeners received it into `delivered`: 1, or
// 0 with why given; and `bytes` of a body on `channel` counted as gone through the transfer layer
int cli_comms_send(const char *key, const char *channel, const char *body, unsigned long long size,
                   unsigned long long *delivered, VerbaLinea *err_msg);
void cli_comms_transferred(const char *channel, unsigned long long bytes);

// every part, by name, and every channel that has carried or refused a message, by name, as the view lists them,
// written into `out` (comms_view.c)
void cli_comms_parts(VerbaLinea *out);
void cli_comms_channels(VerbaLinea *out);

#endif
