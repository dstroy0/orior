// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_jupyter.c: a message is signed and read back, a Python kernel runs cells and stops when interrupted, and a run prepared to debug is named by
// its file. The cases on a live kernel are ignored on a machine with no Python kernel
#include "ui/cli/src/notebooks/jupyter_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/text/digest.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// how long a kernel is waited for, in milliseconds
#define JUPYTER_PATIENCE 30000ull

static const char s_header[] = "{\"msg_id\":\"m1\",\"msg_type\":\"execute_request\"}";
static const char s_content[] = "{\"code\":\"1\"}";
static const char s_forged[] = "{\"code\":\"2\"}";
static const char s_file_code[] = "import sys\nsys._getframe().f_code.co_filename";

static CliKernel s_kernel;
static CliKernelReader s_reader;
static CliKernelSpec s_spec;
static CliWake s_wake = CLI_WAKE_INIT;
static char s_messages[1u << 23u];
static unsigned long long s_messages_at;
static char s_seen[1u << 23u];
static char s_result[1u << 12u];
static const CliJson *s_json;
static char s_signature[72];
static unsigned long long s_signature_size;
static CliZmtpFrame s_frames[7];
static int s_python_known;
static int s_python_found;

// what the live cells sequence recorded
static int s_cells_run_result;
static int s_cells_started;
static int s_print_received;
static int s_value_received;
static int s_error_received;
static int s_interrupted;
static int s_interrupt_stopped;
static int s_kernel_stopped;

// what the debug sequence recorded
static int s_debug_run_result;
static int s_debug_started;
static int s_debugpy_absent;
static char s_debug_error[512];
static unsigned int s_debug_port;
static int s_other_code_named_by_file;
static char s_debug_named[1u << 12u];
static int s_later_run_named_by_file;
static int s_debug_prepared_again;
static unsigned int s_debug_port_again;

// a signed message of seven frames set up, its content the signed one
static void frames_signed(void)
{
    char signed_text[256];
    VerbaLinea signed_parts = {signed_text, sizeof(signed_text), 0ull};
    mmgr_line_put(&signed_parts, s_header);
    mmgr_line_put(&signed_parts, "{}{}");
    mmgr_line_put(&signed_parts, s_content);
    VerbaLinea hex = {s_signature, sizeof(s_signature), 0ull};
    EMBED_CALL(cli_digest_text.hmac_sha256_text, CliDigestTextCfg, .key = "secret", .key_size = 6ull, .message = signed_text, .message_size = signed_parts.at, .text = &hex);
    s_signature_size = hex.at;
    memor.cpy(&(MemoriaCfg){.dst = s_kernel.key, .src = "secret", .bytes = 7ull});
    s_reader.kernel = &s_kernel;
    s_reader.channel = "shell";
    s_frames[0] = (CliZmtpFrame){(const unsigned char *)"route", 5ull};
    s_frames[1] = (CliZmtpFrame){(const unsigned char *)"<IDS|MSG>", 9ull};
    s_frames[2] = (CliZmtpFrame){(const unsigned char *)s_signature, s_signature_size};
    s_frames[3] = (CliZmtpFrame){(const unsigned char *)s_header, sizeof(s_header) - 1u};
    s_frames[4] = (CliZmtpFrame){(const unsigned char *)"{}", 2ull};
    s_frames[5] = (CliZmtpFrame){(const unsigned char *)"{}", 2ull};
    s_frames[6] = (CliZmtpFrame){(const unsigned char *)s_content, sizeof(s_content) - 1u};
}

// a message a kernel sent recorded, a line each
static void message_recorded(const CliKernelRecvCfg *args)
{
    if (s_messages_at + args->size + 1ull < sizeof(s_messages))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_messages + s_messages_at, .src = args->message, .bytes = args->size});
        s_messages[s_messages_at + args->size] = '\n';
        s_messages_at += args->size + 1ull;
    }
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_wake);
}

