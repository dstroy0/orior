// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_internal.h: what the recording sources share: a recorded run read from the recorder's lines, and where the
// replay stands and what it stops at
#ifndef RECORDING_INTERNAL_H
#define RECORDING_INTERNAL_H

#include "../text/json_tree.h"
#include "recording.h"

// the threads whose stacks are followed as a recording is read and the depth of each, the files and lines
// breakpoints are set at, and no step: a frame's last step where it has none, and a step asked for that is none
#define CLI_RECORDING_THREADS 256u
#define CLI_RECORDING_DEPTH 4096u
#define CLI_RECORDING_BREAK_FILES 256u
#define CLI_RECORDING_BREAK_LINES 4096u
#define CLI_RECORDING_NO_STEP 0xffffffffu

// A step of the recording: where its line record starts in the recording's text, its frame, its line, and how deep
// its frame stood
typedef struct
{
    unsigned long long record;
    unsigned int frame;
    unsigned int line;
    unsigned int depth;
} CliRecordedStep;

// A frame the recording saw: its function's name in the names, its file, the frame that called it, its thread, and
// where its steps start in the steps by frame and how many it has
typedef struct
{
    unsigned int name;
    unsigned int file;
    unsigned int parent;
    unsigned int thread;
    unsigned int first;
    unsigned int count;
    unsigned int last;
    int seen;
} CliRecordedFrame;

// An exception raised: the step it was raised at, and its kind and message in the names
typedef struct
{
    unsigned int step;
    unsigned int kind;
    unsigned int message;
} CliRecordedRaise;

// A recorded run, read from the recorder's lines in its file, which is kept mapped as it is read from: its steps, its
// frames by number, its steps by frame, its files, threads and raises, the names they take, the program's exit code,
// and whether the recording stopped short of the run's end. The steps, frames, files, threads, raises and names are
// parts of one cell, `room`, sized by a count of the file's lines by kind before they are read, and given back with
// the recording; each part's `most` is what the count found
typedef struct
{
    char path[CLI_PATH_LONGEST];
    const char *text;
    unsigned long long size;
    void *room;
    CliRecordedStep *steps;
    unsigned int step_count;
    unsigned int step_most;
    CliRecordedFrame *frames;
    unsigned int frame_most;
    unsigned int *by_frame;
    unsigned int *files;
    unsigned int file_count;
    unsigned int file_most;
    unsigned long long *thread_ids;
    unsigned int *thread_names;
    unsigned int thread_count;
    unsigned int thread_most;
    CliRecordedRaise *raised;
    unsigned int raised_count;
    unsigned int raised_most;
    char *names;
    unsigned int used;
    unsigned long long names_most;
    int has_code;
    long long code;
    int cut;
} CliRecording;

// A value a frame's name holds as a step was about to run, the name and the value in a room of their own
typedef struct
{
    char name[256];
    char value[2048];
    int has;
} CliRecordedValue;

// Where the replay stands and what it stops at: the tree, the program, its Python, the recording, the step, the
// breakpoints by file, and whether it stops at raises
typedef struct
{
    char root[CLI_PATH_LONGEST];
    char program[CLI_PATH_LONGEST];
    char python[CLI_PATH_LONGEST];
    CliRecording recording;
    unsigned int at;
    char break_files[CLI_RECORDING_BREAK_FILES][CLI_PATH_LONGEST];
    unsigned int break_lines[CLI_RECORDING_BREAK_FILES][CLI_RECORDING_BREAK_LINES];
    unsigned int break_count[CLI_RECORDING_BREAK_FILES];
    unsigned int break_file_count;
    int on_raised;
} CliReplay;

// the replay, a cell taken as a replay first begins or a recording is first read, and kept for the replays after it;
// and it taken where it is not: 1, or 0 where no room for it is left (recording_read.c, recording_request.c)
extern CliReplay *g_cli_replay;
int cli_replay_held(void);

