// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_answer.c: what the replay answers: the threads, the stack, a frame's names and their values, a name looked
// up, the exception raised at a step, a move and the stop it ends at, and orior's own two, a name's history through
// the run and a step gone to
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "recording_internal.h"

// the most values of a frame's names reported at once
#define CLI_REPLAY_VALUES 1024u

static unsigned int s_standing[512][2];

typedef struct
{
    const CliJson *json;
    unsigned int arguments;
    const char *name;
    unsigned long long *length;
    VerbaLinea *body;
    VerbaLinea *err_msg;
    CliDap *dap;
    const char *command;
} RecordingAnswerCtx;
// a string member of the arguments, its text and length, "" where it is none
EMBED_INLINE const char *answer_text(const RecordingAnswerCtx *args)
{
    const CliJson *json = args->json;
    const char *name = args->name;
    unsigned long long *length = args->length;
    const unsigned int node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->arguments, .name = name);
    const char *const text = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                        .node = node, .kind = CFG_JSON_STRING)
                                 ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                              .json_read = json, .node = node, .length_pointer = length)
                                 : NULL;
    if (text == NULL)
    {
        *length = 0ull;
        return "";
    }
    return text;
}

EMBED_INLINE void answer_threads(const RecordingAnswerCtx *args)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    mmgr_line_put(args->body, "{\"threads\":[");
    if (recording->thread_count == 0u)
    {
        mmgr_line_put(args->body, "{\"id\":1,\"name\":\"MainThread\"}");
    }
    for (unsigned int at = 0u; at < recording->thread_count; at += 1u)
    {
        const char *const name = EMBED_CALL(cli_recording_at_frames_internal.name, CliRecordingAtFramesCtx,
                                            .at = recording->thread_names[at]);
        mmgr_line_put(args->body, (at > 0u) ? ",{\"id\":" : "{\"id\":");
        mmgr_line_decimal(args->body, recording->thread_ids[at], 1u);
        mmgr_line_put(args->body, ",\"name\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->body,
                   .text_bytes = (const unsigned char *)name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
        mmgr_line_ch(args->body, '}');
    }
    mmgr_line_put(args->body, "]}");
}

// the stack of a thread, standing at its own last step where the replay stands on another's
EMBED_INLINE void answer_stack(const RecordingAnswerCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long thread = 0ull;
    if (!EMBED_CALL(cli_recording_json_internal.replay_whole, CliRecordingJsonCtx, .json = json,
                    .id = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                     .object = args->arguments, .name = "threadId"),
                    .value = &thread))
    {
        thread = cli_replay_thread();
    }
    unsigned int at = g_cli_replay->at;
    for (unsigned int one = g_cli_replay->at + 1u; one > 0u; one -= 1u)
    {
        if (EMBED_CALL(cli_recording_serial_step_internal.replay_thread_get, CliRecordingSerialStepCtx,
                       .step = one - 1u) == thread)
        {
            at = one - 1u;
            break;
        }
    }
    const unsigned int count = EMBED_CALL(cli_recording_at_frames_internal.stack, CliRecordingAtFramesCtx, .at = at,
                                          .frames = s_standing, .room = 512u);
    mmgr_line_put(args->body, "{\"stackFrames\":[");
    for (unsigned int frame = 0u; frame < count; frame += 1u)
    {
        if (frame > 0u)
        {
            mmgr_line_ch(args->body, ',');
        }
        EMBED_CALL(cli_recording_serial_step_internal.replay_frame_write, CliRecordingSerialStepCtx, .out = args->body,
                   .serial = s_standing[frame][0], .step = s_standing[frame][1]);
    }
    mmgr_line_put(args->body, "],\"totalFrames\":");
    mmgr_line_decimal(args->body, count, 1u);
    mmgr_line_ch(args->body, '}');
}

