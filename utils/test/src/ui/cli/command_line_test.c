// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_test.c: the command line's checks: the help's lines wrapped keeping every word and the width, the help
// keeping to its width and naming every command, a run a terminal of the window's starts told on a channel of its
// own, a job named by its id, the tail of its id or its title, and the values a run's words give a job
#include "../../../../../ui/cli/src/command_line/command_line_internal.h"
#include "../../../../../ui/cli/src/command_line/commands.h"
#include "../../../../../ui/cli/src/comms/comms.h"
#include "../../../../../ui/cli/src/comms/door.h"
#include "../../../../../ui/cli/src/runs/runner.h"
#include "../../../../../ui/cli/src/text/text.h"
#include "cli_check.h"

// the most lines and messages a check keeps
#define TEST_LINES 64u
#define TEST_TOLD 64u

// The lines a wrapping gave
typedef struct
{
    char line[TEST_LINES][256];
    unsigned int count;
} TestLines;

// a line of a wrapping kept
static void test_line_kept(void *context, const char *line, unsigned long long size)
{
    TestLines *const lines = context;
    if ((lines->count < TEST_LINES) && (size < 256ull))
    {
        memor.cpy(&(MemoriaCfg){.dst = lines->line[lines->count], .src = line, .bytes = size});
        lines->line[lines->count][size] = '\0';
        lines->count += 1u;
    }
}

// wrapping keeps every word and the width
static void test_wrapping(void)
{
    static const char s_text[] =
        "orior_cli file new-plugin <name> --ext <ext,...> [--from <plugin>] [--line-comment <text>] "
        "[--keywords <a,b>]";
    static TestLines s_lines;
    s_lines.count = 0u;
    cli_help_wrapped(s_text, 40ull, 2ull, test_line_kept, &s_lines);
    cli_check(s_lines.count > 1u, "a long usage wraps onto more than one line");
    int within = 1;
    int indented = 1;
    char joined[512];
    CliLine whole = {joined, sizeof(joined), 0ull};
    for (unsigned int at = 0u; at < s_lines.count; at += 1u)
    {
        within &=
            cli_text_letters(s_lines.line[at],
                             cellul.len(&(CatenaFinitaCfg){.src = s_lines.line[at], .cap = CLI_CELLS_POOL})) <= 40ull;
        if (at > 0u)
        {
            indented &=
                (cellul.starts(&(CatenaFinitaCfg){.src = s_lines.line[at], .other = "  ", .cap = CLI_CELLS_POOL})) &&
                (!cellul.starts(&(CatenaFinitaCfg){.src = s_lines.line[at], .other = "   ", .cap = CLI_CELLS_POOL}));
        }
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        cli_text_trimmed(s_lines.line[at],
                         cellul.len(&(CatenaFinitaCfg){.src = s_lines.line[at], .cap = CLI_CELLS_POOL}), &from, &to);
        cli_line_text(&whole, (at > 0u) ? " " : "");
        cli_line_counted(&whole, s_lines.line[at] + from, to - from);
    }
    cli_check(within, "every wrapped line is 40 letters at most");
    cli_check(indented, "every line after the first is indented two spaces");
    cli_check_text(joined, whole.at, s_text, "the lines trimmed and joined give the text back");
    s_lines.count = 0u;
    cli_help_wrapped("short", 40ull, 2ull, test_line_kept, &s_lines);
    cli_check((s_lines.count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_lines.line[0], .other = "short", .cap = CLI_CELLS_POOL})),
              "a short text is one line");
}

