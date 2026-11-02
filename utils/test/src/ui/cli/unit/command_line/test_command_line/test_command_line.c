// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_command_line.c: the help's lines wrapped preserving every word and the width, the help staying within its width and naming every command, a run
// a terminal of the window's starts reported on a channel of its own, a job named by its id, the tail of its id or its title, and the values a run's
// words set on a job
#include "ui/cli/src/command_line/command_line_internal.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/command_line/commands.h"
#include "ui/cli/src/comms/comms.h"
#include "ui/cli/src/comms/door.h"
#include "ui/cli/src/runs/runner.h"
#include "ui/cli/src/tests/test_runs.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// the most lines and messages a case records
#define RECORDED_LINES 64u
#define RECORDED_MESSAGES 64u

// The lines a wrapping wrote
typedef struct
{
    char line[RECORDED_LINES][256];
    unsigned int count;
} WrappedLines;

// What the window received of a run, under its lock: each message's channel, speaker and body, and whether the end came
typedef struct
{
    CliWake wake;
    char channel[RECORDED_MESSAGES][CLI_NAME_MAX_LEN];
    char from[RECORDED_MESSAGES][CLI_NAME_MAX_LEN];
    char body[RECORDED_MESSAGES][4096];
    unsigned int count;
    int ended;
} RunMessages;

static const char s_usage[] = "orior_cli file new-plugin <name> --ext <ext,...> [--from <plugin>] [--line-comment <text>] "
                              "[--keywords <a,b>]";

static WrappedLines s_lines;
static RunMessages s_messages = {CLI_WAKE_INIT};
static char s_help[1u << 18u];
static CliItem s_items[CLI_ITEMS_MOST];
static CliJob s_jobs[8];
static CliParam s_params[8];
static CliStep s_steps[8];
static CliArg s_args[8];
static unsigned int s_lists[32];
static char s_text[8192];
static CliCatalog s_catalog;
static CliValues s_values;
static CliWords s_words;
static char s_why[1024];
static unsigned int s_values_job;

static int s_channel_ran;
static int s_channel_python;
static int s_channel_joined;
static int s_channel_code;

// a line of a wrapping recorded
static void line_recorded(const CliEachLineSizeCfg *args)
{
    WrappedLines *const lines = args->context;
    if ((lines->count < RECORDED_LINES) && (args->size < 256ull))
    {
        memor.cpy(&(MemoriaCfg){.dst = lines->line[lines->count], .src = args->line, .bytes = args->size});
        lines->line[lines->count][args->size] = '\0';
        lines->count += 1u;
    }
}

// the help written into s_help, the menus read first
static void help_written(void)
{
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_commands.read, CliCommandsCfg, .err_msg = &err_msg));
    VerbaLinea help = {s_help, sizeof(s_help) - 1u, 0ull};
    EMBED_CALL(cli_command_line_out_err_msg_shell_internal.help, CliCommandLineOutErrMsgShellCtx, .out = &help);
    s_help[help.at] = '\0';
}

// a message the window received recorded
static int message_recorded(const CliCommsRecvCfg *args)
{
    if ((s_messages.count < RECORDED_MESSAGES) && (args->size < 4096ull))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_messages.channel[s_messages.count], .src = args->channel, .bytes = CLI_UNITY_LENGTH(args->channel) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = s_messages.from[s_messages.count], .src = args->from, .bytes = CLI_UNITY_LENGTH(args->from) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = s_messages.body[s_messages.count], .src = args->body, .bytes = args->size});
        s_messages.body[s_messages.count][args->size] = '\0';
        s_messages.ended |= cellul.find(&(CatenaFinitaCfg){.src = s_messages.body[s_messages.count], .cap = CLI_CELLS_POOL, .other = "\"state\":\"ended\"", .other_cap = CLI_CELLS_POOL}) != NULL;
        s_messages.count += 1u;
    }
    EMBED_CALL(cli_wake_internal.wake_all, CliWakeCtx, .wake = &s_messages.wake);
    return 1;
}

