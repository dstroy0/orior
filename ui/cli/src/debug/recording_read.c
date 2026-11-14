// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_read.c: a recorded run read from the recorder's lines, its steps, frames, files, threads and raises laid
// in one movable cell the replay owns with the lines they came from, and what the replay reads of it by index: a step,
// a frame, a step by frame, a file, a thread, a raise, a name and a line record
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

// The tables of a recording as it is read, in a cell of the read's own with the text the lines are read from in
// another: each table's start, and where each stands from the cell's start, which the recording's cell keeps; the
// tables and the text are written into the recording's movable cell as the read ends
typedef struct
{
    unsigned char *cell;
    unsigned long long bytes;
    CliRecordedStep *steps;
    unsigned int *by_frame;
    CliRecordedFrame *frames;
    unsigned int *files;
    unsigned long long *thread_ids;
    unsigned int *thread_names;
    CliRecordedRaise *raised;
    char *names;
    char *text;
    unsigned long long size;
} ReadWork;

// each thread's stack of frames as the recording is read, in a cell of its own of `s_capacities` frames, its depth, and
// whether it has one; each cell given back once the recording is read
static unsigned int *s_stacks[CLI_RECORDING_THREADS];
static unsigned int s_capacities[CLI_RECORDING_THREADS];
static unsigned int s_depths[CLI_RECORDING_THREADS];
static int s_has_stack[CLI_RECORDING_THREADS];

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
    ReadCounts *counts;
    unsigned char **at_u8s;
    unsigned long long *part_at;
    unsigned long long bytes;
    CliRecording *recording;
    CliJson *json;
    unsigned int record;
    unsigned int at_u32;
    int slashed;
    const char *word_text;
    unsigned int thread;
    unsigned int serial;
    int *found;
    void *dst;
    unsigned long long offset;
    ReadWork *work;
} RecordingReadCtx;

// `bytes` of the recording's cell at `offset` read into `dst`, which is left zero where the recording has no cell
EMBED_INLINE void part_read(const RecordingReadCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->dst, .val = 0, .bytes = (embed_word)args->bytes});
    if (g_cli_replay->recording.cell != NULL)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = g_cli_replay->recording.cell, .offset = args->offset, .dst = args->dst, .bytes = args->bytes});
    }
}

CliRecordedStep cli_recording_step(const CliRecordingIndexCtx *args)
{
    CliRecordedStep step;
    EMBED_CALL(part_read, RecordingReadCtx, .dst = &step, .bytes = sizeof(step),
               .offset = g_cli_replay->recording.steps_at + ((unsigned long long)args->index * sizeof(step)));
    return step;
}

CliRecordedFrame cli_recording_frame(const CliRecordingIndexCtx *args)
{
    CliRecordedFrame frame;
    EMBED_CALL(part_read, RecordingReadCtx, .dst = &frame, .bytes = sizeof(frame),
               .offset = g_cli_replay->recording.frames_at + ((unsigned long long)args->index * sizeof(frame)));
    return frame;
}

unsigned int cli_recording_by_frame(const CliRecordingIndexCtx *args)
{
    unsigned int step;
    EMBED_CALL(part_read, RecordingReadCtx, .dst = &step, .bytes = sizeof(step),
               .offset = g_cli_replay->recording.by_frame_at + ((unsigned long long)args->index * sizeof(step)));
    return step;
}

unsigned int cli_recording_file(const CliRecordingIndexCtx *args)
{
    unsigned int name;
    EMBED_CALL(part_read, RecordingReadCtx, .dst = &name, .bytes = sizeof(name),
               .offset = g_cli_replay->recording.files_at + ((unsigned long long)args->index * sizeof(name)));
    return name;
}

unsigned long long cli_recording_thread_id(const CliRecordingIndexCtx *args)
{
    unsigned long long id;
    EMBED_CALL(part_read, RecordingReadCtx, .dst = &id, .bytes = sizeof(id),
               .offset = g_cli_replay->recording.thread_ids_at + ((unsigned long long)args->index * sizeof(id)));
    return id;
}

