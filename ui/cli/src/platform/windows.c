// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows.c: the command line's calls on Windows: this process the head of a job object that ends every process in it
// as it closes, a process started breaking away from the job, the job's members and the system's processes listed,
// every member ended, and this process's id, program and folder
#include "../comms/container.h"
#include "../text/text.h"

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

#include <shellapi.h>
#include <tlhelp32.h>

#include <stdint.h>

// the job this process heads, open as long as the process is: its closing as the process ends is what ends every
// member; NULL where none was made
static HANDLE s_job;

int cli_container_made(void)
{
    if (s_job != NULL)
    {
        return 1;
    }
    const HANDLE job = CreateJobObjectW(NULL, NULL);
    if (job == NULL)
    {
        return 0;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    memor.set(&(MemoriaCfg){.dst = &limits, .val = 0, .bytes = sizeof(limits)});
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
    const BOOL set = SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    if (!set || !AssignProcessToJobObject(job, GetCurrentProcess()))
    {
        CloseHandle(job);
        return 0;
    }
    s_job = job;
    return 1;
}

int cli_started_outside(const char *const *words, unsigned int *id)
{
    static char s_line[32768];
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    for (unsigned int at = 0u; words[at] != NULL; at += 1u)
    {
        const int quoted = (words[at][0] == '\0') || (words[at][cli_text_upto(words[at], " \t\"")] != '\0');
        mmgr_line_put(&line, (at == 0u) ? "" : " ");
        mmgr_line_put(&line, quoted ? "\"" : "");
        mmgr_line_put(&line, words[at]);
        mmgr_line_put(&line, quoted ? "\"" : "");
    }
    if (mmgr_line_finish(&line) == 0ull)
    {
        return 0;
    }
    STARTUPINFOA startup;
    memor.set(&(MemoriaCfg){.dst = &startup, .val = 0, .bytes = sizeof(startup)});
    startup.cb = sizeof(startup);
    // none of the starter's standard handles, so no reader of the starter's output waits on a process that outlives it
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = NULL;
    startup.hStdOutput = NULL;
    startup.hStdError = NULL;
    PROCESS_INFORMATION started;
    memor.set(&(MemoriaCfg){.dst = &started, .val = 0, .bytes = sizeof(started)});
    // inside a run's job, which lets nothing break away, it starts within the run
    BOOL made =
        CreateProcessA(NULL, s_line, NULL, NULL, FALSE, CREATE_BREAKAWAY_FROM_JOB, NULL, NULL, &startup, &started);
    if (!made)
    {
        made = CreateProcessA(NULL, s_line, NULL, NULL, FALSE, 0u, NULL, NULL, &startup, &started);
    }
    if (!made)
    {
        return 0;
    }
    *id = started.dwProcessId;
    CloseHandle(started.hThread);
    CloseHandle(started.hProcess);
    return 1;
}

unsigned int cli_container_members(unsigned int *ids, unsigned int room)
{
    if (s_job == NULL)
    {
        return 0u;
    }
    // room for 4096 ids after the list's two counts
    static ULONG_PTR s_list[4096u + 2u];
    JOBOBJECT_BASIC_PROCESS_ID_LIST *const list = (JOBOBJECT_BASIC_PROCESS_ID_LIST *)s_list;
    if (!QueryInformationJobObject(s_job, JobObjectBasicProcessIdList, list, sizeof(s_list), NULL))
    {
        return 0u;
    }
    const DWORD own = GetCurrentProcessId();
    unsigned int count = 0u;
    for (DWORD at = 0u; (at < list->NumberOfProcessIdsInList) && (count < room); at += 1u)
    {
        // a process id fits the 32 bits the system gives it
        const unsigned int id = (unsigned int)list->ProcessIdList[at];
        if (id != own)
        {
            ids[count] = id;
            count += 1u;
        }
    }
    return count;
}

unsigned int cli_processes_listed(CliProcess *processes, unsigned int room)
{
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0u);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0u;
    }
    PROCESSENTRY32W entry;
    memor.set(&(MemoriaCfg){.dst = &entry, .val = 0, .bytes = sizeof(entry)});
    entry.dwSize = sizeof(entry);
    unsigned int count = 0u;
    BOOL more = Process32FirstW(snapshot, &entry);
    while (more && (count < room))
    {
        processes[count].id = entry.th32ProcessID;
        processes[count].parent = entry.th32ParentProcessID;
        count += 1u;
        more = Process32NextW(snapshot, &entry);
    }
    CloseHandle(snapshot);
    return count;
}

void cli_container_members_ended(void)
{
    static unsigned int s_ids[4096];
    const unsigned int count = cli_container_members(s_ids, 4096u);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, s_ids[at]);
        if (process != NULL)
        {
            TerminateProcess(process, 1u);
            CloseHandle(process);
        }
    }
}

unsigned int cli_process_own(void)
{
    return GetCurrentProcessId();
}