// the first message received whose parent is `id` and whose type is `kind` or `other`, and, where `member` is not NULL, whose content's `member`, or
// its `inner` where that is not NULL, is `required`: its node in the shared tree, CLI_JSON_NONE where none is
static unsigned int message_found(const char *id, const char *kind, const char *other, const char *member, const char *inner, const char *required)
{
    const unsigned long long size = s_messages_at;
    memor.cpy(&(MemoriaCfg){.dst = s_seen, .src = s_messages, .bytes = size});
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const end = memor.chr(&(MemoriaCfg){.src = s_seen + at, .val = '\n', .bytes = size - at});
        // a line ends inside what was seen, past its start
        const unsigned long long length = (unsigned long long)(end - (s_seen + at));
        unsigned int one = CLI_JSON_NONE;
        const CliJson *const json = cli_unity_parsed(s_seen + at, length, &one);
        s_json = json;
        at += length + 1ull;
        const char *const type = cli_unity_member_text(one, "msg_type");
        const int parent_matches = cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(one, "parent"), .other = id, .cap = CLI_POOL_BYTES});
        const int type_matches = cellul.eq(&(CatenaFinitaCfg){.src = type, .other = kind, .cap = CLI_POOL_BYTES}) || ((other != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = type, .other = other, .cap = CLI_POOL_BYTES}));
        if (!parent_matches || !type_matches)
        {
            continue;
        }
        const unsigned int content = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = one, .name = "content");
        const unsigned int holder = (inner != NULL) ? EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = content, .name = member) : content;
        if ((member == NULL) || cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(holder, (inner != NULL) ? inner : member), .other = required, .cap = CLI_POOL_BYTES}))
        {
            return one;
        }
    }
    return CLI_JSON_NONE;
}

// the run `id` waited for until its kernel is idle again, up to JUPYTER_PATIENCE
static void idle_waited(const char *id)
{
    const unsigned long long until = cli_clock_milliseconds() + JUPYTER_PATIENCE;
    while ((message_found(id, "status", NULL, "execution_state", NULL, "idle") == CLI_JSON_NONE) && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_wake, .milliseconds_u64 = 20ull);
    }
}

// whether the machine has a Python kernel that lacks nothing, read into s_spec once for the process
static int python_found(void)
{
    static char s_specs[1u << 20u];
    if (s_python_known)
    {
        return s_python_found;
    }
    s_python_known = 1;
    VerbaLinea out = {s_specs, sizeof(s_specs), 0ull};
    EMBED_CALL(cli_jupyter_out.specs_write, CliJupyterOutCfg, .out = &out);
    unsigned int specs = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_specs, out.at, &specs);
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = specs, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = specs, .index = at);
        const unsigned int install = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = one, .name = "install");
        const int python = cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(one, "language"), .other = "python", .cap = CLI_POOL_BYTES});
        if (python && ((install == CLI_JSON_NONE) || EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = install, .kind = CFG_JSON_NULL)))
        {
            s_python_found = EMBED_CALL(cli_jupyter_name.spec_by_name, CliJupyterNameCfg, .name = cli_unity_member_text(one, "name"), .spec = &s_spec);
            return s_python_found;
        }
    }
    return 0;
}

// `code` run on the kernel: its id into `id`, which is sized for CLI_KERNEL_ID
static void code_run(CliKernel *kernel, const char *code, char *id)
{
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    id[0] = '\0';
    (void)EMBED_CALL(cli_jupyter_err_msg_kernel_length.execute, CliJupyterErrMsgKernelLengthCfg, .kernel = kernel, .code = code, .length = CLI_UNITY_LENGTH(code), .id = id, .err_msg = &err_msg);
}

// the live cells sequence run once for the process: print, value, error, interrupt and stop
static void cells_run_result(void)
{
    if (s_cells_run_result)
    {
        return;
    }
    s_cells_run_result = 1;
    char folder[CLI_PATH_MAX_LEN];
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_unity_folder("jupyter_cells", folder);
    CliKernel *const kernel = EMBED_CALL(cli_jupyter_spec.start, CliJupyterSpecCfg, .spec_read = &s_spec, .folder = folder, .recv = message_recorded, .context = NULL, .err_msg = &err_msg);
    s_cells_started = kernel != NULL;
    if (kernel == NULL)
    {
        return;
    }
    char first[CLI_KERNEL_ID];
    code_run(kernel, "print('hi')\n1 + 1", first);
    idle_waited(first);
    s_print_received = message_found(first, "stream", NULL, "text", NULL, "hi\n") != CLI_JSON_NONE;
    s_value_received = message_found(first, "execute_result", NULL, "data", "text/plain", "2") != CLI_JSON_NONE;
    char failing[CLI_KERNEL_ID];
    code_run(kernel, "1 / 0", failing);
    idle_waited(failing);
    s_error_received = message_found(failing, "error", NULL, "ename", NULL, "ZeroDivisionError") != CLI_JSON_NONE;
    char looping[CLI_KERNEL_ID];
    code_run(kernel, "import time\nwhile True:\n    time.sleep(0.05)", looping);
    EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 800u);
    s_interrupted = EMBED_CALL(cli_jupyter_err_msg_kernel_file.interrupt, CliJupyterErrMsgKernelFileCfg, .kernel = kernel, .err_msg = &err_msg);
    idle_waited(looping);
    s_interrupt_stopped = message_found(looping, "error", NULL, "ename", NULL, "KeyboardInterrupt") != CLI_JSON_NONE;
    s_kernel_stopped = !EMBED_CALL(cli_jupyter_err_msg.stop, CliJupyterErrMsgCfg, .kernel = kernel, .wait = 10000ull);
    EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
}

