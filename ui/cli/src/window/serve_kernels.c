// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_kernels.c: the kernels notebooks run on, each by the key the page starts it under, a notebook's path: started
// in the notebook's folder, ending the one it ran on before, what each says going to the page as kernel-message under
// its key; code run, readied to be debugged, interrupted, completed and inspected; and every kernel ended as the
// program ends, as another tree opens, and as a page starts with no notebook open
#include "../editor/files.h"
#include "../notebooks/jupyter.h"
#include "serve_internal.h"

// how long a kernel is given to end as one is stopped, as all are, and as a completion or an inspection is waited
// for, in milliseconds
#define CLI_SERVE_KERNEL_STOPPING 5000ull
#define CLI_SERVE_KERNEL_ENDING 2000ull
#define CLI_SERVE_KERNEL_ASKING 3000ull

// the bytes a message's line is begun with beyond what it carries, and those a reply's is begun with, each growing
// past them as it is written
#define CLI_SERVE_KERNEL_MORE 4096ull
#define CLI_SERVE_KERNEL_REPLY 16384ull

// A kernel running: whether its place is taken, its key, and the kernel
typedef struct
{
    int used;
    char key[CLI_PATH_LONGEST];
    CliKernel *kernel;
} CliServeKernel;

// the kernels running, in twice as many places as kernels run at once, so that one being let go never holds a place a
// kernel starting needs
#define CLI_SERVE_KERNELS (CLI_KERNELS_MOST * 2u)
static CliServeKernel s_kernel[CLI_SERVE_KERNELS];
static CliLock s_lock = CLI_LOCK_INIT;

// the kernel under `key` taken out of those running: it, held, or NULL where none runs under it
static CliKernel *kernels_taken_out(const char *key)
{
    cli_lock_take(&s_lock);
    CliKernel *kernel = NULL;
    for (unsigned int at = 0u; (kernel == NULL) && (at < CLI_SERVE_KERNELS); at += 1u)
    {
        if (s_kernel[at].used &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_kernel[at].key, .other = key, .cap = CLI_CELLS_POOL})))
        {
            kernel = s_kernel[at].kernel;
            s_kernel[at].used = 0;
        }
    }
    cli_lock_give(&s_lock);
    return kernel;
}

// a kernel ended within `wait` milliseconds and its hold given back
static void kernels_ended(CliKernel *kernel, unsigned long long wait)
{
    (void)cli_jupyter_stopped(kernel, wait);
    cli_jupyter_given(kernel);
}

void cli_serve_kernels_stopped(void)
{
    static CliKernel *s_taken[CLI_SERVE_KERNELS];
    unsigned int count = 0u;
    cli_lock_take(&s_lock);
    for (unsigned int at = 0u; at < CLI_SERVE_KERNELS; at += 1u)
    {
        if (s_kernel[at].used)
        {
            s_taken[count] = s_kernel[at].kernel;
            s_kernel[at].used = 0;
            count += 1u;
        }
    }
    cli_lock_give(&s_lock);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        kernels_ended(s_taken[at], CLI_SERVE_KERNEL_ENDING);
    }
}

// the kernel under the argument `key`, held for the call, or NULL with why said
static CliKernel *kernels_of(const CliServeArgs *args, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    if (key == NULL)
    {
        return NULL;
    }
    cli_lock_take(&s_lock);
    CliKernel *kernel = NULL;
    for (unsigned int at = 0u; (kernel == NULL) && (at < CLI_SERVE_KERNELS); at += 1u)
    {
        if (s_kernel[at].used &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_kernel[at].key, .other = key, .cap = CLI_CELLS_POOL})))
        {
            kernel = s_kernel[at].kernel;
            cli_jupyter_held(kernel);
        }
    }
    cli_lock_give(&s_lock);
    if (kernel == NULL)
    {
        cli_line_text(said, "no kernel runs for this notebook: pick one to start it");
    }
    return kernel;
}

