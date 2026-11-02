// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_system.c: what the command line requests of Windows itself: its randomness, the folder it uses for passing
// files, an event a started process inherits, by which ipykernel is interrupted, when the machine started, and the
// reader's log in
#include "../command_line/cli_internal.h"
#include "../text/text.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <ntsecapi.h>

#include <stdint.h>

int cli_random_bytes(const CliSizeBytesCtx *args)
{
    unsigned char *const into = (unsigned char *)args->bytes_out;
    unsigned long long given = 0ull;
    while (given < args->size)
    {
        const unsigned long long left = args->size - given;
        // a piece is at most what a ULONG counts
        const ULONG piece = (left > 0xFFFFFFFFull) ? 0xFFFFFFFFul : (ULONG)left;
        if (!RtlGenRandom(into + given, piece))
        {
            return 0;
        }
        given += piece;
    }
    return 1;
}

void cli_temp_folder(const CliFolderCtx *args)
{
    const DWORD length = GetTempPathA(CLI_PATH_MAX_LEN, args->folder_out);
    if ((length == 0u) || (length >= CLI_PATH_MAX_LEN))
    {
        memor.cpy(&(MemoriaCfg){.dst = args->folder_out,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
        return;
    }
    size_t end = cellul.len(&(CatenaFinitaCfg){.src = args->folder_out, .cap = CLI_CELLS_POOL});
    end = (size_t)(3u + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg,
                                   .text = args->folder_out + 3u, .at = end - 3u, .set = "\\/"));
    args->folder_out[end] = '\0';
}

unsigned long long cli_event_inheritable(void)
{
    SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    const HANDLE event = CreateEventW(&attributes, FALSE, FALSE, NULL);
    // a handle is a pointer's width, which an unsigned long long stores
    return (event != NULL) ? (unsigned long long)(uintptr_t)event : 0ull;
}

int cli_event_set(const CliEventCtx *args)
{
    return (args->event != 0ull) && SetEvent((HANDLE)(uintptr_t)args->event);
}

void cli_event_close(const CliEventCtx *args)
{
    if (args->event != 0ull)
    {
        CloseHandle((HANDLE)(uintptr_t)args->event);
    }
}

// a folder and a file of the reader's own are the reader's alone by the profile's own rights on Windows
int cli_private_folder_init(const CliWordsErrMsgCtx *args)
{
    return EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = args->folder);
}

int cli_private_file_write(const CliPathBytesCtx *args)
{
    return EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = args->path,
                      .bytes = args->bytes, .size = args->size);
}

// the seconds from 1601, where the system's clock begins, to 1970, and the clock's ticks in a second
#define CLI_SYSTEM_EPOCH_SECONDS 11644473600ull
#define CLI_SYSTEM_TICKS_SECOND 10000000ull

unsigned long long cli_booted_seconds(void)
{
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    const unsigned long long ticks = ((unsigned long long)now.dwHighDateTime << 32u) | now.dwLowDateTime;
    const unsigned long long seconds = (ticks / CLI_SYSTEM_TICKS_SECOND) - CLI_SYSTEM_EPOCH_SECONDS;
    const unsigned long long running = GetTickCount64() / 1000ull;
    return (seconds > running) ? seconds - running : 0ull;
}

// the logon session this process's token stands in, which every log in makes new, its id written in hexadecimal
void cli_logon_by_name(const CliRoomLogonCtx *args)
{
    VerbaLinea named = {args->logon, args->room_size, 0ull};
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        TOKEN_STATISTICS statistics;
        DWORD length = 0u;
        const BOOL read = GetTokenInformation(token, TokenStatistics, &statistics, sizeof(statistics), &length);
        CloseHandle(token);
        if (read)
        {
            // the id's high word is a LONG the system gives as a number of the session, never below 0
            mmgr_line_hex(&named, (unsigned long long)(unsigned long)statistics.AuthenticationId.HighPart, 1u);
            mmgr_line_hex(&named, statistics.AuthenticationId.LowPart, 8u);
        }
    }
    (void)mmgr_line_finish(&named);
}

#endif
