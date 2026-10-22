// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_test.c: the recorded run's check: a recording keeps each line with its frame's depth and values, a stack
// stands each caller at its line then, a name's history gives each line that changed it, steps go over, into, out and
// back, and continue and run backward stop at breakpoints and raises
#include "../../../../../ui/cli/src/debug/recording_internal.h"
#include "cli_check.h"

static char s_seen[512];
static CliLine s_seen_line = {s_seen, sizeof(s_seen), 0ull};

// the recording of the recorder's lines for a function `grow` called twice from the module, read into the replay
static void recording_sample(void)
{
    static const char s_lines[] =
        "[\"thread\",1,\"MainThread\"]\n[\"file\",0,\"a.py\"]\n[\"call\",1,0,0,\"<module>\",1]\n"
        "[\"line\",1,1,{},[]]\n[\"line\",1,5,{},[]]\n[\"call\",2,1,0,\"grow\",1]\n[\"line\",2,2,{\"n\":\"1\"},[]]\n"
        "[\"line\",2,3,{\"n\":\"2\"},[]]\n[\"return\",2,\"2\"]\n[\"line\",1,6,{\"a\":\"2\"},[]]\n"
        "[\"call\",3,1,0,\"grow\",1]\n[\"line\",3,2,{\"n\":\"2\"},[]]\n[\"line\",3,3,{\"n\":\"3\"},[]]\n"
        "[\"raise\",3,3,\"ValueError\",\"too big\"]\n[\"return\",3,\"None\"]\n[\"line\",1,7,{\"b\":\"3\"},[]]\n"
        "[\"end\",0]\n";
    char dir[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("recording", dir);
    cli_full(dir, "r.jsonl", path);
    (void)query_file_written(path, s_lines, sizeof(s_lines) - 1u);
    cli_recording_read(path);
}

// the values of frame `serial` as step `at` was about to run, `name=value` joined by `|`, checked against `wanted`
static void recording_values(unsigned int serial, unsigned int at, const char *wanted, const char *what)
{
    CliRecordedValue *values = NULL;
    const unsigned int count = cli_recording_values(serial, at, &values, 8u);
    char text[256];
    CliLine line = {text, sizeof(text), 0ull};
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        cli_line_text(&line, (each > 0u) ? "|" : "");
        cli_line_text(&line, values[each].name);
        cli_line_character(&line, '=');
        cli_line_text(&line, values[each].value);
    }
    cli_cell_given(values);
    cli_check_text(text, line.at, wanted, what);
}

// a recording keeps each line with its frame's depth and values
static void recording_steps(void)
{
    static const unsigned int s_depths[8] = {1u, 1u, 2u, 2u, 1u, 2u, 2u, 1u};
    const CliRecording *const found = &g_cli_replay->recording;
    int deep = found->step_count == 8u;
    for (unsigned int at = 0u; deep && (at < 8u); at += 1u)
    {
        deep = found->steps[at].depth == s_depths[at];
    }
    cli_check(deep, "a recording keeps each line with its frame's depth");
    recording_values(1u, 7u, "a=2|b=3", "and the values its frame's names hold");
    recording_values(2u, 3u, "n=2", "in each frame");
    cli_check(found->has_code && (found->code == 0ll), "and the run's exit code");
    cli_check((found->raised_count == 1u) && (found->raised[0].step == 6u), "and each raise at its step");
}

// the stack at `at`, `frame:step` joined by `|`, checked against `wanted`
static void recording_stack(unsigned int at, const char *wanted, const char *what)
{
    unsigned int frames[8][2];
    const unsigned int count = cli_recording_stack(at, frames, 8u);
    char text[128];
    CliLine line = {text, sizeof(text), 0ull};
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        cli_line_text(&line, (each > 0u) ? "|" : "");
        cli_line_decimal(&line, frames[each][0], 1u);
        cli_line_character(&line, ':');
        cli_line_decimal(&line, frames[each][1], 1u);
    }
    cli_check_text(text, line.at, wanted, what);
}

// a change of a name kept, `step:line:value` after the others
static void recording_changed(void *context, unsigned int step, unsigned int line, const char *value)
{
    (void)context;
    cli_line_text(&s_seen_line, (s_seen_line.at > 0ull) ? "|" : "");
    cli_line_decimal(&s_seen_line, step, 1u);
    cli_line_character(&s_seen_line, ':');
    cli_line_decimal(&s_seen_line, line, 1u);
    cli_line_character(&s_seen_line, ':');
    cli_line_text(&s_seen_line, (value != NULL) ? value : "");
}

// the history of `name` in frame `serial`, checked against `wanted`
static void recording_history(unsigned int serial, const char *name, const char *wanted, const char *what)
{
    s_seen_line.at = 0ull;
    cli_recording_history(serial, name, recording_changed, NULL);
    cli_check_text(s_seen, s_seen_line.at, wanted, what);
}

// where `how` goes from step `at`, checked against `step` and `reason`
static void recording_go(unsigned int at, const char *how, unsigned int step, const char *reason, const char *what)
{
    g_cli_replay->at = at;
    unsigned int went = 0u;
    const char *const why = cli_replay_go(how, &went);
    cli_check((went == step) && (why != NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = why, .other = reason, .cap = CLI_CELLS_POOL})),
              what);
}

int main(void)
{
    recording_sample();
    recording_steps();
    recording_stack(3u, "2:3|1:1", "a stack stands each caller at its line then");
    recording_stack(4u, "1:4", "and its own frame alone once the call returned");
    // n is given by the call at the first line, then by line 2 for the second value
    recording_history(2u, "n", "2:2:1|2:2:2", "a name's history gives each line that changed it");
    recording_history(1u, "a", "1:5:2", "in its own frame");
    memor.cpy(&(MemoriaCfg){.dst = g_cli_replay->root,
                            .src = "/tree",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "/tree", .cap = CLI_CELLS_POOL}) + 1u});
    g_cli_replay->break_file_count = 0u;
    g_cli_replay->on_raised = 0;
    recording_go(1u, "next", 4u, "step", "next steps over a call");
    recording_go(1u, "stepIn", 2u, "step", "step in goes into it");
    recording_go(3u, "stepOut", 4u, "step", "step out comes back out");
    recording_go(4u, "stepBack", 1u, "step", "step back goes over it backward");
    recording_go(7u, "next", 7u, "end", "and the last step ends");
    char key[CLI_PATH_LONGEST];
    cli_replay_key("/tree/a.py", key);
    memor.cpy(&(MemoriaCfg){.dst = g_cli_replay->break_files[0],
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u});
    g_cli_replay->break_lines[0][0] = 2u;
    g_cli_replay->break_count[0] = 1u;
    g_cli_replay->break_file_count = 1u;
    recording_go(0u, "continue", 2u, "breakpoint", "continue stops at a breakpoint");
    recording_go(2u, "continue", 5u, "breakpoint", "and at the next");
    recording_go(7u, "reverseContinue", 5u, "breakpoint", "running backward stops at one too");
    g_cli_replay->on_raised = 1;
    recording_go(5u, "continue", 6u, "exception", "and a raise stops it where asked");
    cli_recording_let_go();
    return cli_checked();
}
