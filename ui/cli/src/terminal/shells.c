// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shells.c: a terminal's shell, a passthrough between its pseudo-terminal and its screen (shells.h)
#include "shells.h"
#include "../runs/runner.h"
#include "../toolchains/toolchains.h"

// the settings the reader's shell starts with, at the most; the bytes read from a shell's pipe at once; and the reads
// of one drain, at the most, so a shell that writes without rest does not hold its owner's thread
#define SHELLS_SETTINGS 5u
#define SHELLS_PIECE 4096u
#define SHELLS_PIECES 64u

typedef struct
{
    CliTerminalShell *shell;
    CliTerminalScreen *screen;
    const unsigned char *bytes;
    unsigned long long size;
    long long code;
    const char *name;
} ShellsCtx;

// What the reader's shell starts with, on the owner's thread alone: its words, the program first, the program, the
// PATH a run takes with orior's folder after it, orior's own path, and the settings of its environment
typedef struct
{
    const char *words[4];
    char program[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_TEXT];
    char own[CLI_PATH_MAX_LEN];
    CliSetting settings[SHELLS_SETTINGS];
    unsigned int setting_count;
} ShellsStart;

static ShellsStart s_start;

// the PATH a run takes into s_start.path, with the folder orior stands in after it where it fits
EMBED_INLINE void shells_path(void)
{
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_start.path,
               .size = sizeof(s_start.path));
    if (!EMBED_CALL(cli_path_capacity_internal.program_own, CliPathCapacityCtx, .path_out = s_start.own,
                    .capacity_bytes = sizeof(s_start.own)))
    {
        return;
    }
    unsigned long long cut = 0ULL;
    unsigned long long length = 0ULL;
    while (s_start.own[length] != '\0')
    {
        cut = ((s_start.own[length] == '/') || (s_start.own[length] == '\\')) ? length : cut;
        length += 1ULL;
    }
    s_start.own[cut] = '\0';
    unsigned long long path_length = 0ULL;
    while (s_start.path[path_length] != '\0')
    {
        path_length += 1ULL;
    }
    VerbaLinea line = {s_start.path, sizeof(s_start.path), path_length};
#if defined(_WIN32)
    mmgr_line_ch(&line, ';');
#else
    mmgr_line_ch(&line, ':');
#endif
    mmgr_line_put(&line, s_start.own);
    if ((cut == 0ULL) || (mmgr_line_finish(&line) == 0ULL))
    {
        s_start.path[path_length] = '\0';
    }
}

// whether the environment sets `name`, its value into s_start.program
EMBED_INLINE int shells_environment(const ShellsCtx *args)
{
    return EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = args->name,
                      .value_out = s_start.program, .capacity = sizeof(s_start.program)) &&
           (s_start.program[0] != '\0');
}

// the reader's shell into s_start.words, and the settings it starts with into s_start.settings
EMBED_INLINE void shells_reader_shell(void)
{
    s_start.words[0] = s_start.program;
    s_start.words[1] = NULL;
    s_start.words[2] = NULL;
    s_start.words[3] = NULL;
    const CliSetting settings[4] = {
        {"TERM", "xterm-256color"},
        {"COLORTERM", "truecolor"},
        {"PATH", s_start.path},
        {"PROMPT_COMMAND", "printf '\\033]7;file://%s%s\\007' \"$HOSTNAME\" \"$PWD\""},
    };
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        s_start.settings[at] = settings[at];
    }
    s_start.setting_count = 4u;
    shells_path();
    if (EMBED_CALL(shells_environment, ShellsCtx, .name = "ORIOR_SHELL"))
    {
        return;
    }
#if defined(_WIN32)
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = s_start.program, .err_msg = &unread))
    {
        // Git's bash starts a login shell in its home unless told to stay where it is started
        s_start.words[1] = "--login";
        s_start.words[2] = "-i";
        s_start.settings[s_start.setting_count] = (CliSetting){"CHERE_INVOKING", "1"};
        s_start.setting_count += 1u;
        return;
    }
    if (!EMBED_CALL(shells_environment, ShellsCtx, .name = "ComSpec"))
    {
        s_start.words[0] = "cmd.exe";
    }
