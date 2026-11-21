// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers.h: the language servers the editor talks to, one for each toolchain whose `server` in toolchains.json names a
// program, such as clangd for C, C++ and CUDA. A server starts with the first file of its languages the editor opens,
// at the tree's top folder, and stops when another tree is opened or impero ends. One that cannot be found or does
// not start is not tried again until the tree changes, and the file opens without it.
//
// What the servers return comes back as orior's own shapes. A hover comes back as Markdown, and a definition as a path
// and a place. A usage comes back as a span with its line's text, and a completion as the editor's items. A file's
// diagnostics come back as spans with a severity: 1 an error, 2 a warning, 3 a note and 4 a hint.
//
// An edit comes from a rename, a quick fix or the server's own ask. It comes back as each file's spans and their new
// text. A quick fix goes to the editor whole, as the server gave it, and comes back for the editor to carry out.
//
// The tree's check passes each file of the tree that the editor does not have open to its language's server. It passes
// six at a time, as the editor opens a file. It lets each go once the server reports what is wrong in it. The check
// does not pass on the server's clear of a file it lets go.
//
// Some servers check their projects as a whole, as rust-analyzer does with cargo check. Such a server gets the order to
// check them all in place of that. A file gets a new check when it changes on the disk, with the files whose includes
// or imports name it. It also gets one when the editor closes it, as the disk keeps it.
//
// The checkers the reader names check a file as it opens. A checker that reads a file's text on its input checks it
// again as the editor changes it. Every checker checks a file again as it changes on the disk. With the tree's check,
// each checker that reads a tree whole checks the tree. A file's diagnostics contain each checker's findings, and a
// checker's fixes come as quick fixes.
//
// Every answer goes as JSON into the line passed for it. A path in it goes as the tree has it where the call gives a
// tree, and whole otherwise (servers_*.c)
#ifndef SERVERS_H
#define SERVERS_H
#include "../command_line/callback_cfg.h"
#include "../text/edits.h"
#include "../text/json_tree.h"
#include "embed_dispatch_layout.h"

typedef struct CliServersRootCfg CliServersRootCfg;
typedef struct CliServersRootSinkCfg CliServersRootSinkCfg;
typedef struct CliServersPathCfg CliServersPathCfg;
typedef struct CliServersErrMsgCfg CliServersErrMsgCfg;
typedef struct CliServersPathOutCfg CliServersPathOutCfg;
typedef struct CliServersPathOutErrMsgCfg CliServersPathOutErrMsgCfg;
typedef struct CliServersPathSizeCfg CliServersPathSizeCfg;
typedef struct CliServersRootSinkOutCfg CliServersRootSinkOutCfg;
typedef struct CliServersPathOutErrMsgFromCfg CliServersPathOutErrMsgFromCfg;
typedef struct CliServersPathOutErrMsgFromToCfg CliServersPathOutErrMsgFromToCfg;
typedef struct CliServersErrMsgIdCfg CliServersErrMsgIdCfg;
typedef struct CliServersRootSinkCountCfg CliServersRootSinkCountCfg;
typedef struct CliServersCapacityCfg CliServersCapacityCfg;
typedef struct CliServersOutCfg CliServersOutCfg;
typedef struct CliServersCfg CliServersCfg;
typedef struct CliServersPathSizeTypeCfg CliServersPathSizeTypeCfg;

// One diagnostic of a file: its span's first place and the place after it, each a line and a column in units. Its
// severity: 1 an error, 2 a warning, 3 and 4 notes. Its message, `message_length` bytes in the texts of its report
typedef struct
{
    unsigned long long from_line;
    unsigned long long from_col;
    unsigned long long to_line;
    unsigned long long to_col;
    unsigned int severity;
    const char *message;
    unsigned long long message_length;
} CliServerDiagnostic;

// A file's report, decoded once where the servers store it, in one movable cell. Its owner reaches it by the cell's
// token. Its head comes first, then its diagnostics, each with its message's place in the cell. Then comes its path as
// the sink's tree writes it, at `path_at` with a 0 after it. Last come the messages, each with a 0 after it.
// cli_servers_report_release releases the cell
typedef struct
{
    unsigned int count;
    unsigned long long path_at;
} CliServerReportHead;

typedef struct
{
    unsigned long long from_line;
    unsigned long long from_col;
    unsigned long long to_line;
    unsigned long long to_col;
    unsigned int severity;
    unsigned long long message_at;
    unsigned long long message_length;
} CliServerStoredDiagnostic;

