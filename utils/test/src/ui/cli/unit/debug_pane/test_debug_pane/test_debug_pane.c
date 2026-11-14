// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_debug_pane.c: the Debug pane against a session of the suite's own, a table of requests answering fixed data:
// with no session the views show their notices and no step acts; a stop reads three frames, the first chosen, the
// status line its sentence; a frame in the tree opens at its line and one outside does not; the first scope is open
// and the rest closed, a variable opens and closes and stays open over a new stop; a watch is evaluated and its box
// takes it out, a refusal its failed value; the console runs a piece on into its line, drops its oldest line past its
// max, and Enter evaluates in the chosen frame; the bar's flags follow the session and each step reaches its request;
// continued, exited and terminated change the rows and the status line; and the views' labels and the view displayed
#include "ui/cli/src/debug/debug_pane.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the tree the suite's paths are under, and the session and thread its adapter reports
#define CHECK_ROOT "/check/tree"
#define CHECK_SESSION 1ULL

// what the suite's adapter was last asked to step, how many stops it was asked for, whether it steps back and
// whether its steps are refused
static char s_how[32];
static unsigned int s_stops;
static int s_step_back;
static int s_refused;
static char s_lines[1u << 16u];

static void sessions_written(const CliDebugOutErrMsgIdStepCfg *args)
{
    mmgr_line_put(args->out, "[{\"id\":1,\"name\":\"prog.py\",\"tool\":\"debugpy\",\"language\":\"python\",\"step_back\":");
    mmgr_line_put(args->out, s_step_back ? "true}]" : "false}]");
}

static int threads_written(const CliDebugOutErrMsgIdStepCfg *args)
{
    mmgr_line_put(args->out, "[{\"id\":7,\"name\":\"main\"}]");
    return 1;
}

// three frames: one in the tree, one outside it, and one with no file
static int stack_written(const CliDebugOutErrMsgIdThreadCfg *args)
{
    mmgr_line_put(args->out, "[{\"id\":100,\"name\":\"inner\",\"path\":");
    args->named(&(CliDebugPathWrittenCfg){.context = args->context, .path = CHECK_ROOT "/src/prog.py", .out = args->out});
    mmgr_line_put(args->out, ",\"line\":11,\"col\":0,\"ip\":null},{\"id\":101,\"name\":\"outer\",\"path\":");
    args->named(&(CliDebugPathWrittenCfg){.context = args->context, .path = "/usr/lib/other.py", .out = args->out});
    mmgr_line_put(args->out, ",\"line\":3,\"col\":0,\"ip\":null},{\"id\":102,\"name\":\"<module>\",\"path\":null,"
                             "\"line\":0,\"col\":0,\"ip\":null}]");
    return 1;
}

static int scopes_written(const CliDebugOutErrMsgCfg *args)
{
    mmgr_line_put(args->out, "[{\"name\":\"Locals\",\"reference\":10,\"expensive\":false},"
                             "{\"name\":\"Globals\",\"reference\":20,\"expensive\":false}]");
    return 1;
}

static int variables_written(const CliDebugOutCfg *args)
{
    switch (args->reference)
    {
    case 10LL:
        mmgr_line_put(args->out, "[{\"name\":\"count\",\"value\":\"3\",\"type\":\"int\",\"reference\":0,\"memory\":null},"
                                 "{\"name\":\"items\",\"value\":\"[1, 2]\",\"type\":\"list\",\"reference\":30,"
                                 "\"memory\":null}]");
        return 1;
    case 30LL:
        mmgr_line_put(args->out, "[{\"name\":\"0\",\"value\":\"1\",\"type\":\"int\",\"reference\":0,\"memory\":null},"
                                 "{\"name\":\"1\",\"value\":\"2\",\"type\":\"int\",\"reference\":0,\"memory\":null}]");
        return 1;
    default:
        mmgr_line_put(args->out, "[{\"name\":\"G\",\"value\":\"9\",\"type\":\"int\",\"reference\":0,\"memory\":null}]");
        return 1;
    }
}

static int evaluated(const CliDebugOutErrMsgCfg *args)
{
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->expression, .other = "count", .cap = 64ULL}))
    {
        mmgr_line_put(args->err_msg, "name 'missing' is not defined");
        return 0;
    }
    mmgr_line_put(args->out, "{\"name\":\"count\",\"value\":\"3\",\"type\":\"int\",\"reference\":0,\"memory\":null}");
    return 1;
}

