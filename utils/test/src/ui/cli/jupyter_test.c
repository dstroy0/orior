// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_test.c: the kernels' check: a message is signed and read back; a Python kernel runs cells and stops when
// interrupted; and a run readied to debug is named by its file. The last two run on the Python kernel the machine has,
// where it has one
#include "../../../../../ui/cli/src/notebooks/jupyter_internal.h"
#include "../../../../../ui/cli/src/text/digest.h"
#include "cli_check.h"

// how long a kernel is waited for, in milliseconds
#define JUPYTER_PATIENCE 30000ull

static CliKernel s_kernel;
static CliKernelReader s_reader;
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;
static char s_heard[1u << 23u];
static unsigned long long s_heard_at;
static char s_seen[1u << 23u];
static char s_result[1u << 12u];
static const CliJson *s_json;

// a message is signed and read back
static void jupyter_signed_read(void)
{
    static const char s_header[] = "{\"msg_id\":\"m1\",\"msg_type\":\"execute_request\"}";
    static const char s_content[] = "{\"code\":\"1\"}";
    static const char s_forged[] = "{\"code\":\"2\"}";
    char signed_text[256];
    CliLine signed_parts = {signed_text, sizeof(signed_text), 0ull};
    cli_line_text(&signed_parts, s_header);
    cli_line_text(&signed_parts, "{}{}");
    cli_line_text(&signed_parts, s_content);
    char signature[72];
    CliLine hex = {signature, sizeof(signature), 0ull};
    cli_hmac_sha256_text("secret", 6ull, signed_text, signed_parts.at, &hex);
    memor.cpy(&(MemoriaCfg){.dst = s_kernel.key,
                            .src = "secret",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "secret", .cap = CLI_CELLS_POOL}) + 1u});
    s_reader.kernel = &s_kernel;
    s_reader.channel = "shell";
    CliZmtpFrame frames[7] = {{(const unsigned char *)"route", 5ull},
                              {(const unsigned char *)"<IDS|MSG>", 9ull},
                              {(const unsigned char *)signature, hex.at},
                              {(const unsigned char *)s_header, sizeof(s_header) - 1u},
                              {(const unsigned char *)"{}", 2ull},
                              {(const unsigned char *)"{}", 2ull},
                              {(const unsigned char *)s_content, sizeof(s_content) - 1u}};
    char out[512];
    CliLine message = {out, sizeof(out), 0ull};
    char parent[CLI_KERNEL_ID];
    cli_check(cli_kernel_message_written(&s_reader, frames, 7u, &message, parent), "a signed message reads back");
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(out, message.at, &root);
    cli_check(
        (cellul.eq(&(CatenaFinitaCfg){
            .src = cli_check_member_text(root, "msg_type"), .other = "execute_request", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_member(json, root, "content"), "code"),
                                          .other = "1",
                                          .cap = CLI_CELLS_POOL})),
        "past its routing, with its type and its content");
    frames[6].bytes = (const unsigned char *)s_forged;
    frames[6].size = sizeof(s_forged) - 1u;
    message.at = 0ull;
    cli_check(!cli_kernel_message_written(&s_reader, frames, 7u, &message, parent), "a forged message does not");
}

// a message a kernel said kept, a line each
static void jupyter_heard(void *context, const char *message, unsigned long long size)
{
    (void)context;
    cli_lock_take(&s_lock);
    if (s_heard_at + size + 1ull < sizeof(s_heard))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_heard + s_heard_at, .src = message, .bytes = size});
        s_heard[s_heard_at + size] = '\n';
        s_heard_at += size + 1ull;
    }
    cli_wake_all(&s_wake);
    cli_lock_give(&s_lock);
}

