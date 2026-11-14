// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_replay.c: where the replay goes and what it stops at: a step forward or back, a stop at a breakpoint or a
// raise, a frame's last step, the stop reported, the paths it compares, and a frame as a stack frame
#include "../text/text.h"
#include "recording_internal.h"

// a path of the tree as the replay compares it: whole, with one type of slash, and without case on Windows, into `key`
void cli_replay_key(const CliRecordingPathCtx *args)
{
    VerbaLinea out = {args->key, CLI_PATH_MAX_LEN, 0ULL};
    (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &out, .text = args->path, .from = '\\',
                     .to = '/');
#if defined(_WIN32)
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->key, .size = out.at);
#endif
    mmgr_line_finish(&out);
}

// the full path of the file at tree path `relative`, joined a part at a time, into `full`
void cli_replay_full(const CliRecordingRelativeCtx *args)
{
    char joined[CLI_PATH_MAX_LEN];
    memor.cpy(
        &(MemoriaCfg){.dst = args->full,
                      .src = g_cli_replay->root,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_replay->root, .cap = CLI_POOL_BYTES}) + 1u});
    for (const char *part = args->relative; *part != '\0';)
    {
        const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES, .byte = '/'});
        const embed_word length = (slash != NULL) ? (embed_word)(slash - part)
                                                  : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES});
        if (length > 0u)
        {
            char one[CLI_PATH_MAX_LEN];
            VerbaLinea line = {one, sizeof(one), 0ULL};
            mmgr_line_put_n(&line, part, length);
            mmgr_line_finish(&line);
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->full, .relative = one, .joined = joined);
            memor.cpy(&(MemoriaCfg){.dst = args->full, .src = joined, .bytes = CLI_PATH_MAX_LEN});
        }
        part += length + ((slash != NULL) ? 1u : 0u);
    }
}

// the tree path of the file a frame's lines are in into `path_out`, "" where the recording names none
const char *cli_replay_file_get(const CliRecordingSerialStepCtx *args)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    char *const out = args->path_out;
    out[0] = '\0';
    if (args->serial >= recording->frame_max)
    {
        return out;
    }
    const CliRecordedFrame frame =
        EMBED_CALL(cli_recording_index_internal.frame, CliRecordingIndexCtx, .index = args->serial);
    if (frame.seen && (frame.file < recording->file_count))
    {
        (void)EMBED_CALL(cli_recording_at_frames_internal.name, CliRecordingAtFramesCtx,
                         .at = EMBED_CALL(cli_recording_index_internal.file, CliRecordingIndexCtx, .index = frame.file),
                         .name_out = out, .name_capacity = CLI_PATH_MAX_LEN);
    }
    return out;
}

// the thread a step's frame ran on, 0 where the recording names none
unsigned int cli_replay_thread_get(const CliRecordingSerialStepCtx *args)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    const unsigned int serial =
        EMBED_CALL(cli_recording_index_internal.step, CliRecordingIndexCtx, .index = args->step).frame;
    const CliRecordedFrame frame = (serial < recording->frame_max) ? EMBED_CALL(cli_recording_index_internal.frame,
                                                                                CliRecordingIndexCtx, .index = serial)
                                                                   : (CliRecordedFrame){0u, 0u, 0u, 0u, 0u, 0u, 0u, 0};
    return frame.seen ? frame.thread : 0u;
}

// the thread the replay is on
unsigned int cli_replay_thread(void)
{
    return (g_cli_replay->recording.step_count == 0u) ? 1u
                                                      : EMBED_CALL(cli_recording_serial_step_internal.replay_thread_get,
                                                                   CliRecordingSerialStepCtx, .step = g_cli_replay->at);
}

unsigned int cli_recording_last_get(const CliRecordingAtCtx *args)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    if (args->serial >= recording->frame_max)
    {
        return CLI_RECORDING_NO_STEP;
    }
    const CliRecordedFrame frame =
        EMBED_CALL(cli_recording_index_internal.frame, CliRecordingIndexCtx, .index = args->serial);
    if (frame.count == 0u)
    {
        return CLI_RECORDING_NO_STEP;
    }
    unsigned int low = 0u;
    unsigned int high = frame.count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (EMBED_CALL(cli_recording_index_internal.by_frame, CliRecordingIndexCtx, .index = frame.first + middle) <=
            args->at)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return (low == 0u) ? CLI_RECORDING_NO_STEP
                       : EMBED_CALL(cli_recording_index_internal.by_frame, CliRecordingIndexCtx,
                                    .index = frame.first + low - 1u);
}