#else
    if (!EMBED_CALL(shells_environment, ShellsCtx, .name = "SHELL"))
    {
        s_start.words[0] = "bash";
    }
    s_start.words[1] = "-l";
#endif
}

// `size` bytes of `bytes` written to the shell's input
EMBED_INLINE void shells_send(const ShellsCtx *args)
{
    if (args->size != 0ULL)
    {
        (void)EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = args->shell->spawned.in,
                         .bytes = args->bytes, .size = args->size);
    }
}

int cli_terminal_shell_start(const CliTerminalShellCfg *args)
{
    CliTerminalShell *const shell = args->shell;
    const CliSetting *settings = NULL;
    unsigned int setting_count = 0u;
    const char *const *words = args->words;
    if (words == NULL)
    {
        shells_reader_shell();
        words = s_start.words;
        settings = s_start.settings;
        setting_count = s_start.setting_count;
    }
    CliSpawned spawned;
    if (!EMBED_CALL(cli_words_err_msg_spawned_internal.spawn_terminal, CliWordsErrMsgSpawnedCtx, .words = words,
                    .err_msg = args->err_msg, .spawned = &spawned, .folder = args->folder, .settings = settings,
                    .count = setting_count, .cols = args->screen->columns, .rows = args->screen->rows))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = shell, .val = 0, .bytes = sizeof(*shell)});
    shell->spawned = spawned;
    shell->state = CLI_TERMINAL_SHELL_RUNNING;
    return 1;
}

void cli_terminal_shell_write(const CliTerminalShellCfg *args)
{
    CliTerminalShell *const shell = args->shell;
    if ((shell->state != CLI_TERMINAL_SHELL_RUNNING) || shell->hung_up)
    {
        return;
    }
    if (shell->ready)
    {
        EMBED_CALL(shells_send, ShellsCtx, .shell = shell, .bytes = args->bytes, .size = args->size);
        return;
    }
    const unsigned long long left = (unsigned long long)CLI_TERMINAL_WAITING_MAX - shell->waiting_size;
    const unsigned long long taken = (args->size < left) ? args->size : left;
    memor.cpy(&(MemoriaCfg){.dst = shell->waiting + shell->waiting_size, .src = args->bytes, .bytes = taken});
    // what is taken is at most CLI_TERMINAL_WAITING_MAX, which an unsigned int counts
    shell->waiting_size += (unsigned int)taken;
}

void cli_terminal_shell_resize(const CliTerminalShellCfg *args)
{
    const CliTerminalShell *const shell = args->shell;
    if (shell->state == CLI_TERMINAL_SHELL_RUNNING)
    {
        (void)EMBED_CALL(cli_cols_internal.spawn_terminal_resize, CliColsCtx, .cols = args->screen->columns,
                         .rows = args->screen->rows, .spawned_read = &shell->spawned);
    }
}

void cli_terminal_shell_end(const CliTerminalShellCfg *args)
{
    CliTerminalShell *const shell = args->shell;
    if ((shell->state != CLI_TERMINAL_SHELL_RUNNING) || shell->hung_up)
    {
        return;
    }
    EMBED_CALL(cli_cols_internal.spawn_terminal_hung_up, CliColsCtx, .spawned_read = &shell->spawned);
    shell->hung_up = 1;
    shell->hung_up_ms = args->now_ms;
    shell->waiting_size = 0u;
}

// what waits in the shell's pipe read onto the screen, or read past where there is none, each read no more than waits,
// each answer the screen has for the shell sent back after the bytes that asked: 1 where any came
EMBED_INLINE int shells_pipe_read(const ShellsCtx *args)
{
    CliTerminalShell *const shell = args->shell;
    CliTerminalScreen *const screen = args->screen;
    int heard = 0;
    for (unsigned int pieces = 0u; pieces < SHELLS_PIECES; pieces += 1u)
    {
        const unsigned long long waiting = EMBED_CALL(cli_capacity_pipe_handle_internal.spawn_waiting,
                                                      CliCapacityPipeHandleCtx, .pipe_handle = shell->spawned.out);
        if (waiting == 0ULL)
        {
            break;
        }
        unsigned char piece[SHELLS_PIECE];
        const unsigned long long size = EMBED_CALL(
            cli_capacity_pipe_handle_internal.spawn_read, CliCapacityPipeHandleCtx, .pipe_handle = shell->spawned.out,
            .piece = (char *)piece, .capacity = (waiting < sizeof(piece)) ? waiting : sizeof(piece));
        if (size == 0ULL)
        {
            break;
        }
        heard = 1;
        if (screen != NULL)
        {
            EMBED_CALL(cli_terminal_screen_bytes.write, CliTerminalScreenBytesCfg, .screen = screen, .bytes = piece,
                       .size = size);
            EMBED_CALL(shells_send, ShellsCtx, .shell = shell, .bytes = screen->answer, .size = screen->answer_size);
            screen->answer_size = 0u;
        }
    }
    return heard;
}

