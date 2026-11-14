// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shells.h: a terminal's shell, a passthrough: started in a pseudo-terminal the size of its xterm, what is typed
// written to it, and on its owner's thread what it wrote and waits in its pipe read onto the xterm, never more than
// waits so no read waits. The xterm's answers go back to it as they come; text typed while it starts waits until it
// has written and then been quiet CLI_TERMINAL_SETTLE_MS, as the system's terminal reads its first input as the answer
// to where the cursor is and the shell takes keys once its prompt is up. A shell whose process has ended is read on
// until its pipe is quiet CLI_TERMINAL_EXIT_QUIET_MS, as a pseudo-console sends its last output after the process,
// and then prints `[exited CODE]` on its xterm; a shell hung up on and still going CLI_TERMINAL_HANG_UP_MS later is
// killed (shells.c)
#ifndef CLI_TERMINAL_SHELLS_H
#define CLI_TERMINAL_SHELLS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"
#include "verba_scribo/verba_linea.h"
#include "xterm.h"

typedef struct CliTerminalShellCfg CliTerminalShellCfg;

// how long a shell starting is quiet before the text typed while it started goes to it; how long a shell hung up on
// goes on before it is killed; how often a running shell's pipe is looked at; how long an ended shell's pipe is quiet
// before its end is printed, and the max its last output is waited for
#define CLI_TERMINAL_SETTLE_MS 300ULL
#define CLI_TERMINAL_HANG_UP_MS 1000ULL
#define CLI_TERMINAL_LOOK_MS 16ULL
#define CLI_TERMINAL_EXIT_QUIET_MS 100ULL
#define CLI_TERMINAL_EXIT_MAX_MS 2000ULL
// the bytes typed while a shell starts that wait for it
#define CLI_TERMINAL_WAITING_MAX 4096u

// where a shell is: not started, running, or ended with its end printed
#define CLI_TERMINAL_SHELL_IDLE 0u
#define CLI_TERMINAL_SHELL_RUNNING 1u
#define CLI_TERMINAL_SHELL_EXITED 2u

// One shell: the process; where it is; whether it was hung up on and when, and whether it was killed; whether its
// process has ended, with its code, when it ended and when its pipe last gave bytes after it; whether typed text goes
// straight to it, whether it has written and when it last did; and the text typed while it started
typedef struct
{
    CliSpawned spawned;
    unsigned int state;
    int hung_up;
    unsigned long long hung_up_ms;
    int killed;
    int ended;
    long long code;
    unsigned long long ended_ms;
    unsigned long long quiet_ms;
    int ready;
    int written;
    unsigned long long written_ms;
    unsigned int waiting_size;
    unsigned char waiting[CLI_TERMINAL_WAITING_MAX];
} CliTerminalShell;

// the shell started in `folder`, the folder this process is in where it is NULL, its pseudo-terminal the size of
// `xterm`: `words`, the program first and NULL after the last, or where `words` is NULL the reader's shell:
// ORIOR_SHELL, else on Windows Git's bash as a login shell and cmd where there is none, else SHELL as a login shell and
// bash where it is not set. Its environment this process's with TERM xterm-256color, COLORTERM truecolor, the PATH a
// run takes with orior's folder after it, and PROMPT_COMMAND reporting bash's folder: 1, or 0 with why put after
// `err_msg` and the shell as it was
int cli_terminal_shell_start(const CliTerminalShellCfg *args);
// `size` bytes of `bytes` typed into the shell, waiting while it starts; a running shell told its xterm's size; and
// the shell hung up on at `now_ms`, after which it ends and its owner drains it until it is exited
void cli_terminal_shell_write(const CliTerminalShellCfg *args);
void cli_terminal_shell_resize(const CliTerminalShellCfg *args);
void cli_terminal_shell_end(const CliTerminalShellCfg *args);
// at `now_ms`: what waits in the shell's pipe read onto `xterm`, read past where it is NULL, the xterm's answers sent
// back, the text that waited sent once the shell settles, a shell hung up on killed once it goes on too long, and an
// ended shell's end printed and its process let go: 1 where the xterm changed or the shell ended, else 0
int cli_terminal_shell_drain(const CliTerminalShellCfg *args);
// the clock's reading the shell is next drained at, `now_ms` and CLI_TERMINAL_LOOK_MS on while it runs; 0 for none
unsigned long long cli_terminal_shell_next_ms(const CliTerminalShellCfg *args);
struct CliTerminalShellCfg
{
    CliTerminalShell *shell;
    CliTerminalXterm *xterm;
    const char *const *words;
    const char *folder;
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
