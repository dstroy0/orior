// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_kernels.c: the kernels notebooks run on, each by the key the page starts it under, a notebook's path: started
// in the notebook's folder, ending the one it ran on before, what each sends going to the page as kernel-message under
// its key; code run, readied to be debugged, interrupted, completed and inspected; and every kernel ended as the
// program ends, as another tree opens, and as a page starts with no notebook open
#include "../command_line/callback_cfg.h"
#include "../editor/files.h"
#include "../notebooks/jupyter.h"
#include "serve_internal.h"

// how long a kernel is given to end as one is stopped, as all are, and as a completion or an inspection is waited
// for, in milliseconds
#define CLI_SERVE_KERNEL_STOPPING 5000ull
#define CLI_SERVE_KERNEL_ENDING 2000ull
#define CLI_SERVE_KERNEL_REQUEST_MS 3000ull

// the bytes a message's line is taken with beyond what it carries, and those a reply's is taken with as it is written
#define CLI_SERVE_KERNEL_MORE 4096ull
#define CLI_SERVE_KERNEL_REPLY (8ull << 20u)

// A kernel running: whether its place is taken, its key, and the kernel
typedef struct
{
    int used;
    char key[CLI_PATH_MAX_LEN];
    CliKernel *kernel;
} CliServeKernel;

// the kernels running, in twice as many places as kernels run at once, so that one being let go never occupies a place
// a kernel starting needs
#define CLI_SERVE_KERNELS (CLI_KERNELS_MOST * 2u)
static CliServeKernel s_kernel[CLI_SERVE_KERNELS];

typedef struct
{
    const char *key;
    CliKernel *kernel;
    unsigned long long wait;
    const CliServeArgs *request;
    VerbaLinea *err_msg;
    const char *msg_type;
    int detail;
    VerbaLinea *out;
} ServeKernelsCtx;
// the kernel under `key` taken out of those running: it, referenced, or NULL where none runs under it
EMBED_INLINE CliKernel *kernels_take_out(const ServeKernelsCtx *args)
{
    CliKernel *kernel = NULL;
    for (unsigned int at = 0u; (kernel == NULL) && (at < CLI_SERVE_KERNELS); at += 1u)
    {
        if (s_kernel[at].used &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_kernel[at].key, .other = args->key, .cap = CLI_CELLS_POOL})))
        {
            kernel = s_kernel[at].kernel;
            s_kernel[at].used = 0;
        }
    }
    return kernel;
}

// a kernel ended within `wait` milliseconds and its reference given back
EMBED_INLINE void kernels_end(const ServeKernelsCtx *args)
{
    CliKernel *kernel = args->kernel;
    unsigned long long wait = args->wait;
    (void)EMBED_CALL(cli_jupyter_err_msg.stop, CliJupyterErrMsgCfg, .kernel = kernel, .wait = wait);
    EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
}

void cli_serve_kernels_stop(void)
{
    static CliKernel *s_taken[CLI_SERVE_KERNELS];
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < CLI_SERVE_KERNELS; at += 1u)
    {
        if (s_kernel[at].used)
        {
            s_taken[count] = s_kernel[at].kernel;
            s_kernel[at].used = 0;
            count += 1u;
        }
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(kernels_end, ServeKernelsCtx, .kernel = s_taken[at], .wait = CLI_SERVE_KERNEL_ENDING);
    }
}

// the kernel under the argument `key`, referenced for the call, or NULL with why given
EMBED_INLINE CliKernel *kernels_get(const ServeKernelsCtx *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if (key == NULL)
    {
        return NULL;
    }
    CliKernel *kernel = NULL;
    for (unsigned int at = 0u; (kernel == NULL) && (at < CLI_SERVE_KERNELS); at += 1u)
    {
        if (s_kernel[at].used &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_kernel[at].key, .other = key, .cap = CLI_CELLS_POOL})))
        {
            kernel = s_kernel[at].kernel;
            EMBED_CALL(cli_jupyter_err_msg_kernel_file.ref, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
        }
    }
    if (kernel == NULL)
    {
        mmgr_line_put(err_msg, "no kernel runs for this notebook: pick one to start it");
    }
    return kernel;
}

// what a kernel sends sent to the page as kernel-message under its key, its context
static void kernels_recv(const CliKernelRecvCfg *args)
{
    unsigned long long size = args->size;
    const char *const key = args->context;
    const unsigned long long body_room = size + CLI_SERVE_KERNEL_MORE;
    char *const body_cell = g_cli_cells->temporary_buf_alloc(body_room, &g_cli_window_cells);
    VerbaLinea body = mmgr_line_over(body_cell, body_room);
    if (body.out == NULL)
    {
        return;
    }
    mmgr_line_put(&body, "{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &body, .text = key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&body, ",\"message\":");
    mmgr_line_put_n(&body, args->message, size);
    mmgr_line_ch(&body, '}');
    if (mmgr_line_ok(&body))
    {
        EMBED_CALL(cli_serve_event_internal.emit, CliServeEventCtx, .event = "kernel-message", .body = body.out,
                   .size = body.at);
    }
    g_cli_cells->persistent_buf_release(body.out, &g_cli_window_cells);
}

