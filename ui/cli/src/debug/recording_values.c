// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_values.c: what the replay asks of a recording read: a frame's names' values at a step, the frames standing
// at a step, and a name's history through the run, each from the line records the steps name
#include "recording_internal.h"

#include "../text/text.h"

// the changes a step's line record names, each handed to `each` with its value, NULL for a name gone with none: the
// names whose values changed, then the names gone
static void read_changes(unsigned int step, void (*each)(void *context, const char *name, const char *value),
                         void *context)
{
    unsigned int record = CLI_JSON_NONE;
    const CliJson *const json = cli_recording_record(g_cli_replay->recording.steps[step].record, &record);
    const unsigned int changed = cli_json_item(json, record, 3u);
    for (unsigned int member = cli_json_is(json, changed, CFG_JSON_OBJECT) ? json->node[changed].first : CLI_JSON_NONE;
         member != CLI_JSON_NONE; member = json->node[member].next)
    {
        each(context, json->text + json->node[member].key, cli_json_string(json, member, NULL));
    }
    const unsigned int gone = cli_json_item(json, record, 4u);
    for (unsigned int index = 0u; cli_json_item(json, gone, index) != CLI_JSON_NONE; index += 1u)
    {
        const char *const name = cli_json_string(json, cli_json_item(json, gone, index), NULL);
        if (name != NULL)
        {
            each(context, name, NULL);
        }
    }
}

// the values a frame's are begun to be held in, doubling as they come
#define CLI_RECORDED_VALUES_FIRST 16u

// The values held as the steps of a frame are read: a cell of `room` of them grown as they come, the most it holds,
// and how many there are
typedef struct
{
    CliRecordedValue *values;
    unsigned int room;
    unsigned int most;
    unsigned int count;
} CliRecordedHeld;

// room made among the values held for one more, below their most: the values moved into a cell of twice their room
static void read_held_room(CliRecordedHeld *held)
{
    if ((held->count < held->room) || (held->room >= held->most))
    {
        return;
    }
    const unsigned int doubled = (held->room == 0u) ? CLI_RECORDED_VALUES_FIRST : (held->room * 2u);
    const unsigned int room = (doubled < held->most) ? doubled : held->most;
    CliRecordedValue *const grown = cli_cell_taken(room * sizeof(*grown), "the values a frame held");
    if (grown == NULL)
    {
        return;
    }
    if (held->count != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = held->values, .bytes = held->count * sizeof(*grown)});
    }
    cli_cell_given(held->values);
    held->values = grown;
    held->room = room;
}

// a change applied to the values held: the name's value taken out, and its new one put after the rest where it has one
static void read_held(void *context, const char *name, const char *value)
{
    CliRecordedHeld *const held = (CliRecordedHeld *)context;
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < held->count; at += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = held->values[at].name, .other = name, .cap = CLI_CELLS_POOL}))
        {
            held->values[kept] = held->values[at];
            kept += 1u;
        }
    }
    held->count = kept;
    if (value != NULL)
    {
        read_held_room(held);
    }
    if ((value != NULL) && (held->count < held->room))
    {
        CliRecordedValue *const put = &held->values[held->count];
        memor.cpy(&(MemoriaCfg){.dst = put->name,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = put->value,
                                .src = value,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}) + 1u});
        put->has = 1;
        held->count += 1u;
    }
}

unsigned int cli_recording_values(unsigned int serial, unsigned int at, CliRecordedValue **taken, unsigned int most)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    CliRecordedHeld held = {NULL, 0u, most, 0u};
    *taken = NULL;
    if (serial >= recording->frame_most)
    {
        return 0u;
    }
    const CliRecordedFrame *const frame = &recording->frames[serial];
    for (unsigned int index = 0u; (index < frame->count) && (recording->by_frame[frame->first + index] <= at);
         index += 1u)
    {
        read_changes(recording->by_frame[frame->first + index], read_held, &held);
    }
    // by name, as a stable sort leaves them
    CliRecordedValue *const values = held.values;
    *taken = values;
    for (unsigned int one = 1u; one < held.count; one += 1u)
    {
        const CliRecordedValue moved = values[one];
        unsigned int place = one;
        while ((place > 0u) && (cli_text_order(values[place - 1u].name, moved.name) > 0))
        {
            values[place] = values[place - 1u];
            place -= 1u;
        }
        values[place] = moved;
    }
    return held.count;
}

unsigned int cli_recording_stack(unsigned int at, unsigned int (*frames)[2], unsigned int room)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    unsigned int count = 0u;
    unsigned int serial = recording->steps[at].frame;
    while ((serial != 0u) && (count < 512u) && (count < room))
    {
        const unsigned int step = cli_recording_last_of(serial, at);
        if (step == CLI_RECORDING_NO_STEP)
        {
            break;
        }
        frames[count][0] = serial;
        frames[count][1] = step;
        count += 1u;
        serial = ((serial < recording->frame_most) && recording->frames[serial].seen) ? recording->frames[serial].parent
                                                                                      : 0u;
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
} CliRecordedSought;

static void read_sought(void *context, const char *name, const char *value)
{
    CliRecordedSought *const sought = (CliRecordedSought *)context;
    if (!sought->found && (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = sought->name, .cap = CLI_CELLS_POOL})))
    {
        sought->found = 1;
        sought->has = (value != NULL);
        memor.cpy(
            &(MemoriaCfg){.dst = sought->value, .bytes = sizeof(sought->value), .src = (value != NULL) ? value : ""});
    }
}

void cli_recording_history(unsigned int serial, const char *name,
                           void (*each)(void *context, unsigned int step, unsigned int line, const char *value),
                           void *context)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    if (serial >= recording->frame_most)
    {
        return;
    }
    const CliRecordedFrame *const frame = &recording->frames[serial];
    for (unsigned int index = 0u; index < frame->count; index += 1u)
    {
        CliRecordedSought sought = {name, 0, "", 0};
        read_changes(recording->by_frame[frame->first + index], read_sought, &sought);
        if (!sought.found)
        {
            continue;
        }
        // a value seen first at a line was given by the frame's line before it, or by the call, for an argument,
        // where the line is the frame's first
        const unsigned int by = recording->by_frame[frame->first + ((index > 0u) ? index - 1u : index)];
        each(context, by, recording->steps[by].line, sought.has ? sought.value : NULL);
    }
}