static int stepped(const CliDebugOutErrMsgIdThreadHowCfg *args)
{
    if (s_refused)
    {
        mmgr_line_put(args->err_msg, "the adapter refused the step");
        return 0;
    }
    VerbaLinea how = {s_how, sizeof(s_how), 0ULL};
    mmgr_line_put(&how, args->how);
    (void)mmgr_line_finish(&how);
    return 1;
}

static int stepped_across(const CliDebugRootOutErrMsgCfg *args)
{
    (void)args;
    VerbaLinea how = {s_how, sizeof(s_how), 0ULL};
    mmgr_line_put(&how, "across");
    (void)mmgr_line_finish(&how);
    return 1;
}

static void stopped(const CliDebugCfg *args)
{
    (void)args;
    s_stops += 1u;
}

static const CliDebugPaneRequestsNs s_requests = {
    .sessions = sessions_written,
    .threads = threads_written,
    .stack = stack_written,
    .scopes = scopes_written,
    .variables = variables_written,
    .evaluate = evaluated,
    .step = stepped,
    .step_across = stepped_across,
    .stop = stopped,
};

void setUp(void)
{
    cli_unity_begun();
    s_how[0] = '\0';
    s_stops = 0u;
    s_step_back = 0;
    s_refused = 0;
    EMBED_CALL(cli_debug_pane_view.begin, CliDebugPaneViewCfg, .root = CHECK_ROOT, .requests = &s_requests);
}

void tearDown(void)
{
}

// the event `event` with the body `body` of the suite's session passed to the sink, then the step run
static int event_read(const char *event, const char *body)
{
    const CliDebugSink sink = EMBED_CALL(cli_debug_pane_view.sink, CliDebugPaneViewCfg, .root = CHECK_ROOT);
    sink.emit(&(CliDebugEventCfg){.context = sink.context, .session = CHECK_SESSION, .event = event, .body = body,
                                  .size = CLI_UNITY_LENGTH(body)});
    return EMBED_CALL(cli_debug_pane_view.step, CliDebugPaneViewCfg, .root = CHECK_ROOT, .now_ms = 1ULL);
}

static int stop_read(void)
{
    return event_read("stopped", "{\"reason\":\"breakpoint\",\"threadId\":7}");
}

// the row `at` of `view` read into `row`, whose texts are in the row's own bytes
static void row_of(unsigned int view, unsigned int at, CliDebugPaneRow *row)
{
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_view.row, CliDebugPaneViewCfg, .view = view, .at = at,
                                        .row = row));
}

// the row `at` of `view` read into the one row retained here, for a member read at once
static CliDebugPaneRow s_row;
static const CliDebugPaneRow *row_at(unsigned int view, unsigned int at)
{
    row_of(view, at, &s_row);
    return &s_row;
}

static unsigned int rows_of(unsigned int view)
{
    return EMBED_CALL(cli_debug_pane_view.rows, CliDebugPaneViewCfg, .view = view);
}

static CliDebugBarItem item_of(unsigned int at)
{
    CliDebugBarItem item;
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_bar.item, CliDebugPaneBarCfg, .at = at, .item = &item));
    return item;
}

static CliDebugPaneStatus status_of(void)
{
    CliDebugPaneStatus status;
    EMBED_CALL(cli_debug_pane_bar.status, CliDebugPaneBarCfg, .status_out = &status);
    return status;
}

// `text` typed into the field and Enter pressed
static void entered(const char *text)
{
    EMBED_CALL(cli_debug_pane_input.type, CliDebugPaneInputCfg, .text = text, .size = CLI_UNITY_LENGTH(text));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_input.key, CliDebugPaneInputCfg, .name = "Enter",
                                        .root = CHECK_ROOT));
}