static int kernel_specs(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    EMBED_CALL(cli_jupyter_out.specs_write, CliJupyterOutCfg, .out = out);
    return 1;
}

static int kernels_stop_all(const CliServeAnswerCfg *args)
{
    cli_serve_kernels_stop();
    mmgr_line_put(args->out, "null");
    return 1;
}

static int kernel_start(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    static CliKernelSpec s_spec;
    char root[CLI_PATH_MAX_LEN];
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    const char *const name = (key != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                        .request = arguments, .key = "name", .err_msg = err_msg)
                                           : NULL;
    if ((name == NULL) || !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx,
                                      .root = root, .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_jupyter_name.spec_by_name, CliJupyterNameCfg, .name = name, .spec = &s_spec))
    {
        mmgr_line_put(err_msg, "the machine has no kernel ");
        mmgr_line_put(err_msg, name);
        return 0;
    }
    const char *notebook = key;
    while (cellul.starts(&(CatenaFinitaCfg){.src = notebook, .other = "check:", .cap = CLI_CELLS_POOL}))
    {
        notebook += 6;
    }
    char full[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = notebook, .joined = full);
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = full, .out = folder))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = root, .bytes = sizeof(folder)});
    }
    CliKernel *const old = EMBED_CALL(kernels_take_out, ServeKernelsCtx, .key = key);
    if (old != NULL)
    {
        EMBED_CALL(kernels_end, ServeKernelsCtx, .kernel = old, .wait = CLI_SERVE_KERNEL_ENDING);
    }
    // a place is free while a kernel can start, the places being twice the kernels that run at once
    unsigned int at = 0u;
    while (s_kernel[at].used)
    {
        at += 1u;
    }
    VerbaLinea named = {s_kernel[at].key, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&named, key);
    (void)mmgr_line_finish(&named);
    CliKernel *const kernel =
        EMBED_CALL(cli_jupyter_spec.start, CliJupyterSpecCfg, .spec_read = &s_spec, .folder = folder,
                   .recv = kernels_recv, .context = s_kernel[at].key, .err_msg = err_msg);
    s_kernel[at].kernel = kernel;
    s_kernel[at].used = (kernel != NULL);
    if (kernel == NULL)
    {
        return 0;
    }
    const char *const shown = EMBED_CALL(cli_jupyter.display_name, CliJupyterCfg, .kernel_read = kernel);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = shown,
               .length = cellul.len(&(CatenaFinitaCfg){.src = shown, .cap = CLI_CELLS_POOL}));
    return 1;
}

static int kernel_execute(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const code = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "code", .err_msg = err_msg);
    CliKernel *const kernel =
        (code != NULL) ? EMBED_CALL(kernels_get, ServeKernelsCtx, .request = arguments, .err_msg = err_msg) : NULL;
    if (kernel == NULL)
    {
        return 0;
    }
    char id[CLI_KERNEL_ID];
    const int done = EMBED_CALL(
        cli_jupyter_err_msg_kernel_length.execute, CliJupyterErrMsgKernelLengthCfg, .kernel = kernel, .code = code,
        .length = cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL}), .id = id, .err_msg = err_msg);
    EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
    if (done)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = id,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    }
    return done;
}

static int kernel_debug(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char root[CLI_PATH_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    const char *const file = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "file", .err_msg = err_msg);
    const char *const code = (file != NULL) ? EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx,
                                                         .request = arguments, .key = "code", .err_msg = err_msg)
                                            : NULL;
    if ((code == NULL) ||
        !EMBED_CALL(cli_serve_err_msg_request_root_internal.tree, CliServeErrMsgRequestRootCtx, .root = root,
                    .err_msg = err_msg) ||
        !EMBED_CALL(cli_root_internal.inside, CliRootCtx, .root = root, .relative = file, .joined = full,
                    .err_msg = err_msg))
    {
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = full, .out = folder) &&
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    if (!EMBED_CALL(cli_files_root_err_msg_text.write, CliFilesRootErrMsgTextCfg, .root = root, .file = file,
                    .text = code, .length = cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL}),
                    .err_msg = err_msg))
    {
        return 0;
    }
    CliKernel *const kernel = EMBED_CALL(kernels_get, ServeKernelsCtx, .request = arguments, .err_msg = err_msg);
    if (kernel == NULL)
    {
        return 0;
    }
    unsigned int port = 0u;
    const int done = EMBED_CALL(cli_jupyter_err_msg_kernel_file.debug_next, CliJupyterErrMsgKernelFileCfg,
                                .kernel = kernel, .file = full, .port = &port, .err_msg = err_msg);
    EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
    if (done)
    {
        mmgr_line_decimal(out, port, 1u);
    }
    return done;
}

