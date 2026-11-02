// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_internal.h: what the kernel sources share: the kernels running, each with its sockets, its readers and the
// requests waiting on replies, and the wire: keys, ids, frames signed and messages read back
#ifndef JUPYTER_INTERNAL_H
#define JUPYTER_INTERNAL_H
typedef struct CliJupyterKernelCtx CliJupyterKernelCtx;
typedef struct CliJupyterOutCtx CliJupyterOutCtx;
typedef struct CliJupyterPythonsPythonCountCtx CliJupyterPythonsPythonCountCtx;
typedef struct CliJupyterSessionCtx CliJupyterSessionCtx;
typedef struct CliJupyterPythonsCtx CliJupyterPythonsCtx;
typedef struct CliJupyterAtCtx CliJupyterAtCtx;
typedef struct CliJupyterKernelParentCtx CliJupyterKernelParentCtx;
typedef struct CliJupyterSpecCtx CliJupyterSpecCtx;
typedef struct CliJupyterCtx CliJupyterCtx;
typedef struct CliJupyterKeyCtx CliJupyterKeyCtx;

#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "embed_dispatch_layout.h"
#include "jupyter.h"
#include "zmtp.h"

// the most requests that wait on replies at once, the bytes a reader of published messages and a reader of replies
// each contains, half for the frames read and half for the message written, the longest line a kernel writes of itself,
// the most frames a message has, and the most nodes and bytes of a header read
#define CLI_KERNEL_WAITERS 16u
#define CLI_KERNEL_IOPUB_ROOM (1u << 22u)
#define CLI_KERNEL_REPLY_ROOM (1u << 19u)
#define CLI_KERNEL_LINE (1u << 16u)
#define CLI_KERNEL_FRAMES 64u
#define CLI_KERNEL_HEADER_NODES 512u
#define CLI_KERNEL_HEADER_TEXT (1u << 14u)

// the most bytes of content a request orior_cli makes carries, and how long a kernel's readers are waited for as it is
// let go, in milliseconds
#define CLI_KERNEL_CONTENT (1u << 20u)
#define CLI_KERNEL_READERS_WAIT 5000ull

// the readers a kernel has: what it publishes, its replies on the shell and the control sockets, and the lines its
// process writes to its output and its errors
#define CLI_KERNEL_READERS 5u

