// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_serve.c: the server: base64 round-trips every length, a name of more than one word is found as the page writes it, calls over lines are
// answered by id, and a job started here sends its start, its lines and its end in order
#include "ui/cli/src/text/base64.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/window/serve_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_input[] = "{\"id\":1,\"name\":\"file_read\",\"args\":{\"path\":\"a.txt\"}}\n"
                              "{\"id\":2,\"name\":\"nothing_at_all\",\"args\":{}}\n"
                              "{\"id\":3,\"name\":\"file_write\",\"args\":{\"path\":\"b.txt\",\"text\":\"two\\n\"}}\n"
                              "{\"id\":4,\"name\":\"notebook_read\",\"args\":{\"path\":\"c.ipynb\"}}\n";

// a notebook of one cell whose source is stored as two lines
static const char s_notebook[] = "{\"cells\":[{\"cell_type\":\"code\",\"source\":[\"a = 1\\n\",\"b = 2\"],\"metadata\":{},\"outputs\":[],"
                                 "\"execution_count\":null}],\"metadata\":{},\"nbformat\":4,\"nbformat_minor\":5}";
static const char s_names_args[] = "{\"withErrors\": true, \"path\": \"a\"}";
static const char s_strings_args[] = "{\"paths\": [\"a\", \"b\", \"c\"], \"bad\": [\"a\", 1]}";

static unsigned long long s_read;
static char s_written[1u << 20u];
static unsigned long long s_written_at;
static CliLock s_lock = CLI_LOCK_INIT;
static char s_events[1u << 16u];
static unsigned long long s_events_at;
static int s_ended;
static char s_why[512];
static VerbaLinea s_err_msg;
static const CliJson *s_json;
static char s_dir[CLI_PATH_MAX_LEN];
static int s_lines_served;
static int s_job_ran;
static int s_job_started;
static int s_job_ended;
static unsigned long long s_gathering_in_use;

// `text` parsed into arguments
static CliServeArgs args_of(const char *text, unsigned long long size)
{
    unsigned int object = CLI_JSON_NONE;
    s_json = cli_unity_parsed(text, size, &object);
    return (CliServeArgs){s_json, object};
}

// a line of the input read, QRY_MEMBER_UNREAD once it ends
static unsigned long long input_line_read(const CliEachLineRoomCfg *args)
{
    const unsigned long long size = sizeof(s_input) - 1u;
    if (s_read >= size)
    {
        return QRY_MEMBER_UNREAD;
    }
    const char *const end = memor.chr(&(MemoriaCfg){.src = s_input + s_read, .val = '\n', .bytes = size - s_read});
    // a line ends inside the input, at or past the read position
    const unsigned long long length = (end != NULL) ? (unsigned long long)(end - (s_input + s_read)) : size - s_read;
    const unsigned long long copied = (length < args->room) ? length : args->room;
    memor.cpy(&(MemoriaCfg){.dst = args->line, .src = s_input + s_read, .bytes = copied});
    s_read += length + 1ull;
    return copied;
}

// what the server writes recorded
static void output_recorded(const CliEachBytesSizeCfg *args)
{
    const char *bytes = args->bytes;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (s_written_at + args->size < sizeof(s_written))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_written + s_written_at, .src = bytes, .bytes = args->size});
        s_written_at += args->size;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// the line the server wrote that answers `id`, or, where `id` is 0, the first line: its node in the shared tree, CLI_JSON_NONE where none does
static unsigned int answer_of(unsigned int id)
{
    for (unsigned long long at = 0ull; at < s_written_at;)
    {
        const char *const end = memor.chr(&(MemoriaCfg){.src = s_written + at, .val = '\n', .bytes = s_written_at - at});
        // a line ends inside what was received, at or past its start
        const unsigned long long length = (end != NULL) ? (unsigned long long)(end - (s_written + at)) : s_written_at - at;
        unsigned int line = CLI_JSON_NONE;
        s_json = cli_unity_parsed(s_written + at, length, &line);
        at += length + 1ull;
        const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = line, .name = "id");
        // a digit's value is below 10
        const int answers = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = number, .kind = CFG_JSON_NUMBER) && (s_json->node[number].length == 1u) && ((unsigned int)(s_json->text[s_json->node[number].text] - '0') == id);
        if ((id == 0u) || answers)
        {
            return line;
        }
    }
    return CLI_JSON_NONE;
}