// where the diagnostic `at_` of a report is in its cell
#define CLI_SERVER_REPORT_ITEM_AT(at_)                                                                                 \
    (sizeof(CliServerReportHead) + ((unsigned long long)(at_) * sizeof(CliServerStoredDiagnostic)))

// A file's diagnostics passed to the sink's function, in place of its last, on the thread that found them. The call
// gives its path, its diagnostics for the call alone, and the token of their report cell, NULL with none. A sink that
// reads the report after the call returns sets `*taken` to 1 and releases the report itself. For any other sink, the
// report goes as the call returns
typedef struct
{
    void *context;
    const char *path;
    const CliServerDiagnostic *item;
    unsigned int count;
    const CarcerMovableEntry *report;
    int *taken;
} CliServerDiagnosticsCfg;

// a report's cell released (servers_emit.c)
typedef struct
{
    const CarcerMovableEntry *report;
} CliServersReportCfg;
void cli_servers_report_release(const CliServersReportCfg *args);

// The progress of the tree's check, the total 0 where it is over
typedef struct
{
    void *context;
    unsigned long long done;
    unsigned long long total;
} CliServerCheckCfg;

// Where the events the servers send unasked go: a function that receives each event with its JSON and its context. The
// events are `lsp-diagnostics`, `lsp-edits`, `lsp-hints`, `lsp-parse` and `tree-check`. Then the tree of their paths
typedef struct
{
    void (*emit)(const CliEachEventJsonSizeCfg *args);
    void (*diagnostics)(const CliServerDiagnosticsCfg *args);
    void (*check_moved)(const CliServerCheckCfg *args);
    void *context;
    char tree[CLI_PATH_MAX_LEN];
} CliServersSink;

// The hover: the Markdown of its parts, `length` bytes, and the word's line and its first unit and the unit after it
typedef struct
{
    const char *text;
    unsigned long long length;
    unsigned long long line;
    unsigned long long from;
    unsigned long long to;
} CliHoverPanel;

// the rows of the completion's list displayed at once
#define CLI_COMPLETION_ROWS 10u

// One row of the completion: its label and its length, its type, and its detail and its length. Then the bytes of the
// label that the prefix marks
typedef struct
{
    const char *label;
    unsigned long long label_length;
    const char *type;
    const char *detail;
    unsigned long long detail_length;
    unsigned int marked;
} CliCompletionRow;

// The completion: the line and the first unit of the prefix, and the prefix. The count of items that answer it, and
// the place of the first row displayed among them. The rows displayed, and the place of the row chosen among the items
typedef struct
{
    unsigned long long line;
    unsigned long long from;
    const char *prefix;
    unsigned int count;
    unsigned int first;
    unsigned int rows;
    unsigned int chosen;
} CliCompletionPanel;

// The pointer moved: whether it is on the editor's text, at the row `row` from the view's top and the cell `column`.
// Then whether it is on the hover's box
typedef struct
{
    int in_text;
    unsigned long long row;
    unsigned long long column;
    int on_hover;
} CliServersPanelsPointerCfg;

// the panels over the editor of the tree `root`, on the window's thread (servers_panels.c). They begin and watch the
// views. Each view opened on a file goes to its language's server, and each view about to close lets go of it. The
// panels follow each frame of the view displayed, and then end.
//
// The calls after: every panel hidden, the pointer moved, and text typed, `length` bytes. A key's effect on the
// panels, 1 where the key was theirs. The completion's row `at` accepted, and the completion's rows turned by the
// wheel. Their timers taken, 1 where any came, and the clock's reading of the next, 0 for none. Then what they show:
// the hover, the completion and its row `row` displayed, each 1 where it shows, else 0. Last, the signature, NULL for
// none.
//
// The arguments: the tree `root` they begin for, and `length` bytes of `text` typed. The key `name`, the row `at`
// accepted, and the wheel's `rows`. The hover panel into `hover_out`, the completion `panel`, and its row `row` into
// `out`
typedef struct
{
    const char *root;
    const char *text;
    unsigned long long length;
    const char *name;
    unsigned int at;
    int rows;
    CliHoverPanel *hover_out;
    CliCompletionPanel *panel;
    unsigned int row;
    CliCompletionRow *out;
} CliServersPanelsCfg;
void cli_servers_panels_begin(const CliServersPanelsCfg *args);
void cli_servers_panels_end(void);
void cli_servers_panels_hide(void);
void cli_servers_panels_pointer(const CliServersPanelsPointerCfg *args);
void cli_servers_panels_text(const CliServersPanelsCfg *args);
int cli_servers_panels_key(const CliServersPanelsCfg *args);
void cli_servers_panels_accept(const CliServersPanelsCfg *args);
void cli_servers_panels_wheel(const CliServersPanelsCfg *args);
int cli_servers_panels_step(void);
unsigned long long cli_servers_panels_due_ms(void);
int cli_servers_panels_hover(const CliServersPanelsCfg *args);
int cli_servers_panels_completion(const CliServersPanelsCfg *args);
int cli_servers_panels_completion_row(const CliServersPanelsCfg *args);
const CliSignaturePanel *cli_servers_panels_signature(void);