void test_with_no_session_the_views_show_their_notices_and_no_step_acts(void)
{
    TEST_ASSERT_EQUAL_UINT(1u, rows_of(CLI_DEBUG_VIEW_FRAMES));
    CliDebugPaneRow frames;
    row_of(CLI_DEBUG_VIEW_FRAMES, 0u, &frames);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_ROW_NOTICE, frames.type);
    CLI_TEST_ASSERT_TEXT("Frames show here when the program stops.", frames.text, frames.text_length);
    TEST_ASSERT_EQUAL_UINT(1u, rows_of(CLI_DEBUG_VIEW_VARIABLES));
    CliDebugPaneRow variables;
    row_of(CLI_DEBUG_VIEW_VARIABLES, 0u, &variables);
    CLI_TEST_ASSERT_TEXT("Variables show here when the program stops.", variables.text, variables.text_length);
    TEST_ASSERT_EQUAL_UINT(0u, rows_of(CLI_DEBUG_VIEW_CONSOLE));
    CliDebugPaneRow none;
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_view.row, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEW_FRAMES,
                                        .at = 1u, .row = &none));
    const CliDebugPaneStatus status = status_of();
    CLI_TEST_ASSERT_TEXT("Debug File (Shift+F9) runs the file in the editor under its debugger.", status.text,
                         status.length);
    for (unsigned int at = 0u; at < CLI_DEBUG_STEPS; at += 1u)
    {
        TEST_ASSERT_EQUAL_INT(0, item_of(at).enabled);
        TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_bar.run, CliDebugPaneBarCfg, .at = at, .root = CHECK_ROOT));
    }
    TEST_ASSERT_EQUAL_INT(0, item_of(CLI_DEBUG_STEP_BACK).displayed);
    TEST_ASSERT_EQUAL_INT(1, item_of(CLI_DEBUG_STEP_CONTINUE).displayed);
    const CliDebugBarItem item = item_of(CLI_DEBUG_STEP_OVER);
    CLI_TEST_ASSERT_TEXT("Step Over", item.label, item.label_length);
    TEST_ASSERT_EQUAL_STRING("step-over", item.command);
    TEST_ASSERT_EQUAL_STRING("F10", item.keys);
    CliDebugBarItem past;
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_bar.item, CliDebugPaneBarCfg, .at = CLI_DEBUG_STEPS,
                                        .item = &past));
}

void test_a_stop_reads_the_frames_the_first_chosen(void)
{
    const CliDebugSink sink = EMBED_CALL(cli_debug_pane_view.sink, CliDebugPaneViewCfg, .root = CHECK_ROOT);
    sink.emit(&(CliDebugEventCfg){.context = sink.context, .session = CHECK_SESSION, .event = "stopped",
                                  .body = "{\"reason\":\"breakpoint\",\"threadId\":7}", .size = 36ULL});
    TEST_ASSERT_EQUAL_UINT64(5ULL, EMBED_CALL(cli_debug_pane_view.deadline_ms, CliDebugPaneViewCfg, .now_ms = 5ULL));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_view.step, CliDebugPaneViewCfg, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_UINT64(0ULL, EMBED_CALL(cli_debug_pane_view.deadline_ms, CliDebugPaneViewCfg, .now_ms = 6ULL));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_view.step, CliDebugPaneViewCfg, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_UINT(3u, rows_of(CLI_DEBUG_VIEW_FRAMES));
    CliDebugPaneRow top;
    row_of(CLI_DEBUG_VIEW_FRAMES, 0u, &top);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_ROW_FRAME, top.type);
    CLI_TEST_ASSERT_TEXT("inner", top.text, top.text_length);
    CLI_TEST_ASSERT_TEXT("prog.py:12", top.second, top.second_length);
    CLI_TEST_ASSERT_TEXT("src/prog.py", top.third, top.third_length);
    TEST_ASSERT_EQUAL_UINT(11u, top.line);
    TEST_ASSERT_EQUAL_INT(1, top.current);
    TEST_ASSERT_EQUAL_INT(0, row_at(CLI_DEBUG_VIEW_FRAMES, 1u)->current);
    CliDebugPaneRow fileless;
    row_of(CLI_DEBUG_VIEW_FRAMES, 2u, &fileless);
    CLI_TEST_ASSERT_TEXT("<module>", fileless.text, fileless.text_length);
    TEST_ASSERT_EQUAL_UINT64(0ULL, fileless.second_length);
    const CliDebugPaneStatus status = status_of();
    CLI_TEST_ASSERT_TEXT("Stopped at a breakpoint, prog.py:12", status.text, status.length);
    TEST_ASSERT_EQUAL_INT(0, status.failed);
}