// the help keeps to its width and names every command
static void test_help(void)
{
    static char s_help[1u << 18u];
    static CliItem s_items[CLI_ITEMS_MOST];
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_commands_read(&said), "the menus read");
    CliLine help = {s_help, sizeof(s_help) - 1u, 0ull};
    cli_help(&help);
    s_help[help.at] = '\0';
    int within = 1;
    for (const char *line = s_help; *line != '\0';)
    {
        const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL, .byte = '\n'});
        const unsigned long long size = (end != NULL)
                                            ? (unsigned long long)(end - line)
                                            : cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_CELLS_POOL});
        within &= cli_text_letters(line, size) <= CLI_HELP_WIDTH;
        line += size + ((end != NULL) ? 1u : 0u);
    }
    cli_check(within, "every line of the help is 100 letters at most");
    int named = 1;
    for (unsigned int menu = 0u; menu < cli_menus(); menu += 1u)
    {
        char word[CLI_LABEL_LONGEST];
        cli_menu_word(menu, word);
        const unsigned int items = cli_menu_items(menu, s_items, CLI_ITEMS_MOST);
        for (unsigned int at = 0u; at < items; at += 1u)
        {
            char usage[CLI_LABEL_LONGEST * 2u + 16u];
            CliLine line = {usage, sizeof(usage), 0ull};
            cli_line_text(&line, "orior_cli ");
            cli_line_text(&line, word);
            cli_line_character(&line, ' ');
            cli_line_text(&line, s_items[at].command);
            (void)cli_line_closed(&line);
            if (cellul.find(&(CatenaFinitaCfg){
                    .src = s_help, .cap = CLI_CELLS_POOL, .other = usage, .other_cap = CLI_CELLS_POOL}) == NULL)
            {
                named = 0;
                cli_check(0, usage);
            }
        }
    }
    cli_check(named, "the help names every command of every menu");
}

// What the window heard of a run, under its lock: each message's channel, speaker and body, and whether the end came
typedef struct
{
    CliLock lock;
    CliWake wake;
    char channel[TEST_TOLD][CLI_NAME_LONGEST];
    char from[TEST_TOLD][CLI_NAME_LONGEST];
    char body[TEST_TOLD][4096];
    unsigned int count;
    int ended;
} TestHeard;

static TestHeard s_heard = {CLI_LOCK_INIT, CLI_WAKE_INIT};

// a message the window heard kept
static int test_heard(void *context, const char *channel, const char *from, const char *body, unsigned long long size)
{
    (void)context;
    cli_lock_take(&s_heard.lock);
    if ((s_heard.count < TEST_TOLD) && (size < 4096ull))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_heard.channel[s_heard.count],
                                .src = channel,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = channel, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = s_heard.from[s_heard.count],
                                .src = from,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = s_heard.body[s_heard.count], .src = body, .bytes = size});
        s_heard.body[s_heard.count][size] = '\0';
        s_heard.ended |= (cellul.find(&(CatenaFinitaCfg){.src = s_heard.body[s_heard.count],
                                                         .cap = CLI_CELLS_POOL,
                                                         .other = "\"state\":\"ended\"",
                                                         .other_cap = CLI_CELLS_POOL}) != NULL);
        s_heard.count += 1u;
    }
    cli_wake_all(&s_heard.wake);
    cli_lock_give(&s_heard.lock);
    return 1;
}

