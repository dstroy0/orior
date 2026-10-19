// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// comms_internal.h: what the comms sources share: the parts, the listeners and each channel's traffic, under one lock
#ifndef COMMS_INTERNAL_H
#define COMMS_INTERNAL_H

#include "comms.h"

// the most parts, listeners and channels; how many of each channel's last messages are kept, and the bytes of a body
// kept whole among them
#define CLI_COMMS_PARTS 256u
#define CLI_COMMS_LISTENERS 512u
#define CLI_COMMS_CHANNELS 256u
#define CLI_COMMS_KEPT 20u
#define CLI_COMMS_KEPT_BODY 2048u

// A part: whether the place is taken, its key, its name, the process it was given, whether the door takes its key
// from any process, its grants to listen and to speak, and how many messages it said and had refused
typedef struct
{
    int taken;
    char key[CLI_COMMS_KEY];
    char name[CLI_NAME_LONGEST];
    int has_process;
    unsigned int process;
    int held;
    char listen[CLI_COMMS_GRANTS][CLI_NAME_LONGEST];
    unsigned int listens;
    char speak[CLI_COMMS_GRANTS][CLI_NAME_LONGEST];
    unsigned int speaks;
    unsigned long long said;
    unsigned long long refused;
} CliCommsPart;

// A listener: whether the place is taken, the part it is of, the channel or grant it listens on, whether it is
// handed each message under the lock, and where messages go
typedef struct
{
    int taken;
    unsigned int part;
    char pattern[CLI_NAME_LONGEST];
    int queued;
    CliCommsHeard heard;
    void *context;
} CliCommsListener;

// A message kept as a channel's last: the part that said it, and its body, whole where it fits and else said by its
// size
typedef struct
{
    char from[CLI_NAME_LONGEST];
    char body[CLI_COMMS_KEPT_BODY];
    unsigned long long size;
} CliCommsKept;

// A channel's traffic: its name, how many messages and bytes it carried, the bytes that went through the transfer
// layer, how many messages it refused, and its last messages, oldest first from `first`
typedef struct
{
    char name[CLI_NAME_LONGEST];
    unsigned long long messages;
    unsigned long long bytes;
    unsigned long long transferred;
    unsigned long long refused;
    CliCommsKept last[CLI_COMMS_KEPT];
    unsigned int first;
    unsigned int count;
} CliCommsTraffic;

// The comms layer: its parts, listeners and channels, and their lock
typedef struct
{
    CliCommsPart parts[CLI_COMMS_PARTS];
    CliCommsListener listeners[CLI_COMMS_LISTENERS];
    CliCommsTraffic traffic[CLI_COMMS_CHANNELS];
    unsigned int channels;
    CliLock lock;
} CliCommsRoom;

extern CliCommsRoom g_cli_comms;

#endif