unsigned int cli_recording_thread_name(const CliRecordingIndexCtx *args)
{
    unsigned int name;
    EMBED_CALL(part_read, RecordingReadCtx, .dst = &name, .bytes = sizeof(name),
               .offset = g_cli_replay->recording.thread_names_at + ((unsigned long long)args->index * sizeof(name)));
    return name;
}

CliRecordedRaise cli_recording_raise(const CliRecordingIndexCtx *args)
{
    CliRecordedRaise raised;
    EMBED_CALL(part_read, RecordingReadCtx, .dst = &raised, .bytes = sizeof(raised),
               .offset = g_cli_replay->recording.raised_at + ((unsigned long long)args->index * sizeof(raised)));
    return raised;
}

const char *cli_recording_name(const CliRecordingAtFramesCtx *args)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    char *const out = args->name_out;
    out[0] = '\0';
    if ((recording->cell == NULL) || (args->at >= recording->used) || (args->name_capacity == 0ULL))
    {
        return out;
    }
    const unsigned long long length = g_cli_guard->movable_buf_measure(
        &(CarcerMovableCfg){.cell = recording->cell, .offset = recording->names_at + args->at});
    const unsigned long long bytes = (length < args->name_capacity - 1ULL) ? length : args->name_capacity - 1ULL;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = recording->cell, .offset = recording->names_at + args->at, .dst = out, .bytes = bytes});
    out[bytes] = '\0';
    return out;
}

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
                (memor.cmp(&(MemoriaCfg){.src = text + at, .other = s_types[type], .bytes = (embed_word)length}) != 0))
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

// the next part of the work cell, `bytes` long, taken from `*at`, its start aligned to a word's boundary and written
// into `*part_at` as its place from the cell's start: its start
EMBED_INLINE void *read_part(const RecordingReadCtx *args)
{
    unsigned char **at = args->at_u8s;
    void *const part = *at;
    *args->part_at = (unsigned long long)(*at - args->work->cell);
    *at += (args->bytes + 7ULL) & ~7ULL;
    return part;
}

// the work cell taken for the counts of its `size` bytes of `text` and its parts set into it, the frames and files
// zero and the names one empty name, each part's place kept on the recording: 1, or 0 with the read holding no tables
EMBED_INLINE int read_work_alloc(const RecordingReadCtx *args)
{
    CliRecording *const recording = args->recording;
    ReadCounts counts;
    EMBED_CALL(read_on_count, RecordingReadCtx, .text = args->text, .size = args->size, .counts = &counts);
    const unsigned long long bytes =
        ((unsigned long long)counts.steps * (sizeof(CliRecordedStep) + sizeof(unsigned int))) +
        ((unsigned long long)counts.frames * sizeof(CliRecordedFrame)) +
        ((unsigned long long)counts.files * sizeof(unsigned int)) +
        ((unsigned long long)counts.threads * (sizeof(unsigned long long) + sizeof(unsigned int))) +
        ((unsigned long long)counts.raises * sizeof(CliRecordedRaise)) + args->size + 1ULL + (8ULL * 8ULL);
    // the tables last the read alone, which writes them into the recording's cell as it ends
    unsigned char *const cell = g_cli_guard->temporary_buf_alloc(bytes, g_cli_debug_owner);
    if (cell == NULL)
    {
        return 0;
    }
    unsigned char *at = cell;
    args->work->cell = cell;
    args->work->bytes = bytes;
    args->work->steps =
        EMBED_CALL(read_part, RecordingReadCtx, .work = args->work, .at_u8s = &at, .part_at = &recording->steps_at,
                   .bytes = (unsigned long long)counts.steps * sizeof(CliRecordedStep));
    recording->step_max = counts.steps;
    args->work->by_frame =
        EMBED_CALL(read_part, RecordingReadCtx, .work = args->work, .at_u8s = &at, .part_at = &recording->by_frame_at,
                   .bytes = (unsigned long long)counts.steps * sizeof(unsigned int));
    args->work->frames =
        EMBED_CALL(read_part, RecordingReadCtx, .work = args->work, .at_u8s = &at, .part_at = &recording->frames_at,
                   .bytes = (unsigned long long)counts.frames * sizeof(CliRecordedFrame));
    recording->frame_max = counts.frames;
    args->work->files =
        EMBED_CALL(read_part, RecordingReadCtx, .work = args->work, .at_u8s = &at, .part_at = &recording->files_at,
                   .bytes = (unsigned long long)counts.files * sizeof(unsigned int));
    recording->file_max = counts.files;
    args->work->thread_ids =
        EMBED_CALL(read_part, RecordingReadCtx, .work = args->work, .at_u8s = &at, .part_at = &recording->thread_ids_at,
                   .bytes = (unsigned long long)counts.threads * sizeof(unsigned long long));
    args->work->thread_names = EMBED_CALL(read_part, RecordingReadCtx, .work = args->work, .at_u8s = &at,
                                          .part_at = &recording->thread_names_at,
                                          .bytes = (unsigned long long)counts.threads * sizeof(unsigned int));
    recording->thread_max = counts.threads;
    args->work->raised =
        EMBED_CALL(read_part, RecordingReadCtx, .work = args->work, .at_u8s = &at, .part_at = &recording->raised_at,
                   .bytes = (unsigned long long)counts.raises * sizeof(CliRecordedRaise));
    recording->raised_max = counts.raises;
    args->work->names = EMBED_CALL(read_part, RecordingReadCtx, .work = args->work, .at_u8s = &at,
                                   .part_at = &recording->names_at, .bytes = args->size + 1ULL);
    recording->names_max = args->size + 1ULL;
    memor.set(&(MemoriaCfg){
        .dst = args->work->frames, .val = 0, .bytes = (embed_word)counts.frames * sizeof(CliRecordedFrame)});
    memor.set(
        &(MemoriaCfg){.dst = args->work->files, .val = 0, .bytes = (embed_word)counts.files * sizeof(unsigned int)});
    args->work->names[0] = '\0';
    recording->used = 1u;
    return 1;
}