// a door opened, a window and a terminal registered on it, and a job run as the terminal, its messages recorded, once for the process
static void channel_ran(void)
{
    static const char *const s_all[1] = {"*"};
    static const char *const s_runs[1] = {"runs/*"};
    static CliJob s_run_jobs[4];
    static CliParam s_run_params[4];
    static CliStep s_run_steps[4];
    static CliArg s_run_args[4];
    static unsigned int s_run_lists[16];
    static char s_run_text[4096];
    static CliCatalog s_run_catalog;
    char python[CLI_PATH_MAX_LEN];
    char why[1024];
    char address[CLI_PATH_MAX_LEN];
    char window[CLI_COMMS_KEY];
    char terminal[CLI_COMMS_KEY];
    char root[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    unsigned int listener = 0u;
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (s_channel_ran)
    {
        return;
    }
    s_channel_ran = 1;
    s_channel_python = EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python);
    if (!s_channel_python)
    {
        return;
    }
    s_channel_joined = EMBED_CALL(cli_door_address.open, CliDoorAddressCfg, .address = address, .err_msg = &err_msg) && EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = "window", .has_process = 0, .process = 0u, .listen = s_all, .listens = 1u, .send_grant = NULL, .send_grants = 0u, .key_out = window, .err_msg = &err_msg) && EMBED_CALL(cli_comms_channel.listen, CliCommsChannelCfg, .key = window, .channel = "runs/*", .queued = 0, .recv = message_recorded, .context = NULL, .listener = &listener, .err_msg = &err_msg) && EMBED_CALL(cli_comms_name.comms_register, CliCommsNameCfg, .name = "terminal", .has_process = 0, .process = 0u, .listen = s_runs, .listens = 1u, .send_grant = s_runs, .send_grants = 1u, .key_out = terminal, .err_msg = &err_msg);
    if (!s_channel_joined)
    {
        return;
    }
    EMBED_CALL(cli_comms_process.bind, CliCommsProcessCfg, .key = terminal, .process = cli_process_own());
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = CLI_DOOR_ADDRESS_VARIABLE, .value = address);
    (void)EMBED_CALL(query_name_internal.environment_set, QueryNameCtx, .name = CLI_DOOR_KEY_VARIABLE, .value = terminal);
    cli_unity_folder("channel", root);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = "print.py", .joined = script);
    (void)EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = script, .bytes = "print('done')\n", .size = 14ull);
    EMBED_CALL(cli_catalog_jobs_most.begin, CliCatalogJobsMostCfg, .catalog = &s_run_catalog, .jobs = s_run_jobs, .jobs_most = 4u, .params = s_run_params, .params_most = 4u, .steps = s_run_steps, .steps_most = 4u, .request = s_run_args, .args_most = 4u, .lists = s_run_lists, .lists_most = 16u, .text_out = s_run_text, .text_most = 4096u);
    const unsigned int job = EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_run_catalog, .id = "test/print", .group = "test", .title = "print", .file = "print.py", .about = "", .opens = "");
    EMBED_CALL(cli_catalog_text_program.step, CliCatalogTextProgramCfg, .catalog = &s_run_catalog, .program = CLI_PROGRAM_PYTHON, .text = "print.py", .tool_program = NULL, .system = 0);
    s_channel_code = EMBED_CALL(cli_command_line_root_internal.job_run, CliCommandLineRootCtx, .root = root, .catalog = &s_run_catalog, .job = job, .words = NULL, .count = 0u);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (!s_messages.ended && (cli_clock_milliseconds() < until))
    {
        (void)EMBED_CALL(cli_wake_internal.wake_wait_for, CliWakeCtx, .wake = &s_messages.wake, .milliseconds_u64 = 100ull);
    }
    EMBED_CALL(cli_comms_listener.unlisten, CliCommsListenerCfg, .listener_value = listener);
}

// the run made and asserted to have joined, the case ignored with no Python
static void channel_ready(void)
{
    channel_ran();
    if (!s_channel_python)
    {
        TEST_IGNORE_MESSAGE("no Python on this machine");
    }
    TEST_ASSERT_TRUE(s_channel_joined);
}

// a catalog of three jobs, two of one title
static void jobs_named(void)
{
    EMBED_CALL(cli_catalog_jobs_most.begin, CliCatalogJobsMostCfg, .catalog = &s_catalog, .jobs = s_jobs, .jobs_most = 8u, .params = s_params, .params_most = 8u, .steps = s_steps, .steps_most = 8u, .request = s_args, .args_most = 8u, .lists = s_lists, .lists_most = 32u, .text_out = s_text, .text_most = 8192u);
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_catalog, .id = "build/src/build_engine.sh", .group = "test", .title = "src/build_engine.sh", .file = "", .about = "", .opens = "");
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_catalog, .id = "sim/wave", .group = "test", .title = "wave", .file = "", .about = "", .opens = "");
    (void)EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_catalog, .id = "view/a/build_wave_view.py", .group = "test", .title = "wave", .file = "", .about = "", .opens = "");
}