// A request waiting on its reply: its id, where the reply is written, and whether it came
typedef struct
{
    int waiting;
    char id[CLI_KERNEL_ID];
    VerbaLinea *reply;
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

// A kernel: whether its place is taken and how many reference it; its spec; the words, settings, PATH and interrupt
// event's number its process is started with; its process, whether it has ended and its code; its sockets and the locks
// a write to each is made under; the id of the request in flight on the shell, "" for none, and the requests queued
// behind it, each its id, its size and its message whole, a line read from `shell_queue_from`, under the shell's lock;
// its key, session and connection file; the requests waiting on replies, its readers still reading, under its lock and
// woken by its wake; the port debugpy listens at, under a lock of its own; its interrupt event on Windows; who receives
// it; its readers and their rooms
struct CliKernel
{
    int taken;
    unsigned int refs;
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
    char shell_flight[CLI_KERNEL_ID];
    VerbaLinea shell_queue;
    unsigned long long shell_queue_from;
    char key[72];
    char session[24];
    char connection[CLI_PATH_MAX_LEN];
    CliKernelWaiter waiters[CLI_KERNEL_WAITERS];
    unsigned int reading;
    CliLock lock;
    CliWake wake;
    CliLock debug_lock;
    unsigned int debug_port;
    int listening;
    unsigned long long interrupt_event;
    CliKernelRecvFn recv;
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

// the lock a kernel's references are counted under; each kernel is a cell of its own, taken as it starts and given back
// as it is let go (jupyter.c)
extern CliLock g_cli_kernels_lock;

// a text of a spec by its place, "" for none (jupyter_specs.c)
const char *cli_kernel_text(const CliJupyterSpecCtx *args);

// the most folders Jupyter's data is read from and Pythons found
#define CLI_KERNEL_FOLDERS 64u
#define CLI_KERNEL_PYTHONS 8u

// the Pythons a notebook can run on, the tree's environment's, then the one the toolchains find, each into
// `pythons`, sized for CLI_KERNEL_PYTHONS: their count, and whether the tree has an environment; the folders
// Jupyter stores its data in, in the order it reads them, the Pythons' own following the reader's, each into
// `folders`, sized for CLI_KERNEL_FOLDERS: their count; and whether a Python contains ipykernel, by the package's
// folder in its site-packages (jupyter_folders.c)
unsigned int cli_kernel_pythons(const CliJupyterPythonsCtx *args);
unsigned int cli_kernel_data_folders(const CliJupyterPythonsPythonCountCtx *args);
int cli_kernel_has_ipykernel(const CliJupyterCtx *args);

// the reader `at` of the kernel's started on a thread of its own, counted while it reads (jupyter_readers.c)
void cli_kernel_reader_start(const CliJupyterAtCtx *args);

// the kernel's process ended where it runs, the whole tree on Windows, as a kernel's own children would outlive it, and
// the process elsewhere, and waited on; and its connection file and interrupt event let go (jupyter_requests.c)
void cli_kernel_end(const CliJupyterAtCtx *args);
void cli_kernel_forgotten(const CliJupyterAtCtx *args);

// a key no other process can guess, 64 hex digits, into `key`, sized for 65; the next of the numbers ids and
// connection files are made with, from 1; an id made of the session and the next number into `id`, which is sized for
// CLI_KERNEL_ID; and the time now written as Jupyter's headers write it, in UTC to the microsecond (jupyter_wire.c)
void cli_kernel_key_init(const CliJupyterKeyCtx *args);
unsigned long long cli_kernel_next(void);
void cli_kernel_id_init(const CliJupyterSessionCtx *args);
void cli_kernel_now_write(const CliJupyterOutCtx *args);

// a message of `msg_type` with `length` bytes of JSON content signed and sent whole on `socket` under `lock`, its
// header carrying `id`, or on the shell queued where a request is in flight on it: 1, or 0 where the connection failed
// or the cells cannot meet it; the reply to `parent` received on the shell, the request in flight there done where it
// is `parent`'s, and the next queued sent; and a message's frames read back, past the routing frames before the mark,
// its signature checked, written as the page reads a message with `channel`, its parent's id into `parent`, sized for
// CLI_KERNEL_ID, "" where it is longer: 1, or 0 where the frames are not a whole message signed with the kernel's key,
// or what they contain is no JSON (jupyter_wire.c)
int cli_kernel_frames_send(const CliJupyterKernelCtx *args);
void cli_kernel_shell_answer(const CliJupyterKernelParentCtx *args);
int cli_kernel_message_write(const CliJupyterOutCtx *args);

struct CliJupyterKernelCtx
{
    CliKernel *kernel;
    const CliSocket *socket;
    CliLock *lock;
    const char *id_read;
    const char *msg_type;
    const char *content;
    unsigned long long length;
};

struct CliJupyterOutCtx
{
    VerbaLinea *out;
    CliKernelReader *reader;
    const CliZmtpFrame *frames;
    unsigned int count;
    char *parent_out;
};

struct CliJupyterPythonsPythonCountCtx
{
    const char (*pythons_callback)[CLI_PATH_MAX_LEN];
    unsigned int python_count;
    char (*folders)[CLI_PATH_MAX_LEN];
};

struct CliJupyterSessionCtx
{
    const char *session;
    char *id;
};

struct CliJupyterPythonsCtx
{
    char (*pythons)[CLI_PATH_MAX_LEN];
    int *has_environment;
};

struct CliJupyterAtCtx
{
    unsigned int at;
    CliKernel *kernel;
};

struct CliJupyterKernelParentCtx
{
    CliKernel *kernel;
    const char *parent;
};

struct CliJupyterSpecCtx
{
    const CliKernelSpec *spec;
    unsigned int at;
};

struct CliJupyterCtx
{
    const char *python;
};

struct CliJupyterKeyCtx
{
    char *key;
};

typedef struct
{
    const char *(*kernel_text)(const CliJupyterSpecCtx *args);
} CliJupyterSpecNs;
EMBED_TABLE_LAYOUT(CliJupyterSpecNs, kernel_text);
EMBED_TABLE_STORAGE CliJupyterSpecNs cli_jupyter_spec_internal EMBED_UNUSED = {
    .kernel_text = cli_kernel_text,
};

typedef struct
{
    unsigned int (*kernel_pythons)(const CliJupyterPythonsCtx *args);
} CliJupyterPythonsNs;
EMBED_TABLE_LAYOUT(CliJupyterPythonsNs, kernel_pythons);
EMBED_TABLE_STORAGE CliJupyterPythonsNs cli_jupyter_pythons_internal EMBED_UNUSED = {
    .kernel_pythons = cli_kernel_pythons,
};

typedef struct
{
    unsigned int (*kernel_data_folders)(const CliJupyterPythonsPythonCountCtx *args);
} CliJupyterPythonsPythonCountNs;
EMBED_TABLE_LAYOUT(CliJupyterPythonsPythonCountNs, kernel_data_folders);
EMBED_TABLE_STORAGE CliJupyterPythonsPythonCountNs cli_jupyter_pythons_python_count_internal EMBED_UNUSED = {
    .kernel_data_folders = cli_kernel_data_folders,
};

typedef struct
{
    int (*kernel_has_ipykernel)(const CliJupyterCtx *args);
} CliJupyterNsTable;
EMBED_TABLE_LAYOUT(CliJupyterNsTable, kernel_has_ipykernel);
EMBED_TABLE_STORAGE CliJupyterNsTable cli_jupyter_internal EMBED_UNUSED = {
    .kernel_has_ipykernel = cli_kernel_has_ipykernel,
};

typedef struct
{
    void (*kernel_reader_start)(const CliJupyterAtCtx *args);
    void (*kernel_end)(const CliJupyterAtCtx *args);
    void (*kernel_forgotten)(const CliJupyterAtCtx *args);
} CliJupyterAtNs;
EMBED_TABLE_LAYOUT(CliJupyterAtNs, kernel_reader_start, kernel_end, kernel_forgotten);
EMBED_TABLE_STORAGE CliJupyterAtNs cli_jupyter_at_internal EMBED_UNUSED = {
    .kernel_reader_start = cli_kernel_reader_start,
    .kernel_end = cli_kernel_end,
    .kernel_forgotten = cli_kernel_forgotten,
};

typedef struct
{
    void (*kernel_key_init)(const CliJupyterKeyCtx *args);
} CliJupyterKeyNs;
EMBED_TABLE_LAYOUT(CliJupyterKeyNs, kernel_key_init);
EMBED_TABLE_STORAGE CliJupyterKeyNs cli_jupyter_key_internal EMBED_UNUSED = {
    .kernel_key_init = cli_kernel_key_init,
};

typedef struct
{
    void (*kernel_id_init)(const CliJupyterSessionCtx *args);
} CliJupyterSessionNs;
EMBED_TABLE_LAYOUT(CliJupyterSessionNs, kernel_id_init);
EMBED_TABLE_STORAGE CliJupyterSessionNs cli_jupyter_session_internal EMBED_UNUSED = {
    .kernel_id_init = cli_kernel_id_init,
};

typedef struct
{
    void (*kernel_now_write)(const CliJupyterOutCtx *args);
    int (*kernel_message_write)(const CliJupyterOutCtx *args);
} CliJupyterOutNsTable;
EMBED_TABLE_LAYOUT(CliJupyterOutNsTable, kernel_now_write, kernel_message_write);
EMBED_TABLE_STORAGE CliJupyterOutNsTable cli_jupyter_out_internal EMBED_UNUSED = {
    .kernel_now_write = cli_kernel_now_write,
    .kernel_message_write = cli_kernel_message_write,
};

typedef struct
{
    int (*kernel_frames_send)(const CliJupyterKernelCtx *args);
} CliJupyterKernelNs;
EMBED_TABLE_LAYOUT(CliJupyterKernelNs, kernel_frames_send);
EMBED_TABLE_STORAGE CliJupyterKernelNs cli_jupyter_kernel_internal EMBED_UNUSED = {
    .kernel_frames_send = cli_kernel_frames_send,
};

typedef struct
{
    void (*kernel_shell_answer)(const CliJupyterKernelParentCtx *args);
} CliJupyterKernelParentNs;
EMBED_TABLE_LAYOUT(CliJupyterKernelParentNs, kernel_shell_answer);
EMBED_TABLE_STORAGE CliJupyterKernelParentNs cli_jupyter_kernel_parent_internal EMBED_UNUSED = {
    .kernel_shell_answer = cli_kernel_shell_answer,
};

#endif