int cli_program_own(char *path, size_t room)
{
    // a room is far below what a DWORD counts
    const DWORD length = GetModuleFileNameA(NULL, path, (DWORD)room);
    return (length != 0u) && (length < room);
}

int cli_errors_terminal(void)
{
    return GetFileType(GetStdHandle(STD_ERROR_HANDLE)) == FILE_TYPE_CHAR;
}

int cli_output_terminal(void)
{
    return GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_CHAR;
}

unsigned int cli_arguments(int count, char **given, const char **words, unsigned int room)
{
    (void)count;
    (void)given;
    static char s_text[1u << 16u];
    int wide_count = 0;
    LPWSTR *const wide = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    if (wide == NULL)
    {
        return 0u;
    }
    unsigned int taken = 0u;
    int used = 0;
    for (int at = 1; (at < wide_count) && (taken < room); at += 1)
    {
        const int size =
            WideCharToMultiByte(CP_UTF8, 0, wide[at], -1, s_text + used, (int)sizeof(s_text) - used, NULL, NULL);
        if (size <= 0)
        {
            break;
        }
        words[taken] = s_text + used;
        taken += 1u;
        used += size;
    }
    LocalFree(wide);
    return taken;
}

void cli_console_let_go(void)
{
    DWORD list[2] = {0u, 0u};
    if (GetConsoleProcessList(list, 2u) == 1u)
    {
        FreeConsole();
    }
}

int cli_folder_current(char *path, size_t room)
{
    // a room is far below what a DWORD counts
    const DWORD length = GetCurrentDirectoryA((DWORD)room, path);
    return (length != 0u) && (length < room);
}

int cli_file_runnable(const char *path)
{
    return query_is_file(path);
}

int cli_is_link(const char *path)
{
    const DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0u);
}

unsigned long long cli_clock_milliseconds(void)
{
    return GetTickCount64();
}

int cli_raised_platform(const char *from, const char *to, VerbaLinea *err_msg)
{
    char program[CLI_PATH_MAX_LEN];
    char parameters[(2u * CLI_PATH_MAX_LEN) + 32u];
    VerbaLinea line = {parameters, sizeof(parameters), 0ull};
    mmgr_line_put(&line, "write-raised \"");
    mmgr_line_put(&line, from);
    mmgr_line_put(&line, "\" \"");
    mmgr_line_put(&line, to);
    mmgr_line_ch(&line, '"');
    if (!cli_program_own(program, sizeof(program)) || (mmgr_line_finish(&line) == 0ull))
    {
        mmgr_line_put(err_msg, "orior's own program could not be named");
        return 0;
    }
    static wchar_t s_program[CLI_PATH_MAX_LEN];
    static wchar_t s_parameters[(2u * CLI_PATH_MAX_LEN) + 32u];
    MultiByteToWideChar(CP_UTF8, 0u, program, -1, s_program, (int)(sizeof(s_program) / sizeof(s_program[0])));
    MultiByteToWideChar(CP_UTF8, 0u, parameters, -1, s_parameters,
                        (int)(sizeof(s_parameters) / sizeof(s_parameters[0])));
    SHELLEXECUTEINFOW info;
    memor.set(&(MemoriaCfg){.dst = &info, .val = 0, .bytes = sizeof(info)});
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE;
    info.lpVerb = L"runas";
    info.lpFile = s_program;
    info.lpParameters = s_parameters;
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info))
    {
        const DWORD error = GetLastError();
        if (error == ERROR_CANCELLED)
        {
            mmgr_line_put(err_msg, "the reader did not give the rights");
        }
        else
        {
            mmgr_line_put(err_msg, "the system's prompt for the rights failed (");
            mmgr_line_decimal(err_msg, error, 0u);
            mmgr_line_ch(err_msg, ')');
        }
        return 0;
    }
    DWORD code = 1u;
    WaitForSingleObject(info.hProcess, INFINITE);
    GetExitCodeProcess(info.hProcess, &code);
    CloseHandle(info.hProcess);
    if (code != 0u)
    {
        mmgr_line_put(err_msg, "the raised write of ");
        mmgr_line_put(err_msg, to);
        mmgr_line_put(err_msg, " failed (");
        mmgr_line_decimal(err_msg, code, 0u);
        mmgr_line_ch(err_msg, ')');
        return 0;
    }
    return 1;
}

// the system's sockets started once
static int cli_sockets_started(void)
{
    static int s_started;
    if (!s_started)
    {
        WSADATA data;
        s_started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }
    return s_started;
}

