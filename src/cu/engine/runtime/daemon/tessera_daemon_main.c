// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tessera_daemon_main.c: the daemon's arguments and main
// the internal header comes first, since it sets the macros of the system before any header
#include "tessera_daemon_internal.h"

#include "../../../../../includes/platform/platform.h"
#include "../../engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

_Alignas(8) static const char s_daemon_lock_file[] = "daemon.lock";
_Alignas(8) static const char s_daemon_usage[] = "  tessera daemon: --idle <microseconds>\n";
_Alignas(8) static const char s_daemon_no_state[] = "  tessera daemon: no state directory could be made\n";
_Alignas(8) static const char s_daemon_gpu_unmeasured[] = "  tessera daemon: this GPU could not be measured, and is not "
                                                          "gated: ";
_Alignas(8) static const
    char s_daemon_long_socket[] = "  tessera daemon: the socket path is longer than a Unix socket allows: ";
_Alignas(8) static const char s_daemon_unbound[] = "  tessera daemon: this socket could not be bound: ";
_Alignas(8) static const char s_daemon_answered[] = "  tessera daemon: another daemon already answers at this socket: ";
_Alignas(8) static const char s_daemon_no_cores[] =
    "  tessera daemon: the host's cores could not be read, and no processor could be given to jobs\n";
#if defined(_WIN32)
_Alignas(8) static const char s_daemon_no_pipe[] = "  tessera daemon: the next pipe instance could not be made\n";
#endif

// The devices of the system opened at their index in the ledger: the host's processors at index 0, then every GPU that
// NVML lists. The call notes a GPU whose memory does not measure and leaves it out. 0 where the host's cores do not
// read
static int daemon_devices_open(void)
{
    TesseraDaemonDevice *const host = &g_daemon.devices[0];
    EMBED_CALL(memor.set, MemoriaCfg, .dst = host, .bytes = sizeof(*host), .val = 0);
    host->luid_known = 1;
    host->measure = tessera_measure_open(host->device, 0ULL);
    if (host->measure == NULL)
    {
        return 0;
    }
    g_daemon.device_count = 1u;
    unsigned char listed[TESSERA_LEDGER_DEVICES - 1u][TESSERA_DEVICE_BYTES];
    const unsigned int gpus = tessera_measure_devices(listed, TESSERA_LEDGER_DEVICES - 1u);
    for (unsigned int at = 0u; at < gpus; at += 1u)
    {
        TesseraDaemonDevice *const gpu = &g_daemon.devices[g_daemon.device_count];
        EMBED_CALL(memor.set, MemoriaCfg, .dst = gpu, .bytes = sizeof(*gpu), .val = 0);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = gpu->device, .src = listed[at], .bytes = TESSERA_DEVICE_BYTES);
        gpu->measure = tessera_measure_open(gpu->device, 0ULL);
        if (gpu->measure == NULL)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, s_daemon_gpu_unmeasured);
            tessera_bytes_hex(&engine_note.line, gpu->device, TESSERA_DEVICE_BYTES);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
            continue;
        }
        g_daemon.device_count += 1u;
    }
    return 1;
}

int program_main(int count, char **arguments)
{
    unsigned long long idle = 0ULL;
    if (!daemon_arguments(count, arguments, &idle))
    {
        {
            const char *const error_text = s_daemon_usage;
            platform_terminal_write(1, error_text,
                                    (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = error_text,
                                                                   .cap = PLATFORM_TEXT_TO_TERMINATOR));
        }
        return 2;
    }
    if (!tessera_path_root(g_daemon.root, ENGINE_PATH_CAPACITY) ||
        !platform_folders_made(g_daemon.root, TESSERA_DAEMON_FOLDER_MODE) ||
        !tessera_path_endpoint(g_daemon.endpoint, ENGINE_PATH_CAPACITY))
    {
        {
            const char *const error_text = s_daemon_no_state;
            platform_terminal_write(1, error_text,
                                    (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = error_text,
                                                                   .cap = PLATFORM_TEXT_TO_TERMINATOR));
        }
        return daemon_error();
    }
    // the daemon reads its history before the endpoint exists: no client reaches a daemon that then errors on it
    if (!tessera_ledger_open(&g_daemon.ledger))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  tessera daemon: the ledger could not be opened");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return daemon_error();
    }
    g_daemon.ledger.next_identity = daemon_wall();
    if (!daemon_history_load() || !tessera_ledger_idle(&g_daemon.ledger, daemon_now(), idle))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  tessera daemon: the history in ");
            mmgr_line_put(&engine_note.line, g_daemon.root);
            mmgr_line_put(&engine_note.line, " could not be read");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return daemon_error();
    }