static int kernel_interrupt(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    CliKernel *const kernel = EMBED_CALL(kernels_get, ServeKernelsCtx, .request = arguments, .err_msg = err_msg);
    if (kernel == NULL)
    {
        return 0;
    }
    const int done = EMBED_CALL(cli_jupyter_err_msg_kernel_file.interrupt, CliJupyterErrMsgKernelFileCfg,
                                .kernel = kernel, .err_msg = err_msg);
    EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
    mmgr_line_put(args->out, done ? "null" : "");
    return done;
}

static int kernel_stop(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *err_msg = args->err_msg;
    const char *const key = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                       .key = "key", .err_msg = err_msg);
    if (key == NULL)
    {
        return 0;
    }
    CliKernel *const kernel = EMBED_CALL(kernels_take_out, ServeKernelsCtx, .key = key);
    if (kernel == NULL)
    {
        mmgr_line_put(err_msg, "no kernel runs for this notebook");
        return 0;
    }
    const int ended =
        EMBED_CALL(cli_jupyter_err_msg.stop, CliJupyterErrMsgCfg, .kernel = kernel, .wait = CLI_SERVE_KERNEL_STOPPING);
    EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
    mmgr_line_put(args->out, ended ? "true" : "false");
    return 1;
}

// a request of `msg_type` sent to the kernel with the code and the cursor of the call, and the content of its reply
// written
static int kernels_request(const ServeKernelsCtx *args)
{
    const CliServeArgs *arguments = args->request;
    const char *msg_type = args->msg_type;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const code = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "code", .err_msg = err_msg);
    unsigned long long cursor = 0ull;
    if ((code == NULL) || !EMBED_CALL(cli_serve_err_msg_internal.whole, CliServeErrMsgCtx, .request = arguments,
                                      .key = "cursor", .value = &cursor, .err_msg = err_msg))
    {
        return 0;
    }
    CliKernel *const kernel = EMBED_CALL(kernels_get, ServeKernelsCtx, .request = arguments, .err_msg = err_msg);
    if (kernel == NULL)
    {
        return 0;
    }
    const unsigned long long code_length = cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL});
    // the code's byte is escaped to at most 6
    const unsigned long long content_room = (code_length * 6ull) + CLI_SERVE_KERNEL_MORE;
    char *const content_cell = g_cli_cells->temporary_buf_alloc(content_room, &g_cli_window_cells);
    char *const reply_cell = g_cli_cells->temporary_buf_alloc(CLI_SERVE_KERNEL_REPLY, &g_cli_window_cells);
    VerbaLinea content = mmgr_line_over(content_cell, content_room);
    VerbaLinea reply = mmgr_line_over(reply_cell, CLI_SERVE_KERNEL_REPLY);
    const int begun = (content_cell != NULL) && (reply_cell != NULL);
    mmgr_line_put(&content, "{\"code\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &content, .text = code,
               .length = code_length);
    mmgr_line_put(&content, ",\"cursor_pos\":");
    mmgr_line_decimal(&content, cursor, 1u);
    mmgr_line_put(&content, args->detail ? ",\"detail_level\":0}" : "}");
    if (!begun)
    {
        mmgr_line_put(err_msg, "the kernel's request could not be stored");
    }
    // the waiter writes the reply only while the request waits on it: it is read here and given back after
    const int done = begun && mmgr_line_ok(&content) &&
                     EMBED_CALL(cli_jupyter_err_msg.request, CliJupyterErrMsgCfg, .kernel = kernel, .channel = "shell",
                                .msg_type = msg_type, .content = content.out, .length = content.at,
                                .wait = CLI_SERVE_KERNEL_REQUEST_MS, .reply = &reply, .err_msg = err_msg);
    EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    if (done && EMBED_CALL(cli_lsp_body.member, CliLspBodyCfg, .body = reply.out, .size_value = reply.at,
                           .name = "content", .start = &at, .length_pointer = &length))
    {
        mmgr_line_put_n(out, reply.out + at, length);
    }
    else if (done)
    {
        mmgr_line_put(out, "null");
    }
    g_cli_cells->persistent_buf_release(content.out, &g_cli_window_cells);
    g_cli_cells->persistent_buf_release(reply.out, &g_cli_window_cells);
    return done;
}

static int kernel_complete(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(kernels_request, ServeKernelsCtx, .request = arguments, .msg_type = "complete_request",
                      .detail = 0, .out = out, .err_msg = err_msg);
}

static int kernel_inspect(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    return EMBED_CALL(kernels_request, ServeKernelsCtx, .request = arguments, .msg_type = "inspect_request",
                      .detail = 1, .out = out, .err_msg = err_msg);
}

const CliServeCall g_cli_serve_kernel_calls[] = {
    {"kernel_specs", kernel_specs},     {"kernels_stop_all", kernels_stop_all}, {"kernel_start", kernel_start},
    {"kernel_execute", kernel_execute}, {"kernel_debug", kernel_debug},         {"kernel_interrupt", kernel_interrupt},
    {"kernel_stop", kernel_stop},       {"kernel_complete", kernel_complete},   {"kernel_inspect", kernel_inspect},
};
const unsigned int g_cli_serve_kernel_count = sizeof(g_cli_serve_kernel_calls) / sizeof(g_cli_serve_kernel_calls[0]);
