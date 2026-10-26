// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_debug.c: a kernel's next run of a file's code readied to be debugged under that file's name, with debugpy
// listening in the kernel's process from the first time it is requested
#include "jupyter_internal.h"

// how long the kernel has to ready the run, in milliseconds
#define CLI_KERNEL_DEBUG_WAIT 30000ull

// The kernel's next run of the code `{file}` contains named by that file, in place of the name the kernel gives a cell:
// a breakpoint set in the file stops it. A run of other code retains the kernel's own name, and the first run of that
// code puts the kernel's own naming back
static const char s_debug_next[] =
    "def _orior_named(raw_code, transformed_code, number, _shell=get_ipython(), _file={file}, "
    "_named=type(get_ipython().compile).get_code_name):\n"
    "    with open(_file, encoding=\"utf-8\") as source:\n"
    "        meant = source.read()\n"
    "    if raw_code.replace(\"\\r\\n\", \"\\n\") != meant.replace(\"\\r\\n\", \"\\n\"):\n"
    "        return _named(_shell.compile, raw_code, transformed_code, number)\n"
    "    del _shell.compile.get_code_name\n"
    "    return _file\n"
    "get_ipython().compile.get_code_name = _orior_named\n"
    "del _orior_named\n";

// the string a path of members names under `node`, NULL where it names none or no string
static const char *jupyter_string_at(const CliJson *json, unsigned int node, const char *first, const char *second)
{
    const unsigned int member = cli_json_member(json, node, first);
    return cli_json_string(json, (second != NULL) ? cli_json_member(json, member, second) : member, NULL);
}

// an error a reply gives written as `ename: evalue`
static void jupyter_failed(const CliJson *json, unsigned int node, VerbaLinea *err_msg)
{
    const char *const name = jupyter_string_at(json, node, "ename", NULL);
    const char *const value = jupyter_string_at(json, node, "evalue", NULL);
    mmgr_line_put(err_msg, (name != NULL) ? name : "error");
    mmgr_line_put(err_msg, ": ");
    mmgr_line_put(err_msg, (value != NULL) ? value : "");
}

// a whole text read as a port, a + before it as Rust takes one: 1, or 0 where it reads as none
static int jupyter_port_read(const char *text, unsigned int *port)
{
    unsigned long long start = 0ull;
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    while ((start < end) &&
           ((text[start] == ' ') || (text[start] == '\t') || (text[start] == '\n') || (text[start] == '\r')))
    {
        start += 1ull;
    }
    while ((end > start) && ((text[end - 1ull] == ' ') || (text[end - 1ull] == '\t') || (text[end - 1ull] == '\n') ||
                             (text[end - 1ull] == '\r')))
    {
        end -= 1ull;
    }
    start += ((start < end) && (text[start] == '+')) ? 1ull : 0ull;
    unsigned int value = 0u;
    for (unsigned long long at = start; at < end; at += 1ull)
    {
        if ((text[at] < '0') || (text[at] > '9'))
        {
            return 0;
        }
        value = (value * 10u) + (unsigned int)(text[at] - '0');
        if (value > 65535u)
        {
            return 0;
        }
    }
    *port = value;
    return start < end;
}

