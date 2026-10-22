// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_internal.h: what the kernel sources share: the kernels running, each with its sockets, its readers and the
// requests waiting on replies, and the wire: keys, ids, frames signed and messages read back
#ifndef JUPYTER_INTERNAL_H
#define JUPYTER_INTERNAL_H

#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "jupyter.h"
#include "zmtp.h"

// the most requests that wait on replies at once, the bytes a reader of published messages and a reader of replies
// each holds, half for the frames read and half for the message written, the longest line a kernel writes of itself,
// the most frames a message has, and the most nodes and bytes of a header read
#define CLI_KERNEL_WAITERS 16u
#define CLI_KERNEL_IOPUB_ROOM (1u << 24u)
#define CLI_KERNEL_REPLY_ROOM (1u << 21u)
#define CLI_KERNEL_LINE (1u << 16u)
#define CLI_KERNEL_FRAMES 64u
#define CLI_KERNEL_HEADER_NODES 512u
#define CLI_KERNEL_HEADER_TEXT (1u << 14u)

// the most bytes of content a request orior_cli makes carries, and how long a kernel's readers are waited for as it is let
// go, in milliseconds
#define CLI_KERNEL_CONTENT (1u << 22u)
#define CLI_KERNEL_READERS_WAIT 5000ull

// the readers a kernel has: what it publishes, its replies on the shell and the control sockets, and the lines its
// process writes to its output and its errors
#define CLI_KERNEL_READERS 5u

// A request waiting on its reply: its id, where the reply is written, and whether it came
typedef struct
{
    int waiting;
    char id[CLI_KERNEL_ID];
    CliLine *reply;
    int answered;
} CliKernelWaiter;

// A reader of a kernel's: the kernel, the channel it reads, its socket or its pipe, its room and the room's size,
// whether what it reads are replies, and its thread
typedef struct
{
    CliKernel *kernel;
    const char *channel;
    const CliSocket *socket;
    unsigned long long pipe;
    unsigned char *room;
    unsigned long long room_size;
    int replies;
    CliThread thread;
    CliJsonNode nodes[CLI_KERNEL_HEADER_NODES];
    CfgJsonToken tokens[CLI_KERNEL_HEADER_NODES];
    char text[CLI_KERNEL_HEADER_TEXT];
} CliKernelReader;

// A kernel: whether its place is taken and how many hold it; its spec; the words, settings, PATH and interrupt event's
// number its process is started with; its process, whether it has ended and its code; its sockets and the locks a write
// to each is made under; its key, session and connection file; the requests waiting on replies, its readers still
// reading, under its lock and woken by its wake; the port debugpy listens at, under a lock of its own; its interrupt
// event on Windows; who hears it; its readers and their rooms
struct CliKernel
{
    int taken;
    unsigned int holds;
    CliKernelSpec spec;
    CliWords words;
    CliSetting settings[CLI_KERNEL_SETTINGS + 2u];
    char path[CLI_PATH_TEXT];
    char event_number[24];
    CliSpawned spawned;
    int ended;
    long long code;
    CliSocket shell;
    CliSocket control;
    CliSocket iopub;
    CliLock shell_lock;
    CliLock control_lock;
    char key[72];
    char session[24];
    char connection[CLI_PATH_LONGEST];
    CliKernelWaiter waiters[CLI_KERNEL_WAITERS];
    unsigned int reading;
    CliLock lock;
    CliWake wake;
    CliLock debug_lock;
    unsigned int debug_port;
    int listening;
    unsigned long long interrupt_event;
    CliKernelHeard heard;
    void *context;
    char content[CLI_KERNEL_CONTENT];
    CliLock content_lock;
    char debug_reply[CLI_KERNEL_REPLY_ROOM];
    CliKernelReader readers[CLI_KERNEL_READERS];
    unsigned char iopub_room[CLI_KERNEL_IOPUB_ROOM];
    unsigned char shell_room[CLI_KERNEL_REPLY_ROOM];
    unsigned char control_room[CLI_KERNEL_REPLY_ROOM];
    unsigned char out_room[CLI_KERNEL_LINE * 8u];
    unsigned char err_room[CLI_KERNEL_LINE * 8u];
};

// the lock a kernel's holds are counted under; each kernel is a cell of its own, taken as it starts and given back as
// it is let go (jupyter.c)
extern CliLock g_cli_kernels_lock;

// a text of a spec by its place, "" for none (jupyter_specs.c)
const char *cli_kernel_text(const CliKernelSpec *spec, unsigned int at);

// the most folders Jupyter's data is read from and Pythons found
#define CLI_KERNEL_FOLDERS 64u
#define CLI_KERNEL_PYTHONS 8u

// the Pythons a notebook can run on, the tree's environment's, then the one the toolchains find, each into
// `pythons`, which holds CLI_KERNEL_PYTHONS: their count, and whether the tree has an environment; the folders
// Jupyter keeps its data in, in the order it reads them, the Pythons' own following the reader's, each into
// `folders`, which holds CLI_KERNEL_FOLDERS: their count; and whether a Python holds ipykernel, by the package's
// folder in its site-packages (jupyter_folders.c)
unsigned int cli_kernel_pythons(char (*pythons)[CLI_PATH_LONGEST], int *has_environment);
unsigned int cli_kernel_data_folders(const char (*pythons)[CLI_PATH_LONGEST], unsigned int python_count,
                                     char (*folders)[CLI_PATH_LONGEST]);
int cli_kernel_holds_ipykernel(const char *python);

// the reader `at` of the kernel's started on a thread of its own, counted while it reads (jupyter_readers.c)
void cli_kernel_reader_started(CliKernel *kernel, unsigned int at);

// the kernel's process ended where it runs, the whole tree on Windows, as a kernel's own children would outlive it, and
// the process elsewhere, and waited on; and its connection file and interrupt event let go (jupyter_requests.c)
void cli_kernel_ended(CliKernel *kernel);
void cli_kernel_forgotten(CliKernel *kernel);

// a key no other process can guess, 64 hex digits, into `key`, which holds 65; the next of the numbers ids and
// connection files are made with, from 1; an id made of the session and the next number into `id`, which holds
// CLI_KERNEL_ID; and the time now written as Jupyter's headers write it, in UTC to the microsecond (jupyter_wire.c)
void cli_kernel_key_made(char *key);
unsigned long long cli_kernel_next(void);
void cli_kernel_id_made(const char *session, char *id);
void cli_kernel_now_written(CliLine *out);

// a message of `msg_type` with `length` bytes of JSON content signed and sent on `socket` under `lock`, its header
// carrying `id`: 1, or 0 where the connection failed; and a message's frames read back, past the routing frames before
// the mark, its signature checked, written as the page reads a message with `channel`, its parent's id into `parent`,
// which holds CLI_KERNEL_ID, "" where it is longer: 1, or 0 where the frames are not a whole message signed with the
// kernel's key, or what they say is no JSON (jupyter_wire.c)
int cli_kernel_frames_sent(const CliKernel *kernel, const CliSocket *socket, CliLock *lock, const char *id,
                           const char *msg_type, const char *content, unsigned long long length);
int cli_kernel_message_written(CliKernelReader *reader, const CliZmtpFrame *frames, unsigned int count, CliLine *out,
                               char *parent);

#endif