// the end of the shell printed on its screen, `[exited CODE]` with no code where a signal ended it, and its process
// let go
EMBED_INLINE void shells_exited(const ShellsCtx *args)
{
    CliTerminalShell *const shell = args->shell;
    // a pseudo-console's last output comes once it is let go, and its pipe ends after it
    EMBED_CALL(cli_spawned_internal.spawn_terminal_close, CliSpawnedCtx, .spawned = &shell->spawned);
    (void)EMBED_CALL(shells_pipe_read, ShellsCtx, .shell = shell, .screen = args->screen);
    if (args->screen != NULL)
    {
        char text[48];
        VerbaLinea line = {text, sizeof(text), 0ULL};
        mmgr_line_put(&line, "\r\n[exited ");
#if defined(_WIN32)
        const int signalled = 0;
#else
        const int signalled = args->code < 0LL;
#endif
        if (!signalled)
        {
            mmgr_line_put(&line, (args->code < 0LL) ? "-" : "");
            // a code's size, its sign put apart, is what an unsigned long long counts
            mmgr_line_decimal(
                &line, (args->code < 0LL) ? (0ULL - (unsigned long long)args->code) : (unsigned long long)args->code,
                1u);
        }
        mmgr_line_put(&line, "]\r\n");
        EMBED_CALL(cli_terminal_screen_bytes.write, CliTerminalScreenBytesCfg, .screen = args->screen,
                   .bytes = (const unsigned char *)text, .size = mmgr_line_ok(&line) ? line.at : 0ULL);
    }
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &shell->spawned);
    shell->waiting_size = 0u;
    shell->ready = 0;
    shell->state = CLI_TERMINAL_SHELL_EXITED;
}

int cli_terminal_shell_drain(const CliTerminalShellCfg *args)
{
    CliTerminalShell *const shell = args->shell;
    if (shell->state != CLI_TERMINAL_SHELL_RUNNING)
    {
        return 0;
    }
    const int heard = EMBED_CALL(shells_pipe_read, ShellsCtx, .shell = shell, .screen = args->screen);
    if (heard)
    {
        shell->written = 1;
        shell->written_ms = args->now_ms;
    }
    if (!shell->ready && shell->written && (args->now_ms >= (shell->written_ms + CLI_TERMINAL_SETTLE_MS)))
    {
        shell->ready = 1;
        EMBED_CALL(shells_send, ShellsCtx, .shell = shell, .bytes = shell->waiting, .size = shell->waiting_size);
        shell->waiting_size = 0u;
    }
    if (shell->hung_up && !shell->killed && (args->now_ms >= (shell->hung_up_ms + CLI_TERMINAL_HANG_UP_MS)))
    {
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &shell->spawned);
        shell->killed = 1;
    }
    long long code = 0LL;
    if (EMBED_CALL(cli_spawned_internal.spawn_poll, CliSpawnedCtx, .spawned = &shell->spawned, .code = &code))
    {
        EMBED_CALL(shells_exited, ShellsCtx, .shell = shell, .screen = args->screen, .code = code);
        return 1;
    }
    return heard;
}

unsigned long long cli_terminal_shell_next_ms(const CliTerminalShellCfg *args)
{
    return (args->shell->state == CLI_TERMINAL_SHELL_RUNNING) ? (args->now_ms + CLI_TERMINAL_LOOK_MS) : 0ULL;
}
