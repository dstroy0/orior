// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording_internal.h: what the recording sources share: a recorded run read from the recorder's lines, and where the
// replay is and what it stops at
#ifndef RECORDING_INTERNAL_H
#define RECORDING_INTERNAL_H
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"
#include "recording.h"

typedef struct CliRecordingDapCtx CliRecordingDapCtx;
typedef struct CliRecordingAtCtx CliRecordingAtCtx;
typedef struct CliRecordingSerialCtx CliRecordingSerialCtx;
typedef struct CliRecordingDapJsonCtx CliRecordingDapJsonCtx;
typedef struct CliRecordingAtFramesCtx CliRecordingAtFramesCtx;
typedef struct CliRecordingSerialStepCtx CliRecordingSerialStepCtx;
typedef struct CliRecordingJsonCtx CliRecordingJsonCtx;
typedef struct CliRecordingCtx CliRecordingCtx;
typedef struct CliRecordingRelativeCtx CliRecordingRelativeCtx;
typedef struct CliRecordingHowCtx CliRecordingHowCtx;
typedef struct CliRecordingPathCtx CliRecordingPathCtx;
typedef struct CliRecordingDapReasonCtx CliRecordingDapReasonCtx;
typedef struct CliRecordingIndexCtx CliRecordingIndexCtx;

// the threads whose stacks are followed as a recording is read and the depth of each, the files and lines
// breakpoints are set at, the max bytes of one name read out of the recording, and no step: a frame's last step
// where it has none, and a step requested that is none
#define CLI_RECORDING_THREADS 256u
#define CLI_RECORDING_DEPTH 4096u
#define CLI_RECORDING_BREAK_FILES 256u
#define CLI_RECORDING_BREAK_LINES 4096u
#define CLI_RECORDING_NAME_BYTES 4096u
#define CLI_RECORDING_NO_STEP 0xffffffffu

// A step of the recording: where its line record starts in the recording's text, its frame, its line, and how deep
// its frame was
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

// An exception raised: the step it was raised at, and its type and message in the names
typedef struct
{
    unsigned int step;
    unsigned int type;
    unsigned int message;
} CliRecordedRaise;

// A recorded run, read from the recorder's lines in its file: its steps, its frames by number, its steps by frame, its
// files, threads and raises, the names they take, the lines they were read from, the program's exit code, and whether
// the recording stopped short of the run's end. The steps, frames, steps by frame, files, threads, raises, names and
// the text are parts of one movable cell, `cell`, the replay owns, each part at its place from the cell's start, sized
// by a count of the file's lines by type before they are read, and given back with the recording; each part's `max` is
// what the count found. The text's lines end with zero bytes, so each is as one text at the place its step names
typedef struct
{
    char path[CLI_PATH_MAX_LEN];
    const CarcerMovableEntry *cell;
    unsigned long long text_at;
    unsigned long long size;
    unsigned long long steps_at;
    unsigned int step_count;
    unsigned int step_max;
    unsigned long long frames_at;
    unsigned int frame_max;
    unsigned long long by_frame_at;
    unsigned long long files_at;
    unsigned int file_count;
    unsigned int file_max;
    unsigned long long thread_ids_at;
    unsigned long long thread_names_at;
    unsigned int thread_count;
    unsigned int thread_max;
    unsigned long long raised_at;
    unsigned int raised_count;
    unsigned int raised_max;
    unsigned long long names_at;
    unsigned int used;
    unsigned long long names_max;
    int has_code;
    long long code;
    int cut;
} CliRecording;

// A value a frame's name contains as a step was about to run, the name and the value in a capacity of their own
typedef struct
{
    char name[256];
    char value[2048];
    int has;
} CliRecordedValue;

