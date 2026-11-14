// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter.h: Jupyter's kernels, in any language the machine has one for: found where Jupyter stores their specs and in
// the Pythons impero finds, started with a connection file of orior's, and communicated with by Jupyter's messaging
// protocol over ZMTP, each message signed with the kernel's key.
//
// A kernel's specs are read in Jupyter's order, the first of a name standing: the folders JUPYTER_PATH names, the
// reader's own Jupyter folder, each Python's own, and the system's. A Python impero finds with ipykernel in it and
// no spec of its own is a kernel too; one without ipykernel is listed with the line that installs it.
//
// An interrupt reaches a kernel as its spec requires: by a message on its control socket, or else by SIGINT, or on
// Windows by the event ipykernel waits on, which impero makes and names in JPY_INTERRUPT_EVENT as it starts the
// kernel. A kernel that does not stop when requested is ended (jupyter.c, jupyter_folders.c, jupyter_specs.c,
// jupyter_readers.c, jupyter_requests.c, jupyter_wire.c, jupyter_debug.c)
#ifndef JUPYTER_H
#define JUPYTER_H
#include "../command_line/callback_cfg.h"
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliJupyterErrMsgCfg CliJupyterErrMsgCfg;
typedef struct CliJupyterErrMsgKernelCfg CliJupyterErrMsgKernelCfg;
typedef struct CliJupyterErrMsgKernelLengthCfg CliJupyterErrMsgKernelLengthCfg;
typedef struct CliJupyterSpecCfg CliJupyterSpecCfg;
typedef struct CliJupyterErrMsgKernelFileCfg CliJupyterErrMsgKernelFileCfg;
typedef struct CliJupyterNameCfg CliJupyterNameCfg;
typedef struct CliJupyterCfg CliJupyterCfg;
typedef struct CliJupyterOutCfg CliJupyterOutCfg;

// the most kernels that run at once, the most words and settings a spec gives, the bytes its texts take, the place
// of a text a spec does not give, and the longest id a message is sent under
#define CLI_KERNELS_MAX 4u
#define CLI_KERNEL_WORDS 64u
#define CLI_KERNEL_SETTINGS 64u
#define CLI_KERNEL_TEXT (1u << 16u)
#define CLI_KERNEL_NONE 0xffffffffu
#define CLI_KERNEL_ID 96u

// A kernel a notebook can run on: the places in `text` of its name, the name shown, its language, its words, how it
// is interrupted, its environment's names and values, the folder its spec is read from, which `{resource_dir}` in its
// words names, and the line that installs what it lacks, CLI_KERNEL_NONE where it lacks nothing
typedef struct
{
    unsigned int name;
    unsigned int display_name;
    unsigned int language;
    unsigned int argv[CLI_KERNEL_WORDS];
    unsigned int argv_count;
    unsigned int interrupt_mode;
    unsigned int env_name[CLI_KERNEL_SETTINGS];
    unsigned int env_value[CLI_KERNEL_SETTINGS];
    unsigned int env_count;
    unsigned int folder;
    unsigned int install;
    char text[CLI_KERNEL_TEXT];
    unsigned int used;
    int full;
} CliKernelSpec;

// A running kernel and the sockets impero communicates with it on
typedef struct CliKernel CliKernel;

// what a kernel sent, as the page reads a message, `{"channel", "msg_type", "msg_id", "parent", "content"}`, handed to
// whoever started it, on a thread of the kernel's own
typedef void (*CliKernelRecvFn)(const CliKernelRecvCfg *args);

// every kernel the machine has, each name once, in Jupyter's order, written as a JSON array of specs; and the first
// of them named `name` into `spec`: 1, or 0 where none is (jupyter_specs.c)
void cli_jupyter_specs_write(const CliJupyterOutCfg *args);
int cli_jupyter_spec_by_name(const CliJupyterNameCfg *args);

// the kernel `spec` started in `folder`, each message it publishes and each reply to no request handed to `recv`,
// given once it has answered that it is ready and referenced once for its starter: the kernel, or NULL with why given;
// the kernel referenced once more, for as long as a caller uses it; and a reference given back, the kernel let go with
// the last, its threads waited for. The name it shows (jupyter.c)
CliKernel *cli_jupyter_start(const CliJupyterSpecCfg *args);
void cli_jupyter_ref(const CliJupyterErrMsgKernelFileCfg *args);
void cli_jupyter_free(const CliJupyterErrMsgKernelFileCfg *args);
const char *cli_jupyter_display_name(const CliJupyterCfg *args);

// a request of `msg_type` with `length` bytes of JSON `content` sent on the "shell" or the "control" socket, its id
// into `id`, sized for CLI_KERNEL_ID; the same, its reply waited for up to `wait` milliseconds and written into
// `reply` as the page reads a message; and code run, its id, which its output and its reply carry as their parent,
// into `id`. Each 1, or 0 with why given (jupyter_requests.c)
int cli_jupyter_send(const CliJupyterErrMsgKernelCfg *args);
int cli_jupyter_request(const CliJupyterErrMsgCfg *args);
int cli_jupyter_execute(const CliJupyterErrMsgKernelLengthCfg *args);

// what the kernel runs interrupted, as its spec specifies: 1, or 0 with why given; whether its process still runs; and
// the kernel requested to stop, its process ended where it has not stopped within `wait` milliseconds: whether it had
// to be ended (jupyter_requests.c)
int cli_jupyter_interrupt(const CliJupyterErrMsgKernelFileCfg *args);
int cli_jupyter_alive(const CliJupyterErrMsgCfg *args);
int cli_jupyter_stop(const CliJupyterErrMsgCfg *args);

