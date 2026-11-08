// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_read.c: a recorded run read from the recorder's lines, its steps, frames, files, threads and raises, and
// what the replay requests of it: a frame's last step, its names' values at a step, the frames standing at a step, and
// a name's history through the run
#include "../command_line/pool.h"
#include "../text/text.h"
#include "recording_internal.h"

// The counts a recording's lines give by their types, read off the text before the lines are read whole: the steps,
// the frames by the largest number a call or a line names, the files by the largest number a file record names, the
// threads and the raises
typedef struct
{
    unsigned int steps;
    unsigned int frames;
    unsigned int files;
    unsigned int threads;
    unsigned int raises;
} ReadCounts;
CliReplay *g_cli_replay;

// the most bytes of one line of the recording read
#define CLI_RECORDING_LINE_TEXT (1ULL << 20u)

// the line read last, in a tree of its own sized to it, given back as the next is read and as the recording is let go
static CliJson s_json;

// each thread's stack of frames as the recording is read, in a cell of its own of `s_capacities` frames, its depth, and
// whether it has one; each cell given back once the recording is read
static unsigned int *s_stacks[CLI_RECORDING_THREADS];
static unsigned int s_capacities[CLI_RECORDING_THREADS];
static unsigned int s_depths[CLI_RECORDING_THREADS];
static int s_has_stack[CLI_RECORDING_THREADS];

// the names of a recording with no capacity: one empty name
static char s_no_names[1] = "";

const char *cli_recording_name(const CliRecordingAtFramesCtx *args)
{
    return g_cli_replay->recording.names + args->at;
}

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
    ReadCounts *counts;
    unsigned char **at_u8s;
    unsigned long long bytes;
    CliRecording *recording;
    unsigned int record;
    unsigned int at_u32;
    int slashed;
    const char *word_text;
    unsigned int thread;
    unsigned int serial;
    int *found;
} RecordingReadCtx;
// the whole number written at `at` of `size` bytes of `text`, read up to its first letter that is no digit: the
// number, all ones where none is written or it passes an unsigned int
EMBED_INLINE unsigned int read_digits(const RecordingReadCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long at = args->at;
    unsigned long long value = 0ULL;
    unsigned long long digits = 0ULL;
    unsigned long long end = at;
    value = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = text, .at = at, .size = size, .end = &end);
    digits = end - at;
    return ((digits == 0ULL) || (value > 0xffffffffULL)) ? 0xffffffffu : (unsigned int)value;
}

// the counts of `size` bytes of `text`: each line's type is the word its array opens with, and the number after it
// where the type takes one
EMBED_INLINE void read_on_count(const RecordingReadCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    ReadCounts *counts = args->counts;
    static const char *const s_types[5] = {"[\"line\",", "[\"call\",", "[\"file\",", "[\"thread\",", "[\"raise\","};
    *counts = (ReadCounts){0u, 0u, 0u, 0u, 0u};
    for (unsigned long long at = 0ULL; at < size;)
    {
        const unsigned long long end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size).stop;
        for (unsigned int type = 0u; type < 5u; type += 1u)
        {
            const unsigned long long length =
                cellul.len(&(CatenaFinitaCfg){.src = s_types[type], .cap = CLI_POOL_BYTES});
            if ((end - at < length) ||
                (memor.cmp(&(MemoriaCfg){.src = text + at, .other = s_types[type], .bytes = (size_t)length}) != 0))
            {
                continue;
            }
            const unsigned int number =
                EMBED_CALL(read_digits, RecordingReadCtx, .text = text, .size = end, .at = at + length);
            const unsigned int named = (number == 0xffffffffu) ? 0u : (number + 1u);
            counts->steps += (type == 0u) ? 1u : 0u;
            counts->frames = ((type <= 1u) && (named > counts->frames)) ? named : counts->frames;
            counts->files = ((type == 2u) && (named > counts->files)) ? named : counts->files;
            counts->threads += (type == 3u) ? 1u : 0u;
            counts->raises += (type == 4u) ? 1u : 0u;
            break;
        }
        at = end + 1ULL;
    }
}