// a run a terminal of the window's starts is told on a channel of its own
static void test_telling(void)
{
    static const char *const s_all[1] = {"*"};
    static const char *const s_runs[1] = {"runs/*"};
    static CliJob s_jobs[4];
    static CliParam s_params[4];
    static CliStep s_steps[4];
    static CliArg s_args[4];
    static unsigned int s_lists[16];
    static char s_text[4096];
    static CliCatalog s_catalog;
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    char address[CLI_PATH_LONGEST];
    char window[CLI_COMMS_KEY];
    char terminal[CLI_COMMS_KEY];
    unsigned int listener = 0u;
    const int joined = cli_door_open(address, &said) &&
                       cli_comms_register("window", 0, 0u, s_all, 1u, NULL, 0u, window, &said) &&
                       cli_comms_listen(window, "runs/*", 0, test_heard, NULL, &listener, &said) &&
                       cli_comms_register("terminal", 0, 0u, s_runs, 1u, s_runs, 1u, terminal, &said);
    cli_check(joined, "the door opens, and the window and the terminal join it");
    if (!joined)
    {
        return;
    }
    cli_comms_bind(terminal, cli_process_own());
    (void)query_environment_set(CLI_DOOR_ADDRESS_VARIABLE, address);
    (void)query_environment_set(CLI_DOOR_KEY_VARIABLE, terminal);
    char root[CLI_PATH_LONGEST];
    char script[CLI_PATH_LONGEST];
    cli_check_folder("telling", root);
    cli_full(root, "say.py", script);
    (void)query_file_written(script, "print('said')\n", 14ull);
    cli_catalog_begun(&s_catalog, s_jobs, 4u, s_params, 4u, s_steps, 4u, s_args, 4u, s_lists, 16u, s_text, 4096u);
    const unsigned int job = cli_catalog_job(&s_catalog, "test/say", "test", "say", "say.py", "", "");
    cli_catalog_step(&s_catalog, CLI_PROGRAM_PYTHON, "say.py", NULL, 0);
    const int code = cli_job_run(root, &s_catalog, job, NULL, 0u);
    cli_check(code == 0, "the run exits 0");
    cli_lock_take(&s_heard.lock);
    const unsigned long long until = cli_clock_milliseconds() + 5000ull;
    while (!s_heard.ended && (cli_clock_milliseconds() < until))
    {
        (void)cli_wake_wait_for(&s_heard.wake, &s_heard.lock, 100ull);
    }
    char channel[CLI_NAME_LONGEST];
    CliLine own = {channel, sizeof(channel), 0ull};
    cli_line_text(&own, "runs/");
    cli_line_decimal(&own, cli_process_own(), 1u);
    (void)cli_line_closed(&own);
    int its_own = (s_heard.count > 0u);
    int said_line = 0;
    for (unsigned int at = 0u; at < s_heard.count; at += 1u)
    {
        its_own &=
            (cellul.eq(&(CatenaFinitaCfg){.src = s_heard.channel[at], .other = channel, .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_heard.from[at], .other = "terminal", .cap = CLI_CELLS_POOL}));
        said_line |= (cellul.find(&(CatenaFinitaCfg){.src = s_heard.body[at],
                                                     .cap = CLI_CELLS_POOL,
                                                     .other = "\"line\":\"said\"",
                                                     .other_cap = CLI_CELLS_POOL}) != NULL) &&
                     (cellul.find(&(CatenaFinitaCfg){.src = s_heard.body[at],
                                                     .cap = CLI_CELLS_POOL,
                                                     .other = "\"stream\":\"stdout\"",
                                                     .other_cap = CLI_CELLS_POOL}) != NULL);
    }
    cli_check(its_own, "each message is told on the run's own channel by the terminal");
    cli_check((s_heard.count > 0u) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = s_heard.body[0],
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "\"state\":\"started\"",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = s_heard.body[0],
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "\"job\":\"test/say\"",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "the first message says the run started, and its job");
    cli_check(said_line, "the line the run said is told, on stdout");
    cli_check(s_heard.ended && (cellul.find(&(CatenaFinitaCfg){.src = s_heard.body[s_heard.count - 1u],
                                                               .cap = CLI_CELLS_POOL,
                                                               .other = "\"code\":0",
                                                               .other_cap = CLI_CELLS_POOL}) != NULL),
              "the last message says the run ended with 0");
    cli_lock_give(&s_heard.lock);
    cli_comms_unlisten(listener);
}

static CliJob s_jobs[8];
static CliParam s_params[8];
static CliStep s_steps[8];
static CliArg s_args[8];
static unsigned int s_lists[32];
static char s_text[8192];
static CliCatalog s_catalog;

