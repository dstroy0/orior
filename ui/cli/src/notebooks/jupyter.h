// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter.h: Jupyter's kernels, in any language the machine has one for: found where Jupyter keeps their specs and in
// the Pythons orior_cli finds, started with a connection file of orior's, and spoken to by Jupyter's messaging protocol
// over ZMTP, each message signed with the kernel's key.
//
// A kernel's specs are read in Jupyter's order, the first of a name standing: the folders JUPYTER_PATH names, the
// reader's own Jupyter folder, each Python's own, and the system's. A Python orior_cli finds with ipykernel in it and no
// spec of its own is a kernel too; one without ipykernel is listed with the line that installs it.
//
// An interrupt reaches a kernel as its spec asks: by a message on its control socket, or else by SIGINT, or on Windows
// by the event ipykernel waits on, which orior_cli makes and names in JPY_INTERRUPT_EVENT as it starts the kernel. A
// kernel that does not stop when asked is ended (jupyter.c, jupyter_folders.c, jupyter_specs.c, jupyter_readers.c,
// jupyter_requests.c, jupyter_wire.c, jupyter_debug.c)
#ifndef JUPYTER_H
#define JUPYTER_H

#include "../command_line/cli_internal.h"

// the most kernels that run at once, the most words and settings a spec gives, the bytes its texts take, the place
// of a text a spec does not give, and the longest id a message is sent under
#define CLI_KERNELS_MOST 4u
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

// A running kernel and the sockets orior_cli speaks to it on
typedef struct CliKernel CliKernel;

// what a kernel said, as the page reads a message, `{"channel", "msg_type", "msg_id", "parent", "content"}`, handed to
// whoever started it, on a thread of the kernel's own
typedef void (*CliKernelHeard)(void *context, const char *message, unsigned long long size);

// every kernel the machine has, each name once, in Jupyter's order, written as a JSON array of specs; and the first
// of them named `name` into `spec`: 1, or 0 where none is (jupyter_specs.c)
void cli_jupyter_specs_written(CliLine *out);
int cli_jupyter_spec_named(const char *name, CliKernelSpec *spec);

// the kernel `spec` started in `folder`, each message it publishes and each reply to no request handed to `heard`,
// given once it has answered that it is ready and held once for its starter: the kernel, or NULL with why said; the
// kernel held once more, for as long as a caller uses it; and a hold given back, the kernel let go with the last, its
// threads waited for. The name it shows (jupyter.c)
CliKernel *cli_jupyter_started(const CliKernelSpec *spec, const char *folder, CliKernelHeard heard, void *context,
                               CliLine *said);
void cli_jupyter_held(CliKernel *kernel);
void cli_jupyter_given(CliKernel *kernel);
const char *cli_jupyter_display_name(const CliKernel *kernel);

// a request of `msg_type` with `length` bytes of JSON `content` sent on the "shell" or the "control" socket, its id
// into `id`, which holds CLI_KERNEL_ID; the same, its reply waited for up to `wait` milliseconds and written into
// `reply` as the page reads a message; and code run, its id, which its output and its reply carry as their parent,
// into `id`. Each 1, or 0 with why said (jupyter_requests.c)
int cli_jupyter_sent(CliKernel *kernel, const char *channel, const char *msg_type, const char *content,
                     unsigned long long length, char *id, CliLine *said);
int cli_jupyter_requested(CliKernel *kernel, const char *channel, const char *msg_type, const char *content,
                          unsigned long long length, unsigned long long wait, CliLine *reply, CliLine *said);
int cli_jupyter_executed(CliKernel *kernel, const char *code, unsigned long long length, char *id, CliLine *said);

// what the kernel runs interrupted, as its spec asks: 1, or 0 with why said; whether its process still runs; and the
// kernel asked to stop, its process ended where it has not stopped within `wait` milliseconds: whether it had to be
// ended (jupyter_requests.c)
int cli_jupyter_interrupted(CliKernel *kernel, CliLine *said);
int cli_jupyter_alive(CliKernel *kernel);
int cli_jupyter_stopped(CliKernel *kernel, unsigned long long wait);

// the kernel's next run of the code the file at `file` holds readied to be debugged, under that file's name, with
// debugpy listening in the kernel's process from the first time it is asked: the port debugpy listens at into `port`,
// 1, or 0 with why said (jupyter_debug.c)
int cli_jupyter_debug_next(CliKernel *kernel, const char *file, unsigned int *port, CliLine *said);

#endif