// `size` bytes of `text`, one line of the recording, read into `json`, a tree sized to it: its array, CLI_JSON_NONE
// where the line is none or passes the most a line holds
EMBED_INLINE unsigned int record_read(const RecordingReadCtx *args)
{
    CliJson *const json = args->json;
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int root = (args->size <= CLI_RECORDING_LINE_TEXT)
                                  ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                               .json = json, .text = args->text, .size = args->size, .nodes_more = 0u,
                                               .text_more = 0ULL, .err_msg = &unread, .owner = g_cli_debug_owner)
                                  : CLI_JSON_NONE;
    return EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = root,
                      .type = CFG_JSON_ARRAY)
               ? root
               : CLI_JSON_NONE;
}

void cli_recording_record(const CliRecordingCtx *args)
{
    CliJson *const json = args->json;
    const CliRecording *const recording = &g_cli_replay->recording;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    *args->record = CLI_JSON_NONE;
    if ((recording->cell == NULL) || (args->at_u64 >= recording->size))
    {
        return;
    }
    // the line stands zero-ended in the recording's cell, copied into a cell of this call for the tree's read
    const unsigned long long length = g_cli_guard->movable_buf_measure(
        &(CarcerMovableCfg){.cell = recording->cell, .offset = recording->text_at + args->at_u64});
    char *const line =
        (length <= CLI_RECORDING_LINE_TEXT) ? g_cli_guard->temporary_buf_alloc(length + 1ULL, g_cli_debug_owner) : NULL;
    if (line == NULL)
    {
        return;
    }
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = recording->cell, .offset = recording->text_at + args->at_u64, .dst = line, .bytes = length});
    line[length] = '\0';
    *args->record = EMBED_CALL(record_read, RecordingReadCtx, .json = json, .text = line, .size = length);
    g_cli_guard->persistent_buf_release(line, g_cli_debug_owner);
}

