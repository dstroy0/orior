// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// door.h: the comms layer reached from another process: a named pipe of the container's own on Windows, which takes no
// one from another machine, and elsewhere a socket in a folder only the reader can open.
//
// A frame is the length of its JSON in four bytes, least first, then the JSON. A process joins by its first frame,
// {"key": its key}; the container takes it only where the key is a part's and the key was given to the process at the
// other end, as the system reports, or to a process it stands under, as a shell of the window's is given a key for
// every program run in it. A part registered as persistent is taken from any process with its key, which is stored
// where only the reader can read it. Then the process requests {"listen": channel} and {"send": channel, "body": what},
// each answered with
// {"ok": what was done} or {"refused": why}, and each message sent on a channel it listens on comes as {"message":
// {"channel", "from", "body"}}.
//
// A body longer than the transfer layer's SMALL goes through it: the frame contains {"transfer": {"length"}} where the
// body would be, and the region handed over with the frame contains the body's JSON. On Windows the transfer names the
// region's handle too, the one the sending process has, which the container takes from it, or the one the container
// made in the listening process (door.c, door_serve.c, door_client.c, platform/windows_door.c, platform/posix_door.c)
#ifndef DOOR_H
#define DOOR_H

#include "../command_line/cli_internal.h"

// the variables a process of the container's is started with: the door's address, and its key
#define CLI_DOOR_ADDRESS_VARIABLE "ORIOR_COMMS"
#define CLI_DOOR_KEY_VARIABLE "ORIOR_KEY"

// One process's end of the door: what it joined as, the answers to what it requests, and the messages on the channels
// it listens on
typedef struct CliDoorClient CliDoorClient;

// the door opened to the comms layer, once, its address into `address`, sized for CLI_PATH_MAX_LEN: 1, or 0 with why
// given (door_serve.c)
int cli_door_open(char *address, VerbaLinea *err_msg);

// the door at `address` joined with `key`: the client, or NULL with why given; the door the variables name joined with
// the key they contain, where this process was started with both, NULL where it was not or the door refused; the name
// the client joined as; and the client let go (door_client.c)
CliDoorClient *cli_door_joined(const char *address, const char *key, VerbaLinea *err_msg);
CliDoorClient *cli_door_from_environment(void);
const char *cli_door_name(const CliDoorClient *client);
void cli_door_left(CliDoorClient *client);

// `channel` listened on; `size` bytes of `body`, JSON, sent on `channel`, through the transfer layer where it is long,
// how many listeners received it into `delivered`; each 1, or 0 with why given; and the next message on a channel the
// client listens on, waited for up to `wait` milliseconds, its channel and its speaker's name each into what is sized
// for CLI_NAME_MAX_LEN and its body into `body`: 1, or 0 where none came
int cli_door_listened(CliDoorClient *client, const char *channel, VerbaLinea *err_msg);
int cli_door_send(CliDoorClient *client, const char *channel, const char *body, unsigned long long size,
                  unsigned long long *delivered, VerbaLinea *err_msg);
int cli_door_next(CliDoorClient *client, unsigned long long wait, char *channel, char *from, VerbaLinea *body);

#endif
