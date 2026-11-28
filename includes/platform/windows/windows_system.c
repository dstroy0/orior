// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_system.c: what the command line requests of Windows itself: its randomness, the folder it uses for passing
// files, an event a started process inherits, by which ipykernel is interrupted, when the machine started, and the
// reader's log in
#include "../../../ui/cli/src/command_line/cli_internal.h"
#include "../../../ui/cli/src/text/text.h"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <ntsecapi.h>

int cli_random_bytes(const CliSizeBytesCtx *args)
{
    unsigned char *const into = (unsigned char *)args->bytes_out;
    unsigned long long supplied = 0ULL;
    while (supplied < args->size)
    {
        const unsigned long long left = args->size - supplied;
        // a piece is at max what a ULONG counts
        const ULONG piece = (left > 0xFFFFFFFFULL) ? 0xFFFFFFFFUL : (ULONG)left;
        if (!RtlGenRandom(into + supplied, piece))
        {
            return 0;
        }
        supplied += piece;
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
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_POOL_BYTES}) + 1u});
        return;
    }
    embed_word end = cellul.len(&(CatenaFinitaCfg){.src = args->folder_out, .cap = CLI_POOL_BYTES});
    end = (embed_word)(3u + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                       .text = args->folder_out + 3u, .at = end - 3u, .set = "\\/"));
    args->folder_out[end] = '\0';
}

unsigned long long cli_event_inheritable(void)
{
    SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    const HANDLE event = CreateEventW(&attributes, FALSE, FALSE, NULL);
    // a handle is a pointer's width, which an unsigned long long stores
    return (event != NULL) ? (unsigned long long)(embed_word)event : 0ULL;
}

int cli_event_set(const CliEventCtx *args)
{
    return (args->event != 0ULL) && SetEvent((HANDLE)(embed_word)args->event);
}

void cli_event_close(const CliEventCtx *args)
{
    if (args->event != 0ULL)
    {
        CloseHandle((HANDLE)(embed_word)args->event);
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
#define CLI_SYSTEM_EPOCH_SECONDS 11644473600ULL
#define CLI_SYSTEM_TICKS_SECOND 10000000ULL

unsigned long long cli_booted_seconds(void)
{
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    const unsigned long long ticks = ((unsigned long long)now.dwHighDateTime << 32u) | now.dwLowDateTime;
    const unsigned long long seconds = (ticks / CLI_SYSTEM_TICKS_SECOND) - CLI_SYSTEM_EPOCH_SECONDS;
    const unsigned long long running = GetTickCount64() / 1000ULL;
    return (seconds > running) ? seconds - running : 0ULL;
}

void cli_clock_civil(const CliClockCivilCtx *args)
{
    CliCivil *const civil = args->civil_out;
    *civil = (CliCivil){1970u, 1u, 1u, 0u, 0u};
    // the call reads a time before 1970 as 1970, which is not negative
    const unsigned long long since = (args->seconds > 0LL) ? (unsigned long long)args->seconds : 0ULL;
    const unsigned long long ticks = (since + CLI_SYSTEM_EPOCH_SECONDS) * CLI_SYSTEM_TICKS_SECOND;
    // the ticks split into the clock's two 32-bit halves
    const FILETIME file = {(DWORD)(ticks & 0xFFFFFFFFULL), (DWORD)(ticks >> 32u)};
    SYSTEMTIME universal;
    SYSTEMTIME local;
    if (!FileTimeToSystemTime(&file, &universal) || !SystemTimeToTzSpecificLocalTime(NULL, &universal, &local))
    {
        return;
    }
    *civil = (CliCivil){local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute};
}

// the logon session this process's token is in, which every log in makes new, its id written in hexadecimal
void cli_logon_by_name(const CliCapacityLogonCtx *args)
{
    VerbaLinea named = {args->logon, args->capacity_bytes, 0ULL};
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

// the names Go and .NET give the processor the build targets
#if defined(__x86_64__) || defined(_M_X64)
#define SYSTEM_GO_PROCESSOR "amd64"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define SYSTEM_GO_PROCESSOR "arm64"
#elif defined(__i386__) || defined(_M_IX86)
#define SYSTEM_GO_PROCESSOR "x86"
#elif defined(__arm__) || defined(_M_ARM)
#define SYSTEM_GO_PROCESSOR "arm"
#else
#define SYSTEM_GO_PROCESSOR "riscv64"
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
#define SYSTEM_DOTNET_RUNTIME "win-arm64"
#else
#define SYSTEM_DOTNET_RUNTIME "win-x64"
#endif
#if defined(__x86_64__) || defined(_M_X64)
#define SYSTEM_RUST_PROCESSOR "x86_64"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define SYSTEM_RUST_PROCESSOR "aarch64"
#elif defined(__i386__) || defined(_M_IX86)
#define SYSTEM_RUST_PROCESSOR "x86"
#else
#define SYSTEM_RUST_PROCESSOR "arm"
#endif

// the folders Windows saves its fonts in, for every reader and for the reader alone
static const CliSystemFolder s_font_folders[2] = {{"WINDIR", "/Fonts"}, {"LOCALAPPDATA", "/Microsoft/Windows/Fonts"}};

// the endings a name on the PATH answers to
static const char *const s_program_ends[3] = {".exe", ".cmd", ".bat"};

// the folder that holds the reader's settings
static const CliSystemFolder s_config_folders[1] = {{"APPDATA", ""}};

// Windows' facts
const CliSystemFacts g_cli_system_facts = {
    .program_end = ".exe",
    .program_ends = s_program_ends,
    .program_end_count = 3u,
    .list_separator = ';',
    .list_quoted = 1,
    .path_separator = '\\',
    .path_separators = "/\\",
    .case_folded = 1,
    .host_variable = "COMPUTERNAME",
    .home_variable = "USERPROFILE",
    .system_name = "windows",
    .rust_processor = SYSTEM_RUST_PROCESSOR,
    .env_bin = "Scripts",
    .opener = "rundll32",
    .opener_word = "url.dll,FileProtocolHandler",
    .font_folders = s_font_folders,
    .font_folder_count = 2u,
    .fonts_nested = 0,
    .path_in_profiles = 0,
    .zsh_default = 0,
    .config_folders = s_config_folders,
    .config_folder_count = 1u,
    .file_opener = "explorer",
    .python_name = "python",
    .signals = 0,
    .lldb_watch_by_script = 1,
    .go_system = "windows",
    .go_processor = SYSTEM_GO_PROCESSOR,
    .dotnet_runtime = SYSTEM_DOTNET_RUNTIME,
    .on_linux = 0,
};

#endif