EMBED_INLINE void answer_variables(const RecordingAnswerCtx *args)
{
    const CliJson *json = args->json;
    unsigned long long serial = 0ull;
    if (!EMBED_CALL(cli_recording_json_internal.replay_whole, CliRecordingJsonCtx, .json = json,
                    .id = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                     .object = args->arguments, .name = "variablesReference"),
                    .value = &serial) ||
        (serial >= g_cli_replay->recording.frame_most))
    {
        serial = 0ull;
    }
    CliRecordedValue *values = NULL;
    const unsigned int count =
        EMBED_CALL(cli_recording_at_internal.values, CliRecordingAtCtx, .serial = (unsigned int)serial,
                   .at = g_cli_replay->at, .taken = &values, .most = CLI_REPLAY_VALUES, .owner = &g_cli_debug_cells);
    mmgr_line_put(args->body, "{\"variables\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(args->body, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->body,
                   .text_bytes = (const unsigned char *)values[at].name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = values[at].name, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->body, ",\"value\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->body,
                   .text_bytes = (const unsigned char *)values[at].value,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = values[at].value, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(args->body, ",\"type\":\"\",\"variablesReference\":0}");
    }
    mmgr_line_put(args->body, "]}");
    g_cli_cells->persistent_buf_release(values, &g_cli_debug_cells);
}

// a name looked up in the frame, then in each module's frame standing, one whose caller is none
EMBED_INLINE int answer_evaluate(const RecordingAnswerCtx *args)
{
    const CliJson *json = args->json;
    const CliRecording *const recording = &g_cli_replay->recording;
    unsigned long long length = 0ull;
    const char *name = EMBED_CALL(answer_text, RecordingAnswerCtx, .json = json, .arguments = args->arguments,
                                  .name = "expression", .length = &length);
    const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = name,
                                               .at = 0ull, .size = length, .set = " \t\n\r");
    name += lead;
    length -= lead;
    length = EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = name, .at = length,
                        .set = " \t\n\r");
    const unsigned int serial =
        EMBED_CALL(cli_recording_json_internal.replay_frame_get, CliRecordingJsonCtx, .json = json,
                   .id = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                    .object = args->arguments, .name = "frameId"));
    const unsigned int count = EMBED_CALL(cli_recording_at_frames_internal.stack, CliRecordingAtFramesCtx,
                                          .at = g_cli_replay->at, .frames = s_standing, .room = 512u);
    for (unsigned int frame = 0u; frame <= count; frame += 1u)
    {
        const unsigned int one = (frame == 0u) ? serial : s_standing[frame - 1u][0];
        if ((frame > 0u) && !(recording->frames[one].seen && (recording->frames[one].parent == 0u)))
        {
            continue;
        }
        CliRecordedValue *values = NULL;
        const unsigned int value_count =
            EMBED_CALL(cli_recording_at_internal.values, CliRecordingAtCtx, .serial = one, .at = g_cli_replay->at,
                       .taken = &values, .most = CLI_REPLAY_VALUES, .owner = &g_cli_debug_cells);
        for (unsigned int at = 0u; at < value_count; at += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = values[at].name, .cap = CLI_CELLS_POOL}) == length) &&
                (memor.cmp(&(MemoriaCfg){.src = values[at].name, .other = name, .bytes = length}) == 0))
            {
                mmgr_line_put(args->body, "{\"result\":");
                EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->body,
                           .text_bytes = (const unsigned char *)values[at].value,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = values[at].value, .cap = CLI_CELLS_POOL}));
                mmgr_line_put(args->body, ",\"type\":\"\",\"variablesReference\":0}");
                g_cli_cells->persistent_buf_release(values, &g_cli_debug_cells);
                return 1;
            }
        }
        g_cli_cells->persistent_buf_release(values, &g_cli_debug_cells);
    }
    mmgr_line_put(args->err_msg, "a recorded run reads the names its frames contained, and ");
    mmgr_line_put_n(args->err_msg, name, length);
    mmgr_line_put(args->err_msg, " is none of them");
    return 0;
}

