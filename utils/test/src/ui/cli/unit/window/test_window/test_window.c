// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_window.c: the window enters orior through cli_window_command_line and runs no main of orior's, so this suite makes no pool of its own, and a
// call the window makes after the command line is answered in cells of the pool the command line made; the window's place for a tree stored and
// restored, fitted to the monitors present; a terminal screen's scrollback stored on disk and read back; a page of the tree served to a window of its
// own; and a terminal's shell sent a line, its output sent to the page, then its end
#include "ui/cli/src/window/serve.h"
#include "ui/cli/src/window/serve_internal.h"
#include "ui/cli/src/window/window.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// What a page window was served: its status, its media type and its bytes
typedef struct
{
    unsigned int status;
    char media_type[64];
    char bytes[64];
    unsigned long long size;
} ViewServed;

// What the suite's terminals sent the page: the text of every `term-out`, joined, and how many `term-exit`s came, each stored under the lock as it
// comes and the waiter woken
typedef struct
{
    CliWake wake;
    char output[1u << 16u];
    unsigned long long size;
    unsigned int ended;
} TerminalOutput;

static const CliWindowArea s_main_monitor[1] = {{0, 0, 1920u, 1080u}};
static const CliWindowArea s_both_monitors[2] = {{0, 0, 1920u, 1080u}, {1920, 0, 2560u, 1440u}};

static TerminalOutput s_terminal = {CLI_WAKE_INIT, {0}, 0ull, 0u};
static char s_tree[CLI_PATH_MAX_LEN];
static char s_call_args[1u << 16u];
static int s_window_opened;
static int s_window_code;
static int s_window_word;
static int s_args_temporary;
static int s_tree_begun;
static char s_scrollback_id[32];

static int s_terminal_ran;
static int s_terminal_opened;
static int s_terminal_written;
static int s_terminal_output_seen;
static int s_terminal_ended;

// whether a call's argument tree is in the temporary tier: a cell from the tree's tier lies between two temporary cells around it only where that tier
// is the temporary one
static int args_read(const CliServeAnswerCfg *args)
{
    char *const below = g_cli_cells->temporary_buf_alloc(16u, &g_checks_cells);
    s_args_temporary = (const char *)args->request->json->node > below;
    g_cli_cells->persistent_buf_release(below, &g_checks_cells);
    mmgr_line_put(args->out, "null");
    return 1;
}

// the suite's tree begun, once for the process
static void tree_begun(void)
{
    if (!s_tree_begun)
    {
        s_tree_begun = 1;
        cli_unity_folder("window_place", s_tree);
        EMBED_CALL(cli_serve_root_table.begin, CliServeRootCfg, .root = s_tree, .detached_dir = NULL);
    }
}

// the place restored on the monitors, asserted to stand at `x`, `y`, `width` by `height`, filling its monitor where `maximized`
static void assert_place(const CliWindowArea *areas, unsigned int count, int x, int y, unsigned int width, unsigned int height, int maximized)
{
    CliWindowPlace place = {0, 0, 0u, 0u, 0};
    TEST_ASSERT_TRUE(cli_window_place_kept(areas, count, &place));
    TEST_ASSERT_EQUAL_INT(x, place.x);
    TEST_ASSERT_EQUAL_INT(y, place.y);
    TEST_ASSERT_EQUAL_UINT(width, place.width);
    TEST_ASSERT_EQUAL_UINT(height, place.height);
    TEST_ASSERT_EQUAL_INT(maximized, place.maximized);
}

// the window's own call `name` with the JSON `args` answered, its answer, or why it failed, into `answer`, sized for `room`: 1 where it answered
static int window_call(const char *name, const char *args, char *answer, unsigned long long room)
{
    const unsigned int called = cli_window_called(1, name, args, CLI_UNITY_LENGTH(args));
    const char *text = NULL;
    unsigned long long size = 0ull;
    const int answered = cli_window_answer(called, &text, &size);
    VerbaLinea copied = {answer, room, 0ull};
    mmgr_line_put_n(&copied, text, size);
    (void)mmgr_line_finish(&copied);
    cli_window_given(called);
    return answered;
}