// Where the replay is and what it stops at: the tree, the program, its Python, the recording, the step, the
// breakpoints by file, and whether it stops at raises
typedef struct
{
    char root[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    char python[CLI_PATH_MAX_LEN];
    CliRecording recording;
    unsigned int at;
    char break_files[CLI_RECORDING_BREAK_FILES][CLI_PATH_MAX_LEN];
    unsigned int break_lines[CLI_RECORDING_BREAK_FILES][CLI_RECORDING_BREAK_LINES];
    unsigned int break_count[CLI_RECORDING_BREAK_FILES];
    unsigned int break_file_count;
    int on_raised;
} CliReplay;

// the replay, one for the process, which is fixed outside the pool from the first replay begun or recording read;
// and it made ready where it is not: 1 (recording_read.c, recording_request.c)
extern CliReplay *g_cli_replay;
int cli_replay_lock(void);

// the shim a recorded run runs under: its out file, the tree's folder, the program, then the program's words
// (recording_shim.c)
extern const char g_cli_recording_shim[];

// the recording the recorder wrote at `path` read into the replay's, its file's lines retained in the recording's cell; the
// recording let go; the name at place `at` of its names copied into `name_out`, sized `name_capacity`, cut where it is
// longer: `name_out`; the last step of frame `serial` at or before step `at`, CLI_RECORDING_NO_STEP for none; the
// values of frame `serial`'s names as step `at` was about to run, by name, `max` of them at max, in a cell of
// their count into `reserved`, given back with g_cli_guard->persistent_buf_release: their count; the frames located
// at step `at`, the innermost first, each frame's number and the step of its line then: their count; and each step
// where `name` of frame `serial` took a new value, the line that gave it and the value, passed to `each`
// (recording_read.c, recording_values.c, the last step recording_replay.c)
void cli_recording_read(const CliRecordingPathCtx *args);
void cli_recording_let_go(void);
const char *cli_recording_name(const CliRecordingAtFramesCtx *args);
// the recording's step, frame, step by frame, file's name place, thread's id, thread's name place and raise at
// `index`, each read out of the recording's cell, zero where the recording has no cell (recording_read.c)
CliRecordedStep cli_recording_step(const CliRecordingIndexCtx *args);
CliRecordedFrame cli_recording_frame(const CliRecordingIndexCtx *args);
unsigned int cli_recording_by_frame(const CliRecordingIndexCtx *args);
unsigned int cli_recording_file(const CliRecordingIndexCtx *args);
unsigned long long cli_recording_thread_id(const CliRecordingIndexCtx *args);
unsigned int cli_recording_thread_name(const CliRecordingIndexCtx *args);
CliRecordedRaise cli_recording_raise(const CliRecordingIndexCtx *args);
// the line record that starts at byte `at` of the recording read into `json`, a tree sized to it in a cell of the
// call's own, which the caller gives back with g_cli_guard->persistent_buf_release: the record's array into `record`,
// CLI_JSON_NONE where it is none (recording_read.c)
void cli_recording_record(const CliRecordingCtx *args);
unsigned int cli_recording_last_get(const CliRecordingAtCtx *args);
unsigned int cli_recording_values(const CliRecordingAtCtx *args);
unsigned int cli_recording_stack(const CliRecordingAtFramesCtx *args);
void cli_recording_history(const CliRecordingSerialCtx *args);

// the run the replay was recording ended, where it goes on; the same, then waited for until its readers end; the
// program run under the shim, its output sent to the client by `dap` as it comes, and the recording passed `dap` as the
// run ends: 1, or 0 with why given, a run the replay was recording before ended first; and the recording the message
// `message` of `json` names read, what it contains sent, and the replay stopped at its first stop (recording_serve.c)
void cli_recording_run_kill(void);
void cli_recording_run_end(void);
int cli_recording_run(const CliRecordingDapCtx *args);
void cli_recording_on_record(const CliRecordingDapJsonCtx *args);

// the answer to `command` with the `arguments` node of `json`, each event it sends the client sent by `dap`, written
// into `body` as JSON: 1, or 0 with why given (recording_answer.c)
int cli_replay_answer(const CliRecordingDapCtx *args);

// the stop at `reason` reported to the client; whether a step is at a breakpoint, and at a raise where raises stop the
// replay; a path as the replay compares it, into `key`; the full path of tree path `relative`, into `full`; the tree
// path of frame `serial`'s file into `path_out`, sized CLI_PATH_MAX_LEN, "" where the recording saw none: `path_out`;
// the thread of a step, 0 where it names none, and the
// thread the replay is on; where `how` goes from the step the replay is at, into `step`, and why it stops
// there; frame `serial` written as a stack frame at `step`; whether node `id` of `json` is a whole number from 0 below
// 2^63, it into `value`; and the frame `id` names, or the innermost where it names none or one not located
// (recording_replay.c)
void cli_replay_stopped_send(const CliRecordingDapReasonCtx *args);
int cli_replay_at_breakpoint(const CliRecordingSerialStepCtx *args);
int cli_replay_at_raise(const CliRecordingSerialStepCtx *args);
void cli_replay_key(const CliRecordingPathCtx *args);
void cli_replay_full(const CliRecordingRelativeCtx *args);
const char *cli_replay_file_get(const CliRecordingSerialStepCtx *args);
unsigned int cli_replay_thread_get(const CliRecordingSerialStepCtx *args);
unsigned int cli_replay_thread(void);
const char *cli_replay_go(const CliRecordingHowCtx *args);
void cli_replay_frame_write(const CliRecordingSerialStepCtx *args);
int cli_replay_integer(const CliRecordingJsonCtx *args);
unsigned int cli_replay_frame_get(const CliRecordingJsonCtx *args);

struct CliRecordingDapCtx
{
    CliDap *dap;
    VerbaLinea *err_msg;
    const CliJson *json;
    const char *command;
    unsigned int arguments;
    VerbaLinea *body;
};

struct CliRecordingAtCtx
{
    unsigned int at;
    unsigned int serial;
    CliRecordedValue **reserved;
    unsigned int max;
    const CellOwner *owner;
};

struct CliRecordingSerialCtx
{
    unsigned int serial;
    const char *name;
    void (*each)(const CliEachStepLineValueCfg *args);
    void *context;
};

struct CliRecordingDapJsonCtx
{
    CliDap *dap;
    const CliJson *json;
    unsigned int message;
};

struct CliRecordingAtFramesCtx
{
    unsigned int at;
    unsigned int (*frames)[2];
    unsigned int capacity;
    char *name_out;
    unsigned long long name_capacity;
};

struct CliRecordingSerialStepCtx
{
    unsigned int serial;
    unsigned int step;
    VerbaLinea *out;
    char *path_out;
};

struct CliRecordingIndexCtx
{
    unsigned int index;
};

struct CliRecordingJsonCtx
{
    const CliJson *json;
    unsigned int id;
    unsigned long long *value;
};

struct CliRecordingCtx
{
    unsigned long long at_u64;
    unsigned int *record;
    CliJson *json;
};

struct CliRecordingRelativeCtx
{
    const char *relative;
    char *full;
};

struct CliRecordingHowCtx
{
    const char *how;
    unsigned int *step_pointer;
};

struct CliRecordingPathCtx
{
    const char *path;
    char *key;
};

struct CliRecordingDapReasonCtx
{
    CliDap *dap;
    const char *reason;
};

typedef struct
{
    void (*read)(const CliRecordingPathCtx *args);
    void (*replay_key)(const CliRecordingPathCtx *args);
} CliRecordingPathNs;
EMBED_TABLE_LAYOUT(CliRecordingPathNs, read, replay_key);
EMBED_TABLE_STORAGE CliRecordingPathNs cli_recording_path_internal EMBED_UNUSED = {
    .read = cli_recording_read,
    .replay_key = cli_replay_key,
};

typedef struct
{
    const char *(*name)(const CliRecordingAtFramesCtx *args);
    unsigned int (*stack)(const CliRecordingAtFramesCtx *args);
} CliRecordingAtFramesNs;
EMBED_TABLE_LAYOUT(CliRecordingAtFramesNs, name, stack);
EMBED_TABLE_STORAGE CliRecordingAtFramesNs cli_recording_at_frames_internal EMBED_UNUSED = {
    .name = cli_recording_name,
    .stack = cli_recording_stack,
};

typedef struct
{
    void (*record)(const CliRecordingCtx *args);
} CliRecordingNsTable;
EMBED_TABLE_LAYOUT(CliRecordingNsTable, record);
EMBED_TABLE_STORAGE CliRecordingNsTable cli_recording_internal EMBED_UNUSED = {
    .record = cli_recording_record,
};

typedef struct
{
    CliRecordedStep (*step)(const CliRecordingIndexCtx *args);
    CliRecordedFrame (*frame)(const CliRecordingIndexCtx *args);
    unsigned int (*by_frame)(const CliRecordingIndexCtx *args);
    unsigned int (*file)(const CliRecordingIndexCtx *args);
    unsigned long long (*thread_id)(const CliRecordingIndexCtx *args);
    unsigned int (*thread_name)(const CliRecordingIndexCtx *args);
    CliRecordedRaise (*raise)(const CliRecordingIndexCtx *args);
} CliRecordingIndexNs;
EMBED_TABLE_LAYOUT(CliRecordingIndexNs, step, frame, by_frame, file, thread_id, thread_name, raise);
EMBED_TABLE_STORAGE CliRecordingIndexNs cli_recording_index_internal EMBED_UNUSED = {
    .step = cli_recording_step,
    .frame = cli_recording_frame,
    .by_frame = cli_recording_by_frame,
    .file = cli_recording_file,
    .thread_id = cli_recording_thread_id,
    .thread_name = cli_recording_thread_name,
    .raise = cli_recording_raise,
};

typedef struct
{
    unsigned int (*last_get)(const CliRecordingAtCtx *args);
    unsigned int (*values)(const CliRecordingAtCtx *args);
} CliRecordingAtNs;
EMBED_TABLE_LAYOUT(CliRecordingAtNs, last_get, values);
EMBED_TABLE_STORAGE CliRecordingAtNs cli_recording_at_internal EMBED_UNUSED = {
    .last_get = cli_recording_last_get,
    .values = cli_recording_values,
};

typedef struct
{
    void (*history)(const CliRecordingSerialCtx *args);
} CliRecordingSerialNs;
EMBED_TABLE_LAYOUT(CliRecordingSerialNs, history);
EMBED_TABLE_STORAGE CliRecordingSerialNs cli_recording_serial_internal EMBED_UNUSED = {
    .history = cli_recording_history,
};

typedef struct
{
    int (*run)(const CliRecordingDapCtx *args);
    int (*replay_answer)(const CliRecordingDapCtx *args);
} CliRecordingDapNs;
EMBED_TABLE_LAYOUT(CliRecordingDapNs, run, replay_answer);
EMBED_TABLE_STORAGE CliRecordingDapNs cli_recording_dap_internal EMBED_UNUSED = {
    .run = cli_recording_run,
    .replay_answer = cli_replay_answer,
};

typedef struct
{
    void (*record)(const CliRecordingDapJsonCtx *args);
} CliRecordingDapJsonNs;
EMBED_TABLE_LAYOUT(CliRecordingDapJsonNs, record);
EMBED_TABLE_STORAGE CliRecordingDapJsonNs cli_recording_dap_json_internal EMBED_UNUSED = {
    .record = cli_recording_on_record,
};

typedef struct
{
    void (*replay_stopped_send)(const CliRecordingDapReasonCtx *args);
} CliRecordingDapReasonNs;
EMBED_TABLE_LAYOUT(CliRecordingDapReasonNs, replay_stopped_send);
EMBED_TABLE_STORAGE CliRecordingDapReasonNs cli_recording_dap_reason_internal EMBED_UNUSED = {
    .replay_stopped_send = cli_replay_stopped_send,
};

typedef struct
{
    int (*replay_at_breakpoint)(const CliRecordingSerialStepCtx *args);
    int (*replay_at_raise)(const CliRecordingSerialStepCtx *args);
    const char *(*replay_file_get)(const CliRecordingSerialStepCtx *args);
    unsigned int (*replay_thread_get)(const CliRecordingSerialStepCtx *args);
    void (*replay_frame_write)(const CliRecordingSerialStepCtx *args);
} CliRecordingSerialStepNs;
EMBED_TABLE_LAYOUT(CliRecordingSerialStepNs, replay_at_breakpoint, replay_at_raise, replay_file_get, replay_thread_get,
                   replay_frame_write);
EMBED_TABLE_STORAGE CliRecordingSerialStepNs cli_recording_serial_step_internal EMBED_UNUSED = {
    .replay_at_breakpoint = cli_replay_at_breakpoint,
    .replay_at_raise = cli_replay_at_raise,
    .replay_file_get = cli_replay_file_get,
    .replay_thread_get = cli_replay_thread_get,
    .replay_frame_write = cli_replay_frame_write,
};

typedef struct
{
    void (*replay_full)(const CliRecordingRelativeCtx *args);
} CliRecordingRelativeNs;
EMBED_TABLE_LAYOUT(CliRecordingRelativeNs, replay_full);
EMBED_TABLE_STORAGE CliRecordingRelativeNs cli_recording_relative_internal EMBED_UNUSED = {
    .replay_full = cli_replay_full,
};

typedef struct
{
    const char *(*replay_go)(const CliRecordingHowCtx *args);
} CliRecordingHowNs;
EMBED_TABLE_LAYOUT(CliRecordingHowNs, replay_go);
EMBED_TABLE_STORAGE CliRecordingHowNs cli_recording_how_internal EMBED_UNUSED = {
    .replay_go = cli_replay_go,
};

typedef struct
{
    int (*replay_integer)(const CliRecordingJsonCtx *args);
    unsigned int (*replay_frame_get)(const CliRecordingJsonCtx *args);
} CliRecordingJsonNs;
EMBED_TABLE_LAYOUT(CliRecordingJsonNs, replay_integer, replay_frame_get);
EMBED_TABLE_STORAGE CliRecordingJsonNs cli_recording_json_internal EMBED_UNUSED = {
    .replay_integer = cli_replay_integer,
    .replay_frame_get = cli_replay_frame_get,
};

#endif