// what the reply to the readying reports, read into `json` at `root`: the port debugpy listens at, 1, or 0 with why
// given
static int jupyter_reply_read(CliKernel *kernel, const CliJson *reply_json, unsigned int root, unsigned int *port,
                              VerbaLinea *err_msg)
{
    const char *const shown = cli_jupyter_display_name(kernel);
    const CliJson json = *reply_json;
    const unsigned int content = cli_json_member(&json, root, "content");
    const char *const status = jupyter_string_at(&json, content, "status", NULL);
    if ((status == NULL) || (!cellul.eq(&(CatenaFinitaCfg){.src = status, .other = "ok", .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(err_msg, shown);
        mmgr_line_put(err_msg, " cannot name a cell's code as its own file, as IPython 8 and later can: ");
        jupyter_failed(&json, content, err_msg);
        return 0;
    }
    if (kernel->listening)
    {
        *port = kernel->debug_port;
        return 1;
    }
    const unsigned int listened = cli_json_member(&json, cli_json_member(&json, content, "user_expressions"), "port");
    const char *const listened_status = jupyter_string_at(&json, listened, "status", NULL);
    if ((listened_status == NULL) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = listened_status, .other = "ok", .cap = CLI_CELLS_POOL})))
    {
        const char *const name = jupyter_string_at(&json, listened, "ename", NULL);
        if ((name != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "ModuleNotFoundError", .cap = CLI_CELLS_POOL})))
        {
            const char *const python =
                (kernel->spec.argv_count > 0u) ? cli_kernel_text(&kernel->spec, kernel->spec.argv[0]) : "python";
            mmgr_line_put(err_msg, shown);
            mmgr_line_put(err_msg, " has no debugpy: \"");
            mmgr_line_put(err_msg, python);
            mmgr_line_put(err_msg, "\" -m pip install debugpy installs it");
            return 0;
        }
        mmgr_line_put(err_msg, "debugpy did not listen in ");
        mmgr_line_put(err_msg, shown);
        mmgr_line_put(err_msg, ": ");
        jupyter_failed(&json, listened, err_msg);
        return 0;
    }
    const char *const plain = jupyter_string_at(&json, listened, "data", "text/plain");
    unsigned int listening = 0u;
    if ((plain == NULL) || !jupyter_port_read(plain, &listening))
    {
        mmgr_line_put(err_msg, "debugpy gave no port it listens at");
        return 0;
    }
    kernel->debug_port = listening;
    kernel->listening = 1;
    *port = listening;
    return 1;
}

// the readying sent to the kernel and its reply read, under the kernel's debug lock
static int jupyter_debug_request(CliKernel *kernel, const char *file, unsigned int *port, VerbaLinea *err_msg)
{
    char text[16384];
    char content[32768];
    VerbaLinea code = {text, sizeof(text), 0ull};
    const char *const mark = cellul.find(
        &(CatenaFinitaCfg){.src = s_debug_next, .cap = CLI_CELLS_POOL, .other = "{file}", .other_cap = CLI_CELLS_POOL});
    mmgr_line_put_n(&code, s_debug_next, (unsigned long long)(mark - s_debug_next));
    cli_json_quoted(&code, file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&code, mark + 6);
    VerbaLinea content_line = {content, sizeof(content), 0ull};
    mmgr_line_put(&content_line, "{\"code\":");
    cli_json_quoted(&content_line, code.out, code.at);
    mmgr_line_put(&content_line, ",\"silent\":true,\"store_history\":false,\"user_expressions\":");
    mmgr_line_put(&content_line,
                  kernel->listening ? "{}" : "{\"port\":\"__import__('debugpy').listen(('127.0.0.1', 0))[1]\"}");
    mmgr_line_put(&content_line, ",\"allow_stdin\":false,\"stop_on_error\":false}");
    const unsigned long long length = content_line.at;
    if (!mmgr_line_ok(&code) || !mmgr_line_ok(&content_line))
    {
        mmgr_line_put(err_msg, file);
        mmgr_line_put(err_msg, " is a longer path than orior_cli supports");
        return 0;
    }
    VerbaLinea reply = {kernel->debug_reply, sizeof(kernel->debug_reply), 0ull};
    if (!cli_jupyter_requested(kernel, "shell", "execute_request", content, length, CLI_KERNEL_DEBUG_WAIT, &reply,
                               err_msg))
    {
        return 0;
    }
    // the reply read into a tree taken to its size and given back
    CliJson json = {0};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, reply.out, reply.at, 0u, 0ull, &unread);
    const int read = jupyter_reply_read(kernel, &json, root, port, err_msg);
    cli_json_given(&json);
    return read;
}

int cli_jupyter_debug_next(CliKernel *kernel, const char *file, unsigned int *port, VerbaLinea *err_msg)
{
    cli_lock_take(&kernel->debug_lock);
    const int requested = jupyter_debug_request(kernel, file, port, err_msg);
    cli_lock_give(&kernel->debug_lock);
    return requested;
}