// a job is named by its id, the tail of its id, or its title
static void test_named(void)
{
    cli_catalog_begun(&s_catalog, s_jobs, 8u, s_params, 8u, s_steps, 8u, s_args, 8u, s_lists, 32u, s_text, 8192u);
    (void)cli_catalog_job(&s_catalog, "build/src/build_engine.sh", "test", "src/build_engine.sh", "", "", "");
    (void)cli_catalog_job(&s_catalog, "sim/wave", "test", "wave", "", "", "");
    (void)cli_catalog_job(&s_catalog, "view/a/build_wave_view.py", "test", "wave", "", "", "");
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    unsigned int job = cli_job_named(&s_catalog, NULL, 0u, "build_engine.sh", &said);
    cli_check((job != CLI_CATALOG_NONE) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = cli_catalog_at(&s_catalog, s_catalog.job[job].id),
                                                .other = "build/src/build_engine.sh",
                                                .cap = CLI_CELLS_POOL})),
              "build_engine.sh names the job whose id ends in it");
    job = cli_job_named(&s_catalog, NULL, 0u, "sim/wave", &said);
    cli_check(
        (job != CLI_CATALOG_NONE) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = cli_catalog_at(&s_catalog, s_catalog.job[job].id), .other = "sim/wave", .cap = CLI_CELLS_POOL})),
        "sim/wave names the job of that id");
    said.at = 0ull;
    job = cli_job_named(&s_catalog, NULL, 0u, "wave", &said);
    (void)cli_line_closed(&said);
    cli_check((job == CLI_CATALOG_NONE) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = why, .cap = CLI_CELLS_POOL, .other = "2 jobs", .other_cap = CLI_CELLS_POOL}) != NULL),
              "wave names 2 jobs, and is refused");
    said.at = 0ull;
    cli_check(cli_job_named(&s_catalog, NULL, 0u, "none", &said) == CLI_CATALOG_NONE, "none names no job");
}

// the values of `key`, joined by a bar
static void test_values_of(const CliValues *values, const char *key, char *out, unsigned long long room)
{
    CliLine line = {out, room, 0ull};
    unsigned int count = 0u;
    const unsigned int *const items = cli_values_of(values, key, &count);
    for (unsigned int at = 0u; (items != NULL) && (at < count); at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_text(&line, cli_values_at(values, items[at]));
    }
    (void)cli_line_closed(&line);
}

// values take keys, free words and defaults
static void test_values(void)
{
    static CliValues s_values;
    static CliWords s_words;
    cli_catalog_begun(&s_catalog, s_jobs, 8u, s_params, 8u, s_steps, 8u, s_args, 8u, s_lists, 32u, s_text, 8192u);
    const unsigned int job = cli_catalog_job(&s_catalog, "run/x", "test", "x", "", "", "");
    cli_catalog_param(&s_catalog, "cfg", "choice", 0);
    cli_catalog_choice(&s_catalog, "a.cfg");
    cli_catalog_param(&s_catalog, "run", "many", 0);
    cli_catalog_param(&s_catalog, "arguments", "words", 0);
    static const char *const s_given[6] = {"run=one", "run=two", "--", "--steps", "a b", "say \"hi\""};
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    const int read = cli_job_values(&s_catalog, job, s_given, 6u, &s_values, &said);
    cli_check(read, "the words read as values");
    char text[1024];
    test_values_of(&s_values, "cfg", text, sizeof(text));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = text, .other = "a.cfg", .cap = CLI_CELLS_POOL}),
              "cfg takes its default");
    test_values_of(&s_values, "run", text, sizeof(text));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = text, .other = "one|two", .cap = CLI_CELLS_POOL}),
              "run, given twice, holds both");
    const char *const after = cli_values_first(&s_values, "arguments");
    said.at = 0ull;
    const int split = (after != NULL) && cli_runner_shell_words(after, &s_words, &said);
    cli_check(split && (s_words.count == 3u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[0], .other = "--steps", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[1], .other = "a b", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[2], .other = "say \"hi\"", .cap = CLI_CELLS_POOL})),
              "the words after -- come back whole");
    static const char *const s_wrong[1] = {"nope=1"};
    static const char *const s_bare[1] = {"bare"};
    said.at = 0ull;
    cli_check(!cli_job_values(&s_catalog, job, s_wrong, 1u, &s_values, &said),
              "a key the job does not take is refused");
    said.at = 0ull;
    cli_check(!cli_job_values(&s_catalog, job, s_bare, 1u, &s_values, &said), "a word that is no key=value is refused");
}

int main(void)
{
    test_wrapping();
    test_help();
    test_telling();
    test_named();
    test_values();
    return cli_checked();
}