// One row of the problems. A file's row has its path, its name, and the bytes of its path that its folder takes. It
// also has its diagnostics' count, and whether its rows show. A diagnostic's row has its severity, its message,
// the bytes of its first line, and its place. Each text lies in the file's report and lasts until the reading thread's
// next step
typedef struct
{
    int is_file;
    const char *path;
    const char *name;
    unsigned long long folder_length;
    unsigned int count;
    int closed;
    unsigned int severity;
    const char *message;
    unsigned long long message_length;
    unsigned long long line;
    unsigned long long col;
} CliProblemRow;

// One span a file's diagnostic underlines: its severity, and its first place and the place after it
typedef struct
{
    unsigned int severity;
    unsigned long long from_line;
    unsigned long long from_col;
    unsigned long long to_line;
    unsigned long long to_col;
} CliProblemSpan;

// The problems counted: the files, the diagnostics and the errors; the check's files done and total, the total 0 where
// none runs; and the rows displayed
typedef struct
{
    unsigned int files;
    unsigned int items;
    unsigned int errors;
    unsigned long long done;
    unsigned long long total;
    unsigned int rows;
} CliProblemsCounts;

// The problems (servers_problems.c). They begin with `woken`, which the thread of each report calls. Then the place the
// servers report to for the tree `root`. As they end, every report goes and the check stops. A report comes in on the
// thread that read it, which publishes it whole with an atomic swap. The report it replaced goes once no reader reads
// it.
//
// A step ends the caller thread's pass through the problems and begins its next, from the last report published. 1
// where that differs from what its last step read, or where the check moved. From one step to the next, on that
// thread: the count of the problems, and whether the tree's check runs. The row `at` into `row`, 1 or 0 past them. The
// span `at` of the file `path`'s diagnostics into `span`, 1 or 0 past them.
//
// A press on the row `at` closes or opens a file's rows, or puts a diagnostic's row into `row`. 1 where it was a
// diagnostic's, and then the caller opens its file at its place. Last, the tree's check of `root` starts, or stops
// where it runs
void cli_servers_problems_begin(void (*woken)(void));
// the problems: the sink and the check for the tree `root`; the row `at` into `row`, or pressed; and the span `at` of
// the file at `path` into `span`
typedef struct
{
    const char *root;
    unsigned int at;
    CliProblemRow *row;
    const char *path;
    CliProblemSpan *span;
} CliServersProblemsCfg;
CliServersSink cli_servers_problems_sink(const CliServersProblemsCfg *args);
int cli_servers_problems_step(void);
void cli_servers_problems_end(void);
CliProblemsCounts cli_servers_problems_counts(void);
int cli_servers_problems_checking(void);
int cli_servers_problems_row(const CliServersProblemsCfg *args);
int cli_servers_problems_span(const CliServersProblemsCfg *args);
int cli_servers_problems_press(const CliServersProblemsCfg *args);
void cli_servers_problems_check(const CliServersProblemsCfg *args);

// the file at `path` passed to its language's server, and its text to the checkers. The server starts at `root` where
// it does not run. The call writes `true` or `false` as a server took it. Then the file's new text, and the file let go
// as the editor closes it, read again as the disk keeps it. Each 1, or 0 with why (servers_files.c)
int cli_servers_open(const CliServersRootCfg *args);
int cli_servers_change(const CliServersRootCfg *args);
int cli_servers_close(const CliServersPathOutErrMsgFromToCfg *args);
// the view of `path` the page rests on: its lines `first` to `last`, and the lines `opener`, `openers` of them. Those
// open each region around its first line, outermost first. Some servers get the tree's slice in place of the file.
//
// For such a server with another view's slice, the call cuts the slice for this view. It gives that slice as the
// file's next version, forgets its parse, and writes `true`. It writes `false` where nothing changed. 1, or 0 with why
// (servers_slice.c)
int cli_servers_view(const CliServersPathOutErrMsgCfg *args);