void test_a_frame_in_the_tree_opens_at_its_line_and_one_outside_does_not(void)
{
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    CliDebugPaneOpen open = {NULL, 0u, 0};
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_view.press, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEW_FRAMES,
                                        .at = 0u, .open_out = &open, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_STRING("src/prog.py", open.path);
    TEST_ASSERT_EQUAL_UINT(11u, open.line);
    TEST_ASSERT_EQUAL_INT(1, open.placed);
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_view.press, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEW_FRAMES,
                                        .at = 1u, .open_out = &open, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_INT(1, row_at(CLI_DEBUG_VIEW_FRAMES, 1u)->current);
    TEST_ASSERT_EQUAL_INT(0, row_at(CLI_DEBUG_VIEW_FRAMES, 0u)->current);
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_view.press, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEW_FRAMES,
                                        .at = 2u, .open_out = &open, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_view.press, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEW_FRAMES,
                                        .at = 3u, .open_out = &open, .root = CHECK_ROOT));
}

void test_the_first_scope_is_open_and_the_rest_closed(void)
{
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    TEST_ASSERT_EQUAL_UINT(4u, rows_of(CLI_DEBUG_VIEW_VARIABLES));
    CliDebugPaneRow locals;
    row_of(CLI_DEBUG_VIEW_VARIABLES, 0u, &locals);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_ROW_SCOPE, locals.type);
    CLI_TEST_ASSERT_TEXT("Locals", locals.text, locals.text_length);
    TEST_ASSERT_EQUAL_INT(1, locals.opens);
    TEST_ASSERT_EQUAL_INT(1, locals.open);
    CliDebugPaneRow count;
    row_of(CLI_DEBUG_VIEW_VARIABLES, 1u, &count);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_ROW_VARIABLE, count.type);
    TEST_ASSERT_EQUAL_UINT(1u, count.depth);
    CLI_TEST_ASSERT_TEXT("count", count.text, count.text_length);
    CLI_TEST_ASSERT_TEXT("3", count.second, count.second_length);
    CLI_TEST_ASSERT_TEXT("int", count.third, count.third_length);
    TEST_ASSERT_EQUAL_INT(0, count.opens);
    CliDebugPaneRow items;
    row_of(CLI_DEBUG_VIEW_VARIABLES, 2u, &items);
    TEST_ASSERT_EQUAL_INT(1, items.opens);
    TEST_ASSERT_EQUAL_INT(0, items.open);
    CliDebugPaneRow globals;
    row_of(CLI_DEBUG_VIEW_VARIABLES, 3u, &globals);
    CLI_TEST_ASSERT_TEXT("Globals", globals.text, globals.text_length);
    TEST_ASSERT_EQUAL_INT(0, globals.open);
}

void test_a_variable_opens_closes_and_stays_open_over_a_new_stop(void)
{
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_view.toggle, CliDebugPaneViewCfg,
                                        .view = CLI_DEBUG_VIEW_VARIABLES, .at = 2u, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_UINT(6u, rows_of(CLI_DEBUG_VIEW_VARIABLES));
    TEST_ASSERT_EQUAL_INT(1, row_at(CLI_DEBUG_VIEW_VARIABLES, 2u)->open);
    CliDebugPaneRow child;
    row_of(CLI_DEBUG_VIEW_VARIABLES, 4u, &child);
    TEST_ASSERT_EQUAL_UINT(2u, child.depth);
    CLI_TEST_ASSERT_TEXT("1", child.text, child.text_length);
    CLI_TEST_ASSERT_TEXT("2", child.second, child.second_length);
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    TEST_ASSERT_EQUAL_UINT(6u, rows_of(CLI_DEBUG_VIEW_VARIABLES));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_view.toggle, CliDebugPaneViewCfg,
                                        .view = CLI_DEBUG_VIEW_VARIABLES, .at = 2u, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_UINT(4u, rows_of(CLI_DEBUG_VIEW_VARIABLES));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_view.toggle, CliDebugPaneViewCfg,
                                        .view = CLI_DEBUG_VIEW_VARIABLES, .at = 0u, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_UINT(2u, rows_of(CLI_DEBUG_VIEW_VARIABLES));
    TEST_ASSERT_EQUAL_INT(0, row_at(CLI_DEBUG_VIEW_VARIABLES, 0u)->open);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_view.toggle, CliDebugPaneViewCfg,
                                        .view = CLI_DEBUG_VIEW_VARIABLES, .at = 1u, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_UINT(3u, rows_of(CLI_DEBUG_VIEW_VARIABLES));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_view.toggle, CliDebugPaneViewCfg,
                                        .view = CLI_DEBUG_VIEW_VARIABLES, .at = 2u, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_view.toggle, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEW_FRAMES,
                                        .at = 0u, .root = CHECK_ROOT));
}