// the next part of a cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary: its start
EMBED_INLINE void *read_part(const RecordingReadCtx *args)
{
    unsigned char **at = args->at_u8s;
    void *const part = *at;
    *at += (args->bytes + 7ULL) & ~7ULL;
    return part;
}

// the recording's capacity taken for the counts of its `size` bytes of `text` and its parts set into it, the frames and
// files zero and the names one empty name: 1, or 0 with the recording containing no capacity
EMBED_INLINE int read_work_state_alloc(const RecordingReadCtx *args)
{
    ReadCounts counts;
    EMBED_CALL(read_on_count, RecordingReadCtx, .text = args->text, .size = args->size, .counts = &counts);
    const unsigned long long bytes =
        ((unsigned long long)counts.steps * (sizeof(CliRecordedStep) + sizeof(unsigned int))) +
        ((unsigned long long)counts.frames * sizeof(CliRecordedFrame)) +
        ((unsigned long long)counts.files * sizeof(unsigned int)) +
        ((unsigned long long)counts.threads * (sizeof(unsigned long long) + sizeof(unsigned int))) +
        ((unsigned long long)counts.raises * sizeof(CliRecordedRaise)) + args->size + 1ULL + (8ULL * 8ULL);
    unsigned char *const cell = g_cli_guard->persistent_buf_alloc(bytes, g_cli_debug_owner);
    if (cell == NULL)
    {
        return 0;
    }
    unsigned char *at = cell;
    args->recording->capacity = cell;
    args->recording->steps = EMBED_CALL(read_part, RecordingReadCtx, .at_u8s = &at,
                                        .bytes = (unsigned long long)counts.steps * sizeof(CliRecordedStep));
    args->recording->step_max = counts.steps;
    args->recording->by_frame = EMBED_CALL(read_part, RecordingReadCtx, .at_u8s = &at,
                                           .bytes = (unsigned long long)counts.steps * sizeof(unsigned int));
    args->recording->frames = EMBED_CALL(read_part, RecordingReadCtx, .at_u8s = &at,
                                         .bytes = (unsigned long long)counts.frames * sizeof(CliRecordedFrame));
    args->recording->frame_max = counts.frames;
    args->recording->files = EMBED_CALL(read_part, RecordingReadCtx, .at_u8s = &at,
                                        .bytes = (unsigned long long)counts.files * sizeof(unsigned int));
    args->recording->file_max = counts.files;
    args->recording->thread_ids = EMBED_CALL(read_part, RecordingReadCtx, .at_u8s = &at,
                                             .bytes = (unsigned long long)counts.threads * sizeof(unsigned long long));
    args->recording->thread_names = EMBED_CALL(read_part, RecordingReadCtx, .at_u8s = &at,
                                               .bytes = (unsigned long long)counts.threads * sizeof(unsigned int));
    args->recording->thread_max = counts.threads;
    args->recording->raised = EMBED_CALL(read_part, RecordingReadCtx, .at_u8s = &at,
                                         .bytes = (unsigned long long)counts.raises * sizeof(CliRecordedRaise));
    args->recording->raised_max = counts.raises;
    args->recording->names = EMBED_CALL(read_part, RecordingReadCtx, .at_u8s = &at, .bytes = args->size + 1ULL);
    args->recording->names_max = args->size + 1ULL;
    memor.set(&(MemoriaCfg){
        .dst = args->recording->frames, .val = 0, .bytes = (size_t)counts.frames * sizeof(CliRecordedFrame)});
    memor.set(
        &(MemoriaCfg){.dst = args->recording->files, .val = 0, .bytes = (size_t)counts.files * sizeof(unsigned int)});
    args->recording->names[0] = '\0';
    args->recording->used = 1u;
    return 1;
}

