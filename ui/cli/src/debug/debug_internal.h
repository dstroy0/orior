// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_internal.h: what the debug sources share: the sessions, each in a place of its own, the adapter it connects to,
// what it is, and the marks its stops are read by
#ifndef DEBUG_INTERNAL_H
#define DEBUG_INTERNAL_H
#include "../builds/compile_db.h"
#include "../command_line/pool.h"
#include "debug.h"
#include "embed_dispatch_layout.h"

typedef struct CliDebugErrMsgCtx CliDebugErrMsgCtx;
typedef struct CliDebugErrMsgLanguageCtx CliDebugErrMsgLanguageCtx;
typedef struct CliDebugJsonCtx CliDebugJsonCtx;
typedef struct CliDebugSessionCtx CliDebugSessionCtx;
typedef struct CliDebugOutCtx CliDebugOutCtx;
typedef struct CliDebugSessionCommandCtx CliDebugSessionCommandCtx;
typedef struct CliDebugJsonLanguageCtx CliDebugJsonLanguageCtx;
typedef struct CliDebugTextCtx CliDebugTextCtx;
typedef struct CliDebugSessionErrMsgCtx CliDebugSessionErrMsgCtx;
typedef struct CliDebugErrMsgProgramCtx CliDebugErrMsgProgramCtx;
typedef struct CliDebugJsonNodeCtx CliDebugJsonNodeCtx;
typedef struct CliDebugJsonNodeFallbackCtx CliDebugJsonNodeFallbackCtx;
typedef struct CliDebugJsonNodeValueU64sCtx CliDebugJsonNodeValueU64sCtx;
typedef struct CliDebugInfoCtx CliDebugInfoCtx;
typedef struct CliDebugErrMsgIdCtx CliDebugErrMsgIdCtx;
typedef struct CliDebugFileCtx CliDebugFileCtx;
typedef struct CliDebugCtx CliDebugCtx;

// the max requests sent to adapters at once, and the max bytes of a request's arguments
#define CLI_DEBUG_ANSWERS 4u
#define CLI_DEBUG_ARGS (1ull << 20u)

// the max sessions at once, one adapter each; the exceptions one adapter can stop on; the max breakpoints the reader
// placed that a session stores, and its data breakpoints, and those each list is taken with; and the nodes and bytes
// over an event's own its tree is read with, for what a stop rewritten adds
#define CLI_DEBUG_SESSIONS CLI_DAPS_MAX
#define CLI_DEBUG_FILTERS 32u
#define CLI_DEBUG_POSITIONED 4096u
#define CLI_DEBUG_DATA 256u
#define CLI_DEBUG_EVENT_MORE_NODES 8u
#define CLI_DEBUG_EVENT_MORE_TEXT 64u
// the max nodes and bytes of the adapter's spec and the launch's arguments a session is started with, the tree's cell
// taken as the session is planned and given back as its place is freed
#define CLI_DEBUG_START_NODES 8192u
#define CLI_DEBUG_START_TEXT (1u << 18u)

// how long, in milliseconds, the adapter has to start, to be ready for breakpoints, and to launch the program; a
// request about the stopped program may take; after it is initialized an adapter has to report it is ready before the
// launch, as gdb does; and a build for debugging may take
#define CLI_DEBUG_STARTING 30000ull
#define CLI_DEBUG_REQUEST_TIMEOUT_MS 10000ull
#define CLI_DEBUG_EARLY 150ull
#define CLI_DEBUG_BUILDING 300000ull

// the name of the breakpoints a step from Python into C sets on the tree's native code
#define CLI_DEBUG_ACROSS "orior_across"

// An exception the adapter can stop on: its name to the adapter, its label, and whether it is on
typedef struct
{
    char filter[CLI_NAME_MAX_LEN];
    char label[CLI_NAME_MAX_LEN];
    int on;
} CliDebugFilter;