// the shim a recorded run runs under: its out file, the tree's folder, the program, then the program's words
// (recording_shim.c)
extern const char g_cli_recording_shim[];

// the recording the recorder wrote at `path` read into the replay's, the file kept until the next is read; the
// recording let go; a text of its names; the last step of frame `serial` at or before step `at`, CLI_RECORDING_NO_STEP
// for none; the values of frame `serial`'s names as step `at` was about to run, by name, `most` of them at the most, in
// a cell of their count into `taken`, given back with cli_cell_given: their count; the frames standing at step `at`,
// the innermost first, each frame's number and the step of its line then: their count; and each step where `name` of
// frame `serial` took a new value, the line that gave it and the value, handed to `each` (recording_read.c,
// recording_values.c, the last step recording_replay.c)
void cli_recording_read(const char *path);
void cli_recording_let_go(void);
const char *cli_recording_name(unsigned int at);
// the line record that starts at byte `at` of the recording read into a tree sized to it, held until the next is read
// or the recording let go: the tree, the record's array into `record`, CLI_JSON_NONE where it is none
// (recording_read.c)
const CliJson *cli_recording_record(unsigned long long at, unsigned int *record);
unsigned int cli_recording_last_of(unsigned int serial, unsigned int at);
unsigned int cli_recording_values(unsigned int serial, unsigned int at, CliRecordedValue **taken, unsigned int most);
unsigned int cli_recording_stack(unsigned int at, unsigned int (*frames)[2], unsigned int room);
void cli_recording_history(unsigned int serial, const char *name,
                           void (*each)(void *context, unsigned int step, unsigned int line, const char *value),
                           void *context);

// the run the replay was recording ended, where it goes on; the same, then waited for until its readers end; the
// program run under the shim, its output told the client by `dap` as it comes, and the recording handed `dap` as the
// run ends: 1, or 0 with why said, a run the replay was recording before ended first; and the recording the message
// `message` of `json` names read, what it holds told, and the replay stopped at its first stop (recording_serve.c)
void cli_recording_run_killed(void);
void cli_recording_run_ended(void);
int cli_recording_run(CliDap *dap, CliLine *said);
void cli_recording_recorded(CliDap *dap, const CliJson *json, unsigned int message);

// the answer to `command` with the `arguments` node of `json`, each event it sends the client sent by `dap`, written
// into `body` as JSON: 1, or 0 with why said (recording_answer.c)
int cli_replay_answer(CliDap *dap, const char *command, const CliJson *json, unsigned int arguments, CliLine *body,
                      CliLine *said);

// the stop at `reason` told the client; whether a step is at a breakpoint, and at a raise where raises stop the replay;
// a path as the replay compares it, into `key`; the full path of tree path `relative`, into `full`; the tree path of
// frame `serial`'s file, "" where the recording saw none; the thread of a step, 0 where it names none, and the thread
// the replay stands on; where `how` goes from the step the replay stands at, into `step`, and why it stops there; frame
// `serial` written as a stack frame at `step`; whether node `id` of `json` is a whole number from 0 below 2^63, it into
// `value`; and the frame `id` names, or the innermost where it names none or one not standing (recording_replay.c)
void cli_replay_stopped_told(CliDap *dap, const char *reason);
int cli_replay_at_breakpoint(unsigned int step);
int cli_replay_at_raise(unsigned int step);
void cli_replay_key(const char *path, char *key);
void cli_replay_full(const char *relative, char *full);
const char *cli_replay_file_of(unsigned int serial);
unsigned int cli_replay_thread_of(unsigned int step);
unsigned int cli_replay_thread(void);
const char *cli_replay_go(const char *how, unsigned int *step);
void cli_replay_frame_written(CliLine *out, unsigned int serial, unsigned int step);
int cli_replay_whole(const CliJson *json, unsigned int id, unsigned long long *value);
unsigned int cli_replay_frame_of(const CliJson *json, unsigned int id);

#endif