// the language server the script plugin `id` brings, as its plugin.json's `server` names it. It starts at `root` with
// `length` bytes of JSON `options` as its first options. The `firsts` arguments `first` come before the arguments its
// plugin.json names. What it sends goes where `emit` sends it. It gets every file of its languages the editor has
// open, and returns 1, or 0 with why. Then the plugin's server stopped, where it runs.
//
// Then a request sent to it, with a wait of up to `wait` milliseconds for its answer into `result`. Then a
// notification sent to it. Last, an answer to a request it sent, of the id `request` the window got, with `length`
// bytes of JSON `result`. Each 1, or 0 with why (servers_plugins.c)
void cli_servers_plugin_stop(const CliServersRootSinkCfg *args);
int cli_servers_plugin_request(const CliServersErrMsgCfg *args);
int cli_servers_plugin_notify(const CliServersErrMsgCfg *args);
// The text of a file that the editor has open goes into `out` a window at a time. The measure writes the bytes of that
// text into `size`. Each returns 1, or 0 where the editor has no text of the file
int cli_servers_text_get(const CliServersPathOutErrMsgFromToCfg *args);
int cli_servers_text_measure(const CliServersPathSizeTypeCfg *args);

// what the server of `path` answers at `line`, `col`. A hover as Markdown, `null` for none. The definitions, each
// `{"path", "line", "col"}`. The completions, each an item, with orior's own read of a build file or JSON first.
//
// Every usage, each `{"path", "from", "to", "text"}`, the declaration among them. Whether a rename of the symbol can
// go, `{"from", "to", "name"}` or `null`. The edits that rename it to `name`, each file's `{"path", "edits"}`. Last,
// the signature of the call the cursor is in, `{"label", "params", "active", "doc", "count"}` or `null`. Each 1, or 0
// with why (servers_request.c, servers_shapes.c)
int cli_servers_hover(const CliServersPathOutCfg *args);
// One item a completion answers: what it shows, its type, what it states of itself and what it inserts. A 0 ends each.
// Then whether what it inserts is a snippet
typedef struct
{
    const char *label;
    const char *type;
    const char *detail;
    const char *insert;
    int snippet;
} CliServerItem;
// The items of one completion in one cell
typedef struct
{
    unsigned int count;
    CliServerItem *item;
} CliServerItems;
// the completion items at `line`, `col` of the file at `path`, in a cell of `owner`. The tree gives items where the
// file is a slice's, and a build or a schema gives items where the file is one. The file's server gives the rest. The
// cell, or NULL with why where no ask goes or the cells cannot meet it. Then the server's signature at the place,
// written into `store`: 1, or 0 where it gives none
typedef struct
{
    const char *path;
    unsigned int line;
    unsigned int col;
    const CellOwner *owner;
    VerbaLinea *err_msg;
    CliSignatureStore *store;
} CliServersAskCfg;
CliServerItems *cli_servers_items_alloc(const CliServersAskCfg *args);
int cli_servers_signature_read(const CliServersAskCfg *args);

// the lines `from` to `to` of the parse of `path`, from orior's own read or its server's semantic tokens. Then its
// folds, `{"whole", "from", "lines", "folds"}` or `null`. Then the spans that contain `from` to `to` and are more than
// it, the least first.
//
// Those come from the parse where it reads the file whole. Else they come from the branch under `key`, NULL for none,
// or from the text the servers got. Then the hints its server writes on the lines `from` to `to`. Last, the parses from
// servers forgotten, for a new ask of each. Each 1, or 0 with why (servers_parse.c)
void cli_servers_forget_tokens(void);

// A server running: its toolchain's id, its program's name, and its process
typedef struct
{
    char tool[CLI_NAME_MAX_LEN];
    char program[CLI_NAME_MAX_LEN];
    unsigned int pid;
} CliServerListing;

