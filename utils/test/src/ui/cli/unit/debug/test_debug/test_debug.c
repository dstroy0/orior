// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_debug.c: a function's bytecode is read with its lines, the machine's processes are listed with the lines they started with, launch arguments
// read the program and the folder, a frame counts its line from zero, breakpoints carry their conditions, counts and messages, memory comes as
// base64, stepping sets debugpy's rules and lldb's patterns, and C, Rust and Python each have a debugger
#include "ui/cli/src/debug/debug_internal.h"
#include "ui/cli/src/debug/debug_start.h"
#include "ui/cli/src/runs/runner.h"
#include "ui/cli/src/toolchains/toolchains.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_out[1u << 20u];
static CliJson s_step_tree;
static CliJsonNode s_to_nodes[1024];
static char s_to_text[1u << 14u];
static int s_processes_run_result;
static int s_process_started;
static int s_own_listing;
static int s_child_listing;
static char s_child_line[4096];
static unsigned long long s_in_use_before;
static unsigned long long s_in_use_after;
static unsigned long long s_high_water;
static unsigned long long s_list_consumed;

// `text` read into the step tree in a cell sized to it with capacity for the arguments a case adds to it, the tree before it released: its root
static unsigned int step_tree_read(const char *text)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    g_cli_guard->persistent_buf_release(s_step_tree.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_step_tree.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_step_tree),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &s_step_tree, .text = text, .size = CLI_UNITY_LENGTH(text), .nodes_more = 64u, .text_more = 4096ull, .err_msg = &err_msg, .owner = g_checks_owner);
}

// node `node` of `json` written compact, its keys in order, into s_out
static const char *node_written(const CliJson *json, unsigned int node)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = node, .out = &out, .indent = 0u, .sorted = 1);
    (void)mmgr_line_finish(&out);
    return s_out;
}

// the word of the line the child is started with
#if defined(_WIN32)
static const char s_expected[] = "127.0.0.1";
#else
static const char s_expected[] = "3";
#endif

// whether the processes listed contain `pid` with a line it started with: its line into s_child_line, and whether this process is listed recorded
static int process_listing(unsigned long long pid)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_debug_out_err_msg_id_step.processes, CliDebugOutErrMsgIdStepCfg, .out = &out);
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &read);
    s_child_line[0] = '\0';
    int found = 0;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = read, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = read, .index = at);
        const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = one, .name = "pid");
        unsigned long long value = 0ull;
        for (unsigned long long digit = 0ull; (number != CLI_JSON_NONE) && (digit < json->node[number].length); digit += 1ull)
        {
            // a digit's value is below 10
            value = (value * 10ull) + (unsigned long long)(json->text[json->node[number].text + digit] - '0');
        }
        s_own_listing = s_own_listing || (value == cli_process_own());
        if (value == pid)
        {
            const char *const command = cli_unity_member_text(one, "command");
            memor.cpy(&(MemoriaCfg){.dst = s_child_line, .src = command, .bytes = CLI_UNITY_LENGTH(command) + 1u});
            found = 1;
        }
    }
    return found && (s_child_line[0] != '\0');
}

