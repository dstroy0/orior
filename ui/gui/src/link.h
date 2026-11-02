// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link.h: the CLI in the window's process. A call is queued with a ticket and run through cli_window_called, its
// answer, or why it failed, queued for the window's thread under the ticket: the window's own calls, the editor's
// inputs among them, run one after another in the order queued on a worker of their own, the tree's on a pool of
// workers at once; what the CLI emits arrives on any thread and is queued the same way under no ticket; each queueing
// wakes the window's thread through the wake it gave; the window's thread drains the queue in order. Nothing of the
// CLI's runs on the window's thread (link.c)
#ifndef GUI_LINK_H
#define GUI_LINK_H
typedef struct GuiLinkOwnCfg GuiLinkOwnCfg;
typedef struct GuiLinkCfg GuiLinkCfg;
typedef struct GuiLinkContextCfg GuiLinkContextCfg;
#include "../../cli/src/command_line/callback_cfg.h"
#include "../../cli/src/window/window.h"
#include "embed_dispatch_layout.h"

// the calls waiting in a ring at the most, the answers and events waiting for the window's thread at the most, the
// pool's workers, as many as the CLI answers at once, and the workers with the own lane's
#define GUI_LINK_PENDING 256u
#define GUI_LINK_DONE 1024u
#define GUI_LINK_POOL CLI_WINDOW_ROOMS
#define GUI_LINK_WORKERS (GUI_LINK_POOL + 1u)

// Where the window's thread is woken, from any thread; and where each item drained is handed: the call's ticket, 0 for
// an event, its name, 1 where its text is an answer and not why it failed, and `size` bytes of the text, each valid
// while the call runs
typedef void (*GuiLinkWoken)(void *context);
typedef void (*GuiLinkEach)(const GuiLinkEachCfg *args);

// A call waiting for a worker: its ticket, whether it is the window's own, and its name and `size` bytes of its
// arguments in one cell
typedef struct
{
    unsigned long long ticket;
    int own;
    char *name;
    char *args;
    unsigned long long size;
} GuiLinkCall;

// An item waiting for the window's thread: the call's ticket, 0 for an event, whether its text is an answer, its name,
// and `size` bytes of its text; an answer's text lying in the room the call ran in, CLI_WINDOW_ROOMS for an event,
// whose name and text are copied into one cell; the room given back once the window's thread has read the answer
typedef struct
{
    unsigned long long ticket;
    int ok;
    char *name;
    const char *text;
    unsigned long long size;
    unsigned int room;
} GuiLinkItem;

// A ring of calls waiting: the calls in a cell, its head and count
typedef struct
{
    GuiLinkCall *call;
    unsigned int head;
    unsigned int count;
} GuiLinkRing;

// The link: its lock; the wake workers wait on for a call and the one a queueing waits on for room; the window's own
// calls waiting and the tree's, each a ring; the items waiting, a ring in a cell, its head and count; the tickets
// given; the workers running, whether the link is ending, and each worker's thread, the own lane's last; and the
// window's wake
typedef struct
{
    CliLock lock;
    CliWake wake;
    CliWake room;
    GuiLinkRing own;
    GuiLinkRing tree;
    GuiLinkItem *done;
    unsigned int done_head;
    unsigned int done_count;
    unsigned long long tickets;
    unsigned int workers;
    int ending;
    CliThread thread[GUI_LINK_WORKERS];
    GuiLinkWoken woken;
    void *context;
} GuiLink;

// the link begun, its workers started, the window's thread woken through `woken` with `context`: 1, or 0 where a cell
// or a worker could not be had; and the link ended, each worker let end and every cell given back
int gui_link_begin(const GuiLinkCfg *args);
void gui_link_end(const GuiLinkCfg *args);
// the call `name` of `size` bytes of JSON `args` queued, the window's own where `own` is 1, run after every own call
// queued before it, else the tree's, run on the pool: its ticket, or 0 where its ring is full or the cell could not be
// taken
unsigned long long gui_link_call(const GuiLinkOwnCfg *args);
// an event emitted by the CLI, `context` the link, queued under no ticket: what cli_window_begun is given to emit to
void gui_link_emit(void *context, const char *event, const char *body, unsigned long long size);
// every item waiting handed to `each` in order, on the window's thread: how many
unsigned int gui_link_drain(const GuiLinkContextCfg *args);
struct GuiLinkOwnCfg
{
    GuiLink *link;
    int own;
    const char *name;
    const char *request;
    unsigned long long size;
};

struct GuiLinkCfg
{
    GuiLink *link;
    GuiLinkWoken woken;
    void *context;
};

struct GuiLinkContextCfg
{
    GuiLink *link;
    void *context;
    GuiLinkEach each;
};

typedef struct
{
    int (*begin)(const GuiLinkCfg *args);
    void (*end)(const GuiLinkCfg *args);
} CliLinkNs;
EMBED_TABLE_LAYOUT(CliLinkNs, begin, end);
EMBED_TABLE_STORAGE CliLinkNs cli_link EMBED_UNUSED = {
    .begin = gui_link_begin,
    .end = gui_link_end,
};

typedef struct
{
    unsigned long long (*call)(const GuiLinkOwnCfg *args);
} GuiLinkOwnNs;
EMBED_TABLE_LAYOUT(GuiLinkOwnNs, call);
EMBED_TABLE_STORAGE GuiLinkOwnNs gui_link_own EMBED_UNUSED = {
    .call = gui_link_call,
};

typedef struct
{
    unsigned int (*drain)(const GuiLinkContextCfg *args);
} GuiLinkContextNs;
EMBED_TABLE_LAYOUT(GuiLinkContextNs, drain);
EMBED_TABLE_STORAGE GuiLinkContextNs gui_link_context EMBED_UNUSED = {
    .drain = gui_link_drain,
};

#endif