// every server that runs into `servers`, sized for `capacity`: their count. The toolchains whose servers serve any of
// `languages`, `count` of them, into `tools`, sized for `capacity`: their count. The servers of `tools`, `count` of
// them, stopped without a wait, with the files they had open written as an array. Every server stopped with a wait for
// each, as the app closes. Every server let go as another tree opens, and each server told that its settings changed
// (`servers_running.c`)
unsigned int cli_servers_list(const CliServersCfg *args);
unsigned int cli_servers_tools_for(const CliServersCapacityCfg *args);
void cli_servers_stop_all(void);
void cli_servers_let_go(void);

// the tree's check started over `files`, `count` of them, each by its path in the tree at `root`: the count that
// waits. Then the check stopped where it is: the files that wait go, and the check reports done. It stays on for the
// files that change after. Then whether it has files that wait or are in a check.
//
// Last, a batch of `count` files changed on the disk, each by its path in the tree. Each gets a new check with the
// files whose includes or imports name it. A file gone loses its diagnostics (servers_check.c, servers_changed.c)
unsigned int cli_servers_check_tree(const CliServersRootSinkCountCfg *args);
void cli_servers_stop_check(void);
// the jobs ring put over its entries, its part of the rings block, as the pool starts (servers_jobs.c)
void cli_servers_jobs_positioned(void);
// whether every job a dispatcher queued ran to its end: 1 where no job waits or runs (servers_jobs.c)
int cli_servers_jobs_idle(void);
int cli_servers_checking(void);
void cli_servers_on_change(const CliServersRootSinkCountCfg *args);

struct CliServersRootCfg
{
    const char *root;
    const char *path;
    const char *language;
    const char *text;
    unsigned long long size;
    const CliServersSink *sink;
    int *served;
    VerbaLinea *err_msg;
};

struct CliServersRootSinkCfg
{
    const char *root;
    const CliServersSink *sink;
    VerbaLinea *err_msg;
    const char *id;
    const char *options;
    unsigned long long length;
    const char *const *first_characters;
    unsigned int firsts;
};

struct CliServersPathCfg
{
    const char *path;
    unsigned long long size;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *tree;
    const char *item;
    int incoming;
};

struct CliServersErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *id;
    unsigned long long length;
    const char *method;
    const char *params;
    unsigned long long wait;
    VerbaLinea *result;
};

struct CliServersPathOutCfg
{
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned int line;
    unsigned int col;
    const char *tree;
    const char *name;
};

struct CliServersPathOutErrMsgCfg
{
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned int first;
    unsigned int last;
    const unsigned int *opener;
    unsigned int openers;
};

struct CliServersPathSizeCfg
{
    const char *path;
    unsigned long long size;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const char *tree;
    const char *raw;
};

struct CliServersRootSinkOutCfg
{
    const char *root;
    const CliServersSink *sink;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned int count;
    const char *const *names;
};

struct CliServersPathOutErrMsgFromCfg
{
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    CliTextPosition from;
    CliTextPosition to;
    const char *key;
};

struct CliServersPathOutErrMsgFromToCfg
{
    const char *path;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    unsigned int from_u32;
    unsigned int to_u32;
};

struct CliServersErrMsgIdCfg
{
    VerbaLinea *err_msg;
    const char *id;
    unsigned long long length;
    const char *request;
    const char *result_text;
};

struct CliServersRootSinkCountCfg
{
    const char *root;
    const CliServersSink *sink;
    unsigned int count;
    const char *const *files;
};

struct CliServersCapacityCfg
{
    unsigned int capacity;
    const char *const *languages;
    unsigned int count;
    char (*tools)[CLI_NAME_MAX_LEN];
};

struct CliServersOutCfg
{
    VerbaLinea *out;
    unsigned int count;
    const char *const *tools_characters;
};

struct CliServersCfg
{
    CliServerListing *servers;
    unsigned int capacity;
};

struct CliServersPathSizeTypeCfg
{
    const char *path;
    unsigned long long *size_pointer;
};

typedef struct
{
    int (*open)(const CliServersRootCfg *args);
    int (*change)(const CliServersRootCfg *args);
} CliServersRootNs;
EMBED_TABLE_LAYOUT(CliServersRootNs, open, change);
EMBED_TABLE_STORAGE CliServersRootNs cli_servers_root EMBED_UNUSED = {
    .open = cli_servers_open,
    .change = cli_servers_change,
};