// what the run `id` returned, its value as text or else its content written, waited for up to JUPYTER_PATIENCE: empty where nothing came
static const char *run_result(const char *id)
{
    const unsigned long long until = cli_clock_milliseconds() + JUPYTER_PATIENCE;
    unsigned int found = message_found(id, "execute_result", "error", NULL, NULL, NULL);
    while ((found == CLI_JSON_NONE) && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_wake, .milliseconds_u64 = 20ull);
        found = message_found(id, "execute_result", "error", NULL, NULL, NULL);
    }
    VerbaLinea result = {s_result, sizeof(s_result), 0ull};
    if (found != CLI_JSON_NONE)
    {
        const unsigned int content = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = found, .name = "content");
        const char *const plain = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = content, .name = "data"), .name = "text/plain"), .length_pointer = NULL);
        if (plain != NULL)
        {
            mmgr_line_put(&result, plain);
        }
        else
        {
            EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = s_json, .node = content, .out = &result, .indent = 0u, .sorted = 0);
        }
    }
    (void)mmgr_line_finish(&result);
    return s_result;
}

// the debug sequence run once for the process: the next run of a file's code is named by the file with debugpy listening, the runs after it and a
// run of other code are named as the kernel names them
static void debug_run_result(void)
{
    if (s_debug_run_result)
    {
        return;
    }
    s_debug_run_result = 1;
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {s_debug_error, sizeof(s_debug_error), 0ull};
    cli_unity_folder("jupyter_debug_cell", folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "cell.py", .joined = file);
    (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file, .bytes = s_file_code, .size = sizeof(s_file_code) - 1u);
    CliKernel *const kernel = EMBED_CALL(cli_jupyter_spec.start, CliJupyterSpecCfg, .spec_read = &s_spec, .folder = folder, .recv = message_recorded, .context = NULL, .err_msg = &err_msg);
    s_debug_started = kernel != NULL;
    if (kernel == NULL)
    {
        return;
    }
    err_msg.at = 0ull;
    if (!EMBED_CALL(cli_jupyter_err_msg_kernel_file.debug_next, CliJupyterErrMsgKernelFileCfg, .kernel = kernel, .file = file, .port = &s_debug_port, .err_msg = &err_msg))
    {
        (void)mmgr_line_finish(&err_msg);
        s_debugpy_absent = 1;
        (void)EMBED_CALL(cli_jupyter_err_msg.stop, CliJupyterErrMsgCfg, .kernel = kernel, .wait = 10000ull);
        EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
        return;
    }
    char id[CLI_KERNEL_ID];
    code_run(kernel, "import sys\nsys._getframe().f_code.co_filename + ''", id);
    s_other_code_named_by_file = cellul.find(&(CatenaFinitaCfg){.src = run_result(id), .cap = CLI_POOL_BYTES, .other = "cell.py", .other_cap = CLI_POOL_BYTES}) != NULL;
    code_run(kernel, s_file_code, id);
    VerbaLinea named = {s_debug_named, sizeof(s_debug_named), 0ull};
    mmgr_line_put(&named, run_result(id));
    (void)mmgr_line_finish(&named);
    code_run(kernel, s_file_code, id);
    s_later_run_named_by_file = cellul.find(&(CatenaFinitaCfg){.src = run_result(id), .cap = CLI_POOL_BYTES, .other = "cell.py", .other_cap = CLI_POOL_BYTES}) != NULL;
    err_msg.at = 0ull;
    s_debug_prepared_again = EMBED_CALL(cli_jupyter_err_msg_kernel_file.debug_next, CliJupyterErrMsgKernelFileCfg, .kernel = kernel, .file = file, .port = &s_debug_port_again, .err_msg = &err_msg);
    (void)EMBED_CALL(cli_jupyter_err_msg.stop, CliJupyterErrMsgCfg, .kernel = kernel, .wait = 10000ull);
    EMBED_CALL(cli_jupyter_err_msg_kernel_file.free, CliJupyterErrMsgKernelFileCfg, .kernel = kernel);
}

