// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug.h: debugging through debug adapters: lldb-dap for C, C++ and Rust, gdb where lldb-dap is not found, and debugpy
// for Python, as each toolchain's `debugger` in toolchains.json names it. A file of a compiled language is first built
// for debugging, by the `builds` of the toolchain that has its language, into build/debug/, and the program built is
// what runs.
//
// More than one session runs at once, each by its number, each its own adapter with its own threads and stack. A
// session starts by launching a file, by launching pytest on one test, by attaching to a process by its number or to a
// debugger listening at an address, or as the child a session's program started, which debugpy requests by its
// `debugpyAttach` event. A session goes as the protocol has it: the adapter is started and informed of who requests,
// the program is launched or attached to, the breakpoints and the exceptions to stop on are set once the adapter
// reports it is ready for them, and the program runs. From then on the adapter's events go to the function the session
// was started with, with the session's number, and the stack, the variables, the memory, the instructions and the steps
// are requested from the session named.
//
// A breakpoint stops only where its condition is true, or at the count of hits it gives, and one with a message writes
// the message in place of stopping. A file's breakpoints go to each session that debugs its language. Stepping stays
// to the tree's own code where the tree requests it, and passes over the code its patterns name: by debugpy's rules for
// Python, and by lldb's step-avoid pattern for the rest.
//
// Lines here count from 0, as the editor counts them, and the adapter is informed they count from 1. What a call gives
// is written as JSON, as the window reads it (debug_session.c, debug_start.c, debug_launch.c, debug_events.c,
// debug_find.c, debug_arguments.c, debug_requests.c, debug_step.c, debug_watch.c, debug_lldb.c, debug_processes.c,
// debug_bytecode.c)
#ifndef DEBUG_H
#define DEBUG_H
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "dap.h"
#include "embed_dispatch_layout.h"

typedef struct CliDebugOutCfg CliDebugOutCfg;
typedef struct CliDebugOutErrMsgCfg CliDebugOutErrMsgCfg;
typedef struct CliDebugOutErrMsgIdCfg CliDebugOutErrMsgIdCfg;
typedef struct CliDebugRootCfg CliDebugRootCfg;
typedef struct CliDebugOutErrMsgIdReferenceCfg CliDebugOutErrMsgIdReferenceCfg;
typedef struct CliDebugOutErrMsgIdReferenceOffsetCfg CliDebugOutErrMsgIdReferenceOffsetCfg;
typedef struct CliDebugOutErrMsgIdThreadCfg CliDebugOutErrMsgIdThreadCfg;
typedef struct CliDebugRootJsonCfg CliDebugRootJsonCfg;
typedef struct CliDebugRootOutCfg CliDebugRootOutCfg;
typedef struct CliDebugJsonCfg CliDebugJsonCfg;
typedef struct CliDebugOutErrMsgIdThreadHowCfg CliDebugOutErrMsgIdThreadHowCfg;
typedef struct CliDebugRootOutErrMsgCfg CliDebugRootOutErrMsgCfg;
typedef struct CliDebugOutErrMsgIdStepCfg CliDebugOutErrMsgIdStepCfg;
typedef struct CliDebugCfg CliDebugCfg;

// what a session reports on its own: its number, an adapter's event, and `size` bytes of the event's body as JSON
typedef void (*CliDebugEventFn)(const CliDebugEventCfg *args);

// Where a session's events go: the function and its context
typedef struct
{
    CliDebugEventFn emit;
    void *context;
} CliDebugSink;

// a path an adapter named written into `out` as the JSON string the window reads it as
typedef void (*CliDebugPathWritten)(const CliDebugPathWrittenCfg *args);

// a session started as the object `start` of `json` specifies, in the tree at `root`, with the object `options`, its
// events going to `emit`: what the session is into `out`, its number first, 1, or 0 with why given. `start` is tagged
// by its "type": "file" with "path" and "language", "test" with "id", "process" with "pid", "language" and "parent",
// "address" with "host", "port" and "remote", "child" with "parent" and "config", and "record" with "path". `options`
// contains "breakpoints", by file, "exceptions", "stepping" with "ours_only" and "skip", and "entry" (debug_start.c)
int cli_debug_start(const CliDebugRootCfg *args);