// the line of the recording that starts at `at` read: its array, CLI_JSON_NONE where it is none
EMBED_INLINE unsigned int read_record(const RecordingReadCtx *args)
{
    unsigned long long at = args->at;
    const CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long end =
        EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = recording->text, .at = at, .size = recording->size).stop;
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int root =
        (end - at <= CLI_RECORDING_LINE_TEXT)
            ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &s_json,
                         .text = recording->text + at, .size = end - at, .nodes_more = 0u, .text_more = 0ULL,
                         .err_msg = &unread, .owner = g_cli_debug_owner)
            : CLI_JSON_NONE;
    return EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_json, .node = root,
                      .type = CFG_JSON_ARRAY)
               ? root
               : CLI_JSON_NONE;
}

const CliJson *cli_recording_record(const CliRecordingCtx *args)
{
    unsigned int *record = args->record;
    *record = EMBED_CALL(read_record, RecordingReadCtx, .at = args->at_u64);
    return &s_json;
}

// the whole number at place `at` of a record, 0 where it contains none
EMBED_INLINE unsigned long long read_number(const RecordingReadCtx *args)
{
    const unsigned int node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                         .json_read = &s_json, .array = args->record, .index = args->at_u32);
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_json, .node = node,
                    .type = CFG_JSON_NUMBER))
    {
        return 0ULL;
    }
    const char *const text = s_json.text + s_json.node[node].text;
    unsigned long long value = 0ULL;
    for (unsigned long long letter = 0ULL; letter < s_json.node[node].length; letter += 1ULL)
    {
        const unsigned long long digit = (unsigned long long)(text[letter] - '0');
        if ((text[letter] < '0') || (text[letter] > '9') || (value > (~0ULL - digit) / 10ULL))
        {
            return 0ULL;
        }
        value = (value * 10ULL) + digit;
    }
    return value;
}

// the text at place `at` of a record put in the names: its place there, the names' first, empty, where it contains none
EMBED_INLINE unsigned int read_by_name(const RecordingReadCtx *args)
{
    CliRecording *const recording = &g_cli_replay->recording;
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                      .json_read = &s_json, .array = args->record, .index = args->at_u32),
                   .length_pointer = &length);
    if ((text == NULL) || (recording->used + length + 1ULL > recording->names_max))
    {
        return 0u;
    }
    const unsigned int place = recording->used;
    for (unsigned long long letter = 0ULL; letter < length; letter += 1ULL)
    {
        recording->names[place + letter] = (args->slashed && (text[letter] == '\\')) ? '/' : text[letter];
    }
    recording->names[place + length] = '\0';
    recording->used += (unsigned int)length + 1u;
    return place;
}

// whether the text at place `at` of a record is `word`
EMBED_INLINE int read_type(const RecordingReadCtx *args)
{
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                      .json_read = &s_json, .array = args->record, .index = 0u),
                   .length_pointer = NULL);
    return (text != NULL) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = args->word_text, .cap = CLI_POOL_BYTES}));
}

// a call read: the frame stored by its number, its thread's stack grown by it where it is not its top
EMBED_INLINE void read_call(const RecordingReadCtx *args)
{
    CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long serial = EMBED_CALL(read_number, RecordingReadCtx, .record = args->record, .at_u32 = 1u);
    if (serial >= recording->frame_max)
    {
        return;
    }
    CliRecordedFrame *const frame = &recording->frames[serial];
    const unsigned int count = frame->seen ? frame->count : 0u;
    const unsigned int last = frame->seen ? frame->last : CLI_RECORDING_NO_STEP;
    *frame = (CliRecordedFrame){
        EMBED_CALL(read_by_name, RecordingReadCtx, .record = args->record, .at_u32 = 4u, .slashed = 0),
        (unsigned int)EMBED_CALL(read_number, RecordingReadCtx, .record = args->record, .at_u32 = 3u),
        (unsigned int)EMBED_CALL(read_number, RecordingReadCtx, .record = args->record, .at_u32 = 2u),
        (unsigned int)EMBED_CALL(read_number, RecordingReadCtx, .record = args->record, .at_u32 = 5u),
        0u,
        count,
        last,
        1};
    const unsigned int thread = frame->thread;
    if (thread >= CLI_RECORDING_THREADS)
    {
        return;
    }
    if (!s_has_stack[thread])
    {
        s_stacks[thread] = g_cli_guard->persistent_buf_alloc(
            (unsigned long long)CLI_RECORDING_DEPTH * sizeof(unsigned int), g_cli_debug_owner);
        s_capacities[thread] = (s_stacks[thread] != NULL) ? CLI_RECORDING_DEPTH : 0u;
        s_has_stack[thread] = 1;
    }
    if ((s_stacks[thread] != NULL) && (s_depths[thread] < s_capacities[thread]) &&
        ((s_depths[thread] == 0u) || (s_stacks[thread][s_depths[thread] - 1u] != serial)))
    {
        s_stacks[thread][s_depths[thread]] = (unsigned int)serial;
        s_depths[thread] += 1u;
    }
}

