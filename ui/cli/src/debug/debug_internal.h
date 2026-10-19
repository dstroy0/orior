// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_internal.h: what the debug sources share: the sessions, each in a place of its own, the adapter it speaks to,
// what it is, and the marks its stops are read by
#ifndef DEBUG_INTERNAL_H
#define DEBUG_INTERNAL_H

#include "../builds/compile_db.h"
#include "../command_line/cells.h"
#include "../text/pieces.h"
#include "debug.h"

// the most requests asked of adapters at once
#define CLI_DEBUG_ANSWERS 4u

// the most sessions at once, one adapter each; the exceptions one adapter can stop on; the breakpoints the reader
// placed that a session keeps, and its data breakpoints; and the bytes of an adapter's event read again as a tree, and
// its nodes
#define CLI_DEBUG_SESSIONS CLI_DAPS_MOST
#define CLI_DEBUG_FILTERS 32u
#define CLI_DEBUG_PLACED 4096u
#define CLI_DEBUG_DATA 256u
#define CLI_DEBUG_EVENT (1u << 16u)
#define CLI_DEBUG_EVENT_NODES 1024u
// the nodes and bytes of the adapter's spec and the launch's arguments a session is started with
#define CLI_DEBUG_START_NODES 8192u
#define CLI_DEBUG_START_TEXT (1u << 18u)

// how long, in milliseconds, the adapter has to start, to be ready for breakpoints, and to launch the program; a
// request about the stopped program may take; after it is initialized an adapter has to say it is ready before the
// launch, as gdb does; and a build for debugging may take
#define CLI_DEBUG_STARTING 30000ull
#define CLI_DEBUG_ASKING 10000ull
#define CLI_DEBUG_EARLY 150ull
#define CLI_DEBUG_BUILDING 300000ull

// the name of the breakpoints a step from Python into C sets on the tree's native code
#define CLI_DEBUG_ACROSS "orior_across"

// An exception the adapter can stop on: its name to the adapter, its label, and whether it is on
typedef struct
{
    char filter[CLI_NAME_LONGEST];
    char label[CLI_NAME_LONGEST];
    int on;
} CliDebugFilter;

// A session as the window shows it: its number, its name, the toolchain debugging it, its language, the session that
// started it, the exceptions it can stop on, what its adapter can do, the number of the process it debugs once the
// adapter says it, and whether it steps backward and is a recorded run
typedef struct
{
    unsigned long long id;
    char name[CLI_PATH_LONGEST];
    char tool[CLI_NAME_LONGEST];
    char language[CLI_NAME_LONGEST];
    int has_parent;
    unsigned long long parent;
    CliDebugFilter filters[CLI_DEBUG_FILTERS];
    unsigned int filter_count;
    int conditions;
    int hits;
    int logs;
    int memory;
    int disassembly;
    int data;
    int exception_info;
    int has_process;
    unsigned int process;
    int step_back;
    int recorded;
} CliDebugInfo;

// A session's place: whether it is taken, listed among the sessions, its adapter's last event heard, and how many use
// it, the session's own use one of them; what it is; its adapter and the adapter's program; its data breakpoints, set
// again whole with each one added; the breakpoints the reader placed, and whether a data breakpoint is set through
// lldb's own watchpoints, whose stops come at breakpoints of no such number; whether orior's script is loaded into
// lldb; the native session whose breakpoints of a step across go at the next stop; the process the adapter says it
// debugs; whether the adapter said it is initialized; where its events go; the room an event is read again in; the
// threads that let the adapter go and take a step across's breakpoints away, and the session whose breakpoints they
// take; and the room the session's start is made in
typedef struct
{
    int taken;
    int listed;
    int heard_end;
    unsigned int users;
    CliDebugInfo info;
    CliDap *dap;
    char program[CLI_PATH_LONGEST];
    char data[CLI_DEBUG_DATA][CLI_NAME_LONGEST];
    unsigned int data_count;
    long long placed[CLI_DEBUG_PLACED];
    unsigned int placed_count;
    int watching;
    int scripted;
    int has_across;
    unsigned long long across;
    unsigned long long pid;
    int initialized;
    CliDebugEmit emit;
    CliJsonNode event_nodes[CLI_DEBUG_EVENT_NODES];
    CfgJsonToken event_tokens[CLI_DEBUG_EVENT_NODES];
    char event_text[CLI_DEBUG_EVENT];
    char event_out[CLI_DEBUG_EVENT];
    CliThread giver;
    int giving;
    CliThread clearer;
    int clearing;
    unsigned long long cleared;
    CliJsonNode start_nodes[CLI_DEBUG_START_NODES];
    char start_text[CLI_DEBUG_START_TEXT];
    CliJson start;
} CliDebugSession;

// The sessions, the number the next takes, their lock and the wake of a session's adapter saying it is initialized
typedef struct
{
    CliDebugSession sessions[CLI_DEBUG_SESSIONS];
    unsigned long long next;
    CliLock lock;
    CliWake wake;
} CliDebugRoom;

extern CliDebugRoom g_cli_debug;

// A request asked of an adapter: whether it is taken; its arguments as written, held in pieces over a piece of its own
// and joined as it is asked; the answer's body, in a cell of its size; and the body read as a tree taken to its size,
// its root
typedef struct
{
    int taken;
    CliPieces pieces;
    char piece[1024];
    CliLine asked;
    char *body;
    CliJson json;
    unsigned int root;
} CliDebugAnswer;

