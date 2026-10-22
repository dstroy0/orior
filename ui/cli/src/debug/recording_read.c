// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_read.c: a recorded run read from the recorder's lines, its steps, frames, files, threads and raises, and
// what the replay asks of it: a frame's last step, its names' values at a step, the frames standing at a step, and a
// name's history through the run
#include "recording_internal.h"

CliReplay *g_cli_replay;

// the most bytes of one line of the recording read
#define CLI_RECORDING_LINE_TEXT (1ull << 20u)

// the line read last, in a tree of its own sized to it, given back as the next is read and as the recording is let go
static CliJson s_json;
// the frames a thread's stack's room is begun with, doubling as frames are called
#define CLI_RECORDING_STACK_FIRST 16u

// each thread's stack of frames as the recording is read, in a cell of its own of `s_rooms` frames, its depth, and
// whether it has one; each cell given back once the recording is read
static unsigned int *s_stacks[CLI_RECORDING_THREADS];
static unsigned int s_rooms[CLI_RECORDING_THREADS];
static unsigned int s_depths[CLI_RECORDING_THREADS];
static int s_has_stack[CLI_RECORDING_THREADS];

// room made on a thread's stack for one frame more: 1, or 0 where it holds the most or the larger cell is refused
static int read_stack_room(unsigned int thread)
{
    if (s_depths[thread] < s_rooms[thread])
    {
        return 1;
    }
    const unsigned int doubled = (s_rooms[thread] == 0u) ? CLI_RECORDING_STACK_FIRST : (s_rooms[thread] * 2u);
    const unsigned int room = (doubled < CLI_RECORDING_DEPTH) ? doubled : CLI_RECORDING_DEPTH;
    unsigned int *const larger =
        (s_depths[thread] < CLI_RECORDING_DEPTH)
            ? cli_cell_taken((unsigned long long)room * sizeof(unsigned int), "a thread's stack")
            : NULL;
    if (larger == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = larger, .src = s_stacks[thread], .bytes = s_depths[thread] * sizeof(unsigned int)});
    cli_cell_given(s_stacks[thread]);
    s_stacks[thread] = larger;
    s_rooms[thread] = room;
    return 1;
}

// every thread's stack given back
static void read_stacks_given(void)
{
    for (unsigned int thread = 0u; thread < CLI_RECORDING_THREADS; thread += 1u)
    {
        cli_cell_given(s_stacks[thread]);
        s_stacks[thread] = NULL;
        s_rooms[thread] = 0u;
        s_depths[thread] = 0u;
        s_has_stack[thread] = 0;
    }
}

// the names of a recording with no room: one empty name
static char s_no_names[1] = "";

const char *cli_recording_name(unsigned int at)
{
    return g_cli_replay->recording.names + at;
}

// The counts a recording's lines give by their kinds, read off the text before the lines are read whole: the steps,
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

// the whole number written at `at` of `size` bytes of `text`, read up to its first letter that is no digit: the
// number, all ones where none is written or it passes an unsigned int
static unsigned int read_digits(const char *text, unsigned long long size, unsigned long long at)
{
    unsigned long long value = 0ull;
    unsigned long long digits = 0ull;
    while ((at < size) && (text[at] >= '0') && (text[at] <= '9') && (value < 0xffffffffull))
    {
        value = (value * 10ull) + (unsigned long long)(text[at] - '0');
        at += 1ull;
        digits += 1ull;
    }
    return ((digits == 0ull) || (value > 0xffffffffull)) ? 0xffffffffu : (unsigned int)value;
}