// a tree's marker file written in the folder `name`, its full path in s_dir
static void tree_written(const char *name)
{
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder(name, s_dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "src/cu/engine", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    cli_unity_file(s_dir, "src/cu/engine/engine_config.h", "// marks the tree\n");
}

// the input's four calls served over lines, once for the process
static void lines_served(void)
{
    if (s_lines_served)
    {
        return;
    }
    s_lines_served = 1;
    tree_written("serve");
    cli_unity_file(s_dir, "a.txt", "one\n");
    cli_unity_file(s_dir, "c.ipynb", s_notebook);
    const CliServeLink link = {input_line_read, output_recorded, NULL};
    EMBED_CALL(cli_serve_root_link.over, CliServeRootLinkCfg, .root = s_dir, .link = &link);
}

// an event the server sends recorded as its name, a space and its body on a line, and whether a run ended
static void event_recorded(void *context, const char *event, const char *body, unsigned long long size)
{
    (void)context;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    VerbaLinea line = {s_events + s_events_at, sizeof(s_events) - s_events_at, 0ull};
    mmgr_line_put(&line, event);
    mmgr_line_ch(&line, ' ');
    mmgr_line_put_n(&line, body, size);
    mmgr_line_ch(&line, '\n');
    s_events_at += mmgr_line_ok(&line) ? line.at : 0ull;
    s_ended |= cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "run-end", .cap = CLI_CELLS_POOL});
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

// where `text` first is among what the server sent, past the sent text's end where it never is
static unsigned long long event_position(const char *text)
{
    const char *const found = cellul.find(&(CatenaFinitaCfg){.src = s_events, .cap = CLI_CELLS_POOL, .other = text, .other_cap = CLI_CELLS_POOL});
    // a find inside the sent text, at or past its start
    return (found != NULL) ? (unsigned long long)(found - s_events) : s_events_at + 1ull;
}

// a job of two lines started and waited on for up to thirty seconds, once for the process
static void job_ran(void)
{
    static const char s_args[] = "{\"job\": \"build/src/build_echo.sh\", \"values\": {\"arguments\": [\"\"], \"trace\": [\"quiet\"]}}";
    if (s_job_ran)
    {
        return;
    }
    s_job_ran = 1;
    tree_written("serve_jobs");
    cli_unity_file(s_dir, "src/build_echo.sh", "#!/usr/bin/env bash\necho first_line\necho second_line\n");
    EMBED_CALL(cli_serve_root_table.begin, CliServeRootCfg, .root = s_dir, .detached_dir = NULL);
    EMBED_CALL(cli_serve_emit.emit_set, CliServeEmitCfg, .emit = event_recorded, .context = NULL);
    const CliServeArgs args = args_of(s_args, sizeof(s_args) - 1u);
    char run[32];
    VerbaLinea out = {run, sizeof(run), 0ull};
    s_job_started = EMBED_CALL(cli_serve_err_msg_out_internal.job_start, CliServeErrMsgOutCtx, .job = "build/src/build_echo.sh", .request = &args, .out = &out, .err_msg = &s_err_msg);
    for (unsigned int wait = 0u; s_job_started && !s_job_ended && (wait < 600u); wait += 1u)
    {
        EMBED_CALL(query_milliseconds_internal.pause, QueryMillisecondsCtx, .milliseconds = 50u);
        EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
        s_job_ended = s_ended;
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    }
    s_gathering_in_use = embed_atomic_usize_load(&g_cli_serve_gathering_cells.in_use);
}

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_why, sizeof(s_why), 0ull};
}

void tearDown(void)
{
}

void test_base64_round_trips_every_length(void)
{
    for (unsigned int length = 0u; length < 40u; length += 1u)
    {
        unsigned char bytes[40];
        for (unsigned int at = 0u; at < length; at += 1u)
        {
            // a byte of the pattern, its low eight bits
            bytes[at] = (unsigned char)((at * 37u + 11u) & 0xffu);
        }
        char text[64];
        VerbaLinea line = {text, sizeof(text), 0ull};
        EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = &line, .bytes = bytes, .size = length);
        unsigned char back[40];
        unsigned long long size = 0ull;
        TEST_ASSERT_TRUE(EMBED_CALL(cli_base64_text.unbase64, CliBase64TextCfg, .text = text, .length = line.at, .out_bytes = back, .room = sizeof(back), .size_pointer = &size));
        TEST_ASSERT_EQUAL_UINT64(length, size);
        if (length > 0u)
        {
            TEST_ASSERT_EQUAL_MEMORY(bytes, back, length);
        }
    }
}

void test_base64_writes_the_standards_letters(void)
{
    char text[16];
    VerbaLinea line = {text, sizeof(text), 0ull};
    EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = &line, .bytes = (const unsigned char *)"orior", .size = 5ull);
    CLI_TEST_ASSERT_TEXT("b3Jpb3I=", text, line.at);
}

void test_a_name_of_two_words_is_found_as_the_page_writes_it(void)
{
    const CliServeArgs args = args_of(s_names_args, sizeof(s_names_args) - 1u);
    int with_errors = 0;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_serve_err_msg_internal.serve_bool, CliServeErrMsgCtx, .request = &args, .key = "with_errors", .value_i32s = &with_errors, .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(with_errors);
}

void test_a_name_of_one_word_is_itself(void)
{
    const CliServeArgs args = args_of(s_names_args, sizeof(s_names_args) - 1u);
    TEST_ASSERT_EQUAL_STRING("a", EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = &args, .key = "path", .err_msg = &s_err_msg));
}