// the lines `first` up to `last` of the suite's scrollback written as JSON strings
static void lines_written(VerbaLinea *out, unsigned int first, unsigned int last)
{
    for (unsigned int line = first; line < last; line += 1u)
    {
        mmgr_line_put(out, (line > first) ? ",\"<span>line " : "\"<span>line ");
        mmgr_line_decimal(out, line, 1u);
        mmgr_line_put(out, "</span>\"");
    }
}

// a fresh scrollback's id in s_scrollback_id
static void scrollback_opened(void)
{
    char answer[256];
    TEST_ASSERT_TRUE(window_call("scrollback_reset", "{}", answer, sizeof(answer)));
    TEST_ASSERT_TRUE(window_call("scrollback_open", "{}", answer, sizeof(answer)));
    VerbaLinea named = {s_scrollback_id, sizeof(s_scrollback_id), 0ull};
    mmgr_line_put(&named, answer);
    (void)mmgr_line_finish(&named);
}

// the scrollback's arguments `tail` written into s_call_args
static const char *scrollback_args(const char *tail)
{
    VerbaLinea args = {s_call_args, sizeof(s_call_args), 0ull};
    mmgr_line_put(&args, "{\"id\":");
    mmgr_line_put(&args, s_scrollback_id);
    mmgr_line_put(&args, tail);
    (void)mmgr_line_finish(&args);
    return s_call_args;
}

// the lines `first` up to `last` written to the scrollback, asserted to answer how many it now stores
static void lines_stored(unsigned int first, unsigned int last, const char *count)
{
    char answer[256];
    VerbaLinea args = {s_call_args, sizeof(s_call_args), 0ull};
    mmgr_line_put(&args, "{\"id\":");
    mmgr_line_put(&args, s_scrollback_id);
    mmgr_line_put(&args, ",\"lines\":[");
    lines_written(&args, first, last);
    mmgr_line_put(&args, "]}");
    (void)mmgr_line_finish(&args);
    TEST_ASSERT_TRUE(window_call("scrollback_keep", s_call_args, answer, sizeof(answer)));
    TEST_ASSERT_EQUAL_STRING(count, answer);
}

// the scrollback read with `tail` asserted to return the lines `first` up to `last`
static void assert_lines_read(const char *tail, unsigned int first, unsigned int last)
{
    static char s_answer[1u << 16u];
    static char s_expected[1u << 16u];
    VerbaLinea expected = {s_expected, sizeof(s_expected), 0ull};
    mmgr_line_ch(&expected, '[');
    lines_written(&expected, first, last);
    mmgr_line_ch(&expected, ']');
    (void)mmgr_line_finish(&expected);
    TEST_ASSERT_TRUE(window_call("scrollback_read", scrollback_args(tail), s_answer, sizeof(s_answer)));
    TEST_ASSERT_EQUAL_STRING(s_expected, s_answer);
}

// a file a page window requests stored as it was served
static void view_recorded(void *context, unsigned int status, const char *media_type, const unsigned char *bytes, unsigned long long size)
{
    ViewServed *const served = context;
    served->status = status;
    VerbaLinea type = {served->media_type, sizeof(served->media_type), 0ull};
    mmgr_line_put(&type, media_type);
    (void)mmgr_line_finish(&type);
    VerbaLinea body = {served->bytes, sizeof(served->bytes), 0ull};
    mmgr_line_put_n(&body, (const char *)bytes, size);
    (void)mmgr_line_finish(&body);
    served->size = size;
}

// a page of the tree written at views/3
static void page_written(void)
{
    char folder[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    tree_begun();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = "views", .joined = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_tree, .relative = "views/3", .joined = folder);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "a view#1.html", .joined = file);
    TEST_ASSERT_TRUE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file, .bytes = "<p>page</p>", .size = 11ull));
}