// the first message heard whose parent is `id` and whose type is `kind`, or one of the two types `kind` and `other`,
// and, where `member` is not NULL, whose content's `member`, or its `inner` where that is not NULL, is `wanted`: its
// node in the shared tree, CLI_JSON_NONE where none is
static unsigned int jupyter_said(const char *id, const char *kind, const char *other, const char *member,
                                 const char *inner, const char *wanted)
{
    cli_lock_take(&s_lock);
    const unsigned long long size = s_heard_at;
    memor.cpy(&(MemoriaCfg){.dst = s_seen, .src = s_heard, .bytes = size});
    cli_lock_give(&s_lock);
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const end = memor.chr(&(MemoriaCfg){.src = s_seen + at, .val = '\n', .bytes = size - at});
        // a line ends inside what was seen, past its start
        const unsigned long long length = (unsigned long long)(end - (s_seen + at));
        unsigned int one = CLI_JSON_NONE;
        const CliJson *const json = cli_check_parsed(s_seen + at, length, &one);
        s_json = json;
        at += length + 1ull;
        const char *const type = cli_check_member_text(one, "msg_type");
        if ((!cellul.eq(
                &(CatenaFinitaCfg){.src = cli_check_member_text(one, "parent"), .other = id, .cap = CLI_CELLS_POOL})) ||
            ((!cellul.eq(&(CatenaFinitaCfg){.src = type, .other = kind, .cap = CLI_CELLS_POOL})) &&
             ((other == NULL) || (!cellul.eq(&(CatenaFinitaCfg){.src = type, .other = other, .cap = CLI_CELLS_POOL})))))
        {
            continue;
        }
        const unsigned int content = cli_json_member(json, one, "content");
        const unsigned int holder = (inner != NULL) ? cli_json_member(json, content, member) : content;
        if ((member == NULL) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(holder, (inner != NULL) ? inner : member),
                                          .other = wanted,
                                          .cap = CLI_CELLS_POOL})))
        {
            return one;
        }
    }
    return CLI_JSON_NONE;
}

// the run `id` waited for until its kernel is idle again, up to JUPYTER_PATIENCE
static void jupyter_idle(const char *id)
{
    const unsigned long long until = cli_clock_milliseconds() + JUPYTER_PATIENCE;
    while ((jupyter_said(id, "status", NULL, "execution_state", NULL, "idle") == CLI_JSON_NONE) &&
           (cli_clock_milliseconds() < until))
    {
        cli_lock_take(&s_lock);
        (void)cli_wake_wait_for(&s_wake, &s_lock, 20ull);
        cli_lock_give(&s_lock);
    }
}

// the machine's Python kernel that lacks nothing into `spec`: 1, or 0 where it has none
static int jupyter_python(CliKernelSpec *spec)
{
    static char s_specs[1u << 20u];
    CliLine out = {s_specs, sizeof(s_specs), 0ull};
    cli_jupyter_specs_written(&out);
    unsigned int specs = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_specs, out.at, &specs);
    for (unsigned int at = 0u; cli_json_item(json, specs, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(json, specs, at);
        const unsigned int install = cli_json_member(json, one, "install");
        if ((cellul.eq(&(CatenaFinitaCfg){
                .src = cli_check_member_text(one, "language"), .other = "python", .cap = CLI_CELLS_POOL})) &&
            ((install == CLI_JSON_NONE) || cli_json_is(json, install, CFG_JSON_NULL)))
        {
            return cli_jupyter_spec_named(cli_check_member_text(one, "name"), spec);
        }
    }
    return 0;
}

// `code` run on the kernel: its id into `id`, which holds CLI_KERNEL_ID
static void jupyter_run(CliKernel *kernel, const char *code, char *id)
{
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    id[0] = '\0';
    (void)cli_jupyter_executed(kernel, code, cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_CELLS_POOL}), id,
                               &said);
}

// a Python kernel runs cells and stops when interrupted
static void jupyter_cells(const CliKernelSpec *spec)
{
    char folder[CLI_PATH_LONGEST];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("jupyter_cells", folder);
    CliKernel *const kernel = cli_jupyter_started(spec, folder, jupyter_heard, NULL, &said);
    cli_check(kernel != NULL, "the Python kernel starts");
    if (kernel == NULL)
    {
        return;
    }
    char first[CLI_KERNEL_ID];
    jupyter_run(kernel, "print('hi')\n1 + 1", first);
    jupyter_idle(first);
    cli_check(jupyter_said(first, "stream", NULL, "text", NULL, "hi\n") != CLI_JSON_NONE, "a cell's print is heard");
    cli_check(jupyter_said(first, "execute_result", NULL, "data", "text/plain", "2") != CLI_JSON_NONE, "and its value");
    char failing[CLI_KERNEL_ID];
    jupyter_run(kernel, "1 / 0", failing);
    jupyter_idle(failing);
    cli_check(jupyter_said(failing, "error", NULL, "ename", NULL, "ZeroDivisionError") != CLI_JSON_NONE,
              "a cell's error is heard");
    char looping[CLI_KERNEL_ID];
    jupyter_run(kernel, "import time\nwhile True:\n    time.sleep(0.05)", looping);
    query_paused(800u);
    cli_check(cli_jupyter_interrupted(kernel, &said), "the kernel is interrupted");
    jupyter_idle(looping);
    cli_check(jupyter_said(looping, "error", NULL, "ename", NULL, "KeyboardInterrupt") != CLI_JSON_NONE,
              "a loop interrupted stops");
    cli_check(!cli_jupyter_stopped(kernel, 10000ull), "the kernel stops when asked");
    cli_jupyter_given(kernel);
}

