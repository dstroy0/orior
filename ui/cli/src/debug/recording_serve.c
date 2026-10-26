// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_serve.c: the run a replay steps through: the program run under the shim, its output reported as it comes,
// the recording read when the run ends and what it contains reported, and the run ended
#include "../tests/test_runs.h"
#include "../toolchains/toolchains.h"
#include "recording_internal.h"

// the bytes of one line of the run's output reported
#define CLI_RECORDING_OUTPUT_LINE 65536u

// One reader of the run's output or errors: the pipe, the category its lines are reported as, the line so far, and the
// event that reports it
typedef struct
{
    unsigned long long pipe;
    const char *category;
    char line[CLI_RECORDING_OUTPUT_LINE];
    char out_buf[(CLI_RECORDING_OUTPUT_LINE * 6u) + 256u];
    CliThread thread;
} CliRecordingReader;

// the run being recorded: its process, whether it goes on not ended by the adapter, whether its readers go on, the
// adapter they report to, its out file, the readers, its words and PATH, and the count of runs recorded, which names
// each out file
static CliSpawned s_running;
static int s_has_running;
static int s_going;
static unsigned int s_readers;
static CliDap *s_dap;
static char s_out[CLI_PATH_MAX_LEN];
static CliRecordingReader s_output;
static CliRecordingReader s_errors;
static CliWords s_words;
static char s_path[CLI_PATH_MAX_LEN * 8u];
static unsigned long long s_recordings;
static CliLock s_lock = CLI_LOCK_INIT;
static CliWake s_wake = CLI_WAKE_INIT;

// the run ended where it goes on; and the same, then waited for until its readers end
void cli_recording_run_killed(void)
{
    cli_lock_take(&s_lock);
    if (s_has_running)
    {
        cli_spawn_killed(&s_running);
        s_has_running = 0;
    }
    cli_lock_give(&s_lock);
}

void cli_recording_run_ended(void)
{
    cli_recording_run_killed();
    cli_lock_take(&s_lock);
    while (s_going)
    {
        cli_wake_wait(&s_wake, &s_lock);
    }
    cli_lock_give(&s_lock);
}

// a line of the run's output or errors sent to the client as an output event, the newline that ends it retained
static void serve_line_emitted(CliRecordingReader *reader, unsigned long long size)
{
    VerbaLinea out = {reader->out_buf, sizeof(reader->out_buf), 0ull};
    mmgr_line_put(&out, "{\"type\":\"event\",\"event\":\"output\",\"body\":{\"category\":\"");
    mmgr_line_put(&out, reader->category);
    mmgr_line_put(&out, "\",\"output\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)reader->line, size);
    mmgr_line_put(&out, "}}");
    (void)cli_dap_send(s_dap, out.out, out.at);
}

// what the run writes to one pipe read until it ends, a line at a time, a line longer than the room sent in pieces
static void serve_lines(CliRecordingReader *reader)
{
    char piece[4096];
    unsigned long long used = 0ull;
    for (unsigned long long count = cli_spawn_read(reader->pipe, piece, sizeof(piece)); count > 0ull;
         count = cli_spawn_read(reader->pipe, piece, sizeof(piece)))
    {
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            reader->line[used] = piece[at];
            used += 1ull;
            if ((piece[at] == '\n') || (used == CLI_RECORDING_OUTPUT_LINE))
            {
                serve_line_emitted(reader, used);
                used = 0ull;
            }
        }
    }
    if (used > 0ull)
    {
        serve_line_emitted(reader, used);
    }
}