// an event the window sends the page stored where it is a terminal's: of a `term-out`, the text between its `"text":"` and the `"}` that ends it
static void event_recorded(void *context, const char *event, const char *body, unsigned long long size)
{
    static const char s_text[] = "\"text\":\"";
    (void)context;
    if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "term-out", .cap = 64u}))
    {
        const char *const text = cellul.find(&(CatenaFinitaCfg){.src = body, .cap = size, .other = s_text, .other_cap = sizeof(s_text)});
        // a find inside the body, at or past its start
        const unsigned long long from = (text != NULL) ? (unsigned long long)(text - body) + sizeof(s_text) - 1u : size;
        const unsigned long long to = (size >= 2ull) ? size - 2ull : 0ull;
        VerbaLinea output = {s_terminal.output, sizeof(s_terminal.output) - 1u, s_terminal.size};
        mmgr_line_put_n(&output, body + from, (to > from) ? to - from : 0ull);
        s_terminal.size = output.at;
        s_terminal.output[output.at] = '\0';
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = event, .other = "term-exit", .cap = 64u}))
    {
        s_terminal.ended += 1u;
    }
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_terminal.wake);
}

// whether the terminals' output contains `wanted` within `milliseconds`, woken as each event comes
static int output_contains(const char *wanted, unsigned long long milliseconds)
{
    unsigned long long waited = 0ull;
    int found = 0;
    while (!found && (waited < milliseconds))
    {
        found = cellul.find(&(CatenaFinitaCfg){.src = s_terminal.output, .cap = sizeof(s_terminal.output), .other = wanted, .other_cap = 64u}) != NULL;
        if (!found)
        {
            (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_terminal.wake, .milliseconds_u64 = 250ull);
            waited += 250ull;
        }
    }
    return found;
}

// whether `count` terminals have ended within `milliseconds`, woken as each event comes
static int terminals_ended(unsigned int count, unsigned long long milliseconds)
{
    unsigned long long waited = 0ull;
    while ((s_terminal.ended < count) && (waited < milliseconds))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_terminal.wake, .milliseconds_u64 = 250ull);
        waited += 250ull;
    }
    const int ended = s_terminal.ended >= count;
    return ended;
}

// a terminal opened on the reader's shell, sent a line whose output the line itself does not contain, then told to exit, recorded once for the process
static void terminal_ran(void)
{
    char id[32];
    char args[256];
    char answer[256];
    if (s_terminal_ran)
    {
        return;
    }
    s_terminal_ran = 1;
    EMBED_CALL(cli_serve_emit.emit_set, CliServeEmitCfg, .emit = event_recorded, .context = NULL);
    s_terminal_opened = window_call("term_open", "{\"cols\":80,\"rows\":24}", id, sizeof(id)) && (id[0] >= '1') && (id[0] <= '9');
    if (!s_terminal_opened)
    {
        return;
    }
    VerbaLinea line = {args, sizeof(args), 0ull};
    mmgr_line_put(&line, "{\"id\":");
    mmgr_line_put(&line, id);
    mmgr_line_put(&line, ",\"text\":\"echo orior_$((6*7))_seen\\r\"}");
    (void)mmgr_line_finish(&line);
    s_terminal_written = window_call("term_write", args, answer, sizeof(answer));
    s_terminal_output_seen = output_contains("orior_42_seen", 30000ull);
    line.at = 0ull;
    mmgr_line_put(&line, "{\"id\":");
    mmgr_line_put(&line, id);
    mmgr_line_put(&line, ",\"text\":\"exit\\r\"}");
    (void)mmgr_line_finish(&line);
    (void)window_call("term_write", args, answer, sizeof(answer));
    s_terminal_ended = terminals_ended(1u, 15000ull);
}

void setUp(void)
{
    char home[CLI_PATH_MAX_LEN];
    if (!s_window_opened)
    {
        s_window_opened = 1;
        cli_unity_folder("window_home", home);
        (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = "ORIOR_HOME", .value = home);
        s_window_code = cli_window_command_line(&s_window_word);
    }
}

void tearDown(void)
{
}

void test_no_words_open_the_window(void)
{
    TEST_ASSERT_EQUAL_INT(0, s_window_code);
    TEST_ASSERT_EQUAL_INT(1, s_window_word);
}

