// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_replay.c: where the replay goes and what it stops at: a step forward or back, a stop at a breakpoint or a
// raise, a frame's last step, the stop told, the paths it compares, and a frame as a stack frame
#include "recording_internal.h"

// a path of the tree as the replay compares it: whole, with one kind of slash, and without case on Windows, into `key`
void cli_replay_key(const char *path, char *key)
{
    CliLine out = {key, CLI_PATH_LONGEST, 0ull};
    for (const char *letter = path; *letter != '\0'; letter += 1)
    {
        char one = (*letter == '\\') ? '/' : *letter;
#if defined(_WIN32)
        one = ((one >= 'A') && (one <= 'Z')) ? (char)(one - 'A' + 'a') : one;
#endif
        cli_line_character(&out, one);
    }
    cli_line_closed(&out);
}

// the full path of the file at tree path `relative`, joined a part at a time, into `full`
void cli_replay_full(const char *relative, char *full)
{
    char joined[CLI_PATH_LONGEST];
    memor.cpy(
        &(MemoriaCfg){.dst = full,
                      .src = g_cli_replay->root,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = g_cli_replay->root, .cap = CLI_CELLS_POOL}) + 1u});
    for (const char *part = relative; *part != '\0';)
    {
        const char *const slash = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL, .byte = '/'});
        const size_t length = (slash != NULL) ? (size_t)(slash - part)
                                              : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
        if (length > 0u)
        {
            char one[CLI_PATH_LONGEST];
            CliLine line = {one, sizeof(one), 0ull};
            cli_line_counted(&line, part, length);
            cli_line_closed(&line);
            cli_full(full, one, joined);
            memor.cpy(&(MemoriaCfg){.dst = full, .src = joined, .bytes = CLI_PATH_LONGEST});
        }
        part += length + ((slash != NULL) ? 1u : 0u);
    }
}

// the tree path of the file a frame's lines are in, "" where the recording names none
const char *cli_replay_file_of(unsigned int serial)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    const CliRecordedFrame *const frame = (serial < recording->frame_most) ? &recording->frames[serial] : NULL;
    return ((frame != NULL) && frame->seen && (frame->file < recording->file_count))
               ? cli_recording_name(recording->files[frame->file])
               : "";
}

// the thread a step's frame ran on, 0 where the recording names none
unsigned int cli_replay_thread_of(unsigned int step)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    const unsigned int serial = recording->steps[step].frame;
    return ((serial < recording->frame_most) && recording->frames[serial].seen) ? recording->frames[serial].thread : 0u;
}

// the thread the replay stands on
unsigned int cli_replay_thread(void)
{
    return (g_cli_replay->recording.step_count == 0u) ? 1u : cli_replay_thread_of(g_cli_replay->at);
}

unsigned int cli_recording_last_of(unsigned int serial, unsigned int at)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    if ((serial >= recording->frame_most) || (recording->frames[serial].count == 0u))
    {
        return CLI_RECORDING_NO_STEP;
    }
    const CliRecordedFrame *const frame = &recording->frames[serial];
    unsigned int low = 0u;
    unsigned int high = frame->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (recording->by_frame[frame->first + middle] <= at)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return (low == 0u) ? CLI_RECORDING_NO_STEP : recording->by_frame[frame->first + low - 1u];
}

int cli_replay_at_breakpoint(unsigned int step)
{
    char full[CLI_PATH_LONGEST];
    char key[CLI_PATH_LONGEST];
    cli_replay_full(cli_replay_file_of(g_cli_replay->recording.steps[step].frame), full);
    cli_replay_key(full, key);
    for (unsigned int file = 0u; file < g_cli_replay->break_file_count; file += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = g_cli_replay->break_files[file], .other = key, .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        for (unsigned int at = 0u; at < g_cli_replay->break_count[file]; at += 1u)
        {
            if (g_cli_replay->break_lines[file][at] == g_cli_replay->recording.steps[step].line)
            {
                return 1;
            }
        }
    }
    return 0;
}

int cli_replay_at_raise(unsigned int step)
{
    for (unsigned int at = 0u; g_cli_replay->on_raised && (at < g_cli_replay->recording.raised_count); at += 1u)
    {
        if (g_cli_replay->recording.raised[at].step == step)
        {
            return 1;
        }
    }
    return 0;
}

// whether a step stops a continue: at a breakpoint, or at a raise where raises stop the replay; and why
static int replay_stops(unsigned int step)
{
    return cli_replay_at_breakpoint(step) || cli_replay_at_raise(step);
}

static const char *replay_reason(unsigned int step)
{
    return (cli_replay_at_raise(step) && !cli_replay_at_breakpoint(step)) ? "exception" : "breakpoint";
}

