// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter.c: a kernel started with a connection file of orior's, its sockets connected while it starts, its readers
// on threads of their own, and the kernel referenced while it is used and let go with its last reference
#include "../command_line/cells.h"
#include "jupyter_internal.h"

#include "../text/reply.h"
#include "../text/text.h"

CliLock g_cli_kernels_lock = CLI_LOCK_INIT;

// how long a kernel has to start and answer that it is ready, and how long each query that it is ready waits, in
// milliseconds
#define CLI_KERNEL_START 60000ull
#define CLI_KERNEL_READY 2000ull

const char *cli_jupyter_display_name(const CliKernel *kernel)
{
    return cli_kernel_text(&kernel->spec, kernel->spec.display_name);
}

// the kernel's cell given back, where no reader of its reads on
static void jupyter_place_given(CliKernel *kernel)
{
    cli_cell_release(kernel->shell_queue.out, &g_cli_notebook_cells);
    cli_cell_release(kernel, &g_cli_notebook_cells);
}

// a kernel let go with its last reference: its process ended where it runs, its sockets ended, its readers waited for,
// and its place given back once they have ended. A reader that does not end, as one reading a pipe a child of the
// kernel still references, retains the place taken
static void jupyter_let_go(CliKernel *kernel)
{
    cli_kernel_ended(kernel);
    cli_kernel_forgotten(kernel);
    cli_socket_ended(&kernel->shell);
    cli_socket_ended(&kernel->control);
    cli_socket_ended(&kernel->iopub);
    const unsigned long long until = cli_clock_milliseconds() + CLI_KERNEL_READERS_WAIT;
    cli_lock_take(&kernel->lock);
    while ((kernel->reading > 0u) && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&kernel->wake, &kernel->lock, until - cli_clock_milliseconds());
    }
    const int ended = (kernel->reading == 0u);
    cli_lock_give(&kernel->lock);
    if (!ended)
    {
        return;
    }
    cli_socket_closed(&kernel->shell);
    cli_socket_closed(&kernel->control);
    cli_socket_closed(&kernel->iopub);
    cli_spawn_closed(&kernel->spawned);
    jupyter_place_given(kernel);
}

void cli_jupyter_ref(CliKernel *kernel)
{
    cli_lock_take(&g_cli_kernels_lock);
    kernel->refs += 1u;
    cli_lock_give(&g_cli_kernels_lock);
}

void cli_jupyter_given(CliKernel *kernel)
{
    cli_lock_take(&g_cli_kernels_lock);
    kernel->refs -= 1u;
    const int last = (kernel->refs == 0u);
    cli_lock_give(&g_cli_kernels_lock);
    if (last)
    {
        jupyter_let_go(kernel);
    }
}

// a kernel's cell taken, referenced once and made empty: the kernel, or NULL where the cells cannot meet it
static CliKernel *jupyter_place_taken(void)
{
    CliKernel *const kernel = g_cli_cells->persistent_buf_alloc(sizeof(*kernel), &g_cli_notebook_cells);
    if (kernel == NULL)
    {
        return NULL;
    }
    kernel->taken = 1;
    kernel->refs = 1u;
    kernel->ended = 0;
    kernel->code = 0ll;
    memor.set(&(MemoriaCfg){.dst = &kernel->spawned, .val = 0, .bytes = sizeof(kernel->spawned)});
    kernel->shell.handle = 0ull;
    kernel->control.handle = 0ull;
    kernel->iopub.handle = 0ull;
    kernel->shell_lock = (CliLock)CLI_LOCK_INIT;
    kernel->control_lock = (CliLock)CLI_LOCK_INIT;
    kernel->shell_flight[0] = '\0';
    kernel->shell_queue = (VerbaLinea){NULL, 0ull, 0ull, NULL, NULL, 0ull};
    kernel->shell_queue_from = 0ull;
    kernel->lock = (CliLock)CLI_LOCK_INIT;
    kernel->wake = (CliWake)CLI_WAKE_INIT;
    kernel->debug_lock = (CliLock)CLI_LOCK_INIT;
    kernel->content_lock = (CliLock)CLI_LOCK_INIT;
    memor.set(&(MemoriaCfg){.dst = kernel->waiters, .val = 0, .bytes = sizeof(kernel->waiters)});
    kernel->reading = 0u;
    kernel->listening = 0;
    kernel->debug_port = 0u;
    kernel->interrupt_event = 0ull;
    kernel->connection[0] = '\0';
    return kernel;
}

