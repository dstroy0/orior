// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface.h: the CLI in the window's process, and the scheduler until MMgr has one. Every queue is an anularis ring with
// one writer and one reader. The window's thread writes a call to one worker's ring, the worker runs it through
// cli_window_called, and the worker writes its answer and what it emits to an item ring of its own. Any other thread
// emitting claims one of the emitters' rings for that one write. The window's thread reads the item rings a mask names,
// and no thread takes a lock or wakes a worker that is running. Nothing of the CLI's runs on the window's thread
// (interface.c)
#ifndef GUI_INTERFACE_H
#define GUI_INTERFACE_H
typedef struct GuiInterfaceOwnCfg GuiInterfaceOwnCfg;
typedef struct GuiInterfaceCfg GuiInterfaceCfg;
typedef struct GuiInterfaceContextCfg GuiInterfaceContextCfg;
#include "../../cli/src/command_line/callback_cfg.h"
#include "../../cli/src/window/window.h"
#include "embed_atomic.h"
#include "embed_dispatch_layout.h"
#include "memoria_anularis/memoria_anularis.h"

// the calls a worker's ring holds, the items an item ring holds, each a power of two; the pool's workers, as many as
// the CLI answers at once; the workers with the own lane's, which runs the window's own calls in order; the rings
// threads other than the workers emit on; and every item ring, one bit each in a word
#define GUI_INTERFACE_CALLS 256u
#define GUI_INTERFACE_ITEMS 256u
#define GUI_INTERFACE_POOL CLI_WINDOW_ROOMS
#define GUI_INTERFACE_WORKERS (GUI_INTERFACE_POOL + 1u)
#define GUI_INTERFACE_EMITTERS 16u
#define GUI_INTERFACE_RINGS (GUI_INTERFACE_WORKERS + GUI_INTERFACE_EMITTERS)
_Static_assert(GUI_INTERFACE_RINGS <= 64u, "the cli's masks name each ring by one bit of a 64-bit word");

// Where the window's thread is woken, from any thread; and where each item drained is handed: the call's ticket, 0 for
// an event, its name, 1 where its text is an answer and not why it failed, and `size` bytes of the text, each valid
// while the call runs
typedef void (*GuiInterfaceWoken)(void *context);
typedef void (*GuiInterfaceEach)(const GuiInterfaceEachCfg *args);

// A call waiting for its worker: its ticket, whether it is the window's own, and its name and `size` bytes of its
// arguments in one cell
typedef struct
{
    unsigned long long ticket;
    int own;
    char *name;
    char *args;
    unsigned long long size;
} GuiInterfaceCall;

// An item waiting for the window's thread: the call's ticket, 0 for an event, whether its text is an answer, its name,
// and `size` bytes of its text; an answer's text lying in the room the call ran in, CLI_WINDOW_ROOMS for an event,
// whose name and text are copied into one cell; the room released once the window's thread has read the answer
typedef struct
{
    unsigned long long ticket;
    int ok;
    char *name;
    const char *text;
    unsigned long long size;
    unsigned int room;
} GuiInterfaceItem;

struct GuiInterface;

// A worker: the ring of calls the window's thread writes, the wake it sleeps on while that ring is empty, its thread,
// its index, which is also its item ring's, and its interface
typedef struct
{
    mmgr_ring calls_ring;
    GuiInterfaceCall calls[GUI_INTERFACE_CALLS];
    CliWake wake;
    CliThread thread;
    unsigned int index;
    struct GuiInterface *cli;
} GuiInterfaceWorker;

// An item ring and its items
typedef struct
{
    mmgr_ring ring;
    GuiInterfaceItem items[GUI_INTERFACE_ITEMS];
} GuiInterfaceItems;

// The interface: each word several threads write on a cache line of its own: the workers idle, one bit each; the item rings
// with items, one bit each; the emitters' rings a thread stands in, one bit each; the count that spreads emitting
// threads over those rings; the workers running; whether the interface is ending. Then the wakes a full ring and the end of
// the interface sleep on, what only the window's thread writes, the workers, the item rings, and the window's wake
typedef struct GuiInterface
{
    _Alignas(64) embed_atomic_u64 idle;
    _Alignas(64) embed_atomic_u64 pending;
    _Alignas(64) embed_atomic_u64 emitters_taken;
    _Alignas(64) embed_atomic_u32 emitter_next;
    _Alignas(64) embed_atomic_u32 workers;
    _Alignas(64) embed_atomic_u32 ending;
    _Alignas(64) CliWake full;
    CliWake ended;
    unsigned long long tickets;
    unsigned int pool_next;
    GuiInterfaceWorker worker[GUI_INTERFACE_WORKERS];
    GuiInterfaceItems items[GUI_INTERFACE_RINGS];
    GuiInterfaceWoken woken;
    void *context;
} GuiInterface;

// the interface begun, its workers started, the window's thread woken through `woken` with `context`: 1, or 0 where a worker
// could not be started; and the interface ended, each worker let end and every cell released
int gui_interface_begin(const GuiInterfaceCfg *args);
void gui_interface_end(const GuiInterfaceCfg *args);
// from the window's thread alone: the call `name` of `size` bytes of JSON `args` written to a worker's ring, the own
// lane's where `own` is 1, run after every own call before it, else an idle pool worker's: its ticket, or 0 where that
// ring is full or the cell could not be taken
unsigned long long gui_interface_call(const GuiInterfaceOwnCfg *args);
// an event emitted by the CLI, `context` the interface, written under no ticket: what cli_window_begun is handed to emit to
void gui_interface_emit(void *context, const char *event, const char *body, unsigned long long size);
// from the window's thread alone: every item waiting handed to `each`, each ring's in the order written: how many
unsigned int gui_interface_drain(const GuiInterfaceContextCfg *args);
struct GuiInterfaceOwnCfg
{
    GuiInterface *cli;
    int own;
    const char *name;
    const char *request;
    unsigned long long size;
};

struct GuiInterfaceCfg
{
    GuiInterface *cli;
    GuiInterfaceWoken woken;
    void *context;
};

struct GuiInterfaceContextCfg
{
    GuiInterface *cli;
    void *context;
    GuiInterfaceEach each;
};

typedef struct
{
    int (*begin)(const GuiInterfaceCfg *args);
    void (*end)(const GuiInterfaceCfg *args);
} GuiInterfaceNs;
EMBED_TABLE_LAYOUT(GuiInterfaceNs, begin, end);
EMBED_TABLE_STORAGE GuiInterfaceNs gui_interface EMBED_UNUSED = {
    .begin = gui_interface_begin,
    .end = gui_interface_end,
};

typedef struct
{
    unsigned long long (*call)(const GuiInterfaceOwnCfg *args);
} GuiInterfaceOwnNs;
EMBED_TABLE_LAYOUT(GuiInterfaceOwnNs, call);
EMBED_TABLE_STORAGE GuiInterfaceOwnNs gui_interface_own EMBED_UNUSED = {
    .call = gui_interface_call,
};

typedef struct
{
    unsigned int (*drain)(const GuiInterfaceContextCfg *args);
} GuiInterfaceContextNs;
EMBED_TABLE_LAYOUT(GuiInterfaceContextNs, drain);
EMBED_TABLE_STORAGE GuiInterfaceContextNs gui_interface_context EMBED_UNUSED = {
    .drain = gui_interface_drain,
};

#endif