void test_a_watch_is_evaluated_and_its_box_takes_it_out(void)
{
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    EMBED_CALL(cli_debug_pane_view.view_set, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEW_VARIABLES);
    CliDebugPaneField field;
    EMBED_CALL(cli_debug_pane_input.field, CliDebugPaneInputCfg, .field_out = &field);
    TEST_ASSERT_EQUAL_INT(1, field.displayed);
    CLI_TEST_ASSERT_TEXT("Add a watch: an expression, then Enter", field.placeholder, field.placeholder_length);
    EMBED_CALL(cli_debug_pane_input.type, CliDebugPaneInputCfg, .text = "  cou", .size = 5ULL);
    EMBED_CALL(cli_debug_pane_input.type, CliDebugPaneInputCfg, .text = "ntx ", .size = 4ULL);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_input.key, CliDebugPaneInputCfg, .name = "Backspace"));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_input.key, CliDebugPaneInputCfg, .name = "Backspace"));
    EMBED_CALL(cli_debug_pane_input.field, CliDebugPaneInputCfg, .field_out = &field);
    CLI_TEST_ASSERT_TEXT("  count", field.text, field.length);
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_input.key, CliDebugPaneInputCfg, .name = "Tab"));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_input.key, CliDebugPaneInputCfg, .name = "Enter",
                                        .root = CHECK_ROOT));
    EMBED_CALL(cli_debug_pane_input.field, CliDebugPaneInputCfg, .field_out = &field);
    TEST_ASSERT_EQUAL_UINT64(0ULL, field.length);
    TEST_ASSERT_EQUAL_UINT(5u, rows_of(CLI_DEBUG_VIEW_VARIABLES));
    CliDebugPaneRow watch;
    row_of(CLI_DEBUG_VIEW_VARIABLES, 0u, &watch);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_ROW_WATCH, watch.type);
    CLI_TEST_ASSERT_TEXT("count", watch.text, watch.text_length);
    CLI_TEST_ASSERT_TEXT("3", watch.second, watch.second_length);
    TEST_ASSERT_EQUAL_INT(0, watch.failed);
    entered("missing");
    CliDebugPaneRow refused;
    row_of(CLI_DEBUG_VIEW_VARIABLES, 1u, &refused);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_ROW_WATCH, refused.type);
    TEST_ASSERT_EQUAL_INT(1, refused.failed);
    CLI_TEST_ASSERT_TEXT("name 'missing' is not defined", refused.second, refused.second_length);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_view.toggle, CliDebugPaneViewCfg,
                                        .view = CLI_DEBUG_VIEW_VARIABLES, .at = 0u, .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_UINT(5u, rows_of(CLI_DEBUG_VIEW_VARIABLES));
    CliDebugPaneRow left;
    row_of(CLI_DEBUG_VIEW_VARIABLES, 0u, &left);
    CLI_TEST_ASSERT_TEXT("missing", left.text, left.text_length);
}

