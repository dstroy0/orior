// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_read.c: a recorded run read from the recorder's lines, its steps, frames, files, threads and raises, and
// what the replay asks of it: a frame's last step, its names' values at a step, the frames standing at a step, and a
// name's history through the run
#include "recording_internal.h"

#include "../text/text.h"

CliReplay *g_cli_replay;

// the most nodes and bytes of one line of the recording read
#define CLI_RECORDING_LINE_NODES 8192u
#define CLI_RECORDING_LINE_TEXT (1u << 20u)

static CliJsonNode s_nodes[CLI_RECORDING_LINE_NODES];
static CfgJsonToken s_tokens[CLI_RECORDING_LINE_NODES];
static char s_text[CLI_RECORDING_LINE_TEXT];
static CliJson s_json;
static unsigned int s_stacks[CLI_RECORDING_THREADS][CLI_RECORDING_DEPTH];
static unsigned int s_depths[CLI_RECORDING_THREADS];
static int s_has_stack[CLI_RECORDING_THREADS];

const char *cli_recording_name(unsigned int at)
{
    return g_cli_replay->recording.names + at;
}

// the line of the recording that starts at `at` read: its array, CLI_JSON_NONE where it is none
static unsigned int read_record(unsigned long long at)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    const char *const newline =
        memor.chr(&(MemoriaCfg){.src = recording->text + at, .val = '\n', .bytes = recording->size - at});
    const unsigned long long end =
        (newline != NULL) ? (unsigned long long)(newline - recording->text) : recording->size;
    cli_json_begun(&s_json, s_nodes, CLI_RECORDING_LINE_NODES, s_text, CLI_RECORDING_LINE_TEXT);
    char why[CLI_SAID_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root =
        cli_json_read(&s_json, recording->text + at, end - at, s_tokens, CLI_RECORDING_LINE_NODES, &unread);
    return cli_json_is(&s_json, root, CFG_JSON_ARRAY) ? root : CLI_JSON_NONE;
}

// the whole number at place `at` of a record, 0 where it holds none
static unsigned long long read_number(unsigned int record, unsigned int at)
{
    const unsigned int node = cli_json_item(&s_json, record, at);
    if (!cli_json_is(&s_json, node, CFG_JSON_NUMBER))
    {
        return 0ull;
    }
    const char *const text = s_json.text + s_json.node[node].text;
    unsigned long long value = 0ull;
    for (unsigned long long letter = 0ull; letter < s_json.node[node].length; letter += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(text[letter] - '0');
        if ((text[letter] < '0') || (text[letter] > '9') || (value > (~0ull - digit) / 10ull))
        {
            return 0ull;
        }
        value = (value * 10ull) + digit;
    }
    return value;
}

// the text at place `at` of a record put in the names: its place there, the names' first, empty, where it holds none
static unsigned int read_named(unsigned int record, unsigned int at, int slashed)
{
    CliRecording *const recording = &g_cli_replay->recording;
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(&s_json, cli_json_item(&s_json, record, at), &length);
    if ((text == NULL) || (recording->used + length + 1ull > CLI_RECORDING_TEXT))
    {
        return 0u;
    }
    const unsigned int place = recording->used;
    for (unsigned long long letter = 0ull; letter < length; letter += 1ull)
    {
        recording->names[place + letter] = (slashed && (text[letter] == '\\')) ? '/' : text[letter];
    }
    recording->names[place + length] = '\0';
    recording->used += (unsigned int)length + 1u;
    return place;
}

// whether the text at place `at` of a record is `word`
static int read_kind(unsigned int record, const char *word)
{
    const char *const text = cli_json_string(&s_json, cli_json_item(&s_json, record, 0u), NULL);
    return (text != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = word, .cap = CLI_CELLS_POOL}));
}

// a call read: the frame kept by its number, its thread's stack grown by it where it is not its top
static void read_call(unsigned int record)
{
    CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long serial = read_number(record, 1u);
    if (serial >= CLI_RECORDING_FRAMES)
    {
        return;
    }
    CliRecordedFrame *const frame = &recording->frames[serial];
    const unsigned int count = frame->seen ? frame->count : 0u;
    const unsigned int last = frame->seen ? frame->last : CLI_RECORDING_STEPS;
    *frame = (CliRecordedFrame){read_named(record, 4u, 0),
                                (unsigned int)read_number(record, 3u),
                                (unsigned int)read_number(record, 2u),
                                (unsigned int)read_number(record, 5u),
                                0u,
                                count,
                                last,
                                1};
    const unsigned int thread = frame->thread;
    if (thread >= CLI_RECORDING_THREADS)
    {
        return;
    }
    s_has_stack[thread] = 1;
    if (((s_depths[thread] == 0u) || (s_stacks[thread][s_depths[thread] - 1u] != serial)) &&
        (s_depths[thread] < CLI_RECORDING_DEPTH))
    {
        s_stacks[thread][s_depths[thread]] = (unsigned int)serial;
        s_depths[thread] += 1u;
    }
}