#if defined(_WIN32)
    HANDLE listening =
        CreateNamedPipeA(g_daemon.endpoint, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                         PIPE_UNLIMITED_INSTANCES, TESSERA_FRAME_BYTES, TESSERA_FRAME_BYTES, 0u, NULL);
    if (listening == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
#else
    pthread_condattr_t attributes;
    pthread_condattr_init(&attributes);
    pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    pthread_cond_init(&g_daemon_changed, &attributes);
    char lock_path[ENGINE_PATH_CAPACITY];
    const int lock = daemon_state_file(s_daemon_lock_file, lock_path) ? open(lock_path, O_RDWR | O_CREAT, 0600) : -1;
    if ((lock < 0) || (flock(lock, LOCK_EX | LOCK_NB) != 0))
    {
        return 0;
    }
    g_daemon.socket_activated = daemon_socket_passed();
    int listening = 3;
    if (!g_daemon.socket_activated)
    {
        struct sockaddr_un address;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &address, .bytes = sizeof(address), .val = 0);
        address.sun_family = AF_UNIX;
        if (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = g_daemon.endpoint, .cap = PLATFORM_TEXT_TO_TERMINATOR) >=
            sizeof(address.sun_path))
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, s_daemon_long_socket);
                mmgr_line_put(&engine_note.line, g_daemon.endpoint);
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            return 1;
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = address.sun_path, .src = g_daemon.endpoint,
                   .bytes = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = g_daemon.endpoint,
                                       .cap = PLATFORM_TEXT_TO_TERMINATOR) +
                            1u);
        // a daemon that answers at the system's socket, or systemd's socket, may occupy the path
        const int probe = socket(AF_UNIX, SOCK_STREAM, 0);
        const int answered = (probe >= 0) && (connect(probe, (const struct sockaddr *)&address, sizeof(address)) == 0);
        if (probe >= 0)
        {
            close(probe);
        }
        if (answered)
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, s_daemon_answered);
                mmgr_line_put(&engine_note.line, g_daemon.endpoint);
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            return 1;
        }
        unlink(g_daemon.endpoint);
        listening = socket(AF_UNIX, SOCK_STREAM, 0);
        struct stat endpoint;
        if ((listening < 0) || (bind(listening, (const struct sockaddr *)&address, sizeof(address)) != 0) ||
            (listen(listening, SOMAXCONN) != 0) || (stat(g_daemon.endpoint, &endpoint) != 0))
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, s_daemon_unbound);
                mmgr_line_put(&engine_note.line, g_daemon.endpoint);
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            return 1;
        }
        g_daemon.endpoint_device = endpoint.st_dev;
        g_daemon.endpoint_inode = endpoint.st_ino;
    }
#endif
    if (!daemon_devices_open())
    {
        {
            const char *const error_text = s_daemon_no_cores;
            platform_terminal_write(1, error_text,
                                    (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = error_text,
                                                                   .cap = PLATFORM_TEXT_TO_TERMINATOR));
        }
        return daemon_error();
    }
    daemon_devices_read();
    g_daemon.living = 1;
#if defined(_WIN32)
    const HANDLE timer = CreateThread(NULL, 0u, daemon_timer, NULL, 0u, NULL);
    if (timer == NULL)
    {
        return 1;
    }
    CloseHandle(timer);
    for (;;)
    {
        OVERLAPPED connecting;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &connecting, .bytes = sizeof(connecting), .val = 0);
        connecting.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
        DWORD moved = 0u;
        const int connected =
            (connecting.hEvent != NULL) &&
            (ConnectNamedPipe(listening, &connecting) || (GetLastError() == ERROR_PIPE_CONNECTED) ||
             ((GetLastError() == ERROR_IO_PENDING) && GetOverlappedResult(listening, &connecting, &moved, TRUE)));
        if (connecting.hEvent != NULL)
        {
            CloseHandle(connecting.hEvent);
        }
        ULONG pid = 0u;
        TesseraPeer *const peer =
            connected ? (TesseraPeer *)platform_cell_cleared(1u, sizeof(TesseraPeer), platform_place_g_program_owner, 0)
                      : NULL;
        const int named = (peer != NULL) && GetNamedPipeClientProcessId(listening, &pid);
        if (named)
        {
            peer->pipe = listening;
            peer->wake = CreateEventA(NULL, FALSE, FALSE, NULL);
        }
        if (!named || (peer->wake == NULL) || !daemon_peer_start(peer, pid))
        {
            DisconnectNamedPipe(listening);
            CloseHandle(listening);
            platform_cell_released(peer, platform_place_g_program_owner);
        }
        listening = CreateNamedPipeA(g_daemon.endpoint, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                     PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                     PIPE_UNLIMITED_INSTANCES, TESSERA_FRAME_BYTES, TESSERA_FRAME_BYTES, 0u, NULL);
        if (listening == INVALID_HANDLE_VALUE)
        {
            {
                const char *const error_text = s_daemon_no_pipe;
                platform_terminal_write(1, error_text,
                                        (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = error_text,
                                                                       .cap = PLATFORM_TEXT_TO_TERMINATOR));
            }
            return 1;
        }
    }
#else
    pthread_t timer;
    if (pthread_create(&timer, NULL, daemon_timer, NULL) != 0)
    {
        return 1;
    }
    pthread_detach(timer);
    for (;;)
    {
        const int accepted = accept(listening, NULL, NULL);
        if (accepted < 0)
        {
            continue;
        }
        struct ucred credentials;
        socklen_t length = sizeof(credentials);
        TesseraPeer *const peer =
            (TesseraPeer *)platform_cell_cleared(1u, sizeof(TesseraPeer), platform_place_g_program_owner, 0);
        const int named = (peer != NULL) &&
                          (getsockopt(accepted, SOL_SOCKET, SO_PEERCRED, &credentials, &length) == 0) &&
                          (pipe(peer->wake) == 0);
        if (named)
        {
            peer->socket_descriptor = accepted;
            fcntl(peer->wake[0], F_SETFL, O_NONBLOCK);
            fcntl(peer->wake[1], F_SETFL, O_NONBLOCK);
        }
        // a peer's pid is positive, which fits in an unsigned long long
        if (!named || !daemon_peer_start(peer, (unsigned long long)credentials.pid))
        {
            close(accepted);
            platform_cell_released(peer, platform_place_g_program_owner);
        }
    }
#endif
}
