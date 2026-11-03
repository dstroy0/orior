// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter.c: a kernel started with a connection file of orior's, its sockets connected while it starts, its readers
// on threads of their own, and the kernel referenced while it is used and let go with its last reference
#include "../command_line/cells.h"
#include "jupyter_internal.h"

#include "../text/reply.h"
#include "../text/text.h"

// how long a kernel has to start and answer that it is ready, and how long each query that it is ready waits, in
// milliseconds
#define CLI_KERNEL_START 60000ULL
#define CLI_KERNEL_READY 2000ULL

const char *cli_jupyter_display_name(const CliJupyterCfg *args)
{
    return EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = &args->kernel_read->spec,
                      .at = args->kernel_read->spec.display_name);
}

typedef struct
{
    CliKernel *kernel;
    VerbaLinea *err_msg;
    long long code;
    CliSocket *socket;
    unsigned int port;
    const char *kind;
    unsigned long long until;
    const char *text;
    const char *name;
    const char *value;
    char *out;
    unsigned long long capacity;
    const char *folder;
    const unsigned int *ports;
    unsigned int *ports_out;
} JupyterCtx;
// the kernel's cell given back, where no reader of its reads on
EMBED_INLINE void jupyter_position_free(const JupyterCtx *args)
{
    g_cli_cells->persistent_buf_release(args->kernel->shell_queue.out, &g_cli_notebook_cells);
    g_cli_cells->persistent_buf_release(args->kernel, &g_cli_notebook_cells);
}

// a kernel let go with its last reference: its process ended where it runs, its sockets ended, its readers waited for,
// and its place given back once they have ended. A reader that does not end, as one reading a pipe a child of the
// kernel still references, retains the place taken
EMBED_INLINE void jupyter_let_go(const JupyterCtx *args)
{
    CliKernel *kernel = args->kernel;
    EMBED_CALL(cli_jupyter_at_internal.kernel_end, CliJupyterAtCtx, .kernel = kernel);
    EMBED_CALL(cli_jupyter_at_internal.kernel_forgotten, CliJupyterAtCtx, .kernel = kernel);
    EMBED_CALL(cli_sock_internal.socket_end, CliSockCtx, .sock = &kernel->shell);
    EMBED_CALL(cli_sock_internal.socket_end, CliSockCtx, .sock = &kernel->control);
    EMBED_CALL(cli_sock_internal.socket_end, CliSockCtx, .sock = &kernel->iopub);
    const unsigned long long until = cli_clock_milliseconds() + CLI_KERNEL_READERS_WAIT;
    while ((kernel->read_state > 0u) && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &kernel->wake,
                         .milliseconds_u64 = until - cli_clock_milliseconds());
    }
    const int ended = (kernel->read_state == 0u);
    if (!ended)
    {
        return;
    }
    EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &kernel->shell);
    EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &kernel->control);
    EMBED_CALL(cli_err_msg_internal.socket_close, CliErrMsgCtx, .sock_out = &kernel->iopub);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &kernel->spawned);
    EMBED_CALL(jupyter_position_free, JupyterCtx, .kernel = kernel);
}

void cli_jupyter_ref(const CliJupyterErrMsgKernelFileCfg *args)
{
    args->kernel->refs += 1u;
}

void cli_jupyter_free(const CliJupyterErrMsgKernelFileCfg *args)
{
    args->kernel->refs -= 1u;
    const int last = (args->kernel->refs == 0u);
    if (last)
    {
        EMBED_CALL(jupyter_let_go, JupyterCtx, .kernel = args->kernel);
    }
}

