// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_debug.c: a kernel's next run of a file's code readied to be debugged under that file's name, with debugpy
// listening in the kernel's process from the first time it is asked
#include "jupyter_internal.h"

// how long the kernel has to ready the run, in milliseconds
#define CLI_KERNEL_DEBUG_WAIT 30000ull

// The kernel's next run of the code `{file}` holds named by that file, in place of the name the kernel gives a cell: a
// breakpoint set in the file stops it. A run of other code keeps the kernel's own name, and the first run of that code
// puts the kernel's own naming back
static const char s_debug_next[] =
    "def _orior_named(raw_code, transformed_code, number, _shell=get_ipython(), _file={file}, "
    "_named=type(get_ipython().compile).get_code_name):\n"
    "    with open(_file, encoding=\"utf-8\") as held:\n"
    "        meant = held.read()\n"
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
static void jupyter_failed(const CliJson *json, unsigned int node, CliLine *said)
{
    const char *const name = jupyter_string_at(json, node, "ename", NULL);
    const char *const value = jupyter_string_at(json, node, "evalue", NULL);
    cli_line_text(said, (name != NULL) ? name : "error");
    cli_line_text(said, ": ");
    cli_line_text(said, (value != NULL) ? value : "");
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

// what the reply to the readying says, read into `json` at `root`: the port debugpy listens at, 1, or 0 with why said
static int jupyter_reply_read(CliKernel *kernel, const CliJson *reply_json, unsigned int root, unsigned int *port,
                              CliLine *said)
{
    const char *const shown = cli_jupyter_display_name(kernel);
    const CliJson json = *reply_json;
    const unsigned int content = cli_json_member(&json, root, "content");
    const char *const status = jupyter_string_at(&json, content, "status", NULL);
    if ((status == NULL) || (!cellul.eq(&(CatenaFinitaCfg){.src = status, .other = "ok", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(said, shown);
        cli_line_text(said, " cannot name a cell's code as its own file, as IPython 8 and later can: ");
        jupyter_failed(&json, content, said);
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
            cli_line_text(said, shown);
            cli_line_text(said, " has no debugpy: \"");
            cli_line_text(said, python);
            cli_line_text(said, "\" -m pip install debugpy installs it");
            return 0;
        }
        cli_line_text(said, "debugpy did not listen in ");
        cli_line_text(said, shown);
        cli_line_text(said, ": ");
        jupyter_failed(&json, listened, said);
        return 0;
    }
    const char *const plain = jupyter_string_at(&json, listened, "data", "text/plain");
    unsigned int listening = 0u;
    if ((plain == NULL) || !jupyter_port_read(plain, &listening))
    {
        cli_line_text(said, "debugpy gave no port it listens at");
        return 0;
    }
    kernel->debug_port = listening;
    kernel->listening = 1;
    *port = listening;
    return 1;
}

// the readying asked of the kernel and its reply read, under the kernel's debug lock
static int jupyter_debug_asked(CliKernel *kernel, const char *file, unsigned int *port, CliLine *said)
{
    char text[16384];
    char content[32768];
    CliLine code = {text, sizeof(text), 0ull};
    const char *const mark = cellul.find(
        &(CatenaFinitaCfg){.src = s_debug_next, .cap = CLI_CELLS_POOL, .other = "{file}", .other_cap = CLI_CELLS_POOL});
    cli_line_counted(&code, s_debug_next, (unsigned long long)(mark - s_debug_next));
    cli_json_quoted(&code, file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    cli_line_text(&code, mark + 6);
    CliLine asked = {content, sizeof(content), 0ull};
    cli_line_text(&asked, "{\"code\":");
    cli_json_quoted(&asked, code.out, code.at);
    cli_line_text(&asked, ",\"silent\":true,\"store_history\":false,\"user_expressions\":");
    cli_line_text(&asked,
                  kernel->listening ? "{}" : "{\"port\":\"__import__('debugpy').listen(('127.0.0.1', 0))[1]\"}");
    cli_line_text(&asked, ",\"allow_stdin\":false,\"stop_on_error\":false}");
    const unsigned long long length = asked.at;
    if (!cli_line_fits(&code) || !cli_line_fits(&asked))
    {
        cli_line_text(said, file);
        cli_line_text(said, " is a longer path than orior_cli holds");
        return 0;
    }
    CliLine reply = {kernel->debug_reply, sizeof(kernel->debug_reply), 0ull};
    if (!cli_jupyter_requested(kernel, "shell", "execute_request", content, length, CLI_KERNEL_DEBUG_WAIT, &reply,
                               said))
    {
        return 0;
    }
    // the reply read into a tree taken to its size and given back
    CliJson json = {0};
    char why[CLI_SAID_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, reply.out, reply.at, 0u, 0ull, &unread);
    const int read = jupyter_reply_read(kernel, &json, root, port, said);
    cli_json_given(&json);
    return read;
}

int cli_jupyter_debug_next(CliKernel *kernel, const char *file, unsigned int *port, CliLine *said)
{
    cli_lock_take(&kernel->debug_lock);
    const int asked = jupyter_debug_asked(kernel, file, port, said);
    cli_lock_give(&kernel->debug_lock);
    return asked;
}