// a child process started and listed, what the list reported recorded, once for the process
static void processes_run_result(void)
{
#if defined(_WIN32)
    const char *const words[8] = {"cmd", "/c", "ping", "-n", "4", "127.0.0.1", ">NUL", NULL};
#else
    const char *const words[3] = {"sleep", "3", NULL};
#endif
    CliSpawned child;
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (s_processes_run_result)
    {
        return;
    }
    s_processes_run_result = 1;
    s_process_started = EMBED_CALL(cli_words_err_msg_internal.spawn, CliWordsErrMsgCtx, .spawned = &child, .words = words, .folder = NULL, .settings = NULL, .count = 0u, .detached = 1, .err_msg = &err_msg);
    if (!s_process_started)
    {
        return;
    }
    s_in_use_before = mmgr_owner_in_use(g_cli_debug_processes_owner);
    const unsigned long long began = cli_clock_milliseconds();
    s_child_listing = process_listing(child.id);
    s_list_consumed = cli_clock_milliseconds() - began;
    s_in_use_after = mmgr_owner_in_use(g_cli_debug_processes_owner);
    s_high_water = mmgr_owner_hw(g_cli_debug_processes_owner);
    // on Linux a process stands under its parent's line until it has started its program, and has its own an instant after: the list is read again until
    // the child's own line shows
    for (unsigned int tries = 0u; (!s_child_listing || (cellul.find(&(CatenaFinitaCfg){.src = s_child_line, .cap = CLI_POOL_BYTES, .other = s_expected, .other_cap = CLI_POOL_BYTES}) == NULL)) && (tries < 40u); tries += 1u)
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 25u);
        s_child_listing = process_listing(child.id);
    }
    EMBED_CALL(cli_id_internal.spawn_tree_end, CliIdCtx, .id_u64 = child.id);
    (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &child);
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &child);
}

// a session's place with 100 breakpoints and a start planned on it
static CliDebugSession *position_planned(void)
{
    static const char s_start[] = "{\"name\":\"x\"}";
    static CliDebugPlan s_plan;
    CliDebugSession *const place = cli_debug_position();
    TEST_ASSERT_NOT_NULL(place);
    for (long long number = 0ll; number < 100ll; number += 1ll)
    {
        place->positioned[place->positioned_count] = number;
        place->positioned_count += 1u;
    }
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_start, sizeof(s_start) - 1u, &read);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)EMBED_CALL(cli_debug_start_table.debug_plan, CliDebugStartCfg, .place = place, .root = ".", .json = json, .start = read, .options = CLI_JSON_NONE, .plan = &s_plan, .err_msg = &err_msg);
    return place;
}

// `place` freed under the debugger's lock
static void position_freed(CliDebugSession *place)
{
    g_cli_guard->persistent_buf_release(place->positioned, g_cli_debug_owner);
    place->data = NULL;
    place->data_count = 0u;
    place->data_capacity = 0u;
    place->positioned = NULL;
    place->positioned_count = 0u;
    place->positioned_capacity = 0u;
    g_cli_guard->persistent_buf_release(place->start.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), place->start.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&place->start),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    place->reserved = 0;
}

// whether a tool of the manifest debugs `language`, or builds `type`
static int tool_has(const CliJson *json, unsigned int manifest, const char *language, const char *type)
{
    for (unsigned int tool = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = manifest, .type = CFG_JSON_ARRAY) ? json->node[manifest].first : CLI_JSON_NONE; tool != CLI_JSON_NONE; tool = json->node[tool].next)
    {
        if (type != NULL)
        {
            if (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = tool, .name = "builds"), .name = type) != CLI_JSON_NONE)
            {
                return 1;
            }
            continue;
        }
        const unsigned int languages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = tool, .name = "debugger"), .name = "languages");
        for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = languages, .index = at) != CLI_JSON_NONE; at += 1u)
        {
            const char *const one = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = languages, .index = at), .length_pointer = NULL);
            if ((one != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = one, .other = language, .cap = CLI_POOL_BYTES}))
            {
                return 1;
            }
        }
    }
    return 0;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_functions_bytecode_is_read_with_its_name_and_lines(void)
{
    static const char s_area[] = "x = 1\n\n\ndef area(width, height):\n    return width * height\n";
    char dir[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char python[CLI_PATH_MAX_LEN];
    char why[1024];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    cli_unity_folder("debug_bytecode", dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "a.py", .joined = file);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file, .bytes = s_area, .size = sizeof(s_area) - 1u));
    EMBED_CALL(cli_runner_name.python, CliRunnerNameCfg, .path = python);
    const char *const version[3] = {python, "--version", NULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    const int has_python = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = version, .folder = NULL, .settings = NULL, .count = 0u, .run_result = run_result, .err_msg = &err_msg) && (run_result->code == 0ll);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    if (!has_python)
    {
        TEST_IGNORE_MESSAGE("no Python to compile the function with");
    }
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    err_msg.at = 0ull;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_debug_root_out.bytecode, CliDebugRootOutCfg, .root = dir, .path = file, .line = 4u, .out = &out, .err_msg = &err_msg));
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_out, out.at, &read);
    TEST_ASSERT_EQUAL_STRING("area", EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = read, .index = 0u), .length_pointer = NULL));
    const unsigned int instructions = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = read, .index = 1u);
    int found = 0;
    for (unsigned int at = 0u; !found && (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = instructions, .index = at) != CLI_JSON_NONE); at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json, .array = instructions, .index = at);
        const unsigned int line = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = one, .name = "line");
        found = cellul.starts(&(CatenaFinitaCfg){.src = cli_unity_member_text(one, "text"), .other = "BINARY_OP", .cap = CLI_POOL_BYTES}) && (line != CLI_JSON_NONE) && (json->node[line].length == 1ull) && (json->text[json->node[line].text] == '4');
    }
    TEST_ASSERT_TRUE(found);
}