// what a kernel says sent to the page as kernel-message under its key, its context
static void kernels_heard(void *context, const char *message, unsigned long long size)
{
    const char *const key = context;
    CliLine body;
    if (!cli_line_growing(&body, size + CLI_SERVE_KERNEL_MORE, "a kernel's message"))
    {
        return;
    }
    cli_line_text(&body, "{\"key\":");
    cli_json_quoted(&body, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
    cli_line_text(&body, ",\"message\":");
    cli_line_counted(&body, message, size);
    cli_line_character(&body, '}');
    if (cli_line_fits(&body))
    {
        cli_serve_emitted("kernel-message", body.out, body.at);
    }
    cli_cell_given(body.out);
}

static int kernel_specs(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_jupyter_specs_written(out);
    return 1;
}

static int kernels_stop_all(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_serve_kernels_stopped();
    cli_line_text(out, "null");
    return 1;
}

static int kernel_start(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static CliKernelSpec s_spec;
    static CliLock s_spec_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_LONGEST];
    const char *const key = cli_serve_string(args, "key", said);
    const char *const name = (key != NULL) ? cli_serve_string(args, "name", said) : NULL;
    if ((name == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_lock_take(&s_spec_lock);
    if (!cli_jupyter_spec_named(name, &s_spec))
    {
        cli_lock_give(&s_spec_lock);
        cli_line_text(said, "the machine has no kernel ");
        cli_line_text(said, name);
        return 0;
    }
    const char *notebook = key;
    while (cellul.starts(&(CatenaFinitaCfg){.src = notebook, .other = "check:", .cap = CLI_CELLS_POOL}))
    {
        notebook += 6;
    }
    char full[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    cli_full(root, notebook, full);
    if (!cli_path_parent(full, folder))
    {
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = root, .bytes = sizeof(folder)});
    }
    CliKernel *const old = kernels_taken_out(key);
    if (old != NULL)
    {
        kernels_ended(old, CLI_SERVE_KERNEL_ENDING);
    }
    cli_lock_take(&s_lock);
    // a place is free while a kernel can start, the places being twice the kernels that run at once
    unsigned int at = 0u;
    while (s_kernel[at].used)
    {
        at += 1u;
    }
    CliLine named = {s_kernel[at].key, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&named, key);
    (void)cli_line_closed(&named);
    CliKernel *const kernel = cli_jupyter_started(&s_spec, folder, kernels_heard, s_kernel[at].key, said);
    s_kernel[at].kernel = kernel;
    s_kernel[at].used = (kernel != NULL);
    cli_lock_give(&s_lock);
    cli_lock_give(&s_spec_lock);
    if (kernel == NULL)
    {
        return 0;
    }
    const char *const shown = cli_jupyter_display_name(kernel);
    cli_json_quoted(out, shown, cellul.len(&(CatenaFinitaCfg){.src = shown, .cap = CLI_CELLS_POOL}));
    return 1;
}

static int kernel_execute(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const code = cli_serve_string(args, "code", said);
    CliKernel *const kernel = (code != NULL) ? kernels_of(args, said) : NULL;
    if (kernel == NULL)
    {
        return 0;
    }
    char id[CLI_KERNEL_ID];
    const int done = cli_jupyter_executed(kernel, code,
                                          cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL}), id, said);
    cli_jupyter_given(kernel);
    if (done)
    {
        cli_json_quoted(out, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
    }
    return done;
}

static int kernel_debug(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    const char *const file = cli_serve_string(args, "file", said);
    const char *const code = (file != NULL) ? cli_serve_string(args, "code", said) : NULL;
    if ((code == NULL) || !cli_serve_tree(root, said) || !cli_inside(root, file, full, said))
    {
        return 0;
    }
    char folder[CLI_PATH_LONGEST];
    if (cli_path_parent(full, folder) && !query_folder_made(folder))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": it could not be made");
        return 0;
    }
    if (!cli_files_write(root, file, code, cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL}), said))
    {
        return 0;
    }
    CliKernel *const kernel = kernels_of(args, said);
    if (kernel == NULL)
    {
        return 0;
    }
    unsigned int port = 0u;
    const int done = cli_jupyter_debug_next(kernel, full, &port, said);
    cli_jupyter_given(kernel);
    if (done)
    {
        cli_line_decimal(out, port, 1u);
    }
    return done;
}