// the debug sequence run, the case ignored on a machine with no debugpy
static void debug_ready(void)
{
    if (!python_found())
    {
        TEST_IGNORE_MESSAGE("no Python kernel on this machine");
    }
    debug_run_result();
    TEST_ASSERT_TRUE(s_debug_started);
    if (s_debugpy_absent)
    {
        TEST_IGNORE_MESSAGE("the Python kernel has no debugpy");
    }
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_signed_message_reads_back_with_its_type(void)
{
    char out[512];
    char parent[CLI_KERNEL_ID];
    VerbaLinea message = {out, sizeof(out), 0ull};
    frames_signed();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_jupyter_out_internal.kernel_message_write, CliJupyterOutCtx, .reader = &s_reader, .frames = s_frames, .count = 7u, .out = &message, .parent_out = parent));
    unsigned int root = CLI_JSON_NONE;
    (void)cli_unity_parsed(out, message.at, &root);
    TEST_ASSERT_EQUAL_STRING("execute_request", cli_unity_member_text(root, "msg_type"));
}

void test_a_signed_message_reads_back_with_its_content(void)
{
    char out[512];
    char parent[CLI_KERNEL_ID];
    VerbaLinea message = {out, sizeof(out), 0ull};
    frames_signed();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_jupyter_out_internal.kernel_message_write, CliJupyterOutCtx, .reader = &s_reader, .frames = s_frames, .count = 7u, .out = &message, .parent_out = parent));
    unsigned int root = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(out, message.at, &root);
    TEST_ASSERT_EQUAL_STRING("1", cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = root, .name = "content"), "code"));
}

void test_a_forged_message_does_not_read_back(void)
{
    char out[512];
    char parent[CLI_KERNEL_ID];
    VerbaLinea message = {out, sizeof(out), 0ull};
    frames_signed();
    s_frames[6].bytes = (const unsigned char *)s_forged;
    s_frames[6].size = sizeof(s_forged) - 1u;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_jupyter_out_internal.kernel_message_write, CliJupyterOutCtx, .reader = &s_reader, .frames = s_frames, .count = 7u, .out = &message, .parent_out = parent));
}

void test_a_cells_print_is_received(void)
{
    if (!python_found())
    {
        TEST_IGNORE_MESSAGE("no Python kernel on this machine");
    }
    cells_run_result();
    TEST_ASSERT_TRUE(s_cells_started);
    TEST_ASSERT_TRUE(s_print_received);
}

void test_a_cells_value_is_received(void)
{
    if (!python_found())
    {
        TEST_IGNORE_MESSAGE("no Python kernel on this machine");
    }
    cells_run_result();
    TEST_ASSERT_TRUE(s_value_received);
}

void test_a_cells_error_is_received(void)
{
    if (!python_found())
    {
        TEST_IGNORE_MESSAGE("no Python kernel on this machine");
    }
    cells_run_result();
    TEST_ASSERT_TRUE(s_error_received);
}

void test_a_loop_interrupted_stops(void)
{
    if (!python_found())
    {
        TEST_IGNORE_MESSAGE("no Python kernel on this machine");
    }
    cells_run_result();
    TEST_ASSERT_TRUE(s_interrupted);
    TEST_ASSERT_TRUE(s_interrupt_stopped);
}

void test_the_kernel_stops_when_requested(void)
{
    if (!python_found())
    {
        TEST_IGNORE_MESSAGE("no Python kernel on this machine");
    }
    cells_run_result();
    TEST_ASSERT_TRUE(s_kernel_stopped);
}

void test_a_kernel_with_no_debugpy_reports_it(void)
{
    if (!python_found())
    {
        TEST_IGNORE_MESSAGE("no Python kernel on this machine");
    }
    debug_run_result();
    if (!s_debugpy_absent)
    {
        TEST_IGNORE_MESSAGE("the Python kernel has debugpy");
    }
    CLI_TEST_ASSERT_CONTAINS("has no debugpy", s_debug_error);
}

void test_debugpy_listens_at_a_port(void)
{
    debug_ready();
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_debug_port);
}

void test_other_code_is_named_as_the_kernel_names_it(void)
{
    debug_ready();
    TEST_ASSERT_FALSE(s_other_code_named_by_file);
}

void test_the_files_code_is_named_by_the_file(void)
{
    debug_ready();
    CLI_TEST_ASSERT_CONTAINS("jupyter_debug_cell", s_debug_named);
    CLI_TEST_ASSERT_CONTAINS("cell.py", s_debug_named);
}

void test_only_the_next_run_is_named_by_the_file(void)
{
    debug_ready();
    TEST_ASSERT_FALSE(s_later_run_named_by_file);
}

void test_debugpy_listens_at_the_same_port_again(void)
{
    debug_ready();
    TEST_ASSERT_TRUE_MESSAGE(s_debug_prepared_again, s_debug_error);
    TEST_ASSERT_EQUAL_UINT(s_debug_port, s_debug_port_again);
}
