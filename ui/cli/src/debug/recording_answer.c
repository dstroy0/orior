// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_answer.c: what the replay answers: the threads, the stack, a frame's names and their values, a name looked
// up, the exception raised at a step, a move and the stop it ends at, and orior's own two, a name's history through
// the run and a step gone to
#include "recording_internal.h"

// the most values of a frame's names reported at once
#define CLI_REPLAY_VALUES 1024u

static unsigned int s_standing[512][2];

// a string member of the arguments, its text and length, "" where it is none
static const char *answer_text(const CliJson *json, unsigned int arguments, const char *name,
                               unsigned long long *length)
{
    const unsigned int node = cli_json_member(json, arguments, name);
    const char *const text = cli_json_is(json, node, CFG_JSON_STRING) ? cli_json_string(json, node, length) : NULL;
    if (text == NULL)
    {
        *length = 0ull;
        return "";
    }
    return text;
}

static void answer_threads(VerbaLinea *body)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    mmgr_line_put(body, "{\"threads\":[");
    if (recording->thread_count == 0u)
    {
        mmgr_line_put(body, "{\"id\":1,\"name\":\"MainThread\"}");
    }
    for (unsigned int at = 0u; at < recording->thread_count; at += 1u)
    {
        const char *const name = cli_recording_name(recording->thread_names[at]);
        mmgr_line_put(body, (at > 0u) ? ",{\"id\":" : "{\"id\":");
        mmgr_line_decimal(body, recording->thread_ids[at], 1u);
        mmgr_line_put(body, ",\"name\":");
        cli_json_quoted_lossy(body, (const unsigned char *)name,
                              cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(body, '}');
    }
    mmgr_line_put(body, "]}");
}

// the stack of a thread, standing at its own last step where the replay stands on another's
static void answer_stack(const CliJson *json, unsigned int arguments, VerbaLinea *body)
{
    unsigned long long thread = 0ull;
    if (!cli_replay_whole(json, cli_json_member(json, arguments, "threadId"), &thread))
    {
        thread = cli_replay_thread();
    }
    unsigned int at = g_cli_replay->at;
    for (unsigned int one = g_cli_replay->at + 1u; one > 0u; one -= 1u)
    {
        if (cli_replay_thread_of(one - 1u) == thread)
        {
            at = one - 1u;
            break;
        }
    }
    const unsigned int count = cli_recording_stack(at, s_standing, 512u);
    mmgr_line_put(body, "{\"stackFrames\":[");
    for (unsigned int frame = 0u; frame < count; frame += 1u)
    {
        if (frame > 0u)
        {
            mmgr_line_ch(body, ',');
        }
        cli_replay_frame_written(body, s_standing[frame][0], s_standing[frame][1]);
    }
    mmgr_line_put(body, "],\"totalFrames\":");
    mmgr_line_decimal(body, count, 1u);
    mmgr_line_ch(body, '}');
}