// a kernel's cell taken, referenced once and made empty: the kernel, or NULL where the cells cannot meet it
static CliKernel *jupyter_position_alloc(void)
{
    // a kernel's buffers stand in its cell, after it
    CliKernel *const kernel = g_cli_cells->persistent_buf_alloc(
        sizeof(*kernel) + (unsigned long long)CLI_KERNEL_CONTENT + (3ULL * CLI_KERNEL_REPLY_CAPACITY) +
            CLI_KERNEL_IOPUB_CAPACITY + (2ULL * CLI_KERNEL_LINE * 8ULL),
        &g_cli_notebook_cells);
    if (kernel == NULL)
    {
        return NULL;
    }
    kernel->content = (char *)(void *)(kernel + 1);
    kernel->debug_reply = kernel->content + CLI_KERNEL_CONTENT;
    kernel->iopub_capacity = (unsigned char *)(kernel->debug_reply + CLI_KERNEL_REPLY_CAPACITY);
    kernel->shell_capacity = kernel->iopub_capacity + CLI_KERNEL_IOPUB_CAPACITY;
    kernel->control_capacity = kernel->shell_capacity + CLI_KERNEL_REPLY_CAPACITY;
    kernel->out_capacity = kernel->control_capacity + CLI_KERNEL_REPLY_CAPACITY;
    kernel->err_capacity = kernel->out_capacity + (CLI_KERNEL_LINE * 8u);
    kernel->reserved = 1;
    kernel->refs = 1u;
    kernel->ended = 0;
    kernel->code = 0LL;
    memor.set(&(MemoriaCfg){.dst = &kernel->spawned, .val = 0, .bytes = sizeof(kernel->spawned)});
    kernel->shell.handle = 0ULL;
    kernel->control.handle = 0ULL;
    kernel->iopub.handle = 0ULL;
    kernel->shell_flight[0] = '\0';
    kernel->shell_queue = (VerbaLinea){NULL, 0ULL, 0ULL, NULL, NULL, 0ULL};
    kernel->shell_queue_from = 0ULL;
    kernel->wake = (CliWake)CLI_WAKE_INIT;
    memor.set(&(MemoriaCfg){.dst = kernel->waiters, .val = 0, .bytes = sizeof(kernel->waiters)});
    kernel->read_state = 0u;
    kernel->listening = 0;
    kernel->debug_port = 0u;
    kernel->interrupt_event = 0ULL;
    kernel->connection[0] = '\0';
    return kernel;
}

// how a process that ended reports so, as Rust writes an exit status
EMBED_INLINE void jupyter_status_write(const JupyterCtx *args)
{
#if defined(_WIN32)
    mmgr_line_put(args->err_msg, "exit code: ");
#else
    if (args->code < 0LL)
    {
        mmgr_line_put(args->err_msg, "signal");
        return;
    }
    mmgr_line_put(args->err_msg, "exit status: ");
#endif
    mmgr_line_decimal_signed(args->err_msg, args->code);
}

// a socket of the kernel's connected, tried again while the kernel starts and its process stands: 1, or 0 with why
// reported where the process ended or the time ran out
static int jupyter_connected_while(const JupyterCtx *args)
{
    CliSocket *socket = args->socket;
    unsigned int port = args->port;
    const char *kind = args->kind;
    VerbaLinea *err_msg = args->err_msg;
    for (;;)
    {
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea refused = {why, sizeof(why), 0ULL};
        if (EMBED_CALL(cli_zmtp_socket_host.connect, CliZmtpSocketHostCfg, .socket_out = socket, .host = "127.0.0.1",
                       .port = port, .kind = kind, .err_msg = &refused))
        {
            return 1;
        }
        long long code = 0LL;
        if (!args->kernel->ended && EMBED_CALL(cli_spawned_internal.spawn_poll, CliSpawnedCtx,
                                               .spawned = &args->kernel->spawned, .code = &code))
        {
            args->kernel->ended = 1;
            args->kernel->code = code;
        }
        const int ended = args->kernel->ended;
        code = args->kernel->code;
        if (ended)
        {
            mmgr_line_put(err_msg, "the kernel ended as it started, ");
            EMBED_CALL(jupyter_status_write, JupyterCtx, .err_msg = err_msg, .code = code);
            return 0;
        }
        if (cli_clock_milliseconds() > args->until)
        {
            mmgr_line_put(err_msg, "the kernel's ");
            mmgr_line_put(err_msg, kind);
            mmgr_line_put(err_msg, " socket did not open: ");
            mmgr_line_put_n(err_msg, why, refused.at);
            return 0;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 100u);
    }
}