// the place of `serial` in a thread's stack from its top, one past it; the stack's depth where it is not there
EMBED_INLINE unsigned int read_position(const RecordingReadCtx *args)
{
    int *found = args->found;
    for (unsigned int at = s_depths[args->thread]; at > 0u; at -= 1u)
    {
        if (s_stacks[args->thread][at - 1u] == args->serial)
        {
            *found = 1;
            return at;
        }
    }
    *found = 0;
    return s_depths[args->thread];
}

// a line read: a step of its frame, as deep as the frame stands in its thread's stack
EMBED_INLINE void read_line(const RecordingReadCtx *args)
{
    CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long serial = EMBED_CALL(read_number, RecordingReadCtx, .record = args->record, .at_u32 = 1u);
    if ((serial >= recording->frame_max) || (recording->step_count == recording->step_max))
    {
        recording->cut = recording->cut || (recording->step_count == recording->step_max);
        return;
    }
    CliRecordedFrame *const frame = &recording->frames[serial];
    const unsigned int thread = frame->seen ? frame->thread : 0u;
    int found = 0;
    const unsigned int depth = ((thread < CLI_RECORDING_THREADS) && s_has_stack[thread])
                                   ? EMBED_CALL(read_position, RecordingReadCtx, .thread = thread,
                                                .serial = (unsigned int)serial, .found = &found)
                                   : 1u;
    if (!frame->seen)
    {
        *frame = (CliRecordedFrame){0u, 0u, 0u, 0u, 0u, 0u, CLI_RECORDING_NO_STEP, 1};
    }
    frame->count += 1u;
    frame->last = recording->step_count;
    recording->steps[recording->step_count] = (CliRecordedStep){
        args->at, (unsigned int)serial,
        (unsigned int)EMBED_CALL(read_number, RecordingReadCtx, .record = args->record, .at_u32 = 2u), depth};
    recording->step_count += 1u;
}

// a return read: its frame and every frame above it taken off its thread's stack
EMBED_INLINE void read_return(const RecordingReadCtx *args)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long serial = EMBED_CALL(read_number, RecordingReadCtx, .record = args->record, .at_u32 = 1u);
    const unsigned int thread =
        ((serial < recording->frame_max) && recording->frames[serial].seen) ? recording->frames[serial].thread : 0u;
    int found = 0;
    if ((thread < CLI_RECORDING_THREADS) && s_has_stack[thread])
    {
        const unsigned int place = EMBED_CALL(read_position, RecordingReadCtx, .thread = thread,
                                              .serial = (unsigned int)serial, .found = &found);
        s_depths[thread] = found ? place - 1u : s_depths[thread];
    }
}

