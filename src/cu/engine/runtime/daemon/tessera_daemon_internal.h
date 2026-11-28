// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the tessera_daemon_*.c pieces share: its includes, types and the functions one piece calls in another
#ifndef TESSERA_DAEMON_INTERNAL_H
#define TESSERA_DAEMON_INTERNAL_H

#if !defined(_WIN32)
#define _GNU_SOURCE
#endif

#include "../obsignatio/obsignatio.h"
#include "tessera_ledger.h"
#include "tessera_measure.h"
#include "tessera_text.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#define NOMINMAX
#include <direct.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

// the permission bits of each folder of the daemon's state, the reader's own alone (platform_folders_made)
#define TESSERA_DAEMON_FOLDER_MODE 0700u
#define TESSERA_DAEMON_FOREVER 0xFFFFFFFFFFFFFFFFULL
#define TESSERA_DAEMON_MILLION 1000000ULL
#define TESSERA_HISTORY_RECORD (ENGINE_SIGNUM_BYTES + 16u)
#define TESSERA_TICKET_CAPACITY 4096u
#if defined(_WIN32)
#define TESSERA_DAEMON_SEPARATOR '\\'
#else
#define TESSERA_DAEMON_SEPARATOR '/'
#endif

// One device of the system, at its index in the one ledger. It holds its UUID's bytes, all zero for the host's
// processors at index 0. Then it holds the adapter LUID that the first job on it reports, and its measure
typedef struct
{
    unsigned char device[TESSERA_DEVICE_BYTES];
    unsigned long long luid;
    int luid_known;
    TesseraMeasure *measure;
} TesseraDaemonDevice;

typedef struct TesseraPeer
{
    struct TesseraPeer *next;
#if defined(_WIN32)
    HANDLE pipe;
    HANDLE wake;
#else
    int socket_descriptor;
    int wake[2];
#endif
    unsigned long long pid;
    TesseraProcess *process;
    // the device that the peer's submit named, NULL until then
    TesseraDaemonDevice *device;
    unsigned long long identity;
    unsigned long long lost_identity;
    EngineSignum signum;
    unsigned long long declared;
    unsigned long long located;
    unsigned long long grace_microseconds;
    unsigned long long sweep_microseconds;
    int has_decisive;
    TesseraFrame decisive;
    int has_grew;
    unsigned long long grew_to;
} TesseraPeer;

// The system's one daemon. It holds its one ledger of every job on every device, and the devices by their index in
// it. Then it holds the folder of its lock and its history, its endpoint, and its peers
typedef struct
{
    TesseraLedger ledger;
    TesseraDaemonDevice devices[TESSERA_LEDGER_DEVICES];
    unsigned int device_count;
    char root[ENGINE_PATH_CAPACITY];
    char endpoint[ENGINE_PATH_CAPACITY];
    TesseraPeer *peers;
    unsigned long long peer_count;
    int socket_activated;
#if !defined(_WIN32)
    // the socket file this daemon bound: it removes that file and never one bound after it at the same path
    dev_t endpoint_device;
    ino_t endpoint_inode;
#endif
    int living;
} TesseraDaemon;

extern TesseraDaemon g_daemon;
#if (defined(_WIN32))

extern CONDITION_VARIABLE g_daemon_changed;
#endif
#if !(defined(_WIN32))

extern pthread_cond_t g_daemon_changed;
#endif

void daemon_lock(void);

void daemon_unlock(void);

void daemon_signal_changed(void);

unsigned long long daemon_now(void);

unsigned long long daemon_wall(void);

void daemon_wait_until(unsigned long long when);


// the file `name` in the daemon's root
int daemon_state_file(const char *name, char *path);

int daemon_history_load(void);

int daemon_history_save(void);

int daemon_ticket_write(unsigned long long identity, const TesseraPeer *peer, unsigned long long peak,
                        unsigned long long measured, const char *reason);

int daemon_ticket_note(unsigned long long identity, const TesseraPeer *peer, const char *note);

// a frame of `type` for the job `identity` on `device`, which names the host's processors where it is NULL
void daemon_frame_start(TesseraFrame *frame, const TesseraDaemonDevice *device, unsigned int type,
                        unsigned long long identity);

// every device's capacity and use read into the ledger
void daemon_devices_read(void);

void daemon_job_dropped(TesseraPeer *peer, unsigned long long now, const char *reason);

void daemon_handle(TesseraPeer *peer, const TesseraFrame *frame);

#if defined(_WIN32)
DWORD WINAPI daemon_timer(LPVOID unused);
#else
void *daemon_timer(void *unused);
#endif

int daemon_peer_start(TesseraPeer *peer, unsigned long long pid);

int daemon_arguments(int count, char **arguments, unsigned long long *idle);

int daemon_socket_passed(void);

int daemon_error(void);

#endif
