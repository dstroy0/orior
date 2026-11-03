// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// jupyter_debug.c: a kernel's next run of a file's code readied to be debugged under that file's name, with debugpy
// listening in the kernel's process from the first time it is requested
#include "../text/text.h"
#include "jupyter_internal.h"

// how long the kernel has to ready the run, in milliseconds
#define CLI_KERNEL_DEBUG_WAIT 30000ULL

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

typedef struct
{
    const CliJson *json;
    unsigned int node;
    const char *first;
    const char *second;
    VerbaLinea *err_msg;
    const char *text;
    unsigned int *port;
    CliKernel *kernel;
    const CliJson *reply_json;
    unsigned int root;
    const char *file;
} JupyterDebugCtx;
// the string a path of members names under `node`, NULL where it names none or no string
EMBED_INLINE const char *jupyter_string_at(const JupyterDebugCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = node, .name = args->first);
    return EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                      .node = (args->second != NULL)
                                  ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = member, .name = args->second)
                                  : member,
                      .length_pointer = NULL);
}

// an error a reply gives written as `ename: evalue`
EMBED_INLINE void jupyter_fail(const JupyterDebugCtx *args)
{
    const char *const name = EMBED_CALL(jupyter_string_at, JupyterDebugCtx, .json = args->json, .node = args->node,
                                        .first = "ename", .second = NULL);
    const char *const value = EMBED_CALL(jupyter_string_at, JupyterDebugCtx, .json = args->json, .node = args->node,
                                         .first = "evalue", .second = NULL);
    mmgr_line_put(args->err_msg, (name != NULL) ? name : "error");
    mmgr_line_put(args->err_msg, ": ");
    mmgr_line_put(args->err_msg, (value != NULL) ? value : "");
}

// a whole text read as a port, a + before it as Rust takes one: 1, or 0 where it reads as none
EMBED_INLINE int jupyter_port_read(const JupyterDebugCtx *args)
{
    const char *text = args->text;
    unsigned int *port = args->port;
    unsigned long long start = 0ULL;
    unsigned long long end = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    start = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = start,
                       .size = end, .set = " \t\n\r");
    end = (start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = text + start,
                              .at = end - start, .set = " \t\n\r"));
    start += ((start < end) && (text[start] == '+')) ? 1ULL : 0ULL;
    unsigned int value = 0u;
    for (unsigned long long at = start; at < end; at += 1ULL)
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
EMBED_INLINE int jupyter_reply_read(const JupyterDebugCtx *args)
{
    CliKernel *kernel = args->kernel;
    unsigned int *port = args->port;
    const char *const displayed = EMBED_CALL(cli_jupyter.display_name, CliJupyterCfg, .kernel_read = kernel);
    const CliJson json = *args->reply_json;
    const unsigned int content = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = args->root, .name = "content");
    const char *const status = EMBED_CALL(jupyter_string_at, JupyterDebugCtx, .json = &json, .node = content,
                                          .first = "status", .second = NULL);
    if ((status == NULL) || (!cellul.eq(&(CatenaFinitaCfg){.src = status, .other = "ok", .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(args->err_msg, displayed);
        mmgr_line_put(args->err_msg, " cannot name a cell's code as its own file, as IPython 8 and later can: ");
        EMBED_CALL(jupyter_fail, JupyterDebugCtx, .json = &json, .node = content, .err_msg = args->err_msg);
        return 0;
    }
    if (kernel->listening)
    {
        *port = kernel->debug_port;
        return 1;
    }
    const unsigned int listened =
        EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                   .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                        .object = content, .name = "user_expressions"),
                   .name = "port");
    const char *const listened_status = EMBED_CALL(jupyter_string_at, JupyterDebugCtx, .json = &json, .node = listened,
                                                   .first = "status", .second = NULL);
    if ((listened_status == NULL) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = listened_status, .other = "ok", .cap = CLI_CELLS_POOL})))
    {
        const char *const name = EMBED_CALL(jupyter_string_at, JupyterDebugCtx, .json = &json, .node = listened,
                                            .first = "ename", .second = NULL);
        if ((name != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "ModuleNotFoundError", .cap = CLI_CELLS_POOL})))
        {
            const char *const python = (kernel->spec.argv_count > 0u)
                                           ? EMBED_CALL(cli_jupyter_spec_internal.kernel_text, CliJupyterSpecCtx,
                                                        .spec = &kernel->spec, .at = kernel->spec.argv[0])
                                           : "python";
            mmgr_line_put(args->err_msg, displayed);
            mmgr_line_put(args->err_msg, " has no debugpy: \"");
            mmgr_line_put(args->err_msg, python);
            mmgr_line_put(args->err_msg, "\" -m pip install debugpy installs it");
            return 0;
        }
        mmgr_line_put(args->err_msg, "debugpy did not listen in ");
        mmgr_line_put(args->err_msg, displayed);
        mmgr_line_put(args->err_msg, ": ");
        EMBED_CALL(jupyter_fail, JupyterDebugCtx, .json = &json, .node = listened, .err_msg = args->err_msg);
        return 0;
    }
    const char *const plain = EMBED_CALL(jupyter_string_at, JupyterDebugCtx, .json = &json, .node = listened,
                                         .first = "data", .second = "text/plain");
    unsigned int listening = 0u;
    if ((plain == NULL) || !EMBED_CALL(jupyter_port_read, JupyterDebugCtx, .text = plain, .port = &listening))
    {
        mmgr_line_put(args->err_msg, "debugpy gave no port it listens at");
        return 0;
    }
    kernel->debug_port = listening;
    kernel->listening = 1;
    *port = listening;
    return 1;
}