// how a process that ended reports so, as Rust writes an exit status
static void jupyter_status_written(VerbaLinea *err_msg, long long code)
{
#if defined(_WIN32)
    mmgr_line_put(err_msg, "exit code: ");
#else
    if (code < 0ll)
    {
        mmgr_line_put(err_msg, "signal");
        return;
    }
    mmgr_line_put(err_msg, "exit status: ");
#endif
    mmgr_line_decimal_signed(err_msg, code);
}

// a socket of the kernel's connected, tried again while the kernel starts and its process stands: 1, or 0 with why
// reported where the process ended or the time ran out
static int jupyter_connected_while(CliKernel *kernel, CliSocket *socket, unsigned int port, const char *kind,
                                   unsigned long long until, VerbaLinea *err_msg)
{
    for (;;)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea refused = {why, sizeof(why), 0ull};
        if (cli_zmtp_connected(socket, "127.0.0.1", port, kind, &refused))
        {
            return 1;
        }
        cli_lock_take(&kernel->lock);
        long long code = 0ll;
        if (!kernel->ended && cli_spawn_poll(&kernel->spawned, &code))
        {
            kernel->ended = 1;
            kernel->code = code;
        }
        const int ended = kernel->ended;
        code = kernel->code;
        cli_lock_give(&kernel->lock);
        if (ended)
        {
            mmgr_line_put(err_msg, "the kernel ended as it started, ");
            jupyter_status_written(err_msg, code);
            return 0;
        }
        if (cli_clock_milliseconds() > until)
        {
            mmgr_line_put(err_msg, "the kernel's ");
            mmgr_line_put(err_msg, kind);
            mmgr_line_put(err_msg, " socket did not open: ");
            mmgr_line_put_n(err_msg, why, refused.at);
            return 0;
        }
        query_paused(100u);
    }
}

// `text` with every `name` in it written as `value` into `out`, sized for `room`
static void jupyter_replaced(const char *text, const char *name, const char *value, char *out, unsigned long long room)
{
    VerbaLinea line = {out, room, 0ull};
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    for (const char *at = text; *at != '\0';)
    {
        if (cli_text_same(at, name, size))
        {
            mmgr_line_put(&line, value);
            at += size;
            continue;
        }
        mmgr_line_ch(&line, *at);
        at += 1;
    }
    mmgr_line_finish(&line);
}