int cli_replay_at_breakpoint(const CliRecordingSerialStepCtx *args)
{
    char full[CLI_PATH_MAX_LEN];
    char key[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    const CliRecordedStep step =
        EMBED_CALL(cli_recording_index_internal.step, CliRecordingIndexCtx, .index = args->step);
    EMBED_CALL(cli_recording_relative_internal.replay_full, CliRecordingRelativeCtx,
               .relative = EMBED_CALL(cli_recording_serial_step_internal.replay_file_get, CliRecordingSerialStepCtx,
                                      .serial = step.frame, .path_out = file),
               .full = full);
    EMBED_CALL(cli_recording_path_internal.replay_key, CliRecordingPathCtx, .path = full, .key = key);
    for (unsigned int file = 0u; file < g_cli_replay->break_file_count; file += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_replay->break_files[file], .other = key, .cap = CLI_POOL_BYTES}))
        {
            continue;
        }
        for (unsigned int at = 0u; at < g_cli_replay->break_count[file]; at += 1u)
        {
            if (g_cli_replay->break_lines[file][at] == step.line)
            {
                return 1;
            }
        }
    }
    return 0;
}

int cli_replay_at_raise(const CliRecordingSerialStepCtx *args)
{
    for (unsigned int at = 0u; g_cli_replay->on_raised && (at < g_cli_replay->recording.raised_count); at += 1u)
    {
        if (EMBED_CALL(cli_recording_index_internal.raise, CliRecordingIndexCtx, .index = at).step == args->step)
        {
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    unsigned int step;
} RecordingReplayCtx;
// whether a step stops a continue: at a breakpoint, or at a raise where raises stop the replay; and why
EMBED_INLINE int replay_stops(const RecordingReplayCtx *args)
{
    unsigned int step = args->step;
    return EMBED_CALL(cli_recording_serial_step_internal.replay_at_breakpoint, CliRecordingSerialStepCtx,
                      .step = step) ||
           EMBED_CALL(cli_recording_serial_step_internal.replay_at_raise, CliRecordingSerialStepCtx, .step = step);
}

EMBED_INLINE const char *replay_reason(const RecordingReplayCtx *args)
{
    unsigned int step = args->step;
    return (EMBED_CALL(cli_recording_serial_step_internal.replay_at_raise, CliRecordingSerialStepCtx, .step = step) &&
            !EMBED_CALL(cli_recording_serial_step_internal.replay_at_breakpoint, CliRecordingSerialStepCtx,
                        .step = step))
               ? "exception"
               : "breakpoint";
}

// where `how` goes from where the replay is into `step`, and the reason it stops there
const char *cli_replay_go(const CliRecordingHowCtx *args)
{
    unsigned int *step = args->step_pointer;
    const unsigned int last = g_cli_replay->recording.step_count - 1u;
    const unsigned int here = g_cli_replay->at;
    const unsigned int thread =
        EMBED_CALL(cli_recording_serial_step_internal.replay_thread_get, CliRecordingSerialStepCtx, .step = here);
    const unsigned int depth = EMBED_CALL(cli_recording_index_internal.step, CliRecordingIndexCtx, .index = here).depth;
    const int forward = (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "continue", .cap = CLI_POOL_BYTES})) ||
                        (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "next", .cap = CLI_POOL_BYTES})) ||
                        (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "stepIn", .cap = CLI_POOL_BYTES})) ||
                        (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "stepOut", .cap = CLI_POOL_BYTES}));
    const int backward =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "reverseContinue", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "stepBack", .cap = CLI_POOL_BYTES}));
    if (!forward && !backward)
    {
        *step = here;
        return "pause";
    }
    const int continues =
        (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "continue", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "reverseContinue", .cap = CLI_POOL_BYTES}));
    for (unsigned int one = forward ? here + 1u : here; forward ? (one <= last) : (one > 0u);
         one = forward ? one + 1u : one - 1u)
    {
        const unsigned int at = forward ? one : one - 1u;
        const int same = (EMBED_CALL(cli_recording_serial_step_internal.replay_thread_get, CliRecordingSerialStepCtx,
                                     .step = at) == thread);
        const unsigned int at_depth =
            EMBED_CALL(cli_recording_index_internal.step, CliRecordingIndexCtx, .index = at).depth;
        int hit;
        if (continues)
        {
            hit = EMBED_CALL(replay_stops, RecordingReplayCtx, .step = at);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "next", .cap = CLI_POOL_BYTES}))
        {
            hit = same && (at_depth <= depth);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "stepIn", .cap = CLI_POOL_BYTES}))
        {
            hit = same;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = args->how, .other = "stepOut", .cap = CLI_POOL_BYTES}))
        {
            hit = same && (at_depth < depth);
        }
        else
        {
            hit = same && (at_depth <= depth);
        }
        if (hit)
        {
            *step = at;
            return continues ? EMBED_CALL(replay_reason, RecordingReplayCtx, .step = at) : "step";
        }
    }
    *step = forward ? last : 0u;
    return forward ? "end" : "start";
}