// the kernel's next run of the code the file at `file` contains readied to be debugged, under that file's name, with
// debugpy listening in the kernel's process from the first time it is requested: the port debugpy listens at into
// `port`, 1, or 0 with why given (jupyter_debug.c)
int cli_jupyter_debug_next(const CliJupyterErrMsgKernelFileCfg *args);

struct CliJupyterErrMsgCfg
{
    VerbaLinea *err_msg;
    CliKernel *kernel;
    const char *channel;
    const char *msg_type;
    const char *content;
    unsigned long long length;
    unsigned long long wait;
    VerbaLinea *reply;
};

struct CliJupyterErrMsgKernelCfg
{
    VerbaLinea *err_msg;
    CliKernel *kernel;
    const char *channel;
    const char *msg_type;
    const char *content;
    unsigned long long length;
    char *id;
};

struct CliJupyterErrMsgKernelLengthCfg
{
    VerbaLinea *err_msg;
    CliKernel *kernel;
    unsigned long long length;
    char *id;
    const char *code;
};

struct CliJupyterSpecCfg
{
    const CliKernelSpec *spec_read;
    const char *folder;
    CliKernelRecvFn recv;
    void *context;
    VerbaLinea *err_msg;
};

struct CliJupyterErrMsgKernelFileCfg
{
    VerbaLinea *err_msg;
    CliKernel *kernel;
    const char *file;
    unsigned int *port;
};

struct CliJupyterNameCfg
{
    const char *name;
    CliKernelSpec *spec;
};

struct CliJupyterCfg
{
    const CliKernel *kernel_read;
};

struct CliJupyterOutCfg
{
    VerbaLinea *out;
};

typedef struct
{
    void (*specs_write)(const CliJupyterOutCfg *args);
} CliJupyterOutNs;
EMBED_TABLE_LAYOUT(CliJupyterOutNs, specs_write);
EMBED_TABLE_STORAGE CliJupyterOutNs cli_jupyter_out EMBED_UNUSED = {
    .specs_write = cli_jupyter_specs_write,
};

typedef struct
{
    int (*spec_by_name)(const CliJupyterNameCfg *args);
} CliJupyterNameNs;
EMBED_TABLE_LAYOUT(CliJupyterNameNs, spec_by_name);
EMBED_TABLE_STORAGE CliJupyterNameNs cli_jupyter_name EMBED_UNUSED = {
    .spec_by_name = cli_jupyter_spec_by_name,
};

typedef struct
{
    CliKernel *(*start)(const CliJupyterSpecCfg *args);
} CliJupyterSpecNsTable;
EMBED_TABLE_LAYOUT(CliJupyterSpecNsTable, start);
EMBED_TABLE_STORAGE CliJupyterSpecNsTable cli_jupyter_spec EMBED_UNUSED = {
    .start = cli_jupyter_start,
};

typedef struct
{
    void (*ref)(const CliJupyterErrMsgKernelFileCfg *args);
    void (*free)(const CliJupyterErrMsgKernelFileCfg *args);
    int (*interrupt)(const CliJupyterErrMsgKernelFileCfg *args);
    int (*debug_next)(const CliJupyterErrMsgKernelFileCfg *args);
} CliJupyterErrMsgKernelFileNs;
EMBED_TABLE_LAYOUT(CliJupyterErrMsgKernelFileNs, ref, free, interrupt, debug_next);
EMBED_TABLE_STORAGE CliJupyterErrMsgKernelFileNs cli_jupyter_err_msg_kernel_file EMBED_UNUSED = {
    .ref = cli_jupyter_ref,
    .free = cli_jupyter_free,
    .interrupt = cli_jupyter_interrupt,
    .debug_next = cli_jupyter_debug_next,
};

typedef struct
{
    const char *(*display_name)(const CliJupyterCfg *args);
} CliJupyterNs;
EMBED_TABLE_LAYOUT(CliJupyterNs, display_name);
EMBED_TABLE_STORAGE CliJupyterNs cli_jupyter EMBED_UNUSED = {
    .display_name = cli_jupyter_display_name,
};

typedef struct
{
    int (*send)(const CliJupyterErrMsgKernelCfg *args);
} CliJupyterErrMsgKernelNs;
EMBED_TABLE_LAYOUT(CliJupyterErrMsgKernelNs, send);
EMBED_TABLE_STORAGE CliJupyterErrMsgKernelNs cli_jupyter_err_msg_kernel EMBED_UNUSED = {
    .send = cli_jupyter_send,
};

typedef struct
{
    int (*request)(const CliJupyterErrMsgCfg *args);
    int (*alive)(const CliJupyterErrMsgCfg *args);
    int (*stop)(const CliJupyterErrMsgCfg *args);
} CliJupyterErrMsgNs;
EMBED_TABLE_LAYOUT(CliJupyterErrMsgNs, request, alive, stop);
EMBED_TABLE_STORAGE CliJupyterErrMsgNs cli_jupyter_err_msg EMBED_UNUSED = {
    .request = cli_jupyter_request,
    .alive = cli_jupyter_alive,
    .stop = cli_jupyter_stop,
};

typedef struct
{
    int (*execute)(const CliJupyterErrMsgKernelLengthCfg *args);
} CliJupyterErrMsgKernelLengthNs;
EMBED_TABLE_LAYOUT(CliJupyterErrMsgKernelLengthNs, execute);
EMBED_TABLE_STORAGE CliJupyterErrMsgKernelLengthNs cli_jupyter_err_msg_kernel_length EMBED_UNUSED = {
    .execute = cli_jupyter_execute,
};

#endif