// `text` with every `name` in it written as `value` into `out`, sized for `capacity`
EMBED_INLINE void jupyter_replace(const JupyterCtx *args)
{
    const char *text = args->text;
    const char *name = args->name;
    const char *value = args->value;
    char *out = args->out;
    VerbaLinea line = {out, args->capacity, 0ULL};
    EMBED_CALL(cli_text_out_name.append_fill, CliTextOutNameCfg, .out = &line, .text = text, .name = name,
               .value = value);
    mmgr_line_finish(&line);
}

// the connection file written and the kernel's process started with its words, its settings and the PATH every step
// runs with, its output and its errors read as the log channel: 1, or 0 with why given
EMBED_INLINE int jupyter_process_start(const JupyterCtx *args)
{
    CliKernel *kernel = args->kernel;
    const char *folder = args->folder;
    VerbaLinea *err_msg = args->err_msg;
    const CliKernelSpec *const spec = &kernel->spec;
    char temporary[CLI_PATH_MAX_LEN];
    char name[96];
    VerbaLinea name_line = {name, sizeof(name), 0ULL};
    mmgr_line_put(&name_line, "orior-kernel-");
    mmgr_line_decimal(&name_line, cli_process_own(), 1u);
    mmgr_line_ch(&name_line, '-');
    mmgr_line_decimal(&name_line, cli_kernel_next(), 1u);
    mmgr_line_put(&name_line, ".json");
    mmgr_line_finish(&name_line);
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = temporary);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = temporary, .relative = name, .joined = kernel->connection);
    char info[1024];
    VerbaLinea line = {info, sizeof(info), 0ULL};
    static const char *const keys[5] = {
        "{\"shell_port\":", ",\"iopub_port\":", ",\"stdin_port\":", ",\"control_port\":", ",\"hb_port\":"};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        mmgr_line_put(&line, keys[at]);
        mmgr_line_decimal(&line, args->ports[at], 1u);
    }
    mmgr_line_put(&line, ",\"ip\":\"127.0.0.1\",\"key\":\"");
    mmgr_line_put(&line, kernel->key);
    mmgr_line_put(&line, "\",\"transport\":\"tcp\",\"signature_scheme\":\"hmac-sha256\",\"kernel_name\":");
    const char *const kernel_name =
        EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec, .at = spec->name);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &line, .text = kernel_name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = kernel_name, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&line, '}');
    if (!mmgr_line_ok(&line) || !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                            .path = kernel->connection, .bytes = info, .size = line.at))
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
        EMBED_CALL(jupyter_replace, JupyterCtx,
                   .text = EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec,
                                      .at = spec->argv[at]),
                   .name = "{connection_file}", .value = kernel->connection, .out = s_named,
                   .capacity = sizeof(s_named));
        EMBED_CALL(jupyter_replace, JupyterCtx, .text = s_named, .name = "{resource_dir}",
                   .value = EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx, .spec = spec,
                                       .at = spec->folder),
                   .out = s_word, .capacity = sizeof(s_word));
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &kernel->words,
                   .word_text = s_word);
    }
    if (kernel->words.count == 0u)
    {
        mmgr_line_put(err_msg, "the kernel's spec names no program");
        return 0;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < spec->env_count; at += 1u)
    {
        kernel->settings[count] = (CliSetting){EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx,
                                                          .spec = spec, .at = spec->env_name[at]),
                                               EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx,
                                                          .spec = spec, .at = spec->env_value[at])};
        count += 1u;
    }
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = kernel->path,
               .size = CLI_PATH_TEXT);
    kernel->settings[count] = (CliSetting){"PATH", kernel->path};
    count += 1u;
#if defined(_WIN32)
    kernel->interrupt_event = cli_event_inheritable();
    VerbaLinea number = {kernel->event_number, sizeof(kernel->event_number), 0ULL};
    mmgr_line_decimal(&number, kernel->interrupt_event, 1u);
    mmgr_line_finish(&number);
    kernel->settings[count] = (CliSetting){"JPY_INTERRUPT_EVENT", kernel->event_number};
    count += 1u;