// the counts of `size` bytes of `text`: each line's kind is the word its array opens with, and the number after it
// where the kind takes one
static void read_counted(const char *text, unsigned long long size, ReadCounts *counts)
{
    static const char *const s_kinds[5] = {"[\"line\",", "[\"call\",", "[\"file\",", "[\"thread\",", "[\"raise\","};
    *counts = (ReadCounts){0u, 0u, 0u, 0u, 0u};
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        for (unsigned int kind = 0u; kind < 5u; kind += 1u)
        {
            const unsigned long long length =
                cellul.len(&(CatenaFinitaCfg){.src = s_kinds[kind], .cap = CLI_CELLS_POOL});
            if ((end - at < length) ||
                (memor.cmp(&(MemoriaCfg){.src = text + at, .other = s_kinds[kind], .bytes = (size_t)length}) != 0))
            {
                continue;
            }
            const unsigned int number = read_digits(text, end, at + length);
            const unsigned int named = (number == 0xffffffffu) ? 0u : (number + 1u);
            counts->steps += (kind == 0u) ? 1u : 0u;
            counts->frames = ((kind <= 1u) && (named > counts->frames)) ? named : counts->frames;
            counts->files = ((kind == 2u) && (named > counts->files)) ? named : counts->files;
            counts->threads += (kind == 3u) ? 1u : 0u;
            counts->raises += (kind == 4u) ? 1u : 0u;
            break;
        }
        at = end + 1ull;
    }
}

// the next part of a cell, `bytes` long, taken from `*at`, its start kept on a word's boundary: its start
static void *read_part(unsigned char **at, unsigned long long bytes)
{
    void *const part = *at;
    *at += (bytes + 7ull) & ~7ull;
    return part;
}

// the recording's room taken for the counts of its `size` bytes of `text` and its parts set into it, the frames and
// files zero and the names one empty name: 1, or 0 with the recording holding no room
static int read_room_taken(CliRecording *recording, const char *text, unsigned long long size)
{
    ReadCounts counts;
    read_counted(text, size, &counts);
    const unsigned long long bytes =
        ((unsigned long long)counts.steps * (sizeof(CliRecordedStep) + sizeof(unsigned int))) +
        ((unsigned long long)counts.frames * sizeof(CliRecordedFrame)) +
        ((unsigned long long)counts.files * sizeof(unsigned int)) +
        ((unsigned long long)counts.threads * (sizeof(unsigned long long) + sizeof(unsigned int))) +
        ((unsigned long long)counts.raises * sizeof(CliRecordedRaise)) + size + 1ull + (8ull * 8ull);
    unsigned char *const cell = cli_cell_taken(bytes, "a recording read");
    if (cell == NULL)
    {
        return 0;
    }
    unsigned char *at = cell;
    recording->room = cell;
    recording->steps = read_part(&at, (unsigned long long)counts.steps * sizeof(CliRecordedStep));
    recording->step_most = counts.steps;
    recording->by_frame = read_part(&at, (unsigned long long)counts.steps * sizeof(unsigned int));
    recording->frames = read_part(&at, (unsigned long long)counts.frames * sizeof(CliRecordedFrame));
    recording->frame_most = counts.frames;
    recording->files = read_part(&at, (unsigned long long)counts.files * sizeof(unsigned int));
    recording->file_most = counts.files;
    recording->thread_ids = read_part(&at, (unsigned long long)counts.threads * sizeof(unsigned long long));
    recording->thread_names = read_part(&at, (unsigned long long)counts.threads * sizeof(unsigned int));
    recording->thread_most = counts.threads;
    recording->raised = read_part(&at, (unsigned long long)counts.raises * sizeof(CliRecordedRaise));
    recording->raised_most = counts.raises;
    recording->names = read_part(&at, size + 1ull);
    recording->names_most = size + 1ull;
    memor.set(
        &(MemoriaCfg){.dst = recording->frames, .val = 0, .bytes = (size_t)counts.frames * sizeof(CliRecordedFrame)});
    memor.set(&(MemoriaCfg){.dst = recording->files, .val = 0, .bytes = (size_t)counts.files * sizeof(unsigned int)});
    recording->names[0] = '\0';
    recording->used = 1u;
    return 1;
}