// the whole number at place `at` of a record, 0 where it contains none
EMBED_INLINE unsigned long long read_number(const RecordingReadCtx *args)
{
    const CliJson *const json = args->json;
    const unsigned int node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                         .json_read = json, .array = args->record, .index = args->at_u32);
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = node,
                    .type = CFG_JSON_NUMBER))
    {
        return 0ULL;
    }
    const char *const text = json->text + json->node[node].text;
    unsigned long long value = 0ULL;
    for (unsigned long long letter = 0ULL; letter < json->node[node].length; letter += 1ULL)
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
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                      .json_read = args->json, .array = args->record, .index = args->at_u32),
                   .length_pointer = &length);
    if ((text == NULL) || (recording->used + length + 1ULL > recording->names_max))
    {
        return 0u;
    }
    const unsigned int place = recording->used;
    for (unsigned long long letter = 0ULL; letter < length; letter += 1ULL)
    {
        args->work->names[place + letter] = (args->slashed && (text[letter] == '\\')) ? '/' : text[letter];
    }
    args->work->names[place + length] = '\0';
    recording->used += (unsigned int)length + 1u;
    return place;
}

// whether the text at place `at` of a record is `word`
EMBED_INLINE int read_type(const RecordingReadCtx *args)
{
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                      .json_read = args->json, .array = args->record, .index = 0u),
                   .length_pointer = NULL);
    return (text != NULL) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = args->word_text, .cap = CLI_POOL_BYTES}));
}

// a call read: the frame stored by its number, its thread's stack grown by it where it is not its top
EMBED_INLINE void read_call(const RecordingReadCtx *args)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long serial =
        EMBED_CALL(read_number, RecordingReadCtx, .json = args->json, .record = args->record, .at_u32 = 1u);
    if (serial >= recording->frame_max)
    {
        return;
    }
    CliRecordedFrame *const frame = &args->work->frames[serial];
    const unsigned int count = frame->seen ? frame->count : 0u;
    const unsigned int last = frame->seen ? frame->last : CLI_RECORDING_NO_STEP;
    *frame = (CliRecordedFrame){EMBED_CALL(read_by_name, RecordingReadCtx, .work = args->work, .json = args->json,
                                           .record = args->record, .at_u32 = 4u, .slashed = 0),
                                (unsigned int)EMBED_CALL(read_number, RecordingReadCtx, .json = args->json,
                                                         .record = args->record, .at_u32 = 3u),
                                (unsigned int)EMBED_CALL(read_number, RecordingReadCtx, .json = args->json,
                                                         .record = args->record, .at_u32 = 2u),
                                (unsigned int)EMBED_CALL(read_number, RecordingReadCtx, .json = args->json,
                                                         .record = args->record, .at_u32 = 5u),
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
        // the stack lasts the read alone, which gives it back as it ends
        s_stacks[thread] = g_cli_guard->temporary_buf_alloc(
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
    const unsigned long long serial =
        EMBED_CALL(read_number, RecordingReadCtx, .json = args->json, .record = args->record, .at_u32 = 1u);
    if ((serial >= recording->frame_max) || (recording->step_count == recording->step_max))
    {
        recording->cut = recording->cut || (recording->step_count == recording->step_max);
        return;
    }
    CliRecordedFrame *const frame = &args->work->frames[serial];
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
    args->work->steps[recording->step_count] =
        (CliRecordedStep){args->at, (unsigned int)serial,
                          (unsigned int)EMBED_CALL(read_number, RecordingReadCtx, .json = args->json,
                                                   .record = args->record, .at_u32 = 2u),
                          depth};
    recording->step_count += 1u;
}

// a return read: its frame and every frame above it taken off its thread's stack
EMBED_INLINE void read_return(const RecordingReadCtx *args)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long serial =
        EMBED_CALL(read_number, RecordingReadCtx, .json = args->json, .record = args->record, .at_u32 = 1u);
    const unsigned int thread =
        ((serial < recording->frame_max) && args->work->frames[serial].seen) ? args->work->frames[serial].thread : 0u;
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
    const unsigned long long serial =
        EMBED_CALL(read_number, RecordingReadCtx, .json = args->json, .record = args->record, .at_u32 = 1u);
    const CliRecordedFrame *const frame = (serial < recording->frame_max) ? &args->work->frames[serial] : NULL;
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
    const unsigned int type = EMBED_CALL(read_by_name, RecordingReadCtx, .work = args->work, .json = args->json,
                                         .record = args->record, .at_u32 = 3u, .slashed = 0);
    const unsigned int message = EMBED_CALL(read_by_name, RecordingReadCtx, .work = args->work, .json = args->json,
                                            .record = args->record, .at_u32 = 4u, .slashed = 0);
    if (recording->raised_count > 0u)
    {
        const CliRecordedRaise *const last = &args->work->raised[recording->raised_count - 1u];
        const unsigned int here = (step < recording->step_count) ? args->work->steps[step].depth : 0u;
        if ((cellul.eq(&(CatenaFinitaCfg){
                .src = args->work->names + last->type, .other = args->work->names + type, .cap = CLI_POOL_BYTES})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = args->work->names + last->message,
                                          .other = args->work->names + message,
                                          .cap = CLI_POOL_BYTES})) &&
            (last->step < recording->step_count) && (args->work->steps[last->step].depth > here))
        {
            return;
        }
    }
    if (recording->raised_count < recording->raised_max)
    {
        args->work->raised[recording->raised_count] = (CliRecordedRaise){step, type, message};
        recording->raised_count += 1u;
    }
}