EMBED_INLINE int answer_exception(const RecordingAnswerCtx *args)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    for (unsigned int at = 0u; at < recording->raised_count; at += 1u)
    {
        if (recording->raised[at].step == g_cli_replay->at)
        {
            const char *const kind = EMBED_CALL(cli_recording_at_frames_internal.name, CliRecordingAtFramesCtx,
                                                .at = recording->raised[at].kind);
            const char *const message = EMBED_CALL(cli_recording_at_frames_internal.name, CliRecordingAtFramesCtx,
                                                   .at = recording->raised[at].message);
            mmgr_line_put(args->body, "{\"exceptionId\":");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->body,
                       .text_bytes = (const unsigned char *)kind,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(args->body, ",\"description\":");
            EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->body,
                       .text_bytes = (const unsigned char *)message,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(args->body, ",\"breakMode\":\"always\"}");
            return 1;
        }
    }
    mmgr_line_put(args->err_msg, "no exception was raised at this step");
    return 0;
}

// a move made: the client notified the run went on, then where it stopped
EMBED_INLINE void answer_move(const RecordingAnswerCtx *args)
{
    CliDap *dap = args->dap;
    unsigned int step = g_cli_replay->at;
    const char *const reason = EMBED_CALL(cli_recording_how_internal.replay_go, CliRecordingHowCtx,
                                          .how = args->command, .step_pointer = &step);
    g_cli_replay->at = step;
    char out_buf[256];
    VerbaLinea out = {out_buf, sizeof(out_buf), 0ull};
    mmgr_line_put(&out, "{\"type\":\"event\",\"event\":\"continued\",\"body\":{\"threadId\":");
    mmgr_line_decimal(&out, cli_replay_thread(), 1u);
    mmgr_line_put(&out, ",\"allThreadsContinued\":true}}");
    (void)EMBED_CALL(cli_dap_message.send, CliDapMessageCfg, .dap = dap, .message = out.out, .size = out.at);
    EMBED_CALL(cli_recording_dap_reason_internal.replay_stopped_send, CliRecordingDapReasonCtx, .dap = dap,
               .reason = reason);
    mmgr_line_put(args->body, "{\"allThreadsContinued\":true}");
}

// The history of a name being written: where it goes, the path of the frame's file, and whether one is written yet
typedef struct
{
    VerbaLinea *body;
    const char *path;
    int written;
} CliReplayHistory;

static void answer_change(const CliEachStepLineValueCfg *args)
{
    CliReplayHistory *const history = (CliReplayHistory *)args->context;
    mmgr_line_put(history->body, history->written ? ",{\"step\":" : "{\"step\":");
    history->written = 1;
    mmgr_line_decimal(history->body, args->step, 1u);
    mmgr_line_put(history->body, ",\"line\":");
    mmgr_line_decimal(history->body, args->line, 1u);
    mmgr_line_put(history->body, ",\"path\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = history->body,
               .text_bytes = (const unsigned char *)history->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = history->path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(history->body, ",\"value\":");
    if (args->value != NULL)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = history->body,
                   .text_bytes = (const unsigned char *)args->value,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(history->body, "null");
    }
    mmgr_line_put(history->body, (args->step < g_cli_replay->at) ? ",\"before\":true}" : ",\"before\":false}");
}