// the connection file written and the kernel's process started with its words, its settings and the PATH every step
// runs with, its output and its errors read as the log channel: 1, or 0 with why given
static int jupyter_process_started(CliKernel *kernel, const char *folder, const unsigned int *ports,
                                   VerbaLinea *err_msg)
{
    const CliKernelSpec *const spec = &kernel->spec;
    char temporary[CLI_PATH_MAX_LEN];
    char name[96];
    VerbaLinea name_line = {name, sizeof(name), 0ull};
    mmgr_line_put(&name_line, "orior-kernel-");
    mmgr_line_decimal(&name_line, cli_process_own(), 1u);
    mmgr_line_ch(&name_line, '-');
    mmgr_line_decimal(&name_line, cli_kernel_next(), 1u);
    mmgr_line_put(&name_line, ".json");
    mmgr_line_finish(&name_line);
    cli_temp_folder(temporary);
    cli_full(temporary, name, kernel->connection);
    char info[1024];
    VerbaLinea line = {info, sizeof(info), 0ull};
    static const char *const keys[5] = {
        "{\"shell_port\":", ",\"iopub_port\":", ",\"stdin_port\":", ",\"control_port\":", ",\"hb_port\":"};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        mmgr_line_put(&line, keys[at]);
        mmgr_line_decimal(&line, ports[at], 1u);
    }
    mmgr_line_put(&line, ",\"ip\":\"127.0.0.1\",\"key\":\"");
    mmgr_line_put(&line, kernel->key);
    mmgr_line_put(&line, "\",\"transport\":\"tcp\",\"signature_scheme\":\"hmac-sha256\",\"kernel_name\":");
    const char *const kernel_name = cli_kernel_text(spec, spec->name);
    cli_json_quoted(&line, kernel_name, cellul.len(&(CatenaFinitaCfg){.src = kernel_name, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&line, '}');
    if (!mmgr_line_ok(&line) || !query_file_written(kernel->connection, info, line.at))
    {
        mmgr_line_put(err_msg, kernel->connection);
        mmgr_line_put(err_msg, ": it could not be written");
        kernel->connection[0] = '\0';
        return 0;
    }
    kernel->words.count = 0u;
    kernel->words.used = 0u;
    kernel->words.full = 0;
    for (unsigned int at = 0u; at < spec->argv_count; at += 1u)
    {
        static char s_named[CLI_PATH_TEXT];
        static char s_word[CLI_PATH_TEXT];
        static CliLock s_word_lock = CLI_LOCK_INIT;
        cli_lock_take(&s_word_lock);
        jupyter_replaced(cli_kernel_text(spec, spec->argv[at]), "{connection_file}", kernel->connection, s_named,
                         sizeof(s_named));
        jupyter_replaced(s_named, "{resource_dir}", cli_kernel_text(spec, spec->folder), s_word, sizeof(s_word));
        cli_words_put(&kernel->words, s_word);
        cli_lock_give(&s_word_lock);
    }
    if (kernel->words.count == 0u)
    {
        mmgr_line_put(err_msg, "the kernel's spec names no program");
        return 0;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < spec->env_count; at += 1u)
    {
        kernel->settings[count] =
            (CliSetting){cli_kernel_text(spec, spec->env_name[at]), cli_kernel_text(spec, spec->env_value[at])};
        count += 1u;
    }
    cli_toolchains_run_path(kernel->path, CLI_PATH_TEXT);
    kernel->settings[count] = (CliSetting){"PATH", kernel->path};
    count += 1u;
#if defined(_WIN32)
    kernel->interrupt_event = cli_event_inheritable();
    VerbaLinea number = {kernel->event_number, sizeof(kernel->event_number), 0ull};
    mmgr_line_decimal(&number, kernel->interrupt_event, 1u);
    mmgr_line_finish(&number);
    kernel->settings[count] = (CliSetting){"JPY_INTERRUPT_EVENT", kernel->event_number};
    count += 1u;
#endif
    if (!cli_spawn(&kernel->spawned, kernel->words.word, folder, kernel->settings, count, 1, err_msg))
    {
        return 0;
    }
    kernel->readers[3] = (CliKernelReader){
        .channel = "log", .pipe = kernel->spawned.out, .room = kernel->out_room, .room_size = sizeof(kernel->out_room)};
    kernel->readers[4] = (CliKernelReader){
        .channel = "log", .pipe = kernel->spawned.err, .room = kernel->err_room, .room_size = sizeof(kernel->err_room)};
    cli_kernel_reader_started(kernel, 3u);
    cli_kernel_reader_started(kernel, 4u);
    return 1;
}

// the ports the kernel bound, read back from its connection file once the kernel has written them there, each
// looked for every CLI_KERNEL_PORTS_EVERY milliseconds until `until`: 1 with the five into `ports`, or 0 with why
// given
#define CLI_KERNEL_PORTS_EVERY 20u
static int jupyter_ports_read(CliKernel *kernel, unsigned int *ports, unsigned long long until, VerbaLinea *err_msg)
{
    static const char *const s_keys[5] = {"shell_port", "iopub_port", "stdin_port", "control_port", "hb_port"};
    while (cli_clock_milliseconds() < until)
    {
        CliReply file = {NULL, 0ull};
        unsigned int read = 0u;
        if (cli_reply_read(kernel->connection, &file))
        {
            const unsigned long long root = cli_reply_root(&file);
            for (unsigned int at = 0u; at < 5u; at += 1u)
            {
                const unsigned long long port = cli_reply_whole(&file, cli_reply_member(&file, root, s_keys[at]));
                // a port is below 65536, which an unsigned int stores
                ports[at] = ((port > 0ull) && (port < 65536ull)) ? (unsigned int)port : 0u;
                read += (ports[at] > 0u) ? 1u : 0u;
            }
            cli_reply_let_go(&file);
        }
        if (read == 5u)
        {
            return 1;
        }
        // a process id is below 2^32, which an unsigned int stores
        if (!cli_process_running((unsigned int)kernel->spawned.id))
        {
            mmgr_line_put(err_msg, "the kernel ended before it wrote the ports it bound");
            return 0;
        }
        query_paused(CLI_KERNEL_PORTS_EVERY);
    }
    mmgr_line_put(err_msg, "the kernel wrote no ports it bound to its connection file in time");
    return 0;
}

// the kernel's sockets connected, its session made and its sockets' readers started: 1, or 0 with why given
static int jupyter_sockets_connected(CliKernel *kernel, const unsigned int *ports, unsigned long long until,
                                     VerbaLinea *err_msg)
{
    if (!jupyter_connected_while(kernel, &kernel->shell, ports[0], "DEALER", until, err_msg) ||
        !jupyter_connected_while(kernel, &kernel->control, ports[3], "DEALER", until, err_msg) ||
        !jupyter_connected_while(kernel, &kernel->iopub, ports[1], "SUB", until, err_msg))
    {
        return 0;
    }
    char session[72];
    cli_kernel_key_made(session);
    memor.cpy(&(MemoriaCfg){.dst = kernel->session, .src = session, .bytes = 16u});
    kernel->session[16] = '\0';
    kernel->readers[0] = (CliKernelReader){.channel = "iopub",
                                           .socket = &kernel->iopub,
                                           .room = kernel->iopub_room,
                                           .room_size = sizeof(kernel->iopub_room)};
    kernel->readers[1] = (CliKernelReader){.channel = "shell",
                                           .socket = &kernel->shell,
                                           .room = kernel->shell_room,
                                           .room_size = sizeof(kernel->shell_room),
                                           .replies = 1};
    kernel->readers[2] = (CliKernelReader){.channel = "control",
                                           .socket = &kernel->control,
                                           .room = kernel->control_room,
                                           .room_size = sizeof(kernel->control_room),
                                           .replies = 1};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_kernel_reader_started(kernel, at);
    }
    return 1;
}