// the read's stacks given back
static void read_stacks_released(void)
{
    for (unsigned int thread = 0u; thread < CLI_RECORDING_THREADS; thread += 1u)
    {
        g_cli_guard->persistent_buf_release(s_stacks[thread], g_cli_debug_owner);
        s_stacks[thread] = NULL;
        s_capacities[thread] = 0u;
        s_depths[thread] = 0u;
        s_has_stack[thread] = 0;
    }
}

// the read's cells given back
static void read_work_released(ReadWork *work)
{
    g_cli_guard->persistent_buf_release(work->cell, g_cli_debug_owner);
    g_cli_guard->persistent_buf_release(work->text, g_cli_debug_owner);
    memor.set(&(MemoriaCfg){.dst = work, .val = 0, .bytes = sizeof(*work)});
}

void cli_recording_let_go(void)
{
    if (g_cli_replay == NULL)
    {
        return;
    }
    CliRecording *const recording = &g_cli_replay->recording;
    if (recording->path[0] != '\0')
    {
        (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = recording->path);
    }
    if (recording->cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(recording->cell, g_cli_debug_owner);
    }
    memor.set(&(MemoriaCfg){.dst = recording, .val = 0, .bytes = sizeof(*recording)});
    recording->used = 1u;
}

// a record of the recording read by its type
EMBED_INLINE void read_store(const RecordingReadCtx *args)
{
    CliRecording *const recording = &g_cli_replay->recording;
    CliJson *const json = args->json;
    if (EMBED_CALL(read_type, RecordingReadCtx, .json = json, .record = args->record, .word_text = "file"))
    {
        const unsigned long long file =
            EMBED_CALL(read_number, RecordingReadCtx, .json = json, .record = args->record, .at_u32 = 1u);
        for (; (recording->file_count <= file) && (recording->file_count < recording->file_max);
             recording->file_count += 1u)
        {
            args->work->files[recording->file_count] = 0u;
        }
        if (file < recording->file_max)
        {
            args->work->files[file] = EMBED_CALL(read_by_name, RecordingReadCtx, .work = args->work, .json = json,
                                                 .record = args->record, .at_u32 = 2u, .slashed = 1);
        }
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .json = json, .record = args->record, .word_text = "thread") &&
             (recording->thread_count < recording->thread_max))
    {
        args->work->thread_ids[recording->thread_count] =
            EMBED_CALL(read_number, RecordingReadCtx, .json = json, .record = args->record, .at_u32 = 1u);
        args->work->thread_names[recording->thread_count] =
            EMBED_CALL(read_by_name, RecordingReadCtx, .work = args->work, .json = json, .record = args->record,
                       .at_u32 = 2u, .slashed = 0);
        recording->thread_count += 1u;
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .json = json, .record = args->record, .word_text = "call"))
    {
        read_call(args);
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .json = json, .record = args->record, .word_text = "line"))
    {
        read_line(args);
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .json = json, .record = args->record, .word_text = "return"))
    {
        read_return(args);
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .json = json, .record = args->record, .word_text = "raise"))
    {
        read_raise(args);
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .json = json, .record = args->record, .word_text = "cut"))
    {
        recording->cut = 1;
    }
    else if (EMBED_CALL(read_type, RecordingReadCtx, .json = json, .record = args->record, .word_text = "end"))
    {
        const unsigned int code = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = args->record, .index = 1u);
        long long value = 0LL;
        int whole = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = code,
                               .type = CFG_JSON_NUMBER);
        const char *const text = whole ? json->text + json->node[code].text : "";
        const unsigned long long length = whole ? json->node[code].length : 0ULL;
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

