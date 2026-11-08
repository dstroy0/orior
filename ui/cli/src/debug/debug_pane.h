// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_pane.h: the Debug pane as the window reads it (debug_pane.c, debug_pane_rows.c, debug_pane_bar.c). One sink
// takes every session's events on the adapter's thread, stores them under a lock and wakes the view's thread, whose
// step reads them: a stop reads the stopped thread's frames, the first chosen, then the chosen frame's scopes, the
// variables of each open scope and variable and each watch's value; output runs into the console's lines; a continue,
// an exit and an end change the frames, the console and the status line. Three views show one at a time: Frames, a
// row each frame with its function and its file and line; Variables, a row each watch, then each scope and the
// variables under it to CLI_DEBUG_DEPTH levels; and Console, a row each line of the program's output and of what was
// evaluated, at most CLI_DEBUG_CONSOLE_LINES, the oldest going first. Rows stand in one cell a view as indices, their
// strings as offsets into one text cell beside it, each sized to the data and taken anew as it outgrows it, the counts
// changing as the rows do. The bar's steps carry whether each can act and whether it is shown; the field under the
// views takes an expression, a watch in Variables and an evaluation in Console. The requests the pane sends a session
// go through a table of the debug module's requests, cli_debug_pane_requests where the window begins the pane
#ifndef DEBUG_PANE_H
#define DEBUG_PANE_H
#include "../command_line/pool.h"
#include "debug.h"
#include "embed_dispatch_layout.h"

typedef struct CliDebugPaneViewCfg CliDebugPaneViewCfg;
typedef struct CliDebugPaneBarCfg CliDebugPaneBarCfg;
typedef struct CliDebugPaneInputCfg CliDebugPaneInputCfg;

// the views, one shown at a time, and how many
#define CLI_DEBUG_VIEW_FRAMES 0u
#define CLI_DEBUG_VIEW_VARIABLES 1u
#define CLI_DEBUG_VIEW_CONSOLE 2u
#define CLI_DEBUG_VIEWS 3u

// the rows' types: a frame, a scope, a variable, a watch, a line of the console, and the notice a view shows where it
// has no rows of its own
#define CLI_DEBUG_ROW_FRAME 0u
#define CLI_DEBUG_ROW_SCOPE 1u
#define CLI_DEBUG_ROW_VARIABLE 2u
#define CLI_DEBUG_ROW_WATCH 3u
#define CLI_DEBUG_ROW_LINE 4u
#define CLI_DEBUG_ROW_NOTICE 5u

// a console line's types: the program's output and its errors, the debugger's own words, an expression typed and
// what it came to
#define CLI_DEBUG_LINE_STDOUT 0u
#define CLI_DEBUG_LINE_STDERR 1u
#define CLI_DEBUG_LINE_CONSOLE 2u
#define CLI_DEBUG_LINE_INPUT 3u
#define CLI_DEBUG_LINE_RESULT 4u

// the bar's steps in their order, and how many
#define CLI_DEBUG_STEP_CONTINUE 0u
#define CLI_DEBUG_STEP_PAUSE 1u
#define CLI_DEBUG_STEP_OVER 2u
#define CLI_DEBUG_STEP_INTO 3u
#define CLI_DEBUG_STEP_OUT 4u
#define CLI_DEBUG_STEP_BACK 5u
#define CLI_DEBUG_STEP_BACKWARD 6u
#define CLI_DEBUG_STEP_RESTART 7u
#define CLI_DEBUG_STEP_STOP 8u
#define CLI_DEBUG_STEPS 9u

// the most lines the console carries, the most bytes the field takes with its end, and the most levels of variables
// under a scope
#define CLI_DEBUG_CONSOLE_LINES 5000u
#define CLI_DEBUG_FIELD_MAX 4096u
#define CLI_DEBUG_DEPTH 12u

// A row of a view: its type; its depth under the scopes, 0 for a scope; whether it opens, a scope's or a variable's
// children under it, and whether they are open; whether it is the frame chosen; whether its value failed, a watch's
// that could not be evaluated or a line of errors; a console line's type; three texts with their bytes, a frame's
// function, its file and line and its path, a variable's name, value and type, a watch's expression and value, a
// line's text, a notice's sentence; and a frame's line counted from 0
typedef struct
{
    unsigned int type;
    unsigned int depth;
    int opens;
    int open;
    int current;
    int failed;
    unsigned int line_type;
    const char *text;
    unsigned long long text_length;
    const char *second;
    unsigned long long second_length;
    const char *third;
    unsigned long long third_length;
    unsigned int line;
} CliDebugPaneRow;