void test_the_machines_processes_are_listing_with_the_lines_they_started_with(void)
{
    processes_run_result();
    TEST_ASSERT_TRUE(s_process_started);
    TEST_ASSERT_TRUE(s_child_listing);
    CLI_TEST_ASSERT_CONTAINS(s_expected, s_child_line);
}

void test_this_process_is_not_listing(void)
{
    processes_run_result();
    TEST_ASSERT_FALSE(s_own_listing);
}

void test_the_processes_are_listing_in_their_own_cells_released_once_written(void)
{
    processes_run_result();
    TEST_ASSERT_EQUAL_UINT64(s_in_use_before, s_in_use_after);
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, s_high_water);
}

void test_the_list_is_quick_enough_for_a_picker(void)
{
    processes_run_result();
    TEST_ASSERT_LESS_THAN_UINT64(2000ull, s_list_consumed);
}

void test_launch_arguments_read_the_program_and_the_folder(void)
{
    static const char *const s_names[3] = {"program", "path", "root"};
    static const char *const s_withs[3] = {"a.exe", "a.c", "/t"};
    CliJson to;
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed("{\"program\":\"{program}\",\"cwd\":\"{root}\",\"args\":[],\"deep\":{\"file\":\"{path}\"}}", 73ull, &read);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &to, .node_json_node_pointer = s_to_nodes, .nodes_max = 1024u, .text_out = s_to_text, .text_max = sizeof(s_to_text));
    const unsigned int filled = EMBED_CALL(cli_debug_json_internal.fill, CliDebugJsonCtx, .to = &to, .json = json, .from = read, .names = s_names, .withs = s_withs, .count = 3u, .numbered = 0);
    TEST_ASSERT_EQUAL_STRING("{\"args\":[],\"cwd\":\"/t\",\"deep\":{\"file\":\"a.c\"},\"program\":\"a.exe\"}", node_written(&to, filled));
}

void test_a_number_filled_in_is_a_number(void)
{
    static const char *const s_pid[1] = {"pid"};
    static const char *const s_number[1] = {"412"};
    CliJson to;
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed("{\"processId\":\"{pid}\"}", 21ull, &read);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &to, .node_json_node_pointer = s_to_nodes, .nodes_max = 1024u, .text_out = s_to_text, .text_max = sizeof(s_to_text));
    const unsigned int filled = EMBED_CALL(cli_debug_json_internal.fill, CliDebugJsonCtx, .to = &to, .json = json, .from = read, .names = s_pid, .withs = s_number, .count = 1u, .numbered = 1);
    TEST_ASSERT_EQUAL_STRING("{\"processId\":412}", node_written(&to, filled));
}