// the place of `serial` in a thread's stack from its top, one past it; the stack's depth where it is not there
static unsigned int read_place(unsigned int thread, unsigned int serial, int *found)
{
    for (unsigned int at = s_depths[thread]; at > 0u; at -= 1u)
    {
        if (s_stacks[thread][at - 1u] == serial)
        {
            *found = 1;
            return at;
        }
    }
    *found = 0;
    return s_depths[thread];
}

// a line read: a step of its frame, as deep as the frame stands in its thread's stack
static void read_line(unsigned int record, unsigned long long at)
{
    CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long serial = read_number(record, 1u);
    if ((serial >= CLI_RECORDING_FRAMES) || (recording->step_count == CLI_RECORDING_STEPS))
    {
        recording->cut = recording->cut || (recording->step_count == CLI_RECORDING_STEPS);
        return;
    }
    CliRecordedFrame *const frame = &recording->frames[serial];
    const unsigned int thread = frame->seen ? frame->thread : 0u;
    int found = 0;
    const unsigned int depth = ((thread < CLI_RECORDING_THREADS) && s_has_stack[thread])
                                   ? read_place(thread, (unsigned int)serial, &found)
                                   : 1u;
    if (!frame->seen)
    {
        *frame = (CliRecordedFrame){0u, 0u, 0u, 0u, 0u, 0u, CLI_RECORDING_STEPS, 1};
    }
    frame->count += 1u;
    frame->last = recording->step_count;
    recording->steps[recording->step_count] =
        (CliRecordedStep){at, (unsigned int)serial, (unsigned int)read_number(record, 2u), depth};
    recording->step_count += 1u;
}

// a return read: its frame and every frame above it taken off its thread's stack
static void read_return(unsigned int record)
{
    CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long serial = read_number(record, 1u);
    const unsigned int thread =
        ((serial < CLI_RECORDING_FRAMES) && recording->frames[serial].seen) ? recording->frames[serial].thread : 0u;
    int found = 0;
    if ((thread < CLI_RECORDING_THREADS) && s_has_stack[thread])
    {
        const unsigned int place = read_place(thread, (unsigned int)serial, &found);
        s_depths[thread] = found ? place - 1u : s_depths[thread];
    }
}

// a raise read: an exception passing up through frames is raised again in each, and the first is where it began
static void read_raise(unsigned int record)
{
    CliRecording *const recording = &g_cli_replay->recording;
    const unsigned long long serial = read_number(record, 1u);
    const CliRecordedFrame *const frame = (serial < CLI_RECORDING_FRAMES) ? &recording->frames[serial] : NULL;
    const unsigned int step = ((frame != NULL) && frame->seen && (frame->last < CLI_RECORDING_STEPS))
                                  ? frame->last
                                  : ((recording->step_count > 0u) ? recording->step_count - 1u : 0u);
    const unsigned int kind = read_named(record, 3u, 0);
    const unsigned int message = read_named(record, 4u, 0);
    if (recording->raised_count > 0u)
    {
        const CliRecordedRaise *const last = &recording->raised[recording->raised_count - 1u];
        const unsigned int here = (step < recording->step_count) ? recording->steps[step].depth : 0u;
        if ((cellul.eq(&(CatenaFinitaCfg){
                .src = recording->names + last->kind, .other = recording->names + kind, .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = recording->names + last->message,
                                          .other = recording->names + message,
                                          .cap = CLI_CELLS_POOL})) &&
            (last->step < recording->step_count) && (recording->steps[last->step].depth > here))
        {
            return;
        }
    }
    if (recording->raised_count < CLI_RECORDING_RAISED)
    {
        recording->raised[recording->raised_count] = (CliRecordedRaise){step, kind, message};
        recording->raised_count += 1u;
    }
}

void cli_recording_let_go(void)
{
    if (g_cli_replay == NULL)
    {
        return;
    }
    CliRecording *const recording = &g_cli_replay->recording;
    if (recording->text != NULL)
    {
        qry_member_file_unmapped((const unsigned char *)recording->text, recording->size);
        recording->text = NULL;
    }
    if (recording->path[0] != '\0')
    {
        (void)query_removed(recording->path);
        recording->path[0] = '\0';
    }
    recording->size = 0ull;
    recording->step_count = 0u;
    memor.set(&(MemoriaCfg){.dst = recording->frames, .val = 0, .bytes = sizeof(recording->frames)});
    recording->file_count = 0u;
    recording->thread_count = 0u;
    recording->raised_count = 0u;
    recording->names[0] = '\0';
    recording->used = 1u;
    recording->has_code = 0;
    recording->code = 0ll;
    recording->cut = 0;
}

