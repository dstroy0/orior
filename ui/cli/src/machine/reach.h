// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// reach.h: the windows open, as the command line finds them: each window stores a file under windows/ in orior's home,
// readable by the reader alone, naming its process, the tree it has open, its comms door and a persistent key of the
// door's that grants the channel `command` to send on and those under `answer/` to listen on. A command line run on a
// tree with a window open sends the command there, and the window runs it as the menu runs it and answers on `answer/`
// and the command's id. A file whose window is gone is taken away by the first command line that finds the door shut. A
// window's file is its process in decimal, its tree, empty for none, its address and its key, each ended by a 0. A
// command's bytes are its id, menu, command and words, each ended by a 0; an answer's are CLI_REACH_DONE or
// CLI_REACH_REFUSED and then its text (reach.c)
#ifndef REACH_H
#define REACH_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliReachMenuCfg CliReachMenuCfg;
typedef struct CliReachCfg CliReachCfg;

// The fields before a command's words. Then the first byte of an answer: the command ran, it did not, or the window
// holds its answer until a person answers in the window. A held command sends the held byte first and its answer
// after it. Last, what `run` returns for a command whose answer the window holds
#define CLI_REACH_HEAD_FIELDS 3u
#define CLI_REACH_DONE '1'
#define CLI_REACH_REFUSED '0'
#define CLI_REACH_HELD '2'
#define CLI_REACH_HOLD 2

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

// This window's door: opened for every process the container starts and for the command line, which it stores for the
// tree `root`: 1, or 0 with the reason written. A command that comes is read where the door's thread received it, its
// texts places in that body, pushed on the waiting with a compare and swap, and `woken` called; that thread sleeps on
// the command's address until the window has run it, and the body stands as it was until then. On the window's thread,
// every command waiting run by `run` in the order it came, its name, words and their count handed on, answered on the
// command's own channel as `run` returns, 1 where it ran, and its door's thread woken: 1 where any came. And the door
// closed: every command waiting woken unrun, and the door's key let go
int cli_reach_open(const char *root, void (*woken)(void), VerbaLinea *err_msg);
int cli_reach_take(int (*run)(const char *command, const char *const *words, unsigned int count));
void cli_reach_close(void);
// Where `run` returns CLI_REACH_HOLD, the door sends the held byte and keeps the command's thread asleep. This call
// answers the held command later, `done` 1 with `text` or 0 for refused, and wakes its thread. It does nothing where
// no command is held. The window holds one command at a time
void cli_reach_held_answer(int done, const char *text);

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