EMBED_INLINE void answer_history(const RecordingAnswerCtx *args)
{
    const CliJson *json = args->json;
    char name[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ull;
    const char *const text = EMBED_CALL(answer_text, RecordingAnswerCtx, .json = json, .arguments = args->arguments,
                                        .name = "name", .length = &length);
    VerbaLinea sought = {name, sizeof(name), 0ull};
    mmgr_line_put_n(&sought, text, length);
    mmgr_line_finish(&sought);
    const unsigned int serial =
        EMBED_CALL(cli_recording_json_internal.replay_frame_get, CliRecordingJsonCtx, .json = json,
                   .id = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                    .object = args->arguments, .name = "frameId"));
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_recording_relative_internal.replay_full, CliRecordingRelativeCtx,
               .relative = EMBED_CALL(cli_recording_serial_step_internal.replay_file_get, CliRecordingSerialStepCtx,
                                      .serial = serial),
               .full = path);
    CliReplayHistory history = {args->body, path, 0};
    mmgr_line_put(args->body, "{\"changes\":[");
    EMBED_CALL(cli_recording_serial_internal.history, CliRecordingSerialCtx, .serial = serial, .name = name,
               .each = answer_change, .context = &history);
    mmgr_line_put(args->body, "],\"at\":");
    mmgr_line_decimal(args->body, g_cli_replay->at, 1u);
    mmgr_line_ch(args->body, '}');
}

int cli_replay_answer(const CliRecordingDapCtx *args)
{
    const int ready = (g_cli_replay->recording.step_count > 0u);
    const int moves =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "continue", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "next", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "stepIn", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "stepOut", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "stepBack", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "reverseContinue", .cap = CLI_CELLS_POOL}));
    unsigned long long step = 0ull;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "threads", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(answer_threads, RecordingAnswerCtx, .body = args->body);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "pause", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(args->body, "null");
        return 1;
    }
    if (!ready)
    {
        mmgr_line_put(args->err_msg, "the run is still being recorded");
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "stackTrace", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(answer_stack, RecordingAnswerCtx, .json = args->json, .arguments = args->arguments,
                   .body = args->body);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "scopes", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_put(args->body,
                      "{\"scopes\":[{\"name\":\"Locals\",\"presentationHint\":\"locals\",\"variablesReference\":");
        mmgr_line_decimal(
            args->body,
            EMBED_CALL(cli_recording_json_internal.replay_frame_get, CliRecordingJsonCtx, .json = args->json,
                       .id = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                        .object = args->arguments, .name = "frameId")),
            1u);
        mmgr_line_put(args->body, ",\"expensive\":false}]}");
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "variables", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(answer_variables, RecordingAnswerCtx, .json = args->json, .arguments = args->arguments,
                   .body = args->body);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "evaluate", .cap = CLI_CELLS_POOL}))
    {
        return EMBED_CALL(answer_evaluate, RecordingAnswerCtx, .json = args->json, .arguments = args->arguments,
                          .body = args->body, .err_msg = args->err_msg);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "exceptionInfo", .cap = CLI_CELLS_POOL}))
    {
        return EMBED_CALL(answer_exception, RecordingAnswerCtx, .body = args->body, .err_msg = args->err_msg);
    }
    if (moves)
    {
        EMBED_CALL(answer_move, RecordingAnswerCtx, .dap = args->dap, .command = args->command, .body = args->body);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "orior/history", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(answer_history, RecordingAnswerCtx, .json = args->json, .arguments = args->arguments,
                   .body = args->body);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->command, .other = "orior/goto", .cap = CLI_CELLS_POOL}))
    {
        if (!EMBED_CALL(cli_recording_json_internal.replay_whole, CliRecordingJsonCtx, .json = args->json,
                        .id = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->arguments, .name = "step"),
                        .value = &step) ||
            (step >= g_cli_replay->recording.step_count))
        {
            mmgr_line_put(args->err_msg, "no such step");
            return 0;
        }
        g_cli_replay->at = (unsigned int)step;
        EMBED_CALL(cli_recording_dap_reason_internal.replay_stopped_send, CliRecordingDapReasonCtx, .dap = args->dap,
                   .reason = "goto");
        mmgr_line_put(args->body, "null");
        return 1;
    }
    mmgr_line_put(args->err_msg, "a recorded run does not take ");
    mmgr_line_put(args->err_msg, args->command);
    return 0;
}