// a job of three parameters: a choice, a many and free words
static void job_with_values(void)
{
    EMBED_CALL(cli_catalog_jobs_most.begin, CliCatalogJobsMostCfg, .catalog = &s_catalog, .jobs = s_jobs, .jobs_most = 8u, .params = s_params, .params_most = 8u, .steps = s_steps, .steps_most = 8u, .request = s_args, .args_most = 8u, .lists = s_lists, .lists_most = 32u, .text_out = s_text, .text_most = 8192u);
    s_values_job = EMBED_CALL(cli_catalog_id_group.job, CliCatalogIdGroupCfg, .catalog = &s_catalog, .id = "run/x", .group = "test", .title = "x", .file = "", .about = "", .opens = "");
    EMBED_CALL(cli_catalog_key_kind.param, CliCatalogKeyKindCfg, .catalog = &s_catalog, .key = "cfg", .kind = "choice", .required = 0);
    EMBED_CALL(cli_catalog_choice_table.choice, CliCatalogChoiceCfg, .catalog = &s_catalog, .choice = "a.cfg");
    EMBED_CALL(cli_catalog_key_kind.param, CliCatalogKeyKindCfg, .catalog = &s_catalog, .key = "run", .kind = "many", .required = 0);
    EMBED_CALL(cli_catalog_key_kind.param, CliCatalogKeyKindCfg, .catalog = &s_catalog, .key = "arguments", .kind = "words", .required = 0);
}

// the words of a run read as values of the job
static int values_read(const char *const *words, unsigned int count)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    return EMBED_CALL(cli_command_line_catalog_internal.job_values, CliCommandLineCatalogCtx, .catalog = &s_catalog, .job = s_values_job, .words = words, .count = count, .values = &s_values, .err_msg = &err_msg);
}

// the values of `key`, joined by a bar, in a static text
static const char *values_joined(const char *key)
{
    static char s_joined[1024];
    VerbaLinea line = {s_joined, sizeof(s_joined), 0ull};
    unsigned int count = 0u;
    const unsigned int *const items = EMBED_CALL(cli_catalog_key.values_get, CliCatalogKeyCfg, .values = &s_values, .key = key, .count = &count);
    for (unsigned int at = 0u; (items != NULL) && (at < count); at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, EMBED_CALL(cli_catalog_text_values.values_at, CliCatalogTextValuesCfg, .values = &s_values, .text_u32 = items[at]));
    }
    (void)mmgr_line_finish(&line);
    return s_joined;
}

void setUp(void)
{
    cli_unity_begun();
    s_lines.count = 0u;
}

void tearDown(void)
{
}

void test_a_long_usage_wraps_onto_more_than_one_line(void)
{
    EMBED_CALL(cli_command_line_text_each_internal.help_wrap, CliCommandLineTextEachCtx, .text = s_usage, .width = 40ull, .indent = 2ull, .each = line_recorded, .context = &s_lines);
    TEST_ASSERT_GREATER_THAN_UINT(1u, s_lines.count);
}

void test_every_wrapped_line_is_40_letters_at_most(void)
{
    EMBED_CALL(cli_command_line_text_each_internal.help_wrap, CliCommandLineTextEachCtx, .text = s_usage, .width = 40ull, .indent = 2ull, .each = line_recorded, .context = &s_lines);
    for (unsigned int at = 0u; at < s_lines.count; at += 1u)
    {
        TEST_ASSERT_LESS_OR_EQUAL_UINT64(40ull, EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = s_lines.line[at], .size = CLI_UNITY_LENGTH(s_lines.line[at])));
    }
}

void test_every_line_after_the_first_is_indented_two_spaces(void)
{
    EMBED_CALL(cli_command_line_text_each_internal.help_wrap, CliCommandLineTextEachCtx, .text = s_usage, .width = 40ull, .indent = 2ull, .each = line_recorded, .context = &s_lines);
    for (unsigned int at = 1u; at < s_lines.count; at += 1u)
    {
        TEST_ASSERT_TRUE(cellul.starts(&(CatenaFinitaCfg){.src = s_lines.line[at], .other = "  ", .cap = CLI_CELLS_POOL}));
        TEST_ASSERT_FALSE(cellul.starts(&(CatenaFinitaCfg){.src = s_lines.line[at], .other = "   ", .cap = CLI_CELLS_POOL}));
    }
}