// What a row pressed opens: the path in the tree, its end after it, and the line counted from 0 where `placed`
typedef struct
{
    const char *path;
    unsigned int line;
    int placed;
} CliDebugPaneOpen;

// The status line: its text and bytes, and whether it reports a failure
typedef struct
{
    const char *text;
    unsigned long long length;
    int failed;
} CliDebugPaneStatus;

// A step of the bar: its command as the catalog lists it, its label and bytes, the icon it is drawn with, its keys,
// whether it can act now, and whether it is shown
typedef struct
{
    const char *command;
    const char *label;
    unsigned long long label_length;
    const char *glyph;
    const char *keys;
    int enabled;
    int shown;
} CliDebugBarItem;

// The field under the views: the text typed and its bytes, the placeholder shown while it is empty and its bytes, and
// whether the view shown has a field
typedef struct
{
    const char *text;
    unsigned long long length;
    const char *placeholder;
    unsigned long long placeholder_length;
    int shown;
} CliDebugPaneField;

// The requests the pane sends a session, the debug module's own in cli_debug_pane_requests: the sessions running, a
// session's threads, a thread's stack, a frame's scopes, a reference's variables, an expression evaluated, a step, a
// Python session's step into, and a session ended
typedef struct
{
    void (*sessions)(const CliDebugOutErrMsgIdStepCfg *args);
    int (*threads)(const CliDebugOutErrMsgIdStepCfg *args);
    int (*stack)(const CliDebugOutErrMsgIdThreadCfg *args);
    int (*scopes)(const CliDebugOutErrMsgCfg *args);
    int (*variables)(const CliDebugOutCfg *args);
    int (*evaluate)(const CliDebugOutErrMsgCfg *args);
    int (*step)(const CliDebugOutErrMsgIdThreadHowCfg *args);
    int (*step_across)(const CliDebugRootOutErrMsgCfg *args);
    void (*stop)(const CliDebugCfg *args);
} CliDebugPaneRequestsNs;
EMBED_TABLE_LAYOUT(CliDebugPaneRequestsNs, sessions, threads, stack, scopes, variables, evaluate, step, step_across,
                   stop);
EMBED_TABLE_STORAGE CliDebugPaneRequestsNs cli_debug_pane_requests EMBED_UNUSED = {
    .sessions = cli_debug_sessions,
    .threads = cli_debug_threads,
    .stack = cli_debug_stack,
    .scopes = cli_debug_scopes,
    .variables = cli_debug_variables,
    .evaluate = cli_debug_evaluate,
    .step = cli_debug_step,
    .step_across = cli_debug_step_across,
    .stop = cli_debug_stop,
};

// the pane begun for the tree at `root`: `woken` wakes the view's thread as an event is stored, `requests` the table
// the pane's requests go through, every view empty, the status line the opening sentence; and the sink every session
// started for the pane is handed, which stores each event under the lock from the adapter's thread
void cli_debug_pane_begin(const CliDebugPaneViewCfg *args);
CliDebugSink cli_debug_pane_sink(const CliDebugPaneViewCfg *args);
// the events stored since the last step read at `now_ms` for the tree at `root`, their requests sent then: 1 where a
// row, the bar or the status line changed; and when the next step falls, `now_ms` while events wait, else 0
int cli_debug_pane_step(const CliDebugPaneViewCfg *args);
unsigned long long cli_debug_pane_deadline_ms(const CliDebugPaneViewCfg *args);
// how many rows `view` has, a notice's among them where it has none of its own; the row `at` of `view` into `row`: 1,
// or 0 past the last; the scope or variable at the row `at` of Variables opened or closed, its children read again,
// or the watch at it taken out: 1 where the rows changed; and the row `at` of `view` pressed: a frame chosen, its
// variables read again, with its file and line into `open_out` where its path is in the tree: 1 where it opens one
unsigned int cli_debug_pane_rows(const CliDebugPaneViewCfg *args);
int cli_debug_pane_row(const CliDebugPaneViewCfg *args);
int cli_debug_pane_toggle(const CliDebugPaneViewCfg *args);
int cli_debug_pane_press(const CliDebugPaneViewCfg *args);
// the view shown, one of CLI_DEBUG_VIEW_FRAMES and the rest; `view` shown; and the label of `view`, its bytes into
// `length_out`
unsigned int cli_debug_pane_shown(const CliDebugPaneViewCfg *args);
void cli_debug_pane_view_set(const CliDebugPaneViewCfg *args);
const char *cli_debug_pane_label(const CliDebugPaneViewCfg *args);
struct CliDebugPaneViewCfg
{
    const char *root;
    unsigned long long now_ms;
    void (*woken)(void);
    const CliDebugPaneRequestsNs *requests;
    unsigned int view;
    unsigned int at;
    CliDebugPaneRow *row;
    CliDebugPaneOpen *open_out;
    unsigned long long *length_out;
};