// the line of the recording that starts at `at` read: its array, CLI_JSON_NONE where it is none
static unsigned int read_record(unsigned long long at)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    const char *const newline =
        memor.chr(&(MemoriaCfg){.src = recording->text + at, .val = '\n', .bytes = recording->size - at});
    const unsigned long long end =
        (newline != NULL) ? (unsigned long long)(newline - recording->text) : recording->size;
    cli_json_given(&s_json);
    char why[CLI_SAID_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = (end - at <= CLI_RECORDING_LINE_TEXT)
                                  ? cli_json_taken(&s_json, recording->text + at, end - at, 0u, 0ull, &unread)
                                  : CLI_JSON_NONE;
    return cli_json_is(&s_json, root, CFG_JSON_ARRAY) ? root : CLI_JSON_NONE;
}

const CliJson *cli_recording_record(unsigned long long at, unsigned int *record)
{
    *record = read_record(at);
    return &s_json;
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
    if ((text == NULL) || (recording->used + length + 1ull > recording->names_most))
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
    if (serial >= recording->frame_most)
    {
        return;
    }
    CliRecordedFrame *const frame = &recording->frames[serial];
    const unsigned int count = frame->seen ? frame->count : 0u;
    const unsigned int last = frame->seen ? frame->last : CLI_RECORDING_NO_STEP;
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
    if (((s_depths[thread] == 0u) || (s_stacks[thread][s_depths[thread] - 1u] != serial)) && read_stack_room(thread))
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
    if ((serial >= recording->frame_most) || (recording->step_count == recording->step_most))
    {
        recording->cut = recording->cut || (recording->step_count == recording->step_most);
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
        *frame = (CliRecordedFrame){0u, 0u, 0u, 0u, 0u, 0u, CLI_RECORDING_NO_STEP, 1};
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
        ((serial < recording->frame_most) && recording->frames[serial].seen) ? recording->frames[serial].thread : 0u;
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
    const CliRecordedFrame *const frame = (serial < recording->frame_most) ? &recording->frames[serial] : NULL;
    const unsigned int step = ((frame != NULL) && frame->seen && (frame->last != CLI_RECORDING_NO_STEP))
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
    if (recording->raised_count < recording->raised_most)
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
    cli_json_given(&s_json);
    CliRecording *const recording = &g_cli_replay->recording;
    if (recording->text != NULL)
    {
        cli_file_let_go((const unsigned char *)recording->text, recording->size);
        recording->text = NULL;
    }
    if (recording->path[0] != '\0')
    {
        (void)query_removed(recording->path);
        recording->path[0] = '\0';
    }
    recording->size = 0ull;
    cli_cell_given(recording->room);
    recording->room = NULL;
    recording->steps = NULL;
    recording->step_count = 0u;
    recording->step_most = 0u;
    recording->frames = NULL;
    recording->frame_most = 0u;
    recording->by_frame = NULL;
    recording->files = NULL;
    recording->file_count = 0u;
    recording->file_most = 0u;
    recording->thread_ids = NULL;
    recording->thread_names = NULL;
    recording->thread_count = 0u;
    recording->thread_most = 0u;
    recording->raised = NULL;
    recording->raised_count = 0u;
    recording->raised_most = 0u;
    recording->names = s_no_names;
    recording->names_most = 0ull;
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
        for (; (recording->file_count <= file) && (recording->file_count < recording->file_most);
             recording->file_count += 1u)
        {
            recording->files[recording->file_count] = 0u;
        }
        if (file < recording->file_most)
        {
            recording->files[file] = read_named(record, 2u, 1);
        }
    }
    else if (read_kind(record, "thread") && (recording->thread_count < recording->thread_most))
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
    read_stacks_given();
    memor.cpy(&(MemoriaCfg){.dst = recording->path,
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(path, &size);
    recording->text = (const char *)bytes;
    recording->size = (bytes != NULL) ? size : 0ull;
    if ((bytes != NULL) && !read_room_taken(recording, recording->text, recording->size))
    {
        cli_recording_let_go();
        return;
    }
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
    cli_json_given(&s_json);
    read_stacks_given();
    // each frame's steps in turn, in the order they ran
    unsigned int first = 0u;
    for (unsigned int serial = 0u; serial < recording->frame_most; serial += 1u)
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