static int kernel_interrupt(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    CliKernel *const kernel = kernels_of(args, said);
    if (kernel == NULL)
    {
        return 0;
    }
    const int done = cli_jupyter_interrupted(kernel, said);
    cli_jupyter_given(kernel);
    cli_line_text(out, done ? "null" : "");
    return done;
}

static int kernel_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const key = cli_serve_string(args, "key", said);
    if (key == NULL)
    {
        return 0;
    }
    CliKernel *const kernel = kernels_taken_out(key);
    if (kernel == NULL)
    {
        cli_line_text(said, "no kernel runs for this notebook");
        return 0;
    }
    const int ended = cli_jupyter_stopped(kernel, CLI_SERVE_KERNEL_STOPPING);
    cli_jupyter_given(kernel);
    cli_line_text(out, ended ? "true" : "false");
    return 1;
}

// a request of `msg_type` asked of the kernel with the code and the cursor of the call, and the content of its reply
// written
static int kernels_asked(const CliServeArgs *args, const char *msg_type, int detail, CliLine *out, CliLine *said)
{
    const char *const code = cli_serve_string(args, "code", said);
    unsigned long long cursor = 0ull;
    if ((code == NULL) || !cli_serve_whole(args, "cursor", &cursor, said))
    {
        return 0;
    }
    CliKernel *const kernel = kernels_of(args, said);
    if (kernel == NULL)
    {
        return 0;
    }
    const unsigned long long code_length = cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL});
    CliLine content;
    CliLine reply;
    const int content_begun = cli_line_growing(&content, code_length + CLI_SERVE_KERNEL_MORE, "a kernel's request");
    const int reply_begun = cli_line_growing(&reply, CLI_SERVE_KERNEL_REPLY, "a kernel's reply");
    const int begun = content_begun && reply_begun;
    cli_line_text(&content, "{\"code\":");
    cli_json_quoted(&content, code, code_length);
    cli_line_text(&content, ",\"cursor_pos\":");
    cli_line_decimal(&content, cursor, 1u);
    cli_line_text(&content, detail ? ",\"detail_level\":0}" : "}");
    if (!begun)
    {
        cli_line_text(said, "the kernel's request could not be held");
    }
    // the waiter writes the reply only while the request waits on it, so it is read here and given back after
    const int done = begun && cli_line_fits(&content) &&
                     cli_jupyter_requested(kernel, "shell", msg_type, content.out, content.at, CLI_SERVE_KERNEL_ASKING,
                                           &reply, said);
    cli_jupyter_given(kernel);
    unsigned long long at = 0ull;
    unsigned long long length = 0ull;
    if (done && cli_lsp_member(reply.out, reply.at, "content", &at, &length))
    {
        cli_line_counted(out, reply.out + at, length);
    }
    else if (done)
    {
        cli_line_text(out, "null");
    }
    cli_cell_given(content.out);
    cli_cell_given(reply.out);
    return done;
}

static int kernel_complete(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    return kernels_asked(args, "complete_request", 0, out, said);
}

static int kernel_inspect(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    return kernels_asked(args, "inspect_request", 1, out, said);
}

const CliServeCall g_cli_serve_kernel_calls[] = {
    {"kernel_specs", kernel_specs},     {"kernels_stop_all", kernels_stop_all}, {"kernel_start", kernel_start},
    {"kernel_execute", kernel_execute}, {"kernel_debug", kernel_debug},         {"kernel_interrupt", kernel_interrupt},
    {"kernel_stop", kernel_stop},       {"kernel_complete", kernel_complete},   {"kernel_inspect", kernel_inspect},
};
const unsigned int g_cli_serve_kernel_count = sizeof(g_cli_serve_kernel_calls) / sizeof(g_cli_serve_kernel_calls[0]);