void test_the_windows_first_call_is_answered_in_cells_of_the_pool(void)
{
    const unsigned int room = cli_window_called(1, "plugins_read", "{}", 2ull);
    const char *text = NULL;
    unsigned long long size = 0ull;
    const int answered = cli_window_answer(room, &text, &size);
    cli_window_given(room);
    TEST_ASSERT_TRUE(answered);
    TEST_ASSERT_NOT_EQUAL(0ull, size);
}

void test_a_calls_argument_tree_is_in_the_temporary_tier(void)
{
    static const char s_args[] = "{\"key\":\"a call's own\"}";
    char answer[16];
    char why[256];
    VerbaLinea out = {answer, sizeof(answer), 0ull};
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_serve_size_internal.answer, CliServeSizeCtx, .answer = args_read, .args_text = s_args, .size = sizeof(s_args) - 1u, .out = &out, .err_msg = &err_msg));
    TEST_ASSERT_TRUE(s_args_temporary);
}

void test_a_tree_with_no_place_stored_has_none(void)
{
    CliWindowPlace place = {0, 0, 0u, 0u, 0};
    tree_begun();
    TEST_ASSERT_FALSE(cli_window_place_kept(s_both_monitors, 2u, &place));
}

void test_a_place_on_a_monitor_present_is_restored(void)
{
    tree_begun();
    cli_window_place_keep(&(CliWindowPlace){2000, 100, 1440u, 920u, 1});
    assert_place(s_both_monitors, 2u, 2000, 100, 1440u, 920u, 1);
}

void test_a_place_on_a_monitor_gone_moves_to_the_middle_of_the_main_one(void)
{
    tree_begun();
    cli_window_place_keep(&(CliWindowPlace){2000, 100, 1440u, 920u, 1});
    assert_place(s_main_monitor, 1u, 240, 80, 1440u, 920u, 1);
}

void test_with_no_monitor_there_is_no_place(void)
{
    CliWindowPlace place = {0, 0, 0u, 0u, 0};
    tree_begun();
    cli_window_place_keep(&(CliWindowPlace){2000, 100, 1440u, 920u, 1});
    TEST_ASSERT_FALSE(cli_window_place_kept(s_main_monitor, 0u, &place));
}

void test_a_place_larger_than_its_monitor_is_cut_to_it_and_moved_in(void)
{
    tree_begun();
    cli_window_place_keep(&(CliWindowPlace){-40, 30, 2400u, 1300u, 0});
    assert_place(s_both_monitors, 2u, 0, 0, 1920u, 1080u, 0);
}

void test_a_place_hanging_off_its_monitor_moves_wholly_onto_it(void)
{
    tree_begun();
    cli_window_place_keep(&(CliWindowPlace){1200, 600, 1440u, 920u, 0});
    assert_place(s_main_monitor, 1u, 480, 160, 1440u, 920u, 0);
}

void test_a_place_narrower_than_the_least_width_is_none(void)
{
    CliWindowPlace place = {0, 0, 0u, 0u, 0};
    tree_begun();
    cli_window_place_keep(&(CliWindowPlace){0, 0, 305u, 1392u, 0});
    TEST_ASSERT_FALSE(cli_window_place_kept(s_main_monitor, 1u, &place));
}

void test_a_place_lower_than_the_least_height_is_none(void)
{
    CliWindowPlace place = {0, 0, 0u, 0u, 0};
    tree_begun();
    cli_window_place_keep(&(CliWindowPlace){0, 0, 1440u, 300u, 0});
    TEST_ASSERT_FALSE(cli_window_place_kept(s_main_monitor, 1u, &place));
}

void test_a_place_of_the_least_size_is_restored(void)
{
    tree_begun();
    cli_window_place_keep(&(CliWindowPlace){0, 0, CLI_PLACE_LEAST_WIDTH, CLI_PLACE_LEAST_HEIGHT, 0});
    assert_place(s_main_monitor, 1u, 0, 0, CLI_PLACE_LEAST_WIDTH, CLI_PLACE_LEAST_HEIGHT, 0);
}

void test_a_write_answers_how_many_lines_the_scrollback_stores(void)
{
    scrollback_opened();
    lines_stored(0u, 600u, "600");
    lines_stored(600u, 1000u, "1000");
}

