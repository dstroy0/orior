// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macro.h: door macros, sequences of door commands recorded on one window and replayed on the windows of other hosts
// through the sessions. A recording takes every command the door runs on this window, with the menu the catalog gives
// it, until it stops. A send replays a named macro on every joined host as often as asked: each step goes through the
// sessions as one request, the next sent as every host answers the one before, the sender's name as the hand each
// pointer step drives. A pointer step is sent with `--as <sender>` before its words where it has none, so the far
// window moves a hand of the sender's name and never its own pointer (macro.c)
#ifndef MACRO_H
#define MACRO_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliDoorMacroCfg CliDoorMacroCfg;

// the macros kept at most, the steps one macro holds at most, the bytes one macro's steps take at most, and the bytes
// a macro's name takes at most
#define CLI_DOOR_MACROS 8u
#define CLI_DOOR_MACRO_STEPS 64u
#define CLI_DOOR_MACRO_BYTES 8192u
#define CLI_DOOR_MACRO_NAME 64u

// the recording of the macro `name` started, an older macro of that name emptied first: 1, or 0 with why given where
// one records already, the name does not fit or no record is free
int cli_door_macro_record(const CliDoorMacroCfg *args);
// the recording ended, the macro's name and its steps on `out`: 1, or 0 with why given where none records
int cli_door_macro_stop(const CliDoorMacroCfg *args);
// whether a recording is on
int cli_door_macro_recording(void);
// `command` with `count` `request` words appended to the recording as a step, the menu the catalog gives the command
// as its menu: 1, or 0 where none records, the catalog has no menu for the command or the macro is full
int cli_door_macro_recorded(const CliDoorMacroCfg *args);
// every macro's name and its steps on a line of `out`, `no macros` where there is none
void cli_door_macro_list(const CliDoorMacroCfg *args);
// the macro `name` forgotten: 1, or 0 with why given where there is none
int cli_door_macro_forget(const CliDoorMacroCfg *args);
// The macro `name` sent to every joined host `times` times over, 1 where `times` is 0, each pointer step on the hand
// `sender` names, this machine's name where `sender` is NULL: 1 with the first step sent, or 0 with why given where
// there is no such macro, it has no steps, a send is on already or the sessions take no request. The replay goes on
// as the window steps
int cli_door_macro_send(const CliDoorMacroCfg *args);
// whether a send or a list waits to be answered; and the answer once the replay is over, into `out` with `done` 1
// where every host ran every step, each host's refusal on a line after the count: 1, or 0 where nothing waits or
// steps are still to be sent
int cli_door_macro_pending(void);
int cli_door_macro_step(const CliDoorMacroCfg *args);
// an answer put to wait: the text of `out`, `ok` 1 where the command ran, for a record, stop or forget the window
// holds until it steps
void cli_door_macro_answered(const CliDoorMacroCfg *args);
// every macro forgotten and the cell given back
void cli_door_macro_close(void);

struct CliDoorMacroCfg
{
    const char *name;
    const char *menu;
    const char *command;
    const char *const *request;
    unsigned int count;
    unsigned long long times;
    const char *sender;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    int *done;
    int ok;
};

typedef struct
{
    int (*record)(const CliDoorMacroCfg *args);
    int (*stop)(const CliDoorMacroCfg *args);
    int (*recorded)(const CliDoorMacroCfg *args);
    void (*list)(const CliDoorMacroCfg *args);
    int (*forget)(const CliDoorMacroCfg *args);
    int (*send)(const CliDoorMacroCfg *args);
    int (*step)(const CliDoorMacroCfg *args);
    void (*answered)(const CliDoorMacroCfg *args);
} CliDoorMacroNs;
EMBED_TABLE_LAYOUT(CliDoorMacroNs, record, stop, recorded, list, forget, send, step, answered);
EMBED_TABLE_STORAGE CliDoorMacroNs cli_door_macro EMBED_UNUSED = {
    .record = cli_door_macro_record,
    .stop = cli_door_macro_stop,
    .recorded = cli_door_macro_recorded,
    .list = cli_door_macro_list,
    .forget = cli_door_macro_forget,
    .send = cli_door_macro_send,
    .step = cli_door_macro_step,
    .answered = cli_door_macro_answered,
};
#endif
