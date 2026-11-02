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
void cli_recording_run_kill(void)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (s_has_running)
    {
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &s_running);
        s_has_running = 0;
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

void cli_recording_run_end(void)
{
    cli_recording_run_kill();
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    while (s_going)
    {
        EMBED_CALL(cli_lock_internal.wake_wait, CliLockCtx, .wake = &s_wake, .lock = &s_lock);
    }
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

typedef struct
{
    CliRecordingReader *reader;
    unsigned long long size;
} RecordingServeCtx;
// a line of the run's output or errors sent to the client as an output event, the newline that ends it retained
EMBED_INLINE void serve_line_emit(const RecordingServeCtx *args)
{
    unsigned long long size = args->size;
    VerbaLinea out = {args->reader->out_buf, sizeof(args->reader->out_buf), 0ull};
    mmgr_line_put(&out, "{\"type\":\"event\",\"event\":\"output\",\"body\":{\"category\":\"");
    mmgr_line_put(&out, args->reader->category);
    mmgr_line_put(&out, "\",\"output\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->reader->line, .length = size);
    mmgr_line_put(&out, "}}");
    (void)EMBED_CALL(cli_dap_message.send, CliDapMessageCfg, .dap = s_dap, .message = out.out, .size = out.at);
}

// what the run writes to one pipe read until it ends, a line at a time, a line longer than the room sent in pieces
EMBED_INLINE void serve_lines(const RecordingServeCtx *args)
{
    char piece[4096];
    unsigned long long used = 0ull;
    for (unsigned long long count =
             EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                        .pipe_handle = args->reader->pipe, .piece = piece, .room = sizeof(piece));
         count > 0ull; count = EMBED_CALL(cli_room_pipe_handle_internal.spawn_read, CliRoomPipeHandleCtx,
                                          .pipe_handle = args->reader->pipe, .piece = piece, .room = sizeof(piece)))
    {
        for (unsigned long long at = 0ull; at < count; at += 1ull)
        {
            args->reader->line[used] = piece[at];
            used += 1ull;
            if ((piece[at] == '\n') || (used == CLI_RECORDING_OUTPUT_LINE))
            {
                EMBED_CALL(serve_line_emit, RecordingServeCtx, .reader = args->reader, .size = used);
                used = 0ull;
            }
        }
    }
    if (used > 0ull)
    {
        EMBED_CALL(serve_line_emit, RecordingServeCtx, .reader = args->reader, .size = used);
    }
}

// a reader's end: the last waits on the run, lets its process go, and hands the adapter the recording with the code
// the run ended with, none where it was ended or a signal ended it
static void serve_read(void *context)
{
    CliRecordingReader *const reader = (CliRecordingReader *)context;
    EMBED_CALL(serve_lines, RecordingServeCtx, .reader = reader);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_readers -= 1u;
    const int last = (s_readers == 0u);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    if (!last)
    {
        return;
    }
    const long long status = EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &s_running);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    const int has_code = s_has_running && (status >= 0ll);
    s_has_running = 0;
    EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &s_running);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    char recorded[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea out = {recorded, sizeof(recorded), 0ull};
    mmgr_line_put(&out, "{\"type\":\"orior\",\"event\":\"recorded\",\"out\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)s_out,
               .length = cellul.len(&(CatenaFinitaCfg){.src = s_out, .cap = CLI_CELLS_POOL}));
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
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_going = 0;
    EMBED_CALL(cli_lock_internal.wake_all, CliLockCtx, .wake = &s_wake);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    (void)EMBED_CALL(cli_dap_message.itself, CliDapMessageCfg, .dap = dap, .message = out.out, .size = out.at);
    EMBED_CALL(cli_dap.unref, CliDapCfg, .dap = dap);
}