#endif
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &kernel->spawned,
                    .words = kernel->words.word_text, .folder = folder, .settings = kernel->settings, .count = count,
                    .detached = 1, .err_msg = err_msg))
    {
        return 0;
    }
    kernel->readers[3] = (CliKernelReader){.channel = "log",
                                           .pipe = kernel->spawned.out,
                                           .capacity = kernel->out_capacity,
                                           .capacity_bytes = (CLI_KERNEL_LINE * 8u)};
    kernel->readers[4] = (CliKernelReader){.channel = "log",
                                           .pipe = kernel->spawned.err,
                                           .capacity = kernel->err_capacity,
                                           .capacity_bytes = (CLI_KERNEL_LINE * 8u)};
    EMBED_CALL(cli_jupyter_at_internal.kernel_reader_start, CliJupyterAtCtx, .kernel = kernel, .at = 3u);
    EMBED_CALL(cli_jupyter_at_internal.kernel_reader_start, CliJupyterAtCtx, .kernel = kernel, .at = 4u);
    return 1;
}

// the ports the kernel bound, read back from its connection file once the kernel has written them there, each
// looked for every CLI_KERNEL_PORTS_EVERY milliseconds until `until`: 1 with the five into `ports`, or 0 with why
// given
#define CLI_KERNEL_PORTS_EVERY 20u
EMBED_INLINE int jupyter_ports_read(const JupyterCtx *args)
{
    static const char *const s_keys[5] = {"shell_port", "iopub_port", "stdin_port", "control_port", "hb_port"};
    while (cli_clock_milliseconds() < args->until)
    {
        CliReply file = {NULL, 0ULL};
        unsigned int read = 0u;
        if (EMBED_CALL(cli_reply_path.read, CliReplyPathCfg, .path = args->kernel->connection, .reply_out = &file))
        {
            const unsigned long long root = EMBED_CALL(cli_reply.root, CliReplyCfg, .reply = &file);
            for (unsigned int at = 0u; at < 5u; at += 1u)
            {
                const unsigned long long port =
                    EMBED_CALL(cli_reply_at.whole, CliReplyAtCfg, .reply = &file,
                               .at = EMBED_CALL(cli_reply_object.member, CliReplyObjectCfg, .reply = &file,
                                                .object = root, .key = s_keys[at]));
                // a port is below 65536, which an unsigned int stores
                args->ports_out[at] = ((port > 0ULL) && (port < 65536ULL)) ? (unsigned int)port : 0u;
                read += (args->ports_out[at] > 0u) ? 1u : 0u;
            }
            EMBED_CALL(cli_reply_path.let_go, CliReplyPathCfg, .reply_out = &file);
        }
        if (read == 5u)
        {
            return 1;
        }
        // a process id is below 2^32, which an unsigned int stores
        if (!EMBED_CALL(cli_pid_internal.process_running, CliPidCtx, .pid = (unsigned int)args->kernel->spawned.id))
        {
            mmgr_line_put(args->err_msg, "the kernel ended before it wrote the ports it bound");
            return 0;
        }
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = CLI_KERNEL_PORTS_EVERY);
    }
    mmgr_line_put(args->err_msg, "the kernel wrote no ports it bound to its connection file in time");
    return 0;
}

// the kernel's sockets connected, its session made and its sockets' readers started: 1, or 0 with why given
EMBED_INLINE int jupyter_sockets_connect(const JupyterCtx *args)
{
    CliKernel *kernel = args->kernel;
    if (!EMBED_CALL(jupyter_connected_while, JupyterCtx, .kernel = kernel, .socket = &kernel->shell,
                    .port = args->ports[0], .kind = "DEALER", .until = args->until, .err_msg = args->err_msg) ||
        !EMBED_CALL(jupyter_connected_while, JupyterCtx, .kernel = kernel, .socket = &kernel->control,
                    .port = args->ports[3], .kind = "DEALER", .until = args->until, .err_msg = args->err_msg) ||
        !EMBED_CALL(jupyter_connected_while, JupyterCtx, .kernel = kernel, .socket = &kernel->iopub,
                    .port = args->ports[1], .kind = "SUB", .until = args->until, .err_msg = args->err_msg))
    {
        return 0;
    }
    char session[72];
    EMBED_CALL(cli_jupyter_key_internal.kernel_key_init, CliJupyterKeyCtx, .key = session);
    memor.cpy(&(MemoriaCfg){.dst = kernel->session, .src = session, .bytes = 16u});
    kernel->session[16] = '\0';
    kernel->readers[0] = (CliKernelReader){.channel = "iopub",
                                           .socket = &kernel->iopub,
                                           .capacity = kernel->iopub_capacity,
                                           .capacity_bytes = CLI_KERNEL_IOPUB_CAPACITY};
    kernel->readers[1] = (CliKernelReader){.channel = "shell",
                                           .socket = &kernel->shell,
                                           .capacity = kernel->shell_capacity,
                                           .capacity_bytes = CLI_KERNEL_REPLY_CAPACITY,
                                           .replies = 1};
    kernel->readers[2] = (CliKernelReader){.channel = "control",
                                           .socket = &kernel->control,
                                           .capacity = kernel->control_capacity,
                                           .capacity_bytes = CLI_KERNEL_REPLY_CAPACITY,
                                           .replies = 1};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_jupyter_at_internal.kernel_reader_start, CliJupyterAtCtx, .kernel = kernel, .at = at);
    }
    return 1;
}