void test_the_lines_trimmed_and_joined_restore_the_text(void)
{
    char joined[512];
    VerbaLinea whole = {joined, sizeof(joined), 0ull};
    EMBED_CALL(cli_command_line_text_each_internal.help_wrap, CliCommandLineTextEachCtx, .text = s_usage, .width = 40ull, .indent = 2ull, .each = line_recorded, .context = &s_lines);
    for (unsigned int at = 0u; at < s_lines.count; at += 1u)
    {
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = s_lines.line[at], .size = CLI_UNITY_LENGTH(s_lines.line[at]), .from_u64s = &from, .to_u64s = &to);
        mmgr_line_put(&whole, (at > 0u) ? " " : "");
        mmgr_line_put_n(&whole, s_lines.line[at] + from, to - from);
    }
    CLI_TEST_ASSERT_TEXT(s_usage, joined, whole.at);
}

void test_a_short_text_is_one_line(void)
{
    EMBED_CALL(cli_command_line_text_each_internal.help_wrap, CliCommandLineTextEachCtx, .text = "short", .width = 40ull, .indent = 2ull, .each = line_recorded, .context = &s_lines);
    TEST_ASSERT_EQUAL_UINT(1u, s_lines.count);
    TEST_ASSERT_EQUAL_STRING("short", s_lines.line[0]);
}

void test_every_line_of_the_help_is_within_its_width(void)
{
    help_written();
    for (const char *line = s_help; *line != '\0';)
    {
        const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'});
        // a line end inside the help, at or past the line's start
        const unsigned long long size = (end != NULL) ? (unsigned long long)(end - line) : CLI_UNITY_LENGTH(line);
        TEST_ASSERT_LESS_OR_EQUAL_UINT64(CLI_HELP_WIDTH, EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = line, .size = size));
        line += size + ((end != NULL) ? 1u : 0u);
    }
}

void test_the_help_names_every_command_of_every_menu(void)
{
    help_written();
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        char word[CLI_LABEL_MAX_LEN];
        EMBED_CALL(cli_commands_menu_word.menu_word, CliCommandsMenuWordCfg, .menu = menu, .word_out = word);
        const unsigned int items = EMBED_CALL(cli_commands_menu_room.menu_items, CliCommandsMenuRoomCfg, .menu = menu, .items = s_items, .room = CLI_ITEMS_MOST);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            char usage[CLI_LABEL_MAX_LEN * 2u + 16u];
            VerbaLinea line = {usage, sizeof(usage), 0ull};
            mmgr_line_put(&line, "orior_cli ");
            mmgr_line_put(&line, word);
            mmgr_line_ch(&line, ' ');
            mmgr_line_put(&line, s_items[at].command);
            (void)mmgr_line_finish(&line);
            CLI_TEST_ASSERT_CONTAINS(usage, s_help);
        }
    }
}

void test_a_run_at_the_terminal_exits_0(void)
{
    channel_ready();
    TEST_ASSERT_EQUAL_INT(0, s_channel_code);
}

void test_each_message_is_reported_on_the_runs_own_channel_by_the_terminal(void)
{
    char channel[CLI_NAME_MAX_LEN];
    VerbaLinea own = {channel, sizeof(channel), 0ull};
    channel_ready();
    mmgr_line_put(&own, "runs/");
    mmgr_line_decimal(&own, cli_process_own(), 1u);
    (void)mmgr_line_finish(&own);
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_messages.count);
    for (unsigned int at = 0u; at < s_messages.count; at += 1u)
    {
        TEST_ASSERT_EQUAL_STRING(channel, s_messages.channel[at]);
        TEST_ASSERT_EQUAL_STRING("terminal", s_messages.from[at]);
    }
}

void test_the_first_message_reports_the_run_started_and_its_job(void)
{
    channel_ready();
    TEST_ASSERT_GREATER_THAN_UINT(0u, s_messages.count);
    CLI_TEST_ASSERT_CONTAINS("\"state\":\"started\"", s_messages.body[0]);
    CLI_TEST_ASSERT_CONTAINS("\"job\":\"test/print\"", s_messages.body[0]);
}

void test_the_line_the_run_printed_is_reported_on_stdout(void)
{
    int has_line = 0;
    channel_ready();
    for (unsigned int at = 0u; at < s_messages.count; at += 1u)
    {
        const int names_line = cellul.find(&(CatenaFinitaCfg){.src = s_messages.body[at], .cap = CLI_CELLS_POOL, .other = "\"line\":\"done\"", .other_cap = CLI_CELLS_POOL}) != NULL;
        const int names_stream = cellul.find(&(CatenaFinitaCfg){.src = s_messages.body[at], .cap = CLI_CELLS_POOL, .other = "\"stream\":\"stdout\"", .other_cap = CLI_CELLS_POOL}) != NULL;
        has_line |= names_line && names_stream;
    }
    TEST_ASSERT_TRUE(has_line);
}