// a request's room taken, waiting while every one is in use, its arguments empty to write into `asked`; `command`
// asked of the session taken with those arguments, an empty object where none are written, its answer waited for up
// to `wait` milliseconds and read: 1, or 0 with why said; and the room given back (debug_session.c)
CliDebugAnswer *cli_debug_answer_taken(void);
int cli_debug_asked(CliDebugSession *session, const char *command, CliDebugAnswer *answer, unsigned long long wait,
                    CliLine *said);
void cli_debug_answer_given(CliDebugAnswer *answer);

// session `id` listed, taken for a request, its adapter held: NULL with "that session has ended" said where it is not
// listed; a session taken let go, its adapter given back, its place freed where it was the last use of a session no
// longer listed whose adapter ended; a free place taken for a session, NULL where every one is taken; and what a
// session is written as the window reads it (debug_session.c)
CliDebugSession *cli_debug_taken(unsigned long long id, CliLine *said);
void cli_debug_let_go(CliDebugSession *session);
CliDebugSession *cli_debug_place(void);
void cli_debug_info_written(const CliDebugInfo *info, CliLine *out);
// the native session debugging the same process as Python session `id`, taken, NULL where there is none
CliDebugSession *cli_debug_native_of(unsigned long long id);

// the breakpoints of `file` of the array `breakpoints` of `json` set through a session taken, each placed one's number
// kept, each line and whether it is bound written into `out` as pairs: 1, or 0 with why said (debug_session.c)
int cli_debug_set(CliDebugSession *session, const char *file, const CliJson *json, unsigned int breakpoints,
                  CliLine *out, CliLine *said);

// whether node `node` of `json` is a whole number, it into `value`, and the same of one at least 0; whether it is
// true; the text of a string node, `fallback` where it is none; whether a session in `language` debugs the file at
// `file`; `bytes` from base64, `size` of them, into `out`, which holds `room`: their count; the node `from` of `json`
// made in `to` with each "{name}" in its strings filled in from `names` and `withs`, `count` of them, and each string
// of digits alone made a number where `numbered`; the arguments that set stepping for a session in `language` put in
// the object `arguments` of `to`, as the object `stepping` of `json` asks; and the breakpoints of the array
// `breakpoints` of `json` written as the protocol writes them (debug_arguments.c)
int cli_debug_signed(const CliJson *json, unsigned int node, long long *value);
int cli_debug_unsigned(const CliJson *json, unsigned int node, unsigned long long *value);
int cli_debug_true(const CliJson *json, unsigned int node);
const char *cli_debug_text(const CliJson *json, unsigned int node, const char *fallback);
int cli_debug_takes(const char *language, const char *file);
unsigned long long cli_debug_base64(const char *text, unsigned long long size, unsigned char *out,
                                    unsigned long long room);
unsigned int cli_debug_filled(CliJson *to, const CliJson *json, unsigned int from, const char *const *names,
                              const char *const *withs, unsigned int count, int numbered);
void cli_debug_step_arguments(const char *language, const CliJson *json, unsigned int stepping, CliJson *to,
                              unsigned int arguments);
void cli_debug_breakpoints_written(const CliJson *json, unsigned int breakpoints, CliLine *out);
// the stack frame at node `frame` of an adapter's answer written as the window reads it, `{"id", "name", "path",
// "line", "col", "ip"}`, its line and column counted from 0 and its path written by `named` (debug_requests.c)
void cli_debug_frame_written(const CliJson *json, unsigned int frame, CliDebugPathWritten named, void *context,
                             CliLine *out);

// the adapter for `language` that is found, the toolchain `ahead` names tried first, its spec made in `json` from the
// manifest, its node, the toolchain's name and id, and the program's path, each into what holds its longest: 1, or 0
// with why said; the program a CMake or Meson build makes from `made`, built as that build builds it, into `program`;
// and the file at `path` built for debugging where its language is built, by the toolchain `ahead` names where it
// builds it and the first found where not, the program into `program`: 1 with `built` set, 1 with it clear for a
// language that runs from its source, or 0 with why said (debug_find.c)
int cli_debug_adapter(const char *language, const char *ahead, CliJson *json, unsigned int *spec, char *tool,
                      char *tool_id, char *program, CliLine *said);
int cli_debug_build_program(const CliProgram *made, char *program, CliLine *said);
int cli_debug_build(const char *root, const char *path, const char *language, const char *ahead, char *program,
                    int *built, CliLine *said);

// an adapter's event heard for the session in place `context`; a stop at a breakpoint the reader did not place, in a
// session whose data breakpoints are lldb's own watchpoints, told as the stop of a data breakpoint, which it is
// (debug_heard.c)
void cli_debug_heard(void *context, const char *event, const char *body, unsigned long long size);
// `text` run as a command of lldb's own in the session taken, what it says into `out`: 1, or 0 with why said, a
// command that failed said in the answer's text; and orior's script loaded into lldb in the session taken, once
// (debug_step.c)
int cli_debug_lldb_command(CliDebugSession *session, const char *text, CliLine *out, CliLine *said);
int cli_debug_lldb_script(CliDebugSession *session, CliLine *said);

// orior's script in lldb (debug_lldb.c)
extern const char g_cli_debug_lldb_script[];

#endif