CliKernel *cli_jupyter_start(const CliJupyterSpecCfg *args)
{
    CliKernelRecvFn recv = args->recv;
    void *context = args->context;
    if (args->spec_read->install != CLI_KERNEL_NONE)
    {
        mmgr_line_put(args->err_msg, EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx,
                                                .spec = args->spec_read, .at = args->spec_read->display_name));
        mmgr_line_put(args->err_msg, " has no ipykernel: ");
        mmgr_line_put(args->err_msg, EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx,
                                                .spec = args->spec_read, .at = args->spec_read->install));
        mmgr_line_put(args->err_msg, " installs it");
        return NULL;
    }
    // the kernel binds ports of its own choosing, each written 0 in its connection file, and writes the file again
    // with the ports it bound, which are read back from it: no port is probed and let go for another process to take
    unsigned int ports[5] = {0u, 0u, 0u, 0u, 0u};
    CliKernel *const kernel = jupyter_position_alloc();
    if (kernel == NULL)
    {
        mmgr_line_put(args->err_msg, "no more kernels are run at once");
        return NULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = &kernel->spec, .src = args->spec_read, .bytes = sizeof(kernel->spec)});
    kernel->recv = recv;
    kernel->context = context;
    EMBED_CALL(cli_jupyter_key_internal.kernel_key_init, CliJupyterKeyCtx, .key = kernel->key);
    if (!EMBED_CALL(jupyter_process_start, JupyterCtx, .kernel = kernel, .folder = args->folder, .ports = ports,
                    .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_jupyter_at_internal.kernel_forgotten, CliJupyterAtCtx, .kernel = kernel);
        EMBED_CALL(jupyter_position_free, JupyterCtx, .kernel = kernel);
        return NULL;
    }
    const unsigned long long until = cli_clock_milliseconds() + CLI_KERNEL_START;
    // The kernel is ready once it reports what it is; the subscription to what it publishes has had the time to reach
    // it by then
    int ready = EMBED_CALL(jupyter_ports_read, JupyterCtx, .kernel = kernel, .ports_out = ports, .until = until,
                           .err_msg = args->err_msg) &&
                EMBED_CALL(jupyter_sockets_connect, JupyterCtx, .kernel = kernel, .ports = ports, .until = until,
                           .err_msg = args->err_msg);
    int requested = 0;
    while (ready && !requested && (cli_clock_milliseconds() < until))
    {
        char answer[65536];
        VerbaLinea reply = {answer, sizeof(answer), 0ULL};
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea unanswered = {why, sizeof(why), 0ULL};
        requested = EMBED_CALL(cli_jupyter_err_msg.request, CliJupyterErrMsgCfg, .kernel = kernel, .channel = "shell",
                               .msg_type = "kernel_info_request", .content = "{}", .length = 2ULL,
                               .wait = CLI_KERNEL_READY, .reply = &reply, .err_msg = &unanswered);
    }
    if (ready && !requested)
    {
        mmgr_line_put(args->err_msg, "the kernel did not answer that it is ready");
        ready = 0;
    }
    if (!ready)
    {
        // a kernel that did not start is ended, its place given back once its readers end
        EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
        return NULL;
    }
    return kernel;
}