// a record of the recording read by its kind
static void read_kept(unsigned int record, unsigned long long at)
{
    CliRecording *const recording = &g_cli_replay->recording;
    if (read_kind(record, "file"))
    {
        const unsigned long long file = read_number(record, 1u);
        for (; (recording->file_count <= file) && (recording->file_count < CLI_RECORDING_FILES);
             recording->file_count += 1u)
        {
            recording->files[recording->file_count] = 0u;
        }
        if (file < CLI_RECORDING_FILES)
        {
            recording->files[file] = read_named(record, 2u, 1);
        }
    }
    else if (read_kind(record, "thread") && (recording->thread_count < CLI_RECORDING_THREADS))
    {
        recording->thread_ids[recording->thread_count] = read_number(record, 1u);
        recording->thread_names[recording->thread_count] = read_named(record, 2u, 0);
        recording->thread_count += 1u;
    }
    else if (read_kind(record, "call"))
    {
        read_call(record);
    }
    else if (read_kind(record, "line"))
    {
        read_line(record, at);
    }
    else if (read_kind(record, "return"))
    {
        read_return(record);
    }
    else if (read_kind(record, "raise"))
    {
        read_raise(record);
    }
    else if (read_kind(record, "cut"))
    {
        recording->cut = 1;
    }
    else if (read_kind(record, "end"))
    {
        const unsigned int code = cli_json_item(&s_json, record, 1u);
        long long value = 0ll;
        int whole = cli_json_is(&s_json, code, CFG_JSON_NUMBER);
        const char *const text = whole ? s_json.text + s_json.node[code].text : "";
        const unsigned long long length = whole ? s_json.node[code].length : 0ull;
        const int negative = (length > 0ull) && (text[0] == '-');
        for (unsigned long long letter = negative ? 1ull : 0ull; whole && (letter < length); letter += 1ull)
        {
            whole = (text[letter] >= '0') && (text[letter] <= '9') && (value < 922337203685477580ll);
            value = (value * 10ll) + (long long)(text[letter] - '0');
        }
        recording->has_code = whole && (length > (negative ? 1ull : 0ull));
        recording->code = negative ? -value : value;
    }
}

void cli_recording_read(const char *path)
{
    if (!cli_replay_held())
    {
        return;
    }
    CliRecording *const recording = &g_cli_replay->recording;
    cli_recording_let_go();
    memor.set(&(MemoriaCfg){.dst = s_depths, .val = 0, .bytes = sizeof(s_depths)});
    memor.set(&(MemoriaCfg){.dst = s_has_stack, .val = 0, .bytes = sizeof(s_has_stack)});
    memor.cpy(&(MemoriaCfg){.dst = recording->path,
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(path, &size);
    recording->text = (const char *)bytes;
    recording->size = (bytes != NULL) ? size : 0ull;
    unsigned long long at = 0ull;
    while (at < recording->size)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = recording->text + at, .val = '\n', .bytes = recording->size - at});
        const unsigned long long next =
            (newline != NULL) ? (unsigned long long)(newline - recording->text) + 1ull : recording->size;
        const unsigned int record = read_record(at);
        if (record != CLI_JSON_NONE)
        {
            read_kept(record, at);
        }
        at = next;
    }
    // each frame's steps in turn, in the order they ran
    unsigned int first = 0u;
    for (unsigned int serial = 0u; serial < CLI_RECORDING_FRAMES; serial += 1u)
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

// the changes a step's line record names, each handed to `each` with its value, NULL for a name gone with none: the
// names whose values changed, then the names gone
static void read_changes(unsigned int step, void (*each)(void *context, const char *name, const char *value),
                         void *context)
{
    const unsigned int record = read_record(g_cli_replay->recording.steps[step].record);
    const unsigned int changed = cli_json_item(&s_json, record, 3u);
    for (unsigned int member = cli_json_is(&s_json, changed, CFG_JSON_OBJECT) ? s_json.node[changed].first
                                                                              : CLI_JSON_NONE;
         member != CLI_JSON_NONE; member = s_json.node[member].next)
    {
        each(context, s_json.text + s_json.node[member].key, cli_json_string(&s_json, member, NULL));
    }
    const unsigned int gone = cli_json_item(&s_json, record, 4u);
    for (unsigned int index = 0u; cli_json_item(&s_json, gone, index) != CLI_JSON_NONE; index += 1u)
    {
        const char *const name = cli_json_string(&s_json, cli_json_item(&s_json, gone, index), NULL);
        if (name != NULL)
        {
            each(context, name, NULL);
        }
    }
}

// The values held as the steps of a frame are read: where they go, how many it holds, and how many there are
typedef struct
{
    CliRecordedValue *values;
    unsigned int room;
    unsigned int count;
} CliRecordedHeld;

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

unsigned int cli_recording_values(unsigned int serial, unsigned int at, CliRecordedValue *values, unsigned int room)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    CliRecordedHeld held = {values, room, 0u};
    if (serial >= CLI_RECORDING_FRAMES)
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
        if (step == CLI_RECORDING_STEPS)
        {
            break;
        }
        frames[count][0] = serial;
        frames[count][1] = step;
        count += 1u;
        serial =
            ((serial < CLI_RECORDING_FRAMES) && recording->frames[serial].seen) ? recording->frames[serial].parent : 0u;
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
    if (serial >= CLI_RECORDING_FRAMES)
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