void test_lines_are_read_back_across_their_stored_parts(void)
{
    scrollback_opened();
    lines_stored(0u, 600u, "600");
    lines_stored(600u, 1000u, "1000");
    assert_lines_read(",\"from\":595,\"count\":10}", 595u, 605u);
}

void test_lines_read_past_the_end_stop_at_it(void)
{
    scrollback_opened();
    lines_stored(0u, 600u, "600");
    lines_stored(600u, 1000u, "1000");
    assert_lines_read(",\"from\":990,\"count\":50}", 990u, 1000u);
    assert_lines_read(",\"from\":1000,\"count\":5}", 1000u, 1000u);
}

void test_a_newline_in_a_line_reads_back_as_a_space_and_a_two_byte_letter_whole(void)
{
    char answer[256];
    scrollback_opened();
    (void)window_call("scrollback_keep", scrollback_args(",\"lines\":[\"a\\nb\",\"\\u00e9\"]}"), answer, sizeof(answer));
    TEST_ASSERT_TRUE(window_call("scrollback_read", scrollback_args(",\"from\":0,\"count\":2}"), answer, sizeof(answer)));
    TEST_ASSERT_EQUAL_STRING("[\"a b\",\"\xc3\xa9\"]", answer);
}

void test_a_closed_scrollback_reads_nothing(void)
{
    char answer[256];
    scrollback_opened();
    lines_stored(0u, 2u, "2");
    (void)window_call("scrollback_close", scrollback_args("}"), answer, sizeof(answer));
    TEST_ASSERT_FALSE(window_call("scrollback_read", scrollback_args(",\"from\":0,\"count\":1}"), answer, sizeof(answer)));
}

void test_a_page_of_the_tree_has_an_address_its_space_and_mark_escaped(void)
{
    char answer[512];
    page_written();
    TEST_ASSERT_TRUE(window_call("view_window", "{\"path\":\"views/3/a view#1.html\"}", answer, sizeof(answer)));
    CLI_TEST_ASSERT_CONTAINS("views/3/a%20view%231.html\"", answer);
}

void test_a_page_is_served_from_its_address_as_html(void)
{
    static const char s_path[] = "/views/3/a%20view%231.html";
    char answer[512];
    ViewServed served;
    page_written();
    (void)window_call("view_window", "{\"path\":\"views/3/a view#1.html\"}", answer, sizeof(answer));
    cli_window_view_served(s_path, sizeof(s_path) - 1u, view_recorded, &served);
    TEST_ASSERT_EQUAL_UINT(200u, served.status);
    TEST_ASSERT_EQUAL_UINT64(11ull, served.size);
    TEST_ASSERT_EQUAL_STRING("<p>page</p>", served.bytes);
    TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = served.media_type, .other = "text/html", .cap = 64u}));
}

void test_a_file_outside_the_tree_is_refused(void)
{
    static const char s_outside[] = "/../outside.html";
    ViewServed served;
    tree_begun();
    cli_window_view_served(s_outside, sizeof(s_outside) - 1u, view_recorded, &served);
    TEST_ASSERT_EQUAL_UINT(403u, served.status);
}

void test_a_page_that_is_no_file_has_no_window(void)
{
    char answer[512];
    tree_begun();
    TEST_ASSERT_FALSE(window_call("view_window", "{\"path\":\"views/3/none.html\"}", answer, sizeof(answer)));
}

void test_a_terminal_opens_and_is_numbered(void)
{
    terminal_ran();
    TEST_ASSERT_TRUE(s_terminal_opened);
}

void test_a_line_is_written_to_the_terminals_shell(void)
{
    terminal_ran();
    TEST_ASSERT_TRUE(s_terminal_written);
}

void test_the_shells_output_is_sent_to_the_page(void)
{
    terminal_ran();
    TEST_ASSERT_TRUE(s_terminal_output_seen);
}

void test_the_shells_end_is_sent_to_the_page(void)
{
    terminal_ran();
    TEST_ASSERT_TRUE(s_terminal_ended);
}