void test_the_console_runs_a_piece_on_into_its_line(void)
{
    TEST_ASSERT_EQUAL_INT(1, event_read("output", "{\"category\":\"stdout\",\"output\":\"abc\"}"));
    TEST_ASSERT_EQUAL_INT(1, event_read("output", "{\"output\":\"def\\nnext\"}"));
    TEST_ASSERT_EQUAL_INT(1, event_read("output", "{\"category\":\"stderr\",\"output\":\"oops\\n\"}"));
    TEST_ASSERT_EQUAL_INT(1, event_read("output", "{\"category\":\"telemetry\",\"output\":\"hidden\\n\"}"));
    TEST_ASSERT_EQUAL_UINT(3u, rows_of(CLI_DEBUG_VIEW_CONSOLE));
    CliDebugPaneRow first;
    row_of(CLI_DEBUG_VIEW_CONSOLE, 0u, &first);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_ROW_LINE, first.type);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_LINE_STDOUT, first.line_type);
    CLI_TEST_ASSERT_TEXT("abcdef", first.text, first.text_length);
    CliDebugPaneRow next;
    row_of(CLI_DEBUG_VIEW_CONSOLE, 1u, &next);
    CLI_TEST_ASSERT_TEXT("next", next.text, next.text_length);
    CliDebugPaneRow error;
    row_of(CLI_DEBUG_VIEW_CONSOLE, 2u, &error);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_LINE_STDERR, error.line_type);
    TEST_ASSERT_EQUAL_INT(1, error.failed);
    CLI_TEST_ASSERT_TEXT("oops", error.text, error.text_length);
}

void test_the_console_drops_its_oldest_line_past_its_max(void)
{
    VerbaLinea lines = {s_lines, sizeof(s_lines), 0ULL};
    mmgr_line_put(&lines, "{\"output\":\"");
    for (unsigned int at = 0u; at <= CLI_DEBUG_CONSOLE_LINES; at += 1u)
    {
        mmgr_line_put(&lines, "line ");
        mmgr_line_decimal(&lines, at, 1u);
        mmgr_line_put(&lines, "\\n");
    }
    mmgr_line_put(&lines, "\"}");
    TEST_ASSERT_TRUE(mmgr_line_ok(&lines));
    (void)mmgr_line_finish(&lines);
    TEST_ASSERT_EQUAL_INT(1, event_read("output", s_lines));
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_CONSOLE_LINES, rows_of(CLI_DEBUG_VIEW_CONSOLE));
    CliDebugPaneRow oldest;
    row_of(CLI_DEBUG_VIEW_CONSOLE, 0u, &oldest);
    CLI_TEST_ASSERT_TEXT("line 1", oldest.text, oldest.text_length);
    CliDebugPaneRow newest;
    row_of(CLI_DEBUG_VIEW_CONSOLE, CLI_DEBUG_CONSOLE_LINES - 1u, &newest);
    CLI_TEST_ASSERT_TEXT("line 5000", newest.text, newest.text_length);
    TEST_ASSERT_EQUAL_INT(1, event_read("output", "{\"output\":\"last\\n\"}"));
    CliDebugPaneRow moved;
    row_of(CLI_DEBUG_VIEW_CONSOLE, 0u, &moved);
    CLI_TEST_ASSERT_TEXT("line 2", moved.text, moved.text_length);
    CliDebugPaneRow last;
    row_of(CLI_DEBUG_VIEW_CONSOLE, CLI_DEBUG_CONSOLE_LINES - 1u, &last);
    CLI_TEST_ASSERT_TEXT("last", last.text, last.text_length);
}

void test_enter_in_the_console_evaluates_in_the_chosen_frame(void)
{
    EMBED_CALL(cli_debug_pane_view.view_set, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEW_CONSOLE);
    entered("x");
    TEST_ASSERT_EQUAL_UINT(2u, rows_of(CLI_DEBUG_VIEW_CONSOLE));
    CliDebugPaneRow typed;
    row_of(CLI_DEBUG_VIEW_CONSOLE, 0u, &typed);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_LINE_INPUT, typed.line_type);
    CLI_TEST_ASSERT_TEXT("> x", typed.text, typed.text_length);
    CliDebugPaneRow none;
    row_of(CLI_DEBUG_VIEW_CONSOLE, 1u, &none);
    CLI_TEST_ASSERT_TEXT("Nothing is being debugged.", none.text, none.text_length);
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    CliDebugPaneField field;
    EMBED_CALL(cli_debug_pane_input.field, CliDebugPaneInputCfg, .field_out = &field);
    CLI_TEST_ASSERT_TEXT("Evaluate in the chosen frame, then Enter", field.placeholder, field.placeholder_length);
    entered("count");
    CliDebugPaneRow result;
    row_of(CLI_DEBUG_VIEW_CONSOLE, 3u, &result);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_LINE_RESULT, result.line_type);
    CLI_TEST_ASSERT_TEXT("3", result.text, result.text_length);
    entered("missing");
    CliDebugPaneRow refused;
    row_of(CLI_DEBUG_VIEW_CONSOLE, 5u, &refused);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_LINE_STDERR, refused.line_type);
    CLI_TEST_ASSERT_TEXT("name 'missing' is not defined", refused.text, refused.text_length);
    EMBED_CALL(cli_debug_pane_view.view_set, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEW_FRAMES);
    EMBED_CALL(cli_debug_pane_input.field, CliDebugPaneInputCfg, .field_out = &field);
    TEST_ASSERT_EQUAL_INT(0, field.displayed);
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_input.key, CliDebugPaneInputCfg, .name = "Enter"));
}