typedef struct
{
    void (*begin)(const CliDebugPaneViewCfg *args);
    CliDebugSink (*sink)(const CliDebugPaneViewCfg *args);
    int (*step)(const CliDebugPaneViewCfg *args);
    unsigned long long (*deadline_ms)(const CliDebugPaneViewCfg *args);
    unsigned int (*rows)(const CliDebugPaneViewCfg *args);
    int (*row)(const CliDebugPaneViewCfg *args);
    int (*toggle)(const CliDebugPaneViewCfg *args);
    int (*press)(const CliDebugPaneViewCfg *args);
    unsigned int (*view)(const CliDebugPaneViewCfg *args);
    void (*view_set)(const CliDebugPaneViewCfg *args);
    const char *(*label)(const CliDebugPaneViewCfg *args);
} CliDebugPaneViewNs;
EMBED_TABLE_LAYOUT(CliDebugPaneViewNs, begin, sink, step, deadline_ms, rows, row, toggle, press, view, view_set, label);
EMBED_TABLE_STORAGE CliDebugPaneViewNs cli_debug_pane_view EMBED_UNUSED = {
    .begin = cli_debug_pane_begin,
    .sink = cli_debug_pane_sink,
    .step = cli_debug_pane_step,
    .deadline_ms = cli_debug_pane_deadline_ms,
    .rows = cli_debug_pane_rows,
    .row = cli_debug_pane_row,
    .toggle = cli_debug_pane_toggle,
    .press = cli_debug_pane_press,
    .view = cli_debug_pane_shown,
    .view_set = cli_debug_pane_view_set,
    .label = cli_debug_pane_label,
};

// the bar's step `at` into `item`: 1, or 0 past the last; the step `at` run on the chosen session for the tree at
// `root`, a refusal written to the console as an error line: 1 where it ran; and the status line into `status_out`
int cli_debug_pane_item(const CliDebugPaneBarCfg *args);
int cli_debug_pane_run(const CliDebugPaneBarCfg *args);
void cli_debug_pane_status(const CliDebugPaneBarCfg *args);
struct CliDebugPaneBarCfg
{
    const char *root;
    unsigned int at;
    CliDebugBarItem *item;
    CliDebugPaneStatus *status_out;
};

typedef struct
{
    int (*item)(const CliDebugPaneBarCfg *args);
    int (*run)(const CliDebugPaneBarCfg *args);
    void (*status)(const CliDebugPaneBarCfg *args);
} CliDebugPaneBarNs;
EMBED_TABLE_LAYOUT(CliDebugPaneBarNs, item, run, status);
EMBED_TABLE_STORAGE CliDebugPaneBarNs cli_debug_pane_bar EMBED_UNUSED = {
    .item = cli_debug_pane_item,
    .run = cli_debug_pane_run,
    .status = cli_debug_pane_status,
};

// the field of the view shown into `field_out`; `size` bytes of `text` typed at its end; and the key `name` pressed in
// it for the tree at `root`: Enter adds the text as a watch in Variables and evaluates it in the chosen frame in
// Console, the field emptied, and Backspace erases its last letter: 1 where the key was taken
void cli_debug_pane_field(const CliDebugPaneInputCfg *args);
void cli_debug_pane_type(const CliDebugPaneInputCfg *args);
int cli_debug_pane_key(const CliDebugPaneInputCfg *args);
struct CliDebugPaneInputCfg
{
    const char *root;
    const char *text;
    unsigned long long size;
    const char *name;
    CliDebugPaneField *field_out;
};

typedef struct
{
    void (*field)(const CliDebugPaneInputCfg *args);
    void (*type)(const CliDebugPaneInputCfg *args);
    int (*key)(const CliDebugPaneInputCfg *args);
} CliDebugPaneInputNs;
EMBED_TABLE_LAYOUT(CliDebugPaneInputNs, field, type, key);
EMBED_TABLE_STORAGE CliDebugPaneInputNs cli_debug_pane_input EMBED_UNUSED = {
    .field = cli_debug_pane_field,
    .type = cli_debug_pane_type,
    .key = cli_debug_pane_key,
};

#endif
