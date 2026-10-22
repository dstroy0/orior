// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug.h: debugging through debug adapters: lldb-dap for C, C++ and Rust, gdb where lldb-dap is not found, and debugpy
// for Python, as each toolchain's `debugger` in toolchains.json names it. A file of a compiled language is first built
// for debugging, by the `builds` of the toolchain that has its language, into build/debug/, and the program built is
// what runs.
//
// More than one session runs at once, each by its number, each its own adapter with its own threads and stack. A
// session starts by launching a file, by launching pytest on one test, by attaching to a process by its number or to a
// debugger listening at an address, or as the child a session's program started, which debugpy asks for by its
// `debugpyAttach` event. A session goes as the protocol has it: the adapter is started and told who asks, the program
// is launched or attached to, the breakpoints and the exceptions to stop on are set once the adapter says it is ready
// for them, and the program runs. From then on the adapter's events go to the function the session was started with,
// with the session's number, and the stack, the variables, the memory, the instructions and the steps are asked of the
// session named.
//
// A breakpoint stops only where its condition holds, or at the count of hits it gives, and one with a message writes
// the message in place of stopping. A file's breakpoints go to each session that debugs its language. Stepping keeps
// to the tree's own code where the tree asks it to, and passes over the code its patterns name: by debugpy's rules for
// Python, and by lldb's step-avoid pattern for the rest.
//
// Lines here count from 0, as the editor counts them, and the adapter is told they count from 1. What a call gives is
// written as JSON, as the window reads it (debug_session.c, debug_start.c, debug_launch.c, debug_heard.c, debug_find.c,
// debug_arguments.c, debug_requests.c, debug_step.c, debug_watch.c, debug_lldb.c, debug_processes.c,
// debug_bytecode.c)
#ifndef DEBUG_H
#define DEBUG_H

#include "../text/json_tree.h"
#include "dap.h"

// what a session says on its own: its number, an adapter's event, and `size` bytes of the event's body as JSON
typedef void (*CliDebugTold)(void *context, unsigned long long session, const char *event, const char *body,
                             unsigned long long size);

// Where a session's events go: the function and its context
typedef struct
{
    CliDebugTold told;
    void *context;
} CliDebugEmit;

// a path an adapter named written into `out` as the JSON string the window reads it as
typedef void (*CliDebugPathWritten)(void *context, const char *path, CliLine *out);

// a session started as the object `start` of `json` says, in the tree at `root`, with the object `options`, its
// events going to `emit`: what the session is into `out`, its number first, 1, or 0 with why said. `start` is tagged
// by its "kind": "file" with "path" and "language", "test" with "id", "process" with "pid", "language" and "parent",
// "address" with "host", "port" and "remote", "child" with "parent" and "config", and "record" with "path". `options`
// holds "breakpoints", by file, "exceptions", "stepping" with "mine_only" and "skip", and "entry" (debug_start.c)
int cli_debug_start(const char *root, const CliJson *json, unsigned int start, unsigned int options, CliDebugEmit emit,
                    CliLine *out, CliLine *said);

// the sessions running, the first started first, into `out`; whether any is; the breakpoints of the array
// `breakpoints` of `json` set for the file at `path` in the tree, in each session that debugs its language, each line
// the first placed one is on and whether it is bound to code into `out`, null where no session debugs it; the
// number of the process session `id` debugs kept, as its adapter's `process` event says it; session `id` forgotten
// once its adapter says its program ended, without asking it again; session `id` ended with its adapter, and those of
// the sessions it started; and every session ended (debug_session.c)
void cli_debug_sessions(CliLine *out);
int cli_debug_running(void);
void cli_debug_breakpoints(const char *root, const char *path, const CliJson *json, unsigned int breakpoints,
                           CliLine *out);
void cli_debug_process(unsigned long long id, unsigned int pid);
void cli_debug_ended(unsigned long long id);
void cli_debug_stop(unsigned long long id);
void cli_debug_stop_all(void);

// What session `id` is asked, each written into `out`: 1, or 0 with why said (debug_requests.c). The exceptions it
// stops on, the array of strings `on` of `json`; the exception thread `thread` stopped on; its threads; the stack of
// `thread`, each frame's path written by `named`; the scopes of `frame`; the variables of `reference`; what
// `expression` comes to in `frame`, or with no frame where `has_frame` is 0, as a watch asks it, as the console does
// with `context` "repl", or as the pointer resting on a name does with "hover"; the values `name` of `frame` took
// through a recorded run, each path written by `named`; step `step` of a recorded run gone to; `count` bytes from
// `offset` past the address `reference` names; and `count` instructions from `offset` instructions past it
int cli_debug_exceptions(unsigned long long id, const CliJson *json, unsigned int on, CliLine *out, CliLine *said);
int cli_debug_raised(unsigned long long id, long long thread, CliLine *out, CliLine *said);
int cli_debug_threads(unsigned long long id, CliLine *out, CliLine *said);
int cli_debug_stack(unsigned long long id, long long thread, CliDebugPathWritten named, void *context, CliLine *out,
                    CliLine *said);
int cli_debug_scopes(unsigned long long id, long long frame, CliLine *out, CliLine *said);
int cli_debug_variables(unsigned long long id, long long reference, CliLine *out, CliLine *said);
int cli_debug_evaluate(unsigned long long id, const char *expression, int has_frame, long long frame,
                       const char *context, CliLine *out, CliLine *said);
int cli_debug_history(unsigned long long id, long long frame, const char *name, CliDebugPathWritten named,
                      void *context, CliLine *out, CliLine *said);
int cli_debug_goto(unsigned long long id, unsigned long long step, CliLine *out, CliLine *said);
int cli_debug_memory(unsigned long long id, const char *reference, long long offset, unsigned long long count,
                     CliLine *out, CliLine *said);
int cli_debug_instructions(unsigned long long id, const char *reference, long long offset, long long count,
                           CliLine *out, CliLine *said);

// Steps and stops of session `id`, each 1, or 0 with why said (debug_step.c, the stop debug_watch.c). `how`, one of
// continue, next, stepIn, stepOut, pause, stepBack and reverseContinue, for `thread`; Python session `id`'s thread
// stepped into the next line it runs, or into the tree's own C at `root` where the line calls it; and a stop where the
// bytes of `name` change: the variable of that name under the container `reference` where `has_reference`, or the
// address `name` writes, `bytes` of them, where `has_bytes`, what the adapter watches written into `out` as a JSON
// string
int cli_debug_step(unsigned long long id, const char *how, long long thread, CliLine *out, CliLine *said);
int cli_debug_step_across(const char *root, unsigned long long id, long long thread, CliLine *out, CliLine *said);
int cli_debug_watch_data(unsigned long long id, const char *name, int has_reference, long long reference, int has_bytes,
                         unsigned long long bytes, CliLine *out, CliLine *said);

// the processes of the machine, by their names, each its number, its program's name and the line it was started with
// where the system says, written into `out`, from cells sized to the processes found, counted to
// g_cli_debug_processes_cells and given back once written (debug_processes.c)
extern CliCellsTally g_cli_debug_processes_cells;
void cli_debug_processes(CliLine *out);

// the bytecode of the function of the Python file at `path` that holds `line`, counted from 0, as the tree's Python
// compiles it: its name, then each instruction with the line it is of, written into `out`: 1, or 0 with why said
// (debug_bytecode.c)
int cli_debug_bytecode(const char *root, const char *path, unsigned int line, CliLine *out, CliLine *said);

#endif