void test_the_bar_flags_follow_the_session(void)
{
    TEST_ASSERT_EQUAL_INT(1, event_read("initialized", ""));
    const CliDebugPaneStatus running = status_of();
    CLI_TEST_ASSERT_TEXT("Running prog.py under debugpy", running.text, running.length);
    TEST_ASSERT_EQUAL_INT(1, item_of(CLI_DEBUG_STEP_PAUSE).enabled);
    TEST_ASSERT_EQUAL_INT(0, item_of(CLI_DEBUG_STEP_CONTINUE).enabled);
    TEST_ASSERT_EQUAL_INT(1, item_of(CLI_DEBUG_STEP_STOP).enabled);
    TEST_ASSERT_EQUAL_INT(0, item_of(CLI_DEBUG_STEP_RESTART).enabled);
    CliDebugPaneRow frames;
    row_of(CLI_DEBUG_VIEW_FRAMES, 0u, &frames);
    CLI_TEST_ASSERT_TEXT("Running.", frames.text, frames.text_length);
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    TEST_ASSERT_EQUAL_INT(0, item_of(CLI_DEBUG_STEP_PAUSE).enabled);
    TEST_ASSERT_EQUAL_INT(1, item_of(CLI_DEBUG_STEP_CONTINUE).enabled);
    TEST_ASSERT_EQUAL_INT(1, item_of(CLI_DEBUG_STEP_OVER).enabled);
    TEST_ASSERT_EQUAL_INT(1, item_of(CLI_DEBUG_STEP_INTO).enabled);
    TEST_ASSERT_EQUAL_INT(1, item_of(CLI_DEBUG_STEP_OUT).enabled);
    TEST_ASSERT_EQUAL_INT(0, item_of(CLI_DEBUG_STEP_BACK).enabled);
    TEST_ASSERT_EQUAL_INT(0, item_of(CLI_DEBUG_STEP_BACKWARD).displayed);
    s_step_back = 1;
    EMBED_CALL(cli_debug_pane_view.begin, CliDebugPaneViewCfg, .root = CHECK_ROOT, .requests = &s_requests);
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    TEST_ASSERT_EQUAL_INT(1, item_of(CLI_DEBUG_STEP_BACK).displayed);
    TEST_ASSERT_EQUAL_INT(1, item_of(CLI_DEBUG_STEP_BACKWARD).enabled);
}

