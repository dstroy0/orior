// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_tessera_run.c: tessera_run on Windows: the note of a command that runs unmeasured, the interrupts left to
// the command, the paths of WSL, the processors named, the records and the life of the parent. The command's start,
// its time and its end are the platform's (platform.h)
#include "../../../src/cu/engine/runtime/daemon/tessera_run_internal.h"
#include "../platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#if (defined(_WIN32))
_Alignas(8) static const char s_run_unjoined[] =
    "  tessera_run: the command could not join a job object of its own; it runs on the processors given, unmeasured\n";

void run_unmeasured_noted(void)
{
    platform_terminal_write(1, s_run_unjoined,
                            (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = s_run_unjoined,
                                                           .cap = PLATFORM_TEXT_TO_TERMINATOR));
}

// an interrupt reaches the command too, which ends; this process waits for it, then releases its job
static BOOL WINAPI run_console_interrupt(DWORD event)
{
    return (event == CTRL_C_EVENT) || (event == CTRL_BREAK_EVENT);
}

void run_interrupts_passed(void)
{
    SetConsoleCtrlHandler(run_console_interrupt, TRUE);
}

// 1 where the command's program is wsl.exe (or wsl), whatever folder names it
int run_names_wsl(const char *program)
{
    // the program's name past the last separator of either kind
    const char *const slash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = program, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const back =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = program, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    const char *const separator = ((back != NULL) && ((slash == NULL) || (back > slash))) ? back : slash;
    const char *const base = (separator != NULL) ? (separator + 1) : program;
    return (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = base, .other = "wsl.exe", .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .ci = EMBED_TRUE)) ||
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = base, .other = "wsl", .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .ci = EMBED_TRUE));
}

// a path on a drive as WSL mounts it, X:\a\b as /mnt/x/a/b; 0 for a path on no drive character
int run_mounted_path(const char *path, char *mounted, size_t capacity)
{
    // Explicit cast reads the byte as its code point
    const int upper = EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_UPPER, .byte = (uint8_t)path[0]);
    // Explicit cast reads the byte as its code point
    const int lower = EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_LOWER, .byte = (uint8_t)path[0]);
    if ((!upper && !lower) || (path[1] != ':') || ((path[2] != '\\') && (path[2] != '/')))
    {
        return 0;
    }
    // a capital character's lower case is the same character 32 above it, still a char
    const char drive = upper ? (char)(path[0] + ('a' - 'A')) : path[0];
    VerbaLinea formed_line_1 = mmgr_line_over(mounted, capacity);
    mmgr_line_put(&formed_line_1, "/mnt/");
    mmgr_line_ch(&formed_line_1, (char)(drive));
    mmgr_line_put(&formed_line_1, "/");
    mmgr_line_put(&formed_line_1, path + 3);
    (void)mmgr_line_finish(&formed_line_1);
    const int written = (int)formed_line_1.at;
    // the call compares a non-negative length whole against the capacity
    if ((written <= 0) || ((size_t)written >= capacity))
    {
        return 0;
    }
    // each back slash, reached from the one before it, written as a forward slash
    for (size_t at = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = mounted, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = "\\");
         mounted[at] != '\0'; at += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &mounted[at],
                                               .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\\"))
    {
        mounted[at] = '/';
    }
    return 1;
}

// the Linux tessera_run as WSL names it: $TESSERA_RUN_WSL, or the tessera_run beside this program
int run_wsl_program(char *mounted, size_t capacity)
{
    char named_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const named =
        (platform_environment("TESSERA_RUN_WSL", named_text, sizeof(named_text)) != 0ULL) ? named_text : NULL;
    if ((named != NULL) && (named[0] != '\0'))
    {
        VerbaLinea formed_line_2 = mmgr_line_over(mounted, capacity);
        mmgr_line_put(&formed_line_2, named);
        (void)mmgr_line_finish(&formed_line_2);
        const int written = (int)formed_line_2.at;
        // the call compares a non-negative length whole against the capacity
        return (written > 0) && ((size_t)written < capacity);
    }
    char path[ENGINE_PATH_CAPACITY];
    const size_t length = run_own_path(path, sizeof(path));
    // the Linux program is this one's name without its extension
    if ((length < 4u) || (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = path + length - 4u, .other = ".exe",
                                      .cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_TRUE)))
    {
        return 0;
    }
    path[length - 4u] = '\0';
    return (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) && run_mounted_path(path, mounted, capacity);
}

int run_processors_named(unsigned long long processors)
{
    char text[24];
    VerbaLinea formed_line_3 = mmgr_line_over(text, sizeof(text));
    mmgr_line_decimal(&formed_line_3, processors, 1u);
    (void)mmgr_line_finish(&formed_line_3);
    const int written = (int)formed_line_3.at;
    return (written > 0) && (SetEnvironmentVariableA("TESSERA_RUN_PROCESSORS", text) != 0);
}

// the parent's record, shared only for reading. While this process lives no other opens it for writing, and by that
// its child finds that it lives, from Windows or from inside WSL
int run_record_make(RunChannel *channel, const char *path)
{
    channel->record =
        CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    return channel->record != INVALID_HANDLE_VALUE;
}

void run_record_put(RunChannel *channel, const char *line, size_t length)
{
    DWORD moved = 0u;
    SetFilePointer(channel->record, 0, NULL, FILE_BEGIN);
    // a record's line is under its capacity of a hundred and sixty bytes
    WriteFile(channel->record, line, (DWORD)length, &moved, NULL);
}

void run_record_close(RunChannel *channel)
{
    CloseHandle(channel->record);
}

// 1 while the parent lives: its record errors on an open for writing until its handles close with it
int run_parent_alive(const char *parent)
{
    const HANDLE opened = CreateFileA(parent, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (opened == INVALID_HANDLE_VALUE)
    {
        return GetLastError() == ERROR_SHARING_VIOLATION;
    }
    CloseHandle(opened);
    return 0;
}
#endif