// a raise read: an exception passing up through frames is raised again in each, and the first is where it began
EMBED_INLINE void read_raise(const RecordingReadCtx *args)
{
    CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long serial = EMBED_CALL(read_number, RecordingReadCtx, .record = args->record, .at_u32 = 1u);
    const CliRecordedFrame *const frame = (serial < recording->frame_max) ? &recording->frames[serial] : NULL;
    unsigned int step;
    if ((frame != NULL) && frame->seen && (frame->last != CLI_RECORDING_NO_STEP))
    {
        step = frame->last;
    }
    else if (recording->step_count > 0u)
    {
        step = recording->step_count - 1u;
    }
    else
    {
        step = 0u;
    }
    const unsigned int type =
        EMBED_CALL(read_by_name, RecordingReadCtx, .record = args->record, .at_u32 = 3u, .slashed = 0);
    const unsigned int message =
        EMBED_CALL(read_by_name, RecordingReadCtx, .record = args->record, .at_u32 = 4u, .slashed = 0);
    if (recording->raised_count > 0u)
    {
        const CliRecordedRaise *const last = &recording->raised[recording->raised_count - 1u];
        const unsigned int here = (step < recording->step_count) ? recording->steps[step].depth : 0u;
        if ((cellul.eq(&(CatenaFinitaCfg){
                .src = recording->names + last->type, .other = recording->names + type, .cap = CLI_POOL_BYTES})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = recording->names + last->message,
                                          .other = recording->names + message,
                                          .cap = CLI_POOL_BYTES})) &&
            (last->step < recording->step_count) && (recording->steps[last->step].depth > here))
        {
            return;
        }
    }
    if (recording->raised_count < recording->raised_max)
    {
        recording->raised[recording->raised_count] = (CliRecordedRaise){step, type, message};
        recording->raised_count += 1u;
    }
}

void cli_recording_let_go(void)
{
    if (g_cli_replay == NULL)
    {
        return;
    }
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    CliRecording *const recording = &g_cli_replay->recording;
    if (recording->text != NULL)
    {
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)((const unsigned char *)recording->text),
                                            (g_cli_debug_owner));
        recording->text = NULL;
    }
    if (recording->path[0] != '\0')
    {
        (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = recording->path);
        recording->path[0] = '\0';
    }
    recording->size = 0ULL;
    g_cli_guard->persistent_buf_release(recording->capacity, g_cli_debug_owner);
    recording->capacity = NULL;
    recording->steps = NULL;
    recording->step_count = 0u;
    recording->step_max = 0u;
    recording->frames = NULL;
    recording->frame_max = 0u;
    recording->by_frame = NULL;
    recording->files = NULL;
    recording->file_count = 0u;
    recording->file_max = 0u;
    recording->thread_ids = NULL;
    recording->thread_names = NULL;
    recording->thread_count = 0u;
    recording->thread_max = 0u;
    recording->raised = NULL;
    recording->raised_count = 0u;
    recording->raised_max = 0u;
    recording->names = s_no_names;
    recording->names_max = 0ULL;
    recording->used = 1u;
    recording->has_code = 0;
    recording->code = 0LL;
    recording->cut = 0;
}

// a record of the recording read by its type
EMBED_INLINE void read_store(const RecordingReadCtx *args)
{
    CliRecording *const recording = &g_cli_replay->recording;
    if (EMBED_CALL(read_type, RecordingReadCtx, .record = args->record, .word_text = "file"))
    {
        const unsigned long long file = EMBED_CALL(read_number, RecordingReadCtx, .record = args->record, .at_u32 = 1u);
        for (; (recording->file_count <= file) && (recording->file_count < recording->file_max);
             recording->file_count += 1u)
        {
            recording->files[recording->file_count] = 0u;
        }
        if (file < recording->file_max)
        {
            recording->files[file] =
                EMBED_CALL(read_by_name, RecordingReadCtx, .record = args->record, .at_u32 = 2u, .slashed = 1);
        }
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .record = args->record, .word_text = "thread") &&
             (recording->thread_count < recording->thread_max))
    {
        recording->thread_ids[recording->thread_count] =
            EMBED_CALL(read_number, RecordingReadCtx, .record = args->record, .at_u32 = 1u);
        recording->thread_names[recording->thread_count] =
            EMBED_CALL(read_by_name, RecordingReadCtx, .record = args->record, .at_u32 = 2u, .slashed = 0);
        recording->thread_count += 1u;
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .record = args->record, .word_text = "call"))
    {
        read_call(args);
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .record = args->record, .word_text = "line"))
    {
        read_line(args);
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .record = args->record, .word_text = "return"))
    {
        read_return(args);
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .record = args->record, .word_text = "raise"))
    {
        read_raise(args);
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .record = args->record, .word_text = "cut"))
    {
        recording->cut = 1;
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .record = args->record, .word_text = "end"))
    {
        const unsigned int code = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = &s_json, .array = args->record, .index = 1u);
        long long value = 0LL;
        int whole = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_json,
                               .node = code, .type = CFG_JSON_NUMBER);
        const char *const text = whole ? s_json.text + s_json.node[code].text : "";
        const unsigned long long length = whole ? s_json.node[code].length : 0ULL;
        const int negative = (length > 0ULL) && (text[0] == '-');
        unsigned long long letter = negative ? 1ULL : 0ULL;
        while (whole && (letter < length))
        {
            whole = (text[letter] >= '0') && (text[letter] <= '9') && (value < 922337203685477580LL);
            value = (value * 10LL) + (long long)(text[letter] - '0');
            letter += 1ULL;
        }
        recording->has_code = whole && (length > (negative ? 1ULL : 0ULL));
        recording->code = negative ? -value : value;
    }
}