void test_each_step_reaches_its_request(void)
{
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_bar.run, CliDebugPaneBarCfg, .at = CLI_DEBUG_STEP_CONTINUE,
                                        .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_STRING("continue", s_how);
    TEST_ASSERT_EQUAL_INT(1, row_at(CLI_DEBUG_VIEW_FRAMES, 0u)->type == CLI_DEBUG_ROW_NOTICE);
    TEST_ASSERT_EQUAL_INT(1, item_of(CLI_DEBUG_STEP_PAUSE).enabled);
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_bar.run, CliDebugPaneBarCfg, .at = CLI_DEBUG_STEP_PAUSE,
                                        .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_STRING("pause", s_how);
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_bar.run, CliDebugPaneBarCfg, .at = CLI_DEBUG_STEP_OVER,
                                        .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_STRING("next", s_how);
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_bar.run, CliDebugPaneBarCfg, .at = CLI_DEBUG_STEP_OUT,
                                        .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_STRING("stepOut", s_how);
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_bar.run, CliDebugPaneBarCfg, .at = CLI_DEBUG_STEP_INTO,
                                        .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_STRING("across", s_how);
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    s_refused = 1;
    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(cli_debug_pane_bar.run, CliDebugPaneBarCfg, .at = CLI_DEBUG_STEP_CONTINUE,
                                        .root = CHECK_ROOT));
    CliDebugPaneRow refused;
    row_of(CLI_DEBUG_VIEW_CONSOLE, rows_of(CLI_DEBUG_VIEW_CONSOLE) - 1u, &refused);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_LINE_STDERR, refused.line_type);
    CLI_TEST_ASSERT_TEXT("the adapter refused the step", refused.text, refused.text_length);
    TEST_ASSERT_EQUAL_UINT(3u, rows_of(CLI_DEBUG_VIEW_FRAMES));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(cli_debug_pane_bar.run, CliDebugPaneBarCfg, .at = CLI_DEBUG_STEP_STOP,
                                        .root = CHECK_ROOT));
    TEST_ASSERT_EQUAL_UINT(1u, s_stops);
    const CliDebugPaneStatus ended = status_of();
    CLI_TEST_ASSERT_TEXT("Ended", ended.text, ended.length);
    TEST_ASSERT_EQUAL_INT(0, item_of(CLI_DEBUG_STEP_STOP).enabled);
}

void test_continued_exited_and_terminated_change_the_rows_and_the_status_line(void)
{
    TEST_ASSERT_EQUAL_INT(1, stop_read());
    TEST_ASSERT_EQUAL_INT(1, event_read("continued", "{\"threadId\":7}"));
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_ROW_NOTICE, row_at(CLI_DEBUG_VIEW_FRAMES, 0u)->type);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_ROW_NOTICE, row_at(CLI_DEBUG_VIEW_VARIABLES, 0u)->type);
    const CliDebugPaneStatus running = status_of();
    CLI_TEST_ASSERT_TEXT("Running prog.py under debugpy", running.text, running.length);
    TEST_ASSERT_EQUAL_INT(1, event_read("exited", "{\"exitCode\":2}"));
    CliDebugPaneRow exited;
    row_of(CLI_DEBUG_VIEW_CONSOLE, 0u, &exited);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_LINE_CONSOLE, exited.line_type);
    CLI_TEST_ASSERT_TEXT("prog.py ended with exit code 2.", exited.text, exited.text_length);
    TEST_ASSERT_EQUAL_INT(1, event_read("terminated", "{}"));
    const CliDebugPaneStatus ended = status_of();
    CLI_TEST_ASSERT_TEXT("Ended", ended.text, ended.length);
    CliDebugPaneRow frames;
    row_of(CLI_DEBUG_VIEW_FRAMES, 0u, &frames);
    CLI_TEST_ASSERT_TEXT("Frames show here when the program stops.", frames.text, frames.text_length);
    TEST_ASSERT_EQUAL_INT(0, item_of(CLI_DEBUG_STEP_PAUSE).enabled);
}

void test_the_view_displayed_and_the_labels(void)
{
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_VIEW_FRAMES, EMBED_CALL(cli_debug_pane_view.view, CliDebugPaneViewCfg, .at = 0u));
    EMBED_CALL(cli_debug_pane_view.view_set, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEW_CONSOLE);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_VIEW_CONSOLE, EMBED_CALL(cli_debug_pane_view.view, CliDebugPaneViewCfg, .at = 0u));
    EMBED_CALL(cli_debug_pane_view.view_set, CliDebugPaneViewCfg, .view = CLI_DEBUG_VIEWS);
    TEST_ASSERT_EQUAL_UINT(CLI_DEBUG_VIEW_CONSOLE, EMBED_CALL(cli_debug_pane_view.view, CliDebugPaneViewCfg, .at = 0u));
    const char *const labels[CLI_DEBUG_VIEWS] = {"Frames", "Variables", "Console"};
    for (unsigned int view = 0u; view < CLI_DEBUG_VIEWS; view += 1u)
    {
        unsigned long long length = 0ULL;
        const char *const label =
            EMBED_CALL(cli_debug_pane_view.label, CliDebugPaneViewCfg, .view = view, .length_out = &length);
        CLI_TEST_ASSERT_TEXT(labels[view], label, length);
    }
}
