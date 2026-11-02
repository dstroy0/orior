// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach.h: the windows open, as the command line finds them: each window stores a file under windows/ in orior's home,
// readable by the reader alone, naming its process, the tree it has open, its comms door and a persistent key of the
// door's that grants the channel `command` to send on and `answer/*` to listen on. A command line run on a tree with a
// window open sends the command there, and the window runs it as the menu runs it and answers on `answer/` and the
// command's id. A file whose window is gone is taken away by the first command line that finds the door shut. A
// window's file is its process in decimal, its tree, empty for none, its address and its key, each ended by a 0. A
// command's bytes are its id, menu, command and words, each ended by a 0; an answer's are CLI_REACH_DONE or
// CLI_REACH_REFUSED and then its text (reach.c)
#ifndef REACH_H
#define REACH_H
typedef struct CliReachMenuCfg CliReachMenuCfg;
typedef struct CliReachCfg CliReachCfg;

#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

// the fields before a command's words; and the first byte of an answer, where the command ran and where it did not
#define CLI_REACH_HEAD_FIELDS 3u
#define CLI_REACH_DONE '1'
#define CLI_REACH_REFUSED '0'

// What the window answered: whether the command ran, and what it reported
typedef struct
{
    int done;
    char text[CLI_ERR_MSG_MAX_LEN];
} CliReachAnswer;

// what a window stores for the command line, its process `process`, the tree `root` it has open, none where it is NULL,
// its door's `address` and `key`, stored in `home` in a file the reader alone can read; the same of this window in
// orior's home; and this window's file taken away, as it closes. Each 1, or 0 with why given
int cli_reach_store_in(const CliReachCfg *args);
int cli_reach_store(const CliReachCfg *args);
void cli_reach_forget(void);

// `command` of `menu`, with `args`, `count` of them, sent to a window open on `root` among those stored in `home`, or,
// with no tree named, to the one window open where there is only one, its answer into `answer`: 1, 0 with why given,
// or -1 where no window is open there; and the same in orior's home
int cli_reach_request_in(const CliReachMenuCfg *args);
int cli_reach_request(const CliReachMenuCfg *args);

struct CliReachMenuCfg
{
    const char *home;
    const char *root;
    VerbaLinea *err_msg;
    const char *menu;
    const char *command;
    const char *const *request;
    unsigned int count;
    CliReachAnswer *answer;
};

struct CliReachCfg
{
    const char *home;
    unsigned int process;
    const char *root;
    const char *address;
    const char *key;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*store_in)(const CliReachCfg *args);
    int (*store)(const CliReachCfg *args);
} CliReachNs;
EMBED_TABLE_LAYOUT(CliReachNs, store_in, store);
EMBED_TABLE_STORAGE CliReachNs cli_reach EMBED_UNUSED = {
    .store_in = cli_reach_store_in,
    .store = cli_reach_store,
};

typedef struct
{
    int (*request_in)(const CliReachMenuCfg *args);
    int (*request)(const CliReachMenuCfg *args);
} CliReachMenuNs;
EMBED_TABLE_LAYOUT(CliReachMenuNs, request_in, request);
EMBED_TABLE_STORAGE CliReachMenuNs cli_reach_menu EMBED_UNUSED = {
    .request_in = cli_reach_request_in,
    .request = cli_reach_request,
};

#endif