// A session as the window shows it: its number, its name, the toolchain debugging it, its language, the session that
// started it, the exceptions it can stop on, what its adapter can do, the number of the process it debugs once the
// adapter reports it, and whether it steps backward and is a recorded run
typedef struct
{
    unsigned long long id;
    char name[CLI_PATH_MAX_LEN];
    char tool[CLI_NAME_MAX_LEN];
    char language[CLI_NAME_MAX_LEN];
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

// A session's place: whether it is taken, listed among the sessions, its adapter's last event received, and how many
// use it, the session's own use one of them; what it is; its adapter and the adapter's program; its data breakpoints,
// set again whole with each one added; the breakpoints the reader placed, and whether a data breakpoint is set through
// lldb's own watchpoints, whose stops come at breakpoints of no such number; whether orior's script is loaded into
// lldb; the native session whose breakpoints of a step across go at the next stop; the process the adapter reports it
// debugs; whether the adapter reported it is initialized; where its events go; the threads that let the adapter go and
// take a step across's breakpoints away, and the session whose breakpoints they take; and the tree the session's start
// is made in. The two lists are movable cells at their max, a data breakpoint CLI_NAME_MAX_LEN bytes and a placed
// breakpoint a number, read out by copy, and they and the tree are given back with the place
typedef struct
{
    int reserved;
    int list_entries;
    int adapter_ended;
    unsigned int users;
    CliDebugInfo info;
    CliDap *dap;
    char program[CLI_PATH_MAX_LEN];
    const CarcerMovableEntry *data_cell;
    unsigned int data_count;
    unsigned int data_capacity;
    const CarcerMovableEntry *positioned_cell;
    unsigned int positioned_count;
    unsigned int positioned_capacity;
    int watching;
    int scripted;
    int has_across;
    unsigned long long across;
    unsigned long long pid;
    int initialized;
    CliDebugSink sink;
    CliThread provider;
    int releasing;
    CliThread clearer;
    int clearing;
    unsigned long long cleared;
    CliJsonStored start;
} CliDebugSession;

// a place freed, its lists and its tree given back, under the sessions' lock (debug_position.c)

// The sessions, the number the next takes, their lock and the wake of a session's adapter reporting it is initialized
typedef struct
{
    CliDebugSession sessions[CLI_DEBUG_SESSIONS];
    unsigned long long next;
    CliWake wake;
} CliDebugWorkState;

extern CliDebugWorkState g_cli_debug;

// A request sent to an adapter: whether it is taken; the line its arguments are written on, grown to them; and the
// answer's body read as a tree taken to its size, its root
typedef struct
{
    int reserved;
    CliGrownLine args;
    CliJson json;
    unsigned int root;
} CliDebugAnswer;

// a request's work state taken, waiting while every one is in use, its arguments empty to write into `answer`;
// `command` sent to the session taken with those arguments, an empty object where none are written, its answer waited
// for up to `wait` milliseconds and read: 1, or 0 with why given; and the work state given back (debug_session.c)
CliDebugAnswer *cli_debug_answer_alloc(void);
int cli_debug_request(const CliDebugSessionCommandCtx *args);
void cli_debug_answer_free(const CliDebugSessionCommandCtx *args);

// session `id` listed, taken for a request, its adapter referenced: NULL with "that session has ended" given where it
// is not listed; a session taken let go, its adapter given back, its place freed where it was the last use of a session
// no longer listed whose adapter ended; a free place taken for a session, NULL where every one is taken; and what a
// session is written as the window reads it (debug_session.c)
CliDebugSession *cli_debug_alloc(const CliDebugErrMsgIdCtx *args);
void cli_debug_let_go(const CliDebugSessionErrMsgCtx *args);
CliDebugSession *cli_debug_position(void);
// a session's two lists' cells released and the lists empty
void cli_debug_lists_released(const CliDebugSessionErrMsgCtx *args);
void cli_debug_info_write(const CliDebugInfoCtx *args);
// the native session debugging the same process as Python session `id`, taken, NULL where there is none
CliDebugSession *cli_debug_native_get(const CliDebugErrMsgIdCtx *args);

// the breakpoints of `file` of the array `breakpoints` of `json` set through a session taken, each placed one's number
// retained, each line and whether it is bound written into `out` as pairs: 1, or 0 with why given (debug_session.c)
int cli_debug_set(const CliDebugSessionCtx *args);

// whether node `node` of `json` is a whole number, it into `value`, and the same of one at least 0; whether it is
// true; the text of a string node, `fallback` where it is none; whether a session in `language` debugs the file at
// `file`; `bytes` from base64, `size` of them, into `out`, sized for `capacity`: their count; the node `from` of `json`
// made in `to` with each "{name}" in its strings filled in from `names` and `withs`, `count` of them, and each string
// of digits alone made a number where `numbered`; the arguments that set stepping for a session in `language` put in
// the object `arguments` of `to`, as the object `stepping` of `json` specifies; and the breakpoints of the array
// `breakpoints` of `json` written as the protocol writes them (debug_arguments.c)
int cli_debug_sign(const CliDebugJsonNodeCtx *args);
int cli_debug_unsigned(const CliDebugJsonNodeValueU64sCtx *args);
int cli_debug_true(const CliDebugJsonNodeCtx *args);
const char *cli_debug_text(const CliDebugJsonNodeFallbackCtx *args);
int cli_debug_allocs(const CliDebugFileCtx *args);
unsigned long long cli_debug_base64(const CliDebugTextCtx *args);
unsigned int cli_debug_fill(const CliDebugJsonCtx *args);
void cli_debug_step_arguments(const CliDebugJsonLanguageCtx *args);
void cli_debug_breakpoints_write(const CliDebugSessionCtx *args);
// the stack frame at node `frame` of an adapter's answer written as the window reads it, `{"id", "name", "path",
// "line", "col", "ip"}`, its line and column counted from 0 and its path written by `named` (debug_requests.c)
void cli_debug_frame_write(const CliDebugOutCtx *args);

// the adapter for `language` that is found, the toolchain `ahead` names tried first, its spec made in `json` from the
// manifest, its node, the toolchain's name and id, and the program's path, each into what is sized for its longest: 1,
// or 0 with why given; the program a CMake or Meson build makes from `made`, built as that build builds it, into
// `program`; and the file at `path` built for debugging where its language is built, by the toolchain `ahead` names
// where it builds it and the first found where not, the program into `program`: 1 with `built` set, 1 with it clear for
// a language that runs from its source, or 0 with why given (debug_find.c)
int cli_debug_adapter(const CliDebugErrMsgCtx *args);
int cli_debug_build_program(const CliDebugErrMsgProgramCtx *args);
int cli_debug_build(const CliDebugErrMsgLanguageCtx *args);

// an adapter's event received for the session in place `context`; a stop at a breakpoint the reader did not place, in a
// session whose data breakpoints are lldb's own watchpoints, reported as the stop of a data breakpoint, which it is
// (debug_events.c)
void cli_debug_event(void *context, const char *event, const char *body, unsigned long long size);
// `text` run as a command of lldb's own in the session taken, what it prints into `out`: 1, or 0 with why given, a
// command that failed reported in the answer's text; and orior's script loaded into lldb in the session taken, once
// (debug_step.c)
int cli_debug_lldb_command(const CliDebugSessionErrMsgCtx *args);
int cli_debug_lldb_script(const CliDebugSessionErrMsgCtx *args);

// orior's script in lldb (debug_lldb.c)
extern const char g_cli_debug_lldb_script[];

struct CliDebugErrMsgCtx
{
    VerbaLinea *err_msg;
    const char *language;
    const char *ahead;
    CliJson *json_out;
    unsigned int *spec;
    char *tool;
    char *tool_id;
    char *program;
};

struct CliDebugErrMsgLanguageCtx
{
    VerbaLinea *err_msg;
    const char *language;
    const char *ahead;
    char *program;
    const char *root;
    const char *path;
    int *built;
};

struct CliDebugJsonCtx
{
    const CliJson *json;
    CliJson *to;
    unsigned int from;
    const char *const *names;
    const char *const *withs;
    unsigned int count;
    int numbered;
};

struct CliDebugSessionCtx
{
    CliDebugSession *session;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const char *file;
    const CliJson *json;
    unsigned int breakpoints;
};

struct CliDebugOutCtx
{
    VerbaLinea *out;
    const CliJson *json;
    unsigned int frame;
    CliDebugPathWritten named;
    void *context;
};

struct CliDebugSessionCommandCtx
{
    CliDebugSession *session;
    const char *command;
    CliDebugAnswer *answer;
    unsigned long long wait;
    VerbaLinea *err_msg;
};

struct CliDebugJsonLanguageCtx
{
    const CliJson *json;
    const char *language;
    CliJson *to;
    unsigned int stepping;
    unsigned int arguments;
};

struct CliDebugTextCtx
{
    const char *text;
    unsigned long long size;
    unsigned char *out_bytes;
    unsigned long long capacity;
};

struct CliDebugSessionErrMsgCtx
{
    CliDebugSession *session;
    VerbaLinea *err_msg;
    VerbaLinea *out;
    const char *text;
};

struct CliDebugErrMsgProgramCtx
{
    VerbaLinea *err_msg;
    char *program;
    const CliProgram *made;
};

struct CliDebugJsonNodeCtx
{
    const CliJson *json;
    unsigned int node;
    long long *value;
};

struct CliDebugJsonNodeFallbackCtx
{
    const CliJson *json;
    unsigned int node;
    const char *fallback;
};

struct CliDebugJsonNodeValueU64sCtx
{
    const CliJson *json;
    unsigned int node;
    unsigned long long *value_u64s;
};

struct CliDebugInfoCtx
{
    const CliDebugInfo *info;
    VerbaLinea *out;
};

struct CliDebugErrMsgIdCtx
{
    VerbaLinea *err_msg;
    unsigned long long id;
};

struct CliDebugFileCtx
{
    const char *file;
    const char *language;
};

struct CliDebugCtx
{
    CliDebugSession *place;
};

typedef struct
{
    int (*request)(const CliDebugSessionCommandCtx *args);
    void (*answer_free)(const CliDebugSessionCommandCtx *args);
} CliDebugSessionCommandNs;
EMBED_TABLE_LAYOUT(CliDebugSessionCommandNs, request, answer_free);
EMBED_TABLE_STORAGE CliDebugSessionCommandNs cli_debug_session_command_internal EMBED_UNUSED = {
    .request = cli_debug_request,
    .answer_free = cli_debug_answer_free,
};

typedef struct
{
    CliDebugSession *(*take)(const CliDebugErrMsgIdCtx *args);
    CliDebugSession *(*native_get)(const CliDebugErrMsgIdCtx *args);
} CliDebugErrMsgIdNs;
EMBED_TABLE_LAYOUT(CliDebugErrMsgIdNs, take, native_get);
EMBED_TABLE_STORAGE CliDebugErrMsgIdNs cli_debug_err_msg_id_internal EMBED_UNUSED = {
    .take = cli_debug_alloc,
    .native_get = cli_debug_native_get,
};

typedef struct
{
    void (*let_go)(const CliDebugSessionErrMsgCtx *args);
    void (*lists_released)(const CliDebugSessionErrMsgCtx *args);
    int (*lldb_command)(const CliDebugSessionErrMsgCtx *args);
    int (*lldb_script)(const CliDebugSessionErrMsgCtx *args);
} CliDebugSessionErrMsgNs;
EMBED_TABLE_LAYOUT(CliDebugSessionErrMsgNs, let_go, lists_released, lldb_command, lldb_script);
EMBED_TABLE_STORAGE CliDebugSessionErrMsgNs cli_debug_session_err_msg_internal EMBED_UNUSED = {
    .let_go = cli_debug_let_go,
    .lists_released = cli_debug_lists_released,
    .lldb_command = cli_debug_lldb_command,
    .lldb_script = cli_debug_lldb_script,
};

typedef struct
{
    void (*info_write)(const CliDebugInfoCtx *args);
} CliDebugInfoNs;
EMBED_TABLE_LAYOUT(CliDebugInfoNs, info_write);
EMBED_TABLE_STORAGE CliDebugInfoNs cli_debug_info_internal EMBED_UNUSED = {
    .info_write = cli_debug_info_write,
};

typedef struct
{
    int (*set)(const CliDebugSessionCtx *args);
    void (*breakpoints_write)(const CliDebugSessionCtx *args);
} CliDebugSessionNs;
EMBED_TABLE_LAYOUT(CliDebugSessionNs, set, breakpoints_write);
EMBED_TABLE_STORAGE CliDebugSessionNs cli_debug_session_internal EMBED_UNUSED = {
    .set = cli_debug_set,
    .breakpoints_write = cli_debug_breakpoints_write,
};

typedef struct
{
    int (*debug_sign)(const CliDebugJsonNodeCtx *args);
    int (*debug_true)(const CliDebugJsonNodeCtx *args);
} CliDebugJsonNodeNs;
EMBED_TABLE_LAYOUT(CliDebugJsonNodeNs, debug_sign, debug_true);
EMBED_TABLE_STORAGE CliDebugJsonNodeNs cli_debug_json_node_internal EMBED_UNUSED = {
    .debug_sign = cli_debug_sign,
    .debug_true = cli_debug_true,
};

typedef struct
{
    int (*debug_unsigned)(const CliDebugJsonNodeValueU64sCtx *args);
} CliDebugJsonNodeValueU64sNs;
EMBED_TABLE_LAYOUT(CliDebugJsonNodeValueU64sNs, debug_unsigned);
EMBED_TABLE_STORAGE CliDebugJsonNodeValueU64sNs cli_debug_json_node_value_u_64s_internal EMBED_UNUSED = {
    .debug_unsigned = cli_debug_unsigned,
};

typedef struct
{
    const char *(*text)(const CliDebugJsonNodeFallbackCtx *args);
} CliDebugJsonNodeFallbackNs;
EMBED_TABLE_LAYOUT(CliDebugJsonNodeFallbackNs, text);
EMBED_TABLE_STORAGE CliDebugJsonNodeFallbackNs cli_debug_json_node_fallback_internal EMBED_UNUSED = {
    .text = cli_debug_text,
};

typedef struct
{
    int (*allocs)(const CliDebugFileCtx *args);
} CliDebugFileNs;
EMBED_TABLE_LAYOUT(CliDebugFileNs, allocs);
EMBED_TABLE_STORAGE CliDebugFileNs cli_debug_file_internal EMBED_UNUSED = {
    .allocs = cli_debug_allocs,
};

typedef struct
{
    unsigned long long (*base64)(const CliDebugTextCtx *args);
} CliDebugTextNs;
EMBED_TABLE_LAYOUT(CliDebugTextNs, base64);
EMBED_TABLE_STORAGE CliDebugTextNs cli_debug_text_internal EMBED_UNUSED = {
    .base64 = cli_debug_base64,
};

typedef struct
{
    unsigned int (*fill)(const CliDebugJsonCtx *args);
} CliDebugJsonNsTable;
EMBED_TABLE_LAYOUT(CliDebugJsonNsTable, fill);
EMBED_TABLE_STORAGE CliDebugJsonNsTable cli_debug_json_internal EMBED_UNUSED = {
    .fill = cli_debug_fill,
};

typedef struct
{
    void (*step_arguments)(const CliDebugJsonLanguageCtx *args);
} CliDebugJsonLanguageNs;
EMBED_TABLE_LAYOUT(CliDebugJsonLanguageNs, step_arguments);
EMBED_TABLE_STORAGE CliDebugJsonLanguageNs cli_debug_json_language_internal EMBED_UNUSED = {
    .step_arguments = cli_debug_step_arguments,
};

typedef struct
{
    void (*frame_write)(const CliDebugOutCtx *args);
} CliDebugOutNsTable;
EMBED_TABLE_LAYOUT(CliDebugOutNsTable, frame_write);
EMBED_TABLE_STORAGE CliDebugOutNsTable cli_debug_out_internal EMBED_UNUSED = {
    .frame_write = cli_debug_frame_write,
};

typedef struct
{
    int (*adapter)(const CliDebugErrMsgCtx *args);
} CliDebugErrMsgNs;
EMBED_TABLE_LAYOUT(CliDebugErrMsgNs, adapter);
EMBED_TABLE_STORAGE CliDebugErrMsgNs cli_debug_err_msg_internal EMBED_UNUSED = {
    .adapter = cli_debug_adapter,
};

typedef struct
{
    int (*build_program)(const CliDebugErrMsgProgramCtx *args);
} CliDebugErrMsgProgramNs;
EMBED_TABLE_LAYOUT(CliDebugErrMsgProgramNs, build_program);
EMBED_TABLE_STORAGE CliDebugErrMsgProgramNs cli_debug_err_msg_program_internal EMBED_UNUSED = {
    .build_program = cli_debug_build_program,
};

typedef struct
{
    int (*build)(const CliDebugErrMsgLanguageCtx *args);
} CliDebugErrMsgLanguageNs;
EMBED_TABLE_LAYOUT(CliDebugErrMsgLanguageNs, build);
EMBED_TABLE_STORAGE CliDebugErrMsgLanguageNs cli_debug_err_msg_language_internal EMBED_UNUSED = {
    .build = cli_debug_build,
};

#endif
