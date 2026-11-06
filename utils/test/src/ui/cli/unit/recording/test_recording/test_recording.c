// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_recording.c: a recorded run stores each line with its frame's depth and values, a stack places each caller at its line, a
// name's history lists each line that changed it, steps go over, into, out and back, and continue and reverse continue stop at
// breakpoints and raises
#include "ui/cli/src/debug/recording_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_seen[512];
static VerbaLinea s_seen_line = {s_seen, sizeof(s_seen), 0ull};

// the values of frame `serial` as step `at` was about to run, `name=value` joined by `|`, are `required`
static void assert_values(const char *required, unsigned int serial, unsigned int at)
{
    CliRecordedValue *values = NULL;
    const unsigned int count = EMBED_CALL(cli_recording_at_internal.values, CliRecordingAtCtx, .serial = serial, .at = at, .reserved = &values, .most = 8u, .owner = g_checks_owner);
    char text[256];
    VerbaLinea line = {text, sizeof(text), 0ull};
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        mmgr_line_put(&line, (each > 0u) ? "|" : "");
        mmgr_line_put(&line, values[each].name);
        mmgr_line_ch(&line, '=');
        mmgr_line_put(&line, values[each].value);
    }
    g_cli_guard->persistent_buf_release(values, g_checks_owner);
    CLI_TEST_ASSERT_TEXT(required, text, line.at);
}

// the stack at `at`, `frame:step` joined by `|`, is `required`
static void assert_stack(const char *required, unsigned int at)
{
    unsigned int frames[8][2];
    const unsigned int count = EMBED_CALL(cli_recording_at_frames_internal.stack, CliRecordingAtFramesCtx, .at = at, .frames = frames, .capacity = 8u);
    char text[128];
    VerbaLinea line = {text, sizeof(text), 0ull};
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        mmgr_line_put(&line, (each > 0u) ? "|" : "");
        mmgr_line_decimal(&line, frames[each][0], 1u);
        mmgr_line_ch(&line, ':');
        mmgr_line_decimal(&line, frames[each][1], 1u);
    }
    CLI_TEST_ASSERT_TEXT(required, text, line.at);
}

// a change of a name recorded, `step:line:value` after the others
static void name_changed(const CliEachStepLineValueCfg *args)
{
    mmgr_line_put(&s_seen_line, (s_seen_line.at > 0ull) ? "|" : "");
    mmgr_line_decimal(&s_seen_line, args->step, 1u);
    mmgr_line_ch(&s_seen_line, ':');
    mmgr_line_decimal(&s_seen_line, args->line, 1u);
    mmgr_line_ch(&s_seen_line, ':');
    mmgr_line_put(&s_seen_line, (args->value != NULL) ? args->value : "");
}

// the history of `name` in frame `serial` is `required`
static void assert_history(const char *required, unsigned int serial, const char *name)
{
    s_seen_line.at = 0ull;
    EMBED_CALL(cli_recording_serial_internal.history, CliRecordingSerialCtx, .serial = serial, .name = name, .each = name_changed, .context = NULL);
    CLI_TEST_ASSERT_TEXT(required, s_seen, s_seen_line.at);
}

// `how` from step `at` goes to `step` for the reason `reason`
static void assert_goes(unsigned int step, const char *reason, unsigned int at, const char *how)
{
    g_cli_replay->at = at;
    unsigned int went = 0u;
    const char *const why = EMBED_CALL(cli_recording_how_internal.replay_go, CliRecordingHowCtx, .how = how, .step_pointer = &went);
    TEST_ASSERT_EQUAL_UINT(step, went);
    TEST_ASSERT_EQUAL_STRING(reason, why);
}

// a breakpoint set on line 2 of the recorded file
static void breakpoint_set(void)
{
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_recording_path_internal.replay_key, CliRecordingPathCtx, .path = "/tree/a.py", .key = key);
    memor.cpy(&(MemoriaCfg){.dst = g_cli_replay->break_files[0], .src = key, .bytes = CLI_UNITY_LENGTH(key) + 1ull});
    g_cli_replay->break_lines[0][0] = 2u;
    g_cli_replay->break_count[0] = 1u;
    g_cli_replay->break_file_count = 1u;
}