// the sessions running, the first started first, into `out`; whether any is; the breakpoints of the array
// `breakpoints` of `json` set for the file at `path` in the tree, in each session that debugs its language, each line
// the first placed one is on and whether it is bound to code into `out`, null where no session debugs it; the
// number of the process session `id` debugs recorded, as its adapter's `process` event reports it; session `id`
// forgotten once its adapter reports its program ended, without querying it again; session `id` ended with its adapter,
// and those of the sessions it started; and every session ended (debug_session.c)
void cli_debug_sessions(const CliDebugOutErrMsgIdStepCfg *args);
int cli_debug_running(void);
void cli_debug_breakpoints(const CliDebugRootJsonCfg *args);
void cli_debug_process(const CliDebugCfg *args);
void cli_debug_end(const CliDebugCfg *args);
void cli_debug_stop(const CliDebugCfg *args);
void cli_debug_stop_all(void);

// What session `id` is queried, each written into `out`: 1, or 0 with why given (debug_requests.c). The exceptions it
// stops on, the array of strings `on` of `json`; the exception thread `thread` stopped on; its threads; the stack of
// `thread`, each frame's path written by `named`; the scopes of `frame`; the variables of `reference`; what
// `expression` comes to in `frame`, or with no frame where `has_frame` is 0, as a watch evaluates it, as the console
// does with `context` "repl", or as the pointer resting on a name does with "hover"; the values `name` of `frame` took
// through a recorded run, each path written by `named`; step `step` of a recorded run gone to; `count` bytes from
// `offset` past the address `reference` names; and `count` instructions from `offset` instructions past it
int cli_debug_exceptions(const CliDebugJsonCfg *args);
int cli_debug_raise(const CliDebugOutErrMsgIdThreadHowCfg *args);
int cli_debug_threads(const CliDebugOutErrMsgIdStepCfg *args);
int cli_debug_stack(const CliDebugOutErrMsgIdThreadCfg *args);
int cli_debug_scopes(const CliDebugOutErrMsgCfg *args);
int cli_debug_variables(const CliDebugOutCfg *args);
int cli_debug_evaluate(const CliDebugOutErrMsgCfg *args);
int cli_debug_history(const CliDebugOutErrMsgIdCfg *args);
int cli_debug_goto(const CliDebugOutErrMsgIdStepCfg *args);
int cli_debug_memory(const CliDebugOutErrMsgIdReferenceOffsetCfg *args);
int cli_debug_instructions(const CliDebugOutErrMsgIdReferenceCfg *args);

// Steps and stops of session `id`, each 1, or 0 with why given (debug_step.c, the stop debug_watch.c). `how`, one of
// continue, next, stepIn, stepOut, pause, stepBack and reverseContinue, for `thread`; Python session `id`'s thread
// stepped into the next line it runs, or into the tree's own C at `root` where the line calls it; and a stop where the
// bytes of `name` change: the variable of that name under the container `reference` where `has_reference`, or the
// address `name` writes, `bytes` of them, where `has_bytes`, what the adapter watches written into `out` as a JSON
// string
int cli_debug_step(const CliDebugOutErrMsgIdThreadHowCfg *args);
int cli_debug_step_across(const CliDebugRootOutErrMsgCfg *args);
int cli_debug_watch_data(const CliDebugOutCfg *args);

// the processes of the machine, by their names, each its number, its program's name and the line it was started with
// where the system reports, written into `out`, from cells sized to the processes found, counted to
void cli_debug_processes(const CliDebugOutErrMsgIdStepCfg *args);

// the bytecode of the function of the Python file at `path` that contains `line`, counted from 0, as the tree's Python
// compiles it: its name, then each instruction with the line it is of, written into `out`: 1, or 0 with why given
// (debug_bytecode.c)
int cli_debug_bytecode(const CliDebugRootOutCfg *args);

struct CliDebugOutCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long id;
    long long reference;
    const char *name;
    int has_reference;
    int has_bytes;
    unsigned long long bytes;
};

struct CliDebugOutErrMsgCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long id;
    long long frame;
    const char *expression;
    int has_frame;
    const char *context_text;
};

struct CliDebugOutErrMsgIdCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long id;
    CliDebugPathWritten named;
    void *context;
    long long frame;
    const char *name;
};

struct CliDebugRootCfg
{
    const char *root;
    const CliJson *json;
    unsigned int start;
    unsigned int options;
    CliDebugSink sink;
    VerbaLinea *out;
    VerbaLinea *err_msg;
};

struct CliDebugOutErrMsgIdReferenceCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long id;
    const char *reference_text;
    long long offset;
    long long count_i64;
};

struct CliDebugOutErrMsgIdReferenceOffsetCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long id;
    const char *reference_text;
    long long offset;
    unsigned long long count;
};

struct CliDebugOutErrMsgIdThreadCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long id;
    long long thread;
    CliDebugPathWritten named;
    void *context;
};

struct CliDebugRootJsonCfg
{
    const char *root;
    const CliJson *json;
    VerbaLinea *out;
    const char *path;
    unsigned int breakpoints;
};

struct CliDebugRootOutCfg
{
    const char *root;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *path;
    unsigned int line;
};

struct CliDebugJsonCfg
{
    const CliJson *json;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long id;
    unsigned int on;
};

struct CliDebugOutErrMsgIdThreadHowCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long id;
    long long thread;
    const char *how;
};

struct CliDebugRootOutErrMsgCfg
{
    const char *root;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long id;
    long long thread;
};

struct CliDebugOutErrMsgIdStepCfg
{
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned long long id;
    unsigned long long step;
};

struct CliDebugCfg
{
    unsigned long long id;
    unsigned int pid;
};

typedef struct
{
    int (*start)(const CliDebugRootCfg *args);
} CliDebugRootNs;
EMBED_TABLE_LAYOUT(CliDebugRootNs, start);
EMBED_TABLE_STORAGE CliDebugRootNs cli_debug_root EMBED_UNUSED = {
    .start = cli_debug_start,
};

typedef struct
{
    void (*sessions)(const CliDebugOutErrMsgIdStepCfg *args);
    int (*threads)(const CliDebugOutErrMsgIdStepCfg *args);
    int (*debug_goto)(const CliDebugOutErrMsgIdStepCfg *args);
    void (*processes)(const CliDebugOutErrMsgIdStepCfg *args);
} CliDebugOutErrMsgIdStepNs;
EMBED_TABLE_LAYOUT(CliDebugOutErrMsgIdStepNs, sessions, threads, debug_goto, processes);
EMBED_TABLE_STORAGE CliDebugOutErrMsgIdStepNs cli_debug_out_err_msg_id_step EMBED_UNUSED = {
    .sessions = cli_debug_sessions,
    .threads = cli_debug_threads,
    .debug_goto = cli_debug_goto,
    .processes = cli_debug_processes,
};

typedef struct
{
    void (*breakpoints)(const CliDebugRootJsonCfg *args);
} CliDebugRootJsonNs;
EMBED_TABLE_LAYOUT(CliDebugRootJsonNs, breakpoints);
EMBED_TABLE_STORAGE CliDebugRootJsonNs cli_debug_root_json EMBED_UNUSED = {
    .breakpoints = cli_debug_breakpoints,
};

typedef struct
{
    void (*process)(const CliDebugCfg *args);
    void (*end)(const CliDebugCfg *args);
    void (*stop)(const CliDebugCfg *args);
} CliDebugNs;
EMBED_TABLE_LAYOUT(CliDebugNs, process, end, stop);
EMBED_TABLE_STORAGE CliDebugNs cli_debug_table EMBED_UNUSED = {
    .process = cli_debug_process,
    .end = cli_debug_end,
    .stop = cli_debug_stop,
};

typedef struct
{
    int (*exceptions)(const CliDebugJsonCfg *args);
} CliDebugJsonNs;
EMBED_TABLE_LAYOUT(CliDebugJsonNs, exceptions);
EMBED_TABLE_STORAGE CliDebugJsonNs cli_debug_json EMBED_UNUSED = {
    .exceptions = cli_debug_exceptions,
};

