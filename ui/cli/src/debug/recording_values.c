// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_values.c: what the replay requests of a recording read: a frame's names' values at a step, the frames
// standing at a step, and a name's history through the run, each from the line records the steps name
#include "../command_line/callback_cfg.h"
#include "recording_internal.h"

#include "../text/text.h"

typedef struct
{
    unsigned int step;
    void (*each)(const CliEachNameValueCfg *args);
    void *context;
} RecordingValuesCtx;

// the changes a step's line record names, each handed to `each` with its value, NULL for a name gone with none: the
// names whose values changed, then the names gone
EMBED_INLINE void read_changes(const RecordingValuesCtx *args)
{
    unsigned int record = CLI_JSON_NONE;
    const CliJson *const json =
        EMBED_CALL(cli_recording_internal.record, CliRecordingCtx,
                   .at_u64 = g_cli_replay->recording.steps[args->step].record, .record = &record);
    const unsigned int changed = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = record, .index = 3u);
    for (unsigned int member = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                          .node = changed, .kind = CFG_JSON_OBJECT)
                                   ? json->node[changed].first
                                   : CLI_JSON_NONE;
         member != CLI_JSON_NONE; member = json->node[member].next)
    {
        EMBED_CALL(args->each, CliEachNameValueCfg, .context = args->context,
                   .name = json->text + json->node[member].key,
                   .value = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                       .json_read = json, .node = member, .length_pointer = NULL));
    }
    const unsigned int deleted = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = record, .index = 4u);
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = deleted, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = deleted, .index = index),
                       .length_pointer = NULL);
        if (name != NULL)
        {
            EMBED_CALL(args->each, CliEachNameValueCfg, .context = args->context, .name = name, .value = NULL);
        }
    }
}

// The values stored as the steps of a frame are read: a cell of `capacity` of them for `owner`, and how many there are
typedef struct
{
    CliRecordedValue *values;
    unsigned int capacity;
    unsigned int count;
    CellOwner *owner;
} CliRecordedValues;

// a change applied to the values stored: the name's value taken out, and its new one put after the rest where it has
// one
static void read_value_append(const CliEachNameValueCfg *args)
{
    CliRecordedValues *const recorded = (CliRecordedValues *)args->context;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < recorded->count; at += 1u)
    {
        if (!cellul.eq(
                &(CatenaFinitaCfg){.src = recorded->values[at].name, .other = args->name, .cap = CLI_CELLS_POOL}))
        {
            recorded->values[count] = recorded->values[at];
            count += 1u;
        }
    }
    recorded->count = count;
    if ((args->value != NULL) && (recorded->count < recorded->capacity))
    {
        CliRecordedValue *const put = &recorded->values[recorded->count];
        memor.cpy(
            &(MemoriaCfg){.dst = put->name,
                          .src = args->name,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(
            &(MemoriaCfg){.dst = put->value,
                          .src = args->value,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_CELLS_POOL}) + 1u});
        put->has = 1;
        recorded->count += 1u;
    }
}

unsigned int cli_recording_values(const CliRecordingAtCtx *args)
{
    CliRecordedValue **reserved = args->reserved;
    const CliRecording *const recording = &g_cli_replay->recording;
    *reserved = NULL;
    if (args->serial >= recording->frame_max)
    {
        return 0u;
    }
    CliRecordedValue *const cell =
        g_cli_cells->persistent_buf_alloc((unsigned long long)args->most * sizeof(CliRecordedValue), args->owner);
    CliRecordedValues recorded = {cell, (cell != NULL) ? args->most : 0u, 0u, args->owner};
    const CliRecordedFrame *const frame = &recording->frames[args->serial];
    for (unsigned int index = 0u; (index < frame->count) && (recording->by_frame[frame->first + index] <= args->at);
         index += 1u)
    {
        EMBED_CALL(read_changes, RecordingValuesCtx, .step = recording->by_frame[frame->first + index],
                   .each = read_value_append, .context = &recorded);
    }
    // by name, as a stable sort leaves them
    CliRecordedValue *const values = recorded.values;
    *reserved = values;
    for (unsigned int one = 1u; one < recorded.count; one += 1u)
    {
        const CliRecordedValue moved = values[one];
        unsigned int place = one;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = values[place - 1u].name,
                                           .other_pointer = moved.name) > 0))
        {
            values[place] = values[place - 1u];
            place -= 1u;
        }
        values[place] = moved;
    }
    return recorded.count;
}

unsigned int cli_recording_stack(const CliRecordingAtFramesCtx *args)
{
    unsigned int at = args->at;
    const CliRecording *const recording = &g_cli_replay->recording;
    unsigned int count = 0u;
    unsigned int serial = recording->steps[at].frame;
    while ((serial != 0u) && (count < 512u) && (count < args->capacity))
    {
        const unsigned int step =
            EMBED_CALL(cli_recording_at_internal.last_get, CliRecordingAtCtx, .serial = serial, .at = at);
        if (step == CLI_RECORDING_NO_STEP)
        {
            break;
        }
        args->frames[count][0] = serial;
        args->frames[count][1] = step;
        count += 1u;
        serial =
            ((serial < recording->frame_max) && recording->frames[serial].seen) ? recording->frames[serial].parent : 0u;
    }
    return count;
}

// The search of one name's history: the name, whether the step at hand changed it and the value it took
typedef struct
{
    const char *name;
    int found;
    char value[2048];
    int has;
} CliRecordedNeedle;

static void read_on_find(const CliEachNameValueCfg *args)
{
    CliRecordedNeedle *const needle = (CliRecordedNeedle *)args->context;
    if (!needle->found &&
        (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = needle->name, .cap = CLI_CELLS_POOL})))
    {
        needle->found = 1;
        needle->has = (args->value != NULL);
        (void)cellul.copy(&(CatenaFinitaCfg){
            .dst = needle->value, .src = (args->value != NULL) ? args->value : "", .cap = sizeof(needle->value)});
    }
}

void cli_recording_history(const CliRecordingSerialCtx *args)
{
    void (*each)(const CliEachStepLineValueCfg *args) = args->each;
    void *context = args->context;
    const CliRecording *const recording = &g_cli_replay->recording;
    if (args->serial >= recording->frame_max)
    {
        return;
    }
    const CliRecordedFrame *const frame = &recording->frames[args->serial];
    for (unsigned int index = 0u; index < frame->count; index += 1u)
    {
        CliRecordedNeedle needle = {args->name, 0, "", 0};
        EMBED_CALL(read_changes, RecordingValuesCtx, .step = recording->by_frame[frame->first + index],
                   .each = read_on_find, .context = &needle);
        if (!needle.found)
        {
            continue;
        }
        // a value seen first at a line was given by the frame's line before it, or by the call, for an argument,
        // where the line is the frame's first
        const unsigned int by = recording->by_frame[frame->first + ((index > 0u) ? index - 1u : index)];
        EMBED_CALL(each, CliEachStepLineValueCfg, .context = context, .step = by, .line = recording->steps[by].line,
                   .value = needle.has ? needle.value : NULL);
    }
}