void test_the_last_message_reports_the_run_ended_with_0(void)
{
    channel_ready();
    TEST_ASSERT_TRUE(s_messages.ended);
    CLI_TEST_ASSERT_CONTAINS("\"code\":0", s_messages.body[s_messages.count - 1u]);
}

void test_a_name_ending_a_jobs_id_names_it(void)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    jobs_named();
    const unsigned int job = EMBED_CALL(cli_command_line_catalog_among_internal.job_by_name, CliCommandLineCatalogAmongCtx, .catalog = &s_catalog, .among = NULL, .among_count = 0u, .name = "build_engine.sh", .err_msg = &err_msg);
    TEST_ASSERT_NOT_EQUAL(CLI_CATALOG_NONE, job);
    TEST_ASSERT_EQUAL_STRING("build/src/build_engine.sh", EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.job[job].id));
}

void test_a_jobs_id_names_it(void)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    jobs_named();
    const unsigned int job = EMBED_CALL(cli_command_line_catalog_among_internal.job_by_name, CliCommandLineCatalogAmongCtx, .catalog = &s_catalog, .among = NULL, .among_count = 0u, .name = "sim/wave", .err_msg = &err_msg);
    TEST_ASSERT_NOT_EQUAL(CLI_CATALOG_NONE, job);
    TEST_ASSERT_EQUAL_STRING("sim/wave", EMBED_CALL(cli_catalog_text_table.at, CliCatalogTextCfg, .catalog_read = &s_catalog, .text_u32 = s_catalog.job[job].id));
}

void test_a_title_of_two_jobs_is_refused(void)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    jobs_named();
    const unsigned int job = EMBED_CALL(cli_command_line_catalog_among_internal.job_by_name, CliCommandLineCatalogAmongCtx, .catalog = &s_catalog, .among = NULL, .among_count = 0u, .name = "wave", .err_msg = &err_msg);
    (void)mmgr_line_finish(&err_msg);
    TEST_ASSERT_EQUAL_UINT(CLI_CATALOG_NONE, job);
    CLI_TEST_ASSERT_CONTAINS("2 jobs", s_why);
}

void test_an_unknown_name_names_no_job(void)
{
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    jobs_named();
    TEST_ASSERT_EQUAL_UINT(CLI_CATALOG_NONE, EMBED_CALL(cli_command_line_catalog_among_internal.job_by_name, CliCommandLineCatalogAmongCtx, .catalog = &s_catalog, .among = NULL, .among_count = 0u, .name = "none", .err_msg = &err_msg));
}

void test_a_value_not_sent_has_its_default(void)
{
    static const char *const s_sent[6] = {"run=one", "run=two", "--", "--steps", "a b", "echo \"hi\""};
    job_with_values();
    TEST_ASSERT_TRUE(values_read(s_sent, 6u));
    TEST_ASSERT_EQUAL_STRING("a.cfg", values_joined("cfg"));
}

void test_a_key_sent_twice_contains_both(void)
{
    static const char *const s_sent[6] = {"run=one", "run=two", "--", "--steps", "a b", "echo \"hi\""};
    job_with_values();
    TEST_ASSERT_TRUE(values_read(s_sent, 6u));
    TEST_ASSERT_EQUAL_STRING("one|two", values_joined("run"));
}

void test_the_words_after_a_double_dash_come_back_whole(void)
{
    static const char *const s_sent[6] = {"run=one", "run=two", "--", "--steps", "a b", "echo \"hi\""};
    VerbaLinea err_msg = {s_why, sizeof(s_why), 0ull};
    job_with_values();
    TEST_ASSERT_TRUE(values_read(s_sent, 6u));
    const char *const after = EMBED_CALL(cli_catalog_key.values_first, CliCatalogKeyCfg, .values = &s_values, .key = "arguments");
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_runner_text.shell_words, CliRunnerTextCfg, .text = after, .words = &s_words, .err_msg = &err_msg));
    TEST_ASSERT_EQUAL_UINT(3u, s_words.count);
    TEST_ASSERT_EQUAL_STRING("--steps", s_words.word[0]);
    TEST_ASSERT_EQUAL_STRING("a b", s_words.word[1]);
    TEST_ASSERT_EQUAL_STRING("echo \"hi\"", s_words.word[2]);
}

void test_a_key_the_job_does_not_accept_is_refused(void)
{
    static const char *const s_wrong[1] = {"nope=1"};
    job_with_values();
    TEST_ASSERT_FALSE(values_read(s_wrong, 1u));
}

void test_a_word_that_is_no_key_and_value_is_refused(void)
{
    static const char *const s_bare[1] = {"bare"};
    job_with_values();
    TEST_ASSERT_FALSE(values_read(s_bare, 1u));
}