void cli_recording_read(const CliRecordingPathCtx *args)
{
    if (!cli_replay_lock())
    {
        return;
    }
    CliRecording *const recording = &g_cli_replay->recording;
    cli_recording_let_go();
    for (unsigned int thread = 0u; thread < CLI_RECORDING_THREADS; thread += 1u)
    {
        g_cli_guard->persistent_buf_release(s_stacks[thread], g_cli_debug_owner);
        s_stacks[thread] = NULL;
        s_capacities[thread] = 0u;
        s_depths[thread] = 0u;
        s_has_stack[thread] = 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = recording->path,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    // the recording's file saved as long as it is replayed, in a persistent cell of its size and the slack
    unsigned long long size = 0ULL;
    unsigned char *bytes =
        EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = args->path, .size_pointer = &size)
            ? g_cli_guard->persistent_buf_alloc(size + CLI_FILE_SLACK, g_cli_debug_owner)
            : NULL;
    if ((bytes != NULL) && !EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = args->path,
                                       .from = 0ULL, .bytes_bytes = bytes, .count_u64 = size, .got = &size))
    {
        g_cli_guard->persistent_buf_release(bytes, g_cli_debug_owner);
        bytes = NULL;
    }
    recording->text = (const char *)bytes;
    recording->size = 0ULL;
    if (bytes != NULL)
    {
        memor.set(&(MemoriaCfg){.dst = bytes + size, .val = 0, .bytes = CLI_FILE_SLACK});
        recording->size = size;
        if (!EMBED_CALL(read_work_state_alloc, RecordingReadCtx, .recording = recording, .text = recording->text,
                        .size = recording->size))
        {
            cli_recording_let_go();
            return;
        }
    }
    unsigned long long at = 0ULL;
    while (at < recording->size)
    {
        const unsigned long long next =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = recording->text, .at = at, .size = recording->size)
                .next;
        const unsigned int record = EMBED_CALL(read_record, RecordingReadCtx, .at = at);
        if (record != CLI_JSON_NONE)
        {
            EMBED_CALL(read_store, RecordingReadCtx, .record = record, .at = at);
        }
        at = next;
    }
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    for (unsigned int thread = 0u; thread < CLI_RECORDING_THREADS; thread += 1u)
    {
        g_cli_guard->persistent_buf_release(s_stacks[thread], g_cli_debug_owner);
        s_stacks[thread] = NULL;
        s_capacities[thread] = 0u;
        s_depths[thread] = 0u;
        s_has_stack[thread] = 0;
    }
    // each frame's steps in turn, in the order they ran
    unsigned int first = 0u;
    for (unsigned int serial = 0u; serial < recording->frame_max; serial += 1u)
    {
        recording->frames[serial].first = first;
        recording->frames[serial].last = first;
        first += recording->frames[serial].count;
    }
    for (unsigned int step = 0u; step < recording->step_count; step += 1u)
    {
        CliRecordedFrame *const frame = &recording->frames[recording->steps[step].frame];
        recording->by_frame[frame->last] = step;
        frame->last += 1u;
    }
}