void setUp(void)
{
    // the recorder's lines for a function `grow` called twice from the module
    static const char s_lines[] = "[\"thread\",1,\"MainThread\"]\n[\"file\",0,\"a.py\"]\n[\"call\",1,0,0,\"<module>\",1]\n"
                                  "[\"line\",1,1,{},[]]\n[\"line\",1,5,{},[]]\n[\"call\",2,1,0,\"grow\",1]\n[\"line\",2,2,{\"n\":\"1\"},[]]\n"
                                  "[\"line\",2,3,{\"n\":\"2\"},[]]\n[\"return\",2,\"2\"]\n[\"line\",1,6,{\"a\":\"2\"},[]]\n"
                                  "[\"call\",3,1,0,\"grow\",1]\n[\"line\",3,2,{\"n\":\"2\"},[]]\n[\"line\",3,3,{\"n\":\"3\"},[]]\n"
                                  "[\"raise\",3,3,\"ValueError\",\"too big\"]\n[\"return\",3,\"None\"]\n[\"line\",1,7,{\"b\":\"3\"},[]]\n"
                                  "[\"end\",0]\n";
    char dir[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    cli_unity_begun();
    cli_unity_folder("recording", dir);
    cli_unity_file(dir, "r.jsonl", s_lines);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = dir, .relative = "r.jsonl", .joined = path);
    EMBED_CALL(cli_recording_path_internal.read, CliRecordingPathCtx, .path = path);
    memor.cpy(&(MemoriaCfg){.dst = g_cli_replay->root, .src = "/tree", .bytes = 6u});
    g_cli_replay->break_file_count = 0u;
    g_cli_replay->on_raised = 0;
}

void tearDown(void)
{
    cli_recording_let_go();
}

void test_each_line_is_recorded_with_its_frames_depth(void)
{
    static const unsigned int s_depths[8] = {1u, 1u, 2u, 2u, 1u, 2u, 2u, 1u};
    const CliRecording *const found = &g_cli_replay->recording;
    TEST_ASSERT_EQUAL_UINT(8u, found->step_count);
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(s_depths[at], found->steps[at].depth);
    }
}

void test_a_frames_names_are_recorded_with_their_values(void)
{
    assert_values("a=2|b=3", 1u, 7u);
}

void test_each_frame_has_its_own_values(void)
{
    assert_values("n=2", 2u, 3u);
}

void test_the_runs_exit_code_is_recorded(void)
{
    TEST_ASSERT_TRUE(g_cli_replay->recording.has_code);
    TEST_ASSERT_EQUAL_INT64(0ll, g_cli_replay->recording.code);
}

void test_a_raise_is_recorded_at_its_step(void)
{
    TEST_ASSERT_EQUAL_UINT(1u, g_cli_replay->recording.raised_count);
    TEST_ASSERT_EQUAL_UINT(6u, g_cli_replay->recording.raised[0].step);
}

void test_a_stack_positions_each_caller_at_its_line(void)
{
    assert_stack("2:3|1:1", 3u);
}

void test_a_stack_has_only_its_own_frame_once_the_call_returned(void)
{
    assert_stack("1:4", 4u);
}

void test_a_names_history_lists_each_line_that_changed_it(void)
{
    // the call sets n at the first line, then line 2 sets its second value
    assert_history("2:2:1|2:2:2", 2u, "n");
}

void test_a_names_history_is_of_its_own_frame(void)
{
    assert_history("1:5:2", 1u, "a");
}

void test_next_steps_over_a_call(void)
{
    assert_goes(4u, "step", 1u, "next");
}

void test_step_in_enters_a_call(void)
{
    assert_goes(2u, "step", 1u, "stepIn");
}

void test_step_out_leaves_a_call(void)
{
    assert_goes(4u, "step", 3u, "stepOut");
}

void test_step_back_goes_over_a_call_backward(void)
{
    assert_goes(1u, "step", 4u, "stepBack");
}

void test_next_at_the_last_step_ends(void)
{
    assert_goes(7u, "end", 7u, "next");
}

void test_continue_stops_at_a_breakpoint(void)
{
    breakpoint_set();
    assert_goes(2u, "breakpoint", 0u, "continue");
}

void test_continue_stops_at_the_next_breakpoint(void)
{
    breakpoint_set();
    assert_goes(5u, "breakpoint", 2u, "continue");
}

void test_reverse_continue_stops_at_a_breakpoint(void)
{
    breakpoint_set();
    assert_goes(5u, "breakpoint", 7u, "reverseContinue");
}

void test_continue_stops_at_a_raise_where_requested(void)
{
    breakpoint_set();
    g_cli_replay->on_raised = 1;
    assert_goes(6u, "exception", 5u, "continue");
}