void test_a_frame_counts_its_line_from_zero(void)
{
    static const char s_frame[] = "{\"id\":7,\"name\":\"main\",\"source\":{\"path\":\"/t/a.c\"},\"line\":6,\"column\":"
                                  "13,\"instructionPointerReference\":\"0x1000\"}";
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_frame, sizeof(s_frame) - 1u, &read);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_debug_out_internal.frame_write, CliDebugOutCtx, .json = json, .frame = read, .named = NULL, .context = NULL, .out = &out);
    CLI_TEST_ASSERT_TEXT("{\"id\":7,\"name\":\"main\",\"path\":\"/t/a.c\",\"line\":5,\"col\":12,\"ip\":\"0x1000\"}", s_out, out.at);
}

void test_breakpoints_retain_their_conditions_counts_and_messages(void)
{
    static const char s_sent[] = "[{\"line\":4},{\"line\":9,\"condition\":\"x > 2\",\"hits\":\">= 3\",\"log\":\"\"},{"
                                 "\"line\":11,\"log\":\"x is {x}\"}]";
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_sent, sizeof(s_sent) - 1u, &read);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_debug_session_internal.breakpoints_write, CliDebugSessionCtx, .json = json, .breakpoints = read, .out = &out);
    CLI_TEST_ASSERT_TEXT("[{\"line\":5},{\"line\":10,\"condition\":\"x > 2\",\"hitCondition\":\">= 3\"},{\"line\":12,"
                         "\"logMessage\":\"x is {x}\"}]",
                         s_out, out.at);
}

void test_a_session_accepts_the_files_of_its_language(void)
{
    TEST_ASSERT_TRUE(EMBED_CALL(cli_debug_file_internal.allocs, CliDebugFileCtx, .language = "python", .file = "a/b.py"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_debug_file_internal.allocs, CliDebugFileCtx, .language = "python", .file = "a/b.c"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_debug_file_internal.allocs, CliDebugFileCtx, .language = "c", .file = "a/b.c"));
    TEST_ASSERT_FALSE(EMBED_CALL(cli_debug_file_internal.allocs, CliDebugFileCtx, .language = "rust", .file = "x.py"));
}

void test_memory_comes_as_base64(void)
{
    unsigned char bytes[16];
    TEST_ASSERT_EQUAL_UINT64(5ull, EMBED_CALL(cli_debug_text_internal.base64, CliDebugTextCtx, .text = "SGVsbG8=", .size = 8ull, .out_bytes = bytes, .capacity = sizeof(bytes)));
    TEST_ASSERT_EQUAL_MEMORY("Hello", bytes, 5u);
}

void test_every_byte_of_memory_comes_through_base64(void)
{
    static const unsigned char s_requested[4] = {0u, 1u, 2u, 255u};
    unsigned char bytes[16];
    TEST_ASSERT_EQUAL_UINT64(4ull, EMBED_CALL(cli_debug_text_internal.base64, CliDebugTextCtx, .text = "AAEC/w==", .size = 8ull, .out_bytes = bytes, .capacity = sizeof(bytes)));
    TEST_ASSERT_EQUAL_MEMORY(s_requested, bytes, 4u);
}

void test_debugpy_steps_only_in_the_users_code(void)
{
    static const char s_python[] = "{\"mine_only\":true,\"skip\":[\"requests\",\"**/vendor/**\"]}";
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_python, sizeof(s_python) - 1u, &read);
    const unsigned int arguments = step_tree_read("{\"program\":\"a.py\"}");
    EMBED_CALL(cli_debug_json_language_internal.step_arguments, CliDebugJsonLanguageCtx, .language = "python", .json = json, .stepping = read, .to = &s_step_tree, .arguments = arguments);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_step_tree, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_step_tree, .object = arguments, .name = "justMyCode"), .type = CFG_JSON_TRUE));
}