void cli_replay_stopped_send(const CliRecordingDapReasonCtx *args)
{
    CliDap *dap = args->dap;
    char out_buf[1024];
    VerbaLinea out = {out_buf, sizeof(out_buf), 0ULL};
    const int bound = (cellul.eq(&(CatenaFinitaCfg){.src = args->reason, .other = "end", .cap = CLI_POOL_BYTES})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = args->reason, .other = "start", .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&out, "{\"type\":\"event\",\"event\":\"stopped\",\"body\":{\"reason\":\"");
    mmgr_line_put(&out, bound ? "step" : args->reason);
    mmgr_line_put(&out, "\",\"description\":\"");
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->reason, .other = "end", .cap = CLI_POOL_BYTES}))
    {
        mmgr_line_put(&out, "the end of the recording");
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->reason, .other = "start", .cap = CLI_POOL_BYTES}))
    {
        mmgr_line_put(&out, "the start of the recording");
    }
    else
    {
        mmgr_line_put(&out, "step ");
        mmgr_line_decimal(&out, (unsigned long long)g_cli_replay->at + 1ULL, 1u);
        mmgr_line_put(&out, " of ");
        mmgr_line_decimal(&out, g_cli_replay->recording.step_count, 1u);
    }
    mmgr_line_put(&out, "\",\"threadId\":");
    mmgr_line_decimal(&out, cli_replay_thread(), 1u);
    mmgr_line_put(&out, ",\"allThreadsStopped\":true}}");
    (void)EMBED_CALL(cli_dap_message.send, CliDapMessageCfg, .dap = dap, .message = out.out, .size = out.at);
}

// a frame written as a stack frame, its line the step's, its source its file
void cli_replay_frame_write(const CliRecordingSerialStepCtx *args)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    const CliRecordedFrame frame =
        EMBED_CALL(cli_recording_index_internal.frame, CliRecordingIndexCtx, .index = args->serial);
    char full[CLI_PATH_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    char name[CLI_RECORDING_NAME_BYTES];
    file[0] = '\0';
    name[0] = '\0';
    // a frame the recording did not see reads as one of file 0, as a frame made empty does
    if (frame.file < recording->file_count)
    {
        (void)EMBED_CALL(cli_recording_at_frames_internal.name, CliRecordingAtFramesCtx,
                         .at = EMBED_CALL(cli_recording_index_internal.file, CliRecordingIndexCtx, .index = frame.file),
                         .name_out = file, .name_capacity = sizeof(file));
    }
    EMBED_CALL(cli_recording_relative_internal.replay_full, CliRecordingRelativeCtx, .relative = file, .full = full);
    if (frame.seen)
    {
        (void)EMBED_CALL(cli_recording_at_frames_internal.name, CliRecordingAtFramesCtx, .at = frame.name,
                         .name_out = name, .name_capacity = sizeof(name));
    }
    mmgr_line_put(args->out, "{\"id\":");
    mmgr_line_decimal(args->out, args->serial, 1u);
    mmgr_line_put(args->out, ",\"name\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(args->out, ",\"line\":");
    mmgr_line_decimal(
        args->out, EMBED_CALL(cli_recording_index_internal.step, CliRecordingIndexCtx, .index = args->step).line, 1u);
    mmgr_line_put(args->out, ",\"column\":1,\"source\":{\"name\":");
    embed_word end = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_POOL_BYTES});
    embed_word start = end;
    start = (embed_word)EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = full,
                                   .at = start, .set = "/\\");
    if (start == end)
    {
        mmgr_line_put(args->out, "null");
    }
    else
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)full + start, .length = end - start);
    }
    mmgr_line_put(args->out, ",\"path\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)full,
               .length = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(args->out, "}}");
}

// the frames located at a step, each its number and the step of its line then
static unsigned int s_stack[512][2];

// whether node `id` of `json` is a whole number at least 0 and below 2^63, and it into `value`
int cli_replay_integer(const CliRecordingJsonCtx *args)
{
    unsigned long long *value = args->value;
    const char *const text = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                        .json_read = args->json, .node = args->id, .type = CFG_JSON_NUMBER)
                                 ? args->json->text + args->json->node[args->id].text
                                 : NULL;
    const unsigned long long length = (text != NULL) ? args->json->node[args->id].length : 0ULL;
    int whole = (length > 0ULL) && (length <= 19ULL);
    *value = 0ULL;
    unsigned long long at = 0ULL;
    while (whole && (at < length))
    {
        whole = (text[at] >= '0') && (text[at] <= '9');
        *value = (*value * 10ULL) + (unsigned long long)(text[at] - '0');
        at += 1ULL;
    }
    return whole && (*value <= 9223372036854775807ULL);
}

// the frame `id` names, a node of the request's arguments, or the innermost where it names none or one not located
unsigned int cli_replay_frame_get(const CliRecordingJsonCtx *args)
{
    const unsigned int count = EMBED_CALL(cli_recording_at_frames_internal.stack, CliRecordingAtFramesCtx,
                                          .at = g_cli_replay->at, .frames = s_stack, .capacity = 512u);
    unsigned long long value = 0ULL;
    const int whole = EMBED_CALL(cli_recording_json_internal.replay_integer, CliRecordingJsonCtx, .json = args->json,
                                 .id = args->id, .value = &value);
    for (unsigned int at = 0u; whole && (at < count); at += 1u)
    {
        if (s_stack[at][0] == value)
        {
            return (unsigned int)value;
        }
    }
    return (count > 0u) ? s_stack[0][0] : 0u;
}
