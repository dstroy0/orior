// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shells.h: a terminal's shell, started in a pseudo-terminal the size of its screen. A thread of the shell's reads what
// the shell writes onto a ring and wakes the shell's owner, whose thread drains the ring onto the screen, so the ring
// has one writer and one reader; another waits for the shell's end. The screen's answers go back to the shell as they
// come; text typed while the shell starts waits until the shell has written and then been quiet
// CLI_TERMINAL_SETTLE_MS, as the system's terminal reads its first input as the answer to where the cursor is and the
// shell takes keys once its prompt is up. A shell that ends prints `[exited CODE]` on its screen; a shell hung up on
// and still going a second later is killed (shells.c)
#ifndef CLI_TERMINAL_SHELLS_H
#define CLI_TERMINAL_SHELLS_H
#include "../command_line/cli_internal.h"
#include "embed_atomic.h"
#include "embed_dispatch_layout.h"
#include "memoria_anularis/memoria_anularis.h"
#include "screen.h"
#include "verba_scribo/verba_linea.h"

typedef struct CliTerminalShellCfg CliTerminalShellCfg;

// how long a shell starting is quiet before the text typed while it started goes to it, and how long a shell hung up
// on goes on before it is killed
#define CLI_TERMINAL_SETTLE_MS 300ULL
#define CLI_TERMINAL_HANG_UP_MS 1000ULL
// the bytes typed while a shell starts that wait for it; the pieces of a shell's ring, a power of two, and the bytes of
// one, its count first
#define CLI_TERMINAL_WAITING_MAX 4096u
#define CLI_TERMINAL_PIECES 32u
#define CLI_TERMINAL_PIECE_BYTES 4096u

// where a shell stands: not started, running, or ended with its end printed
#define CLI_TERMINAL_SHELL_IDLE 0u
#define CLI_TERMINAL_SHELL_RUNNING 1u
#define CLI_TERMINAL_SHELL_EXITED 2u

// One shell: the process; its ring, the ring's cell and the wake its reader waits on while the ring is full; the
// threads that read and wait for it and how many still run; whether it ended and its exit code, -1 where a signal
// ended it, the waiter's until `ended` is 1; the function that wakes its owner; where it stands; whether its
// pseudo-terminal was let go, whether it was hung up on and when, and whether it was killed; whether typed text goes
// straight to it, whether it has written and when it last did; and the text typed while it started
typedef struct
{
    CliSpawned spawned;
    mmgr_ring output_ring;
    unsigned char *ring_cell;
    CliWake wake;
    CliThread reader;
    CliThread waiter;
    embed_atomic_u32 threads;
    embed_atomic_u32 ended;
    long long code;
    void (*wake_owner)(void);
    unsigned int state;
    int closed;
    int hung_up;
    unsigned long long hung_up_ms;
    int killed;
    int ready;
    int written;
    unsigned long long written_ms;
    unsigned int waiting_size;
    unsigned char waiting[CLI_TERMINAL_WAITING_MAX];
} CliTerminalShell;

// the shell started in `folder`, the folder this process stands in where it is NULL, its pseudo-terminal the size of
// `screen`, `wake_owner` called from its threads each time it writes or ends: `words`, the program first and NULL
// after the last, or where `words` is NULL the reader's shell: ORIOR_SHELL, else on Windows Git's bash as a login shell
// and cmd where there is none, else SHELL as a login shell and bash where it is not set. Its environment this
// process's with TERM xterm-256color, COLORTERM truecolor, the PATH a run takes with orior's folder after it, and
// PROMPT_COMMAND reporting bash's folder: 1, or 0 with why put after `err_msg` and the shell as it was
int cli_terminal_shell_start(const CliTerminalShellCfg *args);
// `size` bytes of `bytes` typed into the shell, waiting while it starts; a running shell told its screen's size; and
// the shell hung up on, after which it ends and its owner drains it until it stands exited
void cli_terminal_shell_write(const CliTerminalShellCfg *args);
void cli_terminal_shell_resize(const CliTerminalShellCfg *args);
void cli_terminal_shell_end(const CliTerminalShellCfg *args);
// on the owner's thread at `now_ms`: what the shell wrote read onto `screen`, read past where it is NULL, the screen's
// answers sent back, the text that waited sent once the shell settles, a shell hung up on killed once it goes on too
// long, and an ended shell's end printed and its process, ring and threads let go: 1 where the screen changed or the
// shell ended, else 0
int cli_terminal_shell_drain(const CliTerminalShellCfg *args);
// the clock's reading the shell next has something to do at with no output coming: the text that waited sent, or a
// shell hung up on killed; 0 for none
unsigned long long cli_terminal_shell_next_ms(const CliTerminalShellCfg *args);
struct CliTerminalShellCfg
{
    CliTerminalShell *shell;
    CliTerminalScreen *screen;
    const char *const *words;
    const char *folder;
    void (*wake_owner)(void);
    const unsigned char *bytes;
    unsigned long long size;
    unsigned long long now_ms;
    VerbaLinea *err_msg;
};

typedef struct
{
    int (*start)(const CliTerminalShellCfg *args);
    void (*write)(const CliTerminalShellCfg *args);
    void (*resize)(const CliTerminalShellCfg *args);
    void (*end)(const CliTerminalShellCfg *args);
    int (*drain)(const CliTerminalShellCfg *args);
    unsigned long long (*next_ms)(const CliTerminalShellCfg *args);
} CliTerminalShellNs;
EMBED_TABLE_LAYOUT(CliTerminalShellNs, start, write, resize, end, drain, next_ms);
EMBED_TABLE_STORAGE CliTerminalShellNs cli_terminal_shell EMBED_UNUSED = {
    .start = cli_terminal_shell_start,
    .write = cli_terminal_shell_write,
    .resize = cli_terminal_shell_resize,
    .end = cli_terminal_shell_end,
    .drain = cli_terminal_shell_drain,
    .next_ms = cli_terminal_shell_next_ms,
};

#endif