typedef struct
{
    int (*raise)(const CliDebugOutErrMsgIdThreadHowCfg *args);
    int (*step)(const CliDebugOutErrMsgIdThreadHowCfg *args);
} CliDebugOutErrMsgIdThreadHowNs;
EMBED_TABLE_LAYOUT(CliDebugOutErrMsgIdThreadHowNs, raise, step);
EMBED_TABLE_STORAGE CliDebugOutErrMsgIdThreadHowNs cli_debug_out_err_msg_id_thread_how EMBED_UNUSED = {
    .raise = cli_debug_raise,
    .step = cli_debug_step,
};

typedef struct
{
    int (*stack)(const CliDebugOutErrMsgIdThreadCfg *args);
} CliDebugOutErrMsgIdThreadNs;
EMBED_TABLE_LAYOUT(CliDebugOutErrMsgIdThreadNs, stack);
EMBED_TABLE_STORAGE CliDebugOutErrMsgIdThreadNs cli_debug_out_err_msg_id_thread EMBED_UNUSED = {
    .stack = cli_debug_stack,
};

typedef struct
{
    int (*scopes)(const CliDebugOutErrMsgCfg *args);
    int (*evaluate)(const CliDebugOutErrMsgCfg *args);
} CliDebugOutErrMsgNs;
EMBED_TABLE_LAYOUT(CliDebugOutErrMsgNs, scopes, evaluate);
EMBED_TABLE_STORAGE CliDebugOutErrMsgNs cli_debug_out_err_msg EMBED_UNUSED = {
    .scopes = cli_debug_scopes,
    .evaluate = cli_debug_evaluate,
};

typedef struct
{
    int (*variables)(const CliDebugOutCfg *args);
    int (*watch_data)(const CliDebugOutCfg *args);
} CliDebugOutNs;
EMBED_TABLE_LAYOUT(CliDebugOutNs, variables, watch_data);
EMBED_TABLE_STORAGE CliDebugOutNs cli_debug_out EMBED_UNUSED = {
    .variables = cli_debug_variables,
    .watch_data = cli_debug_watch_data,
};

typedef struct
{
    int (*history)(const CliDebugOutErrMsgIdCfg *args);
} CliDebugOutErrMsgIdNs;
EMBED_TABLE_LAYOUT(CliDebugOutErrMsgIdNs, history);
EMBED_TABLE_STORAGE CliDebugOutErrMsgIdNs cli_debug_out_err_msg_id EMBED_UNUSED = {
    .history = cli_debug_history,
};

typedef struct
{
    int (*memory)(const CliDebugOutErrMsgIdReferenceOffsetCfg *args);
} CliDebugOutErrMsgIdReferenceOffsetNs;
EMBED_TABLE_LAYOUT(CliDebugOutErrMsgIdReferenceOffsetNs, memory);
EMBED_TABLE_STORAGE CliDebugOutErrMsgIdReferenceOffsetNs cli_debug_out_err_msg_id_reference_offset EMBED_UNUSED = {
    .memory = cli_debug_memory,
};

typedef struct
{
    int (*instructions)(const CliDebugOutErrMsgIdReferenceCfg *args);
} CliDebugOutErrMsgIdReferenceNs;
EMBED_TABLE_LAYOUT(CliDebugOutErrMsgIdReferenceNs, instructions);
EMBED_TABLE_STORAGE CliDebugOutErrMsgIdReferenceNs cli_debug_out_err_msg_id_reference EMBED_UNUSED = {
    .instructions = cli_debug_instructions,
};

typedef struct
{
    int (*step_across)(const CliDebugRootOutErrMsgCfg *args);
} CliDebugRootOutErrMsgNs;
EMBED_TABLE_LAYOUT(CliDebugRootOutErrMsgNs, step_across);
EMBED_TABLE_STORAGE CliDebugRootOutErrMsgNs cli_debug_root_out_err_msg EMBED_UNUSED = {
    .step_across = cli_debug_step_across,
};

typedef struct
{
    int (*bytecode)(const CliDebugRootOutCfg *args);
} CliDebugRootOutNs;
EMBED_TABLE_LAYOUT(CliDebugRootOutNs, bytecode);
EMBED_TABLE_STORAGE CliDebugRootOutNs cli_debug_root_out EMBED_UNUSED = {
    .bytecode = cli_debug_bytecode,
};

#endif