// the tables and the text written into one movable cell of the recording's, the text after the tables: 1, or 0 where
// the cell cannot be taken
EMBED_INLINE int read_stored(const RecordingReadCtx *args)
{
    CliRecording *const recording = args->recording;
    const CarcerMovableEntry *const cell =
        g_cli_guard->movable_buf_alloc(args->work->bytes + args->work->size + 1ULL, g_cli_debug_owner);
    if (cell == NULL)
    {
        return 0;
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = cell, .offset = 0ULL, .src = args->work->cell, .bytes = args->work->bytes});
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = cell, .offset = args->work->bytes, .src = args->work->text, .bytes = args->work->size + 1ULL});
    recording->cell = cell;
    recording->text_at = args->work->bytes;
    recording->size = args->work->size;
    return 1;
}

void cli_recording_read(const CliRecordingPathCtx *args)
{
    if (!cli_replay_lock())
    {
        return;
    }
    CliRecording *const recording = &g_cli_replay->recording;
    cli_recording_let_go();
    read_stacks_released();
    memor.cpy(&(MemoriaCfg){.dst = recording->path,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    // the recording's file read into a cell of this call, its lines ended with zero bytes in place of their ends so
    // each stands as one text in the recording's cell
    unsigned long long size = 0ULL;
    char *text = EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = args->path, .size_pointer = &size)
                     ? g_cli_guard->temporary_buf_alloc(size + CLI_FILE_SLACK, g_cli_debug_owner)
                     : NULL;
    if ((text != NULL) &&
        !EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = args->path, .from = 0ULL,
                    .bytes_bytes = (unsigned char *)text, .count_u64 = size, .got = &size))
    {
        g_cli_guard->persistent_buf_release(text, g_cli_debug_owner);
        text = NULL;
    }
    if (text == NULL)
    {
        return;
    }
    // the tables of the read, in a record of this call
    ReadWork work;
    memor.set(&(MemoriaCfg){.dst = &work, .val = 0, .bytes = sizeof(work)});
    memor.set(&(MemoriaCfg){.dst = text + size, .val = 0, .bytes = CLI_FILE_SLACK});
    work.text = text;
    work.size = size;
    if (!EMBED_CALL(read_work_alloc, RecordingReadCtx, .work = &work, .recording = recording, .text = text,
                    .size = size))
    {
        read_work_released(&work);
        return;
    }
    unsigned long long at = 0ULL;
    while (at < size)
    {
        const CliTextLine line = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size);
        CliJson json;
        const unsigned int record =
            EMBED_CALL(record_read, RecordingReadCtx, .json = &json, .text = text + at, .size = line.stop - at);
        if (record != CLI_JSON_NONE)
        {
            EMBED_CALL(read_store, RecordingReadCtx, .work = &work, .json = &json, .record = record, .at = at);
        }
        g_cli_guard->persistent_buf_release(json.node, g_cli_debug_owner);
        memor.set(&(MemoriaCfg){.dst = text + line.stop, .val = 0, .bytes = (embed_word)(line.next - line.stop)});
        at = line.next;
    }
    read_stacks_released();
    // each frame's steps in turn, in the order they ran
    unsigned int first = 0u;
    for (unsigned int serial = 0u; serial < recording->frame_max; serial += 1u)
    {
        work.frames[serial].first = first;
        work.frames[serial].last = first;
        first += work.frames[serial].count;
    }
    for (unsigned int step = 0u; step < recording->step_count; step += 1u)
    {
        CliRecordedFrame *const frame = &work.frames[work.steps[step].frame];
        work.by_frame[frame->last] = step;
        frame->last += 1u;
    }
    if (!EMBED_CALL(read_stored, RecordingReadCtx, .work = &work, .recording = recording))
    {
        cli_recording_let_go();
    }
    read_work_released(&work);
}
