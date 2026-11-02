// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_internal.h: what the link's sources share: the link's state, its locks and its wake, and the server's
// lines read and handed on (link.c, link_supervisor.c)
#ifndef LINK_INTERNAL_H
#define LINK_INTERNAL_H
typedef struct CliLinkStateCtx CliLinkStateCtx;
typedef struct CliLinkSizeWholeCtx CliLinkSizeWholeCtx;
typedef struct CliLinkSizeCtx CliLinkSizeCtx;
typedef struct CliLinkCtx CliLinkCtx;

#include "../command_line/callback_cfg.h"
#include "embed_dispatch_layout.h"
#include "link.h"

// how long a call waits for the link to be made, and the longest wait between one try at making it and the next, in
// milliseconds; how much of what ssh, docker or wsl prints on its errors is retained, the end of it; the most calls
// waiting at once; the longest line the server writes that is read; and the room of the line that starts the server
#define CLI_LINK_JOINING 60000u
#define CLI_LINK_JOIN_MOST 30000u
#define CLI_LINK_STDERR_TAIL_LEN 2000u
#define CLI_LINK_CALLS 256u
#define CLI_LINK_LINE (64u << 20u)
#define CLI_LINK_START ((4u * CLI_PATH_MAX_LEN) + 256u)
#define CLI_LINK_STANDING ((6u * (CLI_LINK_STDERR_TAIL_LEN + (3u * CLI_PATH_MAX_LEN))) + 128u)

// A call waiting on its answer: its id, whether it is answered and whether that answer is a failure, and where its
// answer and its failure are written
typedef struct
{
    int used;
    int done;
    unsigned long long id;
    VerbaLinea *out;
    VerbaLinea *err_msg;
} CliLinkCall;

// The link: its address and where it reports what it receives; whether it is open, ended, joined to its server, owning
// the server's process and stopping it, and reading its errors; the process; the line that starts the server once it is
// known; the end of what the transport prints on its errors; how it stands as last reported; the calls waiting; the
// next call's id; its threads; and the server's output read and not yet taken
typedef struct
{
    CliLinkAddress address;
    CliLinkEventFn emit;
    void *context;
    int open;
    int ended;
    int joined;
    int running;
    int stopping;
    int reading_errors;
    CliSpawned spawned;
    char start[CLI_LINK_START];
    int has_start;
    char stderr_tail[CLI_LINK_STDERR_TAIL_LEN + 4u];
    unsigned long long stderr_tail_size;
    char standing[CLI_LINK_STANDING];
    unsigned long long standing_size;
    CliLinkCall calls[CLI_LINK_CALLS];
    unsigned long long next;
    CliThread store;
    CliThread errors;
    char piece[1u << 16u];
    unsigned long long piece_at;
    unsigned long long piece_end;
} CliLink;

// the one link; the line of the server's output read last, a cell of CLI_LINK_LINE the store owns while it runs; the
// link's lock, the lock of the server's input, which a writer takes before the link's, and the wake of a change in how
// it stands (link.c)
extern CliLink g_cli_link;
extern char *g_cli_link_line;
extern CliWake g_cli_link_changed;

// the link's state reported and stored as how it stands, by the store's thread alone; the same as preparing the server
// reports it; what the transport prints on its errors read until it ends, run on a thread of its own; the end of what
// it printed, trimmed, into `out`; the next line the server wrote into g_cli_link_line, `*size` bytes, `*whole` 0
// where it was longer than that fits: 1, or 0 at the output's end; and that line handed on, an event to the
// link's teller and an answer to the call waiting on its id (link.c)
void cli_link_state_set(const CliLinkStateCtx *args);
void cli_link_state(const CliLinkStatedCfg *args);
void cli_link_errors_read(void *context);
void cli_link_stderr_write(const CliLinkCtx *args);
int cli_link_line(const CliLinkSizeWholeCtx *args);
void cli_link_recv(const CliLinkSizeCtx *args);

struct CliLinkStateCtx
{
    const char *state;
    const char *detail;
    unsigned long long size;
};

struct CliLinkSizeWholeCtx
{
    unsigned long long *size_pointer;
    int *whole;
};

struct CliLinkSizeCtx
{
    unsigned long long size;
    int whole_value;
};

struct CliLinkCtx
{
    VerbaLinea *out;
};

typedef struct
{
    void (*state_set)(const CliLinkStateCtx *args);
} CliLinkStateNs;
EMBED_TABLE_LAYOUT(CliLinkStateNs, state_set);
EMBED_TABLE_STORAGE CliLinkStateNs cli_link_state_internal EMBED_UNUSED = {
    .state_set = cli_link_state_set,
};

typedef struct
{
    void (*state)(const CliLinkStatedCfg *args);
} CliLinkStatedNs;
EMBED_TABLE_LAYOUT(CliLinkStatedNs, state);
EMBED_TABLE_STORAGE CliLinkStatedNs cli_link_stated_table EMBED_UNUSED = {
    .state = cli_link_state,
};

typedef struct
{
    void (*stderr_write)(const CliLinkCtx *args);
} CliLinkNsTable;
EMBED_TABLE_LAYOUT(CliLinkNsTable, stderr_write);
EMBED_TABLE_STORAGE CliLinkNsTable cli_link_internal EMBED_UNUSED = {
    .stderr_write = cli_link_stderr_write,
};

typedef struct
{
    int (*line)(const CliLinkSizeWholeCtx *args);
} CliLinkSizeWholeNs;
EMBED_TABLE_LAYOUT(CliLinkSizeWholeNs, line);
EMBED_TABLE_STORAGE CliLinkSizeWholeNs cli_link_size_whole_internal EMBED_UNUSED = {
    .line = cli_link_line,
};

typedef struct
{
    void (*recv)(const CliLinkSizeCtx *args);
} CliLinkSizeNsTable;
EMBED_TABLE_LAYOUT(CliLinkSizeNsTable, recv);
EMBED_TABLE_STORAGE CliLinkSizeNsTable cli_link_size_internal EMBED_UNUSED = {
    .recv = cli_link_recv,
};

#endif