// the program run under the recorder, the shim given it as its input, its output sent as it comes: 1, or 0 with why
// given. A run the replay was recording before is ended first
int cli_recording_run(const CliRecordingDapCtx *args)
{
    CliDap *dap = args->dap;
    char python[CLI_PATH_MAX_LEN];
    if (g_cli_replay->python[0] != '\0')
    {
        memor.cpy(&(MemoriaCfg){
            .dst = python,
            .src = g_cli_replay->python,
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_replay->python, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else if (!EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        mmgr_line_put(args->err_msg, "no Python is found: File, Toolchains takes its folder");
        return 0;
    }
    cli_recording_run_end();
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
    EMBED_CALL(cli_folder_internal.temp_folder, CliFolderCtx, .folder_out = temporary);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = temporary, .relative = name, .joined = s_out);
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path, .size = sizeof(s_path));
    const CliSetting settings[2] = {{"PATH", s_path}, {"PYTHONUNBUFFERED", "1"}};
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    const char *const words[5] = {python, "-", s_out, g_cli_replay->root, g_cli_replay->program};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = words[at]);
    }
    memor.set(&(MemoriaCfg){.dst = &s_running, .val = 0, .bytes = sizeof(s_running)});
    if (!EMBED_CALL(cli_words_err_msg_internal.spawn_feed, CliWordsErrMsgCtx, .spawned = &s_running,
                    .words = s_words.word, .folder = g_cli_replay->root, .settings = settings, .count = 2u,
                    .detached = 1, .err_msg = args->err_msg))
    {
        mmgr_line_put(args->err_msg, ": ");
        mmgr_line_put(args->err_msg, python);
        return 0;
    }
    const int given =
        EMBED_CALL(cli_pipe_handle_internal.spawn_write, CliPipeHandleCtx, .pipe_handle = s_running.in,
                   .bytes = g_cli_recording_shim,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_recording_shim, .cap = CLI_CELLS_POOL}));
    EMBED_CALL(cli_spawned_internal.spawn_input_close, CliSpawnedCtx, .spawned = &s_running);
    if (!given)
    {
        EMBED_CALL(cli_cols_internal.spawn_kill, CliColsCtx, .spawned_read = &s_running);
        (void)EMBED_CALL(cli_spawned_internal.spawn_wait, CliSpawnedCtx, .spawned = &s_running);
        EMBED_CALL(cli_spawned_internal.spawn_close, CliSpawnedCtx, .spawned = &s_running);
        mmgr_line_put(args->err_msg, "the shim could not be given to ");
        mmgr_line_put(args->err_msg, python);
        return 0;
    }
    EMBED_CALL(cli_dap.ref, CliDapCfg, .dap = dap);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_dap = dap;
    s_has_running = 1;
    s_going = 1;
    s_readers = 2u;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    s_output.pipe = s_running.out;
    s_output.category = "stdout";
    s_output.thread = (CliThread){serve_read, &s_output};
    s_errors.pipe = s_running.err;
    s_errors.category = "stderr";
    s_errors.thread = (CliThread){serve_read, &s_errors};
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_errors.thread))
    {
        serve_read(&s_errors);
    }
    if (!EMBED_CALL(cli_thread_internal.thread_start, CliThreadCtx, .thread = &s_output.thread))
    {
        serve_read(&s_output);
    }
    return 1;
}

// the recording the recorder wrote read, what it contains sent, and the replay stopped at the first breakpoint it
// reaches, or the first raise where raises stop it, or at its first step
void cli_recording_on_record(const CliRecordingDapJsonCtx *args)
{
    CliDap *dap = args->dap;
    unsigned int message = args->message;
    const CliRecording *const recording = &g_cli_replay->recording;
    char out[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const path =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = message, .name = "out"),
                   .length_pointer = &length);
    VerbaLinea out_line = {out, sizeof(out), 0ull};
    mmgr_line_put_n(&out_line, (path != NULL) ? path : "", length);
    mmgr_line_finish(&out_line);
    EMBED_CALL(cli_recording_path_internal.read, CliRecordingPathCtx, .path = out);
    const unsigned int code_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = message, .name = "code");
    const char *const written = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                           .json_read = args->json, .node = code_node, .kind = CFG_JSON_NUMBER)
                                    ? args->json->text + args->json->node[code_node].text
                                    : NULL;
    const unsigned long long digits = (written != NULL) ? args->json->node[code_node].length : 0ull;
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
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &event,
               .text_bytes = (const unsigned char *)line.out, .length = line.at);
    mmgr_line_put(&event, "}}");
    (void)EMBED_CALL(cli_dap_message.send, CliDapMessageCfg, .dap = dap, .message = event.out, .size = event.at);
    if (recording->step_count == 0u)
    {
        const char terminated[] = "{\"type\":\"event\",\"event\":\"terminated\",\"body\":{}}";
        (void)EMBED_CALL(cli_dap_message.send, CliDapMessageCfg, .dap = dap, .message = terminated,
                         .size = sizeof(terminated) - 1u);
        return;
    }
    unsigned int first = recording->step_count;
    for (unsigned int step = 0u; (step < recording->step_count) && (first == recording->step_count); step += 1u)
    {
        first =
            (EMBED_CALL(cli_recording_serial_step_internal.replay_at_breakpoint, CliRecordingSerialStepCtx,
                        .step = step) ||
             EMBED_CALL(cli_recording_serial_step_internal.replay_at_raise, CliRecordingSerialStepCtx, .step = step))
                ? step
                : first;
    }
    g_cli_replay->at = (first < recording->step_count) ? first : 0u;
    const char *const reason =
        (first == recording->step_count) ? "start"
        : (EMBED_CALL(cli_recording_serial_step_internal.replay_at_raise, CliRecordingSerialStepCtx, .step = first) &&
           !EMBED_CALL(cli_recording_serial_step_internal.replay_at_breakpoint, CliRecordingSerialStepCtx,
                       .step = first))
            ? "exception"
            : "breakpoint";
    EMBED_CALL(cli_recording_dap_reason_internal.replay_stopped_send, CliRecordingDapReasonCtx, .dap = dap,
               .reason = reason);
}