// the readying sent to the kernel and its reply read, under the kernel's debug lock
EMBED_INLINE int jupyter_debug_request(const JupyterDebugCtx *args)
{
    CliKernel *kernel = args->kernel;
    VerbaLinea *err_msg = args->err_msg;
    char text[16384];
    char content[32768];
    VerbaLinea code = {text, sizeof(text), 0ULL};
    const char *const mark = cellul.find(
        &(CatenaFinitaCfg){.src = s_debug_next, .cap = CLI_CELLS_POOL, .other = "{file}", .other_cap = CLI_CELLS_POOL});
    mmgr_line_put_n(&code, s_debug_next, (unsigned long long)(mark - s_debug_next));
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &code, .text = args->file,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&code, mark + 6);
    VerbaLinea content_line = {content, sizeof(content), 0ULL};
    mmgr_line_put(&content_line, "{\"code\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &content_line, .text = code.out,
               .length = code.at);
    mmgr_line_put(&content_line, ",\"silent\":true,\"store_history\":false,\"user_expressions\":");
    mmgr_line_put(&content_line,
                  kernel->listening ? "{}" : "{\"port\":\"__import__('debugpy').listen(('127.0.0.1', 0))[1]\"}");
    mmgr_line_put(&content_line, ",\"allow_stdin\":false,\"stop_on_error\":false}");
    const unsigned long long length = content_line.at;
    if (!mmgr_line_ok(&code) || !mmgr_line_ok(&content_line))
    {
        mmgr_line_put(err_msg, args->file);
        mmgr_line_put(err_msg, " is a longer path than orior_cli supports");
        return 0;
    }
    VerbaLinea reply = {kernel->debug_reply, CLI_KERNEL_REPLY_CAPACITY, 0ULL};
    if (!EMBED_CALL(cli_jupyter_err_msg.request, CliJupyterErrMsgCfg, .kernel = kernel, .channel = "shell",
                    .msg_type = "execute_request", .content = content, .length = length, .wait = CLI_KERNEL_DEBUG_WAIT,
                    .reply = &reply, .err_msg = err_msg))
    {
        return 0;
    }
    // the reply read into a tree taken to its size and given back
    CliJson json = {0};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                         .json = &json, .text = reply.out, .size = reply.at, .nodes_more = 0u,
                                         .text_more = 0ULL, .err_msg = &unread, .owner = &g_cli_notebook_cells);
    const int read = EMBED_CALL(jupyter_reply_read, JupyterDebugCtx, .kernel = kernel, .reply_json = &json,
                                .root = root, .port = args->port, .err_msg = err_msg);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return read;
}

int cli_jupyter_debug_next(const CliJupyterErrMsgKernelFileCfg *args)
{
    const int requested = EMBED_CALL(jupyter_debug_request, JupyterDebugCtx, .kernel = args->kernel, .file = args->file,
                                     .port = args->port, .err_msg = args->err_msg);
    return requested;
}