// what the run `id` gave, its value as text or else its content written, waited for up to JUPYTER_PATIENCE: the empty
// text where nothing came
static const char *jupyter_result(const char *id)
{
    const unsigned long long until = cli_clock_milliseconds() + JUPYTER_PATIENCE;
    unsigned int found = jupyter_said(id, "execute_result", "error", NULL, NULL, NULL);
    while ((found == CLI_JSON_NONE) && (cli_clock_milliseconds() < until))
    {
        cli_lock_take(&s_lock);
        (void)cli_wake_wait_for(&s_wake, &s_lock, 20ull);
        cli_lock_give(&s_lock);
        found = jupyter_said(id, "execute_result", "error", NULL, NULL, NULL);
    }
    CliLine result = {s_result, sizeof(s_result), 0ull};
    if (found != CLI_JSON_NONE)
    {
        const unsigned int content = cli_json_member(s_json, found, "content");
        const char *const plain = cli_json_string(
            s_json, cli_json_member(s_json, cli_json_member(s_json, content, "data"), "text/plain"), NULL);
        if (plain != NULL)
        {
            cli_line_text(&result, plain);
        }
        else
        {
            cli_json_write(s_json, content, &result, 0u, 0);
        }
    }
    (void)cli_line_closed(&result);
    return s_result;
}

// the next run of a file's code is named by the file, with debugpy listening; the runs after it, and a run of other
// code, are named as the kernel names them
static void jupyter_debugged(const CliKernelSpec *spec)
{
    static const char s_code[] = "import sys\nsys._getframe().f_code.co_filename";
    char folder[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("jupyter_debug_cell", folder);
    cli_full(folder, "cell.py", file);
    (void)query_file_written(file, s_code, sizeof(s_code) - 1u);
    CliKernel *const kernel = cli_jupyter_started(spec, folder, jupyter_heard, NULL, &said);
    if (kernel == NULL)
    {
        cli_check(0, "the Python kernel starts");
        return;
    }
    unsigned int port = 0u;
    said.at = 0ull;
    if (!cli_jupyter_debug_next(kernel, file, &port, &said))
    {
        (void)cli_line_closed(&said);
        cli_check(cellul.find(&(CatenaFinitaCfg){
                      .src = why, .cap = CLI_CELLS_POOL, .other = "has no debugpy", .other_cap = CLI_CELLS_POOL}) !=
                      NULL,
                  "a kernel with no debugpy says so");
        (void)cli_jupyter_stopped(kernel, 10000ull);
        cli_jupyter_given(kernel);
        return;
    }
    cli_check(port > 0u, "debugpy listens at a port");
    char id[CLI_KERNEL_ID];
    jupyter_run(kernel, "import sys\nsys._getframe().f_code.co_filename + ''", id);
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = jupyter_result(id), .cap = CLI_CELLS_POOL, .other = "cell.py", .other_cap = CLI_CELLS_POOL}) ==
                  NULL,
              "other code is named as the kernel names it");
    jupyter_run(kernel, s_code, id);
    const char *const named = jupyter_result(id);
    cli_check((cellul.find(&(CatenaFinitaCfg){
                   .src = named, .cap = CLI_CELLS_POOL, .other = "jupyter_debug_cell", .other_cap = CLI_CELLS_POOL}) !=
               NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = named, .cap = CLI_CELLS_POOL, .other = "cell.py", .other_cap = CLI_CELLS_POOL}) != NULL),
              "the file's code is named by the file");
    jupyter_run(kernel, s_code, id);
    cli_check(cellul.find(&(CatenaFinitaCfg){
                  .src = jupyter_result(id), .cap = CLI_CELLS_POOL, .other = "cell.py", .other_cap = CLI_CELLS_POOL}) ==
                  NULL,
              "and only its next run");
    unsigned int again = 0u;
    cli_check(cli_jupyter_debug_next(kernel, file, &again, &said) && (again == port),
              "debugpy listens at the same port again");
    (void)cli_jupyter_stopped(kernel, 10000ull);
    cli_jupyter_given(kernel);
}

int main(void)
{
    jupyter_signed_read();
    static CliKernelSpec s_spec;
    if (!jupyter_python(&s_spec))
    {
        cli_check(1, "the machine has no Python kernel, and none runs");
        return cli_checked();
    }
    jupyter_cells(&s_spec);
    jupyter_debugged(&s_spec);
    return cli_checked();
}