void test_a_name_not_sent_is_none(void)
{
    const CliServeArgs args = args_of(s_names_args, sizeof(s_names_args) - 1u);
    TEST_ASSERT_EQUAL_UINT(CLI_JSON_NONE, EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = &args, .key = "repo"));
}

void test_an_arrays_strings_are_read_into_a_cell_of_their_count(void)
{
    const CliServeArgs args = args_of(s_strings_args, sizeof(s_strings_args) - 1u);
    const char **items = NULL;
    unsigned int count = 0u;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_serve_err_msg_internal.strings_take, CliServeErrMsgCtx, .request = &args, .key = "paths", .most = 8u, .items_pointer = &items, .count = &count, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_UINT(3u, count);
    TEST_ASSERT_EQUAL_STRING("c", items[2]);
    TEST_ASSERT_NULL(items[3]);
    TEST_ASSERT_NOT_EQUAL(0ull, embed_atomic_usize_load(&g_cli_serve_strings_cells.in_use));
    EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = items);
}

void test_no_more_strings_are_read_than_requested(void)
{
    const CliServeArgs args = args_of(s_strings_args, sizeof(s_strings_args) - 1u);
    const char **items = NULL;
    unsigned int count = 0u;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_serve_err_msg_internal.strings_take, CliServeErrMsgCtx, .request = &args, .key = "paths", .most = 2u, .items_pointer = &items, .count = &count, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_UINT(2u, count);
    TEST_ASSERT_NULL(items[2]);
    EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = items);
}

void test_an_array_not_all_strings_is_refused_and_each_cell_is_released(void)
{
    const CliServeArgs args = args_of(s_strings_args, sizeof(s_strings_args) - 1u);
    const char **items = NULL;
    unsigned int count = 0u;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_serve_err_msg_internal.strings_take, CliServeErrMsgCtx, .request = &args, .key = "bad", .most = 8u, .items_pointer = &items, .count = &count, .err_msg = &s_err_msg));
    EMBED_CALL(cli_serve_items_internal.strings_free, CliServeItemsCtx, .items = items);
    TEST_ASSERT_EQUAL_UINT64(0ull, embed_atomic_usize_load(&g_cli_serve_strings_cells.in_use));
}

void test_the_server_reports_it_serves_first(void)
{
    lines_served();
    TEST_ASSERT_EQUAL_STRING("serving", cli_unity_member_text(answer_of(0u), "event"));
}

void test_a_call_is_answered_with_what_it_read(void)
{
    lines_served();
    const unsigned int line = answer_of(1u);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = line, .name = "ok"), "text"), .other = "one", .cap = CLI_CELLS_POOL}));
}

void test_a_call_of_no_name_reports_it(void)
{
    lines_served();
    CLI_TEST_ASSERT_CONTAINS("nothing_at_all", cli_unity_member_text(answer_of(2u), "error"));
}

void test_a_call_that_answers_nothing_is_null(void)
{
    lines_served();
    const unsigned int line = answer_of(3u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = line, .name = "ok"), .kind = CFG_JSON_NULL));
}

void test_what_a_call_writes_is_written(void)
{
    char text[16];
    char path[CLI_PATH_MAX_LEN];
    char dir[CLI_PATH_MAX_LEN];
    lines_served();
    TEST_ASSERT_TRUE(EMBED_CALL(query_room_path_internal.path_full, QueryRoomPathCtx, .path = "serve", .full = dir, .room = sizeof(dir)));
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "b.txt", .joined = path);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)text, sizeof(text));
    TEST_ASSERT_NOT_EQUAL(QRY_MEMBER_UNREAD, size);
    CLI_TEST_ASSERT_TEXT("two\n", text, size);
}

void test_a_notebook_is_read_with_a_cells_lines_joined(void)
{
    lines_served();
    const unsigned int line = answer_of(4u);
    const unsigned int cell = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = line, .name = "ok"), .name = "cells"), .index = 0u);
    TEST_ASSERT_EQUAL_STRING("a = 1\nb = 2", cli_unity_member_text(cell, "source"));
}

void test_a_job_started_here_ends(void)
{
    job_ran();
    TEST_ASSERT_TRUE(s_job_started);
    TEST_ASSERT_TRUE(s_job_ended);
}

void test_a_jobs_start_comes_first_its_lines_in_order_and_its_end_last(void)
{
    job_ran();
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const unsigned long long start = event_position("run-start ");
    const unsigned long long first = event_position("first_line");
    const unsigned long long second = event_position("second_line");
    const unsigned long long end = event_position("run-end ");
    const unsigned long long size = s_events_at;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    TEST_ASSERT_LESS_THAN_UINT64(first, start);
    TEST_ASSERT_LESS_THAN_UINT64(second, first);
    TEST_ASSERT_LESS_THAN_UINT64(end, second);
    TEST_ASSERT_LESS_THAN_UINT64(size, end);
}

void test_a_jobs_gathering_is_released(void)
{
    job_ran();
    TEST_ASSERT_EQUAL_UINT64(0ull, s_gathering_in_use);
}