static void answer_variables(const CliJson *json, unsigned int arguments, VerbaLinea *body)
{
    unsigned long long serial = 0ull;
    if (!cli_replay_whole(json, cli_json_member(json, arguments, "variablesReference"), &serial) ||
        (serial >= g_cli_replay->recording.frame_most))
    {
        serial = 0ull;
    }
    CliRecordedValue *values = NULL;
    const unsigned int count = cli_recording_values((unsigned int)serial, g_cli_replay->at, &values, CLI_REPLAY_VALUES);
    mmgr_line_put(body, "{\"variables\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(body, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        cli_json_quoted_lossy(body, (const unsigned char *)values[at].name,
                              cellul.len(&(CatenaFinitaCfg){.src = values[at].name, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(body, ",\"value\":");
        cli_json_quoted_lossy(body, (const unsigned char *)values[at].value,
                              cellul.len(&(CatenaFinitaCfg){.src = values[at].value, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(body, ",\"type\":\"\",\"variablesReference\":0}");
    }
    mmgr_line_put(body, "]}");
    g_cli_cells->persistent_buf_release(values, NULL);
}

// a name looked up in the frame, then in each module's frame standing, one whose caller is none
static int answer_evaluated(const CliJson *json, unsigned int arguments, VerbaLinea *body, VerbaLinea *err_msg)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    unsigned long long length = 0ull;
    const char *name = answer_text(json, arguments, "expression", &length);
    while ((length > 0ull) && ((*name == ' ') || (*name == '\t') || (*name == '\n') || (*name == '\r')))
    {
        name += 1;
        length -= 1ull;
    }
    while ((length > 0ull) && ((name[length - 1ull] == ' ') || (name[length - 1ull] == '\t') ||
                               (name[length - 1ull] == '\n') || (name[length - 1ull] == '\r')))
    {
        length -= 1ull;
    }
    const unsigned int serial = cli_replay_frame_of(json, cli_json_member(json, arguments, "frameId"));
    const unsigned int count = cli_recording_stack(g_cli_replay->at, s_standing, 512u);
    for (unsigned int frame = 0u; frame <= count; frame += 1u)
    {
        const unsigned int one = (frame == 0u) ? serial : s_standing[frame - 1u][0];
        if ((frame > 0u) && !(recording->frames[one].seen && (recording->frames[one].parent == 0u)))
        {
            continue;
        }
        CliRecordedValue *values = NULL;
        const unsigned int value_count = cli_recording_values(one, g_cli_replay->at, &values, CLI_REPLAY_VALUES);
        for (unsigned int at = 0u; at < value_count; at += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = values[at].name, .cap = CLI_CELLS_POOL}) == length) &&
                (memor.cmp(&(MemoriaCfg){.src = values[at].name, .other = name, .bytes = length}) == 0))
            {
                mmgr_line_put(body, "{\"result\":");
                cli_json_quoted_lossy(body, (const unsigned char *)values[at].value,
                                      cellul.len(&(CatenaFinitaCfg){.src = values[at].value, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(body, ",\"type\":\"\",\"variablesReference\":0}");
                g_cli_cells->persistent_buf_release(values, NULL);
                return 1;
            }
        }
        g_cli_cells->persistent_buf_release(values, NULL);
    }
    mmgr_line_put(err_msg, "a recorded run reads the names its frames contained, and ");
    mmgr_line_put_n(err_msg, name, length);
    mmgr_line_put(err_msg, " is none of them");
    return 0;
}

static int answer_exception(VerbaLinea *body, VerbaLinea *err_msg)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    for (unsigned int at = 0u; at < recording->raised_count; at += 1u)
    {
        if (recording->raised[at].step == g_cli_replay->at)
        {
            const char *const kind = cli_recording_name(recording->raised[at].kind);
            const char *const message = cli_recording_name(recording->raised[at].message);
            mmgr_line_put(body, "{\"exceptionId\":");
            cli_json_quoted_lossy(body, (const unsigned char *)kind,
                                  cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(body, ",\"description\":");
            cli_json_quoted_lossy(body, (const unsigned char *)message,
                                  cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(body, ",\"breakMode\":\"always\"}");
            return 1;
        }
    }
    mmgr_line_put(err_msg, "no exception was raised at this step");
    return 0;
}

// a move made: the client notified the run went on, then where it stopped
static void answer_moved(CliDap *dap, const char *command, VerbaLinea *body)
{
    unsigned int step = g_cli_replay->at;
    const char *const reason = cli_replay_go(command, &step);
    g_cli_replay->at = step;
    char out_buf[256];
    VerbaLinea out = {out_buf, sizeof(out_buf), 0ull};
    mmgr_line_put(&out, "{\"type\":\"event\",\"event\":\"continued\",\"body\":{\"threadId\":");
    mmgr_line_decimal(&out, cli_replay_thread(), 1u);
    mmgr_line_put(&out, ",\"allThreadsContinued\":true}}");
    (void)cli_dap_send(dap, out.out, out.at);
    cli_replay_stopped_sent(dap, reason);
    mmgr_line_put(body, "{\"allThreadsContinued\":true}");
}

// The history of a name being written: where it goes, the path of the frame's file, and whether one is written yet
typedef struct
{
    VerbaLinea *body;
    const char *path;
    int written;
} CliReplayHistory;

static void answer_change(void *context, unsigned int step, unsigned int line, const char *value)
{
    CliReplayHistory *const history = (CliReplayHistory *)context;
    mmgr_line_put(history->body, history->written ? ",{\"step\":" : "{\"step\":");
    history->written = 1;
    mmgr_line_decimal(history->body, step, 1u);
    mmgr_line_put(history->body, ",\"line\":");
    mmgr_line_decimal(history->body, line, 1u);
    mmgr_line_put(history->body, ",\"path\":");
    cli_json_quoted_lossy(history->body, (const unsigned char *)history->path,
                          cellul.len(&(CatenaFinitaCfg){.src = history->path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(history->body, ",\"value\":");
    if (value != NULL)
    {
        cli_json_quoted_lossy(history->body, (const unsigned char *)value,
                              cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(history->body, "null");
    }
    mmgr_line_put(history->body, (step < g_cli_replay->at) ? ",\"before\":true}" : ",\"before\":false}");
}

static void answer_history(const CliJson *json, unsigned int arguments, VerbaLinea *body)
{
    char name[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const text = answer_text(json, arguments, "name", &length);
    VerbaLinea sought = {name, sizeof(name), 0ull};
    mmgr_line_put_n(&sought, text, length);
    mmgr_line_finish(&sought);
    const unsigned int serial = cli_replay_frame_of(json, cli_json_member(json, arguments, "frameId"));
    char path[CLI_PATH_MAX_LEN];
    cli_replay_full(cli_replay_file_of(serial), path);
    CliReplayHistory history = {body, path, 0};
    mmgr_line_put(body, "{\"changes\":[");
    cli_recording_history(serial, name, answer_change, &history);
    mmgr_line_put(body, "],\"at\":");
    mmgr_line_decimal(body, g_cli_replay->at, 1u);
    mmgr_line_ch(body, '}');
}

int cli_replay_answer(CliDap *dap, const char *command, const CliJson *json, unsigned int arguments, VerbaLinea *body,
                      VerbaLinea *err_msg)
{
    const int ready = (g_cli_replay->recording.step_count > 0u);
    const int moves =
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "continue", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "next", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "stepIn", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "stepOut", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "stepBack", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "reverseContinue", .cap = CLI_CELLS_POOL}));
    unsigned long long step = 0ull;
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "threads", .cap = CLI_CELLS_POOL}))
    {
        answer_threads(body);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "pause", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(body, "null");
        return 1;
    }
    if (!ready)
    {
        mmgr_line_put(err_msg, "the run is still being recorded");
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "stackTrace", .cap = CLI_CELLS_POOL}))
    {
        answer_stack(json, arguments, body);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "scopes", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(body,
                      "{\"scopes\":[{\"name\":\"Locals\",\"presentationHint\":\"locals\",\"variablesReference\":");
        mmgr_line_decimal(body, cli_replay_frame_of(json, cli_json_member(json, arguments, "frameId")), 1u);
        mmgr_line_put(body, ",\"expensive\":false}]}");
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "variables", .cap = CLI_CELLS_POOL}))
    {
        answer_variables(json, arguments, body);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "evaluate", .cap = CLI_CELLS_POOL}))
    {
        return answer_evaluated(json, arguments, body, err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "exceptionInfo", .cap = CLI_CELLS_POOL}))
    {
        return answer_exception(body, err_msg);
    }
    if (moves)
    {
        answer_moved(dap, command, body);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "orior/history", .cap = CLI_CELLS_POOL}))
    {
        answer_history(json, arguments, body);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = command, .other = "orior/goto", .cap = CLI_CELLS_POOL}))
    {
        if (!cli_replay_whole(json, cli_json_member(json, arguments, "step"), &step) ||
            (step >= g_cli_replay->recording.step_count))
        {
            mmgr_line_put(err_msg, "no such step");
            return 0;
        }
        g_cli_replay->at = (unsigned int)step;
        cli_replay_stopped_sent(dap, "goto");
        mmgr_line_put(body, "null");
        return 1;
    }
    mmgr_line_put(err_msg, "a recorded run does not take ");
    mmgr_line_put(err_msg, command);
    return 0;
}