// a reader's end: the last waits on the run, lets its process go, and hands the adapter the recording with the code
// the run ended with, none where it was ended or a signal ended it
static void serve_read(void *context)
{
    CliRecordingReader *const reader = (CliRecordingReader *)context;
    serve_lines(reader);
    cli_lock_take(&s_lock);
    s_readers -= 1u;
    const int last = (s_readers == 0u);
    cli_lock_give(&s_lock);
    if (!last)
    {
        return;
    }
    const long long status = cli_spawn_wait(&s_running);
    cli_lock_take(&s_lock);
    const int has_code = s_has_running && (status >= 0ll);
    s_has_running = 0;
    cli_spawn_closed(&s_running);
    cli_lock_give(&s_lock);
    char recorded[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea out = {recorded, sizeof(recorded), 0ull};
    mmgr_line_put(&out, "{\"type\":\"orior\",\"event\":\"recorded\",\"out\":");
    cli_json_quoted_lossy(&out, (const unsigned char *)s_out,
                          cellul.len(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&out, ",\"code\":");
    if (has_code)
    {
        mmgr_line_decimal_signed(&out, status);
    }
    else
    {
        mmgr_line_put(&out, "null");
    }
    mmgr_line_ch(&out, '}');
    // the run is let go before the adapter is handed its recording, so that a record that waits on this end, on the
    // adapter's own thread, does not wait on the adapter taking the message
    CliDap *const dap = s_dap;
    cli_lock_take(&s_lock);
    s_going = 0;
    cli_wake_all(&s_wake);
    cli_lock_give(&s_lock);
    (void)cli_dap_itself(dap, out.out, out.at);
    cli_dap_unref(dap);
}

// the program run under the recorder, the shim given it as its input, its output sent as it comes: 1, or 0 with why
// given. A run the replay was recording before is ended first
int cli_recording_run(CliDap *dap, VerbaLinea *err_msg)
{
    char python[CLI_PATH_MAX_LEN];
    if (g_cli_replay->python[0] != '\0')
    {
        memor.cpy(&(MemoriaCfg){
            .dst = python,
            .src = g_cli_replay->python,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_replay->python, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else if (!cli_test_python(python))
    {
        mmgr_line_put(err_msg, "no Python is found: File, Toolchains takes its folder");
        return 0;
    }
    cli_recording_run_ended();
    char temporary[CLI_PATH_MAX_LEN];
    char name[96];
    VerbaLinea name_line = {name, sizeof(name), 0ull};
    mmgr_line_put(&name_line, "orior-recording-");
    mmgr_line_decimal(&name_line, cli_process_own(), 1u);
    mmgr_line_ch(&name_line, '-');
    mmgr_line_decimal(&name_line, s_recordings, 1u);
    mmgr_line_put(&name_line, ".jsonl");
    mmgr_line_finish(&name_line);
    s_recordings += 1ull;
    cli_temp_folder(temporary);
    cli_full(temporary, name, s_out);
    cli_toolchains_run_path(s_path, sizeof(s_path));
    const CliSetting settings[2] = {{"PATH", s_path}, {"PYTHONUNBUFFERED", "1"}};
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    const char *const words[5] = {python, "-", s_out, g_cli_replay->root, g_cli_replay->program};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        cli_words_put(&s_words, words[at]);
    }
    memor.set(&(MemoriaCfg){.dst = &s_running, .val = 0, .bytes = sizeof(s_running)});
    if (!cli_spawn_fed(&s_running, s_words.word, g_cli_replay->root, settings, 2u, 1, err_msg))
    {
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put(err_msg, python);
        return 0;
    }
    const int given =
        cli_spawn_written(s_running.in, g_cli_recording_shim,
                          cellul.len(&(CatenaFinitaCfg){.src = g_cli_recording_shim, .cap = CLI_CELLS_POOL}));
    cli_spawn_input_closed(&s_running);
    if (!given)
    {
        cli_spawn_killed(&s_running);
        (void)cli_spawn_wait(&s_running);
        cli_spawn_closed(&s_running);
        mmgr_line_put(err_msg, "the shim could not be given to ");
        mmgr_line_put(err_msg, python);
        return 0;
    }
    cli_dap_ref(dap);
    cli_lock_take(&s_lock);
    s_dap = dap;
    s_has_running = 1;
    s_going = 1;
    s_readers = 2u;
    cli_lock_give(&s_lock);
    s_output.pipe = s_running.out;
    s_output.category = "stdout";
    s_output.thread = (CliThread){serve_read, &s_output};
    s_errors.pipe = s_running.err;
    s_errors.category = "stderr";
    s_errors.thread = (CliThread){serve_read, &s_errors};
    if (!cli_thread_started(&s_errors.thread))
    {
        serve_read(&s_errors);
    }
    if (!cli_thread_started(&s_output.thread))
    {
        serve_read(&s_output);
    }
    return 1;
}

// the recording the recorder wrote read, what it contains sent, and the replay stopped at the first breakpoint it
// reaches, or the first raise where raises stop it, or at its first step
void cli_recording_recorded(CliDap *dap, const CliJson *json, unsigned int message)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    char out[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const path = cli_json_string(json, cli_json_member(json, message, "out"), &length);
    VerbaLinea out_line = {out, sizeof(out), 0ull};
    mmgr_line_put_n(&out_line, (path != NULL) ? path : "", (path != NULL) ? length : 0ull);
    mmgr_line_finish(&out_line);
    cli_recording_read(out);
    const unsigned int code_node = cli_json_member(json, message, "code");
    const char *const written =
        cli_json_is(json, code_node, CFG_JSON_NUMBER) ? json->text + json->node[code_node].text : NULL;
    const unsigned long long digits = (written != NULL) ? json->node[code_node].length : 0ull;
    const int negative = (digits > 0ull) && (written[0] == '-');
    int has_code = recording->has_code || ((digits > (negative ? 1ull : 0ull)) && (digits <= 19ull));
    long long code = 0ll;
    for (unsigned long long at = negative ? 1ull : 0ull; !recording->has_code && has_code && (at < digits); at += 1ull)
    {
        has_code = (written[at] >= '0') && (written[at] <= '9');
        code = (code * 10ll) + (long long)(written[at] - '0');
    }
    code = recording->has_code ? recording->code : (negative ? -code : code);
    char line_buf[512];
    VerbaLinea line = {line_buf, sizeof(line_buf), 0ull};
    if (recording->step_count == 0u)
    {
        mmgr_line_put(&line, "The run ran no line of the tree's own code; there is nothing to step through.\n");
    }
    else if (recording->cut)
    {
        mmgr_line_put(&line, "The recording contains the first ");
        mmgr_line_decimal(&line, recording->step_count, 1u);
        mmgr_line_put(&line, " lines the run ran, and stops there.\n");
    }
    else
    {
        mmgr_line_put(&line, "The recording contains the ");
        mmgr_line_decimal(&line, recording->step_count, 1u);
        mmgr_line_put(&line, " lines the run ran; it ended with exit code ");
        if (has_code)
        {
            mmgr_line_decimal_signed(&line, code);
        }
        else
        {
            mmgr_line_put(&line, "unknown");
        }
        mmgr_line_put(&line, ".\n");
    }
    char event_buf[1024];
    VerbaLinea event = {event_buf, sizeof(event_buf), 0ull};
    mmgr_line_put(&event, "{\"type\":\"event\",\"event\":\"output\",\"body\":{\"category\":\"console\",\"output\":");
    cli_json_quoted_lossy(&event, (const unsigned char *)line.out, line.at);
    mmgr_line_put(&event, "}}");
    (void)cli_dap_send(dap, event.out, event.at);
    if (recording->step_count == 0u)
    {
        const char terminated[] = "{\"type\":\"event\",\"event\":\"terminated\",\"body\":{}}";
        (void)cli_dap_send(dap, terminated, sizeof(terminated) - 1u);
        return;
    }
    unsigned int first = recording->step_count;
    for (unsigned int step = 0u; (step < recording->step_count) && (first == recording->step_count); step += 1u)
    {
        first = (cli_replay_at_breakpoint(step) || cli_replay_at_raise(step)) ? step : first;
    }
    g_cli_replay->at = (first < recording->step_count) ? first : 0u;
    const char *const reason = (first == recording->step_count)                                   ? "start"
                               : (cli_replay_at_raise(first) && !cli_replay_at_breakpoint(first)) ? "exception"
                                                                                                  : "breakpoint";
    cli_replay_stopped_sent(dap, reason);
}
