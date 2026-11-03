// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shells.h: a terminal's shell, a passthrough: started in a pseudo-terminal the size of its screen, what is typed
// written to it, and on its owner's thread what it wrote and waits in its pipe read onto the screen, never more than
// waits so no read waits. The screen's answers go back to it as they come; text typed while it starts waits until it
// has written and then been quiet CLI_TERMINAL_SETTLE_MS, as the system's terminal reads its first input as the answer
// to where the cursor is and the shell takes keys once its prompt is up. A shell that ends prints `[exited CODE]` on
// its screen; a shell hung up on and still going CLI_TERMINAL_HANG_UP_MS later is killed (shells.c)
#ifndef CLI_TERMINAL_SHELLS_H
#define CLI_TERMINAL_SHELLS_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"
#include "screen.h"
#include "verba_scribo/verba_linea.h"

typedef struct CliTerminalShellCfg CliTerminalShellCfg;

// how long a shell starting is quiet before the text typed while it started goes to it; how long a shell hung up on
// goes on before it is killed; and how often a running shell's pipe is looked at
#define CLI_TERMINAL_SETTLE_MS 300ULL
#define CLI_TERMINAL_HANG_UP_MS 1000ULL
#define CLI_TERMINAL_LOOK_MS 16ULL
// the bytes typed while a shell starts that wait for it
#define CLI_TERMINAL_WAITING_MAX 4096u

// where a shell stands: not started, running, or ended with its end printed
#define CLI_TERMINAL_SHELL_IDLE 0u
#define CLI_TERMINAL_SHELL_RUNNING 1u
#define CLI_TERMINAL_SHELL_EXITED 2u

// One shell: the process; where it stands; whether it was hung up on and when, and whether it was killed; whether typed
// text goes straight to it, whether it has written and when it last did; and the text typed while it started
typedef struct
{
    CliSpawned spawned;
    unsigned int state;
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
// `screen`: `words`, the program first and NULL after the last, or where `words` is NULL the reader's shell:
// ORIOR_SHELL, else on Windows Git's bash as a login shell and cmd where there is none, else SHELL as a login shell and
// bash where it is not set. Its environment this process's with TERM xterm-256color, COLORTERM truecolor, the PATH a
// run takes with orior's folder after it, and PROMPT_COMMAND reporting bash's folder: 1, or 0 with why put after
// `err_msg` and the shell as it was
int cli_terminal_shell_start(const CliTerminalShellCfg *args);
// `size` bytes of `bytes` typed into the shell, waiting while it starts; a running shell told its screen's size; and
// the shell hung up on at `now_ms`, after which it ends and its owner drains it until it stands exited
void cli_terminal_shell_write(const CliTerminalShellCfg *args);
void cli_terminal_shell_resize(const CliTerminalShellCfg *args);
void cli_terminal_shell_end(const CliTerminalShellCfg *args);
// at `now_ms`: what waits in the shell's pipe read onto `screen`, read past where it is NULL, the screen's answers sent
// back, the text that waited sent once the shell settles, a shell hung up on killed once it goes on too long, and an
// ended shell's end printed and its process let go: 1 where the screen changed or the shell ended, else 0
int cli_terminal_shell_drain(const CliTerminalShellCfg *args);
// the clock's reading the shell is next drained at, `now_ms` and CLI_TERMINAL_LOOK_MS on while it runs; 0 for none
unsigned long long cli_terminal_shell_next_ms(const CliTerminalShellCfg *args);
struct CliTerminalShellCfg
{
    CliTerminalShell *shell;
    CliTerminalScreen *screen;
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
