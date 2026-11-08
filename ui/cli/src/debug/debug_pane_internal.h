// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_pane_internal.h: what the Debug pane's sources share: the stores its rows stand in, the session it follows,
// the events the sink stores for the view's thread, and the helpers that fill the stores (debug_pane.c,
// debug_pane_rows.c, debug_pane_bar.c)
#ifndef DEBUG_PANE_INTERNAL_H
#define DEBUG_PANE_INTERNAL_H
#include "../text/typed.h"
#include "debug_pane.h"
#include "embed_atomic.h"

typedef struct CliDebugPaneCtx CliDebugPaneCtx;

// no offset into a text cell: a text that could not be stored
#define CLI_DEBUG_PANE_NONE 0xffffffffffffffffULL

// One entry of a store: its type, its depth, its console line's type, its line counted from 0, whether it opens, is
// open, failed and, for a console line, has its line end; four texts as offsets into the store's text cell with their
// bytes, the row's three and its key in the open set; and the adapter's reference of its frame or its children, the
// session of an event stored
typedef struct
{
    unsigned int type;
    unsigned int depth;
    unsigned int line_type;
    unsigned int line;
    unsigned char opens;
    unsigned char open;
    unsigned char failed;
    unsigned char ended;
    unsigned long long text;
    unsigned long long text_length;
    unsigned long long second;
    unsigned long long second_length;
    unsigned long long third;
    unsigned long long third_length;
    unsigned long long key;
    unsigned long long key_length;
    long long reference;
} CliDebugPaneEntry;

// A store: its entries in one cell, how many and how many the cell carries, the first of them where the store is a
// ring, and its texts in one cell, each with its end after it, the bytes used and the bytes the cell carries
typedef struct
{
    CliDebugPaneEntry *entry;
    unsigned int count;
    unsigned int capacity;
    unsigned int first;
    char *text;
    unsigned long long used;
    unsigned long long size;
} CliDebugPaneStore;

// The pane: the stores of the frames, the variables, the console's lines as a ring, the watches' expressions, the open
// set's keys sorted by their bytes, a key standing where its row's open state is the reverse of its first one, and the
// events the sink stored since the last step; the field's text in Variables and in Console; the view shown; the
// session followed and whether one is, its name, tool and language as offsets into its own text cell, whether it steps
// back; whether it is stopped, its thread and whether one is known, the frame chosen; the status line in a cell sized
// to it, whether it reports a failure; the bar's flags, set as the state changes; where the wake goes and the requests'
// table; the sink's lock and whether events wait
typedef struct
{
    CliDebugPaneStore frames;
    CliDebugPaneStore variables;
    CliDebugPaneStore console;
    CliDebugPaneStore watches;
    CliDebugPaneStore flipped;
    CliDebugPaneStore events;
    CliTyped watch_typed;
    CliTyped console_typed;
    unsigned int view;
    unsigned long long session;
    int has_session;
    char *session_texts;
    unsigned long long name_length;
    unsigned long long tool_at;
    unsigned long long tool_length;
    unsigned long long language_at;
    unsigned long long language_length;
    int step_back;
    int paused;
    long long thread;
    int has_thread;
    unsigned int frame_at;
    char *status;
    unsigned long long status_length;
    int status_failed;
    unsigned char enabled[CLI_DEBUG_STEPS];
    unsigned char shown[CLI_DEBUG_STEPS];
    void (*woken)(void);
    const CliDebugPaneRequestsNs *requests;
    embed_atomic_u32 lock;
    embed_atomic_u32 waiting;
} CliDebugPaneState;

extern CliDebugPaneState g_cli_debug_pane;

// `length` bytes of `text` put at the end of the store's text cell with an end after them, the cell taken anew twice
// the live bytes where it is outgrown: their offset, CLI_DEBUG_PANE_NONE where the cells cannot meet them; an entry put
// after the store's last, all zero, the cell taken anew twice its count where it is outgrown: the entry, NULL where the
// cells cannot meet it; and the store's cells released, the store empty (debug_pane.c)
unsigned long long cli_debug_pane_text_put(const CliDebugPaneCtx *args);
CliDebugPaneEntry *cli_debug_pane_entry_put(const CliDebugPaneCtx *args);
void cli_debug_pane_store_empty(const CliDebugPaneCtx *args);
// the status line set to `length` bytes of `text`, a failure's where `failed`, in a cell sized to it; and set to the
// sentence of the session followed running, its name and its tool (debug_pane.c)
void cli_debug_pane_status_set(const CliDebugPaneCtx *args);
void cli_debug_pane_running_said(const CliDebugPaneCtx *args);
// the bar's flags set from the session followed: whether each step can act and whether it is shown
// (debug_pane_bar.c)
void cli_debug_pane_bar_settle(const CliDebugPaneCtx *args);
// `length` bytes of `text` written to the console as `type`, a piece run on into the last line of its type where that
// has no line end, each line end ending a line, the oldest line going past CLI_DEBUG_CONSOLE_LINES; the chosen frame's
// variables read again into the store of Variables, each watch evaluated in it, for the tree at `root`; the frames of
// the stopped thread read into the store of Frames; and the session followed let go, `text` the status line, every
// view's rows of it empty (debug_pane_rows.c)
void cli_debug_pane_line_write(const CliDebugPaneCtx *args);
void cli_debug_pane_variables_read(const CliDebugPaneCtx *args);
void cli_debug_pane_frames_read(const CliDebugPaneCtx *args);
void cli_debug_pane_ended(const CliDebugPaneCtx *args);
struct CliDebugPaneCtx
{
    CliDebugPaneStore *store;
    const char *text;
    unsigned long long length;
    unsigned int type;
    int failed;
    const char *root;
};

typedef struct
{
    unsigned long long (*text_put)(const CliDebugPaneCtx *args);
    CliDebugPaneEntry *(*entry_put)(const CliDebugPaneCtx *args);
    void (*store_empty)(const CliDebugPaneCtx *args);
    void (*status_set)(const CliDebugPaneCtx *args);
    void (*running_said)(const CliDebugPaneCtx *args);
    void (*bar_settle)(const CliDebugPaneCtx *args);
    void (*line_write)(const CliDebugPaneCtx *args);
    void (*variables_read)(const CliDebugPaneCtx *args);
    void (*frames_read)(const CliDebugPaneCtx *args);
    void (*ended)(const CliDebugPaneCtx *args);
} CliDebugPaneNs;
EMBED_TABLE_LAYOUT(CliDebugPaneNs, text_put, entry_put, store_empty, status_set, running_said, bar_settle, line_write,
                   variables_read, frames_read, ended);
EMBED_TABLE_STORAGE CliDebugPaneNs cli_debug_pane_internal EMBED_UNUSED = {
    .text_put = cli_debug_pane_text_put,
    .entry_put = cli_debug_pane_entry_put,
    .store_empty = cli_debug_pane_store_empty,
    .status_set = cli_debug_pane_status_set,
    .running_said = cli_debug_pane_running_said,
    .bar_settle = cli_debug_pane_bar_settle,
    .line_write = cli_debug_pane_line_write,
    .variables_read = cli_debug_pane_variables_read,
    .frames_read = cli_debug_pane_frames_read,
    .ended = cli_debug_pane_ended,
};

#endif