// where `how` goes from where the replay stands into `step`, and the reason it stops there
const char *cli_replay_go(const char *how, unsigned int *step)
{
    const CliRecordedStep *const steps = g_cli_replay->recording.steps;
    const unsigned int last = g_cli_replay->recording.step_count - 1u;
    const unsigned int here = g_cli_replay->at;
    const unsigned int thread = cli_replay_thread_of(here);
    const unsigned int depth = steps[here].depth;
    const int forward = (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "continue", .cap = CLI_CELLS_POOL})) ||
                        (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "next", .cap = CLI_CELLS_POOL})) ||
                        (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "stepIn", .cap = CLI_CELLS_POOL})) ||
                        (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "stepOut", .cap = CLI_CELLS_POOL}));
    const int backward =
        (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "reverseContinue", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "stepBack", .cap = CLI_CELLS_POOL}));
    if (!forward && !backward)
    {
        *step = here;
        return "pause";
    }
    const int continues =
        (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "continue", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "reverseContinue", .cap = CLI_CELLS_POOL}));
    for (unsigned int one = forward ? here + 1u : here; forward ? (one <= last) : (one > 0u);
         one = forward ? one + 1u : one - 1u)
    {
        const unsigned int at = forward ? one : one - 1u;
        const int same = (cli_replay_thread_of(at) == thread);
        const int hit = continues ? replay_stops(at)
                        : (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "next", .cap = CLI_CELLS_POOL}))
                            ? (same && (steps[at].depth <= depth))
                        : (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "stepIn", .cap = CLI_CELLS_POOL})) ? same
                        : (cellul.eq(&(CatenaFinitaCfg){.src = how, .other = "stepOut", .cap = CLI_CELLS_POOL}))
                            ? (same && (steps[at].depth < depth))
                            : (same && (steps[at].depth <= depth));
        if (hit)
        {
            *step = at;
            return continues ? replay_reason(at) : "step";
        }
    }
    *step = forward ? last : 0u;
    return forward ? "end" : "start";
}

void cli_replay_stopped_told(CliDap *dap, const char *reason)
{
    char told[1024];
    CliLine out = {told, sizeof(told), 0ull};
    const int bound = (cellul.eq(&(CatenaFinitaCfg){.src = reason, .other = "end", .cap = CLI_CELLS_POOL})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = reason, .other = "start", .cap = CLI_CELLS_POOL}));
    cli_line_text(&out, "{\"type\":\"event\",\"event\":\"stopped\",\"body\":{\"reason\":\"");
    cli_line_text(&out, bound ? "step" : reason);
    cli_line_text(&out, "\",\"description\":\"");
    if (cellul.eq(&(CatenaFinitaCfg){.src = reason, .other = "end", .cap = CLI_CELLS_POOL}))
    {
        cli_line_text(&out, "the end of the recording");
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = reason, .other = "start", .cap = CLI_CELLS_POOL}))
    {
        cli_line_text(&out, "the start of the recording");
    }
    else
    {
        cli_line_text(&out, "step ");
        cli_line_decimal(&out, (unsigned long long)g_cli_replay->at + 1ull, 1u);
        cli_line_text(&out, " of ");
        cli_line_decimal(&out, g_cli_replay->recording.step_count, 1u);
    }
    cli_line_text(&out, "\",\"threadId\":");
    cli_line_decimal(&out, cli_replay_thread(), 1u);
    cli_line_text(&out, ",\"allThreadsStopped\":true}}");
    (void)cli_dap_told(dap, out.out, out.at);
}

// a frame written as a stack frame, its line the step's, its source its file
void cli_replay_frame_written(CliLine *out, unsigned int serial, unsigned int step)
{
    const CliRecording *const recording = &g_cli_replay->recording;
    const CliRecordedFrame *const frame = &recording->frames[serial];
    char full[CLI_PATH_LONGEST];
    // a frame the recording did not see reads as one of file 0, as a frame made empty does
    cli_replay_full((frame->file < recording->file_count) ? cli_recording_name(recording->files[frame->file]) : "",
                    full);
    const char *const name = frame->seen ? cli_recording_name(frame->name) : "";
    cli_line_text(out, "{\"id\":");
    cli_line_decimal(out, serial, 1u);
    cli_line_text(out, ",\"name\":");
    cli_json_quoted_lossy(out, (const unsigned char *)name,
                          cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"line\":");
    cli_line_decimal(out, recording->steps[step].line, 1u);
    cli_line_text(out, ",\"column\":1,\"source\":{\"name\":");
    size_t end = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL});
    size_t start = end;
    while ((start > 0u) && (full[start - 1u] != '/') && (full[start - 1u] != '\\'))
    {
        start -= 1u;
    }
    if (start == end)
    {
        cli_line_text(out, "null");
    }
    else
    {
        cli_json_quoted_lossy(out, (const unsigned char *)full + start, end - start);
    }
    cli_line_text(out, ",\"path\":");
    cli_json_quoted_lossy(out, (const unsigned char *)full,
                          cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, "}}");
}

// the frames standing at a step, each its number and the step of its line then
static unsigned int s_stack[512][2];

// whether node `id` of `json` is a whole number at least 0 and below 2^63, and it into `value`
int cli_replay_whole(const CliJson *json, unsigned int id, unsigned long long *value)
{
    const char *const text = cli_json_is(json, id, CFG_JSON_NUMBER) ? json->text + json->node[id].text : NULL;
    const unsigned long long length = (text != NULL) ? json->node[id].length : 0ull;
    int whole = (length > 0ull) && (length <= 19ull);
    *value = 0ull;
    for (unsigned long long at = 0ull; whole && (at < length); at += 1ull)
    {
        whole = (text[at] >= '0') && (text[at] <= '9');
        *value = (*value * 10ull) + (unsigned long long)(text[at] - '0');
    }
    return whole && (*value <= 9223372036854775807ull);
}

// the frame `id` names, a node of the request's arguments, or the innermost where it names none or one not standing
unsigned int cli_replay_frame_of(const CliJson *json, unsigned int id)
{
    const unsigned int count = cli_recording_stack(g_cli_replay->at, s_stack, 512u);
    unsigned long long value = 0ull;
    const int whole = cli_replay_whole(json, id, &value);
    for (unsigned int at = 0u; whole && (at < count); at += 1u)
    {
        if (s_stack[at][0] == value)
        {
            return (unsigned int)value;
        }
    }
    return (count > 0u) ? s_stack[0][0] : 0u;
}