int cli_socket_connected(CliSocket *sock, const char *host, unsigned int port, VerbaLinea *err_msg)
{
    sock->handle = 0ull;
    char service[16];
    VerbaLinea service_line = {service, sizeof(service), 0ull};
    mmgr_line_decimal(&service_line, port, 0u);
    mmgr_line_finish(&service_line);
    ADDRINFOA hints;
    memor.set(&(MemoriaCfg){.dst = &hints, .val = 0, .bytes = sizeof(hints)});
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    ADDRINFOA *found = NULL;
    if (!cli_sockets_started() || (getaddrinfo(host, service, &hints, &found) != 0))
    {
        mmgr_line_put(err_msg, host);
        mmgr_line_ch(err_msg, ':');
        mmgr_line_put(err_msg, service);
        mmgr_line_put(err_msg, ": the name was not found");
        return 0;
    }
    SOCKET made = INVALID_SOCKET;
    for (ADDRINFOA *at = found; (at != NULL) && (made == INVALID_SOCKET); at = at->ai_next)
    {
        made = socket(at->ai_family, at->ai_socktype, at->ai_protocol);
        if ((made != INVALID_SOCKET) && (connect(made, at->ai_addr, (int)at->ai_addrlen) != 0))
        {
            closesocket(made);
            made = INVALID_SOCKET;
        }
    }
    freeaddrinfo(found);
    if (made == INVALID_SOCKET)
    {
        mmgr_line_put(err_msg, host);
        mmgr_line_ch(err_msg, ':');
        mmgr_line_put(err_msg, service);
        mmgr_line_put(err_msg, ": the connection was refused");
        return 0;
    }
    const BOOL no_delay = TRUE;
    setsockopt(made, IPPROTO_TCP, TCP_NODELAY, (const char *)&no_delay, sizeof(no_delay));
    // a socket is a pointer's width, which an unsigned long long stores; one past it so that 0 stands for none
    sock->handle = (unsigned long long)made + 1ull;
    return 1;
}

int cli_socket_written(const CliSocket *sock, const void *bytes, unsigned long long size)
{
    const SOCKET handle_at = (SOCKET)(sock->handle - 1ull);
    unsigned long long put = 0ull;
    while (put < size)
    {
        // a piece is at most a gibibyte, which an int counts
        const int piece = (int)(((size - put) < (1ull << 30u)) ? (size - put) : (1ull << 30u));
        const int sent = send(handle_at, (const char *)bytes + put, piece, 0);
        if (sent <= 0)
        {
            return 0;
        }
        put += (unsigned long long)sent;
    }
    return 1;
}

int cli_socket_read(const CliSocket *sock, void *bytes, unsigned long long size)
{
    const SOCKET handle_at = (SOCKET)(sock->handle - 1ull);
    unsigned long long got = 0ull;
    while (got < size)
    {
        const int piece = (int)(((size - got) < (1ull << 30u)) ? (size - got) : (1ull << 30u));
        const int read = recv(handle_at, (char *)bytes + got, piece, 0);
        if (read <= 0)
        {
            return 0;
        }
        got += (unsigned long long)read;
    }
    return 1;
}

long long cli_socket_some(const CliSocket *sock, void *bytes, unsigned long long room)
{
    // a piece is at most a gibibyte, which an int counts
    const int piece = (int)((room < (1ull << 30u)) ? room : (1ull << 30u));
    const int read = recv((SOCKET)(sock->handle - 1ull), (char *)bytes, piece, 0);
    if (read > 0)
    {
        return read;
    }
    if (read == 0)
    {
        return CLI_SOCKET_CLOSED;
    }
    return (WSAGetLastError() == WSAETIMEDOUT) ? CLI_SOCKET_WAITED : CLI_SOCKET_FAILED;
}

void cli_socket_waited(const CliSocket *sock, unsigned int milliseconds)
{
    const DWORD wait = milliseconds;
    setsockopt((SOCKET)(sock->handle - 1ull), SOL_SOCKET, SO_RCVTIMEO, (const char *)&wait, sizeof(wait));
}

void cli_socket_write_waited(const CliSocket *sock, unsigned int milliseconds)
{
    const DWORD wait = milliseconds;
    setsockopt((SOCKET)(sock->handle - 1ull), SOL_SOCKET, SO_SNDTIMEO, (const char *)&wait, sizeof(wait));
}

void cli_socket_ended(const CliSocket *sock)
{
    if (sock->handle != 0ull)
    {
        shutdown((SOCKET)(sock->handle - 1ull), SD_BOTH);
    }
}

void cli_socket_closed(CliSocket *sock)
{
    if (sock->handle != 0ull)
    {
        closesocket((SOCKET)(sock->handle - 1ull));
    }
    sock->handle = 0ull;
}

int cli_port_free(unsigned int *port, VerbaLinea *err_msg)
{
    SOCKET made = cli_sockets_started() ? socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) : INVALID_SOCKET;
    struct sockaddr_in address;
    memor.set(&(MemoriaCfg){.dst = &address, .val = 0, .bytes = sizeof(address)});
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int size = (int)sizeof(address);
    const int bound = (made != INVALID_SOCKET) && (bind(made, (const struct sockaddr *)&address, size) == 0) &&
                      (getsockname(made, (struct sockaddr *)&address, &size) == 0);
    if (made != INVALID_SOCKET)
    {
        closesocket(made);
    }
    if (!bound)
    {
        mmgr_line_put(err_msg, "127.0.0.1: no port could be bound");
        return 0;
    }
    *port = ntohs(address.sin_port);
    return 1;
}

#endif