typedef struct
{
    int (*close)(const CliServersPathOutErrMsgFromToCfg *args);
    int (*text_get)(const CliServersPathOutErrMsgFromToCfg *args);
} CliServersPathOutErrMsgFromToNs;
EMBED_TABLE_LAYOUT(CliServersPathOutErrMsgFromToNs, close, text_get);
EMBED_TABLE_STORAGE CliServersPathOutErrMsgFromToNs cli_servers_path_out_err_msg_from_to EMBED_UNUSED = {
    .close = cli_servers_close,
    .text_get = cli_servers_text_get,
};

typedef struct
{
    int (*view)(const CliServersPathOutErrMsgCfg *args);
} CliServersPathOutErrMsgNs;
EMBED_TABLE_LAYOUT(CliServersPathOutErrMsgNs, view);
EMBED_TABLE_STORAGE CliServersPathOutErrMsgNs cli_servers_path_out_err_msg EMBED_UNUSED = {
    .view = cli_servers_view,
};

typedef struct
{
    void (*plugin_stop)(const CliServersRootSinkCfg *args);
} CliServersRootSinkNs;
EMBED_TABLE_LAYOUT(CliServersRootSinkNs, plugin_stop);
EMBED_TABLE_STORAGE CliServersRootSinkNs cli_servers_root_sink EMBED_UNUSED = {
    .plugin_stop = cli_servers_plugin_stop,
};

typedef struct
{
    int (*plugin_request)(const CliServersErrMsgCfg *args);
    int (*plugin_notify)(const CliServersErrMsgCfg *args);
} CliServersErrMsgNs;
EMBED_TABLE_LAYOUT(CliServersErrMsgNs, plugin_request, plugin_notify);
EMBED_TABLE_STORAGE CliServersErrMsgNs cli_servers_err_msg EMBED_UNUSED = {
    .plugin_request = cli_servers_plugin_request,
    .plugin_notify = cli_servers_plugin_notify,
};

typedef struct
{
    int (*text_measure)(const CliServersPathSizeTypeCfg *args);
} CliServersPathSizeTypeNs;
EMBED_TABLE_LAYOUT(CliServersPathSizeTypeNs, text_measure);
EMBED_TABLE_STORAGE CliServersPathSizeTypeNs cli_servers_path_size_type EMBED_UNUSED = {
    .text_measure = cli_servers_text_measure,
};

typedef struct
{
    int (*hover)(const CliServersPathOutCfg *args);
} CliServersPathOutNs;
EMBED_TABLE_LAYOUT(CliServersPathOutNs, hover);
EMBED_TABLE_STORAGE CliServersPathOutNs cli_servers_path_out EMBED_UNUSED = {
    .hover = cli_servers_hover,
};

typedef struct
{
    unsigned int (*list)(const CliServersCfg *args);
} CliServersNs;
EMBED_TABLE_LAYOUT(CliServersNs, list);
EMBED_TABLE_STORAGE CliServersNs cli_servers_table EMBED_UNUSED = {
    .list = cli_servers_list,
};

typedef struct
{
    unsigned int (*tools_for)(const CliServersCapacityCfg *args);
} CliServersCapacityNs;
EMBED_TABLE_LAYOUT(CliServersCapacityNs, tools_for);
EMBED_TABLE_STORAGE CliServersCapacityNs cli_servers_capacity EMBED_UNUSED = {
    .tools_for = cli_servers_tools_for,
};

typedef struct
{
    unsigned int (*check_tree)(const CliServersRootSinkCountCfg *args);
    void (*change)(const CliServersRootSinkCountCfg *args);
} CliServersRootSinkCountNs;
EMBED_TABLE_LAYOUT(CliServersRootSinkCountNs, check_tree, change);
EMBED_TABLE_STORAGE CliServersRootSinkCountNs cli_servers_root_sink_count EMBED_UNUSED = {
    .check_tree = cli_servers_check_tree,
    .change = cli_servers_on_change,
};

typedef struct
{
    CliServerItems *(*items_alloc)(const CliServersAskCfg *args);
    int (*signature_read)(const CliServersAskCfg *args);
} CliServersAskNs;
EMBED_TABLE_LAYOUT(CliServersAskNs, items_alloc, signature_read);
EMBED_TABLE_STORAGE CliServersAskNs cli_servers_ask EMBED_UNUSED = {
    .items_alloc = cli_servers_items_alloc,
    .signature_read = cli_servers_signature_read,
};

#endif
