// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// link_internal.h: what the link's sources share: the link's state, its locks and its wake, and the server's
// lines read and handed on (link.c, link_keep.c)
#ifndef LINK_INTERNAL_H
#define LINK_INTERNAL_H

#include "link.h"

// how long a call waits for the link to be made, and the longest wait between one try at making it and the next, in
// milliseconds; how much of what ssh, docker or wsl says on its errors is kept, the end of it; the most calls waiting
// at once; the longest line the server writes that is read; and the room of the line that starts the server
#define CLI_LINK_JOINING 60000u
#define CLI_LINK_JOIN_MOST 30000u
#define CLI_LINK_SAID 2000u
#define CLI_LINK_CALLS 256u
#define CLI_LINK_LINE (64u << 20u)
#define CLI_LINK_START ((4u * CLI_PATH_LONGEST) + 256u)
#define CLI_LINK_STANDING ((6u * (CLI_LINK_SAID + (3u * CLI_PATH_LONGEST))) + 128u)

// A call waiting on its answer: its id, whether it is answered and whether that answer is a failure, and where its
// answer and its failure are written
typedef struct
{
    int used;
    int done;
    unsigned long long id;
    CliLine *out;
    CliLine *said;
} CliLinkCall;

// The link: its address and where it tells what it hears; whether it is open, ended, joined to its server, holding the
// server's process and stopping it, and reading its errors; the process; the line that starts the server once it is
// known; the end of what the transport says on its errors; how it stands as last told; the calls waiting; the next
// call's id; its threads; and the server's output read and not yet taken
typedef struct
{
    CliLinkAddress address;
    CliLinkTold told;
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
    char said[CLI_LINK_SAID + 4u];
    unsigned long long said_size;
    char standing[CLI_LINK_STANDING];
    unsigned long long standing_size;
    CliLinkCall calls[CLI_LINK_CALLS];
    unsigned long long next;
    CliThread keeper;
    CliThread errors;
    char piece[1u << 16u];
    unsigned long long piece_at;
    unsigned long long piece_end;
} CliLink;

// the one link; the line of the server's output read last, a cell of CLI_LINK_LINE the keeper holds while it runs; the
// link's lock, the lock of the server's input, which a writer takes before the link's, and the wake of a change in how
// it stands (link.c)
extern CliLink g_cli_link;
extern char *g_cli_link_line;
extern CliLock g_cli_link_lock;
extern CliLock g_cli_link_writing;
extern CliWake g_cli_link_changed;

// the link's state told and kept as how it stands, by the keeper's thread alone; the same as preparing the server
// tells it; what the transport says on its errors read until it ends, run on a thread of its own; the end of what
// it said, trimmed, into `out`; the next line the server wrote into g_cli_link_line, `*size` bytes, `*whole` 0
// where it was longer than that holds: 1, or 0 at the output's end; and that line handed on, an event to the
// link's teller and an answer to the call waiting on its id (link.c)
void cli_link_tell(const char *state, const char *said, unsigned long long size);
void cli_link_stated(void *context, const char *state, const char *said);
void cli_link_errors_read(void *context);
void cli_link_said(CliLine *out);
int cli_link_line(unsigned long long *size, int *whole);
void cli_link_heard(unsigned long long size, int whole);

#endif