void test_debugpy_skips_a_module_or_a_path(void)
{
    static const char s_python[] = "{\"mine_only\":true,\"skip\":[\"requests\",\"**/vendor/**\"]}";
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_python, sizeof(s_python) - 1u, &read);
    const unsigned int arguments = step_tree_read("{\"program\":\"a.py\"}");
    EMBED_CALL(cli_debug_json_language_internal.step_arguments, CliDebugJsonLanguageCtx, .language = "python", .json = json, .stepping = read, .to = &s_step_tree, .arguments = arguments);
    TEST_ASSERT_EQUAL_STRING("[{\"include\":false,\"module\":\"requests\"},{\"include\":false,\"path\":\"**/vendor/**\"}]", node_written(&s_step_tree, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_step_tree, .object = arguments, .name = "rules")));
}

void test_lldb_avoids_the_patterns_it_is_sent(void)
{
    static const char s_native[] = "{\"mine_only\":false,\"skip\":[\"^std::\",\"^boost::\"]}";
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_native, sizeof(s_native) - 1u, &read);
    const unsigned int arguments = step_tree_read("{\"program\":\"a.exe\",\"initCommands\":[\"x\"]}");
    EMBED_CALL(cli_debug_json_language_internal.step_arguments, CliDebugJsonLanguageCtx, .language = "c", .json = json, .stepping = read, .to = &s_step_tree, .arguments = arguments);
    TEST_ASSERT_EQUAL_STRING("[\"x\",\"settings set target.process.thread.step-avoid-regexp ^std::|^boost::\"]", node_written(&s_step_tree, EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_step_tree, .object = arguments, .name = "initCommands")));
}

void test_a_positions_breakpoints_are_in_one_cell_of_the_max_it_accepts(void)
{
    CliDebugSession *const place = position_planned();
    TEST_ASSERT_EQUAL_UINT(100u, place->positioned_count);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_POSITIONED, place->positioned_capacity);
    TEST_ASSERT_EQUAL_INT64(99ll, place->positioned[99]);
    position_freed(place);
}

void test_a_positions_start_is_a_tree_of_its_own(void)
{
    CliDebugSession *const place = position_planned();
    TEST_ASSERT_NOT_EQUAL(0u, place->start.nodes_max);
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_in_use(g_cli_debug_owner));
    position_freed(place);
}

void test_all_of_a_position_is_released_as_it_is_freed(void)
{
    CliDebugSession *const place = position_planned();
    position_freed(place);
    TEST_ASSERT_FALSE(place->reserved);
    TEST_ASSERT_NULL(place->positioned);
    TEST_ASSERT_EQUAL_UINT(0u, place->start.nodes_max);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_debug_owner));
}

void test_c_rust_and_python_each_have_a_debugger(void)
{
    CliJson json;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg, .json_out = &json, .nodes_more = 0u, .text_more = 0ull);
    const int c_debugger = tool_has(&json, manifest, "c", NULL);
    const int rust_debugger = tool_has(&json, manifest, "rust", NULL);
    const int python_debugger = tool_has(&json, manifest, "python", NULL);
    g_cli_guard->persistent_buf_release(json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    TEST_ASSERT_TRUE(c_debugger);
    TEST_ASSERT_TRUE(rust_debugger);
    TEST_ASSERT_TRUE(python_debugger);
}

void test_c_and_rust_have_a_build_and_python_does_not(void)
{
    CliJson json;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg, .json_out = &json, .nodes_more = 0u, .text_more = 0ull);
    const int c_build = tool_has(&json, manifest, NULL, "c.c");
    const int rust_build = tool_has(&json, manifest, NULL, "rust");
    const int python_build = tool_has(&json, manifest, NULL, "python");
    g_cli_guard->persistent_buf_release(json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    TEST_ASSERT_TRUE(c_build);
    TEST_ASSERT_TRUE(rust_build);
    TEST_ASSERT_FALSE(python_build);
}