CliKernel *cli_jupyter_started(const CliKernelSpec *spec, const char *folder, CliKernelRecvFn recv, void *context,
                               VerbaLinea *err_msg)
{
    if (spec->install != CLI_KERNEL_NONE)
    {
        mmgr_line_put(err_msg, cli_kernel_text(spec, spec->display_name));
        mmgr_line_put(err_msg, " has no ipykernel: ");
        mmgr_line_put(err_msg, cli_kernel_text(spec, spec->install));
        mmgr_line_put(err_msg, " installs it");
        return NULL;
    }
    // the kernel binds ports of its own choosing, each written 0 in its connection file, and writes the file again
    // with the ports it bound, which are read back from it: no port is probed and let go for another process to take
    unsigned int ports[5] = {0u, 0u, 0u, 0u, 0u};
    CliKernel *const kernel = jupyter_place_taken();
    if (kernel == NULL)
    {
        mmgr_line_put(err_msg, "no more kernels are run at once");
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = &kernel->spec, .src = spec, .bytes = sizeof(kernel->spec)});
    kernel->recv = recv;
    kernel->context = context;
    cli_kernel_key_made(kernel->key);
    if (!jupyter_process_started(kernel, folder, ports, err_msg))
    {
        cli_kernel_forgotten(kernel);
        jupyter_place_given(kernel);
        return NULL;
    }
    const unsigned long long until = cli_clock_milliseconds() + CLI_KERNEL_START;
    // The kernel is ready once it reports what it is; the subscription to what it publishes has had the time to reach
    // it by then
    int ready =
        jupyter_ports_read(kernel, ports, until, err_msg) && jupyter_sockets_connected(kernel, ports, until, err_msg);
    int requested = 0;
    while (ready && !requested && (cli_clock_milliseconds() < until))
    {
        char answer[65536];
        VerbaLinea reply = {answer, sizeof(answer), 0ull};
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea unanswered = {why, sizeof(why), 0ull};
        requested = cli_jupyter_requested(kernel, "shell", "kernel_info_request", "{}", 2ull, CLI_KERNEL_READY, &reply,
                                          &unanswered);
    }
    if (ready && !requested)
    {
        mmgr_line_put(err_msg, "the kernel did not answer that it is ready");
        ready = 0;
    }
    if (!ready)
    {
        // a kernel that did not start is ended, its place given back once its readers end
        cli_jupyter_given(kernel);
        return NULL;
    }
    return kernel;
}
